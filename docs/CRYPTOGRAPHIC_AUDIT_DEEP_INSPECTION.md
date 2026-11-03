# O.Z.O.S Deep Cryptographic & Microarchitectural Inspection
## Master Audit of 6 Fundamental Design Vulnerabilities & Remediation Directives

---

> *"Security is never found in cosmic or quantum metaphors. It is found strictly in the algebraic irreversibility of one-way functions, strict domain separation of binary frames, and rigorous physical channel budgeting."*

This document formalizes the 6 fundamental cryptographic, binary framing, and physical-layer vulnerabilities identified during the deep architectural inspection of O.Z.O.S (Omni-Zero Operating Shield), alongside concrete, binding remediation directives.

---

## 1. Algebraic Invertibility of FNV-1a-64 in Keyed Local Vault (State-Recovery Forgery)

### The Vulnerability
In `contrib/web-cockpit/ozos-engine.js:saveLocalVault`, the keyed vault implementation calculates a 64-bit verification tag using domain-separated FNV-1a:
$$\text{Tag} = \text{FNV-1a-64}(\text{"OZOS\_VAULT\_KEYED\_TRIPWIRE\_V1:"} \parallel \text{vaultKey} \parallel \text{Plaintext\_JSON})$$

While documentation claims brute-force resistance depends on the entropy of `vaultKey`, FNV-1a is **not a cryptographically secure pseudo-random function (PRF)**. It is a linear hash over the ring $\mathbb{Z}_{2^{64}}$:
$$H_i = (H_{i-1} \oplus B_i) \times \text{Prime} \pmod{2^{64}}$$
where $\text{Prime} = \text{0x100000001b3}$ ($1099511628211$).

Because $\text{Prime}$ is an odd integer, $\gcd(\text{Prime}, 2^{64}) = 1$, which mathematically guarantees the existence of a unique multiplicative inverse $\text{Prime}^{-1} \pmod{2^{64}}$:
$$\text{Prime}^{-1} \equiv \text{0x476ff2a7e7161b3b} \pmod{2^{64}}$$
$$\text{Prime} \times \text{Prime}^{-1} \equiv 1 \pmod{2^{64}}$$

### Attack Mechanics (State Recovery in $O(n)$)
Because the Local Vault stores the JSON payload in Base64 plaintext, an attacker possessing an authentic pair $(\text{Plaintext\_JSON}, \text{Tag}_{64})$ can step backwards byte-by-byte in $O(n)$ operations without brute-forcing:
$$H_{i-1} = (H_i \times \text{Prime}^{-1} \pmod{2^{64}}) \oplus B_i$$

Iterating backwards through all bytes of the known JSON payload directly reveals the exact internal state immediately following key absorption:
$$S_{\text{after\_key}} = H(\text{"OZOS\_VAULT\_KEYED\_TRIPWIRE\_V1:"} \parallel \text{vaultKey})$$

Once $S_{\text{after\_key}}$ is recovered (computable in $< 50\text{ ns}$):
1. The attacker can forge valid tags for arbitrary modified JSON payloads (e.g., modifying ledger balances, account IDs, or timestamps).
2. The entropy of `vaultKey` is rendered completely irrelevant.

### Remediation Directive
- Completely excise FNV-1a from authentication and vault sealing operations.
- Replace with **HMAC-SHA256** (WebCrypto API in browser / SHA-256 in C99) or **Poly1305** (already native to `core/tweetnacl.c`).

---

## 2. Cross-Profile Type Confusion & Polyglot Packet (Profile A vs Profile B)

### The Vulnerability
O.Z.O.S defines two distinct 80-byte wire formats sharing the same 32-bit identifier `MAGIC = 0x47454E01`:
* **Profile A (OZOS-80 Bare-Metal):** 
  - `0x00 - 0x3F`: 16 float32 values (`dct_coeffs`)
  - `0x40 - 0x47`: 64-bit Monotonic Nonce
  - `0x48 - 0x4B`: Magic Header (`0x47454E01`)
* **Profile B (GENS-v1 Web Cockpit):**
  - `0x00 - 0x03`: Magic Header (`0x47454E01`)
  - `0x04 - 0x0B`: 64-bit Monotonic Nonce
  - `0x0C - 0x47`: 15 float32 values (`perceptual_fingerprint`)
  - `0x48 - 0x4B`: Payload Checksum (32-bit)

### Attack Mechanics (IEEE-754 Collision & Signed Profile Confusion)
1. In IEEE-754 single-precision float32, the byte sequence `0x47454E01` decodes to:
   $$+50510.00390625 \in \mathbb{R}$$
   This is a finite, normalized real number (neither `NaN` nor `Infinity`). Thus, a Profile A packet whose first DCT coefficient is set to $\approx 50510.004$ begins with the exact Magic bytes expected by Profile B at offset `0x00`.
2. Conversely, a Profile B packet can easily have its 32-bit checksum crafted to equal `0x47454E01`, placing the Magic bytes at offset `0x48` expected by Profile A.
3. **Signed Profile Confusion:** Ed25519 detached signatures sign the raw 80-byte header without domain separation:
   $$\text{Sig} = \text{Sign}_{sk}(\text{Header}_{80} \parallel \text{Payload})$$
   A packet legitimately signed under Profile B in the Web Cockpit can be re-routed to a Bare-Metal Profile A receiver. Profile A interprets bytes `0x40 - 0x47` (the tail of Profile B's perceptual fingerprint) as its **Monotonic Nonce**, enabling an adversary to manipulate the anti-replay state using crafted image/audio frequency data.

### Remediation Directive
- Assign distinct, non-overlapping Magic constants:
  - `PROFILE_A_MAGIC = 0x4F5A4F53` (ASCII: `OZOS`)
  - `PROFILE_B_MAGIC = 0x47454E53` (ASCII: `GENS`)
- Implement strict cryptographic domain separation tags in Ed25519 signing (RFC 8032):
  $$\text{Sig} = \text{Sign}_{sk}(\text{"OZOS\_PROFILE\_A\_v1:"} \parallel \text{Header}_{80} \parallel \text{Payload})$$

---

## 3. Remote Denial-of-Service via Ingress "Fail-Deadly" Memory Paradox

### The Vulnerability
System specifications claim:
> *"If any bit is corrupted (cosmic ray, bus glitch, DFA fault injection), the packet fails MAC16/DFA checks and volatile key memory is instantly sanitized."*

Coupling this invariant to the open, unauthenticated Near-Ultrasound channel (18.1–19.8 kHz) creates a catastrophic Denial-of-Service vector:
* The 18–20 kHz band operates across shared acoustic spaces subject to ambient acoustic reflections, device non-linearities, and background noise.
* If a receiver purges its volatile enclave memory upon detecting an incoming packet with an invalid MAC16 or corrupted bit, **any remote transmitter playing background ultrasonic noise or a single malformed beacon forces all listening nodes to commit cryptographic suicide (instant memory wipe and reboot)**.

### Remediation Directive
Strictly decouple ingress network filtering from internal hardware fault handling:
* **Ingress Packet Drop (Fail-Silent):** Packets received over radio, sound, or network that fail MAC16, checksums, or signatures must be discarded silently without touching enclave key memory.
* **Internal Enclave Fault (Fail-Deadly):** Instant zeroization and hardware crowbar resets are reserved exclusively for **internal execution inconsistencies** (e.g., dual-core lockstep ALU vs FPU discrepancies, failed boot-time memory canaries, or physical tamper sensor assertions).

---

## 4. Anti-Replay Counter Reset on Ephemeral CLI Process Restarts

### The Vulnerability
Anti-replay protection relies on monotonic nonces ($Nonce > Nonce_{last\_seen}$). However, operational scoping admits:
> *"In-memory anti-replay state tracking in Web Cockpit and CLI executions is scoped to active process memory."*

### Attack Mechanics
1. Node A transmits a valid signed command (e.g., $Nonce = 105$) to an air-gapped node running CLI evaluations (`ozos packet-verify`).
2. An eavesdropping microphone in the room records the acoustic FSK transmission.
3. The CLI execution finishes and the process terminates, resetting its in-memory state.
4. When the next invocation occurs without explicit `--last-nonce` arguments, `last_seen_nonce` initializes to `0`.
5. The attacker replays the recorded audio signal. The signature is mathematically authentic, the MAC16 is valid, and $105 > 0$ satisfies the replay condition. The historical command executes a second time.

### Remediation Directive
- For stateless or short-lived CLI utilities, pure monotonic counters cannot provide replay protection without non-volatile rollback-resistant state storage (e.g., TPM 2.0 NVRAM counter or seL4 monotonic hardware capability).
- Alternatively, mandate an interactive **Two-Way Challenge-Response Handshake** where the verifier emits an ephemeral 256-bit random challenge that must be bound into the signed payload.

---

## 5. False Security Assurance in Unkeyed 16-Bit Sub-Profile A2

### The Vulnerability
Documentation states:
> *"Sub-Profile A2 (FNV-1a-64 Default Carrier Gate): Domain-separated 64-bit FNV-1a folded into 16 bits covering Header (0..77) AND Payload (80..80+len)... emitted by default to prevent A1 downgrade tampering."*

This statement contains two critical security fallacies:
1. **Unkeyed Function:** Both A1 (CRC16) and A2 (FNV-1a folded to 16-bit) are completely unkeyed. On unsigned frames, an attacker modifying the payload simply recomputes the new A2 value and overwrites the 2-byte `header_mac16` field.
2. **16-Bit Birthday Collision Bound:** Folding 64 bits to 16 bits leaves only $2^{16} = 65,536$ possible values. Finding a second-preimage collision (a malicious payload yielding the identical 16-bit checksum) requires at most $65,536$ evaluations, computable in $< 0.2\text{ ms}$ on modern CPUs.

### Remediation Directive
- Remove all claims that A2 provides "tamper resistance" or "downgrade protection" on unsigned frames.
- Explicitly reclassify A1/A2 as **Error-Detecting Codes (EDC)** designed solely to detect accidental physical transmission noise.
- Mandate that payload integrity and authenticity require cryptographic signatures (Ed25519) or authenticated encryption (Poly1305).

---

## 6. Microarchitectural Key Residue & Physical Acoustic Channel Limits

### 6.1 Call Stack Residue in C99 TweetNaCl
* `core/tweetnacl.c` implements Ed25519 and X25519 scalar multiplications by expanding secret scalar keys into 64-bit arrays (`gf / i64[16]`) allocated on the thread's Call Stack.
* While `VirtualLock` / `mlock` protects the designated enclave buffer, stack memory resides outside the locked buffer.
* Upon function return, stack frames unwind without memory zeroization, leaving expanded private key material in deallocated stack pages accessible to subsequent process threads or memory scrapers.
* **Remediation:** Introduce a stack hygiene wrapper (`crypto_sanitize_stack`) that invokes `SecureZeroMemory` / `explicit_bzero` across local stack frames immediately following scalar operations.

### 6.2 Unknown Key-Share (UKS) in Detached Signatures
* The packet framing structure:
  $$[\text{OZOS-80 Header (80B)}] \parallel [\text{Payload (L B)}] \parallel [\text{Ed25519 PubKey (32B)}] \parallel [\text{Ed25519 Sig (64B)}]$$
  places `PubKey` outside the signed boundary if the signature is computed solely over $(\text{Header} \parallel \text{Payload})$.
* This permits an Unknown Key-Share attack where an adversary asserts ownership of another party's signed packet in transit.
* **Remediation:** Explicitly bind the signer's Public Key into the signed message transcript:
  $$\text{Sig} = \text{Sign}_{sk}(\text{DomainTag} \parallel \text{PubKey}_{32} \parallel \text{Header}_{80} \parallel \text{Payload})$$

### 6.3 16-FSK Acoustic Bandwidth & Frame Error Rate (FER)
* Tone duration: $40\text{ ms}$ per symbol ($25\text{ Baud}$). Dual-nibble transmission yields $100\text{ bps}$ ($12.5\text{ bytes/s}$).
* A minimal signed frame ($80\text{B Header} + 32\text{B PubKey} + 64\text{B Sig} = 176\text{ bytes}$) requires:
  $$\text{Transmission Time} = \frac{176}{12.5} \approx 14.08\text{ seconds}$$
* Continuous $14$-second ultrasound transmissions without **Forward Error Correction (FEC)** in real-world acoustic rooms suffer near $100\%$ Frame Error Rates due to multipath phase cancellations, ambient noise, or room reverb.
* **Remediation:** Incorporate **Reed-Solomon (RS) FEC** codes and packet fragmentation to tolerate dropped acoustic symbols without losing entire 14-second frames.

---

## Summary of Architectural Action Items

| Vulnerability | Component | Severity | Immediate Fix |
| :--- | :--- | :--- | :--- |
| **FNV-1a Invertibility** | `ozos-engine.js:saveLocalVault` | **CRITICAL** | Replace FNV-1a with HMAC-SHA256 / Poly1305 |
| **Profile Confusion** | `libozos.h` / `genesis-shield.js` | **HIGH** | Distinct Magics (`0x4F5A4F53` vs `0x47454E53`) + Domain Separation |
| **Remote Ingress DoS** | `ozos_ghost.c` / Architecture | **HIGH** | Decouple Ingress Drop (silent) from Enclave Fault (deadly) |
| **Ephemeral Replay** | `ozos_main.c` / CLI | **MEDIUM** | Enforce Two-Way Challenge-Response or NVRAM counter |
| **A2 False Assurance** | `libozos.h` / Documentation | **MEDIUM** | Reclassify A2 as EDC only; mandate Ed25519 for authenticity |
| **Stack Residue & PHY** | `tweetnacl.c` / `ozos_ghost.c` | **HIGH** | Stack zeroization wrappers, UKS binding, Reed-Solomon FEC |

---
*Signed and sealed for immediate engineering execution.*  
**Author:** LumenNull & ZeroStudio  
**Inspection Reference:** `AUDIT-OZOS-2026-6P`  
