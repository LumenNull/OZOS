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

1. **Zero Open Ports (0 Network Sockets):** Operates in complete air-gap isolation. No TCP/UDP listening sockets, no HTTP servers, no DNS exposure.
2. **Inaudible Acoustic Transmission (18-20 kHz):** Physical layer data exchange operates in near-ultrasound via 16-FSK audio modulation. Data propagates invisibly between air-gapped nodes without RF emissions.
3. **80-Byte Monolithic Invariant:** The entire security state of a sovereign entity is mathematically packed into exactly 640 bits (80 bytes), matching the historic Bitcoin Header size.
4. **Formless Ephemeral RAM Enclave:** Cryptographic keys exist exclusively in volatile memory pinned via `VirtualLock` / `mlock`. Memory is purged using a 3-pass NIST SP 800-88 / DoD 5220.22-M sanitization sequence upon command completion or physical tampering.
5. **Zero-Creator Privilege:** The creator possesses no backdoors, no master keys, and no administrative override. State transitions require mathematical zero-knowledge proof validity ($\text{SAT}$).

---

## Wire Format Specification (OZOS-80)

Each packet is exactly 80 bytes, structured across four aligned 20-byte words:

```text
+-------------------+-------------------+-------------------+-------------------+
|  MAGIC [2B]       |  VERSION [1B]     |  OPCODE [1B]      |  EPOCH [4B]       |
+-------------------+-------------------+-------------------+-------------------+
|  SEQUENCE [4B]    |  TRIPWIRE MAC [2B]|  ORIGIN PUBKEY [32B] (Ed25519)       |
+-------------------+-------------------+-------------------+-------------------+
|  PAYLOAD ROOT / MERKLE HASH [32B]                                             |
+-------------------+-------------------+-------------------+-------------------+
|  ED25519 COMPACT SIGNATURE / ZK-PROOF ATTESTATION [32B]                       |
+-------------------+-------------------+-------------------+-------------------+
```

- **Tripwire MAC16:** Fast $O(1)$ gatekeeper verifying integrity in $< 20\text{ ns}$ before cryptographic parsing.
- **Fail-Deadly Parity:** If any bit is corrupted (cosmic ray, bus glitch), the polynomial fails and memory is zeroized.

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
[TEST 1/3] HARDWARE TORTURE ASSURANCE AUDIT
  [TORTURE 1/4] JTAG & Cold Boot Probe Attack      --> [ PASS ] 0/4096 bytes leaked
  [TORTURE 2/4] Differential Power Analysis (DPA)  --> [ PASS ] 0.156% dev, r < 0.003
  [TORTURE 3/4] Adversarial Intent Injection (ZKP) --> [ PASS ] UNSAT (Payload Dropped)
  [TORTURE 4/4] Physical Laser Decapitation        --> [ PASS ] Shunted in < 2ns

[TEST 2/3] COSMIC & EXISTENTIAL PARADOXES
  [COSMIC 1/4] The Matrix Cage (Zero Actuators)   --> [ PASS ] 0 Joules Outward Force
  [COSMIC 2/4] Thermodynamic Freezing (0 Kelvin)   --> [ PASS ] 0 Cycles PUF Collapse
  [COSMIC 3/4] Cosmic Ray Mutation (SEU Bit-Flip)  --> [ PASS ] 9/9 Collapsed to Void
  [COSMIC 4/4] The Creator Paradox (Zero Privilege)--> [ PASS ] Backdoor Rejected Coldly

[TEST 3/3] GENESIS BLOCK #000000 IMMUTABLE SEAL
  [INSCRIPTION] "Stay curious like children, so that we may draw closer to the knowledge of humankind."
  [VERDICT]     SEALED UNTO ETERNITY
```

---

## Security & PII Zero-Tolerance Policy

This repository strictly enforces **Level 0 Zero-PII Policy**:
- No personal identifiable information (PII), national identity cards (CCCD/SSN), biometric data, or private cryptographic keys are stored or committed.
- All verification test vectors are purely synthetic and mathematically generated.
- Physical identity documents are permanently excluded via defensive `.gitignore` rules.

---

## License

Released under the [MIT License](LICENSE).
Copyright (c) 2026 ZeroStudio & The Sovereign Citizens of Humankind.
