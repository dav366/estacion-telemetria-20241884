#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
    CURLcode resultado;

    resultado = curl_global_init(CURL_GLOBAL_DEFAULT);

    if (resultado != CURLE_OK)
    {
        printf("Error al inicializar libcurl.\n");
        return 1;
    }

    printf("libcurl funciona correctamente.\n");
    printf("Version: %s\n", curl_version());

    curl_global_cleanup();

    return 0;
}