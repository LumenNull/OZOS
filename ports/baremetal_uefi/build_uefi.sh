#!/bin/sh
# ==============================================================================
# O.Z.O.S // Bare-Metal UEFI x86_64 Enclave Cross-Builder (Linux / macOS)
# Requires: clang + lld (e.g., apt-get install clang lld)
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORE_DIR="$(cd "$SCRIPT_DIR/../../core" && pwd)"

cd "$SCRIPT_DIR"

echo "====================================================================="
echo "  O.Z.O.S // BARE-METAL UEFI x86_64 CROSS-BUILDER (CLANG + LLD)"
echo "====================================================================="

CC="${CLANG:-clang}"
LD="${LLD_LINK:-}"
if [ -z "$LD" ]; then
    for cand in lld-link lld-link-19 lld-link-18 lld-link-17 lld-link-16; do
        if command -v "$cand" >/dev/null 2>&1; then
            LD="$cand"
            break
        fi
    done
fi

if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[ERROR] clang not found. Install clang and lld."
    exit 1
fi
if [ -z "$LD" ] || ! command -v "$LD" >/dev/null 2>&1; then
    echo "[ERROR] lld-link not found. Install lld."
    exit 1
fi

CFLAGS="-target x86_64-unknown-windows -ffreestanding -fno-stack-protector -mno-stack-arg-probe -fshort-wchar -mno-red-zone -O2 -Wall -DOZOS_BARE_METAL -I. -I$CORE_DIR"

echo "[*] Compiling freestanding UEFI kernel..."
"$CC" $CFLAGS -c ozos_uefi_kernel.c -o ozos_uefi_kernel.obj

echo "[*] Compiling bare-metal TweetNaCl cryptographic engine..."
"$CC" -target x86_64-unknown-windows -ffreestanding -fno-stack-protector -mno-stack-arg-probe -mno-red-zone -O2 -DOZOS_BARE_METAL -I. -I"$CORE_DIR" -c "$CORE_DIR/tweetnacl.c" -o tweetnacl_efi.obj

echo "[*] Linking PE32+ EFI Bare-Metal Boot Image (BOOTX64.EFI)..."
mkdir -p EFI/BOOT
"$LD" -subsystem:efi_application -entry:efi_main -nodefaultlib -machine:x64 -out:EFI/BOOT/BOOTX64.EFI ozos_uefi_kernel.obj tweetnacl_efi.obj

rm -f ozos_uefi_kernel.obj tweetnacl_efi.obj

echo "====================================================================="
echo "  [SUCCESS] BARE-METAL IMAGE BUILT: $SCRIPT_DIR/EFI/BOOT/BOOTX64.EFI"
echo "====================================================================="
