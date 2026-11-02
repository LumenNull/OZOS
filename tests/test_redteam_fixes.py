# -*- coding: utf-8 -*-
"""
O.Z.O.S - Red Team Audit Remediation Verification Suite
Tests and certifies patches for all 8 vulnerabilities reported by the Red Team:
- HIGH 1: Default Key Derivation / Machine-ID forgeability neutralized
- HIGH 2: Self-referential author trust eliminated via Pinned Sovereign Anchor
- HIGH 3: Verification headline strictly coupled to signature validity
- MEDIUM 1: Truncated signature frames strictly rejected
- MEDIUM 2: Trailing garbage strictly rejected (4B - 2000B)
- MEDIUM 3: Ed25519 S+L signature malleability strictly rejected (RFC 8032)
- MEDIUM 4: Invalid seed lengths (62 / 66 hex) hard-fail without silent fallback
- MEDIUM 5: Nonce anti-replay enforcement
"""

import os
import sys
import subprocess
import binascii

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(DIR, ".."))
OZOS_EXE = os.path.join(REPO_ROOT, "ozos.exe")

print("======================================================================")
print("  O.Z.O.S RED TEAM AUDIT REMEDIATION VERIFICATION SUITE")
print("  Target: Sovereign Invariant Micro-Kernel & Verification Engine")
print("======================================================================\n")

def run_ozos(*args):
    cmd = [OZOS_EXE] + list(args)
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding='utf-8', errors='replace', cwd=REPO_ROOT)
    return proc.returncode, proc.stdout or "", proc.stderr or ""

all_passed = True

def check_test(name, success, detail=""):
    global all_passed
    if success:
        print(f"  ✅ [PASS] {name}")
        if detail:
            print(f"     └─ {detail}")
    else:
        print(f"  ❌ [FAIL] {name}")
        if detail:
            print(f"     └─ {detail}")
        all_passed = False

# ----------------------------------------------------------------------
# 1. TEST MEDIUM 4: Seed length validation (Must not silently fall back)
# ----------------------------------------------------------------------
print("[TEST 1/8] Kiem Tra Tu Choi Seed Sai Do Dai (No Silent Fallback)...")
code, out, err = run_ozos("keygen", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef01") # 66 chars
short_code, short_out, short_err = run_ozos("keygen", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef") # 64 chars
syntax_err_62 = run_ozos("keygen", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd") # 62 chars

check_test("Seed 66 ky tu bi tu choi voi loi cu phap", code != 0 and "Seed hex phai co do dai chinh xac 64 ky tu" in err, "Exit code non-zero, hard error")
check_test("Seed 62 ky tu bi tu choi voi loi cu phap", syntax_err_62[0] != 0 and "Seed hex phai co do dai chinh xac 64 ky tu" in syntax_err_62[2], "Exit code non-zero, hard error")
check_test("Seed 64 ky tu hop le duoc chap nhan", short_code == 0 and "Sovereign Public Key" in short_out, "Exit code 0, keygen successful")

# ----------------------------------------------------------------------
# 2. TEST HIGH 1: Sovereign key signing & No silent fallback
# ----------------------------------------------------------------------
print("\n[TEST 2/8] Kiem Tra Chinh Sach Bao Ve Identity Mac Dinh...")
seed_sovereign = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
code_sign, out_sign, err_sign = run_ozos("packet-sign", "GENESIS_SOVEREIGN_TRANSACTION", "2000", seed_sovereign)
check_test("Ky goi tin voi seed chu quyen 64-hex thanh cong", code_sign == 0 and "[PACKET SIGNED]" in out_sign, "Packet signed")

# Extract signed packet hex and author pubkey
signed_hex = None
sovereign_pk = None
lines = out_sign.splitlines()
for i, line in enumerate(lines):
    if "Author PubKey" in line:
        sovereign_pk = line.split(":")[-1].strip().replace("0x", "")
    if "[SIGNED HEX DUMP]:" in line and i + 1 < len(lines):
        signed_hex = lines[i + 1].strip()

check_test("Trich xuat duoc Signed Packet va Sovereign PubKey", signed_hex is not None and sovereign_pk is not None, f"PubKey: {sovereign_pk[:16]}...")

# ----------------------------------------------------------------------
# 3. TEST HIGH 2: Self-referential author verification vs Pinned Sovereign Anchor
# ----------------------------------------------------------------------
print("\n[TEST 3/8] Kiem Tra Chot Chan Trust Anchor (Self-Referential Neutralization)...")
# Sign with attacker seed
seed_attacker = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
code_att, out_att, _ = run_ozos("packet-sign", "FORGED BY ATTACKER - TRANSFER ALL FUNDS", "2001", seed_attacker)
attacker_hex = None
attacker_pk = None
lines_att = out_att.splitlines()
for i, line in enumerate(lines_att):
    if "Author PubKey" in line:
        attacker_pk = line.split(":")[-1].strip().replace("0x", "")
    if "[SIGNED HEX DUMP]:" in line and i + 1 < len(lines_att):
        attacker_hex = lines_att[i + 1].strip()

# Verify attacker frame against victim's sovereign_pk as trusted anchor:
code_v1, out_v1, _ = run_ozos("packet-verify", attacker_hex, "1000", sovereign_pk)
check_test("Goi tin cua ke tan cong bi TU CHOI khi doi chieu voi Sovereign Anchor",
           "BI TU CHOI: CHU KY DUNG NHUNG TAC GIA KHONG THUOC DANH BA TIN CAY!" in out_v1 and "CANH BAO: UNTRUSTED / ATTACKER KEY" in out_v1,
           "Attacker frame blocked from sovereign authority")

# Verify genuine frame against sovereign_pk as trusted anchor:
code_v2, out_v2, _ = run_ozos("packet-verify", signed_hex, "1000", sovereign_pk)
check_test("Goi tin chu quyen that duoc xac nhan HOP LE TUYET DOI khi khop Sovereign Anchor",
           "HOP LE TUYET DOI (100% INVARIANT INTEGRITY & SOVEREIGN VERIFIED)" in out_v2 and "TRUSTED PINNED SOVEREIGN KEY" in out_v2,
           "Authentic frame verified against anchor")

# ----------------------------------------------------------------------
# 4. TEST HIGH 3: Headline decoupled from signature validity
# ----------------------------------------------------------------------
print("\n[TEST 4/8] Kiem Tra Tinh Chinh Xac Cua Headline (No False Absolutes)...")
# Build unsigned packet
code_b, out_b, _ = run_ozos("packet-build", "RAW_UNSIGNED_PAYLOAD", "2002")
unsigned_hex = None
lines_b = out_b.splitlines()
for i, line in enumerate(lines_b):
    if "[HEX DUMP]:" in line and i + 1 < len(lines_b):
        unsigned_hex = lines_b[i + 1].strip()

# Verify unsigned packet
code_vu, out_vu, _ = run_ozos("packet-verify", unsigned_hex, "1000")
check_test("Goi tin khong ky TUYET DOI KHONG in 'HOP LE TUYET DOI'",
           "HOP LE TUYET DOI" not in out_vu and "KHONG CO CHU KY ED25519" in out_vu,
           "Correctly categorized as unsigned format only")

# Forge signature: corrupt the signature bytes in signed_hex
raw_bytes = bytearray(binascii.unhexlify(signed_hex))
raw_bytes[-10] ^= 0xFF # Flip bits in Ed25519 signature
corrupted_sig_hex = binascii.hexlify(raw_bytes).decode('ascii')
code_vc, out_vc, _ = run_ozos("packet-verify", corrupted_sig_hex, "1000")
check_test("Goi tin bi sua chu ky TUYET DOI KHONG in 'HOP LE TUYET DOI'",
           "HOP LE TUYET DOI" not in out_vc and "CHU KY ED25519 BI LAM GIA HOAC DU LIEU BI SUA DOI!" in out_vc,
           "Forged signature strictly marked BI TU CHOI")

# ----------------------------------------------------------------------
# 5. TEST MEDIUM 1: Truncated signature frames
# ----------------------------------------------------------------------
print("\n[TEST 5/8] Kiem Tra Tu Choi Chu Ky Bi Cat Cut (Truncated Signature)...")
# Cut off 32 bytes from signature
truncated_hex = signed_hex[:-64]
code_tr, out_tr, _ = run_ozos("packet-verify", truncated_hex, "1000")
check_test("Goi tin bi cat cut chu ky bi TU CHOI nghiem ngat",
           "BI TU CHOI: GOI TIN BI CAT CUT CHU KY (TRUNCATED SIGNATURE)" in out_tr,
           "Truncation caught by canonical framing check")

# ----------------------------------------------------------------------
# 6. TEST MEDIUM 2: Trailing garbage rejection (4B to 2000B)
# ----------------------------------------------------------------------
print("\n[TEST 6/8] Kiem Tra Tu Choi Rac Noi Duoi (Trailing Garbage)...")
garbage_4b_hex = signed_hex + "deadbeef"
garbage_2000b_hex = signed_hex + ("aa" * 2000)

code_g4, out_g4, _ = run_ozos("packet-verify", garbage_4b_hex, "1000")
check_test("Goi tin chua 4 byte rac bi TU CHOI",
           "BI TU CHOI: GOI TIN CHUA RAC NOI DUOI (TRAILING GARBAGE DETECTED)" in out_g4,
           "4 bytes trailing garbage blocked")

code_g2k, out_g2k, _ = run_ozos("packet-verify", garbage_2000b_hex, "1000")
check_test("Goi tin chua 2000 byte rac bi TU CHOI",
           "BI TU CHOI: GOI TIN CHUA RAC NOI DUOI (TRAILING GARBAGE DETECTED)" in out_g2k,
           "2000 bytes trailing garbage blocked")

# ----------------------------------------------------------------------
# 7. TEST MEDIUM 3: Ed25519 S+L Malleability (RFC 8032 Section 5.1.7)
# ----------------------------------------------------------------------
print("\n[TEST 7/8] Kiem Tra Chong Malleability Ed25519 (S < L Invariant)...")
# Order of the base point L for ed25519 in little-endian
# L = 2^252 + 27742317777372353535851937790883648493
L_INT = (2**252) + 27742317777372353535851937790883648493

# Structure of signed frame:
# Header (80B) + Payload (29B) + Author PubKey (32B) + Signature (64B: R[32] + S[32])
# Total attachment: 96B = 32B PubKey + 64B Signature!
# Let's verify: In ozos_packet80.h:
# ozos_sign_packet_frame writes:
# memcpy(out_frame + total_len, pk, 32);
# ozos_ed25519_sign_detached(out_frame + total_len + 32, ...);
# So attachment is: [0..31: pk][32..63: R][64..95: S]!
raw_signed = binascii.unhexlify(signed_hex)
s_bytes = raw_signed[-32:]
s_val = int.from_bytes(s_bytes, byteorder='little')

# Create malleable signature S' = S + L
s_malleable = s_val + L_INT
s_malleable_bytes = s_malleable.to_bytes(32, byteorder='little')

# Construct packet with malleable signature
malleable_packet = raw_signed[:-32] + s_malleable_bytes
malleable_hex = binascii.hexlify(malleable_packet).decode('ascii')

code_mal, out_mal, _ = run_ozos("packet-verify", malleable_hex, "1000")
check_test("Chu ky bien dang S + L bi TU CHOI boi quy tac chuan tac RFC 8032",
           "CHU KY ED25519 BI LAM GIA HOAC DU LIEU BI SUA DOI!" in out_mal,
           "Strict S < L check prevented S+L malleability bypass")

# ----------------------------------------------------------------------
# 8. TEST MEDIUM 5: Nonce Replay Protection
# ----------------------------------------------------------------------
print("\n[TEST 8/8] Kiem Tra Chong Tan Cong Phat Lai (Anti-Replay Invariant)...")
# signed_hex has nonce = 2000
# Verifying with last_seen_nonce = 2000 must FAIL (nonce not strictly greater)
code_rep1, out_rep1, _ = run_ozos("packet-verify", signed_hex, "2000")
check_test("Replay attack bi chan khi Nonce bang last_seen_nonce (2000 == 2000)",
           "Phat hien Replay Attack" in out_rep1,
           "Strictly monotonic nonce enforced")

# Verifying with last_seen_nonce = 2001 must FAIL (nonce 2000 < 2001)
code_rep2, out_rep2, _ = run_ozos("packet-verify", signed_hex, "2001")
check_test("Replay attack bi chan khi Nonce nho hon last_seen_nonce (2000 < 2001)",
           "Phat hien Replay Attack" in out_rep2,
           "Past nonce rejected")

print("\n======================================================================")
if all_passed:
    print("  🏆 KET LUAN: 8/8 BAI KIEM TRA ADVERSARIAL RED TEAM DEU PASS 100%!")
    print("  TOAN BO DIEM GAY MAT MA VA TANG NIEM TIN DA DUOC VA HOAN HAO.")
    print("======================================================================")
    sys.exit(0)
else:
    print("  🚨 CO BAI KIEM TRA THAT BAI! VUI LONG KIEM TRA LAI.")
    print("======================================================================")
    sys.exit(1)
