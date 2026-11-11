# O.Z.O.S Bare-Metal UEFI Ring-0 Kernel (`BOOTX64.EFI`)

Freestanding `x86_64` UEFI boot image compiled directly from [`core/tweetnacl.c`](../../core/tweetnacl.c), [`core/ozos_brainseed.h`](../../core/ozos_brainseed.h), [`core/ozos_crypto.h`](../../core/ozos_crypto.h), [`core/ozos_omni_phy.h`](../../core/ozos_omni_phy.h), [`core/ozos_pqc_hybrid.h`](../../core/ozos_pqc_hybrid.h), and [`ozos_uefi_kernel.c`](ozos_uefi_kernel.c) with **zero host OS**, **zero C runtime (CRT)**, **zero dynamic heap (`malloc = 0`)**, and **zero PE/DLL imports**.

Unlike the hosted userspace CLIs (`ozos`, `ozos_ghost`) running under Windows, Linux, or Android, `BOOTX64.EFI` executes natively in `x86_64` Ring-0 firmware context before any operating system kernel loads.

---

## Architecture & Capabilities

| Property | Specification |
| :--- | :--- |
| **Execution Mode** | `x86_64` Ring-0 / UEFI Firmware Boot Services Context |
| **Subsystem** | `IMAGE_SUBSYSTEM_EFI_APPLICATION` (`0xA`, PE32+ x64) |
| **External Dependencies** | **0 DLLs / 0 OS imports** (verified via `dumpbin /imports` & `llvm-objdump`) |
| **Memory Allocation** | Zero heap; compiler-barriered 3-pass SRAM wipe (`0xAA -> 0x55 -> 0x00`) |
| **Entropy Source** | CPU `RDRAND` (`_rdrand64_step` via `CPUID.(EAX=1):ECX[30]`), `RDTSC` jitter fallback |
| **Cryptographic Core** | RFC 8032 Ed25519, NIST FIPS 203 ML-KEM-768 + X25519 Hybrid KEM, BIP-39 12-word PBKDF2-HMAC-SHA512 (2,048 rounds), Poly1305, XSalsa20-Poly1305 |
| **Air-Gap Video Output** | Native UEFI GOP (`EFI_GRAPHICS_OUTPUT_PROTOCOL::Blt`) rendering 32x32 Optical Glyphs + UTF-16 Block fallback |

### Interactive Firmware Shell
Upon boot, `BOOTX64.EFI` checks CPU `RDRAND`, generates an initial ephemeral keypair, signs & verifies an `OZOS-80` test frame, renders the 32x32 Optical Glyph via GOP, and runs an interactive shell:
- `[1]` Generate new ephemeral Ed25519 keypair from hardware `RDRAND`
- `[2]` Sign & verify an 80-byte `OZOS-80` packet, print CPU cycle count, and draw its 32x32 Optical Glyph via GOP
- `[3]` Run a 100,000-frame Ring-0 CRC16-CCITT benchmark
- `[4]` NIST SP 800-88 volatile SRAM scrub + `ResetSystem(EfiResetShutdown)` ACPI halt
- `[5]` **BIP-39 12-word Brainseed Restore** (`ozos_brainseed_restore` + 2,048 PBKDF2 rounds + optical identity glyph)
- `[6]` **X25519 ECDH Air-Gap Handshake + XSalsa20-Poly1305** (emits 42-byte `SYN` hex + GOP Optical Glyph; accepts peer `ACK` from Android/PC or runs internal loopback Responder + AEAD self-test)
- `[7]` **Post-Quantum Hybrid KEM (NIST FIPS 203 ML-KEM-768 + X25519)** (RDRAND-seeded keygen, encaps/decaps shared secret check, and 1-bit ciphertext Fujisaki-Okamoto implicit rejection check)
- `[G]` Render active 32x32 Optical Glyph in console block mode (useful for serial / text-only UEFI firmware)
- `[R]` Scrub SRAM and cold reboot (`EfiResetCold`)

---

## Building (`BOOTX64.EFI`)

### Option A: Windows (MSVC Build Tools x64)
Requires MSVC x64 (`vcvars64.bat`, no external EDK2 SDK required):
```cmd
cd ports\baremetal_uefi
build_uefi.bat
```
Verify PE headers and zero DLL imports:
```cmd
dumpbin /headers /imports ports\baremetal_uefi\EFI\BOOT\BOOTX64.EFI
```

### Option B: Linux / macOS (Clang + LLD Cross-Compile)
Requires `clang` and `lld`:
```bash
sh ports/baremetal_uefi/build_uefi.sh
file ports/baremetal_uefi/EFI/BOOT/BOOTX64.EFI
```

Output image: `ports/baremetal_uefi/EFI/BOOT/BOOTX64.EFI` (~148 KB including the full 2,048-word BIP-39 dictionary and NIST FIPS 203 ML-KEM-768 PQC enclave).

---

## Booting on Real Hardware or QEMU

### Real x86_64 PC via FAT32 USB Drive
1. Format a USB flash drive as **FAT32**.
2. Copy the generated `EFI/` folder so the USB contains `/EFI/BOOT/BOOTX64.EFI`.
3. Open UEFI Boot Menu (`F12` / `F8` / `ESC`), disable Secure Boot (unsigned dev binary), and select the USB UEFI boot entry.

### QEMU + OVMF Virtual Firmware
```bash
qemu-system-x86_64 \
  -bios /usr/share/ovmf/OVMF.fd \
  -drive format=raw,file=fat:rw:ports/baremetal_uefi \
  -net none -m 128M
```

---

## Security Limitations
- **Unsigned Image (Secure Boot):** Self-compiled `BOOTX64.EFI` binaries are not signed by Microsoft UEFI CA; disable Secure Boot or enroll a custom DB key (`MOK`/`sbctl`) on physical hardware.
- **Volatile Custody Only:** Keys reside strictly in SRAM during runtime and are wiped on exit; keys are not sealed into TPM NVRAM across power cycles.
