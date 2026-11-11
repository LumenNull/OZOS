#!/usr/bin/env python3
"""
Automated Real Bare-Metal UEFI Boot Harness (QEMU x86_64 + OVMF Firmware)
Boots ports/baremetal_uefi/EFI/BOOT/BOOTX64.EFI inside a real UEFI BIOS/GOP/Serial
environment (zero host OS kernel), exercises Ring-0 RDRAND, Ed25519 sign/verify,
CRC16 benchmark, BIP-39 PBKDF2-HMAC-SHA512 restore, X25519+XSalsa20-Poly1305 AEAD,
and NIST SP 800-88 SRAM wipe + ACPI EfiResetShutdown.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time


REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EFI_SRC = os.path.join(REPO_ROOT, "ports", "baremetal_uefi", "EFI", "BOOT", "BOOTX64.EFI")
ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[ -/]*[@-~]|\x1b[@-Z\\\]^_]")


def strip_ansi(text):
    return ANSI_RE.sub("", text).replace("\x00", "")


def find_ovmf_firmware():
    candidates = [
        "/usr/share/ovmf/OVMF.fd",
        "/usr/share/qemu/OVMF.fd",
        "/usr/share/OVMF/OVMF_CODE_4M.fd",
        "/usr/share/OVMF/OVMF_CODE.fd",
        "/usr/share/edk2-ovmf/x64/OVMF_CODE.fd",
    ]
    for path in candidates:
        if os.path.exists(path):
            return path
    return None


class SerialDriver:
    def __init__(self, proc):
        self.proc = proc
        self.lock = threading.Lock()
        self.raw_buf = ""
        self.thread = threading.Thread(target=self._reader, daemon=True)
        self.thread.start()

    def _reader(self):
        fd = self.proc.stdout.fileno()
        while True:
            try:
                chunk = os.read(fd, 4096)
            except OSError:
                break
            if not chunk:
                break
            text = chunk.decode("utf-8", errors="replace")
            with self.lock:
                self.raw_buf += text

    def clean_slice(self, start_raw=0):
        with self.lock:
            raw_len = len(self.raw_buf)
            clean = strip_ansi(self.raw_buf[start_raw:])
        return clean, raw_len

    def full_text(self):
        with self.lock:
            return strip_ansi(self.raw_buf)

    def cursor(self):
        with self.lock:
            return len(self.raw_buf)

    def wait_for(self, needles, start_pos=0, timeout=30.0):
        if isinstance(needles, str):
            needles = [needles]
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            tail, raw_len = self.clean_slice(start_pos)
            for needle in needles:
                if needle in tail:
                    return needle, raw_len
            if self.proc.poll() is not None:
                break
            time.sleep(0.05)
        tail, _ = self.clean_slice(start_pos)
        with self.lock:
            raw_tail = self.raw_buf[-160:]
        raise TimeoutError(
            f"Timed out after {timeout}s waiting for {needles} from UEFI serial.\n"
            f"--- Clean tail ---\n{tail}\n"
            f"--- Raw tail repr ---\n{repr(raw_tail)}"
        )

    def wait_shell_prompt(self, start_pos=0, timeout=20.0):
        """Wait until [SHELL] menu and its trailing '>' prompt appear in reassembled stream."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            tail, raw_len = self.clean_slice(start_pos)
            sh_idx = tail.rfind("[SHELL]")
            if sh_idx != -1:
                after_shell = tail[sh_idx:].rstrip(" \t\r\n")
                if after_shell.endswith(">"):
                    time.sleep(0.15)
                    return self.cursor()
            if self.proc.poll() is not None:
                break
            time.sleep(0.05)
        tail, _ = self.clean_slice(start_pos)
        with self.lock:
            raw_tail = self.raw_buf[-160:]
        raise TimeoutError(
            f"Timed out after {timeout}s waiting for [SHELL] prompt from UEFI serial.\n"
            f"--- Clean tail ---\n{tail}\n"
            f"--- Raw tail repr ---\n{repr(raw_tail)}"
        )

    def wait_input_prompt(self, start_pos=0, timeout=15.0):
        """Wait until a sub-command input prompt ending in ':' or '>' (not '->') stabilizes."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            tail, raw_len = self.clean_slice(start_pos)
            stripped = tail.rstrip(" \t\r\n")
            if stripped and (stripped.endswith(":") or (stripped.endswith(">") and not stripped.endswith("->"))):
                time.sleep(0.15)
                if self.cursor() == raw_len:
                    return raw_len
                continue
            if self.proc.poll() is not None:
                break
            time.sleep(0.05)
        tail, _ = self.clean_slice(start_pos)
        with self.lock:
            raw_tail = self.raw_buf[-160:]
        raise TimeoutError(
            f"Timed out after {timeout}s waiting for input prompt from UEFI serial.\n"
            f"--- Clean tail ---\n{tail}\n"
            f"--- Raw tail repr ---\n{repr(raw_tail)}"
        )

    def send(self, data_bytes):
        self.proc.stdin.write(data_bytes)
        self.proc.stdin.flush()


def run_uefi_boot_test():
    if not os.path.exists(EFI_SRC):
        raise FileNotFoundError(f"Missing EFI binary: {EFI_SRC}")

    qemu_bin = shutil.which("qemu-system-x86_64")
    if not qemu_bin:
        raise RuntimeError("qemu-system-x86_64 not found in PATH")

    ovmf_path = find_ovmf_firmware()
    if not ovmf_path:
        raise RuntimeError("No OVMF firmware image found in /usr/share/ovmf or /usr/share/OVMF")

    print(f"[UEFI-QEMU] Binary  : {EFI_SRC} ({os.path.getsize(EFI_SRC)} bytes)")
    print(f"[UEFI-QEMU] OVMF    : {ovmf_path}")

    with tempfile.TemporaryDirectory(prefix="ozos_esp_") as esp_dir:
        boot_dir = os.path.join(esp_dir, "EFI", "BOOT")
        os.makedirs(boot_dir, exist_ok=True)
        shutil.copy2(EFI_SRC, os.path.join(boot_dir, "BOOTX64.EFI"))
        with open(os.path.join(esp_dir, "startup.nsh"), "w", encoding="ascii", newline="\r\n") as f:
            f.write("@echo -off\n")
            f.write("fs0:\\EFI\\BOOT\\BOOTX64.EFI\n")

        cmd = [qemu_bin, "-machine", "q35", "-cpu", "qemu64,+rdrand", "-m", "256M"]
        if "OVMF_CODE" in os.path.basename(ovmf_path):
            cmd.extend(["-drive", f"if=pflash,format=raw,readonly=on,file={ovmf_path}"])
        else:
            cmd.extend(["-bios", ovmf_path])
        cmd.extend([
            "-vga", "std",
            "-display", "none",
            "-serial", "stdio",
            "-no-reboot",
            "-net", "none",
            "-drive", f"format=raw,file=fat:rw:{esp_dir}",
        ])

        print(f"[UEFI-QEMU] Command : {' '.join(cmd)}\n")
        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0,
        )
        driver = SerialDriver(proc)

        try:
            # Wait for OVMF POST + initial self-test + initial [SHELL] prompt
            driver.wait_for("BIT-EXACT RING-0 ED25519 VALIDATED", start_pos=0, timeout=45.0)
            driver.wait_shell_prompt(start_pos=0, timeout=15.0)

            # [1] Ephemeral Ed25519 keypair from CPU RDRAND
            c0 = driver.cursor()
            driver.send(b"1")
            driver.wait_shell_prompt(start_pos=c0, timeout=15.0)

            # [2] Sign & verify OZOS-80 packet + GOP 32x32 Optical Glyph
            c0 = driver.cursor()
            driver.send(b"2")
            driver.wait_shell_prompt(start_pos=c0, timeout=15.0)

            # [G] Console UTF-16 Optical Glyph block render
            c0 = driver.cursor()
            driver.send(b"G")
            driver.wait_shell_prompt(start_pos=c0, timeout=15.0)

            # [3] 100,000-frame Ring-0 benchmark
            c0 = driver.cursor()
            driver.send(b"3")
            driver.wait_for("RING-0 BARE-METAL BENCHMARK", start_pos=c0, timeout=20.0)
            driver.wait_shell_prompt(start_pos=c0, timeout=15.0)

            # [5] BIP-39 12-word restore + passphrase prompt
            c0 = driver.cursor()
            driver.send(b"5")
            c1 = driver.wait_input_prompt(start_pos=c0, timeout=15.0)
            driver.send(
                b"abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about\r"
            )
            c2 = driver.wait_input_prompt(start_pos=c1, timeout=30.0)
            slice5, _ = driver.clean_slice(c1)
            if "[SHELL]" not in slice5:
                driver.send(b"\r")
                driver.wait_shell_prompt(start_pos=c2, timeout=30.0)

            # [6] X25519 ECDH + Poly1305 + XSalsa20-Poly1305 internal loopback self-test
            c0 = driver.cursor()
            driver.send(b"6")
            c1 = driver.wait_input_prompt(start_pos=c0, timeout=15.0)
            slice6, _ = driver.clean_slice(c0)
            if "OZOS-UEFI-RING0-VERIFIED" not in slice6:
                driver.send(b"\r")
                driver.wait_for("OZOS-UEFI-RING0-VERIFIED", start_pos=c1, timeout=20.0)
                driver.wait_shell_prompt(start_pos=c1, timeout=15.0)

            # [7] NIST FIPS 203 ML-KEM-768 + X25519 Hybrid Ring-0 PQC Enclave
            c0 = driver.cursor()
            driver.send(b"7")
            driver.wait_for("FO IMPLICIT REJECTION VERIFIED", start_pos=c0, timeout=30.0)
            driver.wait_shell_prompt(start_pos=c0, timeout=15.0)

            # [4] NIST SP 800-88 SRAM wipe + ResetSystem(EfiResetShutdown)
            driver.send(b"4")
            rc = proc.wait(timeout=20.0)
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait(timeout=5.0)
            transcript = driver.full_text()
            print(transcript.replace("\r\n", "\n").replace("\r", ""))

        if rc != 0:
            raise AssertionError(f"QEMU did not exit cleanly after EfiResetShutdown (rc={rc})")

        required_markers = [
            "BIT-EXACT RING-0 ED25519 VALIDATED",
            "RING-0 BARE-METAL BENCHMARK",
            "MUTUAL POLY1305 TAG VERIFIED",
            'XSalsa20-Poly1305 -> "OZOS-UEFI-RING0-VERIFIED"',
            "BIT-EXACT RING-0 PQC SHARED SECRET",
            "FO IMPLICIT REJECTION VERIFIED",
        ]
        missing = [m for m in required_markers if m not in transcript]
        if missing:
            raise AssertionError(
                f"UEFI serial transcript missed required verification markers: {missing}"
            )

        print("\n=================================================================")
        print("  [PASS] REAL UEFI BARE-METAL BOOT & RING-0 CRYPTO SUITE VERIFIED")
        print("=================================================================")


if __name__ == "__main__":
    run_uefi_boot_test()
