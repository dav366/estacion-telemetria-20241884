
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
    /* Comprobar que el puntero recibido sea valido */
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
    /* Obtener la estructura donde se almacenara la respuesta */
    RedRespuesta *respuesta = (RedRespuesta *)usuario;

    /* Comprobar que la estructura sea valida */
    if (respuesta == NULL)
    {
        return 0;
    }

    /* Comprobar que la multiplicacion no provoque un desbordamiento */
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

    /* Informar a libcurl cuantos bytes fueron procesados */
    return bytes;
}