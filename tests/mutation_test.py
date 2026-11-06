#!/usr/bin/env python3
"""
=============================================================================
  O.Z.O.S - EMPIRICAL MUTATION TESTING & FAULT INJECTION ENGINE
  Inspired by Mull (LLVM IR Mutation Framework) - Native C99 & Wire Invariants
=============================================================================

This mutation testing harness introduces synthetic faults (mutants) into the
core cryptographic, physical layer, and micro-kernel invariants to verify
that the test suites (test_libozos, test_omni_phy, test_audit_verify,
test_redteam_fixes) are GENUINE and FAIL IMMEDIATELY upon any violation.

A test suite that passes despite a mutated invariant is flagged as "PASS ẢO"
(False Positive / Weak Test). A high mutation score (100%) proves zero fake passes.
"""

import os
import sys
import time
import shutil
import subprocess

# Ensure UTF-8 stdout and stderr on Windows
if sys.stdout and hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if sys.stderr and hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CORE_DIR = os.path.join(REPO_ROOT, "core")

# List of defined mutants targeting fundamental invariants
MUTANTS = [
    {
        "id": "MUT-01-ANTI-REPLAY",
        "description": "Bypass Anti-Replay monotonic nonce enforcement in libozos.h",
        "file": os.path.join(CORE_DIR, "libozos.h"),
        "target": "if (pkt->nonce <= last_seen_nonce) {",
        "replacement": "if (false && pkt->nonce <= last_seen_nonce) {",
        "test_cmd": [os.path.join(REPO_ROOT, "test_libozos.exe")],
        "fallback_test_cmd": [os.path.join(REPO_ROOT, "test_libozos")]
    },
    {
        "id": "MUT-02-MAC16-TRIPWIRE",
        "description": "Bypass Fast Gatekeeper MAC16 validation in libozos.h",
        "file": os.path.join(CORE_DIR, "libozos.h"),
        "target": "if (!mac_ok) {",
        "replacement": "if (false && !mac_ok) {",
        "test_cmd": [os.path.join(REPO_ROOT, "test_libozos.exe")],
        "fallback_test_cmd": [os.path.join(REPO_ROOT, "test_libozos")]
    },
    {
        "id": "MUT-03-MAGIC-HEADER",
        "description": "Bypass Genesis Magic Header invariant in libozos.h",
        "file": os.path.join(CORE_DIR, "libozos.h"),
        "target": "if (pkt->magic != OZOS_MAGIC_HEADER) {",
        "replacement": "if (false && pkt->magic != OZOS_MAGIC_HEADER) {",
        "test_cmd": [os.path.join(REPO_ROOT, "test_libozos.exe")],
        "fallback_test_cmd": [os.path.join(REPO_ROOT, "test_libozos")]
    },
    {
        "id": "MUT-04-ANTI-DFA-BARRIER",
        "description": "Disable Dual FPU vs ALU Laser Fault Injection detection in libozos.h",
        "file": os.path.join(CORE_DIR, "libozos.h"),
        "target": "if (drift > 0.15f) {",
        "replacement": "if (false && drift > 0.15f) {",
        "test_cmd": [os.path.join(REPO_ROOT, "test_libozos.exe")],
        "fallback_test_cmd": [os.path.join(REPO_ROOT, "test_libozos")]
    },
    {
        "id": "MUT-05-ED25519-VERIFY",
        "description": "Force Ed25519 Sovereign Signature verification to always succeed",
        "file": os.path.join(CORE_DIR, "ozos_crypto.h"),
        "target": "return (r == 0 && mlen_out == (unsigned long long)msg_len);",
        "replacement": "return true; /* FORCED MUTANT BYPASS */",
        "test_cmd": [os.path.join(REPO_ROOT, "test_libozos.exe")],
        "fallback_test_cmd": [os.path.join(REPO_ROOT, "test_libozos")]
    },
    {
        "id": "MUT-06-ED25519-S-CANONICAL",
        "description": "Disable RFC 8032 S < L non-malleability canonical check in ozos_crypto.h",
        "file": os.path.join(CORE_DIR, "ozos_crypto.h"),
        "target": "if (!ozos_ed25519_is_canonical_s(sig + 32)) {",
        "replacement": "if (false) {",
        "test_cmd": [os.path.join(REPO_ROOT, "test_libozos.exe")],
        "fallback_test_cmd": [os.path.join(REPO_ROOT, "test_libozos")]
    },
    {
        "id": "MUT-07-X25519-LOW-ORDER",
        "description": "Disable RFC 7748 X25519 Low-Order Point predicate in ozos_crypto.h",
        "file": os.path.join(CORE_DIR, "ozos_crypto.h"),
        "target": "return (match != 0);",
        "replacement": "return false; /* FORCED MUTANT BYPASS */",
        "test_cmd": [os.path.join(REPO_ROOT, "test_audit_verify.exe")],
        "fallback_test_cmd": [os.path.join(REPO_ROOT, "test_audit_verify")]
    },
    {
        "id": "MUT-08-SIPHASH-AVALANCHE",
        "description": "Degrade SipHash-2-4 PRF output to zero identity in ozos_hardened_core.cpp",
        "file": os.path.join(CORE_DIR, "ozos_hardened_core.cpp"),
        "target": "uint64_t tag64 = v0 ^ v1 ^ v2 ^ v3;",
        "replacement": "uint64_t tag64 = 0; /* MUTATED IDENTITY PRF */",
        "test_cmd": [os.path.join(REPO_ROOT, "test_audit_verify.exe")],
        "fallback_test_cmd": [os.path.join(REPO_ROOT, "test_audit_verify")]
    },
    {
        "id": "MUT-09-LORA-CRC16",
        "description": "Disable LoRa Sub-GHz CRC16-CCITT frame integrity gate in ozos_omni_phy.h",
        "file": os.path.join(CORE_DIR, "ozos_omni_phy.h"),
        "target": "uint16_t computed_crc = ozos_crc16_ccitt(in_frame, 82);\n    if (expected_crc != computed_crc) {",
        "replacement": "uint16_t computed_crc = ozos_crc16_ccitt(in_frame, 82);\n    if (false && expected_crc != computed_crc) {",
        "test_cmd": [os.path.join(REPO_ROOT, "test_omni_phy.exe")],
        "fallback_test_cmd": [os.path.join(REPO_ROOT, "test_omni_phy")]
    },
    {
        "id": "MUT-10-GLYPH-CRC32",
        "description": "Disable Optical Glyph 32x32 CRC32-IEEE checksum in ozos_omni_phy.h",
        "file": os.path.join(CORE_DIR, "ozos_omni_phy.h"),
        "target": "uint32_t computed_crc = ozos_crc32(payload, 80);\n    if (expected_crc != computed_crc) {",
        "replacement": "uint32_t computed_crc = ozos_crc32(payload, 80);\n    if (false && expected_crc != computed_crc) {",
        "test_cmd": [os.path.join(REPO_ROOT, "test_omni_phy.exe")],
        "fallback_test_cmd": [os.path.join(REPO_ROOT, "test_omni_phy")]
    }
]

def recompile():
    """Recompiles the binaries using the platform build system."""
    if sys.platform == "win32":
        cmd = ["cmd.exe", "/c", "call build.bat"]
    else:
        cmd = ["make", "all"]
    p = subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return p.returncode == 0

def get_executable(cmd_list):
    """Resolves Windows .exe vs POSIX binary in root or tests dir."""
    exe = cmd_list[0]
    candidates = [
        exe,
        exe + ".exe" if not exe.endswith(".exe") else exe[:-4],
        os.path.join(REPO_ROOT, "tests", os.path.basename(exe)),
        os.path.join(REPO_ROOT, "tests", os.path.basename(exe) + ".exe" if not exe.endswith(".exe") else os.path.basename(exe)[:-4]),
        os.path.join(REPO_ROOT, os.path.basename(exe)),
        os.path.join(REPO_ROOT, os.path.basename(exe) + ".exe" if not exe.endswith(".exe") else os.path.basename(exe)[:-4]),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return [c] + cmd_list[1:]
    return cmd_list

def run_mutation_audit():
    print("=" * 70)
    print("  O.Z.O.S EMPIRICAL MUTATION TESTING HARNESS (MULL-PARITY)")
    print("  Evaluating Test Suite Resistance Against Fault Injection")
    print("=" * 70)

    # 1. Baseline verification: ensure tests PASS on clean code
    print("\n[PHASE 0] Baseline Test Suite Execution (Clean Code)...")
    if not recompile():
        print("❌ Baseline compilation failed! Aborting.")
        return False

    baseline_targets = [
        [os.path.join(REPO_ROOT, "test_libozos.exe")],
        [os.path.join(REPO_ROOT, "test_audit_verify.exe")],
        [os.path.join(REPO_ROOT, "test_omni_phy.exe")]
    ]
    for b_cmd in baseline_targets:
        resolved = get_executable(b_cmd)
        p = subprocess.run(resolved, cwd=REPO_ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
        if p.returncode != 0:
            print(f"[FAIL] Baseline test failed: {' '.join(resolved)} (RC={p.returncode})", flush=True)
            return False

    print("  [OK] Baseline test suite PASSES cleanly 100%. Starting Mutation Injections.\n", flush=True)

    total_mutants = len(MUTANTS)
    killed_mutants = 0
    survived_mutants = []

    for idx, mut in enumerate(MUTANTS, 1):
        mid = mut["id"]
        desc = mut["description"]
        mfile = mut["file"]
        target = mut["target"]
        replacement = mut["replacement"]
        test_cmd = get_executable(mut["test_cmd"])

        print(f"[{idx}/{total_mutants}] Injecting Mutant {mid}...", flush=True)
        print(f"  • Description: {desc}", flush=True)
        print(f"  • Target File: {os.path.relpath(mfile, REPO_ROOT)}", flush=True)

        # Read original binary content to preserve bit-exact git status
        with open(mfile, "rb") as f:
            original_bytes = f.read()

        original_code = original_bytes.decode("utf-8")
        is_crlf = "\r\n" in original_code
        norm_code = original_code.replace("\r\n", "\n")
        norm_target = target.replace("\r\n", "\n")
        norm_replacement = replacement.replace("\r\n", "\n")

        if norm_target not in norm_code:
            print(f"  [WARN] Target anchor not found in {os.path.basename(mfile)}!", flush=True)
            continue

        try:
            # Apply mutation
            mutated_code = norm_code.replace(norm_target, norm_replacement, 1)
            if is_crlf:
                mutated_code = mutated_code.replace("\n", "\r\n")
            with open(mfile, "wb") as f:
                f.write(mutated_code.encode("utf-8"))

            # Recompile
            comp_ok = recompile()
            if not comp_ok:
                # Compile-time detection (Mutant killed at compile time by type system / MISRA)
                print(f"  [KILLED BY COMPILER] Mutant killed during compilation (Type/Invariant Safety)", flush=True)
                killed_mutants += 1
                continue

            # Run test command
            p = subprocess.run(test_cmd, cwd=REPO_ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
            if p.returncode != 0:
                print(f"  [KILLED BY TEST SUITE] Test failed as expected (RC={p.returncode})", flush=True)
                killed_mutants += 1
            else:
                print(f"  [SURVIVED!] Mutant went undetected by test suite! ('Pass Ao' Alert)", flush=True)
                survived_mutants.append((mid, desc))

        finally:
            # Always restore bit-exact original bytes immediately
            with open(mfile, "wb") as f:
                f.write(original_bytes)
            recompile()

    # Final tally
    mutation_score = (killed_mutants / total_mutants) * 100.0
    print("\n" + "=" * 70, flush=True)
    print(f"  MUTATION TESTING RESULTS SUMMARY:", flush=True)
    print(f"  • Total Mutants Injected : {total_mutants}", flush=True)
    print(f"  • Mutants Killed         : {killed_mutants} [KILLED]", flush=True)
    print(f"  • Mutants Survived       : {len(survived_mutants)} [SURVIVED]", flush=True)
    print(f"  • Mutation Score         : {mutation_score:.1f}%", flush=True)
    print("=" * 70, flush=True)

    if len(survived_mutants) == 0 and mutation_score == 100.0:
        print("  [CHUNG MINH THUC NGHIEM]: 100% CAC BAI TEST LA KIEM THU THAT!", flush=True)
        print("  Khong co bat ky kiem chung ao nao (Zero False Positives / Zero Pass Ao).", flush=True)
        print("=" * 70 + "\n", flush=True)
        return True
    else:
        print("  [CANH BAO]: CO MUTANT SONG SOT! CAN BO SUNG ASSERTION CHO CAC MUC SAU:", flush=True)
        for sm_id, sm_desc in survived_mutants:
            print(f"    - {sm_id}: {sm_desc}", flush=True)
        print("=" * 70 + "\n", flush=True)
        return False

if __name__ == "__main__":
    success = run_mutation_audit()
    sys.exit(0 if success else 1)
