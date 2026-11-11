# O.Z.O.S on seL4 Microkernel (seL4 Microkit Port Foundation)

## 1. Executive Architecture Summary

This directory establishes the formal architecture and porting foundation for deploying the **OZOS Sovereign Invariant Enclave** onto the **seL4 microkernel** via the modern **seL4 Microkit** (formerly known as Microkit / Core Platform).

seL4 is the world's highest-assurance operating system kernel, featuring comprehensive machine-checked mathematical proofs in Isabelle/HOL verifying:
1. **Functional Correctness:** The executable binary behaves in strict accordance with the abstract formal specification.
2. **Integrity:** Unauthorized modification of memory and capabilities is mathematically impossible.
3. **Confidentiality:** Information flow is strictly governed by capability distributions, preventing covert unauthorized reads.
4. **Safety Invariants:** Proven freedom from buffer overflows, null pointer dereferences, dangling pointers, memory leaks, and undefined execution states.

OZOS was designed from inception with the **seL4-Spirit**:
- Zero dynamic heap allocation (`malloc = 0`).
- Deterministic, bounded stack execution.
- Fixed 80-byte SIMD-aligned wire frames.
- Constant-time cryptographic primitives.

This architecture enables seamless migration from host-emulated stack enclaves (Windows `VirtualLock`, Linux `mlock`) directly into hardware-enforced, capability-isolated seL4 Protection Domains (PDs).

---

## 2. Microkit Multi-Domain Topology (`ozos.system`)

The OZOS seL4 deployment decomposes the sovereign stack into four isolated Protection Domains (PDs) communicating exclusively via hardware-enforced protected capabilities and shared memory rings:

```
+-----------------------------------------------------------------------+
|                             seL4 KERNEL                               |
|            (Formally Verified Isabelle/HOL Microkernel Core)          |
+-----------------------------------------------------------------------+
        |                                       |
        v                                       v
+-----------------------+               +-----------------------+
|      airgap_rx        |               |      airgap_tx        |
|  (Acoustic / Optical  |               |  (Acoustic / Optical  |
|   Demodulation Ingest)|               |   Modulation Egress)  |
+-----------------------+               +-----------------------+
        |                                       ^
        | [Protected Channel: pp_rx_enclave]    | [Protected Channel: pp_enclave_tx]
        v                                       |
+---------------------------------------------------------------+
|                        crypto_enclave                         |
|  - Ed25519 Sovereign Private Key (Stack locked)               |
|  - Shamir's Secret Sharing (SSS) GF(2^8) Secret Reconstruction|
|  - Poly1305 / XSalsa20 Sovereign Cryptographic Core           |
|  - Zero Device Access, Zero Network Capabilities              |
+---------------------------------------------------------------+
                                |
                                v [Protected Channel: pp_enclave_storage]
+---------------------------------------------------------------+
|                          storage_vfs                          |
|  - Monotonic Counter Registers (TPM 2.0 / Hardware NVRAM)     |
|  - Anti-Rollback / Anti-Replay State Persistence              |
|  - NIST SP 800-88 Cryptographic Shredding Engine              |
+---------------------------------------------------------------+
```

---

## 3. Protection Domain Invariants

| Protection Domain | Memory Budget | Capabilities Granted | Security Restrictions |
| :--- | :--- | :--- | :--- |
| `crypto_enclave` | 64 KiB Stack, 0 KiB Heap | Receive requests from `airgap_rx`; transmit responses to `airgap_tx`; call `storage_vfs` | No hardware device I/O; no physical memory mappings; no external interrupt lines |
| `airgap_rx` | 128 KiB Buffer | Hardware audio ADC / Camera DMA buffers; notify `crypto_enclave` | Cannot access cryptographic master seeds or private keys |
| `airgap_tx` | 128 KiB Buffer | Hardware audio DAC / Display DMA buffers; notified by `crypto_enclave` | Cannot initiate crypto operations; egress-only transmission |
| `storage_vfs` | 64 KiB Stack | Hardware TPM 2.0 SPI/I2C or eMMC RPMB controller | Access limited exclusively to authenticated enclave requests |

---

## 4. Hardware Verification & Monotonic Counter Guarantee

Running OZOS on seL4 definitively resolves the host virtualization vulnerability:
- On untrusted hosts (Windows/Linux VMs), hypervisor snapshot rollback can reset process RAM counters.
- Under seL4, monotonic sequence numbers are bound to hardware non-volatile registers via `storage_vfs` capabilities, guaranteeing mathematical immunity against replay and time-travel attacks.

---

## 5. Build & Target Roadmap

- **Target Architectures:** ARM64 (Cortex-A53 / A72 / Raspberry Pi 4), RISC-V 64 (SiFive HiFive Unleashed / VisionFive 2), and x86_64.
- **Microkit Toolchain:** Uses the official seL4 Microkit SDK (`microkit.py`) to synthesize the capability distribution graph into a bootable bootimage (`loader.img`).
