
#include <stdio.h>      /* Funciones de entrada y salida */
#include <curl/curl.h>  /* Funciones de la biblioteca libcurl */


/* Funcion principal del programa */
int main(void)
{
    /* Puntero que representa la sesion de comunicacion */
    CURL *curl;

    /* Variable que almacena el resultado de las operaciones */
    CURLcode resultado;


    /* Inicializa los recursos globales de libcurl */
    resultado = curl_global_init(CURL_GLOBAL_DEFAULT);

    /* Comprueba si la inicializacion fue exitosa */
    if (resultado != CURLE_OK)
    {
        printf("Error al inicializar libcurl.\n");
        return 1;
    }


    /* Crea una nueva sesion de comunicacion */
    curl = curl_easy_init();

    /* Comprueba si se pudo crear la sesion */
    if (curl == NULL)
    {
        printf("Error al crear la sesion HTTP.\n");

        /* Libera los recursos globales antes de salir */
        curl_global_cleanup();

        return 1;
    }


    /* Mensaje de confirmacion */
    printf("Sesion HTTP creada correctamente.\n");


    /* Libera la sesion de comunicacion */
    curl_easy_cleanup(curl);

    /* Libera los recursos globales de libcurl */
    curl_global_cleanup();

    return 0;
}
