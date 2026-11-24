
/* MÓDULO DE COMUNICACIÓN */

/* Protección contra inclusiones múltiples */
#ifndef RED_H
#define RED_H

/* Biblioteca */
#include <stddef.h>

/* Estructura para almacenar respuestas HTTP */
typedef struct
{
    char *contenido;
    size_t longitud;
    long codigo_http;

} RedRespuesta;

/* Funciones del módulo de comunicación */
int red_inicializar(void);
void red_finalizar(void);
void red_liberar_respuesta(RedRespuesta *respuesta);

/* Funciones para solicitudes HTTP */
int red_get(const char *ruta, RedRespuesta *respuesta);
int red_post(const char *ruta, const char *json, RedRespuesta *respuesta);

#endif