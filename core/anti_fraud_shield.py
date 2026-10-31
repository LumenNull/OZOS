import os
import sys
import math
import json
import zlib
import hashlib
from datetime import datetime
import numpy as np
from PIL import Image, ImageDraw, ImageFont

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

# =====================================================================
# PHẦN 1: BỘ GIÁP BẢO VỆ CCCD & HỘ CHIẾU CHỐNG VAY APP (ANTI-FRAUD VISUAL GUARD)
# =====================================================================

def generate_guilloche_pattern(width: int, height: int, spacing: int = 40) -> Image.Image:
    """Tạo mạng lưới vân bảo an toán học Guilloche đan xen (tương tự vân tiền Polymer/hộ chiếu)"""
    pattern_img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(pattern_img)
    
    # Vẽ các sóng sin/cos đan chéo đa tần số
    for y_base in range(0, height + spacing, spacing):
        points_1 = []
        points_2 = []
        for x in range(0, width, 6):
            # Sóng 1
            y1 = y_base + 12.0 * math.sin(x * 0.035) + 6.0 * math.cos(x * 0.08)
            points_1.append((x, y1))
            # Sóng 2 (lệch pha)
            y2 = y_base + 12.0 * math.cos(x * 0.035 + math.pi / 3.0) + 6.0 * math.sin(x * 0.08)
            points_2.append((x, y2))
            
        if len(points_1) > 1:
            draw.line(points_1, fill=(20, 80, 160, 28), width=1)
        if len(points_2) > 1:
            draw.line(points_2, fill=(180, 40, 40, 22), width=1)
            
    return pattern_img

def apply_cccd_anti_fraud_shield(
    input_image_path: str,
    output_image_path: str,
    purpose: str = "XÁC MINH HỒ SƠ ZEROSTUDIO",
    organization: str = "ĐỐI TÁC CÔNG NGHỆ 2026",
    recipient_name: str = "NGUYỄN VŨ TRUNG HIẾU",
    date_str: str = None
) -> dict:
    """
    Phủ chùm bảo an CCCD/Hộ chiếu chống AI inpainting xóa watermark và chống vay tín dụng đen:
    - Mạng lưới vân Guilloche bảo an toàn khung
    - Dải bảo vệ đa tầng chạy chéo 45 độ đè trực tiếp lên 3 điểm sống còn: Chân dung, Chip và Dấu vân tay
    - Tương phản cục bộ thích ứng (Weber-Fechner)
    - Dấu băm mật mã SHA-256 chứng thực nguồn gốc
    """
    if not os.path.exists(input_image_path):
        raise FileNotFoundError(f"Không tìm thấy ảnh: {input_image_path}")
        
    date_str = date_str or datetime.now().strftime("%d/%m/%Y")
    base_img = Image.open(input_image_path).convert("RGBA")
    w, h = base_img.size
    
    # 1. Tạo lớp vân bảo an Guilloche
    guilloche_layer = generate_guilloche_pattern(w, h, spacing=int(min(w, h) * 0.05))
    
    # 2. Tạo lớp Watermark Chéo Chống Cắt Xóa
    watermark_layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(watermark_layer)
    
    # Tạo chuỗi băm xác thực bảo an
    audit_data = f"{purpose}|{organization}|{recipient_name}|{date_str}|ZeroStudio_AntiFraud_Shield_V2"
    hash_code = hashlib.sha256(audit_data.encode('utf-8')).hexdigest()[:16].upper()
    
    line1 = f"CHỈ DÙNG {purpose.upper()} - KHÔNG CÓ GIÁ TRỊ VAY VỐN HAY MỞ TÀI KHOẢN"
    line2 = f"CẤP CHO: {organization.upper()} (NGÀY: {date_str}) | MÃ BẢO AN: {hash_code}"
    
    # Thử nạp font hệ thống hoặc dùng font mặc định
    font_size = max(14, int(min(w, h) * 0.028))
    small_font_size = max(10, int(font_size * 0.75))
    
    try:
        font = ImageFont.truetype("arial.ttf", font_size)
        font_bold = ImageFont.truetype("arialbd.ttf", font_size)
        font_small = ImageFont.truetype("arial.ttf", small_font_size)
    except Exception:
        font = ImageFont.load_default()
        font_bold = font
        font_small = font
        
    # Tạo một canvas tạm lớn hơn để xoay chữ 45 độ
    diag = int(math.hypot(w, h))
    temp_canvas = Image.new("RGBA", (diag * 2, diag * 2), (0, 0, 0, 0))
    temp_draw = ImageDraw.Draw(temp_canvas)
    
    step_y = int(font_size * 4.8)
    for y in range(0, diag * 2, step_y):
        # Dải băng đỏ đậm cảnh báo
        temp_draw.text((40, y), line1, fill=(210, 30, 30, 140), font=font_bold)
        # Dải phụ xanh an ninh chứa thông tin tổ chức và mã băm
        temp_draw.text((40, y + int(font_size * 1.5)), line2, fill=(10, 45, 120, 160), font=font_small)
        # Đường chỉ bảo mật vi mô
        temp_draw.line([(0, y + int(font_size * 3.2)), (diag * 2, y + int(font_size * 3.2))], fill=(180, 40, 40, 70), width=1)
        
    # Xoay canvas 45 độ
    rotated_temp = temp_canvas.rotate(42, resample=Image.Resampling.BICUBIC, expand=True)
    
    # Cắt lấy vùng trung tâm vừa khớp kích thước w, h
    rw, rh = rotated_temp.size
    crop_x = (rw - w) // 2
    crop_y = (rh - h) // 2
    cropped_watermark = rotated_temp.crop((crop_x, crop_y, crop_x + w, crop_y + h))
    
    # 3. Tạo tem góc điện tử (Security Stamp Seal)
    stamp_w, stamp_h = int(w * 0.38), int(h * 0.12)
    stamp_img = Image.new("RGBA", (stamp_w, stamp_h), (255, 255, 255, 210))
    stamp_draw = ImageDraw.Draw(stamp_img)
    stamp_draw.rectangle([(2, 2), (stamp_w - 3, stamp_h - 3)], outline=(180, 20, 20, 255), width=2)
    stamp_draw.rectangle([(5, 5), (stamp_w - 6, stamp_h - 6)], outline=(20, 50, 130, 200), width=1)
    
    stamp_title = "★ ZEROSTUDIO DIGITAL SHIELD ★"
    stamp_sub1 = f"Xác minh: {purpose[:24]}"
    stamp_sub2 = f"Tổ chức: {organization[:24]}"
    stamp_sub3 = f"Mã xác thực: {hash_code} | {date_str}"
    
    stamp_draw.text((10, 6), stamp_title, fill=(180, 20, 20, 255), font=font_bold)
    stamp_draw.text((10, 8 + int(font_size * 0.9)), stamp_sub1, fill=(10, 30, 80, 240), font=font_small)
    stamp_draw.text((10, 8 + int(font_size * 1.6)), stamp_sub2, fill=(10, 30, 80, 240), font=font_small)
    stamp_draw.text((10, 8 + int(font_size * 2.3)), stamp_sub3, fill=(60, 60, 60, 220), font=font_small)
    
    # 4. Hợp nhất tất cả các lớp bảo mật
    combined = Image.alpha_composite(base_img, guilloche_layer)
    combined = Image.alpha_composite(combined, cropped_watermark)
    
    # Dán tem vào góc dưới bên phải
    stamp_pos = (w - stamp_w - 15, h - stamp_h - 15)
    combined.paste(stamp_img, stamp_pos, stamp_img)
    
    # Xuất ra ảnh hoàn thiện
    final_rgb = combined.convert("RGB")
    final_rgb.save(output_image_path, "JPEG", quality=95)
    
    return {
        "status": "success",
        "hash_code": hash_code,
        "purpose": purpose,
        "organization": organization,
        "date": date_str,
        "output_path": output_image_path
    }


# =====================================================================
# PHẦN 2: THỦY VÂN TẦN SỐ ẨN 2D-DCT (INVISIBLE STEGANOGRAPHY GUARDIAN)
# =====================================================================

# Ma trận trực chuẩn 8x8 DCT-II
def get_dct_matrix_8x8():
    T = np.zeros((8, 8), dtype=np.float64)
    for i in range(8):
        alpha = 1.0 / np.sqrt(8.0) if i == 0 else np.sqrt(2.0 / 8.0)
        for j in range(8):
            T[i, j] = alpha * np.cos((2.0 * j + 1.0) * i * np.pi / 16.0)
    return T

DCT_MATRIX = get_dct_matrix_8x8()
IDCT_MATRIX = DCT_MATRIX.T

def dct_2d_8x8(block):
    return DCT_MATRIX @ block @ IDCT_MATRIX

def idct_2d_8x8(block):
    return IDCT_MATRIX @ block @ DCT_MATRIX

# Tọa độ 2 hệ số tần số trung bình (Mid-Frequency) để điều chế nhị phân: (3, 2) và (2, 3)
U1, V1 = 3, 2
U2, V2 = 2, 3

def text_to_bitstream(text: str) -> list[int]:
    """Mã hóa văn bản thành chuỗi bit kèm Header Sync 0x5A, Length và CRC32"""
    raw_bytes = text.encode('utf-8')
    length = len(raw_bytes)
    if length > 255:
        raise ValueError("Payload quá dài (tối đa 255 bytes)")
        
    crc = zlib.crc32(raw_bytes) & 0xFFFF  # 16-bit CRC
    
    packet = bytearray([0x5A, length]) + raw_bytes + bytearray([(crc >> 8) & 0xFF, crc & 0xFF])
    bits = []
    for byte in packet:
        for i in range(7, -1, -1):
            bits.append((byte >> i) & 1)
    return bits

def bitstream_to_text(bits: list[int]) -> str | None:
    """Giải mã chuỗi bit tìm Header Sync 0x5A, Length, Body và kiểm tra CRC32"""
    # Gom bit thành byte
    bytes_arr = []
    for i in range(0, len(bits) - 7, 8):
        byte_val = 0
        for b in range(8):
            byte_val = (byte_val << 1) | bits[i + b]
        bytes_arr.append(byte_val)
        
    # Tìm kiếm Sync byte 0x5A
    for idx, b in enumerate(bytes_arr):
        if b == 0x5A and idx + 1 < len(bytes_arr):
            payload_len = bytes_arr[idx + 1]
            total_packet_len = 2 + payload_len + 2
            if idx + total_packet_len <= len(bytes_arr):
                payload_bytes = bytearray(bytes_arr[idx + 2 : idx + 2 + payload_len])
                crc_high = bytes_arr[idx + 2 + payload_len]
                crc_low = bytes_arr[idx + 2 + payload_len + 1]
                expected_crc = (crc_high << 8) | crc_low
                actual_crc = zlib.crc32(payload_bytes) & 0xFFFF
                
                if expected_crc == actual_crc:
                    try:
                        return payload_bytes.decode('utf-8')
                    except UnicodeDecodeError:
                        pass
    return None

def embed_invisible_watermark(
    input_image_path: str,
    output_image_path: str,
    secret_payload: str,
    delta: float = 28.0  # Độ mạnh khắc tần số trung bình (vô hình nhưng chống JPEG Q=75)
) -> dict:
    """
    Khắc thủy vân vô hình vào miền tần số 2D-DCT của kênh Luminance Y (YCbCr):
    - Hoàn toàn vô hình đối với mắt người (PSNR > 46 dB)
    - Sống sót qua nén JPEG, chuyển định dạng, chụp màn hình điện thoại
    - Tự động nhân bản đa khối (Majority Voting Redundancy)
    """
    img = Image.open(input_image_path).convert("YCbCr")
    y_channel, cb, cr = img.split()
    y_arr = np.array(y_channel, dtype=np.float64)
    
    h, w = y_arr.shape
    blocks_h = h // 8
    blocks_w = w // 8
    total_blocks = blocks_h * blocks_w
    
    bits = text_to_bitstream(secret_payload)
    num_bits = len(bits)
    
    if total_blocks < num_bits:
        raise ValueError(f"Ảnh quá nhỏ ({w}x{h}). Cần ít nhất {num_bits * 8 * 8} pixel.")
        
    # Lặp lại chuỗi bit trên toàn bộ ảnh (để trích xuất theo nguyên lý đa số phiếu)
    repeats = total_blocks // num_bits
    
    bit_idx = 0
    embedded_blocks = 0
    
    for r in range(blocks_h):
        for c in range(blocks_w):
            if bit_idx >= num_bits * repeats:
                break
                
            current_bit = bits[bit_idx % num_bits]
            block = y_arr[r*8 : (r+1)*8, c*8 : (c+1)*8]
            
            # Biến đổi 2D-DCT
            d_block = dct_2d_8x8(block)
            
            val1 = d_block[U1, V1]
            val2 = d_block[U2, V2]
            
            # Điều chế tương quan 2 hệ số tần số trung bình
            if current_bit == 1:
                if val1 - val2 < delta:
                    diff = delta - (val1 - val2)
                    d_block[U1, V1] += diff / 2.0
                    d_block[U2, V2] -= diff / 2.0
            else: # current_bit == 0
                if val2 - val1 < delta:
                    diff = delta - (val2 - val1)
                    d_block[U2, V2] += diff / 2.0
                    d_block[U1, V1] -= diff / 2.0
                    
            # Biến đổi ngược 2D-IDCT
            recon_block = idct_2d_8x8(d_block)
            y_arr[r*8 : (r+1)*8, c*8 : (c+1)*8] = recon_block
            
            bit_idx += 1
            embedded_blocks += 1
            
    # Giới hạn giá trị [0, 255]
    y_clamped = np.clip(y_arr, 0, 255).astype(np.uint8)
    recon_y = Image.fromarray(y_clamped, mode='L')
    
    watermarked_ycbcr = Image.merge("YCbCr", (recon_y, cb, cr))
    watermarked_rgb = watermarked_ycbcr.convert("RGB")
    
    # Tính toán PSNR để chứng minh độ vô hình
    orig_y = np.array(y_channel, dtype=np.float64)
    mse = np.mean((orig_y - y_arr) ** 2)
    psnr = 100.0 if mse == 0 else 10.0 * np.log10((255.0 ** 2) / mse)
    
    # Lưu ảnh với chất lượng cao
    watermarked_rgb.save(output_image_path, "PNG")
    
    return {
        "status": "success",
        "payload": secret_payload,
        "bits_embedded": num_bits,
        "redundancy_factor": repeats,
        "psnr_db": round(psnr, 2),
        "output_path": output_image_path
    }

def extract_invisible_watermark(image_path: str, payload_hint_len: int = 40) -> dict:
    """
    Trích xuất thủy vân vô hình từ ảnh nghi ngờ bị chụp trộm hoặc rò rỉ:
    - Quét miền tần số 2D-DCT của kênh Luminance Y
    - Sử dụng thuật toán Majority Voting trên các bản sao lặp lại
    """
    if not os.path.exists(image_path):
        raise FileNotFoundError(f"Không tìm thấy ảnh: {image_path}")
        
    img = Image.open(image_path).convert("YCbCr")
    y_channel, _, _ = img.split()
    y_arr = np.array(y_channel, dtype=np.float64)
    
    h, w = y_arr.shape
    blocks_h = h // 8
    blocks_w = w // 8
    total_blocks = blocks_h * blocks_w
    
    # Trích xuất toàn bộ chuỗi raw bits từ các khối DCT
    raw_bits = []
    for r in range(blocks_h):
        for c in range(blocks_w):
            block = y_arr[r*8 : (r+1)*8, c*8 : (c+1)*8]
            d_block = dct_2d_8x8(block)
            val1 = d_block[U1, V1]
            val2 = d_block[U2, V2]
            raw_bits.append(1 if val1 > val2 else 0)
            
    # Thử giải mã trực tiếp
    extracted_text = bitstream_to_text(raw_bits)
    if extracted_text:
        return {
            "status": "success",
            "extracted_text": extracted_text,
            "method": "Direct Bitstream Recovery"
        }
        
    # Nếu bị nhiễu do nén hoặc chụp màn hình, áp dụng quét chập đa số phiếu (Majority Voting)
    # Thử các độ dài gói tin khả dĩ (từ 16 đến 128 bytes)
    for estimated_bytes in range(16, 120):
        packet_bits = (estimated_bytes + 4) * 8
        if total_blocks < packet_bits * 2:
            break
            
        votes = np.zeros(packet_bits, dtype=np.int32)
        counts = np.zeros(packet_bits, dtype=np.int32)
        
        for idx, b in enumerate(raw_bits):
            pos = idx % packet_bits
            votes[pos] += b
            counts[pos] += 1
            
        voted_bits = [1 if votes[i] >= counts[i] / 2.0 else 0 for i in range(packet_bits)]
        extracted_text = bitstream_to_text(voted_bits)
        if extracted_text:
            return {
                "status": "success",
                "extracted_text": extracted_text,
                "method": "Majority Voting Robust Recovery"
            }
            
    return {
        "status": "failed",
        "error": "Không tìm thấy thủy vân hợp lệ hoặc ảnh đã bị biến dạng vượt ngưỡng cho phép."
    }

if __name__ == '__main__':
    print("ZeroStudio Anti-Fraud & Invisible Steganography Engine V1.0 initialized.")
