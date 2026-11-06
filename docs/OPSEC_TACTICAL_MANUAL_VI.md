# O.Z.O.S // CẨM NANG TÁC CHIẾN AN NINH & QUY CHUẨN THỰC ĐỊA (OPSEC-RFC-2026-VI)
**Mã tài liệu:** OZOS-DOC-OPSEC-01-VI  
**Phân loại:** Học Thuyết Tác Chiến Phòng Thủ Độc Bản  
**Tác giả:** LumenNull & Nhóm Chuyên Gia Kỹ Thuật O.Z.O.S  
**Đối tượng áp dụng:** Chỉ Huy Tác Chiến, Kỹ Sư Mật Mã, Quản Trị Viên Mạng Cách Ly (Air-Gap)  

---

## 1. Học Thuyết Cốt Lõi & Triết Lý Tác Chiến

Hệ điều hành Khiên Phòng Thủ Toàn Không (O.Z.O.S) vận hành dựa trên học thuyết **Không Tin Cậy Bất Kỳ Ai (Zero-Trust)** trong môi trường đối kháng khốc liệt:
1. **Mặc định Hệ Điều Hành Đã Bị Thỏa Hiệp:** Mọi hệ điều hành thương mại (Windows, Linux, macOS) đều bị coi là môi trường có thể đã bị cài cắm mã độc, keylogger, phần mềm gián điệp trích xuất bộ nhớ RAM hoặc chạy trong bẫy máy ảo giám sát.
2. **Mặc định Kênh Vật Lý Bị Nghe Lén:** Toàn bộ không gian âm học, quang học và dải tần vô tuyến đều là môi trường mở công khai, bất kỳ ai cũng có thể đặt máy thu hoặc máy phát nhiễu.
3. **Bất Biến Không Lưu Khóa Trên Ổ Đĩa (Zero-Disk Invariant):** Danh tính mật mã học tuyệt đối không bao giờ được phép lưu trữ tĩnh dưới dạng file trên ổ đĩa cứng. Khóa chỉ tồn tại dưới dạng vùng đệm RAM tạm thời được ghim không phân trang hoặc được phục hồi tức thời từ trí nhớ của người vận hành.

Tài liệu này cung cấp **Quy trình Vận hành Chuẩn (SOP)**, ma trận chuyển tầng kênh vật lý, giải pháp chống nhân bản máy ảo và quy trình tự hủy bộ nhớ khẩn cấp.

---

## 2. Ma Trận Kênh Vật Lý & Giao Thức Chuyển Tầng Nhận Thức

O.Z.O.S từ chối phụ thuộc vào bất kỳ một kênh truyền vật lý đơn lẻ nào. Khi kênh này bị nhiễu động, phá sóng hoặc giới hạn khoảng cách, người vận hành phải lập tức chuyển tầng sang modality khác trong module `ozos-phy.exe`.

```text
+---------------------------------------------------------------------------------------------------+
|                        MA TRẬN CHUYỂN TẦNG KÊNH TRUYỀN NHẬN THỨC (PHY)                            |
+------------------+-------------------+--------------------+-------------------+-------------------+
| TẦNG TRUYỀN DẪN  | MÔI TRƯỜNG        | CỰ LY HIỆU DỤNG    | ĐỘ TRỄ            | KHÁNG THỦ TRỢC DIỆN|
+------------------+-------------------+--------------------+-------------------+-------------------+
| Tier 1 (Quang)   | Ánh Sáng Nhìn Thấy| Tầm Nhìn Thẳng     | 16 ms (60fps)     | Phá Sóng Âm Học   |
| Tier 2 (OFDM)    | Cận Siêu Âm       | < 2.0 Mét          | < 36.6 ms         | Dội Âm Phòng Kín  |
| Tier 3 (LoRa)    | Sóng RF Sub-GHz   | 5 - 15 Kilomet     | 100 - 300 ms      | Xuyên Bê Tông/Hầm |
| Tier 4 (Skywave) | Sóng Ngắn Tầng Điện| Toàn Cầu/Xuyên Biển| 2 - 5 Giây        | Thảm Họa Đứt Cáp  |
+------------------+-------------------+--------------------+-------------------+-------------------+
```

### 2.1 Rào Cản Sóng Âm (< 2 Mét) & Cơ Chế Khắc Chế Dội Âm
* **Bản chất suy hao vật lý:** Sóng cận siêu âm (18.1 – 19.8 kHz) bị hấp thụ năng lượng rất nhanh trong không khí ($e^{-\alpha d}$), chỉ hoạt động tin cậy trong phạm vi $< 2\text{m}$. Ngoài ra, phòng kín có tường gạch hoặc kính phản xạ sẽ tạo ra hiện tượng dội âm giao thoa triệt tiêu pha.
* **Kỷ luật tác chiến sóng âm:**
  1. **Hiệu ứng Doppler:** Tuyệt đối không di chuyển thiết bị phát hoặc thu khi đang truyền. Sự chuyển động làm lệch tần số ra ngoài 32 bộ lọc Goertzel.
  2. **Khắc chế dội âm:** Nếu phòng kín gây tiếng vang, chuyển ngay từ FSK đơn kênh sang **Acoustic 64-FFT OFDM** (`ozos-phy ofdm-synth`). OFDM sử dụng 16 sóng mang con QPSK đồng thời, phát toàn bộ 80 byte trong **$< 36.6\text{ ms}$**—nhanh hơn thời gian dội âm của phòng.
  3. **Tự sửa sai lỗi cụm:** Thuật toán **Reed-Solomon RS(96, 80) FEC** tích hợp sẵn tự động phục hồi toàn vẹn gói tin kể cả khi tiếng vang làm mất tới 7 byte dữ liệu liên tiếp.

### 2.2 Chuyển Tầng Sang Quang Học (Optical Glyph)
* **Điều kiện kích hoạt:** Phòng quá ồn, có máy phá sóng âm thanh, hoặc bắt buộc phải giữ im lặng tuyệt đối (0 dB).
* **Lệnh thực thi:**
  ```bash
  # Bên Phát: Mã hóa ra file ảnh BMP hoặc hiển thị trực tiếp lên màn hình
  ozos-phy.exe glyph-encode <HEX_PAYLOAD> --bmp payload.bmp

  # Bên Thu: Quét và giải mã qua Camera
  ozos-phy.exe glyph-decode payload.bmp
  ```
* **Đặc tính kỹ thuật:** Ma trận động 32x32 pixel, thời gian quét 16ms/frame (chuẩn video 60fps), kiểm tra toàn vẹn bằng CRC32-IEEE. Miễn nhiễm 100% với máy nghe lén âm học.

### 2.3 Chuyển Tầng Sang LoRa Sub-GHz Xuyên Hầm / Cự Ly Xa
* **Điều kiện kích hoạt:** Khoảng cách vượt quá 2m, cần truyền xuyên tường bê tông cốt thép, hoặc không có đường ngắm quang học thẳng.
* **Lệnh thực thi:**
  ```bash
  # Bên Phát: Đóng gói khung dây LoRa 84 byte
  ozos-phy.exe lora-pack <HEX_PAYLOAD>

  # Bên Thu: Giải mã khung dây từ module radio
  ozos-phy.exe lora-unpack <CHUỖI_HEX_168_KÝ_TỰ>
  ```
* **Đặc tính kỹ thuật:** Hoạt động trên dải tần ISM 433 MHz hoặc 868 MHz. Cấu trúc khung: `[Sync 0x12 | Type 0x50 | 80B Payload | CRC16-CCITT]`. Cự ly 5 – 15 km hoàn toàn không cần Internet, SIM điện thoại hay trạm BTS.

---

## 3. Phòng Thủ Chống Máy Ảo (VM) & Chống Nhân Bản Danh Tính

Khi chạy O.Z.O.S trên các máy ảo đám mây hoặc hypervisor (VMware, VirtualBox, Hyper-V, KVM/QEMU), hệ thống đối mặt với 3 hiểm họa lớn:
* **Tấn công tua ngược Snapshot (Rollback Attack):** Kẻ tấn công chụp ảnh VM, sau đó tua ngược lại để phát lại các giao dịch cũ.
* **Nhân bản đĩa cứng ảo (Cloning Attack):** Copy file `.vmdk` hay `.qcow2` sang máy khác, tạo ra nhiều máy ảo giả mạo có cùng mã định danh `/etc/machine-id`.
* **Mất rễ phần cứng:** Máy ảo không có quyền truy cập trực tiếp vào chip silicon TPM 2.0 thật trên mainboard.

### 3.1 Quy Trình Tự Động Quét Hypervisor
Trước khi sinh khóa hoặc thực hiện bất kỳ giao dịch nào, bắt buộc phải chạy:
```bash
ozos-guard.exe workstation-shield
```
* **Cơ chế:** CPUID leaf `0x40000000` quét trực tiếp chữ ký hypervisor (`VMwareVMware`, `Microsoft Hv`, `KVMKVMKVM`, `XenVMMXenVMM`).
* **Cảnh báo:** Nếu phát hiện máy ảo, hệ thống cảnh báo đỏ: `[ALERT] YES (RUNNING UNDER HYPERVISOR)`.
* **Kỷ luật OpSec:** Khi có cảnh báo này, người vận hành tuyệt đối không được tin cậy vào bộ đếm thời gian hay hardware ID của máy đó.

### 3.2 Quy Chuẩn Khóa Vô Hình Trên Đĩa (Zero-Disk Key Protocol)
* **Điều 1:** Tuyệt đối không bao giờ tạo hoặc lưu private key hay file seed trên ổ cứng máy ảo.
* **Điều 2:** Danh tính mật mã chỉ được lưu trong não bộ người vận hành dưới dạng **12 từ khóa BIP-39 (Brainseed)**:
  ```bash
  # Phục hồi danh tính tức thời trực tiếp vào RAM
  ozos-core.exe brainseed-restore "abandon abandon ... about"
  ```
* **Điều 3:** Hệ thống tự động ghim vùng nhớ bằng `VirtualLock` (Windows) hoặc `mlock` (POSIX) để ngăn hypervisor hoán đổi (swap) dữ liệu nhạy cảm xuống file `pagefile.sys`.
* **Điều 4:** Ngay sau khi ký xong gói tin, chuỗi hàm tự hủy 3 bước (NIST SP 800-88: `0xAA -> 0x55 -> 0x00`) lập tức xóa sổ khóa trong vài microsecond. Dù kẻ xấu có copy snapshot đĩa ảo thì cũng chỉ thu được các mảng byte `0x00`.

### 3.3 Phân Rã Bí Mật Shamir (SSS) Đa Đám Mây
Để vận hành an toàn trên hạ tầng đám mây mà không phải tin tưởng bất kỳ nhà cung cấp dịch vụ nào:
1. Tạo khóa gốc và băm nhỏ theo sơ đồ ngưỡng $(3, 5)$:
   ```bash
   ozos-core.exe brainseed-split --threshold 3 --shares 5
   ```
2. Phân tán 5 mảnh khóa độc lập cho 5 máy ảo ở 5 nhà cung cấp khác nhau (ví dụ: AWS, Google Cloud, Azure, Hetzner, Máy trạm vật lý).
3. **Bảo mật tuyệt đối của Shannon (Perfect Secrecy):** Kẻ tấn công có hack hoặc clone được 1 hay 2 máy ảo thì cũng chỉ có tối đa 2 mảnh. Về mặt toán học trên trường $\text{GF}(2^8)$, $k-1$ mảnh tiết lộ **chính xác 0 bit thông tin** về khóa gốc.
4. **Hợp nhất quyền lực:** Chỉ khi tập hợp đủ tối thiểu 3 mảnh trong RAM thì danh tính mới được tái sinh:
   ```bash
   ozos-core.exe brainseed-combine <MẢNH_1> <MẢNH_2> <MẢNH_3>
   ```

### 3.4 Thoát Ly Khỏi Máy Ảo Bằng `ozos-core` Trên seL4 / STM32
Đối với các nhiệm vụ quốc phòng hoặc hạ tầng trọng yếu yêu cầu miễn nhiễm 100% với máy ảo:
* Sử dụng module độc lập **`ozos-core.exe`** (214 KB, `malloc = 0`, stack-only).
* Nạp trực tiếp lên vi điều khiển silicon thật (STM32, ESP32, RISC-V) hoặc chạy trong phân vùng người dùng của vi nhân **seL4 Microkit** đã được kiểm chứng toán học.
* Triệt tiêu hoàn toàn hệ điều hành trung gian và hypervisor, biến các cuộc tấn công sao chép máy ảo thành vô nghĩa.

---

## 4. Phòng Thủ Máy Trạm & Quy Trình Chống Điều Tra Số (Anti-Forensics)

### 4.1 Khiên Bộ Nhớ DACL Chống Dump RAM
Mã độc chạy cùng máy trạm (như phần mềm gián điệp, Mimikatz, Process Hacker) thường tìm cách mở quyền `PROCESS_VM_READ` để soi RAM.
* O.Z.O.S tự tiêm danh sách điều khiển truy cập nghiêm ngặt (DACL) vào tiến trình qua hàm `ozos_harden_process_dacl()`.
* Tước bỏ toàn bộ quyền đọc/ghi/debug của các tiến trình lạ.
* Kích hoạt chính sách `PROCESS_MITIGATION_DYNAMIC_CODE_POLICY` chặn đứng hành vi chèn shellcode động vào tiến trình.

### 4.2 Giáp Bảo Vệ Bộ Nhớ Đệm Clipboard Windows
Tin tặc thường giám sát Clipboard để bắt trộm ảnh QR code hoặc khóa bí mật khi người dùng nhấn Copy.
```bash
ozos-guard.exe clipboard-armor
```
* Bắt chặn sự kiện Clipboard `CF_DIB` trong thời gian $< 5\text{ms}$.
* Tự động cấy thủy vân số 2D-DCT vô hình vào ảnh trong RAM trước khi bất kỳ phần mềm gián điệp nào kịp sao chép ảnh gốc.

### 4.3 Tự Hủy Phần Cứng Chuẩn NIST SP 800-88 (`burn`)
Để tiêu hủy hoàn toàn các tệp tin nhạy cảm trước khi rơi vào tay đối phương:
```bash
ozos-guard.exe burn <ĐƯỜNG_DẪN_FILE>
```
* **Vòng 1:** Ghi đè toàn bộ cung từ/sector vật lý bằng mẫu byte `0xAA`, gọi lệnh xả đĩa `FlushFileBuffers`.
* **Vòng 2:** Ghi đè bằng mẫu đảo ngược `0x55`, gọi lệnh xả đĩa.
* **Vòng 3:** Ghi đè bằng byte `0x00`, gọi lệnh xả đĩa.
* **Hủy liên kết:** Xóa tên file và gỡ bỏ hoàn toàn khỏi bảng chỉ mục hệ thống tập tin (File Allocation Table).

### 4.4 Mỏ Neo Cây Merkle & Hầm Chứa Bí Mật Enclave
* **Kiểm định toàn vẹn thư mục dự án:**
  ```bash
  ozos-guard.exe anchor /duong/dan/thu/muc
  ozos-guard.exe anchor-verify /duong/dan/thu/muc
  ```
* **Đóng gói hầm chứa mã hóa RAM:**
  ```bash
  # Đóng gói thư mục thành file hầm bảo mật (XSalsa20-Poly1305)
  ozos-guard.exe vault pack ./thu_muc_mat ./ham_chua.ozos

  # Trích xuất trực tiếp trong RAM không để lại dấu vết đĩa
  ozos-guard.exe vault open ./ham_chua.ozos ./thu_muc_giai_nen
  ```

---

## 5. Vận Hành Phòng Thủ Hậu Lượng Tử (PQC)

Máy tính lượng tử trong tương lai với thuật toán Shor có thể bẻ gãy các đường cong Elliptic truyền thống (Ed25519, X25519).
* O.Z.O.S tích hợp sẵn module độc lập **`ozos-pqc.exe`** đáp ứng tiêu chuẩn của Viện Tiêu chuẩn và Kỹ thuật Quốc gia Hoa Kỳ: NIST FIPS 203 (ML-KEM-768 / Kyber) và FIPS 204 (ML-DSA-65 / Dilithium).
* **Mô hình bắt tay lai ghép (Hybrid Handshake):**
  - Tính toán khóa bí mật cổ điển ($K_c$) qua X25519.
  - Tính toán khóa bí mật kháng lượng tử ($K_q$) qua ML-KEM-768.
  - Phối trộn hai khóa qua hàm KDF SHA-512:
    $$K_{\text{session}} = \text{SHA-512}(K_c \mathbin{\Vert} K_q)$$
* **Lệnh kiểm tra:**
  ```bash
  ozos-pqc.exe status
  ozos-pqc.exe test
  ```
  Bảo đảm dữ liệu được mã hóa hôm nay vẫn an toàn tuyệt đối trước máy tính lượng tử trong 30 năm tới.

---

## 6. Bảng Quy Trình Vận Hành Chuẩn (SOP Checklist)

### Quy Trình Trước Khi Xuất Kích (Pre-Deployment Checklist)
- [ ] Chạy `ozos-guard workstation-shield` kiểm tra môi trường: Cảnh giác nếu có cờ máy ảo `[ALERT]`.
- [ ] Quét mức độ phơi nhiễm mạng Wi-Fi/Ethernet. Nếu có mạng ngoài, cách ly vật lý hoặc bật Virtual Air-Gap.
- [ ] Bảo đảm không có bất kỳ file private key nào được ghi xuống ổ đĩa cứng.
- [ ] Học thuộc lòng hoặc phân rã 12 từ khóa Brainseed bằng Shamir SSS $(3, 5)$.
- [ ] Chạy `build.bat test` xác nhận 5/5 bài kiểm tra đều vượt qua 100%.

### Quy Trình Trong Khi Truyền Tin (In-Flight Checklist)
- [ ] Khoảng cách $< 2\text{m}$ và phòng yên tĩnh: Sử dụng `ozos-phy audio-synth` hoặc `ozos-phy ofdm-synth`.
- [ ] Phòng ồn ào hoặc bị phá sóng âm: Lập tức chuyển sang `ozos-phy glyph-encode` (Optical 32x32).
- [ ] Khoảng cách xa hoặc xuyên tường: Chuyển sang `ozos-phy lora-pack` (LoRa Sub-GHz).
- [ ] Đối chiếu khóa công khai của đối tác bằng mắt hoặc giọng nói ngoài đời thực để chống tấn công Man-in-the-Middle.

### Quy Trình Khẩn Cấp Khi Bị Đột Kích / Thỏa Hiệp (Emergency Breach SOP)
1. **Phát tín hiệu tự hủy RAM tức thì:** Gửi tín hiệu `SIGSEGV` hoặc chạy `ozos-core crash-test` để hàm `ozos_posix_signal_handler` xóa sạch các vùng RAM nhạy cảm đã đăng ký.
2. **Hủy diệt dữ liệu đĩa:** Chạy `ozos-guard burn` tiêu hủy toàn bộ file hầm `.ozos` và file mỏ neo `.ozos_anchor.json`.
3. **Thu hồi danh tính:** Nếu nghi ngờ một mảnh Shamir bị lộ, tập hợp 3 mảnh còn lại trên một thiết bị bare-metal sạch, sinh ra khóa gốc mới và phân rã lại vào các vùng an toàn mới.
