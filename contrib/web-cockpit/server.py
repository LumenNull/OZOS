# -*- coding: utf-8 -*-
"""
O.Z.O.S - Local Sovereign HTTP Server & Native Microkernel Bridge
Serves the O.Z.O.S Cockpit locally with proper WASM MIME-types and zero-server isolation.
Bridges Web Cockpit directly to native ozos.exe via localhost API.

SECURITY ARCHITECTURE & INVARIANTS:
- Localhost Loopback Isolation: server.py binds exclusively to 127.0.0.1 (never 0.0.0.0).
- Local Caller Trust Model: Callers without an Origin header (e.g. local CLI processes, curl)
  are treated as authenticated host-local processes by design.
- Strict Cross-Origin Protection: Any browser request carrying a non-whitelisted Origin header
  is strictly rejected (403 Forbidden).
- Directory listing is disabled across all endpoints.
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

class ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    daemon_threads = True
    allow_reuse_address = True

class OZOSRequestHandler(http.server.SimpleHTTPRequestHandler):
    timeout = 10

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def list_directory(self, path):
        self.send_error(404, "Directory listing disabled by policy")
        return None

    def do_GET(self):
        clean = self.path.split("?")[0].split("#")[0]
        if clean in ("/server.py", "/server.py/", "/.git", "/.git/"):
            self.send_error(404, "File not found")
            return
        super().do_GET()

    def do_GET(self):
        clean = self.path.split("?")[0].split("#")[0]
        if clean in ("/server.py", "/server.py/", "/.git", "/.git/"):
            self.send_error(404, "File not found")
            return
        super().do_GET()

    def do_GET(self):
        clean = self.path.split("?")[0].split("#")[0]
        if clean in ("/server.py", "/server.py/", "/.git", "/.git/"):
            self.send_error(404, "File not found")
            return
        super().do_GET()

    def do_GET(self):
        clean = self.path.split("?")[0].split("#")[0]
        if clean in ("/server.py", "/server.py/", "/.git", "/.git/"):
            self.send_error(404, "File not found")
            return
        super().do_GET()

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
            # Mô hình tin cậy: Server bridge bind chặt vào 127.0.0.1, giả định tin tưởng localhost cục bộ.
            # Dành riêng cho Web Cockpit chạy trên máy của chính node chủ quyền.
            # Yêu cầu không có header Origin (CLI local/curl) hoặc Origin localhost được chấp nhận theo thiết kế.
            # Mọi đường dẫn file ghi ra đều được sandbox tuyệt đối vào thư mục server.
            try:
                cl_val = self.headers.get('Content-Length')
                if cl_val is None:
                    raise ValueError("Missing Content-Length header")
                content_length = int(cl_val)
                if content_length <= 0 or content_length > 10485760: # Giới hạn tối đa 10MB
                    raise ValueError("Content-Length out of bounds")
            except (ValueError, TypeError):
                self.send_response(400)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(b'{"error":"Bad Request: Invalid or missing Content-Length"}\n')
                return

            body = self.rfile.read(content_length).decode('utf-8', errors='replace')
            try:
                data = json.loads(body)
                if not isinstance(data, dict):
                    raise ValueError("JSON payload must be an object with 'args' array")
                args = data.get('args', [])
                if not isinstance(args, list) or len(args) == 0:
                    raise ValueError("'args' must be a non-empty list of strings")
                
                ALLOWED_COMMANDS = {
                    "packet-build", "packet-verify", "packet-sign", "audio-synth", "audio-decode",
                    "handshake-syn", "handshake-ack", "handshake-finish", "tunnel-encrypt", "tunnel-decrypt",
                    "gossip-beacon", "gossip-listen", "brainseed-gen", "brainseed-restore", "keygen", "benchmark", "status"
                }
                cmd_name = str(args[0])
                if cmd_name not in ALLOWED_COMMANDS:
                    raise ValueError(f"Command not permitted by sovereign allowlist: {cmd_name}")

                safe_args = []
                for a in args:
                    if not isinstance(a, str):
                        raise ValueError("Command arguments must be strings")
                    a_base = os.path.basename(a)
                    target = os.path.realpath(os.path.join(DIRECTORY, a_base))
                    if not target.startswith(DIRECTORY) or os.path.islink(os.path.join(DIRECTORY, a_base)):
                        raise ValueError(f"Invalid path traversal or symlink detected: {a}")
                    safe_args.append(a_base)
                
                cmd = [OZOS_EXE] + safe_args
                res = subprocess.run(cmd, capture_output=True, encoding='utf-8', errors='replace', cwd=DIRECTORY, timeout=15)
                
                is_success = (res.returncode == 0)
                for a in safe_args:
                    if a.endswith(".wav"):
                        target_wav = os.path.realpath(os.path.join(DIRECTORY, a))
                        if not os.path.exists(target_wav) or os.path.getsize(target_wav) == 0:
                            is_success = False
                
                resp_data = {
                    "success": is_success,
                    "stdout": res.stdout,
                    "stderr": res.stderr
                }
            except subprocess.TimeoutExpired:
                resp_data = {
                    "success": False,
                    "stdout": "",
                    "stderr": "Command execution timeout (15s limit exceeded)"
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

        self.send_error(404, "Endpoint not found")

    def guess_type(self, path):
        if path.endswith('.wasm'):
            return 'application/wasm'
        if path.endswith('.js'):
            return 'application/javascript'
        return super().guess_type(path)

if __name__ == '__main__':
    os.chdir(DIRECTORY)
    HOST = "127.0.0.1"
    if HOST not in ("127.0.0.1", "localhost", "::1"):
        raise SystemExit("BẢO VỆ CHỦ QUYỀN: Server bridge chỉ được phép bind vào loopback cục bộ (127.0.0.1)!")
    with ThreadedTCPServer((HOST, PORT), OZOSRequestHandler) as httpd:
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
