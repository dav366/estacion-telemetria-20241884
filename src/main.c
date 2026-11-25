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

/*****************************************************************************************************/
/* ESTACIÓN DE TELEMETRÍA */
/* Programación para Mecatrónicos */
/*****************************************************************************************************/

#include "datos.h"
#include "red.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>

#define LOTE_DESCARGA 200

/* Convertir texto a entero */
static int convertir_entero(const char *texto, int *valor)
{
    char *final;
    long numero;

    errno = 0;
    numero = strtol(texto, &final, 10);

    if (errno != 0 || texto == final || *final != '\0' ||
        numero < 0 || numero > INT_MAX)
        return -1;

    *valor = (int)numero;
    return 0;
}

/* Convertir texto a decimal */
static int convertir_decimal(const char *texto, double *valor)
{
    char *final;

    if (texto == NULL || texto[0] == '\0')
        return -1;

    errno = 0;
    *valor = strtod(texto, &final);

    if (errno != 0 || texto == final || *final != '\0' ||
        !isfinite(*valor))
        return -1;

    return 0;
}

/* Separar una línea CSV conservando los campos vacíos */
static int separar_csv(char *linea, char *campos[], int cantidad)
{
    int total = 0;
    char *inicio = linea;

    for (char *actual = linea; ; actual++)
    {
        if (*actual == ',' || *actual == '\0')
        {
            if (total >= cantidad)
                return -1;

            campos[total++] = inicio;

            if (*actual == '\0')
                break;

            *actual = '\0';
            inicio = actual + 1;
        }
    }

    return total == cantidad ? 0 : -1;
}

/* Procesar una lectura CSV */
static int procesar_lectura(char *linea, DatosLectura *lectura)
{
    char *campos[7];

    if (separar_csv(linea, campos, 7) != 0)
        return -1;

    memset(lectura, 0, sizeof(*lectura));

    if (convertir_entero(campos[0], &lectura->id) != 0 ||
        lectura->id <= 0)
        return -1;

    if (campos[1][0] == '\0' ||
        campos[2][0] == '\0' ||
        strlen(campos[1]) >= sizeof(lectura->maquina) ||
        strlen(campos[2]) >= sizeof(lectura->ts) ||
        strlen(campos[6]) >= sizeof(lectura->estado))
        return -1;

    strcpy(lectura->maquina, campos[1]);
    strcpy(lectura->ts, campos[2]);
    strcpy(lectura->estado, campos[6]);

    lectura->tiene_temp =
        convertir_decimal(campos[3], &lectura->temp) == 0;

    lectura->tiene_corriente =
        convertir_decimal(campos[4], &lectura->corriente) == 0;

    lectura->tiene_vibracion =
        convertir_decimal(campos[5], &lectura->vibracion) == 0;

    return 0;
}

/* Eliminar espacios al inicio y final de una línea */
static void limpiar_linea(char *linea)
{
    size_t longitud = strlen(linea);

    while (longitud > 0 &&
           (linea[longitud - 1] == '\r' ||
            isspace((unsigned char)linea[longitud - 1])))
    {
        linea[--longitud] = '\0';
    }

    size_t inicio = 0;

    while (linea[inicio] != '\0' &&
           isspace((unsigned char)linea[inicio]))
        inicio++;

    if (inicio > 0)
        memmove(linea, linea + inicio, strlen(linea + inicio) + 1);
}

/* Descargar configuración */
static int comando_config(void)
{
    sqlite3 *db = NULL;
    RedRespuesta respuesta = {0};
    int red_iniciada = 0;
    int resultado = 1;

    if (datos_abrir(&db) != 0)
        goto finalizar;

    if (datos_inicializar(db) != 0)
        goto finalizar;

    if (red_inicializar() != 0)
        goto finalizar;

    red_iniciada = 1;

    printf("Solicitando configuración al servidor...\n");

    if (red_get("/api/v1/config", &respuesta) != 0 ||
        respuesta.contenido == NULL)
        goto finalizar;

    int parametros = 0;
    char *linea = strtok(respuesta.contenido, "\n");

    while (linea != NULL)
    {
        limpiar_linea(linea);

        if (linea[0] != '\0')
        {
            char *separador = strchr(linea, '=');

            if (separador == NULL)
                goto finalizar;

            *separador = '\0';

            if (linea[0] == '\0' || separador[1] == '\0')
                goto finalizar;

            if (datos_config_guardar(db, linea, separador + 1) != 0)
                goto finalizar;

            parametros++;
        }

        linea = strtok(NULL, "\n");
    }

    if (parametros != 7)
    {
        fprintf(stderr, "Se esperaban 7 parámetros; recibidos: %d.\n",
                parametros);
        goto finalizar;
    }

    char umbral[50];
    char intervalo[50];
    char hora[50];

    if (datos_config_obtener(db, "umbral_temp", umbral, sizeof(umbral)) != 0 ||
        datos_config_obtener(db, "intervalo_s", intervalo, sizeof(intervalo)) != 0 ||
        datos_config_obtener(db, "hora", hora, sizeof(hora)) != 0)
        goto finalizar;

    printf("\nConfiguración almacenada correctamente.\n");
    printf("Temperatura límite: %s\n", umbral);
    printf("Intervalo: %s segundos\n", intervalo);
    printf("Hora del servidor: %s\n", hora);

    resultado = 0;

finalizar:
    red_liberar_respuesta(&respuesta);

    if (red_iniciada)
        red_finalizar();

    datos_cerrar(db);

    return resultado;
}

/* Descargar todas las lecturas */
static int comando_descargar(void)
{
    sqlite3 *db = NULL;
    RedRespuesta respuesta = {0};

    int red_iniciada = 0;
    int transaccion_activa = 0;
    int resultado = 1;

    int desde_id = 0;
    int ultimo_id = -1;
    int filas_unicas = 0;

    long long suma_ids = 0;

    char ruta[128];

    if (datos_abrir(&db) != 0)
        goto finalizar;

    if (datos_inicializar(db) != 0)
        goto finalizar;

    if (red_inicializar() != 0)
        goto finalizar;

    red_iniciada = 1;

    if (datos_transaccion_iniciar(db) != 0)
    {
        fprintf(stderr, "No se pudo iniciar la transacción.\n");
        goto finalizar;
    }

    transaccion_activa = 1;

    /* La descarga completa sustituye el conjunto anterior */
    if (datos_lecturas_limpiar(db) != 0)
        goto finalizar;

    printf("Iniciando descarga de lecturas...\n");

    while (1)
    {
        int longitud = snprintf(
            ruta,
            sizeof(ruta),
            "/api/v1/lecturas?desde_id=%d&limite=%d",
            desde_id,
            LOTE_DESCARGA
        );

        if (longitud < 0 || (size_t)longitud >= sizeof(ruta))
            goto finalizar;

        if (red_get(ruta, &respuesta) != 0)
        {
            fprintf(stderr, "Error al descargar lecturas.\n");
            goto finalizar;
        }

        if (respuesta.contenido == NULL)
            goto finalizar;

        int cantidad_pagina = 0;
        int nuevo_ultimo_id = ultimo_id;

        char *linea = respuesta.contenido;
        char *siguiente;

        while (linea != NULL)
        {
            siguiente = strchr(linea, '\n');

            if (siguiente != NULL)
            {
                *siguiente = '\0';
                siguiente++;
            }

            limpiar_linea(linea);

            if (linea[0] != '\0')
            {
                /* Ignorar el encabezado CSV */
                if (strncmp(linea, "id,", 3) != 0)
                {
                    DatosLectura lectura = {0};

                    if (procesar_lectura(linea, &lectura) != 0)
                    {
                        fprintf(stderr, "Registro CSV inválido.\n");
                        goto finalizar;
                    }

                    /* Validar el orden de los identificadores */
                    if (lectura.id < desde_id ||
                        (cantidad_pagina > 0 &&
                         lectura.id <= nuevo_ultimo_id))
                    {
                        fprintf(stderr, "Orden de identificadores inválido.\n");
                        goto finalizar;
                    }

                    if (datos_lectura_guardar(db, &lectura) != 0)
                        goto finalizar;

                    nuevo_ultimo_id = lectura.id;
                    cantidad_pagina++;
                }
            }

            linea = siguiente;
        }

        red_liberar_respuesta(&respuesta);

        printf("Página recibida: %d lecturas.\n", cantidad_pagina);

        if (cantidad_pagina == 0 || cantidad_pagina < LOTE_DESCARGA)
            break;

        if (nuevo_ultimo_id <= ultimo_id ||
            nuevo_ultimo_id == INT_MAX)
        {
            fprintf(stderr, "La paginación no avanzó correctamente.\n");
            goto finalizar;
        }

        ultimo_id = nuevo_ultimo_id;
        desde_id = ultimo_id;
    }

    if (datos_lecturas_resumen(db, &filas_unicas, &suma_ids) != 0)
        goto finalizar;

    if (datos_transaccion_confirmar(db) != 0)
    {
        fprintf(stderr, "No se pudo confirmar la descarga.\n");
        goto finalizar;
    }

    transaccion_activa = 0;

    printf("\nDescarga almacenada correctamente.\n");
    printf("Filas únicas: %d\n", filas_unicas);
    printf("Suma de IDs: %lld\n", suma_ids);

    char json[160];

    int longitud_json = snprintf(
        json,
        sizeof(json),
        "{\"filas_unicas\":%d,\"suma_ids\":%lld}",
        filas_unicas,
        suma_ids
    );

    if (longitud_json < 0 ||
        (size_t)longitud_json >= sizeof(json))
        goto finalizar;

    if (red_post(
            "/api/v1/respuestas/descarga",
            json,
            &respuesta) != 0)
    {
        fprintf(stderr, "No se pudo enviar el resultado de descarga.\n");
        goto finalizar;
    }

    printf("Resultado enviado al servidor correctamente.\n");

    resultado = 0;

finalizar:
    red_liberar_respuesta(&respuesta);

    if (transaccion_activa)
        datos_transaccion_cancelar(db);

    if (red_iniciada)
        red_finalizar();

    datos_cerrar(db);

    return resultado;
}

/* Programa principal */
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Uso: estacion <comando>\n");
        printf("Comandos: config, descargar, analizar, enviar, escuchar\n");
        return 1;
    }

    if (strcmp(argv[1], "config") == 0)
        return comando_config();

    if (strcmp(argv[1], "descargar") == 0)
        return comando_descargar();

    if (strcmp(argv[1], "analizar") == 0 ||
        strcmp(argv[1], "enviar") == 0 ||
        strcmp(argv[1], "escuchar") == 0)
    {
        printf("Comando seleccionado: %s\n", argv[1]);
        return 0;
    }

    fprintf(stderr, "Comando desconocido.\n");
    return 1;
}