
from http.server import BaseHTTPRequestHandler, HTTPServer
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

        if self.path == "/api/v1/ping":
            self.responder(200, "estado=activo")
            return

        if self.path == "/api/v1/config":

            if not self.autenticar():
                self.responder(401, "error=autenticacion")
                return

            self.responder(200, "maquinas=3\nintervalo_s=10")
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
