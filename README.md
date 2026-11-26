# Estación de Telemetría ITLA

Sistema de telemetría desarrollado en C para simular, almacenar, analizar y transmitir información de una planta industrial. Utiliza SQLite para la persistencia local, libcurl para la comunicación HTTP y MQTT, y un sensor simulado para generar datos.

---

## Autor

- **Nombre:** Omar David Guzman Guerrero
- **Matrícula:** 2024-1884

## Descripción

La aplicación permite gestionar lecturas de sensores industriales y comunicarse con un servidor remoto.

Sus funcionalidades principales son consultar la configuración del servidor, descargar lecturas, analizar datos almacenados, transmitir telemetría y recibir alarmas mediante MQTT.

Los datos se conservan en una base de datos SQLite local.

## Objetivos

- Desarrollar una aplicación modular en lenguaje C.
- Implementar comunicación HTTP mediante libcurl.
- Utilizar SQLite para almacenar información.
- Simular lecturas de sensores industriales.
- Transmitir datos de telemetría.
- Recibir y confirmar alarmas mediante MQTT y HTTP.
- Mantener persistencia local ante problemas de conexión.

## Funcionalidades

| Comando | Descripción |
|---|---|
| `config` | Consulta y almacena la configuración del servidor. |
| `descargar` | Descarga lecturas de la planta. |
| `analizar` | Analiza las lecturas almacenadas. |
| `enviar` | Genera y transmite datos de telemetría. |
| `escuchar` | Recibe alarmas mediante MQTT y gestiona sus confirmaciones. |

## Tecnologías

- C
- GCC
- SQLite
- libcurl
- MQTT
- Make
- Linux / WSL

## Estructura del proyecto

```text
estacion-telemetria/
├── src/
│   ├── main.c
│   ├── datos.c
│   ├── datos.h
│   ├── red.c
│   ├── red.h
│   ├── sensor.c
│   └── sensor.h
├── lib/
│   └── sqlite/
│       ├── sqlite3.c
│       └── sqlite3.h
├── tests/
│   ├── prueba_curl.c
│   ├── prueba_http.c
│   ├── prueba_mqtt.c
│   ├── prueba_red.c
│   ├── prueba_sqlite.c
│   └── servidor_prueba.py
├── .gitignore
├── Makefile
└── README.md
```

## Requisitos

En Ubuntu o WSL se necesitan GCC, Make, libcurl, SQLite y Python 3 para las pruebas auxiliares.

Instalación de dependencias:

```bash
sudo apt update
sudo apt install build-essential gcc make libcurl4-openssl-dev libsqlite3-dev sqlite3 python3
```

## Configuración del entorno

Antes de ejecutar los comandos que necesitan conectarse al servidor, configura las variables de entorno:

```bash
export ITLA_HOST="DIRECCION_DEL_SERVIDOR"
export ITLA_TOKEN="TU_TOKEN"
export ITLA_MATRICULA="20241884"
```

Sustituye los valores de ejemplo por los datos proporcionados para el proyecto.

**Importante:**
- La matrícula se escribe sin guion en la variable de entorno.
- No compartas tu token.
- No incluyas credenciales privadas en el repositorio.

Para comprobar que las variables están configuradas sin mostrar el token:

```bash
test -n "$ITLA_HOST" && echo "ITLA_HOST configurado" || echo "Falta ITLA_HOST"
test -n "$ITLA_TOKEN" && echo "ITLA_TOKEN configurado" || echo "Falta ITLA_TOKEN"
test -n "$ITLA_MATRICULA" && echo "ITLA_MATRICULA configurada" || echo "Falta ITLA_MATRICULA"
```

Las variables deben configurarse nuevamente al abrir una terminal nueva, salvo que se hayan establecido de forma persistente en el entorno.

## Compilación

El proyecto incluye un Makefile para automatizar la compilación.

Desde la carpeta principal:

```bash
make
```

Para limpiar los archivos generados y compilar nuevamente:

```bash
make clean
make
```

El ejecutable se genera en:

```text
build/estacion
```

También se puede compilar manualmente:

```bash
mkdir -p build

gcc -Wall -Wextra -Wpedantic -std=c11 \
src/main.c src/datos.c src/red.c src/sensor.c lib/sqlite/sqlite3.c \
-o build/estacion -lcurl -lsqlite3 -lm
```

## Ejecución

La estructura general del comando es:

```bash
./build/estacion COMANDO
```

### 1. Consultar configuración

```bash
./build/estacion config
```

Solicita la configuración al servidor y almacena los valores recibidos localmente.

### 2. Descargar lecturas

```bash
./build/estacion descargar
```

Descarga las lecturas disponibles y las almacena en SQLite.

### 3. Analizar lecturas

```bash
./build/estacion analizar
```

Procesa las lecturas almacenadas y realiza el análisis correspondiente.

### 4. Enviar telemetría

```bash
./build/estacion enviar
```

Inicia el proceso de generación, almacenamiento y transmisión de datos de telemetría.

Las lecturas se almacenan localmente antes de intentar enviarse.

Para detener el proceso:

```text
Ctrl + C
```

### 5. Escuchar alarmas

```bash
./build/estacion escuchar
```

Inicia la conexión MQTT para recibir alarmas del servidor.

Las alarmas recibidas se almacenan localmente y se gestionan sus confirmaciones mediante HTTP.

Para detener el proceso:

```text
Ctrl + C
```

## Pruebas y verificación

Se recomienda ejecutar las siguientes comprobaciones después de compilar.

### Prueba 1: compilación

```bash
make clean
make
```

**Resultado esperado:** compilación exitosa y generación del ejecutable `build/estacion`.

### Prueba 2: configuración

```bash
./build/estacion config
```

Verificar que el servidor responda y que la configuración se almacene correctamente.

### Prueba 3: descarga

```bash
./build/estacion descargar
```

Verificar que las lecturas recibidas se almacenen en la base de datos.

### Prueba 4: análisis

```bash
./build/estacion analizar
```

Verificar que se procesen las lecturas disponibles y se genere el resultado del análisis.

### Prueba 5: telemetría

En una terminal:

```bash
./build/estacion enviar
```

Verificar que se generen lecturas, se almacenen localmente y se realicen los intentos de transmisión.

### Prueba 6: alarmas MQTT

En otra terminal:

```bash
./build/estacion escuchar
```

Verificar que se establezca la conexión MQTT, se reciban alarmas y se gestionen las confirmaciones HTTP.

Para esta prueba deben mantenerse las condiciones de conexión requeridas por el servidor.

## Base de datos SQLite

La aplicación utiliza una base de datos local llamada `estacion.db`.

Tablas principales:

| Tabla | Contenido |
|---|---|
| `config` | Configuración recibida del servidor. |
| `lecturas_planta` | Lecturas descargadas de la planta. |
| `telemetria` | Datos generados y estado de transmisión. |
| `alarmas` | Alarmas recibidas y sus confirmaciones. |

Para inspeccionar la base de datos:

```bash
sqlite3 estacion.db
```

Dentro de SQLite:

```sql
.tables
SELECT * FROM lecturas_planta;
SELECT * FROM telemetria;
SELECT * FROM alarmas;
.quit
```

## Comunicación HTTP

La aplicación utiliza los siguientes endpoints:

| Método | Endpoint | Función |
|---|---|---|
| GET | `/api/v1/config` | Consultar configuración. |
| GET | `/api/v1/lecturas` | Descargar lecturas. |
| POST | `/api/v1/respuestas/descarga` | Reportar la descarga. |
| POST | `/api/v1/respuestas/analisis` | Reportar el análisis. |
| POST | `/api/v1/telemetria` | Transmitir telemetría. |
| POST | `/api/v1/alarmas/ack` | Confirmar alarmas. |

## Comunicación MQTT

El tópico de alarmas se construye utilizando la matrícula:

```text
itla/planta/<MATRICULA>/alarma
```

La aplicación también implementa el envío de latidos para mantener activa la comunicación requerida por el servidor.

## Organización del código

- **main.c:** punto de entrada y gestión de comandos.
- **datos.c / datos.h:** almacenamiento y consultas SQLite.
- **red.c / red.h:** comunicación HTTP y MQTT mediante libcurl.
- **sensor.c / sensor.h:** inicialización y generación de lecturas simuladas.
- **lib/sqlite/:** código fuente y encabezado de SQLite.
- **tests/:** programas auxiliares para comprobar componentes y comunicación.

## Persistencia y manejo de errores

El sistema contempla fallos de conexión, respuestas HTTP no exitosas, problemas de transmisión y recepción de alarmas.

La persistencia local permite conservar los registros para su posterior procesamiento o transmisión.

## Seguridad

Las credenciales se proporcionan mediante variables de entorno. No se deben incluir tokens, contraseñas ni credenciales privadas en el código fuente o en los commits.

## Historial de cambios

Para consultar los commits y sus fechas:

```bash
git log --pretty=format:'%h | %cd | %s' --date=format:'%d/%m/%Y %H:%M'
```
