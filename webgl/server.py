#!/usr/bin/env python

import os
import ssl

from http.server import HTTPServer, SimpleHTTPRequestHandler

ENABLE_HTTPS = os.environ.get('ENABLE_HTTPS', True)
PORT = int(os.environ.get('PORT', 8000))

class MyHTTPRequestHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        super().end_headers()


if __name__ == '__main__':
    with HTTPServer(("", PORT), MyHTTPRequestHandler) as httpd:
        if ENABLE_HTTPS:
            context = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
            context.check_hostname = False
            context.load_cert_chain(
                certfile="example.crt", keyfile="example.key")
            httpd.socket = context.wrap_socket(httpd.socket, server_side=True)

        print(f"server at {'https' if ENABLE_HTTPS else 'http'}://0.0.0.0:{PORT}")
        httpd.serve_forever()
