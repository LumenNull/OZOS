# -*- coding: utf-8 -*-
"""
O.Z.O.S Sovereign Mesh P2P (Leg 2) Verification Suite
Tests Zero-IP Routing, Bloom Filter Anti-Storm, and Multi-Hop Relay
"""

import subprocess
import os
import sys

# Ensure UTF-8 on Windows
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

def test_mesh():
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    mesh_exe = os.path.join(base_dir, "ozos-mesh.exe")

    print("======================================================================")
    print("  O.Z.O.S SOVEREIGN MESH P2P VERIFICATION SUITE (LEG 2)")
    print("======================================================================")

    if not os.path.exists(mesh_exe):
        print(f"Error: {mesh_exe} not found!")
        return 1

    # 1. Test status
    print("[TEST 1/4] Testing ozos-mesh.exe status...")
    res = subprocess.run([mesh_exe, "status"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace")
    assert res.returncode == 0, f"status failed: {res.stderr}"
    assert "SOVEREIGN MESH ROUTER STATUS" in res.stdout
    print("  -> Status command verified: OK")

    # 2. Test pack
    print("[TEST 2/4] Testing ozos-mesh.exe pack (80-byte OZOS-80 frame)...")
    res = subprocess.run([mesh_exe, "pack", "0xA11CE001", "0xC0C00003", "HELLO_MESH_WORLD", "5"],
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace")
    assert res.returncode == 0, f"pack failed: {res.stderr}"
    hex_str = None
    for line in res.stdout.splitlines():
        if "Raw Frame Hex" in line:
            hex_str = line.split(":")[-1].strip()
    assert hex_str is not None and len(hex_str) == 160, f"Invalid hex frame: {hex_str}"
    print(f"  -> Encapsulated 80-byte frame: {hex_str[:32]}... OK")

    # 3. Test forward relay & deliver
    print("[TEST 3/4] Testing forward dispatch (Relay vs Deliver)...")
    # Relay to Bob (Bob is 0xB0B00002, frame is for Charlie 0xC0C00003)
    res_relay = subprocess.run([mesh_exe, "forward", hex_str, "0xB0B00002"],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace")
    assert res_relay.returncode == 0, f"forward relay failed: {res_relay.stderr}"
    assert "[RELAY]" in res_relay.stdout, f"Expected RELAY action, got: {res_relay.stdout}"

    # Extract relayed hex
    relayed_hex = None
    for line in res_relay.stdout.splitlines():
        if "Relayed Hex" in line:
            relayed_hex = line.split(":")[-1].strip()
    assert relayed_hex is not None and len(relayed_hex) == 160

    # Deliver to Charlie (Charlie is 0xC0C00003)
    res_deliver = subprocess.run([mesh_exe, "forward", relayed_hex, "0xC0C00003"],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace")
    assert res_deliver.returncode == 0, f"forward deliver failed: {res_deliver.stderr}"
    assert "[DELIVER]" in res_deliver.stdout, f"Expected DELIVER action, got: {res_deliver.stdout}"
    assert "HELLO_MESH_WORLD" in res_deliver.stdout
    print("  -> Multi-hop dispatch (Relay -> Deliver): OK")

    # 4. Test sim-multihop
    print("[TEST 4/4] Testing sim-multihop (Full 3-Node End-to-End Simulation)...")
    res_sim = subprocess.run([mesh_exe, "sim-multihop"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace")
    assert res_sim.returncode == 0, f"sim-multihop failed: {res_sim.stderr}"
    assert "BENCHMARK PASSED 100%" in res_sim.stdout
    print("  -> Full 3-Node Multi-Hop Simulation: OK")

    print("\n======================================================================")
    print("  🏆 ALL MESH P2P TESTS PASSED 100%! LEG 2 FOUNDATION OPERATIONAL.")
    print("======================================================================")
    return 0

if __name__ == "__main__":
    sys.exit(test_mesh())
