# -*- coding: utf-8 -*-
"""
O.Z.O.S - Native Tactical Desktop Launcher (Trench 1)
Bypasses standard browser UX by launching a Frameless Native Standalone App
bound directly to Windows Hardware TPM 2.0.
"""

import os
import sys
import subprocess
import time
import socket
import json

from core.ozos_tpm_anchor import OZOSHardwareTPM

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

PORT = 8899
DIR = os.path.dirname(os.path.abspath(__file__))

def is_server_running(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        return s.connect_ex(('127.0.0.1', port)) == 0

def find_standalone_browser_binary():
    """Locate browser binary supporting --app flag (Edge or Chrome)."""
    paths = [
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
    ]
    for p in paths:
        if os.path.exists(p):
            return p
    return None

def launch_native_kiosk():
    print("=================================================================")
    print("  [O.Z.O.S] INITIALIZING NATIVE DESKTOP SOVEREIGN CONSOLE")
    print("=================================================================")

    # 1. Initialize Hardware TPM 2.0 Root of Trust
    print("[TPM 2.0] Connecting to hardware silicon via ncrypt.dll...")
    tpm = OZOSHardwareTPM()
    badge = tpm.export_provenance_badge()
    print(f"  -> TPM Status: {badge['tpm_status']}")
    print(f"  -> Provider: {badge['provider']}")
    print(f"  -> Silicon Fingerprint: {badge['master_silicon_seed_fingerprint']}")
    
    # Save TPM provenance cert to runtime
    tpm_cert_path = os.path.join(DIR, "core", "tpm_provenance.json")
    with open(tpm_cert_path, "w", encoding="utf-8") as f:
        json.dump(badge, f, indent=2, ensure_ascii=False)
    tpm.close()

    # 2. Verify Local Server
    if not is_server_running(PORT):
        print(f"[Server] Activating local loopback station on port {PORT}...")
        subprocess.Popen([sys.executable, os.path.join(DIR, "server.py")], cwd=DIR)
        time.sleep(1.2)
    else:
        print(f"[Server] Local station active at http://localhost:{PORT}")

    # 3. Launch Standalone Kiosk Window
    bin_path = find_standalone_browser_binary()
    app_url = f"http://localhost:{PORT}"

    if bin_path:
        print(f"[Desktop Shell] Launching Native Application Window via {os.path.basename(bin_path)}...")
        cmd = [
            bin_path,
            f"--app={app_url}",
            "--window-size=1600,940",
            "--window-position=40,40",
            "--disable-dev-tools",
            "--no-first-run",
            "--no-default-browser-check",
            "--disable-extensions",
            "--disable-plugins",
            "--disable-background-networking"
        ]
        subprocess.Popen(cmd)
        print("  -> Frameless Native Desktop window initialized successfully.")
    else:
        import webbrowser
        print("[Desktop Shell] Standalone binary not found; falling back to default browser.")
        webbrowser.open(app_url)

    print("=================================================================")
    print("  [OK] TRENCH 1 (NATIVE HARDWARE DESKTOP) READY!")
    print("=================================================================")

if __name__ == '__main__':
    launch_native_kiosk()
