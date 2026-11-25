
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

/* Analizar lecturas y calcular estadísticas por máquina */
int datos_lecturas_analizar(
    sqlite3 *db,
    double umbral,
    DatosAnalisis resultados[],
    int capacidad,
    int *cantidad
)
{
    if (db == NULL || resultados == NULL ||
        capacidad <= 0 || cantidad == NULL)
        return 1;

    const char *sql =
        "SELECT "
        "maquina, "

        "SUM(CASE WHEN "
        "estado = 'OK' "
        "AND temp IS NOT NULL "
        "AND temp != -999.0 "
        "AND corriente IS NOT NULL "
        "AND vibracion IS NOT NULL "
        "THEN 1 ELSE 0 END), "

        "SUM(CASE WHEN "
        "estado IS NULL "
        "OR estado != 'OK' "
        "OR temp IS NULL "
        "OR temp = -999.0 "
        "OR corriente IS NULL "
        "OR vibracion IS NULL "
        "THEN 1 ELSE 0 END), "

        "MIN(CASE WHEN "
        "estado = 'OK' "
        "AND temp IS NOT NULL "
        "AND temp != -999.0 "
        "AND corriente IS NOT NULL "
        "AND vibracion IS NOT NULL "
        "THEN temp END), "

        "MAX(CASE WHEN "
        "estado = 'OK' "
        "AND temp IS NOT NULL "
        "AND temp != -999.0 "
        "AND corriente IS NOT NULL "
        "AND vibracion IS NOT NULL "
        "THEN temp END), "

        "AVG(CASE WHEN "
        "estado = 'OK' "
        "AND temp IS NOT NULL "
        "AND temp != -999.0 "
        "AND corriente IS NOT NULL "
        "AND vibracion IS NOT NULL "
        "THEN temp END), "

        "SUM(CASE WHEN "
        "estado = 'OK' "
        "AND temp IS NOT NULL "
        "AND temp != -999.0 "
        "AND corriente IS NOT NULL "
        "AND vibracion IS NOT NULL "
        "AND temp > ? "
        "THEN 1 ELSE 0 END) "

        "FROM lecturas_planta "
        "GROUP BY maquina "
        "ORDER BY maquina;";

    sqlite3_stmt *consulta = NULL;

    int resultado = sqlite3_prepare_v2(
        db, sql, -1, &consulta, NULL
    );

    if (resultado != SQLITE_OK)
        goto error;

    resultado = sqlite3_bind_double(consulta, 1, umbral);

    if (resultado != SQLITE_OK)
    {
        sqlite3_finalize(consulta);
        goto error;
    }

    *cantidad = 0;

    while ((resultado = sqlite3_step(consulta)) == SQLITE_ROW)
    {
        if (*cantidad >= capacidad)
        {
            sqlite3_finalize(consulta);
            return 1;
        }

        DatosAnalisis *dato = &resultados[*cantidad];

        memset(dato, 0, sizeof(*dato));

        const unsigned char *maquina =
            sqlite3_column_text(consulta, 0);

        if (maquina == NULL)
        {
            sqlite3_finalize(consulta);
            return 1;
        }

        size_t longitud = strlen((const char *)maquina);

        if (longitud >= sizeof(dato->maquina))
        {
            sqlite3_finalize(consulta);
            return 1;
        }

        memcpy(dato->maquina, maquina, longitud + 1);

        dato->validas = sqlite3_column_int(consulta, 1);
        dato->descartadas = sqlite3_column_int(consulta, 2);

        dato->temp_min = sqlite3_column_double(consulta, 3);
        dato->temp_max = sqlite3_column_double(consulta, 4);
        dato->temp_prom = sqlite3_column_double(consulta, 5);

        dato->sobre_umbral = sqlite3_column_int(consulta, 6);

        (*cantidad)++;
    }

    sqlite3_finalize(consulta);

    if (resultado != SQLITE_DONE)
        goto error;

    return 0;

error:
    fprintf(stderr, "Error al analizar lecturas: %s\n",
            sqlite3_errmsg(db));

    return 1;
}

/* Guardar una lectura y generar su secuencia */
int datos_telemetria_guardar(
    sqlite3 *db,
    DatosTelemetria *lectura
)
{
    if (db == NULL || lectura == NULL)
        return 1;

    if (datos_transaccion_iniciar(db) != 0)
        return 1;

    const char *sql =
        "INSERT INTO telemetria "
        "(ts, temp, corriente, enviado, intentos) "
        "VALUES (?, ?, ?, 0, 0);";

    sqlite3_stmt *consulta = NULL;

    int resultado = sqlite3_prepare_v2(
        db, sql, -1, &consulta, NULL
    );

    if (resultado != SQLITE_OK)
        goto error;

    sqlite3_bind_int64(consulta, 1, lectura->ts);
    sqlite3_bind_double(consulta, 2, lectura->temp);
    sqlite3_bind_double(consulta, 3, lectura->corriente);

    resultado = sqlite3_step(consulta);

    sqlite3_finalize(consulta);
    consulta = NULL;

    if (resultado != SQLITE_DONE)
        goto error;

    lectura->seq = sqlite3_last_insert_rowid(db);
    lectura->enviado = 0;
    lectura->intentos = 0;

    if (datos_transaccion_confirmar(db) != 0)
        goto error;

    return 0;

error:

    if (consulta != NULL)
        sqlite3_finalize(consulta);

    datos_transaccion_cancelar(db);

    fprintf(stderr, "Error al guardar telemetría: %s\n",
            sqlite3_errmsg(db));

    return 1;
}

/* Obtener la lectura pendiente más antigua */
int datos_telemetria_obtener_pendiente(
    sqlite3 *db,
    DatosTelemetria *lectura
)
{
    if (db == NULL || lectura == NULL)
        return -1;

    const char *sql =
        "SELECT seq, ts, temp, corriente, enviado, intentos "
        "FROM telemetria "
        "WHERE enviado = 0 "
        "ORDER BY seq ASC "
        "LIMIT 1;";

    sqlite3_stmt *consulta = NULL;

    int resultado = sqlite3_prepare_v2(
        db, sql, -1, &consulta, NULL
    );

    if (resultado != SQLITE_OK)
        goto error;

    resultado = sqlite3_step(consulta);

    if (resultado == SQLITE_DONE)
    {
        sqlite3_finalize(consulta);
        return 1;
    }

    if (resultado != SQLITE_ROW)
        goto error;

    lectura->seq = sqlite3_column_int64(consulta, 0);
    lectura->ts = sqlite3_column_int64(consulta, 1);
    lectura->temp = sqlite3_column_double(consulta, 2);
    lectura->corriente = sqlite3_column_double(consulta, 3);
    lectura->enviado = sqlite3_column_int(consulta, 4);
    lectura->intentos = sqlite3_column_int(consulta, 5);

    sqlite3_finalize(consulta);

    return 0;

error:

    if (consulta != NULL)
        sqlite3_finalize(consulta);

    fprintf(stderr, "Error al recuperar telemetría pendiente: %s\n",
            sqlite3_errmsg(db));

    return -1;
}

/* Marcar una lectura como enviada */
int datos_telemetria_marcar_enviada(
    sqlite3 *db,
    long long seq
)
{
    if (db == NULL || seq <= 0)
        return 1;

    const char *sql =
        "UPDATE telemetria "
        "SET enviado = 1 "
        "WHERE seq = ?;";

    sqlite3_stmt *consulta = NULL;

    int resultado = sqlite3_prepare_v2(
        db, sql, -1, &consulta, NULL
    );

    if (resultado != SQLITE_OK)
        goto error;

    sqlite3_bind_int64(consulta, 1, seq);

    resultado = sqlite3_step(consulta);

    sqlite3_finalize(consulta);

    if (resultado != SQLITE_DONE)
        goto error;

    return 0;

error:

    if (consulta != NULL)
        sqlite3_finalize(consulta);

    fprintf(stderr, "Error al marcar telemetría enviada: %s\n",
            sqlite3_errmsg(db));

    return 1;
}

/* Contar las lecturas pendientes */
int datos_telemetria_contar_pendientes(
    sqlite3 *db,
    int *cantidad
)
{
    if (db == NULL || cantidad == NULL)
        return 1;

    const char *sql =
        "SELECT COUNT(*) FROM telemetria WHERE enviado = 0;";

    sqlite3_stmt *consulta = NULL;

    int resultado = sqlite3_prepare_v2(
        db, sql, -1, &consulta, NULL
    );

    if (resultado != SQLITE_OK)
        goto error;

    resultado = sqlite3_step(consulta);

    if (resultado != SQLITE_ROW)
        goto error;

    *cantidad = sqlite3_column_int(consulta, 0);

    sqlite3_finalize(consulta);

    return 0;

error:

    if (consulta != NULL)
        sqlite3_finalize(consulta);

    fprintf(stderr, "Error al contar telemetría pendiente: %s\n",
            sqlite3_errmsg(db));

    return 1;
}

/* Incrementar los intentos de envío */
int datos_telemetria_incrementar_intentos(
    sqlite3 *db,
    long long seq
)
{
    if (db == NULL || seq <= 0)
        return 1;

    const char *sql =
        "UPDATE telemetria "
        "SET intentos = intentos + 1 "
        "WHERE seq = ?;";

    sqlite3_stmt *consulta = NULL;

    int resultado = sqlite3_prepare_v2(
        db, sql, -1, &consulta, NULL
    );

    if (resultado != SQLITE_OK)
        goto error;

    sqlite3_bind_int64(consulta, 1, seq);

    resultado = sqlite3_step(consulta);

    sqlite3_finalize(consulta);

    if (resultado != SQLITE_DONE)
        goto error;

    return 0;

error:

    if (consulta != NULL)
        sqlite3_finalize(consulta);

    fprintf(stderr, "Error al incrementar intentos: %s\n",
            sqlite3_errmsg(db));

    return 1;
}