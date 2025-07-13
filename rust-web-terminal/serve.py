#!/usr/bin/env python3
import http.server
import socketserver
import os

class MyHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        super().end_headers()

    def guess_type(self, path):
        mimetype = super().guess_type(path)
        if path.endswith('.wasm'):
            return 'application/wasm'
        return mimetype

PORT = 8080
HOST = "0.0.0.0"  # Bind to all interfaces
os.chdir(os.path.dirname(os.path.abspath(__file__)))

# Allow socket reuse
socketserver.TCPServer.allow_reuse_address = True

with socketserver.TCPServer((HOST, PORT), MyHTTPRequestHandler) as httpd:
    print(f"Server running at http://{HOST}:{PORT}/")
    print(f"Access locally at: http://localhost:{PORT}/")
    print(f"Access from network at: http://<your-ip>:{PORT}/")
    httpd.serve_forever()
