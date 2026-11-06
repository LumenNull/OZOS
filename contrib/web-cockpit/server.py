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
import time
import urllib.parse
import webbrowser

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

PORT = 8899
DIRECTORY = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(DIRECTORY, "..", ".."))


def resolve_ozos_exe():
    candidates = ["ozos.exe", "ozos"] if os.name == "nt" else ["ozos", "ozos.exe"]
    for base_dir in (DIRECTORY, REPO_ROOT):
        for name in candidates:
            p = os.path.join(base_dir, name)
            if os.path.isfile(p):
                return p
    return os.path.join(DIRECTORY, "ozos.exe" if os.name == "nt" else "ozos")


OZOS_EXE = resolve_ozos_exe()

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
        try:
            if "\x00" in self.path:
                self.send_error(404, "File not found")
                return
            clean_url_path = urllib.parse.unquote(self.path.split('?', 1)[0].split('#', 1)[0])
            if "\x00" in clean_url_path or ".." in clean_url_path:
                self.send_error(404, "File not found")
                return
            if clean_url_path in ("", "/"):
                self.path = "/index.html"

            translated = self.translate_path(self.path)
            if os.path.islink(translated):
                self.send_error(404, "File not found")
                return
            fs_path = os.path.realpath(translated)
            real_dir = os.path.realpath(DIRECTORY)
        except (ValueError, OSError):
            self.send_error(404, "File not found")
            return
        
        # 1. Directory containment
        if not (fs_path == real_dir or fs_path.startswith(real_dir + os.sep)):
            self.send_error(404, "File not found")
            return
            
        rel = os.path.relpath(fs_path, real_dir).replace('\\', '/')
        # 2. Block server.py, .git, python files (.py), hidden files/directories (starting with .)
        parts = rel.split('/')
        for part in parts:
            if part.startswith('.') or part.endswith('.py') or part == 'server.py':
                self.send_error(404, "File not found")
                return
        
        # 3. Allow only safe web cockpit static extensions (strictly no extensionless files)
        ALLOWED_EXTS = {'.html', '.js', '.wasm', '.css', '.wav', '.png', '.jpg', '.jpeg', '.svg', '.json', '.ico'}
        ext = os.path.splitext(fs_path)[1].lower()
        if not os.path.isfile(fs_path) or ext not in ALLOWED_EXTS:
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
            # Trust model: Server bridge strictly binds to 127.0.0.1, trusting local loopback callers.
            # Dedicated to Web Cockpit running on the sovereign node itself.
            # Requires no Origin header (local CLI/curl) or localhost Origin as designed.
            # All written files are strictly sandboxed inside the server directory.
            try:
                cl_val = self.headers.get('Content-Length')
                if cl_val is None:
                    raise ValueError("Missing Content-Length header")
                content_length = int(cl_val)
                if content_length <= 0 or content_length > 10485760: # 10MB maximum payload limit
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
                real_dir = os.path.realpath(DIRECTORY)
                for idx, a in enumerate(args):
                    if not isinstance(a, str):
                        raise ValueError("Command arguments must be strings")
                    if "\x00" in a:
                        raise ValueError(f"NUL byte detected in argument: {a}")

                    # PY-L1/L2: Distinguish file path arguments from generic payload arguments strictly by position
                    is_file_arg = (
                        (cmd_name == "audio-decode" and idx == 1)
                        or (cmd_name == "ofdm-decode" and idx == 1)
                        or (cmd_name == "audio-synth" and idx == 2)
                        or (cmd_name == "ofdm-synth" and idx == 2)
                        or (cmd_name == "handshake-syn" and idx == 1)
                        or (cmd_name == "gossip-beacon" and idx == 2)
                        or (cmd_name == "handshake-ack" and idx == 2 and not a.startswith("-"))
                    )
                    if is_file_arg:
                        if ".." in a or "/" in a or "\\" in a or a != os.path.basename(a) or not a.lower().endswith(".wav"):
                            raise ValueError(f"Invalid WAV filename argument: {a}")
                        raw_join = os.path.join(DIRECTORY, a)
                        target = os.path.realpath(raw_join)
                        if os.path.islink(raw_join) or not target.startswith(real_dir + os.sep):
                            raise ValueError(f"Invalid path traversal or symlink detected: {a}")
                        safe_args.append(a)
                    else:
                        safe_args.append(a)
                
                # PY-H1: Positional WAV targets only. Never suffix-scan all arguments!
                wav_targets = []
                target_arg_idx = None
                if cmd_name in ("audio-synth", "ofdm-synth") and len(safe_args) >= 3:
                    target_arg_idx = 2
                elif cmd_name == "handshake-syn" and len(safe_args) >= 2:
                    target_arg_idx = 1
                elif cmd_name == "gossip-beacon" and len(safe_args) >= 3:
                    target_arg_idx = 2
                elif cmd_name == "handshake-ack" and len(safe_args) >= 3 and not safe_args[2].startswith("-"):
                    target_arg_idx = 2

                if target_arg_idx is not None and target_arg_idx < len(safe_args):
                    target_filename = safe_args[target_arg_idx]
                    if target_filename.lower().endswith(".wav"):
                        target_wav = os.path.realpath(os.path.join(DIRECTORY, target_filename))
                        wav_targets.append(target_wav)
                        if os.path.exists(target_wav):
                            try:
                                os.remove(target_wav)
                            except OSError:
                                pass

                start_time = time.time()
                exe_path = resolve_ozos_exe()
                cmd = [exe_path] + safe_args
                res = subprocess.run(cmd, capture_output=True, encoding='utf-8', errors='replace', cwd=DIRECTORY, timeout=15)
                
                is_success = (res.returncode == 0)
                for target_wav in wav_targets:
                    if not os.path.exists(target_wav) or os.path.getsize(target_wav) <= 44:
                        is_success = False
                    elif os.path.getmtime(target_wav) < start_time - 0.5:
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
        raise SystemExit("SOVEREIGN PROTECTION: Server bridge must strictly bind to local loopback (127.0.0.1)!")
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
            print("\n[OZOS] Command station terminated.")
            httpd.server_close()
