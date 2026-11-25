/* PRUEBA DEL MÓDULO DE COMUNICACIÓN */

#include <stdio.h>
#include <string.h>

#include "red.h"

int main(int argc, char *argv[])
{
    /* Inicializar la estructura de respuesta */
    RedRespuesta respuesta = {0};

    /* Comprobar los argumentos recibidos */
    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s <ping|config>\n", argv[0]);
        return 1;
    }

    /* Inicializar libcurl */
    if (red_inicializar() != 0)
        return 1;

    int resultado;

    /* Probar el endpoint público */
    if (strcmp(argv[1], "ping") == 0)
    {
        resultado = red_get("/api/v1/ping", &respuesta);
    }
    /* Probar un endpoint protegido */
    else if (strcmp(argv[1], "config") == 0)
    {
        resultado = red_get("/api/v1/config", &respuesta);
    }
    else
    {
        fprintf(stderr, "Prueba no reconocida.\n");
        red_finalizar();
        return 1;
    }

    /* Mostrar el resultado de la solicitud */
    if (resultado == 0)
    {
        printf("Código HTTP: %ld\n", respuesta.codigo_http);
        printf("Respuesta:\n%s\n",
               respuesta.contenido != NULL
                   ? respuesta.contenido
                   : "(sin contenido)");
    }

    /* Liberar los recursos */
    red_liberar_respuesta(&respuesta);
    red_finalizar();

    return resultado;
}