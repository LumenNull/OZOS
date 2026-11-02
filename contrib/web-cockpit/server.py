# -*- coding: utf-8 -*-
"""
O.Z.O.S - Local Sovereign HTTP Server & Native Microkernel Bridge
Serves the O.Z.O.S Cockpit locally with proper WASM MIME-types and zero-server isolation.
Bridges Web Cockpit directly to native ozos.exe via localhost API.
"""
import http.server
import socketserver
import os
import sys
import json
import subprocess
import webbrowser

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

PORT = 8899
DIRECTORY = os.path.dirname(os.path.abspath(__file__))
OZOS_EXE = os.path.join(DIRECTORY, "ozos.exe")

class OZOSRequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
        origin = self.headers.get('Origin', '') if hasattr(self, 'headers') and self.headers else ''
        if origin in ['http://localhost:8899', 'http://127.0.0.1:8899']:
            self.send_header('Access-Control-Allow-Origin', origin)
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(200)
        self.end_headers()

    def do_POST(self):
        origin = self.headers.get('Origin')
        if origin and origin not in ['http://localhost:8899', 'http://127.0.0.1:8899']:
            self.send_response(403)
            self.end_headers()
            self.wfile.write(b'{"error":"Forbidden: Cross-Origin Request Blocked"}\n')
            return
        if self.path == '/api/cmd':
            content_length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(content_length).decode('utf-8')
            try:
                data = json.loads(body)
                args = data.get('args', [])
                cmd = [OZOS_EXE] + [str(a) for a in args]
                res = subprocess.run(cmd, capture_output=True, encoding='utf-8', errors='replace', cwd=DIRECTORY)
                resp_data = {
                    "success": res.returncode == 0,
                    "stdout": res.stdout,
                    "stderr": res.stderr
                }
            except Exception as e:
                resp_data = {
                    "success": False,
                    "stdout": "",
                    "stderr": str(e)
                }

            resp_bytes = json.dumps(resp_data, ensure_ascii=False).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.send_header('Content-Length', str(len(resp_bytes)))
            self.end_headers()
            self.wfile.write(resp_bytes)
            return

        super().do_POST()

    def guess_type(self, path):
        if path.endswith('.wasm'):
            return 'application/wasm'
        if path.endswith('.js'):
            return 'application/javascript'
        return super().guess_type(path)

if __name__ == '__main__':
    os.chdir(DIRECTORY)
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("127.0.0.1", PORT), OZOSRequestHandler) as httpd:
        print("=================================================================")
        print("  [OK] O.Z.O.S SOVEREIGN COMMAND STATION & NATIVE BRIDGE ACTIVE")
        print(f"  [URL] http://localhost:{PORT}")
        print("  [MODE] 100% Offline Air-Gapped Local Server")
        print("=================================================================")
        
        if "--open" in sys.argv:
            webbrowser.open(f"http://localhost:{PORT}")
            
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n[OZOS] Đã tắt trạm.")
            httpd.server_close()
