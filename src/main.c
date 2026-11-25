
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

/* Módulos del sistema */
#include "datos.h"
#include "red.h"

/* Bibliotecas necesarias */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Descargar y almacenar la configuración del servidor */
static int comando_config(void)
{
    sqlite3 *db = NULL;
    RedRespuesta respuesta = {0};

    int resultado = 1;
    int red_iniciada = 0;

    /* Abrir la base de datos local */
    if (datos_abrir(&db) != 0)
        goto finalizar;

    /* Crear las tablas necesarias */
    if (datos_inicializar(db) != 0)
        goto finalizar;

    /* Inicializar libcurl */
    if (red_inicializar() != 0)
        goto finalizar;

    red_iniciada = 1;

    /* Solicitar la configuración al servidor */
    printf("Solicitando configuración al servidor...\n");

    if (red_get("/api/v1/config", &respuesta) != 0)
    {
        fprintf(stderr, "No se pudo descargar la configuración.\n");
        goto finalizar;
    }

    /* Comprobar que exista contenido */
    if (respuesta.contenido == NULL)
    {
        fprintf(stderr, "El servidor no devolvió una configuración.\n");
        goto finalizar;
    }

    /* Procesar los parámetros recibidos */
    char *linea = strtok(respuesta.contenido, "\n");

    int parametros = 0;

    while (linea != NULL)
    {
        /* Buscar el separador entre clave y valor */
        char *separador = strchr(linea, '=');

        if (separador == NULL)
        {
            fprintf(stderr, "Error: parámetro de configuración inválido.\n");
            goto finalizar;
        }

        /* Separar la clave del valor */
        *separador = '\0';

        const char *clave = linea;
        const char *valor = separador + 1;

        /* Comprobar que ambos campos tengan contenido */
        if (clave[0] == '\0' || valor[0] == '\0')
        {
            fprintf(stderr, "Error: clave o valor vacío.\n");
            goto finalizar;
        }

        /* Guardar el parámetro en SQLite */
        if (datos_config_guardar(db, clave, valor) != 0)
        {
            fprintf(stderr, "Error al guardar el parámetro: %s\n", clave);
            goto finalizar;
        }

        parametros++;

        linea = strtok(NULL, "\n");
    }

    /* Comprobar que se recibieron los siete parámetros */
    if (parametros != 7)
    {
        fprintf(stderr,
                "Error: se esperaban 7 parámetros y se recibieron %d.\n",
                parametros);
        goto finalizar;
    }

    printf("\nConfiguración descargada y almacenada correctamente.\n");
    printf("Parámetros guardados: %d\n", parametros);

    resultado = 0;

finalizar:

    /* Liberar la respuesta HTTP */
    red_liberar_respuesta(&respuesta);

    /* Finalizar libcurl si fue inicializado */
    if (red_iniciada)
        red_finalizar();

    /* Cerrar la base de datos */
    datos_cerrar(db);

    return resultado;
}

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
        return comando_config();
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
