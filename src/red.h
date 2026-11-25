
/* MÓDULO DE COMUNICACIÓN */
#ifndef RED_H
#define RED_H

#include <stddef.h>

/* Respuesta recibida del servidor */
typedef struct
{
    char *contenido;
    size_t longitud;
    long codigo_http;
    long retry_after_s;

} RedRespuesta;

/* Cliente HTTP reutilizable */
typedef struct RedCliente RedCliente;

/* Funciones generales */
int red_inicializar(void);
void red_finalizar(void);
void red_liberar_respuesta(RedRespuesta *respuesta);

/* Solicitudes HTTP tradicionales */
int red_get(const char *ruta, RedRespuesta *respuesta);

int red_post(
    const char *ruta,
    const char *json,
    RedRespuesta *respuesta
);

/* Gestión del cliente HTTP reutilizable */
RedCliente *red_cliente_crear(void);

void red_cliente_destruir(RedCliente *cliente);

/* Envío HTTP POST con captura de Retry-After */
int red_cliente_post(
    RedCliente *cliente,
    const char *ruta,
    const char *json,
    RedRespuesta *respuesta
);

#endif