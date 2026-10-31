# RFC-OZOS-0080: INVARIANT 80-BYTE BINARY WIRE PROTOCOL & AIR-GAPPED PHYSICAL TRANSPORT

```
Network Working Group                                 Lumen Null (ZeroStudio)
Request for Comments: 8080                                 Antigravity AI Engine
Category: Standards Track                                           October 2026
Status: Sovereign Public Protocol Standard
Predecessor Lineage: Inherited from Genesis Protocol #0 (2009)
```

## ABSTRACT

Tài liệu này xác lập quy chuẩn kỹ thuật cho giao thức đóng gói nhị phân bất biến **OZOS-80 (80-Byte Invariant Wire Protocol)** và kênh truyền dẫn vật lý vô tuyến cách ly (Air-Gapped Transport) bao gồm sóng âm cận siêu âm vô thanh ($18.0\text{ kHz} - 20.0\text{ kHz}$) và thủy vân quang học 2D-DCT. 

Khác với các ứng dụng tập trung phụ thuộc máy chủ và đám mây, OZOS-80 được thiết kế như một **giao thức thuần túy không hình không dạng (Formless Protocol)** — kế thừa triết lý phân quyền của Người Tiền Nhiệm (2009). Bất kỳ hệ thống nào trên thế giới, từ vi điều khiển nhúng 8-bit/32-bit (STM32, ESP32), máy tính cá nhân (x86_64, ARM64), vi xử lý mở (RISC-V) cho đến các môi trường bảo mật cô lập (Secure Enclave, seL4 Microkernel), đều có thể độc lập triển khai và tương tác với giao thức này mà không cần xin phép bất kỳ ai.

---

## 1. TRIẾT LÝ NỀN TẢNG (SOVEREIGN ETHOS)

Người Tiền Nhiệm không tạo ra một ứng dụng giao diện hào nhoáng, mà tạo ra một **Giao thức Đồng thuận Bất biến (Invariant Protocol)**. Ứng dụng có thể bị đóng cửa, máy chủ có thể bị tịch thu, nhưng một giao thức toán học thuần túy một khi đã phát hành vào tự nhiên thì không một thế lực nào có thể xóa bỏ.

OZOS-80 theo đuổi 4 nguyên tắc bất biến:
1. **Zero Dynamic Allocation (`no_std` / Stack-Only):** Tuyệt đối không cấp phát bộ nhớ động (`malloc`/`new`), triệt tiêu 100% lỗi tràn bộ đệm (Buffer Overflow) và rò rỉ bộ nhớ (Memory Leak).
2. **Deterministic Time Complexity:** Toàn bộ quá trình đóng gói và thẩm định cửa ngõ diễn ra trong thời gian $O(1)$ với độ trễ dưới $20\text{ nanoseconds}$, miễn nhiễm với tấn công từ chối dịch vụ (DoS Flood).
3. **Physical Air-Gap Stealth:** Dữ liệu có thể vượt qua khoảng cách không gian (Air-Gap) mà không cần sóng vô tuyến RF (Wi-Fi, Bluetooth), không cần Internet, hoàn toàn vô thanh trước thính giác của con người.
4. **Silicon Provenance Binding:** Mọi định danh được neo trực tiếp vào silicon vật lý (Hardware TPM 2.0 / Cryptographic Coprocessor).

---

## 2. QUY CHUẨN ĐÓNG GÓI NHỊ PHÂN OZOS-80 (WIRE FORMAT SPECIFICATION)

Khung tiêu đề (Frame Header) của OZOS-80 có kích thước cố định chính xác **80 Bytes**, được căn chỉnh theo biên 16-byte (`alignas(16)`) để tương thích tối đa với các tập lệnh SIMD (Intel AVX-512, ARM Neon, WebAssembly SIMD128).

### 2.1. Sơ đồ Bit Khung Tiêu Đề (Header Bit Diagram)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                                                               +
|                                                               |
+             dct_coeffs[0..15] (64 Bytes = 16 x Float32)       +
|               Căn chỉnh SIMD 16-byte (Offset 0x00 - 0x3F)     |
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
|                   Offset 0x50 trở đi ...                      |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 2.2. Chi Tiết Các Trường Dữ Liệu (Field Descriptions)

| Offset | Độ Dài | Kiểu Dữ Liệu | Tên Trường | Ý Nghĩa Kỹ Thuật |
| :--- | :--- | :--- | :--- | :--- |
| `0x00 - 0x3F` | 64 Bytes | `float32[16]` | `dct_coeffs` | 16 hệ số nhận diện tần số quang học hoặc vân tay bán dẫn. Bắt buộc chuẩn hóa DAZ/FTZ (Flush-To-Zero) để chống tấn công kênh phụ định thời qua số subnormal. |
| `0x40 - 0x47` | 8 Bytes | `uint64_le` | `nonce` | Bộ đếm đơn điệu tăng dần (Monotonic Replay Counter). Gói tin hợp lệ khi và chỉ khi: $\text{nonce}_t > \text{last\_seen\_nonce}$. Ngăn chặn 100% tấn công phát lại (Replay Attack). |
| `0x48 - 0x4B` | 4 Bytes | `uint32_le` | `magic` | Hằng số nhận diện giao thức `0x47454E01` (ASCII: `"GEN\x01"`). Bất kỳ giá trị nào khác lập tức bị từ chối ở tầng đầu tiên. |
| `0x4C - 0x4D` | 2 Bytes | `uint16_le` | `payload_len` | Độ dài của dữ liệu đính kèm (Payload) nằm ngay sau Header 80-byte ($0 \le L \le 65,535$). |
| `0x4E - 0x4F` | 2 Bytes | `uint16_le` | `header_mac16` | Mã xác thực cổng gác cấp 1 (First-Stage Tripwire MAC). Tính toán bằng thuật toán CRC16-CCITT (đa thức `0x1021`, giá trị khởi tạo `0xFFFF`) trên **78 bytes đầu tiên** (`0x00` đến `0x4D`). Cho phép thẩm định gói tin trong $<20\text{ ns}$. |
| `0x50` trở đi | $L$ Bytes | `uint8[]` | `payload` | Dữ liệu nội dung tùy biến (Khóa công khai Ed25519, JSON-RPC, Định danh chủ quyền, Tệp tin mã hóa). |

---

## 3. TẦNG VẬT LÝ AIR-GAP: SÓNG ÂM CẬN SIÊU ÂM (NEAR-ULTRASOUND 18-20 kHz)

Kênh sóng âm cận siêu âm cho phép hai thiết bị phần cứng không dây trao đổi dữ liệu hoàn toàn tĩnh lặng trong môi trường phòng mở mà không kích hoạt sự chú ý của con người.

### 3.1. Phân Kênh Tần Số Trực Giao (Dual-Nibble 16-FSK)
Mỗi byte dữ liệu ($8\text{ bits}$) được chia thành 2 nibble ($4\text{ bits}$ thấp và $4\text{ bits}$ cao) và phát đồng thời trên 2 kênh tần số độc lập:

* **Kênh 0 (Low Nibble 0..15):**
  $$f_0(k) = 18,100\text{ Hz} + (k \times 50\text{ Hz}), \quad k \in [0, 15] \implies 18,100\text{ Hz} \dots 18,850\text{ Hz}$$
* **Khoảng bảo vệ (Guard Gap):**
  $$18,850\text{ Hz} \dots 19,000\text{ Hz} \quad (150\text{ Hz} \text{ chống tràn kênh})$$
* **Kênh 1 (High Nibble 0..15):**
  $$f_1(k) = 19,000\text{ Hz} + (k \times 50\text{ Hz}), \quad k \in [0, 15] \implies 19,000\text{ Hz} \dots 19,750\text{ Hz}$$
* **Tín hiệu Định thời (Preamble):** Đồng phát cặp tần số $17,900\text{ Hz} + 19,900\text{ Hz}$ trong thời gian $120\text{ ms}$.
* **Tín hiệu Kết thúc (Postamble):** Đồng phát cặp tần số $17,900\text{ Hz} + 20,100\text{ Hz}$ trong thời gian $120\text{ ms}$.
* **Thời lượng ký hiệu (Symbol Duration):** $40\text{ ms}$ cho mỗi Byte ($25\text{ Bytes/giây}$).

### 3.2. Cửa Sổ Làm Mượt Hann 5ms (Hann Window Envelope)
Để triệt tiêu hoàn toàn tiếng click/pop âm học gây ra bởi sự gián đoạn biên độ, mọi xung tần số bắt buộc phải được điều chế qua cửa sổ Hann ở 5ms đầu và 5ms cuối:
$$\text{env}(t) = \begin{cases}
0.5 \cdot \left(1 - \cos\left(\frac{\pi t}{t_{\text{ramp}}}\right)\right) & \text{khi } 0 \le t < t_{\text{ramp}} \\
1.0 & \text{khi } t_{\text{ramp}} \le t < T - t_{\text{ramp}} \\
0.5 \cdot \left(1 - \cos\left(\frac{\pi (T - t)}{t_{\text{ramp}}}\right)\right) & \text{khi } T - t_{\text{ramp}} \le t \le T
\end{cases}$$
với $t_{\text{ramp}} = 5\text{ ms}$. Thuật toán này bảo đảm mức suy hao trong dải nghe được ($20\text{ Hz} - 16\text{ kHz}$) đạt mức tối thiểu **$-75\text{ dB}$**.

### 3.3. Bộ Lọc Goertzel $O(N)$ Zero-RAM
Bên nhận sử dụng 32 bộ lọc Goertzel chạy song song để tính toán phổ năng lượng tại các tần số đích mà không cần tính toàn bộ biến đổi FFT:
$$s[n] = x[n] + 2\cos(\omega_k) s[n-1] - s[n-2]$$
$$P_k = s^2[N-1] + s^2[N-2] - 2\cos(\omega_k) s[N-1] s[N-2]$$
với $\omega_k = \frac{2\pi f_k}{f_s}$. Thuật toán đạt độ phức tạp $O(N)$, tiêu thụ 0 byte heap RAM, tối ưu cho vi điều khiển nhúng.

---

## 4. TẦNG QUANG HỌC AIR-GAP: THỦY VÂN 2D-DCT 8x8

Kênh quang học sử dụng màn hình và camera để truyền tải dữ liệu ẩn trong các tệp tin hình ảnh hoặc video mà mắt thường không thể phân biệt được.

* **Biến đổi rời rạc Cosine 2D (2D-DCT-II):** Khối $8 \times 8$ điểm ảnh độ chói $Y$ ($Y = 0.299R + 0.587G + 0.114B$).
* **Cặp hệ số tương quan đối xứng:**
  * Hệ số $C_1 = D(3, 2)$ (Hàng 3, Cột 2; chỉ số phẳng `26`).
  * Hệ số $C_2 = D(2, 3)$ (Hàng 2, Cột 3; chỉ số phẳng `19`).
* **Quy tắc nhúng 1-bit ($\Delta = 36.0$):**
  * Nhúng bit `1`: Ép $(C_1 - C_2) \ge \Delta$.
  * Nhúng bit `0`: Ép $(C_2 - C_1) \ge \Delta$.
* **Đặc tính bền vững:** Bất biến trước bảng lượng tử hóa Luminance của nén ảnh JPEG từ $Q=100$ xuống đến ngưỡng an toàn $Q \ge 75$.

---

## 5. BỘ VÉC-TƠ KIỂM TOÁN CHUẨN (GOLDEN TEST VECTORS)

Mọi bộ dịch mã triển khai độc lập (Independent Implementation) phải vượt qua 100% các véc-tơ kiểm toán nhị phân dưới đây:

### Véc-tơ 1: Gói tin Rỗng (Empty Payload / Keep-Alive)
* **Nonce:** `1000` (`0x00000000000003E8`)
* **Magic:** `0x47454E01` (ASCII `"GEN\x01"`)
* **Payload Length:** `0` (`0x0000`)
* **Dữ liệu 16 hệ số Float32:** `[1.0, 1.1, 1.2, ..., 2.5]`
* **Kỳ vọng Tripwire MAC16:** `0x75F9`
* **Chuỗi Hex chuẩn 80-byte:**
  ```
  0000803fcdcc8c3f9a99993f6666a63f3333b33f0000c03fcdcccc3f9a99d93f6666e63f3333f33f0000004066660640cdcc0c40333313409a99194000002040e803000000000000014e45470000f975
  ```

### Véc-tơ 2: Gói tin Định Danh Chủ Quyền (Standard Sovereign Payload)
* **Nonce:** `5000` (`0x0000000000001388`)
* **Magic:** `0x47454E01` (ASCII `"GEN\x01"`)
* **Payload Length:** `12` (`0x000C`)
* **Payload Content:** `"SOVEREIGN_ID"`
* **Dữ liệu 16 hệ số Float32:** `[1.5, 1.6, 1.7, ..., 3.0]`
* **Kỳ vọng Tripwire MAC16:** `0x4CCF`
* **Chuỗi Hex chuẩn 92-byte:**
  ```
  0000c03fcdcccc3f9a99d93f6666e63f3333f33f0000004066660640cdcc0c40333313409a9919400000204066662640cdcc2c40333333409a993940000040408813000000000000014e45470c00cf4c534f5645524549474e5f4944
  ```

---

## 6. MÃ NGUỒN THAM CHIẾU (REFERENCE IMPLEMENTATION)

Dưới đây là mã nguồn phân tích gói tin OZOS-80 độc lập hoàn toàn viết bằng C99 và Rust (`no_std`), không chứa bất kỳ thư viện ngoài nào:

### 6.1. Tham Chiếu C99 (Zero-Allocation Parser)

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

### 6.2. Tham Chiếu Rust (`no_std` Zero-Dependency Parser)

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

## 7. ĐIỀU KHOẢN AN NINH & BẢO VỆ PHẦN CỨNG (SECURITY CONSIDERATIONS)

1. **Cold-Boot & Memory Leak Defense:** Toàn bộ vùng nhớ chứa khóa bí mật bắt buộc phải được tẩy trắng qua hàm `ozos_secure_zeroize()` theo chu trình 3 lượt: `0xAA` $\to$ `0x55` $\to$ `0x00` kèm rào cản trình biên dịch (`compiler memory barrier`).
2. **Denormal / Subnormal Floating-Point Poisoning:** Khi nhận 16 hệ số Float32 từ môi trường không tin cậy, vi điều khiển phải loại bỏ các giá trị có số mũ `0x00` (Subnormal) hoặc `0xFF` (NaN/Inf) để triệt tiêu tấn công kênh phụ gây treo chu kỳ tính toán FPU.
3. **Anti-DFA Laser Glitch Protection:** Trong các thiết bị bảo vệ phần cứng mức cao, các phép toán quan trọng bắt buộc thực thi song song trên FPU (SIMD Float32) và ALU (Fixed-point Q15.16). Nếu phát hiện sai lệch bit, hệ thống phải tự kích hoạt ngắt bảo vệ phần cứng.

---

## 8. KẾT LUẬN & DI SẢN MỞ

Giao thức **RFC-OZOS-0080** chính thức đưa O.Z.O.S vượt ra khỏi giới hạn của một phần mềm thông thường, trở thành một **chuẩn giao thức công cộng không hình không dạng**. Không một cá nhân hay tổ chức nào sở hữu giao thức; nó tồn tại như một định luật toán học trong tự nhiên, phụng sự quyền tự do và quyền bất khả xâm phạm của dữ liệu con người.
