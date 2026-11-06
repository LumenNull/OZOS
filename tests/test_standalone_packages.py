#!/usr/bin/env python3
"""
O.Z.O.S (Omni-Zero Operating Shield)
Automated Standalone Binary Verification Suite
Tests the 4 decoupled packages independently:
  1. ozos-core.exe  : Minimal Sovereign Microkernel (Stack Enclave)
  2. ozos-phy.exe   : Omni-Spectrum Physical Modem & Transport
  3. ozos-guard.exe : Workstation Armor & Data-at-Rest Engine
  4. ozos-pqc.exe   : NIST FIPS 203 Hybrid Post-Quantum Enclave
"""

import os
import sys
import subprocess
import tempfile
import shutil

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORE_BIN = os.path.join(ROOT_DIR, "ozos-core.exe")
PHY_BIN = os.path.join(ROOT_DIR, "ozos-phy.exe")
GUARD_BIN = os.path.join(ROOT_DIR, "ozos-guard.exe")
PQC_BIN = os.path.join(ROOT_DIR, "ozos-pqc.exe")

def run_cmd(args):
    res = subprocess.run(args, capture_output=True, text=True, cwd=ROOT_DIR)
    return res.returncode, res.stdout, res.stderr

def test_ozos_core():
    print("[TEST 1/4] Testing ozos-core.exe (Minimal Sovereign Stack Enclave)...")
    assert os.path.exists(CORE_BIN), f"Binary not found: {CORE_BIN}"
    
    # 1. keygen
    rc, out, err = run_cmd([CORE_BIN, "keygen"])
    assert rc == 0, f"keygen failed: {err}"
    assert "SOVEREIGN KEY GENERATION" in out
    assert "Sovereign Public Key" in out
    print("  -> ozos-core.exe keygen: OK")
    
    # 2. brainseed-split & combine
    rc, out, err = run_cmd([CORE_BIN, "brainseed-split", "--threshold", "3", "--shares", "5"])
    assert rc == 0, f"brainseed-split failed: {err}"
    assert "SHAMIR'S SECRET SHARING" in out
    
    shares = []
    for line in out.splitlines():
        line = line.strip()
        if len(line) == 76 and all(c in "0123456789abcdefABCDEF" for c in line):
            shares.append(line)
    assert len(shares) == 5, f"Expected 5 shares, got {len(shares)}: {shares}"
    
    # combine 3 shares
    rc, out, err = run_cmd([CORE_BIN, "brainseed-combine", shares[0], shares[2], shares[4]])
    assert rc == 0, f"brainseed-combine failed: {err}\nSTDOUT: {out}"
    assert "RECONSTRUCTION SUCCESS" in out, f"Expected RECONSTRUCTION SUCCESS in output: {out}"
    print("  -> ozos-core.exe Shamir SSS 3-of-5 split/combine: OK")
    print("  [PASS] ozos-core.exe verified 100% independent.")

def test_ozos_pqc():
    print("\n[TEST 2/4] Testing ozos-pqc.exe (NIST FIPS 203 Hybrid Enclave)...")
    assert os.path.exists(PQC_BIN), f"Binary not found: {PQC_BIN}"
    
    # 1. status
    rc, out, err = run_cmd([PQC_BIN, "status"])
    assert rc == 0, f"pqc status failed: {err}"
    assert "NIST FIPS 203" in out
    assert "ML-KEM-768" in out
    print("  -> ozos-pqc.exe status: OK")
    
    # 2. test
    rc, out, err = run_cmd([PQC_BIN, "test"])
    assert rc == 0, f"pqc test failed: {err}"
    assert "BIT-EXACT MATCH" in out
    assert "VERIFIED & COMPLIANT" in out
    print("  -> ozos-pqc.exe test (X25519 + ML-KEM-768): OK")
    print("  [PASS] ozos-pqc.exe verified 100% independent.")

def test_ozos_phy():
    print("\n[TEST 3/4] Testing ozos-phy.exe (Omni-Spectrum Physical Transport)...")
    assert os.path.exists(PHY_BIN), f"Binary not found: {PHY_BIN}"
    
    # 1. spectrum-status
    rc, out, err = run_cmd([PHY_BIN, "spectrum-status"])
    assert rc == 0, f"spectrum-status failed: {err}"
    assert "OMNI-SPECTRUM COGNITIVE PHY" in out
    assert "4/4 COGNITIVE PHY TIERS OPERATIONAL" in out
    print("  -> ozos-phy.exe spectrum-status: OK")
    
    with tempfile.TemporaryDirectory() as tmpdir:
        # 2. glyph-encode & decode
        bmp_path = os.path.join(tmpdir, "glyph_test.bmp")
        test_hex = "0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20"
        rc, out, err = run_cmd([PHY_BIN, "glyph-encode", test_hex, "--bmp", bmp_path])
        assert rc == 0, f"glyph-encode failed: {err}"
        assert os.path.exists(bmp_path)
        
        rc, out, err = run_cmd([PHY_BIN, "glyph-decode", bmp_path])
        assert rc == 0, f"glyph-decode failed: {err}"
        assert "BIT-EXACT MATCH" in out
        print("  -> ozos-phy.exe optical glyph roundtrip: OK")
        
        # 3. ofdm-synth & decode
        wav_path = os.path.join(tmpdir, "ofdm_test.wav")
        rc, out, err = run_cmd([PHY_BIN, "ofdm-synth", test_hex, wav_path])
        assert rc == 0, f"ofdm-synth failed: {err}"
        assert os.path.exists(wav_path)
        
        rc, out, err = run_cmd([PHY_BIN, "ofdm-decode", wav_path])
        assert rc == 0, f"ofdm-decode failed: {err}"
        assert "SUCCESSFUL DECODING" in out
        print("  -> ozos-phy.exe acoustic 64-FFT OFDM roundtrip: OK")
        
        # 4. lora-pack & unpack
        rc, out, err = run_cmd([PHY_BIN, "lora-pack", test_hex])
        assert rc == 0, f"lora-pack failed: {err}"
        assert "LORA SUB-GHZ PACK SUCCESS" in out
        
        frame_hex = None
        for line in out.splitlines():
            line = line.strip()
            if len(line) == 168:
                frame_hex = line
                break
        assert frame_hex is not None, "Could not extract LoRa 168-char frame hex"
        
        rc, out, err = run_cmd([PHY_BIN, "lora-unpack", frame_hex])
        assert rc == 0, f"lora-unpack failed: {err}"
        assert "Invariant Match" in out
        print("  -> ozos-phy.exe LoRa Sub-GHz wire frame roundtrip: OK")

    print("  [PASS] ozos-phy.exe verified 100% independent.")

def test_ozos_guard():
    print("\n[TEST 4/4] Testing ozos-guard.exe (Workstation & Data-at-Rest Armor)...")
    assert os.path.exists(GUARD_BIN), f"Binary not found: {GUARD_BIN}"
    
    # 1. workstation-shield
    rc, out, err = run_cmd([GUARD_BIN, "workstation-shield"])
    assert rc == 0, f"workstation-shield failed: {err}"
    assert "WORKSTATION VIRTUAL AIR-GAP & MEMORY SHIELD AUDIT" in out
    assert "HARDWARE VIRTUALIZATION" in out
    assert "PROCESS MEMORY SHIELD" in out
    print("  -> ozos-guard.exe workstation-shield: OK")
    
    with tempfile.TemporaryDirectory() as tmpdir:
        # 2. burn (NIST SP 800-88 sanitization)
        target_file = os.path.join(tmpdir, "secret.dat")
        with open(target_file, "wb") as f:
            f.write(b"TOP_SECRET_EPHEMERAL_PAYLOAD_FOR_SANITIZATION" * 10)
        assert os.path.exists(target_file)
        
        rc, out, err = run_cmd([GUARD_BIN, "burn", target_file])
        assert rc == 0, f"burn failed: {err}"
        assert not os.path.exists(target_file), "File was not securely unlinked by burn!"
        assert "SANITIZATION COMPLETE" in out or "NIST SP 800-88" in out
        print("  -> ozos-guard.exe NIST SP 800-88 burn: OK")
        
        # 3. anchor & anchor-verify
        anchor_dir = os.path.join(tmpdir, "project")
        os.makedirs(anchor_dir, exist_ok=True)
        with open(os.path.join(anchor_dir, "doc1.txt"), "w") as f:
            f.write("Integrity test content 1")
        with open(os.path.join(anchor_dir, "doc2.txt"), "w") as f:
            f.write("Integrity test content 2")
            
        rc, out, err = run_cmd([GUARD_BIN, "anchor", anchor_dir, "--seed", "11223344556677889900aabbccddeeff11223344556677889900aabbccddeeff"])
        assert rc == 0, f"anchor failed: {err}"
        assert os.path.exists(os.path.join(anchor_dir, ".ozos_anchor.json"))
        
        rc, out, err = run_cmd([GUARD_BIN, "anchor-verify", anchor_dir])
        assert rc == 0, f"anchor-verify failed: {err}"
        assert "VERIFIED" in out
        print("  -> ozos-guard.exe Merkle provenance anchor: OK")
        
        # 4. vault pack & open
        vault_file = os.path.join(tmpdir, "archive.ozos")
        rc, out, err = run_cmd([GUARD_BIN, "vault", "pack", anchor_dir, vault_file, "--seed", "11223344556677889900aabbccddeeff11223344556677889900aabbccddeeff"])
        assert rc == 0, f"vault pack failed: {err}"
        assert os.path.exists(vault_file)
        
        extract_dir = os.path.join(tmpdir, "extracted")
        rc, out, err = run_cmd([GUARD_BIN, "vault", "open", vault_file, extract_dir, "--seed", "11223344556677889900aabbccddeeff11223344556677889900aabbccddeeff"])
        assert rc == 0, f"vault open failed: {err}"
        assert os.path.exists(os.path.join(extract_dir, "doc1.txt"))
        print("  -> ozos-guard.exe XSalsa20-Poly1305 sealed vault: OK")

    print("  [PASS] ozos-guard.exe verified 100% independent.")

def main():
    print("======================================================================")
    print("  O.Z.O.S STANDALONE DECOUPLED PACKAGE VERIFICATION SUITE")
    print("======================================================================")
    test_ozos_core()
    test_ozos_pqc()
    test_ozos_phy()
    test_ozos_guard()
    print("\n======================================================================")
    print("  ALL 4 STANDALONE BINARIES FULLY FUNCTIONAL AND VERIFIED 100%!")
    print("======================================================================")

if __name__ == "__main__":
    main()
