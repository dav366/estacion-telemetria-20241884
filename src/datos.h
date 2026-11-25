
/* MÓDULO DE ALMACENAMIENTO */
#ifndef DATOS_H
#define DATOS_H

#include "sqlite3.h"
#include <stddef.h>

/* Estructura para representar una lectura */
typedef struct
{
    int id;
    char maquina[50];
    char ts[30];

    double temp;
    double corriente;
    double vibracion;

    char estado[20];

    int tiene_temp;
    int tiene_corriente;
    int tiene_vibracion;

} DatosLectura;

/* Estadísticas de una máquina */
typedef struct
{
    char maquina[50];

    int validas;
    int descartadas;

    double temp_min;
    double temp_max;
    double temp_prom;

    int sobre_umbral;

} DatosAnalisis;

/* Lectura pendiente de telemetría */
typedef struct
{
    long long seq;
    long long ts;
    double temp;
    double corriente;
    int enviado;
    int intentos;

} DatosTelemetria;

/* Funciones generales */
int datos_abrir(sqlite3 **db);
void datos_cerrar(sqlite3 *db);
int datos_inicializar(sqlite3 *db);

/* Gestión de transacciones */
int datos_transaccion_iniciar(sqlite3 *db);
int datos_transaccion_confirmar(sqlite3 *db);
int datos_transaccion_cancelar(sqlite3 *db);

/* Configuración */
int datos_config_guardar(sqlite3 *db, const char *clave, const char *valor);
int datos_config_obtener(sqlite3 *db, const char *clave,
                         char *valor, size_t capacidad);

/* Lecturas de planta */
int datos_lectura_guardar(sqlite3 *db, const DatosLectura *lectura);
int datos_lecturas_limpiar(sqlite3 *db);
int datos_lecturas_resumen(sqlite3 *db, int *cantidad, long long *suma_ids);

/* Análisis de lecturas */
int datos_lecturas_analizar(
    sqlite3 *db,
    double umbral,
    DatosAnalisis resultados[],
    int capacidad,
    int *cantidad
);

/* Gestión de telemetría */
int datos_telemetria_guardar(
    sqlite3 *db,
    DatosTelemetria *lectura
);

int datos_telemetria_obtener_pendiente(
    sqlite3 *db,
    DatosTelemetria *lectura
);

int datos_telemetria_marcar_enviada(
    sqlite3 *db,
    long long seq
);

int datos_telemetria_contar_pendientes(
    sqlite3 *db,
    int *cantidad
);

int datos_telemetria_incrementar_intentos(
    sqlite3 *db,
    long long seq
);

#endif
