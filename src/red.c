
/* MÓDULO DE COMUNICACIÓN */

#include "red.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <curl/curl.h>

/* Cliente HTTP reutilizable */
struct RedCliente
{
    CURL *curl;
};

/* Inicializar libcurl */
int red_inicializar(void)
{
    CURLcode resultado = curl_global_init(CURL_GLOBAL_DEFAULT);

    if (resultado != CURLE_OK)
    {
        fprintf(stderr, "Error al inicializar libcurl: %s\n",
                curl_easy_strerror(resultado));
        return 1;
    }

    return 0;
}

/* Finalizar libcurl */
void red_finalizar(void)
{
    curl_global_cleanup();
}

/* Liberar una respuesta HTTP */
void red_liberar_respuesta(RedRespuesta *respuesta)
{
    if (respuesta == NULL)
        return;

    free(respuesta->contenido);

    respuesta->contenido = NULL;
    respuesta->longitud = 0;
    respuesta->codigo_http = 0;
    respuesta->retry_after_s = 0;
}

/* Recibir el cuerpo de una respuesta */
static size_t red_recibir_datos(
    void *datos,
    size_t tamano,
    size_t cantidad,
    void *usuario
)
{
    RedRespuesta *respuesta = (RedRespuesta *)usuario;

    if (respuesta == NULL)
        return 0;

    if (tamano != 0 && cantidad > (size_t)-1 / tamano)
        return 0;

    size_t bytes = tamano * cantidad;

    if (bytes == 0 || datos == NULL)
        return 0;

    if (respuesta->longitud == (size_t)-1 ||
        bytes > (size_t)-1 - respuesta->longitud - 1)
        return 0;

    char *nuevo_contenido = realloc(
        respuesta->contenido,
        respuesta->longitud + bytes + 1
    );

    if (nuevo_contenido == NULL)
        return 0;

    respuesta->contenido = nuevo_contenido;

    memcpy(
        respuesta->contenido + respuesta->longitud,
        datos,
        bytes
    );

    respuesta->longitud += bytes;
    respuesta->contenido[respuesta->longitud] = '\0';

    return bytes;
}

/* Capturar el encabezado Retry-After */
static size_t red_recibir_cabecera(
    char *datos,
    size_t tamano,
    size_t cantidad,
    void *usuario
)
{
    RedRespuesta *respuesta = (RedRespuesta *)usuario;

    if (respuesta == NULL || datos == NULL)
        return 0;

    if (tamano != 0 && cantidad > (size_t)-1 / tamano)
        return 0;

    size_t bytes = tamano * cantidad;

    const char nombre[] = "Retry-After:";
    size_t longitud_nombre = sizeof(nombre) - 1;

    if (bytes >= longitud_nombre &&
        strncasecmp(datos, nombre, longitud_nombre) == 0)
    {
        char valor[64];

        size_t inicio = longitud_nombre;

        while (inicio < bytes &&
               isspace((unsigned char)datos[inicio]))
            inicio++;

        size_t longitud = bytes - inicio;

        while (longitud > 0 &&
               isspace((unsigned char)datos[inicio + longitud - 1]))
            longitud--;

        if (longitud > 0 && longitud < sizeof(valor))
        {
            memcpy(valor, datos + inicio, longitud);
            valor[longitud] = '\0';

            char *fin = NULL;
            long segundos = strtol(valor, &fin, 10);

            if (fin != valor && *fin == '\0' && segundos >= 0)
                respuesta->retry_after_s = segundos;
        }
    }

    return bytes;
}

/* Comprobar si una ruta necesita autenticación */
static int red_requiere_autenticacion(const char *ruta)
{
    if (ruta != NULL && strcmp(ruta, "/api/v1/ping") == 0)
        return 0;

    return 1;
}

/* Añadir autenticación Bearer */
static int red_agregar_autenticacion(
    struct curl_slist **cabeceras
)
{
    const char *token = getenv("ITLA_TOKEN");

    if (token == NULL || token[0] == '\0')
    {
        fprintf(stderr,
                "Error: variable ITLA_TOKEN no configurada\n");
        return 1;
    }

    if (strchr(token, '\r') != NULL ||
        strchr(token, '\n') != NULL)
    {
        fprintf(stderr, "Error: token de autenticación no válido\n");
        return 1;
    }

    int longitud = snprintf(
        NULL, 0, "Authorization: Bearer %s", token
    );

    if (longitud < 0)
        return 1;

    char *autorizacion = malloc((size_t)longitud + 1);

    if (autorizacion == NULL)
        return 1;

    snprintf(
        autorizacion,
        (size_t)longitud + 1,
        "Authorization: Bearer %s",
        token
    );

    struct curl_slist *nuevas_cabeceras =
        curl_slist_append(*cabeceras, autorizacion);

    free(autorizacion);

    if (nuevas_cabeceras == NULL)
    {
        fprintf(stderr, "Error al preparar autenticación HTTP\n");
        return 1;
    }

    *cabeceras = nuevas_cabeceras;

    return 0;
}

/* Construir la URL del servidor */
static char *red_construir_url(const char *ruta)
{
    const char *host = getenv("ITLA_HOST");

    if (host == NULL || host[0] == '\0' ||
        ruta == NULL || ruta[0] != '/')
    {
        fprintf(stderr, "Error: servidor o ruta no válidos\n");
        return NULL;
    }

    int longitud = snprintf(
        NULL, 0, "http://%s:4000%s", host, ruta
    );

    if (longitud < 0)
        return NULL;

    char *url = malloc((size_t)longitud + 1);

    if (url == NULL)
        return NULL;

    snprintf(
        url,
        (size_t)longitud + 1,
        "http://%s:4000%s",
        host,
        ruta
    );

    return url;
}

/* Preparar un manejador HTTP tradicional */
static CURL *red_preparar_solicitud(RedRespuesta *respuesta)
{
    if (respuesta == NULL)
        return NULL;

    CURL *curl = curl_easy_init();

    if (curl == NULL)
        return NULL;

    CURLcode resultado;

    resultado = curl_easy_setopt(
        curl, CURLOPT_WRITEFUNCTION, red_recibir_datos
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        curl, CURLOPT_WRITEDATA, respuesta
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        curl, CURLOPT_TIMEOUT, 5L
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        curl, CURLOPT_CONNECTTIMEOUT, 3L
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        curl, CURLOPT_NOSIGNAL, 1L
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        curl, CURLOPT_USERAGENT, "estacion-itla/1.0"
    );

    if (resultado != CURLE_OK)
        goto error;

    return curl;

error:

    fprintf(stderr, "Error al configurar HTTP: %s\n",
            curl_easy_strerror(resultado));

    curl_easy_cleanup(curl);
    return NULL;
}

/* Solicitud HTTP GET */
int red_get(const char *ruta, RedRespuesta *respuesta)
{
    if (respuesta == NULL || ruta == NULL || ruta[0] != '/')
        return 1;

    red_liberar_respuesta(respuesta);

    char *url = red_construir_url(ruta);

    if (url == NULL)
        return 1;

    CURL *curl = red_preparar_solicitud(respuesta);

    if (curl == NULL)
    {
        free(url);
        return 1;
    }

    struct curl_slist *cabeceras = NULL;
    CURLcode resultado;

    if (red_requiere_autenticacion(ruta))
    {
        if (red_agregar_autenticacion(&cabeceras) != 0)
            goto error;

        resultado = curl_easy_setopt(
            curl, CURLOPT_HTTPHEADER, cabeceras
        );

        if (resultado != CURLE_OK)
            goto error;
    }

    resultado = curl_easy_setopt(curl, CURLOPT_URL, url);

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_perform(curl);

    if (resultado != CURLE_OK)
    {
        fprintf(stderr, "Error en solicitud GET: %s\n",
                curl_easy_strerror(resultado));
        goto error;
    }

    resultado = curl_easy_getinfo(
        curl, CURLINFO_RESPONSE_CODE, &respuesta->codigo_http
    );

    if (resultado != CURLE_OK)
        goto error;

    curl_slist_free_all(cabeceras);
    curl_easy_cleanup(curl);
    free(url);

    if (respuesta->codigo_http < 200 ||
        respuesta->codigo_http >= 300)
    {
        fprintf(stderr, "Error HTTP %ld\n",
                respuesta->codigo_http);

        red_liberar_respuesta(respuesta);
        return 1;
    }

    return 0;

error:

    curl_slist_free_all(cabeceras);
    curl_easy_cleanup(curl);
    free(url);
    red_liberar_respuesta(respuesta);

    return 1;
}

/* Solicitud HTTP POST tradicional */
int red_post(
    const char *ruta,
    const char *json,
    RedRespuesta *respuesta
)
{
    if (ruta == NULL || ruta[0] != '/' ||
        json == NULL || respuesta == NULL)
        return 1;

    red_liberar_respuesta(respuesta);

    char *url = red_construir_url(ruta);

    if (url == NULL)
        return 1;

    CURL *curl = red_preparar_solicitud(respuesta);

    if (curl == NULL)
    {
        free(url);
        return 1;
    }

    struct curl_slist *cabeceras = NULL;
    CURLcode resultado;

    cabeceras = curl_slist_append(
        cabeceras, "Content-Type: application/json"
    );

    if (cabeceras == NULL)
        goto error;

    if (red_requiere_autenticacion(ruta))
    {
        if (red_agregar_autenticacion(&cabeceras) != 0)
            goto error;
    }

    resultado = curl_easy_setopt(curl, CURLOPT_URL, url);

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        curl, CURLOPT_HTTPHEADER, cabeceras
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        curl, CURLOPT_POSTFIELDS, json
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        curl, CURLOPT_POSTFIELDSIZE_LARGE,
        (curl_off_t)strlen(json)
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_perform(curl);

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_getinfo(
        curl, CURLINFO_RESPONSE_CODE, &respuesta->codigo_http
    );

    if (resultado != CURLE_OK)
        goto error;

    curl_slist_free_all(cabeceras);
    curl_easy_cleanup(curl);
    free(url);

    if (respuesta->codigo_http < 200 ||
        respuesta->codigo_http >= 300)
    {
        fprintf(stderr, "Error HTTP %ld\n",
                respuesta->codigo_http);

        red_liberar_respuesta(respuesta);
        return 1;
    }

    return 0;

error:

    fprintf(stderr, "Error en solicitud POST: %s\n",
            curl_easy_strerror(resultado));

    curl_slist_free_all(cabeceras);
    curl_easy_cleanup(curl);
    free(url);
    red_liberar_respuesta(respuesta);

    return 1;
}

/* Crear un cliente HTTP reutilizable */
RedCliente *red_cliente_crear(void)
{
    RedCliente *cliente = malloc(sizeof(RedCliente));

    if (cliente == NULL)
        return NULL;

    cliente->curl = curl_easy_init();

    if (cliente->curl == NULL)
    {
        free(cliente);
        return NULL;
    }

    return cliente;
}

/* Destruir el cliente HTTP */
void red_cliente_destruir(RedCliente *cliente)
{
    if (cliente == NULL)
        return;

    if (cliente->curl != NULL)
        curl_easy_cleanup(cliente->curl);

    free(cliente);
}

/* POST reutilizando el mismo manejador de libcurl */
int red_cliente_post(
    RedCliente *cliente,
    const char *ruta,
    const char *json,
    RedRespuesta *respuesta
)
{
    if (cliente == NULL || cliente->curl == NULL ||
        ruta == NULL || ruta[0] != '/' ||
        json == NULL || respuesta == NULL)
        return 1;

    red_liberar_respuesta(respuesta);

    char *url = red_construir_url(ruta);

    if (url == NULL)
        return 1;

    struct curl_slist *cabeceras = NULL;
    CURLcode resultado;

    /* Restablecer opciones sin destruir el manejador */
    curl_easy_reset(cliente->curl);

    cabeceras = curl_slist_append(
        cabeceras, "Content-Type: application/json"
    );

    if (cabeceras == NULL)
        goto error;

    if (red_requiere_autenticacion(ruta))
    {
        if (red_agregar_autenticacion(&cabeceras) != 0)
            goto error;
    }

    resultado = curl_easy_setopt(
        cliente->curl, CURLOPT_URL, url
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        cliente->curl, CURLOPT_HTTPHEADER, cabeceras
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        cliente->curl, CURLOPT_WRITEFUNCTION, red_recibir_datos
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        cliente->curl, CURLOPT_WRITEDATA, respuesta
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        cliente->curl, CURLOPT_HEADERFUNCTION, red_recibir_cabecera
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        cliente->curl, CURLOPT_HEADERDATA, respuesta
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        cliente->curl, CURLOPT_POSTFIELDS, json
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        cliente->curl,
        CURLOPT_POSTFIELDSIZE_LARGE,
        (curl_off_t)strlen(json)
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        cliente->curl, CURLOPT_TIMEOUT, 5L
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        cliente->curl, CURLOPT_CONNECTTIMEOUT, 3L
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        cliente->curl, CURLOPT_NOSIGNAL, 1L
    );

    if (resultado != CURLE_OK)
        goto error;

    resultado = curl_easy_setopt(
        cliente->curl, CURLOPT_USERAGENT, "estacion-itla/1.0"
    );

    if (resultado != CURLE_OK)
        goto error;

    /* Ejecutar la solicitud */
    resultado = curl_easy_perform(cliente->curl);

    if (resultado != CURLE_OK)
    {
        fprintf(stderr, "Error de conexión HTTP: %s\n",
                curl_easy_strerror(resultado));
        goto error;
    }

    resultado = curl_easy_getinfo(
        cliente->curl,
        CURLINFO_RESPONSE_CODE,
        &respuesta->codigo_http
    );

    if (resultado != CURLE_OK)
        goto error;

    /* Liberar recursos temporales.
       El manejador curl permanece activo. */
    curl_slist_free_all(cabeceras);
    free(url);

    /*
       Importante:
       Un HTTP 503 o 429 no es un error de transporte.
       Se conserva la respuesta para que main.c
       pueda interpretar el código y Retry-After.
    */
    return 0;

error:

    curl_slist_free_all(cabeceras);
    free(url);
    red_liberar_respuesta(respuesta);

    return 1;
}

/* Publicar un mensaje mediante MQTT */
int red_mqtt_publicar(
    const char *topico,
    const char *mensaje
)
{
    const char *host = getenv("ITLA_HOST");
    const char *matricula = getenv("ITLA_MATRICULA");
    const char *token = getenv("ITLA_TOKEN");

    if (host == NULL || host[0] == '\0' ||
        matricula == NULL || matricula[0] == '\0' ||
        token == NULL || token[0] == '\0' ||
        topico == NULL || topico[0] == '\0' ||
        mensaje == NULL || mensaje[0] == '\0')
    {
        fprintf(stderr, "Error: configuración MQTT incompleta.\n");
        return 1;
    }

    if (strchr(host, '\r') != NULL ||
        strchr(host, '\n') != NULL ||
        strchr(host, '/') != NULL ||
        strchr(matricula, '\r') != NULL ||
        strchr(matricula, '\n') != NULL ||
        strchr(token, '\r') != NULL ||
        strchr(token, '\n') != NULL ||
        strchr(topico, '\r') != NULL ||
        strchr(topico, '\n') != NULL ||
        strchr(topico, ' ') != NULL)
    {
        fprintf(stderr, "Error: configuración MQTT no válida.\n");
        return 1;
    }

    int longitud = snprintf(
        NULL, 0, "mqtt://%s:1883/%s", host, topico
    );

    if (longitud < 0)
        return 1;

    char *url = malloc((size_t)longitud + 1);

    if (url == NULL)
        return 1;

    snprintf(
        url,
        (size_t)longitud + 1,
        "mqtt://%s:1883/%s",
        host,
        topico
    );

    CURL *curl = curl_easy_init();

    if (curl == NULL)
    {
        free(url);
        return 1;
    }

    CURLcode resultado;

    resultado = curl_easy_setopt(
        curl, CURLOPT_URL, url
    );

    if (resultado != CURLE_OK)
        goto error_mqtt;

    resultado = curl_easy_setopt(
        curl, CURLOPT_USERNAME, matricula
    );

    if (resultado != CURLE_OK)
        goto error_mqtt;

    resultado = curl_easy_setopt(
        curl, CURLOPT_PASSWORD, token
    );

    if (resultado != CURLE_OK)
        goto error_mqtt;

    resultado = curl_easy_setopt(
        curl, CURLOPT_POSTFIELDS, mensaje
    );

    if (resultado != CURLE_OK)
        goto error_mqtt;

    resultado = curl_easy_setopt(
        curl,
        CURLOPT_POSTFIELDSIZE_LARGE,
        (curl_off_t)strlen(mensaje)
    );

    if (resultado != CURLE_OK)
        goto error_mqtt;

    resultado = curl_easy_setopt(
        curl, CURLOPT_CONNECTTIMEOUT, 3L
    );

    if (resultado != CURLE_OK)
        goto error_mqtt;

    resultado = curl_easy_setopt(
        curl, CURLOPT_TIMEOUT, 5L
    );

    if (resultado != CURLE_OK)
        goto error_mqtt;

    resultado = curl_easy_setopt(
        curl, CURLOPT_NOSIGNAL, 1L
    );

    if (resultado != CURLE_OK)
        goto error_mqtt;

    resultado = curl_easy_perform(curl);

    if (resultado != CURLE_OK)
        goto error_mqtt;

    curl_easy_cleanup(curl);
    free(url);

    return 0;

error_mqtt:

    fprintf(
        stderr,
        "Error al publicar mensaje MQTT: %s\n",
        curl_easy_strerror(resultado)
    );

    curl_easy_cleanup(curl);
    free(url);

    return 1;
}
