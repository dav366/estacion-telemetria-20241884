
/* MÓDULO DE ALMACENAMIENTO */

/* Inclusión de la interfaz del módulo */
#include "datos.h"

/* Bibliotecas necesarias */
#include <stdio.h>

/* Abrir o crear la base de datos local */
int datos_abrir(sqlite3 **db)
{
    /* Comprobar que se recibió un puntero válido */
    if (db == NULL)
        return 1;

    /* Inicializar el puntero */
    *db = NULL;

    /* Abrir o crear el archivo de base de datos */
    int resultado = sqlite3_open("estacion.db", db);

    /* Comprobar si SQLite pudo abrir la base de datos */
    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al abrir la base de datos: %s\n",
                sqlite3_errmsg(*db));

        sqlite3_close(*db);
        *db = NULL;

        return 1;
    }

    /* Establecer tiempo máximo de espera por bloqueos */
    sqlite3_busy_timeout(*db, 2000);

    printf("Base de datos abierta correctamente.\n");

    return 0;
}

/* Cerrar la conexión con la base de datos */
void datos_cerrar(sqlite3 *db)
{
    /* Comprobar que exista una conexión */
    if (db != NULL)
    {
        sqlite3_close(db);
        printf("Base de datos cerrada correctamente.\n");
    }
}

/* Crear las tablas necesarias del sistema */
int datos_inicializar(sqlite3 *db)
{
    /* Comprobar que exista una conexión válida */
    if (db == NULL)
        return 1;

    /* Activar el modo de registro WAL */
    const char *sql_wal =
        "PRAGMA journal_mode = WAL;";

    /* Definir las tablas de la base de datos */
    const char *sql_tablas =
        "CREATE TABLE IF NOT EXISTS config ("
        "clave TEXT PRIMARY KEY,"
        "valor TEXT NOT NULL"
        ");"

        "CREATE TABLE IF NOT EXISTS lecturas_planta ("
        "id INTEGER PRIMARY KEY,"
        "maquina TEXT NOT NULL,"
        "ts TEXT NOT NULL,"
        "temp REAL,"
        "corriente REAL,"
        "vibracion REAL,"
        "estado TEXT"
        ");"

        "CREATE TABLE IF NOT EXISTS telemetria ("
        "seq INTEGER PRIMARY KEY,"
        "ts TEXT NOT NULL,"
        "temp REAL,"
        "corriente REAL,"
        "enviado INTEGER DEFAULT 0,"
        "intentos INTEGER DEFAULT 0"
        ");"

        "CREATE TABLE IF NOT EXISTS alarmas ("
        "codigo TEXT PRIMARY KEY,"
        "maquina TEXT NOT NULL,"
        "ts_servidor TEXT,"
        "ts_recibida TEXT,"
        "ts_ack TEXT"
        ");";

    /* Ejecutar la configuración WAL */
    char *mensaje_error = NULL;

    int resultado = sqlite3_exec(
        db,
        sql_wal,
        NULL,
        NULL,
        &mensaje_error
    );

    /* Comprobar el resultado de la configuración */
    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al activar WAL: %s\n",
                mensaje_error != NULL
                    ? mensaje_error
                    : sqlite3_errmsg(db));

        sqlite3_free(mensaje_error);
        return 1;
    }

    /* Liberar posibles mensajes de error */
    sqlite3_free(mensaje_error);
    mensaje_error = NULL;

    /* Crear las tablas */
    resultado = sqlite3_exec(
        db,
        sql_tablas,
        NULL,
        NULL,
        &mensaje_error
    );

    /* Comprobar si las tablas fueron creadas */
    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al crear las tablas: %s\n",
                mensaje_error != NULL
                    ? mensaje_error
                    : sqlite3_errmsg(db));

        sqlite3_free(mensaje_error);
        return 1;
    }

    /* Liberar recursos */
    sqlite3_free(mensaje_error);

    printf("Tablas de la base de datos inicializadas.\n");

    return 0;
}
