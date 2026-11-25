
/* MÓDULO DE SENSORES */

#include "sensor.h"

#include <stdlib.h>
#include <time.h>
#include <math.h>

/* Inicializar el generador aleatorio */
void sensor_inicializar(void)
{
    srand((unsigned int)time(NULL));
}

/* Generar un número aleatorio entre -1 y 1 */
static double sensor_ruido(void)
{
    return ((double)rand() / RAND_MAX) * 2.0 - 1.0;
}

/* Generar lecturas simuladas */
void sensor_generar(double *temperatura, double *corriente)
{
    if (temperatura == NULL || corriente == NULL)
        return;

    /*
       Variación lenta de temperatura:
       valor central de 35 °C,
       oscilación de 8 °C y ruido de hasta 2 °C.
    */
    static double fase = 0.0;

    *temperatura = 35.0
                 + 8.0 * sin(fase)
                 + 2.0 * sensor_ruido();

    fase += 0.15;

    if (fase >= 6.28318530718)
        fase -= 6.28318530718;

    /*
       Corriente simulada entre 2 y 5 amperios.
    */
    *corriente = 3.5 + 1.5 * sensor_ruido();
}
