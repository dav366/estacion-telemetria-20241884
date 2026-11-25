
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs
import json

TOKEN_PRUEBA = "token-prueba-local"

TOTAL_LECTURAS = 405


def generar_lectura(identificador):
    maquinas = ["MOTOR", "BOMBA", "VALVULA", "TURBINA"]
    maquina = maquinas[(identificador - 1) % len(maquinas)]

    temperatura = f"{25 + (identificador % 60) * 0.5:.2f}"
    corriente = f"{2 + (identificador % 30) * 0.1:.2f}"
    vibracion = f"{0.1 + (identificador % 15) * 0.1:.2f}"

    estado = "OK"

    if identificador % 10 == 0:
        estado = "ALARMA"

    # Algunos campos vacíos para comprobar el almacenamiento NULL.
    if identificador == 3:
        temperatura = ""

    if identificador == 7:
        corriente = ""

    return (
        f"{identificador},{maquina},"
        f"2026-11-25 10:00:{identificador % 60:02d},"
        f"{temperatura},{corriente},{vibracion},{estado}"
    )


class ServidorPrueba(BaseHTTPRequestHandler):

    def responder(self, codigo, contenido):
        datos = contenido.encode("utf-8")

        self.send_response(codigo)
        self.send_header(
            "Content-Type",
            "text/plain; charset=utf-8"
        )
        self.send_header("Content-Length", str(len(datos)))
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
