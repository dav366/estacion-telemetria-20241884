
/* MÓDULO DE ALMACENAMIENTO */

/* Protección contra inclusiones múltiples */
#ifndef DATOS_H
#define DATOS_H

/* Biblioteca de SQLite */
#include "sqlite3.h"

/* Biblioteca para manejar tamaños */
#include <stddef.h>

/* Estructura para representar una lectura de la planta */
typedef struct
{
    int id;
    char maquina[50];
    char ts[30];
    double temp;
    double corriente;
    double vibracion;
    char estado[20];
} DatosLectura;

/* Funciones generales del módulo */
int datos_abrir(sqlite3 **db);
void datos_cerrar(sqlite3 *db);
int datos_inicializar(sqlite3 *db);

/* Funciones para gestionar la configuración */
int datos_config_guardar(sqlite3 *db, const char *clave, const char *valor);
int datos_config_obtener(sqlite3 *db, const char *clave, char *valor, size_t capacidad);

/* Funciones para gestionar las lecturas de la planta */
int datos_lectura_guardar(sqlite3 *db, const DatosLectura *lectura);

#endif
