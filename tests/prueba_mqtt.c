
#include <stdio.h>
#include <mosquitto.h>

int main(void)
{
    int resultado;

    resultado = mosquitto_lib_init();

    if (resultado != MOSQ_ERR_SUCCESS)
    {
        printf("Error al inicializar MQTT: %s\n",
               mosquitto_strerror(resultado));

        return 1;
    }

    printf("Biblioteca MQTT inicializada correctamente.\n");

    mosquitto_lib_cleanup();

    return 0;
}
