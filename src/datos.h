
/* MÓDULO DE ALMACENAMIENTO */

/* Protección contra inclusiones múltiples */
#ifndef DATOS_H
#define DATOS_H

/* Biblioteca de SQLite */
#include "sqlite3.h"

/* Funciones del módulo de almacenamiento */
int datos_abrir(sqlite3 **db);
void datos_cerrar(sqlite3 *db);
int datos_inicializar(sqlite3 *db);

#endif
