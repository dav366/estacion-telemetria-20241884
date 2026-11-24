
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

