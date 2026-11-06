# Show HN & Global Public Announcement Brief

This document contains the ready-to-publish launch manuscripts for announcing O.Z.O.S across major technical and cryptographic platforms: **Hacker News ("Show HN")**, **Reddit (`r/crypto`, `r/netsec`, `r/embedded`)**, and **X / Twitter**.

---

## 1. Hacker News ("Show HN")

**Suggested Submission Title:**
> `Show HN: OZOS – Zero-Alloc (malloc=0) C99 Sovereign Enclave with Omni-Spectrum PHY & Post-Quantum Hybrid`

**Submission URL:**
> `https://github.com/LumenNull/OZOS`

**First Comment (The Context & Technical Deep-Dive):**
```markdown
Hi HN,

I spent the past year designing and refining O.Z.O.S (Omni-Zero Operating Shield), an open-source, sovereign cryptographic micro-enclave written entirely in pure C99 without a single dynamic memory allocation (`malloc = 0`).

### The Core Problem
Modern cold-storage setups and software cryptographic vaults carry substantial attack surfaces:
1. Dynamic heap allocators introduce fragmentation, heap-spraying vulnerabilities, and unpredictable timing side-channels.
2. Relying exclusively on Wi-Fi/Bluetooth or USB connections leaves systems vulnerable to RF eavesdropping and malicious USB firmware implants (BadUSB).
3. The advent of cryptanalytically relevant quantum computers (CRQC) threatens traditional discrete-log and elliptic-curve cryptography (RSA, ECDSA, Ed25519).
4. Single-point-of-failure seed phrases ("lose your 12 words and lose everything") create immense custodial anxiety.

### Architectural Principles of O.Z.O.S:
* **Zero Dynamic Allocation (`malloc = 0`):** The entire system runs deterministically on bounded stack frames. Bounded memory consumption guarantees hard real-time execution, making it natively deployable on seL4 Microkit and bare-metal STM32/Cortex-M4 microcontrollers.
* **Hybrid Post-Quantum Enclave:** Combines classical Ed25519 with NIST FIPS 203 (ML-KEM-768 / Kyber) and NIST FIPS 204 (ML-DSA-65 / Dilithium). High-speed microsecond signature verification with post-quantum security.
* **Omni-Spectrum Physical Layer (PHY):** An air-gapped data transmission system that operates without radio sockets or open network ports:
  - Near-ultrasound acoustic modulation: 16-FSK (18–20 kHz) and 64-FFT OFDM (< 36.6 ms pulse duration) with Reed-Solomon RS(96,80) forward error correction.
  - Optical High-Density Glyphs: 32x32 luminance matrix via camera/screen for zero-acoustic environments.
  - Sub-GHz LoRa modulation: Long-range physical telemetry (5–15 km) when acoustic waves cannot penetrate physical barriers.
* **Shamir's Secret Sharing (3-of-5 threshold):** Mathematically decomposes the 256-bit sovereign seed across Galois Field GF(2^8) into 5 distinct polynomial shares. Any 3 shares reconstruct the seed; any 2 shares yield mathematically zero information.
* **Ephemeral RAM & Active Anti-Forensics:** Master keys reside exclusively in pinned volatile RAM (`VirtualLock`/`mlock`). If tampered with, or upon catching `SIGSEGV`/`SIGABRT`, a 3-pass NIST SP 800-88 / DoD 5220.22-M zeroization cascade (`ozos_burn`) executes in < 15 microseconds.

### Decoupled Monorepo Architecture:
O.Z.O.S is structured into 4 independent, zero-dependency packages:
1. `ozos-core`: Minimal sovereign cryptographic enclave (`malloc = 0`, zero host hooks).
2. `ozos-phy`: Omni-spectrum acoustic, optical, and LoRa physical modem.
3. `ozos-guard`: Workstation hardening, anti-tamper, hypervisor detection, and NIST memory wipe.
4. `ozos-pqc`: Standalone NIST FIPS 203/204 Post-Quantum hybrid engine.

The codebase is C99, compiled with MSVC/GCC under AddressSanitizer and full static code analysis.

Repository: https://github.com/LumenNull/OZOS  
Field Manual: https://github.com/LumenNull/OZOS/blob/main/docs/OPSEC_TACTICAL_MANUAL.md

I would love feedback from the HN community—particularly on constant-time polynomial evaluations, timing side-channels in our Galois field arithmetic, and acoustic channel impulse responses under high reverberation.

Happy to answer any questions!
```

---

## 2. Reddit (`r/crypto`, `r/netsec`, `r/embedded`)

**Post Title:**
> `[Open Source] O.Z.O.S: A Zero-Heap (malloc=0) C99 Cryptographic Enclave with Omni-Spectrum PHY (Acoustic/Optical/LoRa) & NIST Post-Quantum Hybrid`

**Body Text:**
```markdown
Hey everyone,

I'm releasing O.Z.O.S (Omni-Zero Operating Shield), an experimental cryptographic micro-kernel and omni-spectrum physical transceiver developed for high-adversity, air-gapped environments.

Key highlights:
- **Zero Heap Invariant:** Strict `malloc = 0`. All cryptographic operations, FFT buffers, and Reed-Solomon polynomial structures are allocated on deterministic bounded stack frames. Compatible with seL4 Microkit and bare-metal ARM Cortex-M4.
- **Hybrid Post-Quantum Cryptography:** Integrates NIST FIPS 203 ML-KEM-768 and NIST FIPS 204 ML-DSA-65 alongside classical Ed25519.
- **Omni-Spectrum Air-Gap PHY:** Transmits binary wire formats (640-bit OZOS-80) over near-ultrasound (18–20 kHz 16-FSK & 64-FFT OFDM), 32x32 Optical Glyphs, and Sub-GHz LoRa without opening network sockets or TCP/UDP listeners.
- **Shamir's Secret Sharing (3-of-5):** Decomposes seeds across $GF(2^8)$ to eliminate single-point-of-failure vulnerabilities.
- **RAM Sanitization:** Ephemeral keys locked with `VirtualLock`/`mlock` and zeroized via 3-pass NIST SP 800-88 upon exit or signal faults.

GitHub Repo: https://github.com/LumenNull/OZOS  
License: MIT  

We welcome independent security researchers to inspect our constant-time execution paths, fuzz our acoustic demodulator, and review our formal specifications in the repository.
```

---

## 3. X / Twitter (Thread Pitch)

**Tweet 1 (Hook):**
> Introducing O.Z.O.S: A sovereign, zero-alloc (`malloc = 0`) C99 cryptographic enclave designed for true air-gapped defense.
> 
> 🔏 NIST FIPS 203/204 Post-Quantum Hybrid
> 🔇 18–20 kHz Near-Ultrasound Acoustic PHY
> 🧩 Shamir Secret Sharing (3-of-5)
> 🛡️ NIST SP 800-88 Ephemeral RAM Wipe
> 
> Open-source under MIT 🧵👇

**Tweet 2 (Engineering):**
> Why zero-heap?
> Modern crypto vaults are vulnerable to heap fragmentation and heap-spraying exploits.
> 
> O.Z.O.S enforces a hard `malloc = 0` invariant: 100% deterministic stack footprint, zero OS hooks, ready for seL4 Microkit & bare-metal ARM Cortex-M4.

**Tweet 3 (Air-Gap PHY):**
> Zero network sockets. Zero TCP/UDP listeners.
> 
> Data moves over an Omni-Spectrum PHY:
> • Inaudible near-ultrasound (16-FSK & 64-FFT OFDM, <36.6ms pulses)
> • 32x32 Optical Glyphs
> • Sub-GHz LoRa (5–15 km range)
> 
> Protected with Reed-Solomon RS(96,80) FEC.

**Tweet 4 (Call for Cryptanalysis):**
> Codebase is decoupled into 4 standalone packages: `ozos-core`, `ozos-phy`, `ozos-guard`, `ozos-pqc`.
> 
> All commits cryptographically signed via Ed25519.
> 
> We invite researchers & cryptographers to audit and red-team:
> 🔗 https://github.com/LumenNull/OZOS
