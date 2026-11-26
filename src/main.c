
#define _POSIX_C_SOURCE 200809L

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

#include "datos.h"
#include "red.h"
#include "sensor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>

#define LOTE_DESCARGA 200
#define CANTIDAD_MAQUINAS 4
#define INTERVALO_REVISION_MS 100
#define INTERVALO_ENVIO_MS 250

/* Variable para detener el envío de forma controlada */
static volatile sig_atomic_t detener_envio = 0;

/* Manejar la señal de interrupción */
static void manejar_interrupcion(int senal)
{
    (void)senal;
    detener_envio = 1;
}

/* Obtener tiempo monotónico en milisegundos */
static long long obtener_tiempo_ms(void)
{
    struct timespec tiempo;

    if (clock_gettime(CLOCK_MONOTONIC, &tiempo) != 0)
        return -1;

    return (long long)tiempo.tv_sec * 1000LL
         + (long long)tiempo.tv_nsec / 1000000LL;
}

/* Esperar una cantidad de milisegundos */
static void esperar_ms(long milisegundos)
{
    struct timespec espera;

    espera.tv_sec = milisegundos / 1000;
    espera.tv_nsec = (milisegundos % 1000) * 1000000L;

    while (nanosleep(&espera, &espera) != 0 && errno == EINTR)
    {
        if (detener_envio)
            break;
    }
}

/* Calcular espera exponencial con máximo de 30 segundos */
static int calcular_reintento(int reintento)
{
    int espera = 1;

    for (int i = 1; i < reintento && espera < 30; i++)
        espera *= 2;

    if (espera > 30)
        espera = 30;

    return espera;
}

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
                if (strncmp(linea, "id,", 3) != 0)
                {
                    DatosLectura lectura = {0};

                    if (procesar_lectura(linea, &lectura) != 0)
                    {
                        fprintf(stderr, "Registro CSV inválido.\n");
                        goto finalizar;
                    }

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

/* Analizar las lecturas y enviar los resultados */
static int comando_analizar(void)
{
    sqlite3 *db = NULL;
    RedRespuesta respuesta = {0};

    int red_iniciada = 0;
    int resultado = 1;

    char texto_umbral[50];
    double umbral;

    DatosAnalisis analisis[CANTIDAD_MAQUINAS] = {0};
    int cantidad = 0;

    char json[2048];
    size_t posicion = 0;

    if (datos_abrir(&db) != 0)
        goto finalizar;

    if (datos_inicializar(db) != 0)
        goto finalizar;

    /* Recuperar el umbral guardado durante la configuración */
    if (datos_config_obtener(
            db, "umbral_temp",
            texto_umbral, sizeof(texto_umbral)) != 0)
    {
        fprintf(stderr, "No se encontró el umbral de temperatura. "
                        "Ejecute primero: estacion config\n");
        goto finalizar;
    }

    if (convertir_decimal(texto_umbral, &umbral) != 0)
    {
        fprintf(stderr, "El umbral de temperatura no es válido.\n");
        goto finalizar;
    }

    /* Calcular las estadísticas mediante SQLite */
    if (datos_lecturas_analizar(
            db,
            umbral,
            analisis,
            CANTIDAD_MAQUINAS,
            &cantidad) != 0)
    {
        fprintf(stderr, "No se pudieron analizar las lecturas.\n");
        goto finalizar;
    }

    if (cantidad != CANTIDAD_MAQUINAS)
    {
        fprintf(stderr, "Se esperaban %d máquinas, pero se encontraron %d.\n",
                CANTIDAD_MAQUINAS, cantidad);
        goto finalizar;
    }

    printf("\nANÁLISIS DE LECTURAS\n");
    printf("Umbral de temperatura: %.2f °C\n\n", umbral);

    /* Construir el objeto JSON */
    int escritos = snprintf(
        json,
        sizeof(json),
        "{\"maquinas\":["
    );

    if (escritos < 0 || (size_t)escritos >= sizeof(json))
        goto finalizar;

    posicion = (size_t)escritos;

    for (int i = 0; i < cantidad; i++)
    {
        DatosAnalisis *dato = &analisis[i];

        for (size_t j = 0; dato->maquina[j] != '\0'; j++)
        {
            unsigned char caracter = (unsigned char)dato->maquina[j];

            if (!isalnum(caracter) && caracter != '_' && caracter != '-')
            {
                fprintf(stderr, "Nombre de máquina no válido.\n");
                goto finalizar;
            }
        }

        if (dato->validas > 0)
        {
            printf("Máquina: %s\n", dato->maquina);
            printf("  Lecturas válidas: %d\n", dato->validas);
            printf("  Lecturas descartadas: %d\n", dato->descartadas);
            printf("  Temperatura mínima: %.2f\n", dato->temp_min);
            printf("  Temperatura máxima: %.2f\n", dato->temp_max);
            printf("  Temperatura promedio: %.2f\n", dato->temp_prom);
            printf("  Sobre el umbral: %d\n\n", dato->sobre_umbral);
        }
        else
        {
            printf("Máquina: %s\n", dato->maquina);
            printf("  Lecturas válidas: 0\n");
            printf("  Lecturas descartadas: %d\n\n", dato->descartadas);
        }

        escritos = snprintf(
            json + posicion,
            sizeof(json) - posicion,
            "%s{"
            "\"maquina\":\"%s\","
            "\"validas\":%d,"
            "\"descartadas\":%d,"
            "\"temp_min\":%.2f,"
            "\"temp_max\":%.2f,"
            "\"temp_prom\":%.2f,"
            "\"sobre_umbral\":%d"
            "}",
            i == 0 ? "" : ",",
            dato->maquina,
            dato->validas,
            dato->descartadas,
            dato->temp_min,
            dato->temp_max,
            dato->temp_prom,
            dato->sobre_umbral
        );

        if (escritos < 0 ||
            (size_t)escritos >= sizeof(json) - posicion)
        {
            fprintf(stderr, "El JSON de análisis excede su capacidad.\n");
            goto finalizar;
        }

        posicion += (size_t)escritos;
    }

    escritos = snprintf(
        json + posicion,
        sizeof(json) - posicion,
        "]}"
    );

    if (escritos < 0 ||
        (size_t)escritos >= sizeof(json) - posicion)
        goto finalizar;

    printf("Análisis calculado correctamente.\n");

    if (red_inicializar() != 0)
        goto finalizar;

    red_iniciada = 1;

    printf("Enviando resultados de análisis al servidor...\n");

    if (red_post(
            "/api/v1/respuestas/analisis",
            json,
            &respuesta) != 0)
    {
        fprintf(stderr, "No se pudo enviar el análisis al servidor.\n");
        goto finalizar;
    }

    printf("Análisis enviado correctamente.\n");
    printf("Código HTTP: %ld\n", respuesta.codigo_http);

    if (respuesta.contenido != NULL &&
        respuesta.contenido[0] != '\0')
    {
        printf("Respuesta del servidor: %s\n", respuesta.contenido);
    }

    resultado = 0;

finalizar:
    red_liberar_respuesta(&respuesta);

    if (red_iniciada)
        red_finalizar();

    datos_cerrar(db);

    return resultado;
}

/* Generar, almacenar y enviar telemetría */
static int comando_enviar(void)
{
    sqlite3 *db = NULL;
    RedCliente *cliente = NULL;

    int red_iniciada = 0;
    int resultado = 1;

    char texto_intervalo[50];
    int intervalo_s = 0;

    const char *matricula = getenv("ITLA_MATRICULA");
    char topico[256];
    char mensaje[100];

    int nivel_reintento = 1;

    long long proximo_envio = 0;
    long long proxima_lectura = 0;
    long long proximo_latido = 0;
    long long ultima_seq = 0;

    struct sigaction accion_anterior;
    struct sigaction accion;

    if (matricula == NULL || matricula[0] == '\0')
    {
        fprintf(stderr, "Falta configurar ITLA_MATRICULA.\n");
        goto finalizar;
    }

    int longitud_topico = snprintf(
        topico,
        sizeof(topico),
        "itla/estudiantes/%s/latido",
        matricula
    );

    if (longitud_topico < 0 ||
        (size_t)longitud_topico >= sizeof(topico))
    {
        fprintf(stderr, "El tópico MQTT es demasiado largo.\n");
        goto finalizar;
    }

    if (datos_abrir(&db) != 0)
        goto finalizar;

    if (datos_inicializar(db) != 0)
        goto finalizar;

    if (datos_telemetria_obtener_ultima_seq(
            db, &ultima_seq) != 0)
        goto finalizar;

    if (datos_config_obtener(
            db, "intervalo_s",
            texto_intervalo, sizeof(texto_intervalo)) != 0)
    {
        fprintf(stderr, "No existe intervalo_s. Ejecute primero: estacion config\n");
        goto finalizar;
    }

    if (convertir_entero(texto_intervalo, &intervalo_s) != 0 ||
        intervalo_s <= 0)
    {
        fprintf(stderr, "El intervalo de envío no es válido.\n");
        goto finalizar;
    }

    if (red_inicializar() != 0)
        goto finalizar;

    red_iniciada = 1;

    cliente = red_cliente_crear();

    if (cliente == NULL)
    {
        fprintf(stderr, "No se pudo crear el cliente HTTP.\n");
        goto finalizar;
    }

    sensor_inicializar();

    memset(&accion, 0, sizeof(accion));
    accion.sa_handler = manejar_interrupcion;
    sigemptyset(&accion.sa_mask);

    if (sigaction(SIGINT, &accion, &accion_anterior) != 0)
    {
        fprintf(stderr, "No se pudo configurar SIGINT.\n");
        goto finalizar;
    }

    long long ahora = obtener_tiempo_ms();

    if (ahora < 0)
        goto finalizar;

    proxima_lectura = ahora;
    proximo_envio = ahora;
    proximo_latido = ahora;

    printf("\nESTACIÓN DE TELEMETRÍA INICIADA\n");
    printf("Intervalo de lectura: %d segundos\n", intervalo_s);
    printf("Latido MQTT: cada 10 segundos\n");
    printf("Las lecturas se almacenan antes de enviarse.\n");
    printf("Presione Ctrl+C para detener el proceso.\n\n");

    while (!detener_envio)
    {
        ahora = obtener_tiempo_ms();

        if (ahora < 0)
        {
            fprintf(stderr, "No se pudo obtener el tiempo del sistema.\n");
            break;
        }

        /* Generar y guardar una lectura */
        if (ahora >= proxima_lectura)
        {
            DatosTelemetria lectura = {0};

            time_t instante = time(NULL);

            if (instante == (time_t)-1)
            {
                fprintf(stderr, "No se pudo obtener la hora actual.\n");
                break;
            }

            lectura.ts = (long long)instante;

            sensor_generar(
                &lectura.temp,
                &lectura.corriente
            );

            if (datos_telemetria_guardar(db, &lectura) != 0)
            {
                fprintf(stderr, "No se pudo guardar la lectura.\n");
                break;
            }

            ultima_seq = lectura.seq;

            printf(
                "Lectura guardada | seq=%lld | temp=%.2f °C | corriente=%.2f A\n",
                lectura.seq,
                lectura.temp,
                lectura.corriente
            );

            proxima_lectura =
                ahora + (long long)intervalo_s * 1000LL;
        }

        /* Publicar el latido MQTT cada 10 segundos */
        if (ahora >= proximo_latido)
        {
            int pendientes = 0;

            if (datos_telemetria_contar_pendientes(
                    db, &pendientes) != 0)
            {
                fprintf(stderr,
                        "No se pudo contar la telemetría pendiente para el latido.\n");
            }
            else
            {
                time_t instante = time(NULL);

                if (instante == (time_t)-1)
                {
                    fprintf(stderr,
                            "No se pudo obtener la hora del latido.\n");
                }
                else
                {
                    int longitud = snprintf(
                        mensaje,
                        sizeof(mensaje),
                        "LATIDO;%lld;%d;%lld\n",
                        ultima_seq,
                        pendientes,
                        (long long)instante
                    );

                    if (longitud < 0 ||
                        (size_t)longitud >= sizeof(mensaje))
                    {
                        fprintf(stderr,
                                "No se pudo construir el mensaje MQTT.\n");
                    }
                    else if (red_mqtt_publicar(topico, mensaje) != 0)
                    {
                        fprintf(stderr,
                                "Latido MQTT descartado; continúa el envío HTTP.\n");
                    }
                    else
                    {
                        printf(
                            "Latido MQTT publicado | seq=%lld | pendientes=%d\n",
                            ultima_seq,
                            pendientes
                        );
                    }
                }
            }

            proximo_latido = ahora + 10000LL;
        }

        /* Envío HTTP de la telemetría pendiente */
        if (ahora >= proximo_envio)
        {
            DatosTelemetria pendiente = {0};

            int estado = datos_telemetria_obtener_pendiente(
                db, &pendiente
            );

            if (estado < 0)
            {
                fprintf(stderr,
                        "Error al consultar la cola de telemetría.\n");
                break;
            }

            if (estado == 0)
            {
                char json[256];

                int longitud = snprintf(
                    json,
                    sizeof(json),
                    "{\"seq\":%lld,\"ts\":%lld,"
                    "\"temp\":%.2f,\"corriente\":%.2f}",
                    pendiente.seq,
                    pendiente.ts,
                    pendiente.temp,
                    pendiente.corriente
                );

                if (longitud < 0 ||
                    (size_t)longitud >= sizeof(json))
                {
                    fprintf(stderr, "No se pudo construir el JSON.\n");
                    break;
                }

                if (datos_telemetria_incrementar_intentos(
                        db, pendiente.seq) != 0)
                {
                    fprintf(stderr,
                            "No se pudo registrar el intento.\n");
                    break;
                }

                RedRespuesta respuesta = {0};

                int error_red = red_cliente_post(
                    cliente,
                    "/api/v1/telemetria",
                    json,
                    &respuesta
                );

                if (error_red != 0)
                {
                    int espera =
                        calcular_reintento(nivel_reintento);

                    printf(
                        "Error de conexión. Reintentando en %d segundos.\n",
                        espera
                    );

                    if (nivel_reintento < 6)
                        nivel_reintento++;

                    proximo_envio =
                        ahora + (long long)espera * 1000LL;

                    red_liberar_respuesta(&respuesta);
                }
                else if (respuesta.codigo_http == 200 ||
                         respuesta.codigo_http == 201)
                {
                    if (datos_telemetria_marcar_enviada(
                            db, pendiente.seq) != 0)
                    {
                        fprintf(stderr,
                                "No se pudo actualizar el estado enviado.\n");
                        red_liberar_respuesta(&respuesta);
                        break;
                    }

                    printf(
                        "Lectura enviada | seq=%lld | HTTP %ld\n",
                        pendiente.seq,
                        respuesta.codigo_http
                    );

                    nivel_reintento = 1;
                    proximo_envio =
                        ahora + INTERVALO_ENVIO_MS;

                    red_liberar_respuesta(&respuesta);
                }
                else if (respuesta.codigo_http == 429 ||
                         respuesta.codigo_http == 503)
                {
                    int espera;

                    if (respuesta.retry_after_s > 0 &&
                        respuesta.retry_after_s <= 86400)
                    {
                        espera = (int)respuesta.retry_after_s;
                    }
                    else
                    {
                        espera =
                            calcular_reintento(nivel_reintento);

                        if (nivel_reintento < 6)
                            nivel_reintento++;
                    }

                    printf(
                        "Servidor temporalmente no disponible "
                        "(HTTP %ld). Reintentando en %d segundos.\n",
                        respuesta.codigo_http,
                        espera
                    );

                    proximo_envio =
                        ahora + (long long)espera * 1000LL;

                    red_liberar_respuesta(&respuesta);
                }
                else
                {
                    fprintf(
                        stderr,
                        "Respuesta HTTP inesperada: %ld. "
                        "La lectura permanece pendiente.\n",
                        respuesta.codigo_http
                    );

                    red_liberar_respuesta(&respuesta);
                    break;
                }
            }
            else
            {
                proximo_envio =
                    ahora + INTERVALO_ENVIO_MS;
            }
        }

        esperar_ms(INTERVALO_REVISION_MS);
    }

    printf("\nDeteniendo estación de telemetría...\n");

    resultado = 0;

finalizar:
    if (cliente != NULL)
        red_cliente_destruir(cliente);

    if (red_iniciada)
        red_finalizar();

    datos_cerrar(db);

    return resultado;
}


/* Contexto compartido con el callback de recepción MQTT */
typedef struct
{
    sqlite3 *db;
} ContextoAlarmas;

/* Procesar mensajes MQTT de alarmas */
static int procesar_mensaje_alarma(
    const char *topico,
    const char *mensaje,
    size_t longitud,
    void *contexto
)
{
    ContextoAlarmas *datos = contexto;
    char copia[256];
    char *campos[4];
    size_t longitudes[4];
    size_t cantidad_campos = 1;
    size_t longitud_util;
    DatosAlarma alarma = {0};
    RedRespuesta respuesta = {0};
    char ts_recibida[30];
    char ts_ack[30];
    char json[96];
    int confirmada = 0;
    int escritos;

    if (topico == NULL || mensaje == NULL ||
        datos == NULL || datos->db == NULL ||
        longitud == 0 || longitud >= sizeof(copia))
    {
        fprintf(stderr, "Mensaje MQTT inválido.\n");
        return 0;
    }

    if (memchr(mensaje, '\0', longitud) != NULL)
    {
        fprintf(stderr, "El mensaje MQTT contiene datos inválidos.\n");
        return 0;
    }

    memcpy(copia, mensaje, longitud);
    copia[longitud] = '\0';

    longitud_util = longitud;

    while (longitud_util > 0 &&
           (copia[longitud_util - 1] == '\n' ||
            copia[longitud_util - 1] == '\r'))
    {
        copia[--longitud_util] = '\0';
    }

    if (longitud_util == 0)
        return 0;

    campos[0] = copia;

    for (size_t i = 0; i < longitud_util; i++)
    {
        if (copia[i] == ';')
        {
            if (cantidad_campos >= 4)
            {
                fprintf(stderr, "Mensaje MQTT con demasiados campos.\n");
                return 0;
            }

            copia[i] = '\0';
            campos[cantidad_campos++] = &copia[i + 1];
        }
    }

    for (size_t i = 0; i < cantidad_campos; i++)
    {
        longitudes[i] = strlen(campos[i]);

        if (longitudes[i] == 0)
        {
            fprintf(stderr, "Mensaje MQTT con campos vacíos.\n");
            return 0;
        }
    }

    if (strcmp(campos[0], "NINGUNA") == 0)
    {
        if (cantidad_campos != 2)
            fprintf(stderr, "Formato NINGUNA inválido.\n");

        return 0;
    }

    if (strcmp(campos[0], "ALARMA") != 0 ||
        cantidad_campos != 4)
    {
        fprintf(stderr, "Tipo o formato de mensaje MQTT desconocido.\n");
        return 0;
    }

    if (longitudes[1] >= sizeof(alarma.codigo) ||
        longitudes[2] >= sizeof(alarma.maquina) ||
        longitudes[3] >= sizeof(alarma.ts_servidor))
    {
        fprintf(stderr, "Los campos de la alarma exceden su capacidad.\n");
        return 0;
    }

    for (size_t i = 0; i < longitudes[1]; i++)
    {
        unsigned char caracter = (unsigned char)campos[1][i];

        if (!isalnum(caracter) &&
            caracter != '_' && caracter != '-')
        {
            fprintf(stderr, "Código de alarma no válido.\n");
            return 0;
        }
    }

    snprintf(alarma.codigo, sizeof(alarma.codigo), "%s", campos[1]);
    snprintf(alarma.maquina, sizeof(alarma.maquina), "%s", campos[2]);
    snprintf(alarma.ts_servidor, sizeof(alarma.ts_servidor),
             "%s", campos[3]);

    snprintf(ts_recibida, sizeof(ts_recibida), "%lld",
             (long long)time(NULL));

    snprintf(alarma.ts_recibida, sizeof(alarma.ts_recibida),
             "%s", ts_recibida);

    if (datos_alarma_guardar(datos->db, &alarma) != 0)
    {
        fprintf(stderr, "No se pudo almacenar la alarma %s.\n",
                alarma.codigo);
        return 0;
    }

    if (datos_alarma_confirmada(
            datos->db, alarma.codigo, &confirmada) != 0)
        return 0;

    if (confirmada)
    {
        printf("Alarma %s ya estaba confirmada; se ignora.\n",
               alarma.codigo);
        return 0;
    }

    printf("Alarma recibida | Código: %s | Máquina: %s\n",
           alarma.codigo, alarma.maquina);

    escritos = snprintf(
        json, sizeof(json),
        "{\"codigo\":\"%s\"}", alarma.codigo
    );

    if (escritos < 0 || (size_t)escritos >= sizeof(json))
    {
        fprintf(stderr, "No se pudo construir el ACK de la alarma.\n");
        return 0;
    }

    if (red_post("/api/v1/alarmas/ack", json, &respuesta) != 0)
    {
        fprintf(stderr, "No se pudo confirmar la alarma %s en el servidor.\n",
                alarma.codigo);
        red_liberar_respuesta(&respuesta);
        return 0;
    }

    red_liberar_respuesta(&respuesta);

    snprintf(ts_ack, sizeof(ts_ack), "%lld",
             (long long)time(NULL));

    if (datos_alarma_confirmar(
            datos->db, alarma.codigo, ts_ack) != 0)
    {
        fprintf(stderr,
                "El servidor confirmó %s, pero no se pudo guardar ts_ack.\n",
                alarma.codigo);
        return 0;
    }

    printf("ACK enviado y almacenado para la alarma %s.\n",
           alarma.codigo);

    return 0;
}


/* Variable para detener la recepción de alarmas */
static volatile sig_atomic_t detener_escucha = 0;

/* Manejar la interrupción del comando escuchar */
static void manejar_interrupcion_escucha(int senal)
{
    (void)senal;
    detener_escucha = 1;
}

/* Escuchar y procesar alarmas MQTT */
static int comando_escuchar(void)
{
    sqlite3 *db = NULL;
    ContextoAlarmas contexto;
    const char *matricula = getenv("ITLA_MATRICULA");
    char topico[128];
    struct sigaction accion;
    int reintento = 1;

    if (matricula == NULL || matricula[0] == '\0')
    {
        fprintf(stderr, "Falta configurar ITLA_MATRICULA.\n");
        return 1;
    }

    if (snprintf(topico, sizeof(topico),
                 "itla/planta/%s/alarma", matricula)
        >= (int)sizeof(topico))
    {
        fprintf(stderr, "El tópico MQTT es demasiado largo.\n");
        return 1;
    }

    if (datos_abrir(&db) != 0)
    {
        fprintf(stderr, "No se pudo abrir la base de datos.\n");
        return 1;
    }

    if (datos_inicializar(db) != 0)
    {
        fprintf(stderr, "No se pudo inicializar la base de datos.\n");
        sqlite3_close(db);
        return 1;
    }

    if (red_inicializar() != 0)
    {
        fprintf(stderr, "No se pudo inicializar la conexión HTTP.\n");
        sqlite3_close(db);
        return 1;
    }

    contexto.db = db;
    detener_escucha = 0;

    memset(&accion, 0, sizeof(accion));
    accion.sa_handler = manejar_interrupcion_escucha;
    sigemptyset(&accion.sa_mask);

    if (sigaction(SIGINT, &accion, NULL) != 0 ||
        sigaction(SIGTERM, &accion, NULL) != 0)
    {
        perror("No se pudieron configurar las señales");
        red_finalizar();
        sqlite3_close(db);
        return 1;
    }

    printf("Escuchando alarmas MQTT...\n");
    printf("Tópico: %s\n", topico);
    printf("Presiona Ctrl+C para detener.\n");

    while (!detener_escucha)
    {
        red_mqtt_escuchar(
            topico,
            procesar_mensaje_alarma,
            &contexto,
            &detener_escucha
        );

        if (detener_escucha)
            break;

        fprintf(stderr,
                "Conexión MQTT interrumpida. Reintentando en %d segundos.\n",
                calcular_reintento(reintento));

        {
            int espera = calcular_reintento(reintento);
            struct timespec tiempo = {
                .tv_sec = espera,
                .tv_nsec = 0
            };

            while (!detener_escucha &&
                   nanosleep(&tiempo, &tiempo) != 0 &&
                   errno == EINTR)
            {
                /* Continuar esperando o detenerse por señal */
            }
        }

        if (reintento < 6)
            reintento++;
    }

    printf("\nDeteniendo escucha de alarmas...\n");

    red_finalizar();

    if (sqlite3_close(db) != SQLITE_OK)
    {
        fprintf(stderr, "No se pudo cerrar la base de datos.\n");
        return 1;
    }

    printf("Base de datos cerrada correctamente.\n");
    return 0;
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

    if (strcmp(argv[1], "analizar") == 0)
        return comando_analizar();

    if (strcmp(argv[1], "enviar") == 0)
        return comando_enviar();

    if (strcmp(argv[1], "escuchar") == 0)
        return comando_escuchar();

    fprintf(stderr, "Comando desconocido.\n");
    return 1;
}
