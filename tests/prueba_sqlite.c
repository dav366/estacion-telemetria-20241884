
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

    /* Eliminar el registro utilizado durante la prueba */
    int resultado = sqlite3_exec(
        db,
        "DELETE FROM config WHERE clave = 'prueba_sqlite';",
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
