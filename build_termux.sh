#!/bin/sh
# ==============================================================================
#   O.Z.O.S (Omni-Zero Operating Shield) - Android Termux 1-Touch Build Script
#   Target: ARM64 / ARMv7 Native Mobile Executable
# ==============================================================================
set -e

echo "================================================================="
echo "  [O.Z.O.S] COMPILING NATIVE ARM64 / ARMv7 BINARIES ON ANDROID"
echo "================================================================="

# Check compilers
if ! command -v clang >/dev/null 2>&1; then
    echo "[!] clang not found. Installing toolchain..."
    pkg update -y && pkg install clang make -y
fi

CFLAGS="-O3 -std=c99 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Icore -I."
CXXFLAGS="-O3 -std=c++11 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Icore -I."
LDFLAGS="-lstdc++ -lm -lpthread"

echo "[*] Compiling TweetNaCl cryptographic engine..."
clang $CFLAGS -c core/tweetnacl.c -o tweetnacl.o

echo "[*] Compiling Hardened Core (Fast Nonce, Tripwire MAC, DFA defense)..."
clang++ $CXXFLAGS -c core/ozos_hardened_core.cpp -o ozos_hardened_core.o

echo "[*] Linking Sovereign CLI utility (ozos)..."
clang $CFLAGS core/ozos_main.c tweetnacl.o ozos_hardened_core.o $LDFLAGS -o ozos

echo "[*] Linking Micro-Kernel daemon (ozos_ghost)..."
clang $CFLAGS core/ozos_ghost.c tweetnacl.o ozos_hardened_core.o $LDFLAGS -o ozos_ghost

echo "[*] Linking Workstation Sentinel (ozos-guard)..."
clang $CFLAGS core/ozos_guard_main.c tweetnacl.o ozos_hardened_core.o $LDFLAGS -o ozos-guard

echo "[*] Linking Post-Quantum Enclave (ozos-pqc)..."
clang $CFLAGS core/ozos_pqc_main.c tweetnacl.o ozos_hardened_core.o $LDFLAGS -o ozos-pqc

echo "[*] Compiling native verification suites (tests/test_libozos, test_audit_verify, test_omni_phy)..."
clang $CFLAGS tests/test_libozos.c tweetnacl.o ozos_hardened_core.o $LDFLAGS -o tests/test_libozos
clang $CFLAGS tests/test_audit_verify.c tweetnacl.o ozos_hardened_core.o $LDFLAGS -o tests/test_audit_verify
clang $CFLAGS tests/test_omni_phy.c $LDFLAGS -o tests/test_omni_phy

chmod +x ozos ozos_ghost ozos-guard ozos-pqc tests/test_libozos tests/test_audit_verify tests/test_omni_phy

echo "================================================================="
echo "  [SUCCESS] BUILT NATIVE BINARIES & TEST SUITES ON ANDROID:"
echo "    - ./ozos              (Sovereign Command-Line Utility & PQC/PHY)"
echo "    - ./ozos_ghost        (Headless Cryptographic Micro-Kernel)"
echo "    - ./ozos-guard        (Silent Background Sentinel Enclave)"
echo "    - ./ozos-pqc          (NIST FIPS 203 ML-KEM-768 + X25519 Enclave)"
echo "    - ./tests/test_*      (libozos, FIPS 203 ACVP KATs, Omni-PHY)"
echo "================================================================="
echo ""
echo "Test running right now:"
./ozos --help
