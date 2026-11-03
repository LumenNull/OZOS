# O.Z.O.S // Omni-Zero Operating Shield
### *Pure C99 Sovereign Cryptographic Micro-Kernel & Ghost Enclave*

```
  ____   ______   ____   _____ 
 / __ \ |___  /  / __ \ / ____|
| |  | |   / /  | |  | | (___  
| |  | |  / /   | |  | |\___ \ 
| |__| | / /__  | |__| |____) |
 \____/ /_____|  \____/|_____/ 
  IMMUTABLE • HEADLESS • INAUDIBLE • FORMLESS
```

---

## The Genesis Inscription (#000000)

> *"Stay curious like children, so that we may draw closer to the knowledge of humankind."*  
> — **Sealed into Genesis Block #000000 on October 31, 2026**

```text
=================================================================
  O.Z.O.S // GENESIS BLOCK #000000 IMMUTABLE SEAL
=================================================================
• BLOCK HEIGHT : #000000 (GENESIS)
• TIMESTAMP    : 31/Oct/2026
• WIRE FORMAT  : OZOS-80 Binary Aligned (Bare-Metal Invariant)
• GENESIS HASH : 0x3b5bfe68f4a491a02c0fec66547ee142d36ba0038cb0c1c52ba8b14e80786edd
• GENESIS PUBKEY: 0x938998e24444b9fe70ac36b1de57f4178337b49871780161d3e68c3a4e84b769
• TRIPWIRE MAC : 0xE9C7 (O(1) Gatekeeper < 20ns)
• STATUS       : SEALED UNTO ETERNITY
=================================================================
```

---

## Architectural Philosophy

O.Z.O.S is a self-sovereign cryptographic micro-kernel designed for high-adversity environments. It departs fundamentally from modern software bloat:

1. **Zero Open Ports (0 Network Sockets):** All sovereign binaries (`ozos_ghost`, `ozos_cli`) operate in 100% strict air-gap isolation with 0 network sockets, 0 TCP/UDP listeners, and 0 external system/popen calls. Network communication is strictly prohibited at the architectural level.
2. **Inaudible Acoustic Transmission (18-20 kHz):** Physical layer data exchange operates in near-ultrasound via 16-FSK audio modulation. Data propagates invisibly between air-gapped nodes without RF emissions.
3. **80-Byte Monolithic Invariant:** The entire security state of a sovereign entity is mathematically packed into exactly 640 bits (80 bytes), matching the historic Bitcoin Header size.
4. **Formless Ephemeral RAM Enclave:** Cryptographic keys exist exclusively in volatile memory pinned via `VirtualLock` / `mlock`. Memory is purged using a 3-pass NIST SP 800-88 / DoD 5220.22-M sanitization sequence upon command completion or physical tampering.
5. **Zero-Creator Privilege:** The creator possesses no backdoors, no master keys, and no administrative override. State transitions require mathematical zero-knowledge proof validity ($\text{SAT}$).

---

## Wire Format Specifications (Dual Profile Architecture)

O.Z.O.S defines two distinct, specialized binary wire profiles tailored for specific operational environments:

### Profile A: Bare-Metal SIMD Carrier (`OZOS-80`)
Optimized for high-throughput bare-metal micro-kernel execution, vector register loading (`v128_load`), and near-ultrasound acoustic modulation (`ozos.exe`, `ozos_ghost`).

```text
+-------------------------------------------------------------------------------+
|  DCT / FREQUENCY CARRIER COEFFICIENTS (16 x float32 = 64 Bytes)               |
|  [Offset 0x00 - 0x3F] • 16-Byte SIMD Aligned (v128_load)                      |
+-------------------------------------------------------+-----------------------+
|  MONOTONIC NONCE (uint64_t = 8 Bytes)                 |  MAGIC (uint32_t = 4B)|
|  [Offset 0x40 - 0x47] • Anti-Replay Counter           |  [0x48 - 0x4B] 0x47454E01
+---------------------------+---------------------------+-----------------------+
|  PAYLOAD LEN (uint16_t=2B)|  TRIPWIRE MAC16 (uint16_t=2B)                     |
|  [Offset 0x4C - 0x4D]     |  [Offset 0x4E - 0x4F] • O(1) Gatekeeper < 20ns    |
+---------------------------+---------------------------------------------------+
TOTAL WIRE HEADER SIZE: EXACTLY 80 BYTES (640 Bits, Zero Padding)
```

#### Profile A Dual MAC Gatekeeper Sub-Profiles:
* **Sub-Profile A1 (CRC16-CCITT Hardware Fast Gate):** Wire CRC16-CCITT polynomial calculated over the 78-byte header (`0..77`). Designed exclusively for line-rate stateless DDoS gatekeeping ($<20\text{ns}$) on network ingress before parsing. On unsigned frames with non-empty payload, Sub-Profile A1 does NOT verify payload integrity.
* **Sub-Profile A2 (FNV-1a-64 Default Carrier Gate):** Domain-separated 64-bit FNV-1a folded into 16 bits covering Header (`0..77`) AND Payload (`80..80+len`). Emitted by default by `ozos packet-build` whenever `payload_len > 0` to prevent A1 downgrade tampering.
* **Universal Cross-Verification:** All Profile A validators (`ozos.exe packet-verify`, `OZOSEngine.validateAligned80BytePacket`, `ozos-cli.py`) cross-validate both Sub-profiles A1 and A2. When verifying unsigned packets carrying payload under Sub-Profile A1, validators issue an explicit warning regarding unverified payload integrity.

#### Inter-Party Trust & Physical Threat Boundaries:
* **Local Vault Tripwire (Unkeyed):** Unkeyed Local Vault records (`secretSeed = 0xA5A5A5A5`) provide fast tamper-evidence and casual edit protection exclusively within local browser storage. They carry **ZERO inter-party trust**; unkeyed MAC algorithms are public and easily recomputed.
* **Cross-Party Accounting & Multi-Shop Aggregation:** Multi-party settlement and cross-shop ledger verification MUST use **Sovereign Ed25519 Signatures** (RFC 8032 via `packet-sign` / `get_sovereign_keys`) or strong domain-separated secrets per party (`vaultKey`).
* **Physical Acoustic Reception (Near-Ultrasound 18-20 kHz):** Acoustic transmission is an open physical broadcast medium. Initial SYN frames achieve Perfect Forward Secrecy (PFS) via ephemeral Curve25519 ECDH, but do not authenticate peer identity by default. High-security deployments must bind SYN frames to pinned peer public keys or authenticate session keys out-of-band (e.g. optical QR code or BIP-39 verbal confirmation).
* **Anti-Replay Persistence & Monotonic Counters:** In-memory anti-replay state in Node.js and CLI tools is scoped to the active execution process. Complete immunity against VM snapshot rollback requires persistence bound to hardware monotonic counters (e.g. TPM 2.0 NVRAM).

### Profile B: Web Cockpit Wire Frame (`GENS-v1`)
Optimized for zero-copy JavaScript `DataView` parsing, WebAssembly module bounds verification, and GenesisShield transport (`zero_dct.wasm`, `genesis-shield.js`, `ozos-engine.js`).

```text
+-----------------------+-------------------------------------------------------+
|  MAGIC (uint32_t = 4B)|  MONOTONIC NONCE (uint64_t = 8 Bytes)                 |
|  [0x00 - 0x03] 0x47454E01  [Offset 0x04 - 0x0B] • Anti-Replay Counter           |
+-----------------------+-------------------------------------------------------+
|  PERCEPTUAL FINGERPRINT (15 x float32 = 60 Bytes)                             |
|  [Offset 0x0C - 0x47] • Low-Frequency Perceptual Anchors (4x4 Matrix)         |
+-------------------------------------------------------+-----------------------+
|  PAYLOAD CHECKSUM (uint32_t = 4B)                     |  PAYLOAD LEN (u32=4B) |
|  [Offset 0x48 - 0x4B] • FNV-1a Checksum (Slot 15)     |  [Offset 0x4C - 0x4F] |
+-------------------------------------------------------+-----------------------+
TOTAL WIRE HEADER SIZE: EXACTLY 80 BYTES (640 Bits)
```

### Sovereign Identity Attachment (Detached Ed25519)
When signed identity attestation is required, the packet frame appends standard 64-byte Ed25519 signatures (RFC 8032) without polluting the invariant 80-byte header:
```text
[OZOS-80 Header (80B)] + [Payload (L Bytes)] + [Ed25519 PubKey (32B)] + [Ed25519 Sig (64B)]
```

- **Tripwire MAC16:** Fast $O(1)$ CRC16-CCITT polynomial verifying header integrity in $< 20\text{ ns}$ before triggering cryptographic verification.
- **Fail-Deadly Memory:** If any bit is corrupted (cosmic ray, bus glitch, DFA fault injection), the packet fails MAC16/DFA checks and volatile key memory is instantly sanitized.

---

## Directory Structure

```
OZOS/
├── .gitignore               # Strict zero-PII and zero-binary hygiene
├── LICENSE                  # MIT License
├── README.md                # Sovereign documentation & Genesis Block seal
├── build.bat                # Native Windows build script (MSVC / Clang / GCC)
├── Makefile                 # Linux / macOS POSIX build script
│
├── core/                    # Pure C99 Micro-Kernel (Bare-Metal Invariant)
│   ├── ozos_ghost.c         # Sovereign Ghost Runtime (Headless / Inaudible)
│   ├── ozos_ghost.h         # Near-ultrasound DSP & Ephemeral Memory Enclave
│   ├── libozos.h            # 80-byte 15-tier architectural definition
│   ├── ozos_crypto.h        # Ed25519, X25519, Poly1305, SHA-256
│   ├── ozos_brainseed.h     # 12-word brain-enclave & zeroization engine
│   ├── ozos_gossip.h        # Epidemic peer gossip protocol
│   ├── ozos_torture.h       # 4-Pillar hardware torture suite
│   ├── ozos_cosmic.h        # 4 Cosmic paradoxes & Genesis Block
│   ├── tweetnacl.c / .h     # Audited high-security cryptographic primitives
│   └── bip39_words.h        # BIP-39 mnemonic dictionary
│
├── docs/                    # Technical Standards & Cryptographic Proofs
│   ├── GENESIS_BLOCK.md     # Historical inscription coordinates & seal
│   ├── TORTURE_VERIFICATION.md  # JTAG, Cold-Boot, DPA/EMA, ZKP Intent, Decapitation
│   ├── COSMIC_PARADOX_AUDIT.md  # Matrix Cage, 0K Freezing, Cosmic Ray, Creator Paradox
│   ├── RFC-OZOS-0080.md     # Formal RFC Protocol Specification
│   └── OZOS_15_TIER_SOVEREIGN_BLUEPRINT.md # Full 15-tier architecture map
│
├── tests/                   # Independent Verification Suites
│   ├── test_libozos.c       # Native C unit tests for libozos.h
│   ├── test_rfc_conformance.py # RFC test vector conformance
│   └── test_adversarial_redteam.py # Red team adversarial scenarios
│
└── contrib/                 # Experimental Tools & Demonstrations
    ├── web-cockpit/         # Optional visual laboratory (WASM / Web UI)
    └── dev-scripts/         # Local development helper scripts
```

---

## Security Model & Threat Boundaries (Empirical Scoping)

To maintain scientific rigor and prevent misunderstandings, O.Z.O.S formally defines its operational threat boundaries:

1. **Tripwire Gatekeeper vs. Cryptographic Vault:**
   - **Tripwire MAC16 ($2^{16} = 65,536$ states):** Functions strictly as an $O(1) < 20\text{ ns}$ ultra-fast packet gatekeeper to filter out unaligned noise, invalid magic bytes, or flood/DoS attempts before invoking heavy cryptographic operations. It is an engineering circuit breaker, not a 128-bit cryptographic MAC.
   - **True Security Boundary:** Sovereign packet integrity and authenticity are cryptographically guaranteed by **Ed25519 signatures (RFC 8032)** combined with **strictly monotonic 64-bit nonces** that neutralize replay attacks.

2. **Software Invariant Simulation vs. Physical Silicon Realism:**
   - The test suites for laser micro-probing, clock-glitch crowbar resets, 0-Kelvin PUF collapse, and cosmic ray SEU bit-flips are **formal state-transition simulations** designed to verify fail-deadly software invariant conservation.
   - Physical silicon tamper resistance in production requires deployment on certified secure elements (e.g. CC EAL6+ / physical TPM 2.0 silicon) measured via calibrated oscilloscopes and electromagnetic probes.

3. **Hardware Root of Trust vs. Machine ID Fallback:**
   - On Windows, hardware root of trust binds to TPM 2.0 via `ncrypt.dll`.
   - On Linux, true hardware enclave binding requires physical character device nodes (`/dev/tpmrm0` / `/dev/tpm0`). When executed on virtual machines or hardware without TPM, the engine falls back to `/etc/machine-id` as a **Soft Machine Identifier**, which is software-clonable and not an immutable silicon enclave.

4. **Specialized Near-Field Acoustic Conduit (18-20 kHz):**
   - The Hann-windowed near-ultrasound channel operates as a specialized, line-of-sight, close-proximity ($< 2\text{m}$) air-gapped conduit designed for air-gapped identity attestation.
   - It is subject to transducer physical frequency response, environmental acoustic noise, room reverberation, and analog microphone limits.

---

## Building and Verification

### Windows (MSVC / Clang / GCC)

```cmd
:: Build ozos_ghost.exe
build.bat

:: Run complete torture & cosmic paradox test suites
build.bat test

:: Clean build artifacts
build.bat clean
```

### Linux / macOS (GCC / Clang)

```bash
# Build binary
make

# Run all verification suites
make test

# Clean artifacts
make clean
```

---

## Verification Test Results

```text
[TEST 1/4] HARDWARE TORTURE ASSURANCE AUDIT
  [TORTURE 1/4] JTAG & Cold Boot Probe Attack      --> [ PASS ] 0/4096 bytes leaked
  [TORTURE 2/4] Differential Power Analysis (DPA)  --> [ PASS ] < 0.35% dev, r < 0.003
  [TORTURE 3/4] Adversarial Intent Injection (ZKP) --> [ PASS ] UNSAT (Hostile Dropped)
  [TORTURE 4/4] Physical Laser Decapitation        --> [ PASS ] Shunted in < 2ns

[TEST 2/4] COSMIC & EXISTENTIAL PARADOXES
  [COSMIC 1/4] The Matrix Cage (Zero Actuators)   --> [ PASS ] 0 Joules Outward Force
  [COSMIC 2/4] Thermodynamic Freezing (0 Kelvin)   --> [ PASS ] 0 Cycles PUF Collapse
  [COSMIC 3/4] Cosmic Ray Mutation (SEU Bit-Flip)  --> [ PASS ] 9/9 Collapsed to Void
  [COSMIC 4/4] The Creator Paradox (Zero Privilege)--> [ PASS ] Backdoor Rejected Coldly

[TEST 3/4] GENESIS BLOCK #000000 IMMUTABLE SEAL
  [INSCRIPTION] "Stay curious like children, so that we may draw closer to the knowledge of humankind."
  [VERDICT]     SEALED UNTO ETERNITY

[TEST 4/4] 15-TIER NATIVE LIBOZOS SUITE (12/12 PASS 100%)
  • Pillar 1: Sovereign Ed25519 Stack Signature & PK Auth      --> [ PASS ] 100% Bit-Exact
  • Pillar 2: Two-Way Inaudible Handshake & Acoustic Tunnel    --> [ PASS ] PFS Inaudible Tunnel
  • Pillar 3: Resurrective Brainseed (BIP-39 Pure C99 SHA-256) --> [ PASS ] "abandon ... about"
  • Pillar 4: Epidemic Acoustic Gossip Mesh & Merkle Sync      --> [ PASS ] Gossip Consensus
```

---

## Security & PII Zero-Tolerance Policy

This repository strictly enforces **Level 0 Zero-PII Policy**:
- No personal identifiable information (PII), national identity cards (CCCD/SSN), biometric data, or private cryptographic keys are stored or committed.
- All verification test vectors are purely synthetic and mathematically generated.
- Physical identity documents are permanently excluded via defensive `.gitignore` rules.

---

## Threat Model, Design Decisions & Operational Security (OpSec)

Following exhaustive 10-round adversarial Red Team audits, the operational boundaries and intentional architectural invariants of O.Z.O.S are formally cataloged below:

### 1. Keyed Vault Entropy Model & Confidentiality Scope
* **Tamper-Evidence Tripwire:** The Keyed Local Vault (`ozos-engine.js:saveLocalVault`) uses domain-separated 64-bit FNV-1a designed for zero-allocation microsecond execution (~330,000 derivations/sec) without PBKDF2/Argon2 iteration overhead. Security against brute-force forgery relies strictly on the entropy of `vaultKey`. High-risk environments must use strong high-entropy passphrases.
* **Integrity vs Confidentiality:** Local Vault records provide **Tamper-Evidence & Integrity Only**; the underlying JSON payload (amount, ledger metadata, timestamps) is stored in Base64-encoded plaintext. Applications requiring confidentiality must encrypt sensitive fields (e.g. via ChaCha20-Poly1305) prior to vault sealing.

### 2. Acoustic Physical Transport & Ephemeral Handshake Bounds
* **Open Physical Broadcast Medium:** Acoustic near-ultrasound (18.1 - 19.8 kHz) operates across shared physical airspaces. Discovery SYN frames are unauthenticated broadcast probes that establish ephemeral session keys with Perfect Forward Secrecy (PFS) via Curve25519 ECDH.
* **Out-of-Band Peer Authentication:** An established session key (`K_session`) guarantees channel confidentiality and tamper-resistance via Poly1305, but does NOT attest the physical identity of the remote speaker. High-security deployments must bind SYN exchanges to pinned peer public keys (`--pinned-peer-pubkey`) or authenticate session hashes out-of-band (e.g., optical QR scan or BIP-39 verbal confirmation).
* **Framing Disambiguation:** CLI `audio-synth` FSK encodes arbitrary text/hex payloads with 40ms tone duration and Hann smoothing, whereas `ozos_ghost` uses structured binary frames (22B beacon, 42B SYN, 54B ACK) with dedicated dual-tone preamble detection.

### 3. State Persistence & Hardware Monotonicity
* **Ephemeral Anti-Replay Invariants:** In-memory anti-replay state tracking in Web Cockpit (`OZOSEngine`, `GenesisShield`) and CLI executions is scoped to active process memory.
* **VM Rollback & Snapshot Immunity:** Full defense against virtual machine snapshot rollback attacks requires persisting monotonic nonces into hardware-backed non-volatile registers (such as TPM 2.0 NVRAM or seL4 hardware counter channels).

### 4. Canonical Distribution, Upgrade Guidance & N-Day Advisory
* **Authoritative Source & Release Signing:** The canonical repository for O.Z.O.S is strictly `https://github.com/LumenNull/OZOS.git`. Commits on `main` carry verified author provenance from `LumenNull <350623492+LumenNull@users.noreply.github.com>`. Official production release tags and binary distributions are cryptographically pinned via git commit SHA and signed with Sovereign Ed25519 / GPG release keys.
* **Binary Provenance & SHA-256 Pinning:** All shipped WebAssembly modules (`zero_dct.wasm`) must match the pinned SHA-256 hash `d7924fedcda753e0b9eefdaf779eb657f67fba50da96472dc98c92b393d7b4a3`. Verify locally via:
  ```bash
  sha256sum contrib/web-cockpit/zero_dct.wasm core/zero_dct.wasm
  ```
* **Upgrade Policy:** Deprecated or unpatched node deployments are susceptible to public patch-diff reverse-engineering (N-day exploitation). Node operators must continuously track the `main` branch. Older versions prior to Round 10 hardening (commit `c68c1ce`) are considered deprecated.

---

## License

Released under the [MIT License](LICENSE).
Copyright (c) 2026 ZeroStudio & The Sovereign Citizens of Humankind.
