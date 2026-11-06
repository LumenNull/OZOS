# O.Z.O.S // OPERATIONAL SECURITY & TACTICAL FIELD MANUAL (OPSEC-RFC-2026)
**Document Identifier:** OZOS-DOC-OPSEC-01  
**Classification:** Sovereign Defensive Operational Doctrine  
**Author:** LumenNull & Sovereign Technical Working Group  
**Target Audience:** Field Operators, Cryptographic Engineers, Air-Gap Administrators  

---

## 1. Executive Doctrine & Operational Philosophy

The Omni-Zero Operating Shield (O.Z.O.S) operates under a zero-trust, hostile-environment doctrine:
1. **Assume Host OS Compromise:** Operating systems (Windows, Linux, macOS) are treated as hostile, surveillance-compromised environments prone to keylogging, RAM dumping, and virtualization traps.
2. **Assume Physical Airspace Eavesdropping:** Acoustic, optical, and electromagnetic spectra are treated as publicly monitored channels.
3. **Zero-Persistence Invariant:** Cryptographic identities must never reside statically on persistent disk storage. They exist exclusively as ephemeral, non-paged volatile RAM buffers or reconstituted from operator memory.

This manual outlines the **standard operating procedures (SOP)**, physical channel selection criteria, hypervisor countermeasures, and emergency breach sanitization protocols required to deploy O.Z.O.S safely in adversarial environments.

---

## 2. Tactical Spectrum Selection & Channel Transition Matrix

O.Z.O.S rejects reliance on any single physical medium. When physical noise, jamming, or attenuation degrades one modality, operators must immediately execute a spectrum transition across the 4-tier cognitive transport matrix (`ozos-phy.exe`).

```text
+---------------------------------------------------------------------------------------------------+
|                           COGNITIVE PHY SPECTRUM TRANSITION MATRIX                                |
+------------------+-------------------+--------------------+-------------------+-------------------+
| TIER             | MEDIUM            | OPERATIONAL RANGE  | LATENCY           | RESISTANCE        |
+------------------+-------------------+--------------------+-------------------+-------------------+
| Tier 1 (Optical) | Visible Light     | Line-of-Sight      | 16 ms (60fps)     | Sound Jammers     |
| Tier 2 (OFDM)    | Acoustic Ultra    | < 2.0 Meters       | < 36.6 ms         | Room Echoes       |
| Tier 3 (LoRa)    | Sub-GHz RF        | 5 - 15 Kilometers  | 100 - 300 ms      | Concrete / Walls  |
| Tier 4 (Skywave) | HF Ionosphere     | Transoceanic       | 2 - 5 Seconds     | Cable Cut / EMP   |
+------------------+-------------------+--------------------+-------------------+-------------------+
```

### 2.1 Acoustic Channel Limits & Multipath Hardening
* **Physical Boundary Disclosed:** Near-ultrasound (18.1 - 19.8 kHz) attenuates rapidly beyond 2.0 meters ($e^{-\alpha d}$). Furthermore, hard boundary reflections (tile walls, glass windows) cause multipath phase cancellation and destructive interference.
* **Tactical Rules for Acoustic Transport:**
  1. **Doppler & Movement:** Do not move either device during acoustic transmission. Relative motion induces frequency shifts outside the 32 Goertzel filter detection bins.
  2. **Room Reverberation Mitigation:** If room geometry causes echo interference, transition immediately from legacy 16-FSK to **Acoustic 64-FFT OFDM** (`ozos-phy ofdm-synth`). OFDM utilizes 16 simultaneous QPSK subcarriers and completes the entire 80-byte burst in $< 36.6\text{ ms}$—faster than typical room impulse decay times.
  3. **Burst Error Correction:** The integrated **Reed-Solomon RS(96, 80) Forward Error Correction (FEC)** automatically reconstructs payloads even if room flutter echoes wipe out up to 7 consecutive bytes.

### 2.2 Optical Glyph Spectrum Transition
* **Trigger Conditions:** High acoustic noise, presence of acoustic jammers, high room reverberation, or requirement for strict 0 dB acoustic silence.
* **Execution:**
  ```bash
  # Transmitting Node (Encode to BMP or Terminal Matrix)
  ozos-phy.exe glyph-encode <HEX_PAYLOAD> --bmp payload.bmp

  # Receiving Node (Extract from Camera Capture)
  ozos-phy.exe glyph-decode payload.bmp
  ```
* **Operational Spec:** 32x32 dynamic matrix, 16 milliseconds transfer latency per 60fps camera frame, verified with CRC32-IEEE checksums. Completely immune to microphones and acoustic jammers.

### 2.3 Long-Range LoRa Sub-GHz Bunker Penetration
* **Trigger Conditions:** Distance exceeds 2 meters, transmission must penetrate reinforced concrete walls, or physical line-of-sight is obstructed.
* **Execution:**
  ```bash
  # Encode Sovereign 84-byte Wire Frame
  ozos-phy.exe lora-pack <HEX_PAYLOAD>

  # Decode Incoming Wire Frame on Receiving Node
  ozos-phy.exe lora-unpack <168_CHAR_HEX_FRAME>
  ```
* **Operational Spec:** Operates on unlicensed 433 MHz or 868 MHz ISM bands. Wire frame structure: `[Sync Word 0x12 | Type 0x50 | 80B Payload | CRC16-CCITT]`. Range: 5 to 15 kilometers without internet, Wi-Fi, or cellular network infrastructure.

---

## 3. Virtualization (VM) Countermeasures & Identity Anti-Cloning

Running cryptographic sovereign nodes inside hosted virtual machines (VMware, VirtualBox, Hyper-V, KVM/QEMU) introduces critical OpSec vulnerabilities:
* **Hypervisor Snapshot Rollback:** Adversaries or cloud providers can snapshot VM state, observe cryptographic operations, and rollback state to replay old transactions.
* **VM Image Cloning:** Copying virtual disk images (`.vmdk`, `.qcow2`) creates duplicate rogue nodes sharing the exact same software hardware ID (`/etc/machine-id`).
* **Loss of Hardware Silicon Anchor:** VMs lack direct access to physical motherboard TPM 2.0 registers.

### 3.1 Pre-Flight Hypervisor Audit
Before initiating sensitive cryptographic sessions, execute `ozos-guard`:
```bash
ozos-guard.exe workstation-shield
```
* **Audit Inspection:**
  - CPUID leaf `0x40000000` queries hypervisor vendor signatures (`VMwareVMware`, `Microsoft Hv`, `KVMKVMKVM`, `XenVMMXenVMM`).
  - If virtualized, the system reports: `[ALERT] YES (RUNNING UNDER HYPERVISOR)`.
  - **Operational Rule:** On virtualized machines, operators must NEVER rely on local monotonic nonces or host hardware identifiers.

### 3.2 The Zero-Disk Ephemeral Protocol
* **Rule 1:** Never write private keys, sovereign seeds, or sensitive ledger states to VM virtual disk storage.
* **Rule 2:** Sovereign identities must exist only as **BIP-39 12-Word Brainseeds** committed to operator human memory:
  ```bash
  # Restore identity dynamically into non-paged RAM
  ozos-core.exe brainseed-restore "abandon abandon ... about"
  ```
* **Rule 3:** The runtime environment pins key material using `VirtualLock` (Windows) or `mlock` (POSIX) to prevent the hypervisor host from swapping sensitive pages to disk swapfiles (`pagefile.sys`).
* **Rule 4:** Upon session completion, `ozos_crypto_zeroize` immediately executes a 3-pass NIST SP 800-88 overwrite (`0xAA -> 0x55 -> 0x00`). Adversaries capturing a VM snapshot capture only zeroes.

### 3.3 Shamir's Secret Sharing (SSS) Multi-Node Dispersion
To deploy across virtualized cloud infrastructure safely without trusting any single cloud vendor:
1. Generate master root seed and split into a $(3, 5)$ threshold scheme:
   ```bash
   ozos-core.exe brainseed-split --threshold 3 --shares 5
   ```
2. Distribute the 5 generated shares across 5 independent virtual machines running under different cloud providers (e.g. AWS, GCP, Azure, Hetzner, Local Bare-Metal).
3. **Shannon Perfect Secrecy Invariant:** Even if an adversary clones, dumps, or seizes 1 or 2 virtual machines, they hold at most 2 shares. By polynomial interpolation over $\text{GF}(2^8)$, $k-1$ shares reveal **zero information** regarding the master sovereign seed.
4. **Reconstruction:** Only when 3 valid shares are aggregated in volatile memory does the node reconstruct authority:
   ```bash
   ozos-core.exe brainseed-combine <SHARE_1> <SHARE_2> <SHARE_3>
   ```

### 3.4 Bare-Metal Escape Target (`ozos-core` on seL4 / STM32)
For mission-critical sovereign deployments requiring zero virtualization vulnerability:
* Deploy the standalone **`ozos-core.exe`** module (`malloc = 0`, stack-only, 214 KB).
* Run `ozos-core` directly on bare-metal ARM Cortex-M4/M7 silicon (STM32) or compile within verified **seL4 Microkit** user partitions.
* By eliminating host operating systems and hypervisors, VM snapshot attacks, debugger dumps, and disk cloning are rendered structurally impossible.

---

## 4. Host OS Hardening & Anti-Forensic Procedures

### 4.1 Process Shield & DACL Anti-Dump Defense
Malware running on the same workstation (e.g., info-stealers, memory dumpers like Mimikatz or Process Hacker) attempts to open `PROCESS_VM_READ` handles.
* O.Z.O.S injects a restrictive Discretionary Access Control List (DACL) into its own process token via `ozos_harden_process_dacl()`.
* Strips all read/write/debug handles from non-SYSTEM callers.
* Enables `PROCESS_MITIGATION_DYNAMIC_CODE_POLICY` to block arbitrary shellcode injection.

### 4.2 Windows Clipboard RAM Interception
Adversaries often monitor the OS clipboard to capture copied QR codes or credentials.
```bash
ozos-guard.exe clipboard-armor
```
* Hooks Windows `CF_DIB` clipboard events in sub-5ms latency.
* Injects an invisible 2D-DCT watermark into the bitmap in volatile memory before malicious spyware can exfiltrate clean screen captures.

### 4.3 Anti-Forensic Hardware Sanitization (`burn`)
To eliminate compromised files permanently:
```bash
ozos-guard.exe burn <FILE_PATH>
```
* **Pass 1:** Overwrite entire physical allocation with `0xAA`. Flush to disk controller via `FlushFileBuffers`.
* **Pass 2:** Overwrite with inverted mask `0x55`. Flush to disk controller.
* **Pass 3:** Overwrite with cryptographic zero `0x00`. Flush to disk controller.
* **Unlink:** File is deleted and unlinked from the filesystem index.

### 4.4 Merkle Provenance Anchor & Vault Enclaves
* **Audit Directory Integrity:**
  ```bash
  ozos-guard.exe anchor /path/to/project
  ozos-guard.exe anchor-verify /path/to/project
  ```
* **Encrypted Volatile Vault:**
  ```bash
  # Pack folder into encrypted vault (XSalsa20-Poly1305)
  ozos-guard.exe vault pack ./sensitive_dir ./secure_vault.ozos

  # Extract from vault with zero temporary disk residue
  ozos-guard.exe vault open ./secure_vault.ozos ./extracted_dir
  ```

---

## 5. Post-Quantum Cryptographic Readiness (PQC)

Quantum adversaries equipped with Shor's algorithm threaten standard elliptic curves (Ed25519, X25519).
* O.Z.O.S integrates the standalone **`ozos-pqc.exe`** module implementing NIST FIPS 203 (ML-KEM-768 / Kyber) and FIPS 204 (ML-DSA-65 / Dilithium).
* **Hybrid Handshake Protocol:**
  - Computes classical ECDH shared secret ($K_c$) over X25519.
  - Computes quantum-resistant shared secret ($K_q$) over ML-KEM-768.
  - Combines both via SHA-512 KDF:
    $$K_{\text{session}} = \text{SHA-512}(K_c \mathbin{\Vert} K_q)$$
* **Verification:**
  ```bash
  ozos-pqc.exe status
  ozos-pqc.exe test
  ```
  Guarantees confidentiality even if classical elliptic curves are broken in the future.

---

## 6. Standard Operating Procedures (SOP) Checklist

### Pre-Deployment Checklist (Before Operation)
- [ ] Execute `ozos-guard workstation-shield` to verify bare-metal execution and detect hypervisors.
- [ ] Inspect Wi-Fi / network exposure. If WAN access is detected, isolate workstation or activate virtual air-gap.
- [ ] Confirm no private key files are saved on physical or virtual storage drives.
- [ ] Memorize or securely disperse the BIP-39 12-word brainseed using Shamir SSS $(3, 5)$.
- [ ] Verify that `build.bat test` passes all 5 verification phases.

### In-Flight Communication Checklist
- [ ] Distance < 2m & quiet environment: Use `ozos-phy audio-synth` or `ozos-phy ofdm-synth`.
- [ ] Noisy room or acoustic jamming: Transition to `ozos-phy glyph-encode` (Optical 32x32).
- [ ] Long distance or structural barrier: Transition to `ozos-phy lora-pack` (LoRa Sub-GHz).
- [ ] Verify peer public key out-of-band to prevent active man-in-the-middle attacks.

### Emergency Breach / Compromise SOP
1. **Immediate Crash Signal:** Trigger SIGSEGV or run `ozos-core crash-test` to activate `ozos_posix_signal_handler` and immediately purge all registered volatile RAM buffers.
2. **Execute File Sanitization:** Run `ozos-guard burn` on all vault archives and anchor files.
3. **Identity Revocation:** If a Shamir share is suspected of compromise, assemble 3 authentic shares on a clean bare-metal node, generate a new root seed, and re-split across new enclaves.
