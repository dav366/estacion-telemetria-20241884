
/* MÓDULO DE ALMACENAMIENTO */

/* Inclusión de la interfaz del módulo */
#include "datos.h"

/* Bibliotecas necesarias */
#include <stdio.h>
#include <string.h>

/* Abrir o crear la base de datos local */
int datos_abrir(sqlite3 **db)
{
    if (db == NULL)
        return 1;

    *db = NULL;

    int resultado = sqlite3_open("estacion.db", db);

    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al abrir la base de datos: %s\n",
                *db != NULL ? sqlite3_errmsg(*db) : "Error desconocido");

        if (*db != NULL)
            sqlite3_close(*db);

        *db = NULL;
        return 1;
    }

    sqlite3_busy_timeout(*db, 2000);

    printf("Base de datos abierta correctamente.\n");

    return 0;
}

/* Cerrar la conexión con la base de datos */
void datos_cerrar(sqlite3 *db)
{
    if (db != NULL)
    {
        sqlite3_close(db);
        printf("Base de datos cerrada correctamente.\n");
    }
}

/* Crear las tablas necesarias del sistema */
int datos_inicializar(sqlite3 *db)
{
    if (db == NULL)
        return 1;

    const char *sql_wal =
        "PRAGMA journal_mode = WAL;";

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

    char *mensaje_error = NULL;

    int resultado = sqlite3_exec(
        db,
        sql_wal,
        NULL,
        NULL,
        &mensaje_error
    );

    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al activar WAL: %s\n",
                mensaje_error != NULL
                    ? mensaje_error
                    : sqlite3_errmsg(db));

        sqlite3_free(mensaje_error);
        return 1;
    }

    sqlite3_free(mensaje_error);
    mensaje_error = NULL;

    resultado = sqlite3_exec(
        db,
        sql_tablas,
        NULL,
        NULL,
        &mensaje_error
    );

    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al crear las tablas: %s\n",
                mensaje_error != NULL
                    ? mensaje_error
                    : sqlite3_errmsg(db));

        sqlite3_free(mensaje_error);
        return 1;
    }

    sqlite3_free(mensaje_error);

    printf("Tablas de la base de datos inicializadas.\n");

    return 0;
}

/* Guardar o actualizar un parámetro de configuración */
int datos_config_guardar(sqlite3 *db, const char *clave, const char *valor)
{
    if (db == NULL || clave == NULL || valor == NULL)
        return 1;

    const char *sql =
        "INSERT INTO config (clave, valor) VALUES (?, ?) "
        "ON CONFLICT(clave) DO UPDATE SET valor = excluded.valor;";

    sqlite3_stmt *consulta = NULL;

    int resultado = sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &consulta,
        NULL
    );

    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al preparar la configuración: %s\n",
                sqlite3_errmsg(db));
        return 1;
    }

    resultado = sqlite3_bind_text(
        consulta, 1, clave, -1, SQLITE_TRANSIENT
    );

    if (resultado == SQLITE_OK)
    {
        resultado = sqlite3_bind_text(
            consulta, 2, valor, -1, SQLITE_TRANSIENT
        );
    }

    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al asignar la configuración: %s\n",
                sqlite3_errmsg(db));

        sqlite3_finalize(consulta);
        return 1;
    }

    resultado = sqlite3_step(consulta);

    sqlite3_finalize(consulta);

    if (resultado != SQLITE_DONE)
    {
        fprintf(stderr, "Error al guardar la configuración: %s\n",
                sqlite3_errmsg(db));
        return 1;
    }

    return 0;
}

/* Recuperar un parámetro de configuración */
int datos_config_obtener(
    sqlite3 *db,
    const char *clave,
    char *valor,
    size_t capacidad
)
{
    if (db == NULL || clave == NULL || valor == NULL || capacidad == 0)
        return 1;

    const char *sql =
        "SELECT valor FROM config WHERE clave = ?;";

    sqlite3_stmt *consulta = NULL;

    int resultado = sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &consulta,
        NULL
    );

    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al preparar la consulta: %s\n",
                sqlite3_errmsg(db));
        return 1;
    }

    resultado = sqlite3_bind_text(
        consulta, 1, clave, -1, SQLITE_TRANSIENT
    );

    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al asignar la clave: %s\n",
                sqlite3_errmsg(db));

        sqlite3_finalize(consulta);
        return 1;
    }

    resultado = sqlite3_step(consulta);

    if (resultado == SQLITE_ROW)
    {
        const unsigned char *dato = sqlite3_column_text(consulta, 0);

        if (dato == NULL)
        {
            sqlite3_finalize(consulta);
            return 1;
        }

        size_t longitud = (size_t)sqlite3_column_bytes(consulta, 0);

        if (longitud >= capacidad)
        {
            fprintf(stderr, "Error: espacio insuficiente para recuperar la configuración.\n");

            sqlite3_finalize(consulta);
            return 1;
        }

        memcpy(valor, dato, longitud);
        valor[longitud] = '\0';
    }
    else
    {
        if (resultado != SQLITE_DONE)
        {
            fprintf(stderr, "Error al consultar la configuración: %s\n",
                    sqlite3_errmsg(db));
        }

        sqlite3_finalize(consulta);
        return 1;
    }

    sqlite3_finalize(consulta);

    return 0;
}

/* Guardar una lectura de la planta */
int datos_lectura_guardar(sqlite3 *db, const DatosLectura *lectura)
{
    if (db == NULL || lectura == NULL)
        return 1;

    /* Preparar la consulta para insertar la lectura */
    const char *sql =
        "INSERT OR IGNORE INTO lecturas_planta "
        "(id, maquina, ts, temp, corriente, vibracion, estado) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *consulta = NULL;

    int resultado = sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &consulta,
        NULL
    );

    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al preparar la lectura: %s\n",
                sqlite3_errmsg(db));
        return 1;
    }

    /* Asignar el identificador */
    resultado = sqlite3_bind_int(consulta, 1, lectura->id);

    /* Asignar el nombre de la máquina */
    if (resultado == SQLITE_OK)
        resultado = sqlite3_bind_text(
            consulta, 2, lectura->maquina, -1, SQLITE_TRANSIENT
        );

    /* Asignar la fecha y hora */
    if (resultado == SQLITE_OK)
        resultado = sqlite3_bind_text(
            consulta, 3, lectura->ts, -1, SQLITE_TRANSIENT
        );

    /* Asignar la temperatura */
    if (resultado == SQLITE_OK)
        resultado = sqlite3_bind_double(consulta, 4, lectura->temp);

    /* Asignar la corriente */
    if (resultado == SQLITE_OK)
        resultado = sqlite3_bind_double(consulta, 5, lectura->corriente);

    /* Asignar la vibración */
    if (resultado == SQLITE_OK)
        resultado = sqlite3_bind_double(consulta, 6, lectura->vibracion);

    /* Asignar el estado */
    if (resultado == SQLITE_OK)
        resultado = sqlite3_bind_text(
            consulta, 7, lectura->estado, -1, SQLITE_TRANSIENT
        );

    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al asignar los datos de la lectura: %s\n",
                sqlite3_errmsg(db));

        sqlite3_finalize(consulta);
        return 1;
    }

    /* Ejecutar la inserción */
    resultado = sqlite3_step(consulta);

    sqlite3_finalize(consulta);

    if (resultado != SQLITE_DONE)
    {
        fprintf(stderr, "Error al guardar la lectura: %s\n",
                sqlite3_errmsg(db));
        return 1;
    }

    return 0;
}
