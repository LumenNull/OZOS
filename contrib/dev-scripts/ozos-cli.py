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
try:
    from PIL import Image
    import numpy as np
except ImportError:
    Image = None
    np = None

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
    print(f"\n{C_BOLD}[TPM 2.0] QUERYING HARDWARE SILICON ANCHOR...{C_RESET}")
    tpm = OZOSHardwareTPM()
    badge = tpm.export_provenance_badge()
    status_color = C_GREEN if badge["tpm_status"] == "AVAILABLE" else C_AMBER
    print(f"  • TPM Status           : {status_color}{badge['tpm_status']}{C_RESET}")
    print(f"  • Silicon Provider     : {C_CYAN}{badge['provider']}{C_RESET}")
    print(f"  • Silicon Seed Fingerprint: {C_AMBER}{badge['master_silicon_seed_fingerprint']}{C_RESET}")
    print(f"  • TRNG Entropy Source  : {badge['trng_entropy_source']}")
    print(f"  • Legal Admissibility  : {C_GREEN}{badge['admissibility']}{C_RESET}")
    tpm.close()

def compute_fast_gate_mac(data: bytes, k0: int, k1: int = 0) -> int:
    if k1 == 0:
        k1 = (k0 ^ 0x4F5A4F5353495048) & 0xFFFFFFFFFFFFFFFF
    def rotl64(x, b):
        return ((x << b) & 0xFFFFFFFFFFFFFFFF) | (x >> (64 - b))

    v0 = (k0 ^ 0x736f6d6570736575) & 0xFFFFFFFFFFFFFFFF
    v1 = (k1 ^ 0x646f72616e646f6d) & 0xFFFFFFFFFFFFFFFF
    v2 = (k0 ^ 0x6c7967656e657261) & 0xFFFFFFFFFFFFFFFF
    v3 = (k1 ^ 0x7465646279746573) & 0xFFFFFFFFFFFFFFFF

    def sipround():
        nonlocal v0, v1, v2, v3
        v0 = (v0 + v1) & 0xFFFFFFFFFFFFFFFF
        v1 = rotl64(v1, 13) ^ v0
        v0 = rotl64(v0, 32)
        v2 = (v2 + v3) & 0xFFFFFFFFFFFFFFFF
        v3 = rotl64(v3, 16) ^ v2
        v0 = (v0 + v3) & 0xFFFFFFFFFFFFFFFF
        v3 = rotl64(v3, 21) ^ v0
        v2 = (v2 + v1) & 0xFFFFFFFFFFFFFFFF
        v1 = rotl64(v1, 17) ^ v2
        v2 = rotl64(v2, 32)

    in_len = len(data)
    blocks = in_len // 8
    for i in range(blocks):
        m = struct.unpack_from('<Q', data, i * 8)[0]
        v3 ^= m
        sipround()
        sipround()
        v0 ^= m

    rem = in_len % 8
    offset = blocks * 8
    last = (in_len & 0xFF) << 56
    for i in range(rem):
        last |= data[offset + i] << (8 * i)
    v3 ^= last
    sipround()
    sipround()
    v0 ^= last

    v2 ^= 0xFF
    sipround()
    sipround()
    sipround()
    sipround()

    h = (v0 ^ v1 ^ v2 ^ v3) & 0xFFFFFFFFFFFFFFFF
    return ((h & 0xFFFF) ^ ((h >> 16) & 0xFFFF) ^ ((h >> 32) & 0xFFFF) ^ ((h >> 48) & 0xFFFF)) & 0xFFFF

def build_aligned_packet(payload_str, nonce=1001, secret_seed=0xA5A5A5A5):
    if not isinstance(payload_str, str):
        raise ValueError(f"payload_str must be a string (str), received {type(payload_str).__name__}")
    if isinstance(nonce, bool) or not isinstance(nonce, int) or nonce < 0 or nonce > 0xFFFFFFFFFFFFFFFF:
        raise ValueError(f"nonce must be unsigned 64-bit integer [0, 2^64-1], received {nonce!r}")
    if isinstance(secret_seed, bool) or not isinstance(secret_seed, int):
        raise ValueError(f"secret_seed must be an integer (int), received {type(secret_seed).__name__}")
    payload_bytes = payload_str.encode('utf-8')
    if len(payload_bytes) > 65535:
        raise ValueError(f"Payload exceeds maximum 16-bit framing limit ({len(payload_bytes)} > 65535 bytes)")
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

    # 6. Tripwire MAC16 (Offset 0x4E - 0x4F) covering Header (0..77) AND Payload (SipHash-2-4)
    k0 = secret_seed if secret_seed != 0 else 0xA5A5A5A5
    mac_input = bytes(packet[:78]) + bytes(payload_bytes)
    mac16 = compute_fast_gate_mac(mac_input, k0, 0)
    struct.pack_into('<H', packet, 78, mac16)
    return bytes(packet), mac16

def validate_aligned_packet(packet_bytes, last_seen_nonce=0, secret_seed=0xA5A5A5A5):
    if packet_bytes is None or not isinstance(packet_bytes, (bytes, bytearray, memoryview)):
        return False, "Invalid packet data (bytes-like object required)", None
    packet_bytes = bytes(packet_bytes)
    if len(packet_bytes) < 80:
        return False, "Size smaller than 80 bytes", None
    if isinstance(last_seen_nonce, bool):
        return False, "last_seen_nonce cannot be boolean (bool)", None
    if isinstance(secret_seed, bool) or not isinstance(secret_seed, int):
        return False, f"secret_seed must be an integer (int), received {type(secret_seed).__name__}", None
    if isinstance(last_seen_nonce, float):
        return False, "last_seen_nonce cannot be float", None
    if last_seen_nonce is None:
        return False, "last_seen_nonce cannot be None", None
    if not isinstance(last_seen_nonce, int):
        if isinstance(last_seen_nonce, str) and len(last_seen_nonce) > 0 and all('0' <= c <= '9' for c in last_seen_nonce):
            last_seen_nonce = int(last_seen_nonce)
        else:
            return False, f"last_seen_nonce invalid (ASCII digits 0-9 only): {last_seen_nonce!r}", None
    if last_seen_nonce < 0 or last_seen_nonce > 0xFFFFFFFFFFFFFFFF:
        return False, f"last_seen_nonce out of 64-bit range [0, 2^64-1]: {last_seen_nonce}", None

    # Validate poisoned floats (NaN / Infinity)
    import math
    for i in range(16):
        coeff = struct.unpack_from('<f', packet_bytes, i * 4)[0]
        if math.isnan(coeff) or math.isinf(coeff):
            return False, "Poisoned float detected (NaN or Infinity in dct_coeffs)", None

    magic = struct.unpack_from('<I', packet_bytes, 72)[0]
    if magic != 0x47454E01:
        return False, f"Magic Header mismatch (0x{magic:08X})", None

    payload_len = struct.unpack_from('<H', packet_bytes, 76)[0]
    if len(packet_bytes) < 80 + payload_len:
        return False, "Truncated payload framing", None
    if len(packet_bytes) > 80 + payload_len:
        return False, "Trailing garbage detected", None

    # Tripwire MAC16 supports both Sub-profile A1 (CRC16) and Sub-profile A2 (SipHash-2-4)
    incoming_mac = struct.unpack_from('<H', packet_bytes, 78)[0]
    k0 = secret_seed if secret_seed != 0 else 0xA5A5A5A5
    mac_input = bytes(packet_bytes[:78]) + bytes(packet_bytes[80:80+payload_len])
    expected_mac = compute_fast_gate_mac(mac_input, k0, 0)

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
        return False, f"Tripwire MAC16 mismatch: Header or Payload was tampered (0x{incoming_mac:04X} vs 0x{expected_mac:04X})", None

    if is_a1 and not is_a2 and payload_len > 0:
        return False, "Rejected Sub-Profile A1 for packet with payload: MAC16 does not authenticate payload", None

    nonce = struct.unpack_from('<Q', packet_bytes, 64)[0]
    if nonce <= last_seen_nonce:
        return False, f"Replay Attack (Nonce {nonce} <= {last_seen_nonce})", None

    payload = packet_bytes[80:80+payload_len].decode('utf-8', errors='replace')
    return True, "VALID", {"nonce": nonce, "payload": payload, "payload_len": payload_len}

def cmd_packet_build(args):
    packet, mac16 = build_aligned_packet(args.payload, args.nonce)
    b64 = base64.b64encode(packet).decode('utf-8')
    print(f"\n{C_BOLD}[FRAMED PACKET - SIZE: {len(packet)} BYTES]{C_RESET}")
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
        print(f"\n{C_RED}{C_BOLD}[VERIFICATION FAILED]: Data is not valid Base64 or Hex{C_RESET}")
        sys.exit(1)

    valid, reason, info = validate_aligned_packet(raw_bytes, args.last_nonce)
    if valid:
        print(f"\n{C_GREEN}{C_BOLD}[VERIFICATION SUCCESSFUL]{C_RESET}")
        print(f"  • Nonce   : {info['nonce']}")
        print(f"  • Payload : {C_AMBER}\"{info['payload']}\"{C_RESET}")
        print(f"  • Length  : {info['payload_len']} bytes")
    else:
        print(f"\n{C_RED}{C_BOLD}[VERIFICATION FAILED]: {reason}{C_RESET}")
        sys.exit(1)

def _build_dct_basis():
    T = np.zeros((8, 8), dtype=np.float64)
    for u in range(8):
        cu = (1.0 / math.sqrt(8.0)) if u == 0 else math.sqrt(2.0 / 8.0)
        for x in range(8):
            T[u, x] = cu * math.cos(((2 * x + 1) * u * math.pi) / 16.0)
    return T

_DCT_T = _build_dct_basis()
_DCT_TT = _DCT_T.T

def _compute_watermark_crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def embed_2d_dct_watermark(image_path, payload_str, output_path, strength=18.0):
    payload_bytes = payload_str.encode('utf-8')
    if len(payload_bytes) > 255:
        raise ValueError("Watermark payload maximum 255 bytes")
    crc = _compute_watermark_crc16(payload_bytes)
    frame = bytes([0x5A, 0xA5, len(payload_bytes), (crc >> 8) & 0xFF, crc & 0xFF]) + payload_bytes
    bits = []
    for byte_val in frame:
        for b in range(7, -1, -1):
            bits.append((byte_val >> b) & 1)

    im = Image.open(image_path).convert('RGB')
    arr = np.array(im, dtype=np.float64)
    h, w, _ = arr.shape
    bh, bw = h // 8, w // 8
    if bh * bw < len(bits):
        raise ValueError(f"Image too small to embed {len(bits)} bits (requires at least {len(bits)} 8x8 blocks)")

    bit_idx = 0
    for by in range(bh):
        for bx in range(bw):
            if bit_idx >= len(bits):
                break
            y0, x0 = by * 8, bx * 8
            block_rgb = arr[y0:y0+8, x0:x0+8, :]
            Y = 0.299 * block_rgb[:, :, 0] + 0.587 * block_rgb[:, :, 1] + 0.114 * block_rgb[:, :, 2] - 128.0
            F = _DCT_T @ Y @ _DCT_TT
            c1, c2 = F[3, 2], F[2, 3]
            avg = 0.5 * (c1 + c2)
            if bits[bit_idx] == 1:
                if c1 - c2 < strength:
                    F[3, 2] = avg + strength * 0.6
                    F[2, 3] = avg - strength * 0.6
            else:
                if c2 - c1 < strength:
                    F[3, 2] = avg - strength * 0.6
                    F[2, 3] = avg + strength * 0.6
            Y_mod = (_DCT_TT @ F @ _DCT_T) + 128.0
            delta = Y_mod - (Y + 128.0)
            for c in range(3):
                arr[y0:y0+8, x0:x0+8, c] = np.clip(block_rgb[:, :, c] + delta, 0, 255)
            bit_idx += 1
        if bit_idx >= len(bits):
            break

    out_im = Image.fromarray(arr.astype(np.uint8), 'RGB')
    out_im.save(output_path)

def _bits_to_bytes(bits):
    raw_bytes = bytearray()
    for i in range(0, len(bits) - 7, 8):
        byte_val = 0
        for b in range(8):
            byte_val = (byte_val << 1) | bits[i + b]
        raw_bytes.append(byte_val)
    return raw_bytes

def extract_2d_dct_watermark_bytes(im):
    arr = np.array(im.convert('RGB'), dtype=np.float64)
    h, w, _ = arr.shape
    bh, bw = h // 8, w // 8
    dct_bits = []
    max_blocks = min(bh * bw, 2064)
    count = 0
    for by in range(bh):
        for bx in range(bw):
            if count >= max_blocks:
                break
            y0, x0 = by * 8, bx * 8
            block_rgb = arr[y0:y0+8, x0:x0+8, :]
            Y = 0.299 * block_rgb[:, :, 0] + 0.587 * block_rgb[:, :, 1] + 0.114 * block_rgb[:, :, 2] - 128.0
            F = _DCT_T @ Y @ _DCT_TT
            dct_bits.append(1 if F[3, 2] > F[2, 3] else 0)
            count += 1
        if count >= max_blocks:
            break
    dct_bytes = _bits_to_bytes(dct_bits)
    if len(dct_bytes) >= 5 and dct_bytes[0] == 0x5A and dct_bytes[1] == 0xA5:
        p_len = dct_bytes[2]
        exp_crc = (dct_bytes[3] << 8) | dct_bytes[4]
        if 5 + p_len <= len(dct_bytes):
            p_bytes = bytes(dct_bytes[5:5+p_len])
            if _compute_watermark_crc16(p_bytes) == exp_crc:
                return p_bytes
            return None # Tampered watermark payload strictly rejected

    if len(dct_bytes) >= 3 and dct_bytes[0] == 0x5A and dct_bytes[1] == 0xA5:
        p_len = dct_bytes[2]
        if 3 + p_len <= len(dct_bytes):
            try:
                cand = bytes(dct_bytes[3:3+p_len])
                cand.decode('utf-8')
                return cand
            except Exception:
                pass

    # Fallback check for legacy LSB stride-16 test fixtures ONLY if 0x5A 0xA5 magic is present
    flat = np.array(im.convert('RGB')).flatten()
    lsb_bits = [int(flat[i] & 1) for i in range(0, min(len(flat), 32768), 16)]
    lsb_bytes = _bits_to_bytes(lsb_bits)
    if len(lsb_bytes) >= 5 and lsb_bytes[0] == 0x5A and lsb_bytes[1] == 0xA5:
        p_len = lsb_bytes[2]
        exp_crc = (lsb_bytes[3] << 8) | lsb_bytes[4]
        if 5 + p_len <= len(lsb_bytes):
            p_bytes = bytes(lsb_bytes[5:5+p_len])
            if _compute_watermark_crc16(p_bytes) == exp_crc:
                return p_bytes
            return None
    if len(lsb_bytes) >= 3 and lsb_bytes[0] == 0x5A and lsb_bytes[1] == 0xA5:
        p_len = lsb_bytes[2]
        if 3 + p_len <= len(lsb_bytes):
            try:
                cand = bytes(lsb_bytes[3:3+p_len])
                cand.decode('utf-8')
                return cand
            except Exception:
                pass

    return None

def cmd_stego_embed(args):
    if not os.path.exists(args.image):
        print(f"{C_RED}File does not exist: {args.image}{C_RESET}")
        sys.exit(1)
    try:
        embed_2d_dct_watermark(args.image, args.payload, args.output)
    except ValueError as e:
        print(f"{C_RED}[STEGO EMBED ERROR]: {e}{C_RESET}")
        sys.exit(1)
    print(f"{C_GREEN}{C_BOLD}[2D-DCT STEGO EMBED SUCCESSFUL]: {args.output}{C_RESET}")

def cmd_stego_extract(args):
    if not os.path.exists(args.image):
        print(f"{C_RED}File does not exist: {args.image}{C_RESET}")
        sys.exit(1)

    print(f"\n{C_BOLD}[2D-DCT] ANALYZING IMAGE FREQUENCY SPECTRUM: {args.image}{C_RESET}")
    try:
        im = Image.open(args.image).convert('RGB')
        extracted = extract_2d_dct_watermark_bytes(im)
    except Exception as e:
        print(f"{C_RED}[EXTRACTION ERROR]: {e}{C_RESET}")
        sys.exit(1)

    if extracted is not None:
        payload = extracted.decode('utf-8', errors='replace')
        print(f"{C_GREEN}{C_BOLD}[EVIDENCE EXTRACTED SUCCESSFULLY]:{C_RESET}")
        print(f"  -> \"{C_AMBER}{payload}{C_RESET}\"")
    else:
        print(f"{C_RED}{C_BOLD}[NO VALID WATERMARK FOUND]: Missing Magic Header 0x5AA5 on 2D-DCT 8x8 spectrum.{C_RESET}")
        sys.exit(1)

def _hann_env(i, total, ramp):
    if ramp <= 0:
        return 1.0
    if i < ramp:
        return 0.5 * (1.0 - math.cos(math.pi * i / ramp))
    if i >= total - ramp:
        rem = total - 1 - i
        return 0.5 * (1.0 - math.cos(math.pi * rem / ramp))
    return 1.0

def cmd_audio_synth(args):
    print(f"\n{C_BOLD}[AUDIO FSK] SYNTHESIZING ACOUSTIC WAVEFORM TO FILE: {args.output}{C_RESET}")
    text_bytes = args.payload.encode('utf-8')
    if len(text_bytes) > 1024:
        print(f"{C_RED}Error: Audio payload maximum 1024 bytes.{C_RESET}")
        sys.exit(1)

    stealth = getattr(args, "mode", "stealth") != "audible"
    sample_rate = 44100
    tone_dur = 0.040
    pre_dur = 0.040
    samples_per_tone = int(sample_rate * tone_dur)
    samples_pre = int(sample_rate * pre_dur)
    ramp = int(sample_rate * 0.005)

    pre_lo = 17900.0 if stealth else 1350.0
    pre_hi = 19900.0 if stealth else 4050.0
    post_lo = 17900.0 if stealth else 1350.0
    post_hi = 20100.0 if stealth else 4250.0

    tones = []
    # 1. Preamble (40ms dual-tone matching native C engine)
    for i in range(samples_pre):
        t = i / sample_rate
        s = 0.4 * (math.sin(2.0 * math.pi * pre_lo * t) + math.sin(2.0 * math.pi * pre_hi * t)) * _hann_env(i, samples_pre, ramp)
        tones.append(int(max(-32767, min(32767, s * 32767.0))))

    # 2. Data bytes (Dual-Nibble 16-FSK matching native C engine)
    for b in text_bytes:
        low_nib = b & 0x0F
        high_nib = (b >> 4) & 0x0F
        if stealth:
            f_low = 18100.0 + low_nib * 50.0
            f_high = 19000.0 + high_nib * 50.0
        else:
            f_low = 1600.0 + low_nib * 64.0
            f_high = 2800.0 + high_nib * 64.0
        for i in range(samples_per_tone):
            t = i / sample_rate
            s = 0.45 * (math.sin(2.0 * math.pi * f_low * t) + math.sin(2.0 * math.pi * f_high * t)) * _hann_env(i, samples_per_tone, ramp)
            tones.append(int(max(-32767, min(32767, s * 32767.0))))

    # 3. Postamble (40ms dual-tone matching native C engine)
    for i in range(samples_pre):
        t = i / sample_rate
        s = 0.4 * (math.sin(2.0 * math.pi * post_lo * t) + math.sin(2.0 * math.pi * post_hi * t)) * _hann_env(i, samples_pre, ramp)
        tones.append(int(max(-32767, min(32767, s * 32767.0))))

    with wave.open(args.output, 'w') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        raw_audio = struct.pack(f"<{len(tones)}h", *tones)
        wf.writeframes(raw_audio)

    print(f"{C_GREEN}  -> Written Dual-Nibble 16-FSK WAV audio file ({len(tones)/sample_rate:.2f} seconds).{C_RESET}")

def main():
    print_banner()
    parser = argparse.ArgumentParser(description="O.Z.O.S Sovereign Tactical CLI")
    subparsers = parser.add_subparsers(dest="command")

    # tpm
    subparsers.add_parser("tpm", help="Query Hardware TPM 2.0 silicon anchor")

    # packet build
    p_build = subparsers.add_parser("packet-build", help="Frame 80-byte SIMD-aligned packet")
    p_build.add_argument("--payload", default="O.Z.O.S-SOVEREIGN-KEY-TOKEN", help="Payload data string")
    p_build.add_argument("--nonce", type=int, default=1001, help="Packet nonce")

    # packet verify
    p_ver = subparsers.add_parser("packet-verify", help="Validate 80-byte packet")
    p_ver.add_argument("--data", required=True, help="Base64 or Hex encoded packet string")
    p_ver.add_argument("--last-nonce", type=int, default=0, help="Last seen monotonic nonce")

    # stego embed
    p_embed = subparsers.add_parser("stego-embed", help="Embed 2D-DCT frequency watermark into image")
    p_embed.add_argument("--image", required=True, help="Path to cover image file")
    p_embed.add_argument("--payload", required=True, help="Watermark secret payload")
    p_embed.add_argument("--output", required=True, help="Path to output watermarked image")

    # stego extract
    p_stego = subparsers.add_parser("stego-extract", help="Extract 2D-DCT frequency watermark from image")
    p_stego.add_argument("--image", required=True, help="Path to image file")

    # audio synth
    p_audio = subparsers.add_parser("audio-synth", help="Synthesize FSK acoustic waveform file")
    p_audio.add_argument("--payload", default="O.Z.O.S-DATA-AIR-GAP", help="Data payload to transmit")
    p_audio.add_argument("--output", default="ozos_fsk_transmission.wav", help="Output WAV file path")
    p_audio.add_argument("--mode", choices=["stealth", "audible"], default="stealth", help="Acoustic waveform transmission mode")

    args = parser.parse_args()

    if args.command == "tpm":
        cmd_tpm()
    elif args.command == "packet-build":
        cmd_packet_build(args)
    elif args.command == "packet-verify":
        cmd_packet_verify(args)
    elif args.command == "stego-embed":
        cmd_stego_embed(args)
    elif args.command == "stego-extract":
        cmd_stego_extract(args)
    elif args.command == "audio-synth":
        cmd_audio_synth(args)
    else:
        # Default run TPM inspection & packet sample
        cmd_tpm()
        print(f"\n{C_DIM}Tip: Run 'python ozos-cli.py -h' to view command options.{C_RESET}")

if __name__ == '__main__':
    main()
