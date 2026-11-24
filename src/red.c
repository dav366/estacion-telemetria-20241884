
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
