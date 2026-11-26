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

Lista de todos los commits y sus fechas:

```bash
a52a8f5 | 25/11/2026 23:24 | Documentando la estación de telemetría en README
6583fce | 25/11/2026 22:51 | Automatizando la compilación del proyecto mediante Makefile
abf78c0 | 25/11/2026 22:50 | Depurando de respuestas HTTP en confirmación de alarmas
919af38 | 25/11/2026 21:22 | Corrigiendo la paginación para completar la evaluación de descarga
f383319 | 25/11/2026 20:10 | Agregando la confirmación HTTP de alarmas
b0287f1 | 25/11/2026 18:57 | Integrando la escucha y reconexión de alarmas MQTT
ff316f1 | 25/11/2026 18:34 | Agregando la suscripción MQTT de alarmas
7bcc767 | 25/11/2026 18:13 | Agregando la gestión de alarmas en SQLite
6895af6 | 25/11/2026 17:58 | Agregando la publicación de latidos MQTT
113c649 | 25/11/2026 16:30 | Implementando el envío resiliente de telemetría
f037a11 | 25/11/2026 15:00 | Implementando los módulos de almacenamiento, comunicación y sensores
a53690d | 25/11/2026 14:24 | Desarrollando el módulo integral de análisis y validación de lecturas de telemetría
71cafc5 | 25/11/2026 12:16 | Corrigiendo paginación y verificando descarga de lecturas
742bce2 | 25/11/2026 03:46 | Implementando la descarga de lecturas
a75a38d | 25/11/2026 03:45 | Renovando el servidor de pruebas
08f713b | 25/11/2026 03:09 | Ampliando el almacenamiento de lecturas
c431b70 | 25/11/2026 02:50 | Integrando descarga de configuración en el programa principal
b413181 | 25/11/2026 02:35 | Expandiendo configuración del servidor de pruebas
3df7fdc | 25/11/2026 02:27 | Ampliando pruebas de configuración en SQLite
8c18ba3 | 25/11/2026 02:22 | Incorporando gestión de configuración en SQLite
027e890 | 25/11/2026 01:58 | Integrando y verificando el módulo de almacenamiento SQLite
077343f | 25/11/2026 01:33 | Implementando la estructura inicial del módulo de almacenamiento
7092157 | 25/11/2026 01:23 | Ampliando las pruebas del módulo de comunicación
ce615b5 | 25/11/2026 00:57 | Preparando pruebas locales del módulo de comunicación
40c8395 | 25/11/2026 00:22 | Agregando autenticación Bearer a solicitudes HTTP
5c70bd0 | 25/11/2026 00:10 | Implementando solicitudes HTTP POST
64f2f0e | 25/11/2026 00:03 | Implementando solicitudes HTTP GET
04e51a6 | 24/11/2026 23:25 | Agregando construccion dinamica de URL HTTP
1f5f249 | 24/11/2026 23:05 | Agregando configuracion del manejador HTTP
9fe73dc | 24/11/2026 18:09 | Corregiendo validacion de datos recibidos
cb1d572 | 24/11/2026 18:01 | Agregando la recepcion de datos HTTP
ceac8fe | 24/11/2026 17:47 | Implementar administracion inicial de libcurl y respuestas HTTP
2175ac0 | 24/11/2026 01:03 | Implementando inicializacion y cierre de libcurl
181d03c | 24/11/2026 00:47 | Creando archivo de implementacion del modulo de comunicacion
e5769b2 | 24/11/2026 00:24 | Definiendo la interfaz del modulo de comunicacion HTTP
63ee9fc | 24/11/2026 00:02 | Creando encabezado inicial del modulo de red
979ade4 | 23/11/2026 23:53 | Organizando los archivos de prueba en otro directorio
3b5d02f | 23/11/2026 22:27 | Implementando reconocimiento de comandos en main.c
4b43e20 | 23/11/2026 22:21 | Implementando funcion principal y validacion de argumentos
7eb1cfb | 23/11/2026 21:52 | Creando estructura inicial de main.c
91807be | 23/11/2026 01:56 | Agregando la prueba de inicializacion de MQTT con libmosquitto
bbe04f0 | 23/11/2026 00:52 | Implementando y probando comunicacion HTTP con libcurl en Ubuntu
00a7ba8 | 22/11/2026 21:05 | Estructurando la sesion HTTP
958d660 | 22/11/2026 20:25 | Probando la integracion de libcurl
3136465 | 22/11/2026 20:06 | Probando la integracion del SQLite
f4a63e2 | 22/11/2026 18:08 | Añadiendo la biblioteca de SQLite
0a99d69 | 22/11/2026 16:55 | Empezando el proyecto definiendo el .gitignore
(END)
```
