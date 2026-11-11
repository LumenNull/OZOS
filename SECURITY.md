# Security Policy & Vulnerability Disclosure

## 1. Philosophical Stance

O.Z.O.S (Omni-Zero Operating Shield) is designed for adversarial environments where zero trust is assumed across all hardware, virtualized, and network boundaries. Because cryptographic algorithms and zero-allocation memory models must remain mathematically incorruptible, we actively welcome independent security research, academic cryptanalysis, and adversarial red-teaming from the global cybersecurity community.

---

## 2. Reporting a Vulnerability

If you discover a security vulnerability, side-channel leakage, timing asymmetry, or memory safety defect in O.Z.O.S, **please do not open a public GitHub issue**.

Follow the Coordinated Vulnerability Disclosure (CVD) protocol:

1. **Primary Reporting Channel:**
   * Submit an advisory directly through **GitHub Private Vulnerability Reporting** on the repository:  
     `https://github.com/LumenNull/OZOS/security/advisories/new`
2. **Encrypted Submission:**
   * Please encrypt your email or advisory payload using our public PGP/GPG key or coordinate via signed GitHub messages.
3. **Required Information:**
   * Detailed description of the defect or attack vector.
   * Minimal reproducible proof-of-concept (PoC) code or script.
   * Target architecture and compiler flags (e.g., x86_64 MSVC /O2, ARM Cortex-M4 GCC -O3).
   * Assessment of exploitability, severity, and potential impact.

---

## 3. Scope of Evaluation

### In-Scope:
* **Constant-Time & Side-Channel Vulnerabilities:** Timing variations in Galois Field multiplication ($GF(2^8)$ Shamir polynomial evaluation), scalar multiplication on Curve25519, or polynomial arithmetic in ML-KEM / ML-DSA.
* **Physical Layer Parsing Deficiencies:** Stack buffer overflows, state corruption, or arithmetic crashes in 64-FFT OFDM demodulation, 16-FSK frequency tracker, or Reed-Solomon RS(96,80) decoding.
* **Zeroization Failures:** Compiler dead-code elimination bypassing `ozos_burn()` volatile memory purging under aggressive optimization flags (`/O2`, `-O3`, `-flto`).
* **Micro-Kernel Memory Safety:** Stack-smashing or unauthorized heap invocations violating the strict `malloc = 0` invariant.
* **Ephemeral State Leakage:** Retention of master seed bytes in swap files, crash dumps, or CPU register spill.

### Out-of-Scope:
* Vulnerabilities requiring unmitigated physical destruction or hardware decapping of the host silicon.
* Host operating system root/kernel compromise where kernel-level rootkits manipulate hardware hypervisors beyond ring 0.
* Denial-of-Service attacks relying purely on flooding the physical acoustic medium with overwhelming white noise (acoustic jamming is a known physical property, mitigated by switching to Optical 32x32 Glyph or LoRa Sub-GHz PHY).
* Social engineering or physical extortion of the key custodian.

---

## 4. Coordinated Disclosure Timeline

* **Initial Response:** Within 48 hours of report receipt, acknowledging receipt and beginning technical triage.
* **Triage & Reproduction:** Within 7 business days, confirming vulnerability reproducibility and establishing severity ranking (CVSS v3.1).
* **Patch Development & Verification:** Within 30 to 90 days (depending on complexity), engineered, fuzzed, and cryptographically verified.
* **Public Release & Advisory:** A coordinated CVE and public security bulletin will be published simultaneously with the patched release.

---

## 5. Security Hall of Fame

Researchers who discover novel, reproducible vulnerabilities that improve the security posture of O.Z.O.S will be permanently acknowledged in our **Cryptographic Hall of Fame** in the Genesis documentation (unless anonymity is requested).

---

*Last Updated: November 2026*  
*O.Z.O.S Core Security Working Group*
