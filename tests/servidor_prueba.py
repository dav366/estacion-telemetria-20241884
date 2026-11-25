
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs
import json

TOKEN_PRUEBA = "token-prueba-local"


class ServidorPrueba(BaseHTTPRequestHandler):

    def responder(self, codigo, contenido):
        datos = contenido.encode("utf-8")

        self.send_response(codigo)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
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
                "maquinas=3\n"
                "lote_max=200"
            )

            self.responder(200, configuracion)
            return

        if ruta.path == "/api/v1/lecturas":

            lecturas = [
                "1,MOTOR,2026-11-25 10:00:00,45.5,2.1,0.3,OK",
                "2,BOMBA,2026-11-25 10:00:10,48.2,2.4,0.5,OK",
                "3,MOTOR,2026-11-25 10:00:20,76.0,3.1,1.2,ALARMA",
                "4,VALVULA,2026-11-25 10:00:30,52.0,1.8,0.4,OK",
                "5,BOMBA,2026-11-25 10:00:40,81.5,3.5,1.4,ALARMA"
            ]

            try:
                desde_id = int(parametros.get("desde_id", ["0"])[0])
                limite = int(parametros.get("limite", ["200"])[0])

                if desde_id < 0 or limite < 1:
                    raise ValueError

            except (ValueError, TypeError):
                self.responder(400, "error=parametros_invalidos")
                return

            resultado = []

            for lectura in lecturas:
                identificador = int(lectura.split(",", 1)[0])

                if identificador >= desde_id:
                    resultado.append(lectura)

            resultado = resultado[:limite]

            self.responder(200, "\n".join(resultado))
            return

        self.responder(404, "error=no_encontrado")

    def do_POST(self):

        if not self.autenticar():
            self.responder(401, "error=autenticacion")
            return

        longitud = int(self.headers.get("Content-Length", 0))
        contenido = self.rfile.read(longitud)

        try:
            json.loads(contenido)
        except (json.JSONDecodeError, UnicodeDecodeError):
            self.responder(400, "error=json_invalido")
            return

        self.responder(200, "resultado=aceptado")

    def log_message(self, formato, *argumentos):
        print("Solicitud:", formato % argumentos)


if __name__ == "__main__":
    servidor = HTTPServer(("0.0.0.0", 4000), ServidorPrueba)

    print("Servidor de pruebas iniciado en el puerto 4000")
    print("Token de prueba configurado")

    try:
        servidor.serve_forever()
    except KeyboardInterrupt:
        print("\nServidor detenido")
    finally:
        servidor.server_close()
