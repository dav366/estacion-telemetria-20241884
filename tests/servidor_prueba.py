
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs
import json
import math
import os
import time

TOKEN_PRUEBA = "token-prueba-local"

TOTAL_LECTURAS = 405
UMBRAL_TEMP = 75.0

MAQUINAS = ["MOTOR", "BOMBA", "VALVULA", "TURBINA"]
TELEMETRIA_RECIBIDA = set()

# Ventanas de interrupción configurables para las pruebas.
DURACION_CORTE_S = int(os.getenv("DURACION_CORTE_S", "60"))
INICIO_CORTE_1_S = int(os.getenv("INICIO_CORTE_1_S", "30"))
INICIO_CORTE_2_S = int(os.getenv("INICIO_CORTE_2_S", "180"))
INICIO_TELEMETRIA = None


def tiempo_corte_restante():
    global INICIO_TELEMETRIA

    if INICIO_TELEMETRIA is None:
        INICIO_TELEMETRIA = time.monotonic()

    transcurrido = time.monotonic() - INICIO_TELEMETRIA

    for inicio in (INICIO_CORTE_1_S, INICIO_CORTE_2_S):
        fin = inicio + DURACION_CORTE_S

        if inicio <= transcurrido < fin:
            return math.ceil(fin - transcurrido)

    return 0


def generar_lectura(identificador):
    maquinas = MAQUINAS
    maquina = maquinas[(identificador - 1) % len(maquinas)]

    temperatura = f"{25 + (identificador % 60) * 0.5:.2f}"
    corriente = f"{2 + (identificador % 30) * 0.1:.2f}"
    vibracion = f"{0.1 + (identificador % 15) * 0.1:.2f}"

    estado = "OK"

    if identificador % 10 == 0:
        estado = "ALARMA"

    # Campos vacíos para comprobar el almacenamiento NULL.
    if identificador == 3:
        temperatura = ""

    if identificador == 7:
        corriente = ""

    return (
        f"{identificador},{maquina},"
        f"2026-11-25 10:00:{identificador % 60:02d},"
        f"{temperatura},{corriente},{vibracion},{estado}"
    )


def calcular_analisis_esperado():
    resultados = {}

    for maquina in MAQUINAS:
        resultados[maquina] = {
            "validas": 0,
            "descartadas": 0,
            "temperaturas": [],
            "sobre_umbral": 0
        }

    for identificador in range(1, TOTAL_LECTURAS + 1):
        campos = generar_lectura(identificador).split(",")

        maquina = campos[1]
        temperatura = campos[3]
        corriente = campos[4]
        vibracion = campos[5]
        estado = campos[6]

        registro = resultados[maquina]

        valido = (
            estado == "OK"
            and temperatura != ""
            and corriente != ""
            and vibracion != ""
        )

        if not valido:
            registro["descartadas"] += 1
            continue

        temp = float(temperatura)

        registro["validas"] += 1
        registro["temperaturas"].append(temp)

        if temp > UMBRAL_TEMP:
            registro["sobre_umbral"] += 1

    for registro in resultados.values():
        temperaturas = registro["temperaturas"]

        if temperaturas:
            registro["temp_min"] = min(temperaturas)
            registro["temp_max"] = max(temperaturas)
            registro["temp_prom"] = (
                sum(temperaturas) / len(temperaturas)
            )
        else:
            registro["temp_min"] = 0.0
            registro["temp_max"] = 0.0
            registro["temp_prom"] = 0.0

        del registro["temperaturas"]

    return resultados


def validar_analisis(datos):
    if not isinstance(datos, dict):
        return False

    maquinas_recibidas = datos.get("maquinas")

    if not isinstance(maquinas_recibidas, list):
        return False

    if len(maquinas_recibidas) != len(MAQUINAS):
        return False

    esperados = calcular_analisis_esperado()
    recibidos = {}

    for maquina in maquinas_recibidas:
        if not isinstance(maquina, dict):
            return False

        nombre = maquina.get("maquina")

        if nombre not in MAQUINAS or nombre in recibidos:
            return False

        recibidos[nombre] = maquina

    if set(recibidos.keys()) != set(MAQUINAS):
        return False

    campos_enteros = [
        "validas",
        "descartadas",
        "sobre_umbral"
    ]

    campos_decimales = [
        "temp_min",
        "temp_max",
        "temp_prom"
    ]

    for nombre in MAQUINAS:
        recibido = recibidos[nombre]
        esperado = esperados[nombre]

        for campo in campos_enteros:
            valor = recibido.get(campo)

            if isinstance(valor, bool) or not isinstance(valor, int):
                return False

            if valor != esperado[campo]:
                return False

        for campo in campos_decimales:
            valor = recibido.get(campo)

            if isinstance(valor, bool) or not isinstance(valor, (int, float)):
                return False

            if abs(float(valor) - esperado[campo]) > 0.011:
                return False

    return True


class ServidorPrueba(BaseHTTPRequestHandler):

    def responder(self, codigo, contenido, encabezados=None):
        datos = contenido.encode("utf-8")

        self.send_response(codigo)
        self.send_header(
            "Content-Type",
            "text/plain; charset=utf-8"
        )
        self.send_header("Content-Length", str(len(datos)))

        if encabezados:
            for nombre, valor in encabezados.items():
                self.send_header(nombre, str(valor))

        self.end_headers()
        self.wfile.write(datos)

    def autenticar(self):
        autorizacion = self.headers.get("Authorization", "")
        return autorizacion == f"Bearer {TOKEN_PRUEBA}"

    def do_GET(self):
        ruta = urlparse(self.path)
        parametros = parse_qs(ruta.query)

        if ruta.path == "/api/v1/ping":
            self.responder(200, "estado=activo")
            return

        if not self.autenticar():
            self.responder(401, "error=autenticacion")
            return

        if ruta.path == "/api/v1/config":
            configuracion = (
                "matricula=20241884\n"
                "hora=12:00\n"
                "umbral_temp=75.0\n"
                "intervalo_s=10\n"
                "latido_s=10\n"
                "maquinas=4\n"
                "lote_max=200"
            )

            self.responder(200, configuracion)
            return

        if ruta.path == "/api/v1/lecturas":
            try:
                desde_id = int(
                    parametros.get("desde_id", ["0"])[0]
                )

                limite = int(
                    parametros.get("limite", ["200"])[0]
                )

                if desde_id < 0 or not 1 <= limite <= 200:
                    raise ValueError

            except (ValueError, TypeError):
                self.responder(400, "error=parametros_invalidos")
                return

            lecturas = [
                generar_lectura(i)
                for i in range(max(1, desde_id), TOTAL_LECTURAS + 1)
            ]

            lecturas = lecturas[:limite]

            contenido = (
                "id,maquina,ts,temp,corriente,vibracion,estado\n"
                + "\n".join(lecturas)
            )

            self.responder(200, contenido)
            return

        self.responder(404, "error=no_encontrado")

    def do_POST(self):
        if not self.autenticar():
            self.responder(401, "error=autenticacion")
            return

        longitud = int(self.headers.get("Content-Length", 0))
        contenido = self.rfile.read(longitud)

        try:
            datos = json.loads(contenido)
        except (json.JSONDecodeError, UnicodeDecodeError):
            self.responder(400, "error=json_invalido")
            return

        ruta = urlparse(self.path).path

        if ruta == "/api/v1/telemetria":
            restante = tiempo_corte_restante()

            if restante > 0:
                print(
                    "Interrupción simulada | HTTP 503 | "
                    "Retry-After:", restante
                )
                self.responder(
                    503,
                    "error=servidor_temporalmente_no_disponible",
                    {"Retry-After": restante}
                )
                return

            if not isinstance(datos, dict):
                self.responder(400, "error=datos_invalidos")
                return

            seq = datos.get("seq")
            ts = datos.get("ts")
            temp = datos.get("temp")
            corriente = datos.get("corriente")

            if (
                isinstance(seq, bool)
                or not isinstance(seq, int)
                or seq <= 0
                or isinstance(ts, bool)
                or not isinstance(ts, int)
                or isinstance(temp, bool)
                or not isinstance(temp, (int, float))
                or isinstance(corriente, bool)
                or not isinstance(corriente, (int, float))
                or not math.isfinite(temp)
                or not math.isfinite(corriente)
            ):
                self.responder(400, "error=datos_invalidos")
                return

            if seq in TELEMETRIA_RECIBIDA:
                self.responder(200, "estado=duplicada")
                return

            TELEMETRIA_RECIBIDA.add(seq)

            print(
                "\nTelemetría recibida:",
                "| seq:", seq,
                "| temp:", temp,
                "| corriente:", corriente
            )

            self.responder(201, "estado=creada")
            return

        if ruta == "/api/v1/respuestas/descarga":
            filas = datos.get("filas_unicas")
            suma = datos.get("suma_ids")

            print("\nResultado de descarga recibido:")
            print("Filas únicas:", filas)
            print("Suma de IDs:", suma)

            if filas != TOTAL_LECTURAS or suma != 82215:
                self.responder(400, "resultado=incorrecto")
                return

            self.responder(200, "resultado=correcto")
            return

        if ruta == "/api/v1/respuestas/analisis":
            print("\nAnálisis recibido del programa:")

            if not isinstance(datos, dict):
                self.responder(400, "resultado=incorrecto")
                return

            for maquina in datos.get("maquinas", []):
                if isinstance(maquina, dict):
                    print(
                        maquina.get("maquina"),
                        "| Válidas:", maquina.get("validas"),
                        "| Descartadas:", maquina.get("descartadas"),
                        "| Promedio:", maquina.get("temp_prom"),
                        "| Sobre umbral:", maquina.get("sobre_umbral")
                    )

            if not validar_analisis(datos):
                print("Validación del análisis: INCORRECTA")
                self.responder(400, "resultado=incorrecto")
                return

            print("Validación del análisis: CORRECTA")
            self.responder(200, "resultado=correcto")
            return

        self.responder(404, "error=no_encontrado")

    def log_message(self, formato, *argumentos):
        print("Solicitud:", formato % argumentos)


if __name__ == "__main__":
    servidor = HTTPServer(("0.0.0.0", 4000), ServidorPrueba)

    print("Servidor de pruebas iniciado en el puerto 4000")
    print("Dataset: 405 lecturas")
    print("Token de prueba configurado")

    try:
        servidor.serve_forever()
    except KeyboardInterrupt:
        print("\nServidor detenido")
    finally:
        servidor.server_close()
