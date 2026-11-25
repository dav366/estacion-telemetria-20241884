
/* MÓDULO DE SENSORES */
#ifndef SENSOR_H
#define SENSOR_H

/* Inicializar el generador de lecturas */
void sensor_inicializar(void);

/* Generar una lectura simulada */
void sensor_generar(double *temperatura, double *corriente);

#endif
