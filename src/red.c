
/* MÓDULO DE COMUNICACIÓN */

/* Inclusión de la interfaz del módulo */
#include "red.h"

/* Bibliotecas necesarias para la comunicación */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

/* Inicializar la biblioteca libcurl */
int red_inicializar(void)
{
    /* Preparar los recursos globales de libcurl */
    CURLcode resultado = curl_global_init(CURL_GLOBAL_DEFAULT);

    /* Comprobar si la inicialización fue exitosa */
    if (resultado != CURLE_OK)
    {
        fprintf(stderr, "Error al inicializar libcurl: %s\n",
                curl_easy_strerror(resultado));

        return 1;
    }

    return 0;
}

/* Finalizar la biblioteca libcurl */
void red_finalizar(void)
{
    /* Liberar los recursos globales de libcurl */
    curl_global_cleanup();
}

/* Liberar la memoria de una respuesta HTTP */
void red_liberar_respuesta(RedRespuesta *respuesta)
{
    /* Comprobar que el puntero recibido sea válido */
    if (respuesta == NULL)
    {
        return;
    }

    /* Liberar el contenido almacenado */
    free(respuesta->contenido);

    /* Restablecer los valores de la estructura */
    respuesta->contenido = NULL;
    respuesta->longitud = 0;
    respuesta->codigo_http = 0;
}

/* Recibir y almacenar los datos enviados por el servidor */
static size_t red_recibir_datos(void *datos, size_t tamano,
                                size_t cantidad, void *usuario)
{
    /* Obtener la estructura donde se almacenará la respuesta */
    RedRespuesta *respuesta = (RedRespuesta *)usuario;

    /* Comprobar que la estructura sea válida */
    if (respuesta == NULL)
    {
        return 0;
    }

    /* Comprobar que la multiplicación no provoque un desbordamiento */
    if (tamano != 0 && cantidad > (size_t)-1 / tamano)
    {
        return 0;
    }

    /* Calcular la cantidad de bytes recibidos */
    size_t bytes = tamano * cantidad;

    /* Comprobar que existan datos para procesar */
    if (bytes == 0)
    {
        return 0;
    }

    if (datos == NULL)
    {
        return 0;
    }

    /* Comprobar que exista espacio suficiente para los nuevos datos */
    if (respuesta->longitud == (size_t)-1 ||
        bytes > (size_t)-1 - respuesta->longitud - 1)
    {
        return 0;
    }

    /* Ampliar la memoria para almacenar los datos recibidos */
    char *nuevo_contenido = realloc(
        respuesta->contenido,
        respuesta->longitud + bytes + 1
    );

    /* Comprobar si se pudo reservar la memoria */
    if (nuevo_contenido == NULL)
    {
        return 0;
    }

    /* Actualizar el puntero al contenido */
    respuesta->contenido = nuevo_contenido;

    /* Copiar los nuevos datos al final del contenido */
    memcpy(
        respuesta->contenido + respuesta->longitud,
        datos,
        bytes
    );

    /* Actualizar la longitud total */
    respuesta->longitud += bytes;

    /* Agregar el terminador de cadena */
    respuesta->contenido[respuesta->longitud] = '\0';

    /* Informar a libcurl cuántos bytes fueron procesados */
    return bytes;
}

/* Preparar un manejador para solicitudes HTTP */
static CURL *red_preparar_solicitud(RedRespuesta *respuesta)
{
    /* Comprobar que la estructura de respuesta sea válida */
    if (respuesta == NULL)
    {
        fprintf(stderr, "Error: estructura de respuesta no válida\n");
        return NULL;
    }

    /* Crear el manejador de la solicitud */
    CURL *curl = curl_easy_init();

    /* Comprobar si se pudo crear el manejador */
    if (curl == NULL)
    {
        fprintf(stderr, "Error al crear el manejador HTTP\n");
        return NULL;
    }

    /* Configurar la función que recibirá los datos del servidor */
    CURLcode resultado = curl_easy_setopt(
        curl, CURLOPT_WRITEFUNCTION, red_recibir_datos
    );

    /* Comprobar la configuración */
    if (resultado != CURLE_OK)
    {
        goto error;
    }

    /* Indicar dónde se almacenará la respuesta */
    resultado = curl_easy_setopt(
        curl, CURLOPT_WRITEDATA, respuesta
    );

    if (resultado != CURLE_OK)
    {
        goto error;
    }

    /* Configurar el tiempo máximo de espera */
    resultado = curl_easy_setopt(
        curl, CURLOPT_TIMEOUT, 5L
    );

    if (resultado != CURLE_OK)
    {
        goto error;
    }

    /* Configurar el identificador de nuestra aplicación */
    resultado = curl_easy_setopt(
        curl, CURLOPT_USERAGENT, "estacion-itla/1.0"
    );

    if (resultado != CURLE_OK)
    {
        goto error;
    }

    return curl;

error:

    /* Informar el error de configuración */
    fprintf(stderr, "Error al configurar la solicitud HTTP: %s\n",
            curl_easy_strerror(resultado));

    /* Liberar el manejador */
    curl_easy_cleanup(curl);

    return NULL;
}

/* Construir la URL de una solicitud HTTP */
static char *red_construir_url(const char *ruta)
{
    /* Obtener la dirección del servidor desde el entorno */
    const char *host = getenv("ITLA_HOST");

    /* Comprobar que el servidor y la ruta sean válidos */
    if (host == NULL || host[0] == '\0' ||
        ruta == NULL || ruta[0] != '/')
    {
        fprintf(stderr, "Error: servidor o ruta no válidos\n");
        return NULL;
    }

    /* Calcular el espacio necesario para construir la URL */
    int longitud = snprintf(
        NULL,
        0,
        "http://%s:4000%s",
        host,
        ruta
    );

    /* Comprobar si se pudo calcular la longitud */
    if (longitud < 0)
    {
        fprintf(stderr, "Error al calcular la longitud de la URL\n");
        return NULL;
    }

    /* Reservar memoria para almacenar la URL */
    char *url = malloc((size_t)longitud + 1);

    /* Comprobar si se pudo reservar la memoria */
    if (url == NULL)
    {
        fprintf(stderr, "Error al reservar memoria para la URL\n");
        return NULL;
    }

    /* Construir la URL completa */
    snprintf(
        url,
        (size_t)longitud + 1,
        "http://%s:4000%s",
        host,
        ruta
    );

    return url;
}


/* Realizar una solicitud HTTP GET */
int red_get(const char *ruta, RedRespuesta *respuesta)
{
    /* Comprobar que la estructura de respuesta sea válida */
    if (respuesta == NULL)
    {
        fprintf(stderr, "Error: estructura de respuesta no válida\n");
        return 1;
    }

    /* Inicializar los valores de la respuesta */
    respuesta->contenido = NULL;
    respuesta->longitud = 0;
    respuesta->codigo_http = 0;

    /* Construir la dirección del servidor */
    char *url = red_construir_url(ruta);

    if (url == NULL)
    {
        return 1;
    }

    /* Preparar el manejador HTTP */
    CURL *curl = red_preparar_solicitud(respuesta);

    if (curl == NULL)
    {
        free(url);
        return 1;
    }

    /* Configurar la dirección de la solicitud */
    CURLcode resultado = curl_easy_setopt(
        curl, CURLOPT_URL, url
    );

    if (resultado != CURLE_OK)
    {
        fprintf(stderr, "Error al configurar la URL: %s\n",
                curl_easy_strerror(resultado));

        curl_easy_cleanup(curl);
        free(url);
        return 1;
    }

    /* Ejecutar la solicitud HTTP */
    resultado = curl_easy_perform(curl);

    if (resultado != CURLE_OK)
    {
        fprintf(stderr, "Error en la solicitud GET: %s\n",
                curl_easy_strerror(resultado));

        curl_easy_cleanup(curl);
        free(url);
        red_liberar_respuesta(respuesta);
        return 1;
    }

    /* Obtener el código de respuesta HTTP */
    resultado = curl_easy_getinfo(
        curl, CURLINFO_RESPONSE_CODE, &respuesta->codigo_http
    );

    if (resultado != CURLE_OK)
    {
        fprintf(stderr, "Error al obtener el código HTTP: %s\n",
                curl_easy_strerror(resultado));

        curl_easy_cleanup(curl);
        free(url);
        red_liberar_respuesta(respuesta);
        return 1;
    }

    /* Liberar los recursos utilizados */
    curl_easy_cleanup(curl);
    free(url);

    /* Comprobar si el servidor respondió correctamente */
    if (respuesta->codigo_http < 200 ||
        respuesta->codigo_http >= 300)
    {
        fprintf(stderr, "Error: el servidor respondió con HTTP %ld\n",
                respuesta->codigo_http);

        red_liberar_respuesta(respuesta);
        return 1;
    }

    return 0;
}
