
/*****************************************************************************************************/
/*           Programación para mecatrónicos                                                          */
/* Nombre:    Omar David Guzmán Guerrero                                                             */
/* Matricula: 2024-1884                                                                              */
/* Correo:    20241884@itla.edu.do                                                                   */
/* Seccion:   Sabados                                                                                */
/* Practica:  Estación de Telemetría (Práctica 1)                                                    */
/* Fecha de entrega: 26/11/2026                                                                      */
/* Link Repositorio GitHub: https://github.com/dav366/estacion-telemetria-20241884                   */
/*****************************************************************************************************/

/* ESTACIÓN DE TELEMETRÍA */

/* Librerías necesarias */
#include <stdio.h>
#include <string.h>

/* Función principal del programa */
int main(int argc, char *argv[])
{
    /* Validar que se haya proporcionado exactamente un comando */
    if (argc != 2)
    {
        printf("Uso: estacion <comando>\n");
        printf("Comandos disponibles:\n");
        printf("  config\n");
        printf("  descargar\n");
        printf("  analizar\n");
        printf("  enviar\n");
        printf("  escuchar\n");

        return 1;
    }

    /* Identificar y procesar el comando solicitado */
    if (strcmp(argv[1], "config") == 0)
    {
        printf("Comando seleccionado: config\n");
    }
    else if (strcmp(argv[1], "descargar") == 0)
    {
        printf("Comando seleccionado: descargar\n");
    }
    else if (strcmp(argv[1], "analizar") == 0)
    {
        printf("Comando seleccionado: analizar\n");
    }
    else if (strcmp(argv[1], "enviar") == 0)
    {
        printf("Comando seleccionado: enviar\n");
    }
    else if (strcmp(argv[1], "escuchar") == 0)
    {
        printf("Comando seleccionado: escuchar\n");
    }
    else
    {
        /* Informar si el comando introducido no existe */
        printf("Error: comando desconocido.\n");
        return 1;
    }

    return 0;
}
