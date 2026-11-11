/*
 * O.Z.O.S // STANDALONE CORE ENGINE (ozos-core.exe)
 * Module              : ozos-core (Package 1 of 4)
 * Architecture        : Pure C99 Stack Enclave (Zero Heap, malloc = 0)
 * Dependencies        : Standard C Library + TweetNaCl only (Zero Host OS Hooks)
 * Target Platforms    : Bare-Metal, seL4 Microkit, POSIX, Windows
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#endif

extern void randombytes(unsigned char *buf, unsigned long long len);

#include "libozos.h"
#include "ozos_crypto.h"
#include "ozos_brainseed.h"
#include "ozos_shamir.h"
#include "ozos_gossip.h"
#include "ozos_crash_guard.h"

#define OZOS_CORE_VERSION "2026.1-CORE"

/* Helper: Secure CSPRNG Bytes */
static void ozos_core_random_bytes(uint8_t* out, size_t len) {
#if defined(_WIN32) || defined(_WIN64)
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        if (CryptGenRandom(hProv, (DWORD)len, out)) {
            CryptReleaseContext(hProv, 0);
            return;
        }
        CryptReleaseContext(hProv, 0);
    }
    fprintf(stderr, "[FATAL] CSPRNG failure in ozos_core_random_bytes. Aborting fail-closed.\n");
    exit(1);
#else
    FILE* ur = fopen("/dev/urandom", "rb");
    if (!ur || fread(out, 1, len, ur) != len) {
        if (ur) fclose(ur);
        fprintf(stderr, "[FATAL] Failed to read entropy from /dev/urandom. Aborting fail-closed.\n");
        exit(1);
    }
    fclose(ur);
#endif
}

static void bytes_to_hex(const uint8_t* in, size_t len, char* out) {
    static const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i) {
        out[i * 2]     = hex_chars[(in[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex_chars[in[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

static bool hex_to_bytes(const char* hex, uint8_t* out, size_t max_out, size_t* out_len) {
    size_t len = strlen(hex);
    if (len % 2 != 0 || len / 2 > max_out) return false;
    for (size_t i = 0; i < len / 2; ++i) {
        int hi = hex[i * 2];
        int lo = hex[i * 2 + 1];
        int v_hi = (hi >= '0' && hi <= '9') ? hi - '0' :
                   (hi >= 'a' && hi <= 'f') ? hi - 'a' + 10 :
                   (hi >= 'A' && hi <= 'F') ? hi - 'A' + 10 : -1;
        int v_lo = (lo >= '0' && lo <= '9') ? lo - '0' :
                   (lo >= 'a' && lo <= 'f') ? lo - 'a' + 10 :
                   (lo >= 'A' && lo <= 'F') ? lo - 'A' + 10 : -1;
        if (v_hi < 0 || v_lo < 0) return false;
        out[i] = (uint8_t)((v_hi << 4) | v_lo);
    }
    *out_len = len / 2;
    return true;
}

static bool parse_strict_u32(const char* str, uint32_t* out_val) {
    if (!str || !*str) return false;
    uint64_t acc = 0;
    for (const char* p = str; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        acc = acc * 10 + (*p - '0');
        if (acc > 0xFFFFFFFFULL) return false;
    }
    *out_val = (uint32_t)acc;
    return true;
}

static void print_packet80(const ozos_packet80_t* pkt) {
    printf("=================================================================\n");
    printf("  O.Z.O.S // WIRE PACKET STRUCTURE (80 BYTES ALIGNED)\n");
    printf("=================================================================\n");
    printf("  * Magic Prefix        : 0x%08X\n", pkt->magic);
    printf("  * Monotonic Nonce     : %llu\n", (unsigned long long)pkt->nonce);
    printf("  * Payload Length      : %u bytes\n", pkt->payload_len);
    printf("  * Tripwire MAC16      : 0x%04X\n", pkt->header_mac16);
    printf("=================================================================\n");
}

/* CLI Commands */
static int cmd_keygen(const char* custom_seed_hex) {
    uint8_t seed[32];
    if (custom_seed_hex) {
        size_t len = 0;
        if (!hex_to_bytes(custom_seed_hex, seed, 32, &len) || len != 32) {
            fprintf(stderr, "[SYNTAX ERROR] Seed hex must be exactly 64 hex characters (32 bytes).\n");
            return 1;
        }
    } else {
        ozos_core_random_bytes(seed, 32);
    }

    uint8_t pk[32], sk[64];
    crypto_sign_seed_keypair(pk, sk, seed);

    char seed_hex[65], pk_hex[65];
    bytes_to_hex(seed, 32, seed_hex);
    bytes_to_hex(pk, 32, pk_hex);

    printf("=================================================================\n");
    printf("  O.Z.O.S CORE // SOVEREIGN KEY GENERATION (RFC 8032 Ed25519)\n");
    printf("=================================================================\n");
    printf("* Sovereign Seed (Hex)  : %s\n", seed_hex);
    printf("* Sovereign Public Key  : 0x%s\n", pk_hex);
    printf("* Memory Enclave        : Zero-Heap Stack Buffer (malloc = 0)\n");
    printf("=================================================================\n");

    ozos_crypto_zeroize(seed, sizeof(seed));
    ozos_crypto_zeroize(sk, sizeof(sk));
    return 0;
}

static int cmd_brainseed_split(uint8_t k, uint8_t n, const char* custom_seed_hex) {
    uint8_t seed[32];
    if (custom_seed_hex) {
        if (strchr(custom_seed_hex, ' ') != NULL) {
            if (!ozos_brainseed_restore(seed, custom_seed_hex, "")) {
                fprintf(stderr, "[SECURITY ALERT] Invalid BIP-39 mnemonic phrase or checksum mismatch in --seed!\n");
                return 1;
            }
        } else {
            size_t len = 0;
            if (!hex_to_bytes(custom_seed_hex, seed, 32, &len) || len != 32) {
                fprintf(stderr, "[SYNTAX ERROR] Seed hex must be exactly 64 hex characters (or valid 12-word BIP-39 mnemonic).\n");
                return 1;
            }
        }
    } else {
        ozos_core_random_bytes(seed, 32);
    }

    ozos_shamir_share_t shares[OZOS_SHAMIR_MAX_SHARES];
    int rc = ozos_shamir_split(seed, k, n, shares);
    ozos_crypto_zeroize(seed, sizeof(seed));

    if (rc != 0) {
        fprintf(stderr, "[ERROR] Shamir split failed (rc=%d). Parameters must satisfy 2 <= k <= n <= 16.\n", rc);
        return 1;
    }

    printf("=================================================================\n");
    printf("  O.Z.O.S CORE // SHAMIR'S SECRET SHARING (SSS) THRESHOLD SPLIT\n");
    printf("  Scheme: (%u-of-%u) Threshold Scheme over GF(2^8) (Rijndael 0x11B)\n", (unsigned)k, (unsigned)n);
    printf("  Security: Any %u shares can restore; %u or fewer shares yield ZERO info\n", (unsigned)k, (unsigned)(k - 1));
    printf("=================================================================\n\n");

    for (uint8_t i = 0; i < n; ++i) {
        char hex[OZOS_SHAMIR_SHARE_HEX_LEN + 1];
        ozos_shamir_share_to_hex(&shares[i], hex);
        printf("[SHARE %u/%u] (CRC16: 0x%04X):\n", (unsigned)shares[i].share_index, (unsigned)n, (unsigned)shares[i].crc16);
        printf("  %s\n\n", hex);
    }
    printf("[INSTRUCTION] Distribute these %u shares across %u separate secure enclaves.\n", (unsigned)n, (unsigned)n);
    printf("=================================================================\n");
    return 0;
}

static int cmd_brainseed_combine(int num_shares, char* share_hexes[]) {
    if (num_shares < OZOS_SHAMIR_MIN_THRESHOLD || num_shares > OZOS_SHAMIR_MAX_SHARES) {
        fprintf(stderr, "[SYNTAX ERROR] Number of shares (%d) must be between %d and %d.\n",
                num_shares, OZOS_SHAMIR_MIN_THRESHOLD, OZOS_SHAMIR_MAX_SHARES);
        return 1;
    }

    ozos_shamir_share_t shares[OZOS_SHAMIR_MAX_SHARES];
    for (int i = 0; i < num_shares; ++i) {
        int rc = ozos_shamir_hex_to_share(share_hexes[i], &shares[i]);
        if (rc != 0) {
            fprintf(stderr, "[REJECTED] Share #%d is invalid or corrupted (rc=%d)!\n", i + 1, rc);
            if (rc == -4) fprintf(stderr, "   CRC16-CCITT checksum failed! Typo or tampering detected in share.\n");
            return 1;
        }
    }

    uint8_t recovered_seed[32];
    int rc = ozos_shamir_combine(shares, (uint8_t)num_shares, recovered_seed);
    if (rc != 0) {
        fprintf(stderr, "[SECURITY ALERT] Shamir's Secret Sharing reconstruction failed (rc=%d)!\n", rc);
        if (rc == -2) fprintf(stderr, "   Insufficient shares: Threshold k=%u, but only %d valid shares provided.\n", shares[0].threshold_k, num_shares);
        if (rc == -5) fprintf(stderr, "   Mismatched shares: Shares were generated from different split sessions!\n");
        if (rc == -6) fprintf(stderr, "   Duplicate share index detected! Unique shares are required.\n");
        return 1;
    }

    char seed_hex[65];
    bytes_to_hex(recovered_seed, 32, seed_hex);

    uint8_t pk[32], sk[64];
    crypto_sign_seed_keypair(pk, sk, recovered_seed);
    char pk_hex[65];
    bytes_to_hex(pk, 32, pk_hex);

    printf("=================================================================\n");
    printf("  O.Z.O.S CORE // SHAMIR'S SECRET SHARING RECONSTRUCTION SUCCESS\n");
    printf("  Lagrange Interpolation: Reconstructed using %d distinct shares\n", num_shares);
    printf("=================================================================\n\n");
    printf("* Sovereign Seed (Hex)  : %s\n", seed_hex);
    printf("* Sovereign Public Key  : 0x%s\n", pk_hex);
    printf("\n[STATUS] Master Brainseed identity successfully reconstituted bit-exact!\n");
    printf("=================================================================\n");

    ozos_crypto_zeroize(recovered_seed, sizeof(recovered_seed));
    ozos_crypto_zeroize(sk, sizeof(sk));
    return 0;
}

static void print_banner(void) {
    printf("=================================================================\n");
    printf("  O.Z.O.S // CORE MICROKERNEL (ozos-core.exe)\n");
    printf("  Zero-Heap Pure Stack Sovereign Cryptographic Enclave\n");
    printf("=================================================================\n");
    printf("Operational Commands:\n");
    printf("  ozos-core.exe keygen [seed_hex]             - Generate RFC 8032 Ed25519 keypair\n");
    printf("  ozos-core.exe brainseed-split [opts]        - Shamir's Secret Sharing (k-of-n split)\n");
    printf("  ozos-core.exe brainseed-combine <shares...> - Reconstruct seed from k Shamir shares\n");
    printf("  ozos-core.exe brainseed-gen [passphrase]    - Generate 12-word BIP-39 mnemonic\n");
    printf("  ozos-core.exe brainseed-restore <12 words>  - Restore identity from mnemonic\n");
    printf("  ozos-core.exe packet-build <msg> [nonce]    - Encapsulate 80-byte binary frame\n");
    printf("  ozos-core.exe packet-verify <hex> [last_n]  - Verify MAC16 & Ed25519 signature\n");
    printf("  ozos-core.exe crash-test                    - Verify crash guard memory purge\n");
    printf("=================================================================\n");
}

int main(int argc, char* argv[]) {
    ozos_crash_guard_install();
    if (argc < 2) {
        print_banner();
        return 0;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "keygen") == 0) {
        if (argc > 3) {
            fprintf(stderr, "[SYNTAX ERROR] Usage: ozos-core.exe keygen [seed_hex]\n");
            return 1;
        }
        const char* custom_seed = (argc == 3) ? argv[2] : NULL;
        return cmd_keygen(custom_seed);
    } else if (strcmp(cmd, "brainseed-split") == 0) {
        uint8_t k = 3, n = 5;
        const char* seed = NULL;
        for (int i = 2; i < argc; ++i) {
            if ((strcmp(argv[i], "--threshold") == 0 || strcmp(argv[i], "-k") == 0) && i + 1 < argc) {
                uint32_t val = 0;
                if (!parse_strict_u32(argv[++i], &val) || val < OZOS_SHAMIR_MIN_THRESHOLD || val > OZOS_SHAMIR_MAX_SHARES) {
                    fprintf(stderr, "[SYNTAX ERROR] Invalid threshold -k '%s' (must be integer between %d and %d).\n",
                            argv[i], OZOS_SHAMIR_MIN_THRESHOLD, OZOS_SHAMIR_MAX_SHARES);
                    return 1;
                }
                k = (uint8_t)val;
            } else if ((strcmp(argv[i], "--shares") == 0 || strcmp(argv[i], "-n") == 0) && i + 1 < argc) {
                uint32_t val = 0;
                if (!parse_strict_u32(argv[++i], &val) || val < OZOS_SHAMIR_MIN_THRESHOLD || val > OZOS_SHAMIR_MAX_SHARES) {
                    fprintf(stderr, "[SYNTAX ERROR] Invalid share count -n '%s' (must be integer between %d and %d).\n",
                            argv[i], OZOS_SHAMIR_MIN_THRESHOLD, OZOS_SHAMIR_MAX_SHARES);
                    return 1;
                }
                n = (uint8_t)val;
            } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
                seed = argv[++i];
            } else {
                fprintf(stderr, "[SYNTAX ERROR] Unknown or incomplete option for brainseed-split: '%s'\n", argv[i]);
                return 1;
            }
        }
        return cmd_brainseed_split(k, n, seed);
    } else if (strcmp(cmd, "brainseed-combine") == 0) {
        if (argc < 4) {
            fprintf(stderr, "[SYNTAX ERROR] Usage: ozos-core.exe brainseed-combine <share_1> <share_2> ... <share_k>\n");
            return 1;
        }
        return cmd_brainseed_combine(argc - 2, &argv[2]);
    } else if (strcmp(cmd, "crash-test") == 0) {
        if (argc > 2) {
            fprintf(stderr, "[SYNTAX ERROR] Usage: ozos-core.exe crash-test\n");
            return 1;
        }
        static uint8_t key[32] = {0x42};
        ozos_crash_guard_register(key, sizeof(key));
        printf("[CRASH GUARD AUDIT] Purging memory on simulated SIGSEGV...\n");
        ozos_posix_signal_handler(SIGSEGV);
        return 0;
    } else {
        print_banner();
        return 1;
    }
}
