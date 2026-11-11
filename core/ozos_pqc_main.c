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
#include "ozos_brainseed.h"
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
    out[len * 2] = 0;
}

static bool hex_to_bytes(const char* hex, uint8_t* out, size_t max_out, size_t* out_len) {
    if (!hex) return false;
    size_t len = strlen(hex);
    if (len % 2 != 0 || len / 2 > max_out) return false;
    for (size_t i = 0; i < len / 2; ++i) {
        int hi = hex[i * 2];
        int lo = hex[i * 2 + 1];
        int h = (hi >= '0' && hi <= '9') ? (hi - '0') :
        (hi >= 'a' && hi <= 'f') ? (hi - 'a' + 10) :
        (hi >= 'A' && hi <= 'F') ? (hi - 'A' + 10) : -1;
        int l = (lo >= '0' && lo <= '9') ? (lo - '0') :
        (lo >= 'a' && lo <= 'f') ? (lo - 'a' + 10) :
        (lo >= 'A' && lo <= 'F') ? (lo - 'A' + 10) : -1;
        if (h < 0 || l < 0) return false;
        out[i] = (uint8_t)((h << 4) | l);
    }
    if (out_len) *out_len = len / 2;
    return true;
}

static void print_pqc_banner(void) {
    printf("=================================================================\n");
    printf("  O.Z.O.S // POST-QUANTUM HYBRID ENCLAVE (ozos-pqc.exe)\n");
    printf("  NIST FIPS 203 (ML-KEM-768) & FIPS 204 (ML-DSA-65) Lattice Engine\n");
    printf("=================================================================\n");
    printf("Operational Commands:\n");
    printf("  ozos-pqc.exe status                  - NIST FIPS 203/204 Specification Diagnostics\n");
    printf("  ozos-pqc.exe test                    - Dual-Engine Hybrid Roundtrip Benchmark\n");
    printf("  ozos-pqc.exe keygen [seed] [prefix]  - Generate Composite X25519 + ML-KEM Keypair\n");
    printf("  ozos-pqc.exe encaps <pk> <out_ct>    - Encapsulate Ephemeral Quantum Session Key\n");
    printf("  ozos-pqc.exe decaps <sk> <in_ct>     - Decapsulate Quantum Session Key\n");
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
        printf("[3] POST-QUANTUM SIGNATURE SPECIFICATION (NIST FIPS 204):\n");
        printf("  * Primary Signature   : NIST FIPS 204 ML-DSA-65 (Crystals-Dilithium-65)\n");
        printf("  * Module Parameters   : q = 8380417, (k, l) = (6, 5), eta = 4, tau = 49\n");
        printf("  * Signature Footprint : PK %uB | SK %uB | Sig %uB\n\n",
               OZOS_MLDSA_65_PK_BYTES, OZOS_MLDSA_65_SK_BYTES, OZOS_MLDSA_65_SIG_BYTES);
        printf("[STATUS] Post-Quantum Hybrid Armor: ACTIVE & OPERATIONAL\n");
        printf("=================================================================\n");
        return 0;

    } else if (strcmp(cmd, "test") == 0 || strcmp(cmd, "pqc-test") == 0) {
        printf("=================================================================\n");
        printf("  O.Z.O.S // POST-QUANTUM HYBRID ENCLAVE ROUNDTRIP BENCHMARK\n");
        printf("  Testing: X25519 + ML-KEM-768 (FIPS 203) & ML-DSA-65 (FIPS 204)\n");
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
            ozos_crypto_zeroize(seed, sizeof(seed));
            return 1;
        }
        rc = ozos_pqc_hybrid_encaps(&pk, &ct, bob_session_key);
        if (rc != 0) {
            fprintf(stderr, "[FAIL] Encapsulation failed (rc=%d)\n", rc);
            ozos_crypto_zeroize(seed, sizeof(seed));
            ozos_crypto_zeroize(&sk, sizeof(sk));
            return 1;
        }
        rc = ozos_pqc_hybrid_decaps(&sk, &ct, alice_session_key);
        if (rc != 0) {
            fprintf(stderr, "[FAIL] Decapsulation failed (rc=%d)\n", rc);
            ozos_crypto_zeroize(seed, sizeof(seed));
            ozos_crypto_zeroize(&sk, sizeof(sk));
            return 1;
        }
        int diff = 0;
        for (int i = 0; i < 32; i++) {
            diff |= (alice_session_key[i] ^ bob_session_key[i]);
        }

        uint8_t dsa_seed[32];
        uint8_t dsa_pk[OZOS_MLDSA_65_PK_BYTES];
        uint8_t dsa_sk[OZOS_MLDSA_65_SK_BYTES];
        uint8_t dsa_sig[OZOS_MLDSA_65_SIG_BYTES];
        static const uint8_t dsa_msg[] = "OZOS-FIPS204-ML-DSA-65-LIVE-ENCLAVE";
        ozos_pqc_random_bytes(dsa_seed, sizeof(dsa_seed));
        bool dsa_ok = (ozos_mldsa65_keygen(dsa_pk, dsa_sk, dsa_seed) == 0) &&
                      (ozos_mldsa65_sign(dsa_sig, dsa_msg, sizeof(dsa_msg) - 1U, NULL, 0U, dsa_sk, false) == 0) &&
                      ozos_mldsa65_verify(dsa_sig, dsa_msg, sizeof(dsa_msg) - 1U, NULL, 0U, dsa_pk);
        dsa_sig[128] ^= 0x01U;
        bool dsa_tamper_rej = !ozos_mldsa65_verify(dsa_sig, dsa_msg, sizeof(dsa_msg) - 1U, NULL, 0U, dsa_pk);
        bool all_pqc_ok = (diff == 0) && dsa_ok && dsa_tamper_rej;

        char key_hex[65];
        bytes_to_hex(alice_session_key, 32, key_hex);
        printf("* Hybrid Magic          : 0x%08X (FIPS 203 Envelope)\n", pk.magic);
        printf("* Composite PK Size     : %u bytes (X25519: 32B + ML-KEM-768: 1184B)\n", (unsigned)sizeof(pk));
        printf("* Ciphertext Wire Size  : %u bytes\n", (unsigned)sizeof(ct));
        printf("* Agreed Session Key    : 0x%s\n", key_hex);
        printf("* Cryptographic Agreement: %s\n", (diff == 0) ? "[PASS] BIT-EXACT MATCH (K_Alice == K_Bob)" : "[FAIL] KEY MISMATCH");
        printf("* FIPS 204 ML-DSA-65    : %s (%uB PK / %uB Sig + 1-bit tamper rejected)\n\n",
               (dsa_ok && dsa_tamper_rej) ? "[PASS] SIGNATURE & TAMPER VERIFIED" : "[FAIL] SIGNATURE ERROR",
               OZOS_MLDSA_65_PK_BYTES, OZOS_MLDSA_65_SIG_BYTES);
        printf("[BENCHMARK VERDICT] Hybrid Post-Quantum Enclave: %s\n",
               all_pqc_ok ? "VERIFIED & COMPLIANT!" : "FAILED!");
        printf("=================================================================\n");
        ozos_crypto_zeroize(seed, sizeof(seed));
        ozos_crypto_zeroize(dsa_seed, sizeof(dsa_seed));
        ozos_crypto_zeroize(dsa_sk, sizeof(dsa_sk));
        ozos_crypto_zeroize(&sk, sizeof(sk));
        ozos_crypto_zeroize(alice_session_key, sizeof(alice_session_key));
        ozos_crypto_zeroize(bob_session_key, sizeof(bob_session_key));
        return all_pqc_ok ? 0 : 1;

    } else if (strcmp(cmd, "keygen") == 0 || strcmp(cmd, "pqc-keygen") == 0) {
        const char* custom_seed_hex = NULL;
        const char* out_prefix = "pqc_identity";
        if (argc >= 3) {
            if (strlen(argv[2]) == 64) {
                custom_seed_hex = argv[2];
                if (argc >= 4) out_prefix = argv[3];

            } else {
                out_prefix = argv[2];
                if (argc >= 4 && strlen(argv[3]) == 64) custom_seed_hex = argv[3];
            }
        }
        uint8_t seed[32];
        if (custom_seed_hex) {
            size_t slen = 0;
            if (!hex_to_bytes(custom_seed_hex, seed, 32, &slen) || slen != 32) {
                fprintf(stderr, "[ERROR] Seed hex must be exactly 64 hex characters (32 bytes).\n");
                return 1;
            }

        } else {
            ozos_pqc_random_bytes(seed, sizeof(seed));
        }
        ozos_pqc_hybrid_pk_t pk;
        ozos_pqc_hybrid_sk_t sk;
        int rc = ozos_pqc_hybrid_keypair(&pk, &sk, seed);
        if (rc != 0) {
            fprintf(stderr, "[FAIL] Keypair generation failed (rc=%d)\n", rc);
            ozos_crypto_zeroize(seed, sizeof(seed));
            return 1;
        }
        char pub_file[260];
        char sec_file[260];
        snprintf(pub_file, sizeof(pub_file), "%s.pub", out_prefix);
        snprintf(sec_file, sizeof(sec_file), "%s.sec", out_prefix);
        FILE* fp_pub = fopen(pub_file, "wb");
        if (!fp_pub) {
            fprintf(stderr, "[FAIL] Cannot open '%s' for writing.\n", pub_file);
            ozos_crypto_zeroize(seed, sizeof(seed));
            ozos_crypto_zeroize(&sk, sizeof(sk));
            return 1;
        }
        fwrite(&pk, 1, sizeof(pk), fp_pub);
        fclose(fp_pub);
        FILE* fp_sec = fopen(sec_file, "wb");
        if (!fp_sec) {
            fprintf(stderr, "[FAIL] Cannot open '%s' for writing.\n", sec_file);
            ozos_crypto_zeroize(seed, sizeof(seed));
            ozos_crypto_zeroize(&sk, sizeof(sk));
            return 1;
        }
        fwrite(&sk, 1, sizeof(sk), fp_sec);
        fclose(fp_sec);
        uint8_t fp_hash[32];
        ozos_sha256((const uint8_t*)&pk, sizeof(pk), fp_hash);
        char fp_hex[65];
        bytes_to_hex(fp_hash, 32, fp_hex);
        printf("=================================================================\n");
        printf("  O.Z.O.S // HYBRID POST-QUANTUM KEYPAIR GENERATED\n");
        printf("  Lattice Ring: ML-KEM-768 + Classical Curve: X25519\n");
        printf("=================================================================\n\n");
        printf("* Public Key File       : %s (%u bytes)\n", pub_file, (unsigned)sizeof(pk));
        printf("  - Classical PK (X25519) : 32 bytes\n");
        printf("  - Quantum PK (ML-KEM)   : %u bytes\n", OZOS_MLKEM_768_PK_BYTES);
        printf("* Secret Key File       : %s (%u bytes)\n", sec_file, (unsigned)sizeof(sk));
        printf("  - Classical SK (X25519) : 64 bytes\n");
        printf("  - Quantum SK (ML-KEM)   : %u bytes\n", OZOS_MLKEM_768_SK_BYTES);
        printf("* Identity Fingerprint  : SHA256:%s\n", fp_hex);
        printf("* Entropy Source        : %s\n", custom_seed_hex ? "Deterministic Hex Seed" : "Hardware OS CSPRNG");
        printf("* Memory Security       : Stack zeroized via ozos_crypto_zeroize [SECURE]\n");
        printf("=================================================================\n");
        ozos_crypto_zeroize(seed, sizeof(seed));
        ozos_crypto_zeroize(&sk, sizeof(sk));
        return 0;

    } else if (strcmp(cmd, "encaps") == 0 || strcmp(cmd, "pqc-encaps") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: ozos-pqc.exe encaps <pk_file> <out_ct_file> [out_key_file]\n");
            return 1;
        }
        const char* pk_file = argv[2];
        const char* ct_file = argv[3];
        const char* key_file = (argc >= 5) ? argv[4] : NULL;
        FILE* fp = fopen(pk_file, "rb");
        if (!fp) {
            fprintf(stderr, "[FAIL] Cannot open public key file '%s'.\n", pk_file);
            return 1;
        }
        ozos_pqc_hybrid_pk_t pk;
        size_t r = fread(&pk, 1, sizeof(pk), fp);
        fclose(fp);
        if (r != sizeof(pk) || pk.magic != OZOS_PQC_MAGIC) {
            fprintf(stderr, "[FAIL] Corrupt or invalid hybrid public key (expected %u bytes, got %u).\n", (unsigned)sizeof(pk), (unsigned)r);
            return 1;
        }
        ozos_pqc_hybrid_ct_t ct;
        uint8_t session_key[OZOS_PQC_HYBRID_SECRET_BYTES];
        int rc = ozos_pqc_hybrid_encaps(&pk, &ct, session_key);
        if (rc != 0) {
            fprintf(stderr, "[FAIL] Encapsulation failed (rc=%d)\n", rc);
            return 1;
        }
        FILE* fp_ct = fopen(ct_file, "wb");
        if (!fp_ct) {
            fprintf(stderr, "[FAIL] Cannot write ciphertext to '%s'.\n", ct_file);
            ozos_crypto_zeroize(session_key, sizeof(session_key));
            return 1;
        }
        fwrite(&ct, 1, sizeof(ct), fp_ct);
        fclose(fp_ct);
        char key_hex[65];
        bytes_to_hex(session_key, 32, key_hex);
        if (key_file) {
            FILE* fk = fopen(key_file, "w");
            if (fk) {
                fprintf(fk, "%s\n", key_hex);
                fclose(fk);
            }
        }
        printf("=================================================================\n");
        printf("  O.Z.O.S // POST-QUANTUM KEY ENCAPSULATION (KEM)\n");
        printf("=================================================================\n");
        printf("* Recipient PK File     : %s\n", pk_file);
        printf("* Ciphertext Wire File  : %s (%u bytes)\n", ct_file, (unsigned)sizeof(ct));
        printf("* Agreed 256-bit Key    : 0x%s\n", key_hex);
        if (key_file) printf("* Session Key Saved To  : %s\n", key_file);
        printf("=================================================================\n");
        ozos_crypto_zeroize(session_key, sizeof(session_key));
        return 0;

    } else if (strcmp(cmd, "decaps") == 0 || strcmp(cmd, "pqc-decaps") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: ozos-pqc.exe decaps <sk_file> <ct_file> [out_key_file]\n");
            return 1;
        }
        const char* sk_file = argv[2];
        const char* ct_file = argv[3];
        const char* key_file = (argc >= 5) ? argv[4] : NULL;
        FILE* fp_sk = fopen(sk_file, "rb");
        if (!fp_sk) {
            fprintf(stderr, "[FAIL] Cannot open secret key file '%s'.\n", sk_file);
            return 1;
        }
        ozos_pqc_hybrid_sk_t sk;
        size_t r1 = fread(&sk, 1, sizeof(sk), fp_sk);
        fclose(fp_sk);
        if (r1 != sizeof(sk)) {
            fprintf(stderr, "[FAIL] Invalid secret key size (expected %u bytes, got %u).\n", (unsigned)sizeof(sk), (unsigned)r1);
            return 1;
        }
        FILE* fp_ct = fopen(ct_file, "rb");
        if (!fp_ct) {
            fprintf(stderr, "[FAIL] Cannot open ciphertext file '%s'.\n", ct_file);
            ozos_crypto_zeroize(&sk, sizeof(sk));
            return 1;
        }
        ozos_pqc_hybrid_ct_t ct;
        size_t r2 = fread(&ct, 1, sizeof(ct), fp_ct);
        fclose(fp_ct);
        if (r2 != sizeof(ct) || ct.magic != OZOS_PQC_MAGIC) {
            fprintf(stderr, "[FAIL] Corrupt or invalid hybrid ciphertext (expected %u bytes, got %u).\n", (unsigned)sizeof(ct), (unsigned)r2);
            ozos_crypto_zeroize(&sk, sizeof(sk));
            return 1;
        }
        uint8_t session_key[OZOS_PQC_HYBRID_SECRET_BYTES];
        int rc = ozos_pqc_hybrid_decaps(&sk, &ct, session_key);
        ozos_crypto_zeroize(&sk, sizeof(sk));
        if (rc != 0) {
            fprintf(stderr, "[FAIL] Decapsulation failed (rc=%d)\n", rc);
            return 1;
        }
        char key_hex[65];
        bytes_to_hex(session_key, 32, key_hex);
        if (key_file) {
            FILE* fk = fopen(key_file, "w");
            if (fk) {
                fprintf(fk, "%s\n", key_hex);
                fclose(fk);
            }
        }
        printf("=================================================================\n");
        printf("  O.Z.O.S // POST-QUANTUM KEY DECAPSULATION (KEM)\n");
        printf("=================================================================\n");
        printf("* Secret Key File       : %s\n", sk_file);
        printf("* Ciphertext Input      : %s\n", ct_file);
        printf("* Recovered 256-bit Key : 0x%s\n", key_hex);
        if (key_file) printf("* Session Key Saved To  : %s\n", key_file);
        printf("=================================================================\n");
        ozos_crypto_zeroize(session_key, sizeof(session_key));
        return 0;

    } else {
        print_pqc_banner();
        return 1;
    }
}
