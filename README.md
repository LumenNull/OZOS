# O.Z.O.S (Omni-Zero Operating Shield)

[![CI: Build & Test](https://github.com/LumenNull/OZOS/actions/workflows/ci.yml/badge.svg)](https://github.com/LumenNull/OZOS/actions/workflows/ci.yml)
[![CI: Mutation Testing](https://github.com/LumenNull/OZOS/actions/workflows/mull_mutation.yml/badge.svg)](https://github.com/LumenNull/OZOS/actions/workflows/mull_mutation.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Spec: RFC-OZOS-0080](https://img.shields.io/badge/Spec-RFC--OZOS--0080-emerald.svg)](docs/RFC-OZOS-0080.md)

A minimal C99/C++20 cryptographic toolkit, fixed-size binary wire protocol (`OZOS-80`), air-gapped physical-layer modem (near-ultrasound 16-FSK/OFDM, 2D-DCT optical glyphs), and standalone bootable `x86_64` UEFI enclave (`BOOTX64.EFI`).

O.Z.O.S separates its **freestanding bare-metal execution target** from its **userspace host utilities**: cryptographic primitives (`core/tweetnacl.c`) compile cleanly with zero heap allocation (`malloc = 0`) either directly to Ring-0 UEFI firmware or into native cross-platform CLIs (Windows, Linux, macOS, and Android ARM64 via Termux).

---

## Execution Targets & Architecture

| Target | Privilege / Context | Dependencies | Source / Entry |
| :--- | :--- | :--- | :--- |
| **Bare-Metal UEFI (`BOOTX64.EFI`)** | `x86_64` Ring-0 firmware context (no host OS) | **0 DLLs / 0 CRT**, CPU `RDRAND`, `ML-KEM-768` PQC, BIP-39, X25519, GOP Optical Glyph | [`ports/baremetal_uefi/`](ports/baremetal_uefi/README.md) |
| **seL4 Microkit Topology** | Multi-PD microkernel architecture (reference port) | seL4 Microkit SDK specs | [`ports/sel4/`](ports/sel4/README.md) |
| **Native CLI Suite (`ozos`, `ozos_ghost`)** | Userspace stack-locked process (`VirtualLock`/`mlock`) | Standard C/C++, Win32 `ncrypt`/`winmm` or POSIX | [`core/`](core/) |
| **Web Cockpit / WASM Lab** | Browser / Node.js `SIMD128` verification harness | `zero_dct.wasm`, Node.js (optional laboratory UI) | [`contrib/web-cockpit/`](contrib/web-cockpit/) |

### Monorepo CLI Binaries (Userspace Targets)
- `ozos`: Unified CLI exposing key generation, Ed25519/X25519 operations, X25519+XSalsa20-Poly1305 air-gap tunnel encryption, acoustic/optical frame coding, and workstation diagnostics.
- `ozos_ghost`: Air-gap runtime for locked-memory ephemeral signing (`VirtualLock`/`mlock`), Goertzel ultrasonic loopback verification, genesis checksum verification, and memory/timing test harnesses.
- `ozos-core`: Minimal zero-heap cryptographic & Shamir Secret Sharing (`k`-of-`n` over `GF(2^8)`) binary.
- `ozos-phy`: Physical-layer codecs (`16-FSK`, `64-FFT OFDM` with Reed-Solomon FEC, `32x32` optical BMP glyphs, `LoRa` 84-byte UART framing).
- `ozos-guard`: Host posture inspector (`CPUID` hypervisor leaf check, active NIC enumeration, DACL hardening on Windows), Merkle directory anchor (`anchor`), `XSalsa20-Poly1305` sealed archive (`vault`), and NIST SP 800-88 file overwrite (`burn`).
- `ozos-pqc`: Hybrid `X25519` + `ML-KEM-768` (NIST FIPS 203 NTT `q=3329` + FO implicit rejection) & `ML-DSA-65` (NIST FIPS 204 Montgomery NTT `q=8380417`), ACVP KAT bit-exact, plus `vault pqc-pack` engine.

---

## Wire Format Specifications (`OZOS-80` & `GENS-v1`)

### Profile A: `OZOS-80` Canonical 80-Byte Frame
Fixed 640-bit header aligned on a 16-byte boundary for SIMD loading (`v128_load`), followed by optional payload and detached Ed25519 authentication.

```text
+-------------------------------------------------------------------------------+
|  DCT / CARRIER COEFFICIENTS (16 x float32 = 64 Bytes)                         |
|  [Offset 0x00 - 0x3F]  16-Byte SIMD Aligned                                   |
+-------------------------------------------------------+-----------------------+
|  MONOTONIC NONCE (uint64_t = 8 Bytes)                 |  MAGIC (uint32_t = 4B)|
|  [Offset 0x40 - 0x47]  Anti-Replay Counter            |  [0x48 - 0x4B] GEN\x01|
+---------------------------+---------------------------+-----------------------+
|  PAYLOAD LEN (uint16_t=2B)|  TRIPWIRE MAC16 (uint16_t = 2B)                   |
|  [Offset 0x4C - 0x4D]     |  [Offset 0x4E - 0x4F]  O(1) Ingress Prefilter     |
+---------------------------+---------------------------------------------------+
TOTAL HEADER SIZE: 80 BYTES (Zero Padding)
```

- **Sub-Profile A1 (`CRC16-CCITT`):** Fast $O(1)$ prefilter over header bytes `0..77` to reject corrupt/noise frames prior to signature verification. Provides **error detection only**, not payload integrity.
- **Sub-Profile A2 (`FNV-fold 16`):** Domain-separated checksum covering header (`0..77`) plus payload (`80..80+len`), emitted by `ozos packet-build` when `payload_len > 0`. Still unkeyed — **authentic integrity requires detached Ed25519 or Poly1305**.
- **Detached Ed25519 Signature:**
  ```text
  [OZOS-80 Header (80B)] + [Payload (L Bytes)] + [Ed25519 PubKey (32B)] + [Ed25519 Sig (64B)]
  ```

### Profile B: Web Cockpit Frame (`GENS-v1`)
Used exclusively by the browser WASM verification harness (`zero_dct.wasm`, `genesis-shield.js`, `ozos-engine.js`):

```text
+-----------------------+-------------------------------------------------------+
|  MAGIC (0x47454E01)   |  MONOTONIC NONCE (uint64_t = 8 Bytes)                 |
|  [Offset 0x00 - 0x03] |  [Offset 0x04 - 0x0B]                                 |
+-----------------------+-------------------------------------------------------+
|  PERCEPTUAL FINGERPRINT (15 x float32 = 60 Bytes)                             |
|  [Offset 0x0C - 0x47]  4x4 DCT Luminance Feature Anchors                      |
+-------------------------------------------------------+-----------------------+
|  PAYLOAD CHECKSUM (uint32_t = 4B)                     |  PAYLOAD LEN (u32=4B) |
|  [Offset 0x48 - 0x4B]                                 |  [Offset 0x4C - 0x4F] |
+-------------------------------------------------------+-----------------------+
```

---

## Threat Model, Verification Scope & Known Limitations

To preserve engineering transparency, O.Z.O.S explicitly defines its verified properties versus simulation limits and fallback behaviors:

1. **Bare-Metal Execution vs. Userspace Process Execution:**
   - True zero-OS hardware execution is provided **only** by [`ports/baremetal_uefi/BOOTX64.EFI`](ports/baremetal_uefi/README.md) (compiled with `/NODEFAULTLIB /SUBSYSTEM:EFI_APPLICATION`).
   - All host binaries (`ozos`, `ozos_ghost`, `ozos-guard`, etc.) execute in OS userspace, relying on `VirtualLock`/`mlock`, stack zeroization, and Win32 DACLs rather than replacing your workstation kernel.
2. **Real Host Measurements vs. Software Fault Simulations:**
   - **`memory-audit` (`ozos_torture.h`):** Runs **real host checks** for non-paged `VirtualLock` memory zeroization (`0/4096` leaked bytes) and paired `RDTSC` `dudect` cycle timing variance (2,000 samples). Its clock-drift watchdog (`+18%`), tamper-mesh impedance delta, and secondary opcode filter (`Audit 3/4`) are **software state-transition simulations**—they do not test physical decapped silicon or measure oscilloscope nano-ampere DPA traces.
   - **`fault-sim` (`ozos_cosmic.h`):** Deterministic userspace **simulations** verifying inbound buffer wiping, zeroed state retention, single-bit `OZOS-80` MAC16 rejection across 9 bit offsets, and absence of master bypass opcodes.
3. **Hardware Root of Trust vs. Platform Fallbacks:**
   - **Windows:** Uses `ncrypt.dll` hardware TPM 2.0 when available; without TPM, `ozos packet-sign` refuses silent fallback and requires explicit seed hex (`--seed` / positional seed) or BIP-39 `brainseed-gen`.
   - **Linux / POSIX / Android:** Hardware TPM requires `/dev/tpmrm0` / `/dev/tpm0`. On devices or VMs without hardware TPM access (e.g., Termux userspace), explicit master seed derivation (`brainseed-gen`) or seed arguments should be used rather than relying on `/etc/machine-id` (which is OS-readable and clonable across VMs).
4. **Physical Air-Gap Channels are Open Broadcast Media:**
   - **Acoustic (18.1–19.8 kHz):** Subject to microphone frequency rolloff, speaker distortion, room reverberation, and close-proximity attenuation (`< 2 m`). Unauthenticated `SYN` frames achieve Perfect Forward Secrecy (X25519 ECDH) but do **not** authenticate peer identity without pinning `--pinned-peer-pubkey` or out-of-band fingerprint verification (QR / verbal SAS / hex).
   - **Optical & RF:** Optical `32x32` glyphs (with `RS(255,223)` FEC) and `LoRa` 84-byte frames mitigate acoustic jamming when physical hardware adapters or line-of-sight optics are present.
5. **Stateless CLI Anti-Replay & Local Vault Storage:**
   - Monotonic nonce tracking in CLI tools persists locally (`.ozos_tunnel_nonces` / in-memory). Immunity against hypervisor VM snapshot rollback requires TPM NVRAM or seL4 hardware storage (`ports/sel4`).
   - Keyed browser Local Vault records (`ozos-engine.js`) prioritize tamper-evidence over confidentiality; store encrypted ciphertexts (not plaintext JSON) for sensitive browser storage.

---

## Deterministic Build Vector (`#000000`)

Every native build compiles a fixed baseline vector (`ozos_ghost genesis`) checked continuously by CI:

```text
Block Height : #000000 (Baseline Test Vector)
Inscription  : "Stay curious like children, so that we may draw closer to the knowledge of humankind."
Genesis Hash : 0x3b5bfe68f4a491a02c0fec66547ee142d36ba0038cb0c1c52ba8b14e80786edd (Double SHA-256)
Genesis PK   : 0x938998e24444b9fe70ac36b1de57f4178337b49871780161d3e68c3a4e84b769 (Ed25519)
Tripwire MAC : 0xE9C7 (CRC16-CCITT)
Status       : [ GENESIS CHECKSUM VERIFIED ]
```

---

## Building & Running Tests

### 1. Bare-Metal UEFI (`x86_64` `BOOTX64.EFI`)
Builds with zero external EDK2 dependencies on either **Windows (MSVC)** or **Linux/macOS (Clang + LLD)**:
```cmd
:: Windows (MSVC Build Tools x64)
cd ports\baremetal_uefi
build_uefi.bat
```
```bash
# Linux / macOS (Clang + LLD)
sh ports/baremetal_uefi/build_uefi.sh
```
Produces an ~148 KB zero-DLL PE32+ EFI boot image at `ports/baremetal_uefi/EFI/BOOT/BOOTX64.EFI` (including CPU `RDRAND` entropy, NIST FIPS 203 `ML-KEM-768` + `X25519` PQC hybrid KEM, BIP-39 12-word restore, XSalsa20-Poly1305 air-gap handshake, and UEFI GOP `32x32` Optical Glyph rendering). See [`ports/baremetal_uefi/README.md`](ports/baremetal_uefi/README.md) for USB FAT32 and QEMU/OVMF boot instructions.

### 2. Windows Native (`MSVC` / `Clang` / `GCC`)
```cmd
:: Build all native executables
build.bat

:: Run native memory, RDTSC timing, fault-sim, crypto, and WASM integrity suites
build.bat test

:: Run Python red-team & RFC conformance test suites
python tests\test_redteam_fixes.py
python tests\test_rfc_conformance.py

:: Clean build outputs
build.bat clean
```

### 3. Linux & macOS (`GCC` / `Clang`)
```bash
make all
make test
python3 tests/test_redteam_fixes.py
make clean
```

### 4. Android ARM64 / ARMv7 (`Termux`)
Tested natively on ARM64 Android devices for cross-architecture X25519 ECDH + XSalsa20-Poly1305 air-gap key exchange with `x86_64` hosts:
```bash
sh build_termux.sh
./ozos --version
./ozos brainseed-gen
```

---

## Verification & Supply-Chain Integrity

| Suite | Command | Coverage |
| :--- | :--- | :--- |
| **Native Kernel & Crypto** | `tests/test_libozos`, `tests/test_audit_verify` | Struct alignment, CRC16/FNV, TweetNaCl Ed25519/X25519/Poly1305, official NIST ACVP FIPS 203 `ML-KEM-768` & FIPS 204 `ML-DSA-65` KATs |
| **Bare-Metal UEFI Boot (CI)** | `python tests/test_uefi_qemu.py` | Live `x86_64` QEMU + OVMF firmware boot of `BOOTX64.EFI` (Ring-0 `RDRAND`, `ML-KEM-768` PQC, Ed25519, BIP-39, X25519+XSalsa20, GOP glyph, ACPI halt) |
| **Memory & RDTSC Audits** | `ozos_ghost memory-audit`, `ozos_ghost fault-sim` | `VirtualLock`/`mlock` 3-pass scrub, paired `RDTSC` `dudect` check, 1-bit parity rejection |
| **Sanitizers & Fuzzing (CI)** | `tests/fuzz_ozos_parsers.c`, `.github/workflows/ci.yml` | Clang ASan + UBSan across native C99 suites + 15s `libFuzzer` campaign on packet, vault archive, BMP, BIP-39, LoRa, `ML-KEM-768`, and `ML-DSA-65` parsers |
| **Mutation Testing (CI)** | `mull-runner-19` & `tests/mutation_test.py` | Official `Mull-19` (`0.37.0`) LLVM IR mutation analysis + empirical mutant-kill harness |
| **Physical Modems** | `tests/test_omni_phy`, `ozos_ghost test-loopback` | 16-FSK Goertzel wake-on-sound, 64-FFT OFDM, 32x32 BMP glyphs + RS(255,223) FEC |
| **Red-Team Regressions** | `python tests/test_redteam_fixes.py` | Strict CLI parsing, anti-downgrade A1/A2, key-pinning/TOFU, nonce monotonicity |
| **WASM Supply-Chain Pin** | `python contrib/dev-scripts/check_wasm_staleness.py --fail-on-drift` | Verifies `zero_dct.wasm` SHA-256 `ea26872f8c14523bb3d823c4eca16dcae96c6a11f1be24bea75081c75b5f7a4e` |

---

## Directory Structure

```text
OZOS/
├── .gitignore
├── LICENSE
├── README.md
├── Makefile                         # Linux / macOS build & test rules
├── build.bat                        # Windows MSVC / Clang / GCC build & test script
├── build_termux.sh                  # Android Termux native ARM64/ARMv7 build script
│
├── ports/
│   ├── baremetal_uefi/              # Standalone Ring-0 x86_64 UEFI kernel (BOOTX64.EFI)
│   └── sel4/                        # seL4 Microkit protection domain architecture
│
├── core/                            # Zero-heap C99 / C++20 cryptographic & PHY sources
│   ├── tweetnacl.c / .h             # TweetNaCl (Ed25519, X25519, XSalsa20-Poly1305, SHA-512)
│   ├── libozos.h                    # OZOS-80 struct alignment, MAC16, 2D-DCT primitives
│   ├── ozos_main.c                  # Unified CLI (ozos)
│   ├── ozos_ghost.c / .h            # Air-gap acoustic daemon & locked enclave runtime
│   ├── ozos_core_main.c             # Minimal stack & Shamir SSS binary (ozos-core)
│   ├── ozos_phy_main.c              # Physical transport CLI (ozos-phy)
│   ├── ozos_guard_main.c            # Host guard, Merkle anchor, sealed vault (ozos-guard)
│   ├── ozos_pqc_main.c / _hybrid.h  # X25519 + NIST FIPS 203 ML-KEM-768 & FIPS 204 ML-DSA-65 (`ozos_mldsa65.h`)
│   ├── ozos_torture.h               # Locked wipe, RDTSC dudect timing & tamper-sim suite
│   ├── ozos_cosmic.h                # Software fault-injection & invariant test suite
│   ├── ozos_brainseed.h / bip39_words.h
│   ├── ozos_crypto.h, ozos_gossip.h, ozos_omni_phy.h, ozos_shamir.h, ...
│   └── dct.cpp, integrity.cpp, transport.cpp, zero_dct.wasm
│
├── docs/
│   ├── RFC-OZOS-0080.md             # Canonical wire protocol & cryptographic specification
│   └── OPSEC_TACTICAL_MANUAL.md     # Operational security & air-gap deployment manual
│
├── tests/                           # Native C, Python Red-Team, Mutation & JS/WASM tests
└── contrib/                         # Web Cockpit demo & dev scripts
```

---

## License

Released under the [MIT License](LICENSE).  
Copyright (c) 2026 LumenNull.
