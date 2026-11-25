
/* MÓDULO DE ALMACENAMIENTO */

#include "datos.h"

#include <stdio.h>
#include <string.h>

/* Abrir o crear la base de datos */
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

/* Cerrar la conexión */
void datos_cerrar(sqlite3 *db)
{
    if (db != NULL)
    {
        sqlite3_close(db);
        printf("Base de datos cerrada correctamente.\n");
    }
}

/* Inicializar las tablas */
int datos_inicializar(sqlite3 *db)
{
    if (db == NULL)
        return 1;

    const char *sql =
        "PRAGMA journal_mode = WAL;"

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
        db, sql, NULL, NULL, &mensaje_error
    );

    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al inicializar SQLite: %s\n",
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

/* Iniciar una transacción */
int datos_transaccion_iniciar(sqlite3 *db)
{
    if (db == NULL)
        return 1;

    return sqlite3_exec(
        db, "BEGIN IMMEDIATE;", NULL, NULL, NULL
    ) == SQLITE_OK ? 0 : 1;
}

/* Confirmar una transacción */
int datos_transaccion_confirmar(sqlite3 *db)
{
    if (db == NULL)
        return 1;

    return sqlite3_exec(
        db, "COMMIT;", NULL, NULL, NULL
    ) == SQLITE_OK ? 0 : 1;
}

/* Cancelar una transacción */
int datos_transaccion_cancelar(sqlite3 *db)
{
    if (db == NULL)
        return 1;

    return sqlite3_exec(
        db, "ROLLBACK;", NULL, NULL, NULL
    ) == SQLITE_OK ? 0 : 1;
}

/* Guardar o actualizar un parámetro */
int datos_config_guardar(
    sqlite3 *db,
    const char *clave,
    const char *valor
)
{
    if (db == NULL || clave == NULL || valor == NULL)
        return 1;

    const char *sql =
        "INSERT INTO config (clave, valor) VALUES (?, ?) "
        "ON CONFLICT(clave) DO UPDATE SET valor = excluded.valor;";

    sqlite3_stmt *consulta = NULL;

    int resultado = sqlite3_prepare_v2(
        db, sql, -1, &consulta, NULL
    );

    if (resultado != SQLITE_OK)
        goto error;

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
        sqlite3_finalize(consulta);
        goto error;
    }

    resultado = sqlite3_step(consulta);

    sqlite3_finalize(consulta);

    if (resultado != SQLITE_DONE)
        goto error;

    return 0;

error:
    fprintf(stderr, "Error al guardar configuración: %s\n",
            sqlite3_errmsg(db));
    return 1;
}

/* Recuperar un parámetro */
int datos_config_obtener(
    sqlite3 *db,
    const char *clave,
    char *valor,
    size_t capacidad
)
{
    if (db == NULL || clave == NULL ||
        valor == NULL || capacidad == 0)
        return 1;

    const char *sql =
        "SELECT valor FROM config WHERE clave = ?;";

    sqlite3_stmt *consulta = NULL;

    int resultado = sqlite3_prepare_v2(
        db, sql, -1, &consulta, NULL
    );

    if (resultado != SQLITE_OK)
        goto error;

    resultado = sqlite3_bind_text(
        consulta, 1, clave, -1, SQLITE_TRANSIENT
    );

    if (resultado != SQLITE_OK)
    {
        sqlite3_finalize(consulta);
        goto error;
    }

    resultado = sqlite3_step(consulta);

    if (resultado != SQLITE_ROW)
    {
        sqlite3_finalize(consulta);
        goto error;
    }

    const unsigned char *dato = sqlite3_column_text(consulta, 0);

    if (dato == NULL)
    {
        sqlite3_finalize(consulta);
        return 1;
    }

    size_t longitud = (size_t)sqlite3_column_bytes(consulta, 0);

    if (longitud >= capacidad)
    {
        sqlite3_finalize(consulta);
        return 1;
    }

    memcpy(valor, dato, longitud);
    valor[longitud] = '\0';

    sqlite3_finalize(consulta);

    return 0;

error:
    fprintf(stderr, "Error al recuperar configuración: %s\n",
            sqlite3_errmsg(db));
    return 1;
}

/* Guardar una lectura */
int datos_lectura_guardar(
    sqlite3 *db,
    const DatosLectura *lectura
)
{
    if (db == NULL || lectura == NULL)
        return 1;

    const char *sql =
        "INSERT OR IGNORE INTO lecturas_planta "
        "(id, maquina, ts, temp, corriente, vibracion, estado) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *consulta = NULL;

    int resultado = sqlite3_prepare_v2(
        db, sql, -1, &consulta, NULL
    );

    if (resultado != SQLITE_OK)
        goto error;

    resultado = sqlite3_bind_int(consulta, 1, lectura->id);

    if (resultado == SQLITE_OK)
        resultado = sqlite3_bind_text(
            consulta, 2, lectura->maquina, -1, SQLITE_TRANSIENT
        );

    if (resultado == SQLITE_OK)
        resultado = sqlite3_bind_text(
            consulta, 3, lectura->ts, -1, SQLITE_TRANSIENT
        );

    if (resultado == SQLITE_OK)
    {
        resultado = lectura->tiene_temp
            ? sqlite3_bind_double(consulta, 4, lectura->temp)
            : sqlite3_bind_null(consulta, 4);
    }

    if (resultado == SQLITE_OK)
    {
        resultado = lectura->tiene_corriente
            ? sqlite3_bind_double(consulta, 5, lectura->corriente)
            : sqlite3_bind_null(consulta, 5);
    }

    if (resultado == SQLITE_OK)
    {
        resultado = lectura->tiene_vibracion
            ? sqlite3_bind_double(consulta, 6, lectura->vibracion)
            : sqlite3_bind_null(consulta, 6);
    }

    if (resultado == SQLITE_OK)
        resultado = sqlite3_bind_text(
            consulta, 7, lectura->estado, -1, SQLITE_TRANSIENT
        );

    if (resultado != SQLITE_OK)
    {
        sqlite3_finalize(consulta);
        goto error;
    }

    resultado = sqlite3_step(consulta);

    sqlite3_finalize(consulta);

    if (resultado != SQLITE_DONE)
        goto error;

    return 0;

error:
    fprintf(stderr, "Error al guardar lectura: %s\n",
            sqlite3_errmsg(db));
    return 1;
}

/* Eliminar las lecturas anteriores */
int datos_lecturas_limpiar(sqlite3 *db)
{
    if (db == NULL)
        return 1;

    return sqlite3_exec(
        db, "DELETE FROM lecturas_planta;",
        NULL, NULL, NULL
    ) == SQLITE_OK ? 0 : 1;
}

/* Obtener cantidad de lecturas únicas y suma de IDs */
int datos_lecturas_resumen(
    sqlite3 *db,
    int *cantidad,
    long long *suma_ids
)
{
    if (db == NULL || cantidad == NULL || suma_ids == NULL)
        return 1;

    const char *sql =
        "SELECT COUNT(*), COALESCE(SUM(id), 0) "
        "FROM lecturas_planta;";

    sqlite3_stmt *consulta = NULL;

    int resultado = sqlite3_prepare_v2(
        db, sql, -1, &consulta, NULL
    );

    if (resultado != SQLITE_OK)
        goto error;

    resultado = sqlite3_step(consulta);

    if (resultado != SQLITE_ROW)
    {
        sqlite3_finalize(consulta);
        goto error;
    }

    *cantidad = sqlite3_column_int(consulta, 0);
    *suma_ids = sqlite3_column_int64(consulta, 1);

    sqlite3_finalize(consulta);

    return 0;

error:
    fprintf(stderr, "Error al calcular resumen de lecturas: %s\n",
            sqlite3_errmsg(db));
    return 1;
}
