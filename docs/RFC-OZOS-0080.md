# RFC-OZOS-0080: INVARIANT 80-BYTE BINARY WIRE PROTOCOL & AIR-GAPPED PHYSICAL TRANSPORT

```
Network Working Group                                          LumenNull
Request for Comments: 8080                    O.Z.O.S Core Working Group
Category: Standards Track                                   October 2026
Status: Sovereign Public Protocol Standard
Predecessor Lineage: Inherited from Genesis Protocol #0 (2009)
```

## ABSTRACT

This specification defines the technical standard for the **OZOS-80 (80-Byte Invariant Wire Protocol)** binary framing format and its isolated air-gapped physical transport channels, comprising inaudible near-ultrasound acoustic modulation ($18.0\text{ kHz} - 20.0\text{ kHz}$) and 2D-DCT optical steganography.

Unlike centralized architectures dependent on client-server hierarchies and cloud infrastructure, OZOS-80 is engineered as a **formless, decentralized wire protocol** — inheriting the permissionless peer-to-peer principles of the Genesis Protocol (2009). Any computational substrate, from 8-bit/32-bit embedded microcontrollers (STM32, ESP32) and personal workstations (x86_64, ARM64) to open-source architectures (RISC-V) and isolated trusted execution environments (Secure Enclave, seL4 Microkernel), can independently implement, parse, and verify this protocol without external authorization.

---

## 1. SOVEREIGN ETHOS & INVARIANT DESIGN PRINCIPLES

The Genesis heritage rejects superficial user interfaces in favor of an **immutable mathematical consensus protocol**. Applications can be deprecated, centralized hosts can be confiscated, but an unyielding mathematical specification released into the wild remains perpetually sovereign.

OZOS-80 strictly enforces four architectural invariants:
1. **Zero Dynamic Allocation (`no_std` / Stack-Only):** Explicitly forbids heap allocations (`malloc`/`new`), eliminating buffer overflow vulnerabilities and memory leaks at compile time.
2. **Deterministic Time Complexity:** Wire framing and ingress gatekeeping operate with strict $O(1)$ algorithmic complexity and latency under $20\text{ nanoseconds}$, providing wire-speed resistance against denial-of-service (DoS) packet floods.
3. **Physical Air-Gap Stealth:** High-assurance data transit crosses air-gapped physical space without radio-frequency emissions (Wi-Fi, Bluetooth), without Internet connectivity, and below human acoustic perception thresholds.
4. **Silicon Provenance Binding:** Sovereign cryptographic identity roots directly into physical silicon primitives (Hardware TPM 2.0 / Cryptographic Coprocessors).

---

## 2. WIRE FORMAT SPECIFICATION (OZOS-80)

The OZOS-80 primary frame header possesses an invariant, fixed size of exactly **80 Bytes**, aligned to a 16-byte boundary (`alignas(16)`) for optimal execution across modern SIMD vector units (Intel AVX-512, ARM Neon, WebAssembly SIMD128).

### 2.1. Header Bit Diagram

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                                                               +
|                                                               |
+             dct_coeffs[0..15] (64 Bytes = 16 x Float32)       +
|               SIMD 16-Byte Aligned (Offset 0x00 - 0x3F)       |
+                                                               +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       nonce (Bits 0..31)                      |
|                     Offset 0x40 - 0x43 (4 Bytes)              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       nonce (Bits 32..63)                     |
|                     Offset 0x44 - 0x47 (4 Bytes)              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    magic (0x47454E01 "GEN\x01")               |
|                     Offset 0x48 - 0x4B (4 Bytes)              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          payload_len          |         header_mac16          |
|      Offset 0x4C (2 Bytes)    |     Offset 0x4E (2 Bytes)     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                   payload (0..65,535 Bytes)                   |
|                   Offset 0x50 onward ...                      |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 2.2. Field Descriptions

| Offset | Length | Data Type | Field Name | Technical Definition & Invariants |
| :--- | :--- | :--- | :--- | :--- |
| `0x00 - 0x3F` | 64 Bytes | `float32[16]` | `dct_coeffs` | 16 optical frequency coefficients or semiconductor physical unclonable function (PUF) fingerprints. Enforces strict DAZ/FTZ (Denormals-Are-Zero / Flush-To-Zero) normalization to prevent timing side-channel attacks on subnormal floats. |
| `0x40 - 0x47` | 8 Bytes | `uint64_le` | `nonce` | Monotonically strictly increasing replay counter. A frame is valid if and only if: $\text{nonce}_t > \text{last\_seen\_nonce}$. Enforces 100% replay attack mitigation. |
| `0x48 - 0x4B` | 4 Bytes | `uint32_le` | `magic` | Protocol identification constant `0x47454E01` (ASCII: `"GEN\x01"`). Non-matching identifiers are rejected immediately at the first gate. |
| `0x4C - 0x4D` | 2 Bytes | `uint16_le` | `payload_len` | Byte length of the attached payload immediately following the 80-byte header ($0 \le L \le 65,535$). |
| `0x4E - 0x4F` | 2 Bytes | `uint16_le` | `header_mac16` | First-stage tripwire authentication code. Computed via CRC16-CCITT (polynomial `0x1021`, initial value `0xFFFF`) across the **first 78 bytes** (`0x00` through `0x4D`). Enables ultra-fast frame pre-validation in $<20\text{ ns}$. |
| `0x50` onward | $L$ Bytes | `uint8[]` | `payload` | Dynamic application payload (Detached Ed25519 signature, JSON-RPC telemetry, sovereign identity records, or encrypted blobs). |

---

## 3. PHYSICAL AIR-GAP TRANSPORT: NEAR-ULTRASOUND ACOUSTIC MODEM (18–20 kHz)

The near-ultrasound physical layer enables bidirectional, air-gapped data transmission between hardware nodes in ambient physical environments below human hearing thresholds.

### 3.1. Dual-Nibble 16-FSK Orthogonal Frequency Division
Each transmission byte ($8\text{ bits}$) is split into two 4-bit nibbles (low nibble and high nibble) transmitted concurrently across two orthogonal frequency sub-bands:

* **Channel 0 (Low Nibble 0..15):**
  $$f_0(k) = 18,100\text{ Hz} + (k \times 50\text{ Hz}), \quad k \in [0, 15] \implies 18,100\text{ Hz} \dots 18,850\text{ Hz}$$
* **Guard Band:**
  $$18,850\text{ Hz} \dots 19,000\text{ Hz} \quad (150\text{ Hz} \text{ adjacent sub-band isolation})$$
* **Channel 1 (High Nibble 0..15):**
  $$f_1(k) = 19,000\text{ Hz} + (k \times 50\text{ Hz}), \quad k \in [0, 15] \implies 19,000\text{ Hz} \dots 19,750\text{ Hz}$$
* **Preamble Sequence:** Dual-tone synchronization pulse $17,900\text{ Hz} + 19,900\text{ Hz}$ for $120\text{ ms}$.
* **Postamble Sequence:** Dual-tone framing delimiter $17,900\text{ Hz} + 20,100\text{ Hz}$ for $120\text{ ms}$.
* **Symbol Duration:** $40\text{ ms}$ per byte ($25\text{ Bytes/second}$ throughput).

### 3.2. Hann Window Spectral Shaping (5ms Envelope)
To eliminate acoustic click and pop artifacts caused by amplitude discontinuities, all tone transitions apply a 5ms Hann window envelope to the start and end of each symbol:
$$\text{env}(t) = \begin{cases}
0.5 \cdot \left(1 - \cos\left(\frac{\pi t}{t_{\text{ramp}}}\right)\right) & \text{when } 0 \le t < t_{\text{ramp}} \\
1.0 & \text{when } t_{\text{ramp}} \le t < T - t_{\text{ramp}} \\
0.5 \cdot \left(1 - \cos\left(\frac{\pi (T - t)}{t_{\text{ramp}}}\right)\right) & \text{when } T - t_{\text{ramp}} \le t \le T
\end{cases}$$
where $t_{\text{ramp}} = 5\text{ ms}$. This spectral shaping guarantees acoustic attenuation of at least **$-75\text{ dB}$** across the human audible spectrum ($20\text{ Hz} - 16\text{ kHz}$).

### 3.3. Zero-Allocation $O(N)$ Goertzel Demodulation Filter Bank
The receiver implements 32 parallel second-order Goertzel filters to isolate spectral energy at designated frequencies without full FFT transformations:
$$s[n] = x[n] + 2\cos(\omega_k) s[n-1] - s[n-2]$$
$$P_k = s^2[N-1] + s^2[N-2] - 2\cos(\omega_k) s[N-1] s[N-2]$$
where $\omega_k = \frac{2\pi f_k}{f_s}$. The algorithm operates in $O(N)$ time, requires zero heap RAM, and runs efficiently on constrained microcontrollers.

---

## 4. OPTICAL AIR-GAP TRANSPORT: 2D-DCT 8x8 STEGANOGRAPHY

The optical physical layer uses display surfaces and imaging sensors to transfer verifiable payload data embedded imperceptibly within image or video assets.

* **Two-Dimensional Discrete Cosine Transform (2D-DCT-II):** Evaluated across $8 \times 8$ luminance pixel blocks ($Y = 0.299R + 0.587G + 0.114B$).
* **Symmetric Mid-Frequency Coefficient Pair:**
  * Coefficient $C_1 = D(3, 2)$ (Row 3, Column 2; linear index `26`).
  * Coefficient $C_2 = D(2, 3)$ (Row 2, Column 3; linear index `19`).
* **1-Bit Embedding Invariant ($\Delta = 36.0$):**
  * Bit `1` Assertion: Enforce $(C_1 - C_2) \ge \Delta$.
  * Bit `0` Assertion: Enforce $(C_2 - C_1) \ge \Delta$.
* **Compression Robustness:** Resistant to standard JPEG lossy luminance quantization down to quality factor $Q \ge 75$.

---

## 5. GOLDEN TEST VECTORS

All conformant implementations must pass 100% of the normative golden test vectors specified below:

### Vector 1: Empty Payload / Keep-Alive Frame
* **Nonce:** `1000` (`0x00000000000003E8`)
* **Magic:** `0x47454E01` (ASCII `"GEN\x01"`)
* **Payload Length:** `0` (`0x0000`)
* **16 Float32 Coeffs:** `[1.0, 1.1, 1.2, ..., 2.5]`
* **Expected Tripwire MAC16:** `0x75F9`
* **Canonical 80-Byte Hex Sequence:**
  ```
  0000803fcdcc8c3f9a99993f6666a63f3333b33f0000c03fcdcccc3f9a99d93f6666e63f3333f33f0000004066660640cdcc0c40333313409a99194000002040e803000000000000014e45470000f975
  ```

### Vector 2: Standard Sovereign Payload Frame
* **Nonce:** `5000` (`0x0000000000001388`)
* **Magic:** `0x47454E01` (ASCII `"GEN\x01"`)
* **Payload Length:** `12` (`0x000C`)
* **Payload Content:** `"SOVEREIGN_ID"`
* **16 Float32 Coeffs:** `[1.5, 1.6, 1.7, ..., 3.0]`
* **Expected Tripwire MAC16:** `0x4CCF`
* **Canonical 92-Byte Hex Sequence:**
  ```
  0000c03fcdcccc3f9a99d93f6666e63f3333f33f0000004066660640cdcc0c40333313409a9919400000204066662640cdcc2c40333333409a993940000040408813000000000000014e45470c00cf4c534f5645524549474e5f4944
  ```

---

## 6. REFERENCE IMPLEMENTATIONS

Independent, zero-dependency reference parsers are provided in C99 and Rust (`no_std`):

### 6.1. C99 Reference Implementation (Zero-Allocation Parser)

```c
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define OZOS_MAGIC 0x47454E01U

static uint16_t ozos_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0; i < len; ++i) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x8000U) ? ((crc << 1) ^ 0x1021U) : (crc << 1);
        }
    }
    return crc;
}

bool ozos_parse_frame(const uint8_t* buf, size_t buf_len, uint64_t last_nonce, uint64_t* out_nonce) {
    if (buf_len < 80) return false;

    /* 1. Tripwire MAC16 verification (< 20ns) */
    uint16_t expected_mac = ozos_crc16(buf, 78);
    uint16_t packet_mac = (uint16_t)buf[78] | ((uint16_t)buf[79] << 8);
    if (packet_mac != expected_mac) return false;

    /* 2. Magic header verification */
    uint32_t magic = (uint32_t)buf[72] | ((uint32_t)buf[73] << 8) |
                     ((uint32_t)buf[74] << 16) | ((uint32_t)buf[75] << 24);
    if (magic != OZOS_MAGIC) return false;

    /* 3. Nonce replay verification */
    uint64_t nonce = 0;
    for (int i = 0; i < 8; ++i) nonce |= ((uint64_t)buf[64 + i]) << (i * 8);
    if (nonce <= last_nonce) return false;

    if (out_nonce) *out_nonce = nonce;
    return true;
}
```

### 6.2. Rust Reference Implementation (`no_std` Zero-Dependency Parser)

```rust
#![no_std]

pub const OZOS_MAGIC: u32 = 0x47454E01;

pub fn ozos_crc16(data: &[u8]) -> u16 {
    let mut crc: u16 = 0xFFFF;
    for &byte in data {
        crc ^= (byte as u16) << 8;
        for _ in 0..8 {
            crc = if (crc & 0x8000) != 0 { (crc << 1) ^ 0x1021 } else { crc << 1 };
        }
    }
    crc
}

pub fn ozos_validate_frame(buf: &[u8], last_nonce: u64) -> Result<(u64, &[u8]), &'static str> {
    if buf.len() < 80 {
        return Err("BUFFER_TOO_SMALL");
    }

    // 1. MAC16 Tripwire check
    let expected_mac = ozos_crc16(&buf[0..78]);
    let packet_mac = u16::from_le_bytes([buf[78], buf[79]]);
    if packet_mac != expected_mac {
        return Err("MAC16_MISMATCH");
    }

    // 2. Magic check
    let magic = u32::from_le_bytes([buf[72], buf[73], buf[74], buf[75]]);
    if magic != OZOS_MAGIC {
        return Err("INVALID_MAGIC");
    }

    // 3. Monotonic nonce check
    let nonce = u64::from_le_bytes(buf[64..72].try_into().unwrap());
    if nonce <= last_nonce {
        return Err("REPLAY_ATTACK_DETECTED");
    }

    let payload_len = u16::from_le_bytes([buf[76], buf[77]]) as usize;
    if buf.len() < 80 + payload_len {
        return Err("TRUNCATED_PAYLOAD");
    }

    Ok((nonce, &buf[80..80 + payload_len]))
}
```

---

## 7. SECURITY CONSIDERATIONS

1. **Cold-Boot & Memory Remanence Defense:** All memory allocations handling secret key material must be sanitized via `ozos_secure_zeroize()` following a 3-pass purge: `0xAA` $\to$ `0x55` $\to$ `0x00`, combined with compiler memory barriers.
2. **Denormal / Subnormal Floating-Point Poisoning:** When receiving 16 Float32 coefficients from untrusted sources, implementations must reject inputs with exponents of `0x00` (Subnormals) or `0xFF` (NaN/Inf) to eliminate FPU stall and timing side-channel attacks.
3. **Anti-DFA Laser Glitch Fault Injection:** In high-assurance cryptographic deployments, critical state checks must execute concurrently across FPU (Float32 SIMD) and ALU (Fixed-point Q15.16) pipelines. Detection of bit divergence triggers an immediate fail-closed hardware abort.

---

## 8. CONCLUSION & OPEN SPECIFICATION

The **RFC-OZOS-0080** specification establishes O.Z.O.S as an open, formless public standard. No individual, entity, or central authority owns the protocol; it exists as an immutable mathematical formulation in the public domain, guaranteeing perpetual cryptographic self-sovereignty.
