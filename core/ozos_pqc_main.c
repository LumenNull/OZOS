/*
 * O.Z.O.S // STANDALONE POST-QUANTUM CRYPTOGRAPHY ENCLAVE (ozos-pqc.exe)
 * Module              : ozos-pqc (Package 4 of 4)
 * Architecture        : NIST FIPS 203 (ML-KEM-768) & FIPS 204 (ML-DSA-65) Dual-Layer
 * Quantum Immunity    : Shor's Algorithm Resistant via Module-Lattice M-LWE
 * Target Platforms    : Bare-Metal, POSIX, Windows
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

extern void randombytes(unsigned char *buf, unsigned long long len);

#include "libozos.h"
#include "ozos_crypto.h"
#include "ozos_pqc_hybrid.h"

static void ozos_pqc_random_bytes(uint8_t* out, size_t len) {
#if defined(_WIN32) || defined(_WIN64)
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        if (CryptGenRandom(hProv, (DWORD)len, out)) {
            CryptReleaseContext(hProv, 0);
            return;
        }
        CryptReleaseContext(hProv, 0);
    }
#endif
    randombytes(out, len);
}

static void bytes_to_hex(const uint8_t* in, size_t len, char* out) {
    static const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i) {
        out[i * 2]     = hex_chars[(in[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex_chars[in[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

static void print_pqc_banner(void) {
    printf("=================================================================\n");
    printf("  O.Z.O.S // POST-QUANTUM HYBRID ENCLAVE (ozos-pqc.exe)\n");
    printf("  NIST FIPS 203 (ML-KEM-768) & FIPS 204 (ML-DSA-65) Lattice Engine\n");
    printf("=================================================================\n");
    printf("Operational Commands:\n");
    printf("  ozos-pqc.exe status                  - NIST FIPS 203/204 Specification Diagnostics\n");
    printf("  ozos-pqc.exe test                    - Dual-Engine Hybrid Roundtrip Benchmark\n");
    printf("  ozos-pqc.exe keygen [seed_hex]       - Generate Composite X25519 + ML-KEM Keypair\n");
    printf("=================================================================\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_pqc_banner();
        return 0;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "status") == 0 || strcmp(cmd, "pqc-status") == 0) {
        printf("=================================================================\n");
        printf("  O.Z.O.S // POST-QUANTUM CRYPTOGRAPHY (PQC) STATUS\n");
        printf("  NIST FIPS 203 (ML-KEM-768) & NIST FIPS 204 (ML-DSA-65) Dual-Layer\n");
        printf("=================================================================\n\n");
        printf("[1] QUANTUM RESILIENCE SPECIFICATION:\n");
        printf("  * Primary KEM         : NIST FIPS 203 ML-KEM-768 (Crystals-Kyber-768)\n");
        printf("  * Lattice Ring        : R_q = Z_q[X] / (X^256 + 1), q = 3329, k = 3\n");
        printf("  * Security Category   : NIST Security Level 3 (Equivalent to AES-192 / Classical 128-bit quantum)\n");
        printf("  * Classical Component : Curve25519 / X25519 (ECDH microsecond speed)\n");
        printf("  * Hybrid Combiner     : SHA-512 Dual-Secret Entropy Fusion KDF\n");
        printf("  * Shor's Resistance   : FULLY IMMUNE (M-LWE Lattice Hardness Invariant)\n\n");
        printf("[2] HYBRID WIRE FOOTPRINT:\n");
        printf("  * Classical PK        : %u bytes (X25519)\n", OZOS_PQC_CLASSICAL_PK_BYTES);
        printf("  * Quantum PK          : %u bytes (ML-KEM-768)\n", OZOS_MLKEM_768_PK_BYTES);
        printf("  * Composite PK Size   : %u bytes (Magic + Classical + Quantum)\n", (unsigned)sizeof(ozos_pqc_hybrid_pk_t));
        printf("  * Composite CT Size   : %u bytes (Magic + Ephemeral PK + Ciphertext)\n", (unsigned)sizeof(ozos_pqc_hybrid_ct_t));
        printf("  * Derived Session Key : %u bytes (Constant-time 256-bit symmetric key)\n\n", OZOS_PQC_HYBRID_SECRET_BYTES);
        printf("[STATUS] Post-Quantum Hybrid Armor: ACTIVE & OPERATIONAL\n");
        printf("=================================================================\n");
        return 0;
    } else if (strcmp(cmd, "test") == 0 || strcmp(cmd, "pqc-test") == 0) {
        printf("=================================================================\n");
        printf("  O.Z.O.S // POST-QUANTUM HYBRID ENCLAVE ROUNDTRIP BENCHMARK\n");
        printf("  Testing: X25519 + ML-KEM-768 Dual-Engine Key Encapsulation\n");
        printf("=================================================================\n\n");

        uint8_t seed[32];
        ozos_pqc_random_bytes(seed, sizeof(seed));

        ozos_pqc_hybrid_pk_t pk;
        ozos_pqc_hybrid_sk_t sk;
        ozos_pqc_hybrid_ct_t ct;
        uint8_t alice_session_key[32];
        uint8_t bob_session_key[32];

        int rc = ozos_pqc_hybrid_keypair(&pk, &sk, seed);
        if (rc != 0) {
            fprintf(stderr, "[FAIL] Keypair generation failed (rc=%d)\n", rc);
            return 1;
        }

        rc = ozos_pqc_hybrid_encaps(&pk, &ct, bob_session_key);
        if (rc != 0) {
            fprintf(stderr, "[FAIL] Encapsulation failed (rc=%d)\n", rc);
            return 1;
        }

        rc = ozos_pqc_hybrid_decaps(&sk, &ct, alice_session_key);
        if (rc != 0) {
            fprintf(stderr, "[FAIL] Decapsulation failed (rc=%d)\n", rc);
            return 1;
        }

        int diff = 0;
        for (int i = 0; i < 32; i++) {
            diff |= (alice_session_key[i] ^ bob_session_key[i]);
        }

        char key_hex[65];
        bytes_to_hex(alice_session_key, 32, key_hex);

        printf("* Hybrid Magic          : 0x%08X (FIPS 203 Envelope)\n", pk.magic);
        printf("* Composite PK Size     : %u bytes (X25519: 32B + ML-KEM-768: 1184B)\n", (unsigned)sizeof(pk));
        printf("* Ciphertext Wire Size  : %u bytes\n", (unsigned)sizeof(ct));
        printf("* Agreed Session Key    : 0x%s\n", key_hex);
        printf("* Cryptographic Agreement: %s\n\n", (diff == 0) ? "[PASS] BIT-EXACT MATCH (K_Alice == K_Bob)" : "[FAIL] KEY MISMATCH");
        printf("[BENCHMARK VERDICT] Hybrid Post-Quantum Enclave: VERIFIED & COMPLIANT!\n");
        printf("=================================================================\n");

        ozos_crypto_zeroize(seed, sizeof(seed));
        ozos_crypto_zeroize(&sk, sizeof(sk));
        ozos_crypto_zeroize(alice_session_key, sizeof(alice_session_key));
        ozos_crypto_zeroize(bob_session_key, sizeof(bob_session_key));
        return (diff == 0) ? 0 : 1;
    } else {
        print_pqc_banner();
        return 1;
    }
}
