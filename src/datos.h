
/* MÓDULO DE ALMACENAMIENTO */

/* Protección contra inclusiones múltiples */
#ifndef DATOS_H
#define DATOS_H

/* Biblioteca de SQLite */
#include "sqlite3.h"

/* Biblioteca para manejar tamaños */
#include <stddef.h>

/* Funciones del módulo de almacenamiento */
int datos_abrir(sqlite3 **db);
void datos_cerrar(sqlite3 *db);
int datos_inicializar(sqlite3 *db);

/* Funciones para gestionar la configuración */
int datos_config_guardar(sqlite3 *db, const char *clave, const char *valor);
int datos_config_obtener(sqlite3 *db, const char *clave, char *valor, size_t capacidad);

#endif
