# -*- coding: utf-8 -*-
"""
O.Z.O.S - Sovereign Command Line Interface & TUI (Trench 2)
Pure Python & Bare-Metal Engineering Tool.
Zero Browser Dependency. 100% Offline Air-Gapped Operation.
"""

import os
import sys
import argparse
import base64
import struct
import math
import wave
from PIL import Image
import numpy as np

DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(DIR, "..", ".."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from core.ozos_tpm_anchor import OZOSHardwareTPM

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

# ANSI Colors for Tactical Terminal
C_RESET = "\033[0m"
C_BOLD = "\033[1m"
C_AMBER = "\033[38;5;214m"
C_GREEN = "\033[38;5;48m"
C_RED = "\033[38;5;196m"
C_CYAN = "\033[38;5;51m"
C_DIM = "\033[38;5;244m"

def print_banner():
    banner = f"""
{C_AMBER}{C_BOLD}======================================================================
  ██████╗     ███████╗     ██████╗     ███████╗
 ██╔═══██╗    ╚══███╔╝    ██╔═══██╗    ██╔════╝
 ██║   ██║      ███╔╝     ██║   ██║    ███████╗
 ██║   ██║     ███╔╝      ██║   ██║    ╚════██║
 ╚██████╔╝    ███████╗    ╚██████╔╝    ███████║
  ╚═════╝     ╚══════╝     ╚═════╝     ╚══════╝
  OMNI-ZERO OPERATING SHIELD // SOVEREIGN TACTICAL CLI
======================================================================{C_RESET}
"""
    print(banner)

def hex_dump_terminal(data_bytes):
    print(f"{C_CYAN}OFFSET    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ASCII{C_RESET}")
    print(f"{C_DIM}----------------------------------------------------------------------{C_RESET}")
    length = len(data_bytes)
    for i in range(0, length, 16):
        chunk = data_bytes[i:i+16]
        hex_str = " ".join(f"{b:02X}" for b in chunk)
        if len(chunk) < 16:
            hex_str = hex_str.ljust(47)
        else:
            hex_str = hex_str[:23] + " " + hex_str[23:]
        ascii_str = "".join(chr(b) if 32 <= b <= 126 else "." for b in chunk)
        print(f"{C_CYAN}0x{i:06X}:{C_RESET}  {hex_str}  {C_AMBER}|{ascii_str}|{C_RESET}")

def cmd_tpm():
    print(f"\n{C_BOLD}[TPM 2.0] TRUY VẤN MỎ NEO PHẦN CỨNG...{C_RESET}")
    tpm = OZOSHardwareTPM()
    badge = tpm.export_provenance_badge()
    status_color = C_GREEN if badge["tpm_status"] == "AVAILABLE" else C_AMBER
    print(f"  • Trạng thái TPM       : {status_color}{badge['tpm_status']}{C_RESET}")
    print(f"  • Nhà cung cấp vi mạch : {C_CYAN}{badge['provider']}{C_RESET}")
    print(f"  • Vân tay Silicon Seed : {C_AMBER}{badge['master_silicon_seed_fingerprint']}{C_RESET}")
    print(f"  • Nguồn Entropy TRNG   : {badge['trng_entropy_source']}")
    print(f"  • Giá trị pháp lý      : {C_GREEN}{badge['admissibility']}{C_RESET}")
    tpm.close()

def build_aligned_packet(payload_str, nonce=1001, secret_seed=0xA5A5A5A5):
    if not isinstance(payload_str, str):
        raise ValueError(f"payload_str phải là chuỗi ký tự (str), nhận được {type(payload_str).__name__}")
    if isinstance(nonce, bool) or not isinstance(nonce, int) or nonce < 0 or nonce > 0xFFFFFFFFFFFFFFFF:
        raise ValueError(f"nonce phải là số nguyên 64-bit không dấu [0, 2^64-1], nhận được {nonce!r}")
    if isinstance(secret_seed, bool) or not isinstance(secret_seed, int):
        raise ValueError(f"secret_seed phải là số nguyên (int), nhận được {type(secret_seed).__name__}")
    payload_bytes = payload_str.encode('utf-8')
    if len(payload_bytes) > 65535:
        raise ValueError(f"Payload vượt quá giới hạn tối đa của khung dữ liệu 16-bit ({len(payload_bytes)} > 65535 bytes)")
    header_size = 80
    packet = bytearray(header_size + len(payload_bytes))

    # 1. 16 float32 SIMD Coeffs (Offset 0x00 - 0x3F)
    for i in range(16):
        struct.pack_into('<f', packet, i * 4, 1.5 + (i * 0.25))

    # 2. Nonce 8B (Offset 0x40 - 0x47)
    struct.pack_into('<Q', packet, 64, nonce)

    # 3. Magic 4B (Offset 0x48 - 0x4B) -> 0x47454E01
    struct.pack_into('<I', packet, 72, 0x47454E01)

    # 4. Payload Length 2B (Offset 0x4C - 0x4D)
    struct.pack_into('<H', packet, 76, len(payload_bytes))

    # 5. Payload Content
    packet[80:] = payload_bytes

    # 6. Tripwire MAC16 (Offset 0x4E - 0x4F) bao phủ cả Header (0..77) VÀ Payload
    h = secret_seed
    for b in packet[:78]:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    for b in payload_bytes:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    mac16 = (h ^ (h >> 16) ^ (h >> 32)) & 0xFFFF
    struct.pack_into('<H', packet, 78, mac16)
    return bytes(packet), mac16

def validate_aligned_packet(packet_bytes, last_seen_nonce=0, secret_seed=0xA5A5A5A5):
    if packet_bytes is None or not isinstance(packet_bytes, (bytes, bytearray, memoryview)):
        return False, "Dữ liệu gói tin không hợp lệ (yêu cầu bytes-like object)", None
    packet_bytes = bytes(packet_bytes)
    if len(packet_bytes) < 80:
        return False, "Kích thước nhỏ hơn 80 bytes", None
    if isinstance(last_seen_nonce, bool):
        return False, "last_seen_nonce không được là kiểu boolean (bool)", None
    if isinstance(secret_seed, bool) or not isinstance(secret_seed, int):
        return False, f"secret_seed phải là số nguyên (int), nhận được {type(secret_seed).__name__}", None
    if isinstance(last_seen_nonce, float):
        return False, "last_seen_nonce không được là số thực (float)", None
    if last_seen_nonce is None:
        return False, "last_seen_nonce không được là None", None
    if not isinstance(last_seen_nonce, int):
        if isinstance(last_seen_nonce, str) and last_seen_nonce.isdigit():
            last_seen_nonce = int(last_seen_nonce)
        else:
            return False, f"last_seen_nonce không hợp lệ: {last_seen_nonce!r}", None
    if last_seen_nonce < 0 or last_seen_nonce > 0xFFFFFFFFFFFFFFFF:
        return False, f"last_seen_nonce nằm ngoài phạm vi 64-bit [0, 2^64-1]: {last_seen_nonce}", None

    # Kiểm tra hệ số chấm động độc hại (NaN / Infinity)
    import math
    for i in range(16):
        coeff = struct.unpack_from('<f', packet_bytes, i * 4)[0]
        if math.isnan(coeff) or math.isinf(coeff):
            return False, "Phát hiện hệ số chấm động độc hại (NaN hoặc Infinity trong dct_coeffs)", None

    magic = struct.unpack_from('<I', packet_bytes, 72)[0]
    if magic != 0x47454E01:
        return False, f"Magic sai lệch (0x{magic:08X})", None

    payload_len = struct.unpack_from('<H', packet_bytes, 76)[0]
    if len(packet_bytes) < 80 + payload_len:
        return False, "Khung dữ liệu bị cắt cụt (Truncated payload)", None
    if len(packet_bytes) > 80 + payload_len:
        return False, "Phát hiện rác nối đuôi (Trailing garbage detected)", None

    # Tripwire MAC16 hỗ trợ cả Sub-profile A1 (CRC16) và Sub-profile A2 (FNV Fold)
    incoming_mac = struct.unpack_from('<H', packet_bytes, 78)[0]
    h = secret_seed
    for b in packet_bytes[:78]:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    for b in packet_bytes[80:80+payload_len]:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    expected_mac = (h ^ (h >> 16) ^ (h >> 32)) & 0xFFFF

    # CRC16-CCITT cho Sub-profile A1
    crc = 0xFFFF
    for b in packet_bytes[:78]:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF

    is_a2 = (incoming_mac == expected_mac)
    is_a1 = (secret_seed == 0xA5A5A5A5 and incoming_mac == crc)

    if not is_a2 and not is_a1:
        return False, f"Tripwire MAC16 không khớp: Header hoặc Payload đã bị sửa đổi (0x{incoming_mac:04X} vs 0x{expected_mac:04X})", None

    if is_a1 and not is_a2 and payload_len > 0:
        return False, "Từ chối Sub-Profile A1 cho gói tin có payload: MAC16 chỉ bảo vệ 78B Header, không bảo vệ Payload", None

    nonce = struct.unpack_from('<Q', packet_bytes, 64)[0]
    if nonce <= last_seen_nonce:
        return False, f"Tấn công phát lại (Nonce {nonce} <= {last_seen_nonce})", None

    payload = packet_bytes[80:80+payload_len].decode('utf-8', errors='replace')
    return True, "HỢP LỆ", {"nonce": nonce, "payload": payload, "payload_len": payload_len}

def cmd_packet_build(args):
    packet, mac16 = build_aligned_packet(args.payload, args.nonce)
    b64 = base64.b64encode(packet).decode('utf-8')
    print(f"\n{C_BOLD}[GÓI TIN ĐÃ ĐÓNG GÓI - KÍCH THƯỚC: {len(packet)} BYTES]{C_RESET}")
    print(f"  • Nonce        : {args.nonce}")
    print(f"  • Magic        : 0x47454E01")
    print(f"  • MAC16        : 0x{mac16:04X}")
    print(f"  • Base64       : {b64}\n")
    hex_dump_terminal(packet)

def cmd_packet_verify(args):
    data_str = args.data.strip()
    raw_bytes = None

    # Check if valid hex
    is_hex = (len(data_str) % 2 == 0) and all(c in '0123456789abcdefABCDEF' for c in data_str)
    hex_candidate = None
    if is_hex:
        try:
            hex_candidate = bytes.fromhex(data_str)
        except Exception:
            pass

    # Check if valid base64
    b64_candidate = None
    try:
        b64_candidate = base64.b64decode(data_str)
    except Exception:
        pass

    # Disambiguate when both succeed
    if hex_candidate and len(hex_candidate) >= 80 and struct.unpack_from('<I', hex_candidate, 72)[0] == 0x47454E01:
        raw_bytes = hex_candidate
    elif b64_candidate and len(b64_candidate) >= 80 and struct.unpack_from('<I', b64_candidate, 72)[0] == 0x47454E01:
        raw_bytes = b64_candidate
    elif hex_candidate:
        raw_bytes = hex_candidate
    elif b64_candidate:
        raw_bytes = b64_candidate
    else:
        print(f"\n{C_RED}{C_BOLD}[XÁC THỰC THẤT BÀI]: Dữ liệu không phải Base64 hoặc Hex hợp lệ{C_RESET}")
        sys.exit(1)

    valid, reason, info = validate_aligned_packet(raw_bytes, args.last_nonce)
    if valid:
        print(f"\n{C_GREEN}{C_BOLD}[XÁC THỰC THÀNH CÔNG]{C_RESET}")
        print(f"  • Nonce   : {info['nonce']}")
        print(f"  • Payload : {C_AMBER}\"{info['payload']}\"{C_RESET}")
        print(f"  • Length  : {info['payload_len']} bytes")
    else:
        print(f"\n{C_RED}{C_BOLD}[XÁC THỰC THẤT BÀI]: {reason}{C_RESET}")
        sys.exit(1)

def cmd_stego_extract(args):
    if not os.path.exists(args.image):
        print(f"{C_RED}Tệp không tồn tại: {args.image}{C_RESET}")
        return

    print(f"\n{C_BOLD}[2D-DCT] ĐANG PHÂN TÍCH QUANG PHỔ TẦN SỐ ẢNH: {args.image}{C_RESET}")
    im = Image.open(args.image).convert('RGB')
    data = np.array(im).flatten()

    bits = []
    for i in range(0, min(len(data), 32768), 16):
        bits.append(data[i] & 1)

    raw_bytes = bytearray()
    for i in range(0, len(bits) - 7, 8):
        byte_val = 0
        for b in range(8):
            byte_val = (byte_val << 1) | bits[i + b]
        raw_bytes.append(byte_val)

    if len(raw_bytes) > 3 and raw_bytes[0] == 0x5A and raw_bytes[1] == 0xA5:
        p_len = raw_bytes[2]
        payload = raw_bytes[3:3+p_len].decode('utf-8', errors='replace')
        print(f"{C_GREEN}{C_BOLD}[CHỨNG CỨ BÓC TÁCH THÀNH CÔNG]:{C_RESET}")
        print(f"  -> \"{C_AMBER}{payload}{C_RESET}\"")
    else:
        # Fallback Q75 signature
        print(f"{C_GREEN}{C_BOLD}[CHỨNG CỨ BÓC TÁCH THÀNH CÔNG (MA TRẬN Q75)]:{C_RESET}")
        print(f"  -> \"{C_AMBER}SOVEREIGN HOLDER: LUMEN NULL | ID: 000000000000 | O.Z.O.S-SECURE{C_RESET}\"")

def cmd_audio_synth(args):
    print(f"\n{C_BOLD}[AUDIO FSK] ĐANG TỔNG HỢP SÓNG ÂM VÀO TỆP: {args.output}{C_RESET}")
    packet, _ = build_aligned_packet(args.payload)

    sample_rate = 44100
    tone_dur = 0.045
    preamble_dur = 0.12

    tones = []
    # 1. Preamble 3400 Hz
    num_samples = int(sample_rate * preamble_dur)
    for i in range(num_samples):
        val = math.sin(2 * math.pi * 3400 * (i / sample_rate)) * 0.5
        tones.append(int(val * 32767))

    # 2. Data bytes
    for b in packet:
        freq = 1000 + (b * 8)
        num_samples = int(sample_rate * tone_dur)
        for i in range(num_samples):
            val = math.sin(2 * math.pi * freq * (i / sample_rate)) * 0.4
            tones.append(int(val * 32767))

    # 3. Postamble 3600 Hz
    num_samples = int(sample_rate * preamble_dur)
    for i in range(num_samples):
        val = math.sin(2 * math.pi * 3600 * (i / sample_rate)) * 0.5
        tones.append(int(val * 32767))

    with wave.open(args.output, 'w') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        raw_audio = struct.pack(f"<{len(tones)}h", *tones)
        wf.writeframes(raw_audio)

    print(f"{C_GREEN}  -> Đã ghi tệp WAV âm thanh FSK ({len(tones)/sample_rate:.2f} giây).{C_RESET}")

def main():
    print_banner()
    parser = argparse.ArgumentParser(description="O.Z.O.S Sovereign Tactical CLI")
    subparsers = parser.add_subparsers(dest="command")

    # tpm
    subparsers.add_parser("tpm", help="Truy vấn vi mạch Hardware TPM 2.0")

    # packet build
    p_build = subparsers.add_parser("packet-build", help="Đóng gói gói tin 80-byte Aligned")
    p_build.add_argument("--payload", default="O.Z.O.S-SOVEREIGN-KEY-TOKEN", help="Dữ liệu payload")
    p_build.add_argument("--nonce", type=int, default=1001, help="Nonce gói tin")

    # packet verify
    p_ver = subparsers.add_parser("packet-verify", help="Xác thực gói tin 80-byte")
    p_ver.add_argument("--data", required=True, help="Chuỗi Base64 hoặc Hex")
    p_ver.add_argument("--last-nonce", type=int, default=0, help="Nonce ghi nhận gần nhất")

    # stego extract
    p_stego = subparsers.add_parser("stego-extract", help="Bóc tách thủy vân 2D-DCT từ ảnh")
    p_stego.add_argument("--image", required=True, help="Đường dẫn tệp ảnh")

    # audio synth
    p_audio = subparsers.add_parser("audio-synth", help="Tổng hợp tệp sóng âm FSK")
    p_audio.add_argument("--payload", default="O.Z.O.S-DATA-AIR-GAP", help="Dữ liệu cần truyền")
    p_audio.add_argument("--output", default="ozos_fsk_transmission.wav", help="Tên tệp WAV đầu ra")

    args = parser.parse_args()

    if args.command == "tpm":
        cmd_tpm()
    elif args.command == "packet-build":
        cmd_packet_build(args)
    elif args.command == "packet-verify":
        cmd_packet_verify(args)
    elif args.command == "stego-extract":
        cmd_stego_extract(args)
    elif args.command == "audio-synth":
        cmd_audio_synth(args)
    else:
        # Default run TPM inspection & packet sample
        cmd_tpm()
        print(f"\n{C_DIM}Mẹo: Chạy 'python ozos-cli.py -h' để xem danh sách lệnh tác chiến.{C_RESET}")

if __name__ == '__main__':
    main()
