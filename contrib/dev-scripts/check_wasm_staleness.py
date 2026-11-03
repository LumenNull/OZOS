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
PINNED_SHA256 = "d7924fedcda753e0b9eefdaf779eb657f67fba50da96472dc98c92b393d7b4a3"

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
    print("=================================================================")
    print("  O.Z.O.S SUPPLY CHAIN WASM INTEGRITY & STALENESS AUDITOR")
    print("=================================================================")
    all_ok = True
    
    # 1. Verify file existence & SHA256 pin
    for path in WASM_PATHS:
        rel = os.path.relpath(path, REPO_ROOT)
        if not os.path.exists(path):
            print(f"❌ [MISSING] File not found: {rel}")
            all_ok = False
            continue
        with open(path, "rb") as f:
            h = hashlib.sha256(f.read()).hexdigest()
        if h == PINNED_SHA256:
            print(f"✅ [MATCH] {rel}: SHA-256 matches canonical pin ({h[:16]}...)")
        else:
            print(f"❌ [MISMATCH] {rel}: SHA-256 {h} != {PINNED_SHA256}")
            all_ok = False

    # 2. Check source drift / mtime
    if os.path.exists(WASM_PATHS[0]):
        wasm_mtime = os.path.getmtime(WASM_PATHS[0])
        newer_sources = []
        for src in CPP_SOURCES:
            if os.path.exists(src) and os.path.getmtime(src) > wasm_mtime:
                newer_sources.append(os.path.relpath(src, REPO_ROOT))
        
        if newer_sources:
            print("⚠️ [SOURCE DRIFT NOTICE] C++ sources newer than shipped zero_dct.wasm:")
            for s in newer_sources:
                print(f"   • {s}")
            print("   💡 Note: Shipped WASM is deterministically pinned. When emcc is available,")
            print("      regenerate via: make wasm && python contrib/dev-scripts/check_wasm_staleness.py")
        else:
            print("✅ [IN SYNC] Shipped WASM binary matches or post-dates C++ source files.")

    print("=================================================================")
    sys.exit(0 if all_ok else 1)

if __name__ == "__main__":
    main()
