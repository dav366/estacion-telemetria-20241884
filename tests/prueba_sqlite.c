
/* PRUEBA DEL MÓDULO DE ALMACENAMIENTO */

#include <stdio.h>

#include "datos.h"

int main(void)
{
    /* Declarar el puntero de conexión */
    sqlite3 *db = NULL;

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

    /* Confirmar que la prueba terminó correctamente */
    printf("Prueba del módulo SQLite completada.\n");

    /* Cerrar la conexión */
    datos_cerrar(db);

    return 0;
}
