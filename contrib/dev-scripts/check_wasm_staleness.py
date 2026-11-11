# -*- coding: utf-8 -*-
"""
O.Z.O.S WASM Staleness & Pinning Audit Tool
Verifies binary bit-exactness of shipped zero_dct.wasm against cryptographic pins
and alerts on supply-chain source drift.
"""
import hashlib
import os
import sys

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PINNED_SHA256 = "ea26872f8c14523bb3d823c4eca16dcae96c6a11f1be24bea75081c75b5f7a4e"

WASM_PATHS = [
    os.path.join(REPO_ROOT, "contrib", "web-cockpit", "zero_dct.wasm"),
    os.path.join(REPO_ROOT, "core", "zero_dct.wasm")
]

CPP_SOURCES = [
    os.path.join(REPO_ROOT, "core", "transport.cpp"),
    os.path.join(REPO_ROOT, "core", "dct.cpp"),
    os.path.join(REPO_ROOT, "core", "integrity.cpp")
]

def main():
    allow_drift = ("--allow-drift" in sys.argv or os.environ.get("OZOS_ALLOW_WASM_DRIFT") == "1")
    parser_fail_on_drift = not allow_drift
    print("=================================================================")
    print("  O.Z.O.S SUPPLY CHAIN WASM INTEGRITY & STALENESS AUDITOR")
    print("=================================================================")
    all_ok = True
    
    # 1. Verify file existence & SHA256 pin
    for path in WASM_PATHS:
        rel = os.path.relpath(path, REPO_ROOT)
        if not os.path.exists(path):
            print(f"[MISSING] File not found: {rel}")
            all_ok = False
            continue
        with open(path, "rb") as f:
            raw = f.read()
            h = hashlib.sha256(raw).hexdigest()
        if h == PINNED_SHA256:
            print(f"[MATCH] {rel}: SHA-256 matches canonical pin ({h[:16]}...)")
        else:
            print(f"[MISMATCH] {rel}: SHA-256 {h} != {PINNED_SHA256}")
            all_ok = False
        # Verify R14-H1 bytecode invariants (FNV-1a prime 0x01000193 LEB128 = 93838008, float mask 0x7F800000 LEB128 = 808080fc07)
        if b"\x93\x83\x80\x08" not in raw or b"\x80\x80\x80\xfc\x07" not in raw:
            print(f"[STALE LOGIC] {rel}: Missing R14-H1 FNV-1a checksum or float finiteness guard in WASM bytecode!")
            all_ok = False

    # 2. Check source drift / mtime
    if os.path.exists(WASM_PATHS[0]):
        wasm_mtime = max(os.path.getmtime(p) for p in WASM_PATHS if os.path.exists(p))
        newer_sources = []
        for src in CPP_SOURCES:
            if os.path.exists(src) and os.path.getmtime(src) > wasm_mtime + 1.0:
                newer_sources.append(os.path.relpath(src, REPO_ROOT))
        
        if newer_sources:
            print("[SOURCE DRIFT NOTICE] C++ sources newer than shipped zero_dct.wasm:")
            for s in newer_sources:
                print(f"   * {s}")
            print("   Note: Shipped WASM is deterministically pinned. When emcc is available,")
            print("   regenerate via: make wasm && python contrib/dev-scripts/check_wasm_staleness.py")
            if parser_fail_on_drift:
                print("[STRICT FAILURE] Source drift detected under strict mode (--fail-on-drift default)!")
                all_ok = False
        else:
            print("[IN SYNC] Shipped WASM binary matches or post-dates C++ source files.")

    print("=================================================================")
    sys.exit(0 if all_ok else 1)

if __name__ == "__main__":
    main()
