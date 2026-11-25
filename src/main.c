
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
#include <errno.h>
#include <limits.h>

/* Cantidad máxima de lecturas por solicitud */
#define LOTE_DESCARGA 200

/* Convertir texto a número entero */
static int convertir_entero(const char *texto, int *valor)
{
    char *final;
    long numero;

    errno = 0;
    numero = strtol(texto, &final, 10);

    if (errno != 0 || texto == final || *final != '\0' ||
        numero < 0 || numero > INT_MAX)
    {
        return -1;
    }

    *valor = (int)numero;

    return 0;
}

/* Convertir texto a número decimal */
static int convertir_decimal(const char *texto, double *valor)
{
    char *final;

    errno = 0;
    *valor = strtod(texto, &final);

    if (errno != 0 || texto == final || *final != '\0')
    {
        return -1;
    }

    return 0;
}

/* Procesar una línea recibida del servidor */
static int procesar_lectura(char *linea, DatosLectura *lectura)
{
    char *campos[7];

    campos[0] = linea;

    /* Separar los siete campos mediante comas */
    for (int i = 0; i < 6; i++)
    {
        char *separador = strchr(campos[i], ',');

        if (separador == NULL)
            return -1;

        *separador = '\0';
        campos[i + 1] = separador + 1;
    }

    /* Comprobar que no existan campos adicionales */
    if (strchr(campos[6], ',') != NULL)
        return -1;

    /* Comprobar que ningún campo esté vacío */
    for (int i = 0; i < 7; i++)
    {
        if (campos[i][0] == '\0')
            return -1;
    }

    /* Validar los tamaños de los campos de texto */
    if (strlen(campos[1]) >= sizeof(lectura->maquina) ||
        strlen(campos[2]) >= sizeof(lectura->ts) ||
        strlen(campos[6]) >= sizeof(lectura->estado))
    {
        return -1;
    }

    /* Convertir los campos numéricos */
    if (convertir_entero(campos[0], &lectura->id) != 0 ||
        convertir_decimal(campos[3], &lectura->temp) != 0 ||
        convertir_decimal(campos[4], &lectura->corriente) != 0 ||
        convertir_decimal(campos[5], &lectura->vibracion) != 0)
    {
        return -1;
    }

    /* Copiar los campos de texto */
    strcpy(lectura->maquina, campos[1]);
    strcpy(lectura->ts, campos[2]);
    strcpy(lectura->estado, campos[6]);

    return 0;
}

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

/* Descargar y almacenar las lecturas de la planta */
static int comando_descargar(void)
{
    sqlite3 *db = NULL;
    RedRespuesta respuesta = {0};

    int resultado = 1;
    int red_iniciada = 0;
    int desde_id = 1;
    int total_lecturas = 0;

    char ruta[100];

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

    printf("Iniciando descarga de lecturas...\n");

    /* Descargar los registros por lotes */
    while (1)
    {
        int cantidad_pagina = 0;
        int ultimo_id = desde_id - 1;

        /* Construir la ruta con los parámetros de paginación */
        int longitud = snprintf(
            ruta,
            sizeof(ruta),
            "/api/v1/lecturas?desde_id=%d&limite=%d",
            desde_id,
            LOTE_DESCARGA
        );

        if (longitud < 0 || (size_t)longitud >= sizeof(ruta))
        {
            fprintf(stderr, "Error al construir la solicitud.\n");
            goto finalizar;
        }

        /* Solicitar las lecturas al servidor */
        if (red_get(ruta, &respuesta) != 0)
        {
            fprintf(stderr, "Error al descargar las lecturas.\n");
            goto finalizar;
        }

        /* Comprobar si existen registros */
        if (respuesta.contenido == NULL ||
            respuesta.longitud == 0)
        {
            red_liberar_respuesta(&respuesta);
            break;
        }

        char *linea = respuesta.contenido;

        /* Procesar cada registro recibido */
        while (linea != NULL && *linea != '\0')
        {
            char *siguiente = strchr(linea, '\n');

            if (siguiente != NULL)
            {
                *siguiente = '\0';
                siguiente++;
            }

            /* Eliminar retorno de carro si existe */
            size_t longitud_linea = strlen(linea);

            if (longitud_linea > 0 &&
                linea[longitud_linea - 1] == '\r')
            {
                linea[longitud_linea - 1] = '\0';
            }

            /* Ignorar líneas vacías */
            if (linea[0] != '\0')
            {
                DatosLectura lectura = {0};

                /* Validar y procesar el registro */
                if (procesar_lectura(linea, &lectura) != 0)
                {
                    fprintf(stderr, "Error: lectura recibida inválida.\n");
                    red_liberar_respuesta(&respuesta);
                    goto finalizar;
                }

                /* Comprobar el orden de los identificadores */
                if (lectura.id < desde_id ||
                    lectura.id <= ultimo_id)
                {
                    fprintf(stderr, "Error: identificador de lectura inválido.\n");
                    red_liberar_respuesta(&respuesta);
                    goto finalizar;
                }

                /* Guardar la lectura en SQLite */
                if (datos_lectura_guardar(db, &lectura) != 0)
                {
                    fprintf(stderr, "Error al almacenar una lectura.\n");
                    red_liberar_respuesta(&respuesta);
                    goto finalizar;
                }

                ultimo_id = lectura.id;
                cantidad_pagina++;
                total_lecturas++;
            }

            linea = siguiente;
        }

        /* Liberar la respuesta del servidor */
        red_liberar_respuesta(&respuesta);

        printf("Lote procesado: %d lecturas.\n", cantidad_pagina);

        /* Si no hay más registros, finalizar */
        if (cantidad_pagina == 0 ||
            cantidad_pagina < LOTE_DESCARGA)
        {
            break;
        }

        /* Preparar el identificador para el siguiente lote */
        if (ultimo_id == INT_MAX)
        {
            fprintf(stderr, "Error: se alcanzó el límite de identificadores.\n");
            goto finalizar;
        }

        desde_id = ultimo_id + 1;
    }

    printf("\nDescarga completada.\n");
    printf("Lecturas procesadas: %d\n", total_lecturas);

    resultado = 0;

finalizar:

    /* Liberar cualquier respuesta pendiente */
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
        return comando_descargar();
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
