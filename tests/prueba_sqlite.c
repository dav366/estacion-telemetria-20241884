
/* PRUEBA DEL MÓDULO DE ALMACENAMIENTO */

/* Bibliotecas necesarias */
#include <stdio.h>
#include <string.h>

#include "datos.h"

int main(void)
{
    /* Declarar el puntero de conexión */
    sqlite3 *db = NULL;

    /* Arreglo para recuperar configuraciones */
    char valor[100];

    /* Estructura para probar el almacenamiento de lecturas */
    DatosLectura lectura = {
        999999,
        "MOTOR_PRUEBA",
        "2026-11-25 12:00:00",
        45.5,
        2.1,
        0.3,
        "OK"
    };

    /* Abrir la base de datos */
    if (datos_abrir(&db) != 0)
    {
        fprintf(stderr, "No se pudo abrir la base de datos.\n");
        return 1;
    }

    /* Inicializar las tablas */
    if (datos_inicializar(db) != 0)
    {
        fprintf(stderr, "No se pudo inicializar la base de datos.\n");
        datos_cerrar(db);
        return 1;
    }

    /* Probar el almacenamiento de una configuración */
    if (datos_config_guardar(db, "prueba_sqlite", "valor inicial") != 0)
    {
        fprintf(stderr, "Error al guardar la configuración.\n");
        datos_cerrar(db);
        return 1;
    }

    printf("Prueba de almacenamiento completada.\n");

    /* Recuperar la configuración almacenada */
    if (datos_config_obtener(db, "prueba_sqlite", valor, sizeof(valor)) != 0)
    {
        fprintf(stderr, "Error al recuperar la configuración.\n");
        datos_cerrar(db);
        return 1;
    }

    /* Comprobar el valor recuperado */
    if (strcmp(valor, "valor inicial") != 0)
    {
        fprintf(stderr, "El valor recuperado no coincide.\n");
        datos_cerrar(db);
        return 1;
    }

    printf("Prueba de recuperación completada: %s\n", valor);

    /* Actualizar una configuración existente */
    if (datos_config_guardar(db, "prueba_sqlite", "valor actualizado") != 0)
    {
        fprintf(stderr, "Error al actualizar la configuración.\n");
        datos_cerrar(db);
        return 1;
    }

    /* Recuperar nuevamente la configuración */
    if (datos_config_obtener(db, "prueba_sqlite", valor, sizeof(valor)) != 0)
    {
        fprintf(stderr, "Error al recuperar la configuración actualizada.\n");
        datos_cerrar(db);
        return 1;
    }

    /* Comprobar que el valor fue actualizado */
    if (strcmp(valor, "valor actualizado") != 0)
    {
        fprintf(stderr, "La actualización de la configuración falló.\n");
        datos_cerrar(db);
        return 1;
    }

    printf("Prueba de actualización completada: %s\n", valor);

    /* Probar el almacenamiento de una lectura */
    if (datos_lectura_guardar(db, &lectura) != 0)
    {
        fprintf(stderr, "Error al guardar la lectura de prueba.\n");
        datos_cerrar(db);
        return 1;
    }

    printf("Prueba de inserción de lectura completada.\n");

    /* Intentar insertar nuevamente la misma lectura */
    if (datos_lectura_guardar(db, &lectura) != 0)
    {
        fprintf(stderr, "Error al comprobar los duplicados.\n");
        datos_cerrar(db);
        return 1;
    }

    /* Comprobar que solamente exista una lectura con ese ID */
    sqlite3_stmt *consulta = NULL;

    int resultado = sqlite3_prepare_v2(
        db,
        "SELECT COUNT(*) FROM lecturas_planta WHERE id = ?;",
        -1,
        &consulta,
        NULL
    );

    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al preparar la consulta de lectura.\n");
        datos_cerrar(db);
        return 1;
    }

    sqlite3_bind_int(consulta, 1, lectura.id);

    resultado = sqlite3_step(consulta);

    if (resultado != SQLITE_ROW || sqlite3_column_int(consulta, 0) != 1)
    {
        fprintf(stderr, "La comprobación de duplicados falló.\n");
        sqlite3_finalize(consulta);
        datos_cerrar(db);
        return 1;
    }

    sqlite3_finalize(consulta);

    printf("Prueba de prevención de duplicados completada.\n");

    /* Eliminar los registros utilizados durante las pruebas */
    resultado = sqlite3_exec(
        db,
        "DELETE FROM config WHERE clave = 'prueba_sqlite';"
        "DELETE FROM lecturas_planta WHERE id = 999999;",
        NULL,
        NULL,
        NULL
    );

    if (resultado != SQLITE_OK)
    {
        fprintf(stderr, "Error al limpiar los datos de prueba.\n");
        datos_cerrar(db);
        return 1;
    }

    printf("Datos de prueba eliminados correctamente.\n");

    /* Confirmar que todas las pruebas terminaron correctamente */
    printf("Pruebas del módulo SQLite completadas.\n");

    /* Cerrar la conexión */
    datos_cerrar(db);

    return 0;
}
