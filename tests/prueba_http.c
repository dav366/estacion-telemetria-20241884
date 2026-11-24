
#include <stdio.h>      /* Funciones de entrada y salida */
#include <curl/curl.h>  /* Funciones de la biblioteca libcurl */

/* Esta funcion es utilizada como callback por libcurl. */
size_t recibir_datos(void *contenido, size_t tamano,
                     size_t cantidad, void *usuario)
{
    /* Calcula la cantidad total de bytes recibidos */
    size_t bytes = tamano * cantidad;

    /* Muestra los datos recibidos en la salida estandar */
    fwrite(contenido, tamano, cantidad, stdout);

    /* Informa a libcurl cuantos bytes fueron procesados */
    return bytes;
}

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

    /* Configura la direccion del servidor HTTP */
    curl_easy_setopt(curl, CURLOPT_URL,
                 "https://httpbin.org/get");

    /* Indica que funcion procesara los datos recibidos */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                 recibir_datos);

    /* Ejecuta la peticion HTTP */
    resultado = curl_easy_perform(curl);

    /* Comprueba si ocurrio algun error */
    if (resultado != CURLE_OK)
    {
    printf("\nError HTTP: %s\n",
           curl_easy_strerror(resultado));
    }
    /* Libera la sesion de comunicacion */
    curl_easy_cleanup(curl);

    /* Libera los recursos globales de libcurl */
    curl_global_cleanup();

    return resultado == CURLE_OK ? 0 : 1;
}
