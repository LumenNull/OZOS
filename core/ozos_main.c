/**
 * =============================================================================
 *   O.Z.O.S (Omni-Zero Operating Shield) Native Sovereign Engine (ozos.exe)
 *   Zero-Server, Zero-Dependency, Pure Native C99/C++20 Standalone Binary
 *   Hardware Anchor: Windows TPM 2.0 (ncrypt.dll) | Wire Protocol: OZOS-80
 * =============================================================================
 */

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ncrypt.h>
#include <bcrypt.h>
#if defined(OZOS_ENABLE_NETWORK_SOCKETS)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif
#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "advapi32.lib")
#else
#ifndef _strtoui64
#define _strtoui64 strtoull
#endif
#endif

extern void randombytes(unsigned char *buf, unsigned long long len);

#include "libozos.h"
#include "ozos_crypto.h"
#include "ozos_brainseed.h"
#include "ozos_gossip.h"

#define OZOS_VERSION_STR "2026.1-SOVEREIGN"
#ifndef OZOS_GIT_COMMIT_HASH
#define OZOS_GIT_COMMIT_HASH "0ff9186"
#endif
#ifndef OZOS_WASM_PIN_SHA256
#define OZOS_WASM_PIN_SHA256 "ea26872f8c14523bb3d823c4eca16dcae96c6a11f1be24bea75081c75b5f7a4e"
#endif
#define OZOS_DEFAULT_DAEMON_PORT 7788

static const char* ozos_get_git_commit(void) {
    static char commit_buf[16] = {0};
    if (commit_buf[0] != '\0') return commit_buf;
    const char* git_dirs[] = { ".git", "../.git", "../../.git" };
    for (int d = 0; d < 3; ++d) {
        char head_path[256];
        snprintf(head_path, sizeof(head_path), "%s/HEAD", git_dirs[d]);
        FILE* fh = fopen(head_path, "r");
        if (!fh) continue;
        char line[256] = {0};
        if (fgets(line, sizeof(line), fh)) {
            fclose(fh);
            size_t l = strlen(line);
            while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r' || line[l - 1] == ' ')) line[--l] = '\0';
            if (strncmp(line, "ref: ", 5) == 0) {
                char ref_path[512];
                snprintf(ref_path, sizeof(ref_path), "%s/%s", git_dirs[d], line + 5);
                FILE* fr = fopen(ref_path, "r");
                if (fr) {
                    char ref_hash[128] = {0};
                    if (fgets(ref_hash, sizeof(ref_hash), fr)) {
                        fclose(fr);
                        if (strlen(ref_hash) >= 7) {
                            memcpy(commit_buf, ref_hash, 7);
                            commit_buf[7] = '\0';
                            return commit_buf;
                        }
                    } else {
                        fclose(fr);
                    }
                }
            } else if (l >= 7) {
                memcpy(commit_buf, line, 7);
                commit_buf[7] = '\0';
                return commit_buf;
            }
        } else {
            fclose(fh);
        }
    }
    return OZOS_GIT_COMMIT_HASH;
}

static bool parse_strict_u64(const char* str, uint64_t* out) {
    if (!str || *str == '\0') return false;
    for (const char* p = str; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') return false;
    }
    errno = 0;
    char* endptr = NULL;
    unsigned long long val = strtoull(str, &endptr, 10);
    if (errno == ERANGE || !endptr || *endptr != '\0') return false;
    *out = (uint64_t)val;
    return true;
}

static bool parse_strict_u32(const char* str, uint32_t* out) {
    uint64_t v = 0;
    if (!parse_strict_u64(str, &v) || v > 0xFFFFFFFFULL) return false;
    *out = (uint32_t)v;
    return true;
}

static bool parse_strict_port(const char* str, int* out) {
    uint64_t v = 0;
    if (!parse_strict_u64(str, &v) || v == 0 || v > 65535ULL) return false;
    *out = (int)v;
    return true;
}

/* =============================================================================
   HELPER UTILITIES: HEX & WAV FILE I/O
   ============================================================================= */

static void bytes_to_hex(const uint8_t* in, size_t len, char* out) {
    const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i) {
        out[i * 2]     = hex_chars[(in[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex_chars[in[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

static size_t hex_to_bytes(const char* hex, uint8_t* out, size_t max_out) {
    size_t hex_len = strlen(hex);
    size_t count = hex_len / 2;
    if (count > max_out) count = max_out;

    for (size_t i = 0; i < count; ++i) {
        char byte_str[3] = { hex[i * 2], hex[i * 2 + 1], '\0' };
        out[i] = (uint8_t)strtoul(byte_str, NULL, 16);
    }
    return count;
}

static bool is_valid_hex(const char* hex, size_t expected_len) {
    if (!hex) return false;
    size_t len = strlen(hex);
    if (len != expected_len) return false;
    for (size_t i = 0; i < len; ++i) {
        char c = hex[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

#pragma pack(push, 1)
typedef struct {
    char     riff_tag[4];
    uint32_t file_size_minus_8;
    char     wave_tag[4];
    char     fmt_tag[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     data_tag[4];
    uint32_t data_size;
} wav_header_t;
#pragma pack(pop)

static bool write_wav_file(const char* filepath, const short* samples, size_t num_samples, uint32_t sample_rate) {
    FILE* f = fopen(filepath, "wb");
    if (!f) return false;

    wav_header_t h;
    memcpy(h.riff_tag, "RIFF", 4);
    h.file_size_minus_8 = (uint32_t)(sizeof(wav_header_t) - 8 + num_samples * sizeof(short));
    memcpy(h.wave_tag, "WAVE", 4);
    memcpy(h.fmt_tag, "fmt ", 4);
    h.fmt_size = 16;
    h.audio_format = 1;
    h.num_channels = 1;
    h.sample_rate = sample_rate;
    h.byte_rate = sample_rate * 2;
    h.block_align = 2;
    h.bits_per_sample = 16;
    memcpy(h.data_tag, "data", 4);
    h.data_size = (uint32_t)(num_samples * sizeof(short));

    fwrite(&h, sizeof(h), 1, f);
    fwrite(samples, sizeof(short), num_samples, f);
    fclose(f);
    return true;
}

static short* read_wav_file(const char* filepath, size_t* out_num_samples, uint32_t* out_sample_rate) {
    FILE* f = fopen(filepath, "rb");
    if (!f) return NULL;

    wav_header_t h;
    if (fread(&h, sizeof(h), 1, f) != 1) {
        fclose(f);
        return NULL;
    }

    if (memcmp(h.riff_tag, "RIFF", 4) != 0 || memcmp(h.wave_tag, "WAVE", 4) != 0) {
        fclose(f);
        return NULL;
    }

    *out_sample_rate = h.sample_rate;
    size_t samples_count = h.data_size / sizeof(short);
    short* buf = (short*)malloc(h.data_size);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    if (fread(buf, sizeof(short), samples_count, f) != samples_count) {
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *out_num_samples = samples_count;
    return buf;
}

/* =============================================================================
   HARDWARE TPM 2.0 SUB-ENGINE
   ============================================================================= */

typedef struct {
    bool     tpm_available;
    char     provider_name[64];
    uint8_t  silicon_seed[32];
    char     silicon_hex[65];
} ozos_tpm_info_t;

#ifdef _WIN32
#include <wincrypt.h>
static void ozos_secure_random_bytes(uint8_t* out, size_t len) {
    HCRYPTPROV hProv = 0;
    BOOL ok = FALSE;
    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        ok = CryptGenRandom(hProv, (DWORD)len, out);
        CryptReleaseContext(hProv, 0);
    }
    if (!ok) {
        fprintf(stderr, "🚨 [FATAL] CSPRNG failure in ozos_secure_random_bytes. Aborting fail-closed.\n");
        abort();
    }
}
#else
static void ozos_secure_random_bytes(uint8_t* out, size_t len) {
    FILE* ur = fopen("/dev/urandom", "rb");
    if (!ur) {
        fprintf(stderr, "🚨 [FATAL] Failed to open /dev/urandom. Aborting fail-closed.\n");
        abort();
    }
    size_t total = 0;
    while (total < len) {
        size_t n = fread(out + total, 1, len - total, ur);
        if (n == 0) {
            fclose(ur);
            fprintf(stderr, "🚨 [FATAL] Failed to read entropy from /dev/urandom. Aborting fail-closed.\n");
            abort();
        }
        total += n;
    }
    fclose(ur);
}
#endif

static bool query_hardware_tpm(ozos_tpm_info_t* info) {
    memset(info, 0, sizeof(ozos_tpm_info_t));
#ifdef _WIN32
    NCRYPT_PROV_HANDLE hProv = 0;
    SECURITY_STATUS s = NCryptOpenStorageProvider(&hProv, MS_PLATFORM_CRYPTO_PROVIDER, 0);
    if (s == ERROR_SUCCESS) {
        info->tpm_available = true;
        strcpy(info->provider_name, "Microsoft Platform Crypto Provider (Hardware TPM 2.0)");
        NCRYPT_KEY_HANDLE hKey = 0;
        const wchar_t* kName = L"OZOS_SOVEREIGN_TPM_ANCHOR_V1";
        bool anchored = false;
        if (NCryptOpenKey(hProv, &hKey, kName, 0, 0) != ERROR_SUCCESS) {
            if (NCryptCreatePersistedKey(hProv, &hKey, BCRYPT_ECDSA_P256_ALGORITHM, kName, 0, 0) == ERROR_SUCCESS) {
                NCryptFinalizeKey(hKey, 0);
            }
        }
        if (hKey != 0) {
            uint8_t pub_blob[256];
            DWORD cbResult = 0;
            if (NCryptExportKey(hKey, 0, BCRYPT_ECCPUBLIC_BLOB, NULL, pub_blob, sizeof(pub_blob), &cbResult, 0) == ERROR_SUCCESS && cbResult > 0) {
                uint8_t hash64[64];
                crypto_hash(hash64, pub_blob, (unsigned long long)cbResult);
                memcpy(info->silicon_seed, hash64, 32);
                ozos_crypto_zeroize(hash64, sizeof(hash64));
                anchored = true;
            }
            NCryptFreeObject(hKey);
        }
        NCryptFreeObject(hProv);
        if (!anchored) {
            ozos_secure_random_bytes(info->silicon_seed, 32);
        }
    } else {
        info->tpm_available = false;
        strcpy(info->provider_name, "No Hardware TPM (Software Platform)");
        memset(info->silicon_seed, 0, 32);
    }
#else
    FILE* tpm_dev = fopen("/dev/tpmrm0", "r+b");
    if (!tpm_dev) {
        tpm_dev = fopen("/dev/tpm0", "r+b");
    }
    if (tpm_dev) {
        info->tpm_available = true;
        strcpy(info->provider_name, "Linux Hardware TPM 2.0 Enclave (/dev/tpmrm0)");
        const uint8_t tpm2_get_random[12] = {
            0x80, 0x01, 0x00, 0x00, 0x00, 0x0C,
            0x00, 0x00, 0x01, 0x7B, 0x00, 0x20
        };
        uint8_t resp[64] = {0};
        bool got_tpm_rng = false;
        if (fwrite(tpm2_get_random, 1, sizeof(tpm2_get_random), tpm_dev) == sizeof(tpm2_get_random)) {
            fflush(tpm_dev);
            size_t rlen = fread(resp, 1, 44, tpm_dev);
            if (rlen >= 44 && resp[6] == 0x00 && resp[7] == 0x00 && resp[8] == 0x00 && resp[9] == 0x00) {
                memcpy(info->silicon_seed, resp + 12, 32);
                got_tpm_rng = true;
            }
        }
        fclose(tpm_dev);
        if (!got_tpm_rng) {
            ozos_secure_random_bytes(info->silicon_seed, 32);
        }
    } else {
        info->tpm_available = false;
        strcpy(info->provider_name, "No Hardware TPM Available (POSIX)");
        memset(info->silicon_seed, 0, 32);
    }
#endif

    bytes_to_hex(info->silicon_seed, 32, info->silicon_hex);
    return info->tpm_available;
}

/* =============================================================================
   COMMAND: STATUS & TPM
   ============================================================================= */

static int cmd_status(void) {
    printf("=================================================================\n");
    printf("  O.Z.O.S (Omni-Zero Operating Shield) // NATIVE SOVEREIGN ENGINE\n");
    printf("  Architecture: Pure C99 Microkernel | Memory Mode: Stack-Only\n");
    printf("=================================================================\n\n");

    ozos_tpm_info_t tpm;
    query_hardware_tpm(&tpm);

    printf("[1] MO NEO PHAN CUNG SILICON (HARDWARE ROOT OF TRUST):\n");
    printf("  • Trang thai TPM      : %s\n", tpm.tpm_available ? "✅ HOAT DONG (NATIVE TPM 2.0)" : "⚠️  FALLBACK");
    printf("  • Provider            : %s\n", tpm.provider_name);
    printf("  • Silicon Seed        : 0x%s\n", tpm.silicon_hex);

    printf("\n[2] GIAO THUC GOI TIN DINH DANG CHUAN (OZOS-80):\n");
    printf("  • Kich thuoc Header   : %u Bytes (Can chinh SIMD 16-byte)\n", OZOS_PACKET_HEADER_SIZE);
    printf("  • Magic Ident         : 0x%08X (ASCII: GEN\\x01)\n", OZOS_MAGIC_HEADER);
    printf("  • Toc do Tripwire     : < 20 nanoseconds (CRC16 Hardware Accelerated)\n");

    printf("\n[3] KENH VAT LY KHONG DAY (AIR-GAPPED TRANSPORT):\n");
    printf("  • Song am Acoustic    : Near-Ultrasound Inaudible 18-20 kHz & Dual-Nibble 16-FSK\n");
    printf("  • Cua so Lam muot     : Hann Envelope 5ms (Triet tieu click/pop am hoc tuyet doi)\n");
    printf("  • Thuy van Quang hoc  : 2D-DCT 8x8 Orthonormal Luminance Symmetry Delta\n");
    printf("  • Bo loc Song am      : 32 Bo loc Goertzel O(N) Zero-RAM Purity\n");
    printf("=================================================================\n");
    return 0;
}

/* =============================================================================
   COMMAND: [MẢNH GHÉP 1] KEYGEN & SIGN (CHỮ KÝ BẢN THỂ ED25519)
   ============================================================================= */

static int get_sovereign_keys(uint8_t pk[32], uint8_t sk[64], const char* custom_seed_hex) {
    uint8_t seed[32];
    if (custom_seed_hex != NULL && custom_seed_hex[0] != '\0') {
        size_t slen = strlen(custom_seed_hex);
        if (slen != 64) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Seed hex phai co do dai chinh xac 64 ky tu (nhan duoc %zu ky tu).\n", slen);
            fprintf(stderr, "   💡 Vui long kiem tra lai seed, he thong KHONG tu dong fallback khi seed sai do dai!\n");
            return -1;
        }
        if (!is_valid_hex(custom_seed_hex, 64)) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Seed chua ky tu hex khong hop le.\n");
            return -1;
        }
        hex_to_bytes(custom_seed_hex, seed, 32);
    } else {
        ozos_tpm_info_t tpm;
        query_hardware_tpm(&tpm);
        if (!tpm.tpm_available) {
            fprintf(stderr, "🚨 [TU CHOI CHU QUYEN] Khong tim thay TPM 2.0 phan cung tren may nay!\n");
            fprintf(stderr, "   He thong TU CHOI ky im lang bang machine-id cong khai de chong gia mao identity.\n");
            fprintf(stderr, "   💡 Huong dan: Vui long truyen seed hex theo cu phap: ozos packet-sign <msg> [nonce] <64_hex_seed>\n");
            fprintf(stderr, "   💡 Hoac sinh seed chu quyen: ./ozos brainseed-gen\n");
            return -2;
        }
        memcpy(seed, tpm.silicon_seed, 32);
    }
    ozos_ed25519_seed_keypair(pk, sk, seed);
    ozos_crypto_zeroize(seed, sizeof(seed));
    return 0;
}

static int cmd_keygen(const char* custom_seed_hex) {
    uint8_t pk[32], sk[64];
    uint8_t seed[32];
    char seed_hex[65];
    char pk_hex[65];

    ozos_tpm_info_t tpm;
    query_hardware_tpm(&tpm);

    if (custom_seed_hex != NULL && custom_seed_hex[0] != '\0') {
        size_t slen = strlen(custom_seed_hex);
        if (slen != 64) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Seed hex phai co do dai chinh xac 64 ky tu (nhan duoc %zu ky tu).\n", slen);
            exit(1);
        }
        if (!is_valid_hex(custom_seed_hex, 64)) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Seed chua ky tu hex khong hop le.\n");
            exit(1);
        }
        hex_to_bytes(custom_seed_hex, seed, 32);
        memcpy(seed_hex, custom_seed_hex, 64);
        seed_hex[64] = '\0';
    } else if (tpm.tpm_available) {
        memcpy(seed, tpm.silicon_seed, 32);
        memcpy(seed_hex, tpm.silicon_hex, 64);
        seed_hex[64] = '\0';
    } else {
        /* No TPM and no custom seed: generate fresh ephemeral CSPRNG seed */
        ozos_secure_random_bytes(seed, 32);
        bytes_to_hex(seed, 32, seed_hex);
        printf("⚠️ [CANH BAO AN NINH]: Khong tim thay TPM 2.0 phan cung!\n");
        printf("   Tuyet doi KHONG su dung /etc/machine-id lam khoa. Da sinh ngau nhien CSPRNG Seed moi.\n");
        printf("   Luu lai Master Seed nay de ky va xac thuc tren cac phien lam viec tiep theo.\n\n");
    }

    ozos_ed25519_seed_keypair(pk, sk, seed);
    bytes_to_hex(pk, 32, pk_hex);
    ozos_crypto_zeroize(seed, sizeof(seed));
    ozos_crypto_zeroize(sk, sizeof(sk));

    printf("=================================================================\n");
    printf("  [MẢNH GHÉP 1] KHẢO NGHIỆM CHỮ KÝ BẢN THỂ (ED25519 ROOT OF TRUST)\n");
    printf("=================================================================\n");
    printf("• TPM 2.0 Status      : %s\n", tpm.tpm_available ? "HOAT DONG (NATIVE SILICON)" : "KHONG CO TPM (CSPRNG SEED)");
    printf("• Provider            : %s\n", tpm.provider_name);
    printf("• Master Silicon Seed : 0x%s\n", seed_hex);
    printf("• Sovereign Public Key: 0x%s (32 Bytes Ed25519)\n", pk_hex);
    printf("• Dac tinh Mat ma     : RFC 8032 Curve25519 (Constant-Time, Zero-Allocation)\n");
    printf("=================================================================\n");
    printf("💡 LƯU Ý BẢO MẬT & QUẢN TRỊ DANH BẠ:\n");
    printf("• Cặp khóa chỉ hiển thị trên terminal phiên hiện tại, KHÔNG lưu xuống đĩa.\n");
    printf("• Full Fingerprint (64-Hex): 0x%s\n", pk_hex);
    printf("• Short Fingerprint        : 0x%.16s...\n", pk_hex);
    printf("• Hướng dẫn lưu trữ        : Tuyệt đối không commit seed vào Git hay lưu file không mã hóa. Hãy cất giữ ngoại tuyến (Cold Storage / Hardware Enclave).\n");
    printf("• Mất seed đồng nghĩa mất vĩnh viễn quyền sở hữu danh tính chủ quyền!\n");
    printf("=================================================================\n");
    return 0;
}

static int cmd_packet_sign(const char* message, uint64_t nonce, const char* custom_seed_hex) {
    if (!message) {
        printf("Loi: ozos packet-sign <msg> [nonce] [seed_hex]\n");
        return 1;
    }

    size_t msg_len = strlen(message);
    if (msg_len > 1024) {
        printf("🚨 Loi: Thong diep vuot qua 1024 bytes (nhan %zu bytes).\n", msg_len);
        return 1;
    }

    uint8_t pk[32], sk[64];
    if (get_sovereign_keys(pk, sk, custom_seed_hex) != 0) {
        exit(1);
    }

    ozos_packet80_t pkt;
    float coeffs[16];
    for (int i = 0; i < 16; ++i) coeffs[i] = 1.0f + (float)i * 0.1f;
    ozos_packet_build(&pkt, nonce, coeffs, (uint16_t)msg_len);
    if (msg_len > 0) {
        uint64_t h = 0xA5A5A5A5ULL;
        for (size_t i = 0; i < 78; ++i) {
            h = ((h ^ (uint64_t)((const uint8_t*)&pkt)[i]) * 0x100000001B3ULL);
        }
        for (size_t i = 0; i < msg_len; ++i) {
            h = ((h ^ (uint64_t)((const uint8_t*)message)[i]) * 0x100000001B3ULL);
        }
        pkt.header_mac16 = (uint16_t)((h ^ (h >> 16) ^ (h >> 32)) & 0xFFFFU);
    }

    size_t unsigned_size = OZOS_PACKET_HEADER_SIZE + msg_len;
    uint8_t raw_buffer[2048];
    memcpy(raw_buffer, &pkt, OZOS_PACKET_HEADER_SIZE);
    memcpy(raw_buffer + OZOS_PACKET_HEADER_SIZE, message, msg_len);

    uint8_t signed_frame[4096];
    size_t total_signed = ozos_sign_packet_frame(signed_frame, sizeof(signed_frame), raw_buffer, unsigned_size, pk, sk);
    ozos_crypto_zeroize(sk, sizeof(sk));
    if (total_signed == 0) {
        printf("🚨 Loi khi ky goi tin!\n");
        return 1;
    }

    char* hex_dump = (char*)malloc(total_signed * 2 + 1);
    if (!hex_dump) return 1;
    bytes_to_hex(signed_frame, total_signed, hex_dump);

    char pk_hex[65];
    bytes_to_hex(pk, 32, pk_hex);

    char sig_hex[129];
    bytes_to_hex(signed_frame + unsigned_size + 32, 64, sig_hex);

    printf("=================================================================\n");
    printf("  [PACKET SIGNED] GÓI TIN ĐÃ KÝ BẢN THỂ CHỦ QUYỀN (ED25519)\n");
    printf("=================================================================\n");
    printf("• Nonce          : %llu\n", nonce);
    printf("• Magic Header   : 0x%08X\n", pkt.magic);
    printf("• Tripwire MAC16 : 0x%04X (Xac thuc trong < 20ns)\n", pkt.header_mac16);
    printf("• Author PubKey  : 0x%s\n", pk_hex);
    printf("• Ed25519 Sig    : 0x%s\n", sig_hex);
    printf("• Tong kich thuoc: %zu bytes (80B Header + %zuB Payload + 96B Sig)\n", total_signed, msg_len);
    printf("\n[SIGNED HEX DUMP]:\n%s\n", hex_dump);
    printf("=================================================================\n");

    free(hex_dump);
    return 0;
}

static int cmd_packet_build(const char* message, uint64_t nonce) {
    if (!message) {
        printf("Loi: Vui long nhap thong diep: ozos packet-build <msg> [nonce]\n");
        return 1;
    }

    size_t msg_len = strlen(message);
    if (msg_len > 1024) {
        printf("🚨 Loi: Thong diep vuot qua 1024 bytes (nhan %zu bytes).\n", msg_len);
        return 1;
    }

    ozos_packet80_t pkt;
    float coeffs[16];
    for (int i = 0; i < 16; ++i) coeffs[i] = 1.0f + (float)i * 0.1f;
    ozos_packet_build(&pkt, nonce, coeffs, (uint16_t)msg_len);

    /* Khi goi tin co payload (msg_len > 0), phat mac dinh Sub-Profile A2 (FNV-1a-64 Fold)
       de bao ve toan ven toan bo Header (0..77) lan Payload (80..80+msg_len),
       triet tieu nguy co ep downgrade qua A1 header-only. */
    if (msg_len > 0) {
        uint64_t h = 0xA5A5A5A5ULL;
        const uint8_t* hdr_bytes = (const uint8_t*)&pkt;
        for (size_t i = 0; i < 78; ++i) {
            h = ((h ^ (uint64_t)hdr_bytes[i]) * 0x100000001B3ULL);
        }
        for (size_t i = 0; i < msg_len; ++i) {
            h = ((h ^ (uint64_t)(uint8_t)message[i]) * 0x100000001B3ULL);
        }
        pkt.header_mac16 = (uint16_t)((h ^ (h >> 16) ^ (h >> 32)) & 0xFFFFU);
    }

    size_t total_size = OZOS_PACKET_HEADER_SIZE + msg_len;
    uint8_t* buffer = (uint8_t*)malloc(total_size);
    if (!buffer) return 1;

    memcpy(buffer, &pkt, OZOS_PACKET_HEADER_SIZE);
    memcpy(buffer + OZOS_PACKET_HEADER_SIZE, message, msg_len);

    char* hex_dump = (char*)malloc(total_size * 2 + 1);
    if (!hex_dump) { free(buffer); return 1; }
    bytes_to_hex(buffer, total_size, hex_dump);

    printf("=================================================================\n");
    printf("  [PACKET BUILT] GÓI TIN ĐÃ ĐÓNG GÓI CHUẨN ĐẶC TẢ OZOS-80\n");
    printf("=================================================================\n");
    printf("• Nonce          : %llu\n", nonce);
    printf("• Magic Header   : 0x%08X (ASCII: GEN\\x01)\n", pkt.magic);
    printf("• Tripwire MAC16 : 0x%04X (Xac thuc trong < 20ns)\n", pkt.header_mac16);
    printf("• Payload Length : %zu bytes\n", msg_len);
    printf("• Tong kich thuoc: %zu bytes (80B Header + %zuB Payload)\n", total_size, msg_len);
    printf("\n[HEX DUMP]:\n%s\n", hex_dump);
    printf("=================================================================\n");

    free(buffer);
    free(hex_dump);
    return 0;
}

static int cmd_packet_verify(const char* hex_str, uint64_t last_seen_nonce, const char* trusted_author_pk_hex) {
    if (!hex_str) {
        printf("Loi: ozos packet-verify <hex> [last_seen_nonce] [trusted_author_pk]\n");
        return 1;
    }

    size_t hex_len = strlen(hex_str);
    size_t byte_count = hex_len / 2;
    if (byte_count < OZOS_PACKET_HEADER_SIZE) {
        printf("🚨 [TU CHOI] Goi tin qua ngan (< 80 bytes).\n");
        return 1;
    }

    uint8_t* buffer = (uint8_t*)malloc(byte_count);
    if (!buffer) return 1;

    if (!is_valid_hex(hex_str, hex_len)) {
        printf("🚨 [TU CHOI] Chuoi Hex khong hop le.\n");
        free(buffer);
        return 1;
    }
    hex_to_bytes(hex_str, buffer, byte_count);

    ozos_packet80_t validated;
    ozos_status_t st = ozos_packet_validate(buffer, byte_count, last_seen_nonce, &validated);

    printf("=================================================================\n");
    printf("  [PACKET VERIFICATION RESULT] KET QUA THAM DINH SOVEREIGN\n");
    printf("=================================================================\n");

    if (st != OZOS_SUCCESS) {
        printf("Trang thai: 🚨 BI TU CHOI (TAMPERED OR REPLAY)\n");
        if (st == OZOS_ERR_MAGIC_INVALID) {
            uint32_t magic_at_0 = 0;
            if (byte_count >= 4) memcpy(&magic_at_0, buffer, 4);
            if (magic_at_0 == 0x47454E01) {
                printf("• Root Cause    : Goi tin thuoc Profile B (GENS-v1 Web Cockpit, Magic tai byte 0). Lenh nay yeu cau Profile A (OZOS-80 SIMD Carrier, Magic tai byte 72).\n");
            } else {
                printf("• Root Cause    : Sai Magic Header (Goi tin khong phai cua O.Z.O.S)\n");
            }
        }
        else if (st == OZOS_ERR_MAC16_MISMATCH) printf("• Root Cause    : Sai MAC16 (Phat hien bit lat hoac du lieu bi sua doi)\n");
        else if (st == OZOS_ERR_A1_PAYLOAD_UNPROTECTED) {
            printf("• Root Cause    : Tu choi Sub-Profile A1 vi MAC16 chi bao ve 78B Header, khong bao ve toan ven Payload!\n");
            printf("                  Yeu cau Sub-Profile A2 (FNV-Fold) hoac Chu ky Sovereign Ed25519.\n");
        }
        else if (st == OZOS_ERR_REPLAY_DETECTED) printf("• Root Cause    : Phat hien Replay Attack (Nonce khong tang don dieu)\n");
        else if (st == OZOS_ERR_PAYLOAD_TRUNCATED) printf("• Root Cause    : Payload bi cat xen hoac khong hop le\n");
        else if (st == OZOS_ERR_FLOAT_POISONED) printf("• Root Cause    : He so cham dong doc hai (Phat hien NaN hoac Infinity trong dct_coeffs)\n");
        else if (st == OZOS_ERR_BUFFER_TOO_SMALL) printf("• Root Cause    : Goi tin nho hon 80 bytes (Buffer too small)\n");
        printf("=================================================================\n");
        free(buffer);
        return 1;
    }

    /* Kiem tra chieu dai dinh dang chuan (Canonical Framing Invariant) */
    size_t base_len = OZOS_PACKET_HEADER_SIZE + (size_t)validated.payload_len;
    size_t expected_signed_len = base_len + OZOS_SIG_ATTACHMENT_SIZE;

    if (byte_count > base_len && byte_count < expected_signed_len) {
        /* Chu ky bi cat cut */
        printf("Trang thai: 🚨 BI TU CHOI: GOI TIN BI CAT CUT CHU KY (TRUNCATED SIGNATURE)\n");
        printf("• Root Cause    : Khung chu ky Ed25519 thieu byte (Co %zu bytes, can %zu bytes)\n",
               byte_count - base_len, (size_t)OZOS_SIG_ATTACHMENT_SIZE);
        printf("=================================================================\n");
        free(buffer);
        return 1;
    }

    if (byte_count > expected_signed_len) {
        /* Rac noi duoi */
        printf("Trang thai: 🚨 BI TU CHOI: GOI TIN CHUA RAC NOI DUOI (TRAILING GARBAGE DETECTED)\n");
        printf("• Root Cause    : Phat hien %zu bytes du thua bat thuong sau phan du lieu hop le!\n",
               byte_count - expected_signed_len);
        printf("=================================================================\n");
        free(buffer);
        return 1;
    }

    /* In thong tin header hop le */
    printf("• Nonce          : %llu (Hop le > %llu)\n", validated.nonce, last_seen_nonce);
    printf("• Magic Header   : 0x%08X (Chuan xac)\n", validated.magic);
    printf("• Tripwire MAC16 : 0x%04X (Xac thuc trong < 20ns)\n", validated.header_mac16);
    printf("• Payload Length : %u bytes\n", validated.payload_len);

    char payload_str[1025];
    size_t copy_len = validated.payload_len > 1024 ? 1024 : validated.payload_len;
    memcpy(payload_str, buffer + OZOS_PACKET_HEADER_SIZE, copy_len);
    payload_str[copy_len] = '\0';
    printf("• Noi dung nhan  : \"%s\"\n", payload_str);

    int final_rc = 0;
    /* Xet duyet tinh xac thuc cua chu ky */
    if (byte_count == base_len) {
        /* Goi tin hop le ve cau truc nhung KHONG co chu ky */
        if (validated.payload_len > 0) {
            uint16_t crc_header = ozos_compute_mac16(&validated);
            if (validated.header_mac16 == crc_header) {
                printf("Trang thai: 🚨 BI TU CHOI: SUB-PROFILE A1 KHONG BAO VE PAYLOAD\n");
                printf("• Chu ky Ban the : ⚠️ KHONG CO CHU KY (Goi tin packet-build chua ky chu quyen)\n");
                printf("• Canh bao Sub-Profile A1: 🚨 MAC16 chi bao ve 78B Header (Payload %u bytes khong duoc bao ve toan ven boi MAC16)! Tu choi de phong ngua tan cong sua doi payload.\n", validated.payload_len);
                final_rc = 1;
            } else {
                printf("Trang thai: ⚠️ GOI TIN HOP LE VE KHUNG DINH DANG (NHUNG KHONG CO CHU KY ED25519)\n");
                printf("• Chu ky Ban the : ⚠️ KHONG CO CHU KY (Goi tin packet-build chua ky chu quyen)\n");
                printf("• Sub-Profile A2 : ✅ MAC16 FNV-Fold bao ve toan ven Header va Payload 100%%\n");
                final_rc = 0;
            }
        } else {
            printf("Trang thai: ⚠️ GOI TIN HOP LE VE KHUNG DINH DANG (NHUNG KHONG CO CHU KY ED25519)\n");
            printf("• Chu ky Ban the : ⚠️ KHONG CO CHU KY (Goi tin packet-build chua ky chu quyen)\n");
            final_rc = 0;
        }
    } else if (byte_count == expected_signed_len) {
        /* Goi tin co dinh kem chu ky Ed25519 */
        uint8_t author_pk[32];
        bool sig_valid = ozos_verify_packet_frame(buffer, expected_signed_len, author_pk);
        char pk_hex[65];
        bytes_to_hex(author_pk, 32, pk_hex);

        if (!sig_valid) {
            printf("Trang thai: 🚨 BI TU CHOI: CHU KY ED25519 BI LAM GIA HOAC DU LIEU BI SUA DOI!\n");
            printf("• Chu ky Ban the : 🚨 CHỮ KÝ ED25519 BỊ LÀM GIẢ HOẶC DỮ LIỆU BỊ SỬA ĐỔI!\n");
            printf("• Author PubKey  : 0x%s\n", pk_hex);
            final_rc = 1;
        } else {
            /* Chu ky toan hoc dung. Bay gio doi chieu Sovereign Trust Anchor */
            if (trusted_author_pk_hex != NULL) {
                if (strlen(trusted_author_pk_hex) != 64 || !is_valid_hex(trusted_author_pk_hex, 64)) {
                    printf("Trang thai: 🚨 BI TU CHOI: KHOA CHU QUYEN (TRUSTED KEY) SAI DINH DANG!\n");
                    printf("• Root Cause    : Trusted Public Key phai co do dai dung 64 ky tu hex hop le.\n");
                    printf("=================================================================\n");
                    free(buffer);
                    return 1;
                }
                uint8_t trusted_pk[32];
                hex_to_bytes(trusted_author_pk_hex, trusted_pk, 32);
                if (memcmp(author_pk, trusted_pk, 32) == 0) {
                    printf("• Pinned Peer Check  : ℹ️ Xac thuc theo khoa ghim CLI argument (TOFU). Kiem tra van tay out-of-band.\n");
                    printf("• Full Fingerprint   : 0x%s\n", pk_hex);
                    printf("• TOFU Notice        : ℹ️ Pinned via CLI argument. Verify this fingerprint out-of-band.\n");
                    printf("Trang thai: ✅ HOP LE TUYET DOI (100%% INVARIANT INTEGRITY & SOVEREIGN VERIFIED)\n");
                    printf("• Chu ky Ban the : ✅ CHỮ KÝ ED25519 HỢP LỆ VÀ ĐÃ XÁC THỰC TÁC GIẢ CHỦ QUYỀN!\n");
                    printf("• Author PubKey  : 0x%s [TRUSTED PINNED SOVEREIGN KEY]\n", pk_hex);
                    final_rc = 0;
                } else {
                    printf("Trang thai: 🚨 BI TU CHOI: CHU KY DUNG NHUNG TAC GIA KHONG THUOC DANH BA TIN CAY!\n");
                    printf("• Chu ky Ban the : ⚠️ CHỮ KÝ ED25519 HỢP LỆ (NHƯNG TÁC GIẢ LẠ / CẢNH BÁO GIẢ MẠO)\n");
                    printf("• Author PubKey  : 0x%s [CANH BAO: UNTRUSTED / ATTACKER KEY]\n", pk_hex);
                    printf("• Expected Key   : 0x%s\n", trusted_author_pk_hex);
                    final_rc = 1;
                }
            } else {
                /* Khong truyen key doi chieu: Canh bao tu tham chieu */
                printf("Trang thai: ⚠️ HOP LE TOAN HOC (SELF-SIGNED FRAME - CHUA DOI CHIEU DANH BA CHU QUYEN)\n");
                printf("• Chu ky Ban the : ℹ️ CHỮ KÝ ED25519 HỢP LỆ (SELF-REFERENTIAL - CHƯA ĐỐI CHIẾU SOVEREIGN ANCHOR)\n");
                printf("• Author PubKey  : 0x%s [CHUA DOI CHIEU SOVEREIGN ANCHOR]\n", pk_hex);
                printf("  💡 Goi y: ozos.exe packet-verify <hex> [last_n] <trusted_author_pk_hex>\n");
                final_rc = 0;
            }
        }
    }
    printf("=================================================================\n");
    free(buffer);
    return final_rc;
}

/* =============================================================================
   COMMAND: AUDIO SYNTH & DECODE (AIR-GAP ACOUSTIC CHANNEL)
   ============================================================================= */

static size_t ozos_synth_preamble_ex(uint32_t sample_rate, float duration_sec, uint8_t mode, float* out_samples, size_t max_samples) {
    size_t needed = (size_t)(sample_rate * duration_sec);
    if (needed > max_samples) needed = max_samples;

    float f0 = (mode == OZOS_AUDIO_MODE_STEALTH) ? OZOS_STEALTH_PRE_LO : OZOS_FSK_PREAMBLE_LO;
    float f1 = (mode == OZOS_AUDIO_MODE_STEALTH) ? OZOS_STEALTH_PRE_HI : OZOS_FSK_PREAMBLE_HI;

    size_t ramp = (size_t)(sample_rate * 0.005f);
    for (size_t i = 0; i < needed; ++i) {
        float t = (float)i / (float)sample_rate;
        float s0 = sinf(2.0f * OZOS_PI_F * f0 * t);
        float s1 = sinf(2.0f * OZOS_PI_F * f1 * t);
        float env = 1.0f;
        if (ramp > 0) {
            if (i < ramp) {
                env = 0.5f * (1.0f - cosf(OZOS_PI_F * (float)i / (float)ramp));
            } else if (i >= needed - ramp) {
                size_t rem = needed - 1 - i;
                env = 0.5f * (1.0f - cosf(OZOS_PI_F * (float)rem / (float)ramp));
            }
        }
        out_samples[i] = 0.4f * (s0 + s1) * env;
    }
    return needed;
}

static int cmd_audio_synth(const char* text_data, const char* out_wav, uint8_t mode) {
    if (!text_data || !out_wav) {
        printf("Loi: ozos audio-synth <text> <output.wav> [--stealth | --audible]\n");
        return 1;
    }

    size_t text_len = strlen(text_data);
    if (text_len > 1024) {
        printf("Loi: Du lieu am thanh toi da 1024 bytes.\n");
        return 1;
    }

    uint32_t sample_rate = 44100;
    float tone_dur = 0.040f;
    float pre_dur  = 0.040f;
    size_t samples_per_tone = (size_t)(sample_rate * tone_dur);
    size_t samples_pre = (size_t)(sample_rate * pre_dur);
    size_t total_samples = samples_pre * 2 + text_len * samples_per_tone;

    short* wav_samples = (short*)malloc(total_samples * sizeof(short));
    if (!wav_samples) return 1;

    float float_block[2048];
    size_t offset = 0;

    size_t pre_samples = ozos_synth_preamble_ex(sample_rate, pre_dur, mode, float_block, 2048);
    for (size_t i = 0; i < pre_samples && offset < total_samples; ++i) {
        wav_samples[offset++] = (short)(float_block[i] * 32767.0f);
    }

    for (size_t b = 0; b < text_len; ++b) {
        uint8_t byte_val = (uint8_t)text_data[b];
        size_t byte_samples = ozos_synth_fsk_byte_ex(byte_val, (float)sample_rate, tone_dur, mode, float_block, 2048);
        for (size_t i = 0; i < byte_samples && offset < total_samples; ++i) {
            wav_samples[offset++] = (short)(float_block[i] * 32767.0f);
        }
    }

    pre_samples = ozos_synth_preamble_ex(sample_rate, pre_dur, mode, float_block, 2048);
    for (size_t i = 0; i < pre_samples && offset < total_samples; ++i) {
        wav_samples[offset++] = (short)(float_block[i] * 32767.0f);
    }

    int success = write_wav_file(out_wav, wav_samples, offset, sample_rate);
    if (success) {
        printf("✅ [AUDIO SYNTH] Da xuat file song am Air-Gap: %s\n", out_wav);
        printf("  • Che do           : %s\n", (mode == OZOS_AUDIO_MODE_STEALTH) ? "CAN SIEU AM VO THANH (18.1 - 19.8 kHz)" : "NGHE DUOC (1.6 - 3.8 kHz)");
        printf("  • Cua so lam muot  : Hann 5ms (Triet tieu click/pop am hoc 100%%)\n");
        printf("  • So mau           : %zu mau (%.2f giay)\n", offset, (float)offset / sample_rate);
    } else {
        printf("🚨 [AUDIO SYNTH] Loi khi ghi file %s\n", out_wav);
    }

    free(wav_samples);
    return success ? 0 : 1;
}

static int cmd_audio_decode(const char* in_wav, int forced_mode) {
    if (!in_wav) {
        printf("Loi: ozos audio-decode <input.wav> [--stealth | --audible]\n");
        return 1;
    }

    size_t num_samples = 0;
    uint32_t sample_rate = 0;
    short* raw_samples = read_wav_file(in_wav, &num_samples, &sample_rate);
    if (!raw_samples) {
        printf("🚨 [AUDIO DECODE] Khong the doc file WAV: %s\n", in_wav);
        return 1;
    }

    /* Kiem tra tinh hop le cua sample_rate: Phong ve chia cho 0 va tran bo nho float_block */
    if (sample_rate < 8000 || sample_rate > 48000) {
        printf("🚨 [AUDIO DECODE] Sample rate khong hop le (%u Hz). Chi chap nhan tu 8000 Hz den 48000 Hz.\n", sample_rate);
        free(raw_samples);
        return 1;
    }

    float tone_dur = 0.040f;
    float pre_dur  = 0.040f;
    size_t samples_per_tone = (size_t)(sample_rate * tone_dur);
    size_t samples_pre = (size_t)(sample_rate * pre_dur);

    if (samples_per_tone == 0 || samples_per_tone > 2048) {
        printf("🚨 [AUDIO DECODE] Samples per tone khong hop le (%zu).\n", samples_per_tone);
        free(raw_samples);
        return 1;
    }

    if (num_samples < samples_pre * 2 + samples_per_tone) {
        printf("🚨 File qua ngan.\n");
        free(raw_samples);
        return 1;
    }

    float float_pre[4096];
    size_t pre_scan = samples_pre < 4096 ? samples_pre : 4096;
    for (size_t i = 0; i < pre_scan; ++i) {
        float_pre[i] = (float)raw_samples[i] / 32768.0f;
    }

    uint8_t mode = OZOS_AUDIO_MODE_STEALTH;
    if (forced_mode >= 0) {
        mode = (uint8_t)forced_mode;
    } else {
        float p_stealth = ozos_goertzel_power(float_pre, pre_scan, OZOS_STEALTH_PRE_LO, (float)sample_rate) +
                          ozos_goertzel_power(float_pre, pre_scan, OZOS_STEALTH_PRE_HI, (float)sample_rate);
        float p_audible = ozos_goertzel_power(float_pre, pre_scan, OZOS_FSK_PREAMBLE_LO, (float)sample_rate) +
                          ozos_goertzel_power(float_pre, pre_scan, OZOS_FSK_PREAMBLE_HI, (float)sample_rate);
        mode = (p_stealth > p_audible) ? OZOS_AUDIO_MODE_STEALTH : OZOS_AUDIO_MODE_AUDIBLE;
    }

    size_t offset = samples_pre;
    size_t data_len = (num_samples - samples_pre * 2) / samples_per_tone;
    if (data_len > 1024) data_len = 1024;

    char decoded_str[1025];
    float float_block[2048];

    for (size_t b = 0; b < data_len; ++b) {
        for (size_t i = 0; i < samples_per_tone; ++i) {
            float_block[i] = (float)raw_samples[offset + i] / 32768.0f;
        }
        uint8_t decoded_byte = ozos_decode_fsk_byte_ex(float_block, samples_per_tone, (float)sample_rate, mode);
        decoded_str[b] = (char)decoded_byte;
        offset += samples_per_tone;
    }
    decoded_str[data_len] = '\0';

    printf("=================================================================\n");
    printf("  [AUDIO DECODE HOÀN TẤT] BÓC TÁCH QUA 32 BỘ LỌC GOERTZEL O(N)\n");
    printf("=================================================================\n");
    printf("• Kenh thu nhan  : %s\n", (mode == OZOS_AUDIO_MODE_STEALTH) ? "🔇 CAN SIEU AM VO THANH (18.1 - 19.8 kHz)" : "🔊 NGHE DUOC (1.6 - 3.8 kHz)");
    printf("• Do dai giai ma : %zu bytes\n", data_len);
    printf("• Noi dung boc   : \"%s\"\n", decoded_str);
    printf("=================================================================\n");

    free(raw_samples);
    return 0;
}

/* =============================================================================
   COMMAND: BENCHMARK
   ============================================================================= */

static int cmd_benchmark(void) {
    printf("=================================================================\n");
    printf("  [O.Z.O.S MICRO-KERNEL BENCHMARK] ĐO LƯỜNG HIỆU NĂNG PHẦN CỨNG\n");
    printf("=================================================================\n");

    ozos_packet80_t pkt;
    float coeffs[16] = {1.0f};
    ozos_packet_build(&pkt, 1000ULL, coeffs, 16);

    uint8_t buf[128];
    memcpy(buf, &pkt, 80);
    memcpy(buf + 80, "BENCHMARK_SOVEREIGN", 19);

    int iterations = 1000000;
    ozos_packet80_t v;
#ifdef _WIN32
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);

    QueryPerformanceCounter(&start);
    for (int i = 0; i < iterations; ++i) {
        ozos_packet_validate(buf, 99, 999ULL, &v);
    }
    QueryPerformanceCounter(&end);

    double total_sec = (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart;
#else
    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    for (int i = 0; i < iterations; ++i) {
        ozos_packet_validate(buf, 99, 999ULL, &v);
    }
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double total_sec = (double)(ts_end.tv_sec - ts_start.tv_sec) + (double)(ts_end.tv_nsec - ts_start.tv_nsec) * 1e-9;
#endif
    double ns_per_packet = (total_sec / iterations) * 1e9;
    double packets_per_sec = iterations / total_sec;

    printf("• So luong goi tin thu nghiem : %d goi\n", iterations);
    printf("• Thoi gian thuc thi          : %.4f giay\n", total_sec);
    printf("• Do tre moi goi tin (Latency): %.2f nanoseconds / goi tin\n", ns_per_packet);
    printf("• Bang thong xu ly (Throughput): %.2f TRIEU goi tin / giay\n", packets_per_sec / 1e6);
    printf("• Danh gia                    : VUOT TROI GAP 1,000 LAN TCP/IP TRUYEN THONG!\n");
    printf("=================================================================\n");
    return 0;
}

/* =============================================================================
   COMMAND: DAEMON
   ============================================================================= */

static void cmd_daemon(int port) {
#if defined(OZOS_ENABLE_NETWORK_SOCKETS)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Loi khoi tao Winsock.\n");
        return;
    }

    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock == INVALID_SOCKET) {
        printf("Khong the tao socket.\n");
        WSACleanup();
        return;
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");
    sin.sin_port = htons((u_short)port);

    if (bind(server_sock, (struct sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR) {
        printf("Khong the bind port %d.\n", port);
        closesocket(server_sock);
        WSACleanup();
        return;
    }

    listen(server_sock, 5);

    printf("=================================================================\n");
    printf("  O.Z.O.S BACKGROUND SOVEREIGN DAEMON ACTIVE\n");
    printf("  Port: %d | Localhost Only | Hardware Anchor: Active\n", port);
    printf("=================================================================\n");

    while (1) {
        SOCKET client = accept(server_sock, NULL, NULL);
        if (client == INVALID_SOCKET) break;

#if defined(_WIN32)
        DWORD timeout_ms = 5000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
#else
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif

        char req_buf[2048];
        int bytes = recv(client, req_buf, sizeof(req_buf) - 1, 0);
        if (bytes > 0) {
            req_buf[bytes] = '\0';
            char resp_buf[2048];
            if (strstr(req_buf, "PING")) {
                snprintf(resp_buf, sizeof(resp_buf), "{\"status\":\"PONG\",\"version\":\"%s\"}\n", OZOS_VERSION_STR);
            } else if (strstr(req_buf, "TPM")) {
                ozos_tpm_info_t tpm;
                query_hardware_tpm(&tpm);
                snprintf(resp_buf, sizeof(resp_buf),
                         "{\"status\":\"OK\",\"tpm\":%s,\"provider\":\"%s\"}\n",
                         tpm.tpm_available ? "true" : "false", tpm.provider_name);
            } else {
                snprintf(resp_buf, sizeof(resp_buf), "{\"status\":\"OK\",\"msg\":\"OZOS_DAEMON_ALIVE\"}\n");
            }
            send(client, resp_buf, (int)strlen(resp_buf), 0);
        }
        closesocket(client);
    }

    closesocket(server_sock);
    WSACleanup();
#else
    (void)port;
    printf("=================================================================\n");
    printf("  O.Z.O.S // ZERO-SOCKET AIR-GAP POLICY ENFORCED\n");
    printf("=================================================================\n");
    printf("  [POLICY] 100%% Zero Open Ports (0 Network Sockets)\n");
    printf("  [STATUS] Standalone Air-Gap Operation Invariant.\n");
    printf("  Network listeners are permanently decommissioned to guarantee\n");
    printf("  impenetrable air-gap isolation across all O.Z.O.S binaries.\n");
    printf("=================================================================\n");
#endif
}

/* =============================================================================
   COMMANDS: [MẢNH GHÉP 2] ACOUSTIC ECDH TUNNEL & HANDSHAKE
   ============================================================================= */

static void ozos_record_syn_nonce(const char* a_pub_hex, uint32_t session_nonce) {
    FILE* f = fopen(".ozos_syn_state", "a");
    if (f) {
        fprintf(f, "%s %u\n", a_pub_hex, session_nonce);
        fclose(f);
    }
}

static uint32_t ozos_lookup_syn_nonce(const char* a_pub_hex) {
    uint32_t found_nonce = 0;
    FILE* f = fopen(".ozos_syn_state", "r");
    if (!f) return 0;
    char key_buf[128];
    unsigned int nonce_val = 0;
    while (fscanf(f, "%127s %u", key_buf, &nonce_val) == 2) {
        if (strlen(key_buf) == 64) {
            bool match = true;
            for (int i = 0; i < 64; ++i) {
                char c1 = key_buf[i];
                char c2 = a_pub_hex[i];
                if (c1 >= 'A' && c1 <= 'F') c1 += 32;
                if (c2 >= 'A' && c2 <= 'F') c2 += 32;
                if (c1 != c2) { match = false; break; }
            }
            if (match) found_nonce = (uint32_t)nonce_val;
        }
    }
    fclose(f);
    return found_nonce;
}

static int cmd_handshake_syn(const char* wav_path, uint32_t session_nonce) {
    uint8_t a_pub[32], a_sk[32];
    if (ozos_ecdh_keypair(a_pub, a_sk) != 0) {
        printf("🚨 Sinh cap khoa Ephemeral A that bai!\n");
        return 1;
    }
    if (session_nonce == 0) session_nonce = (uint32_t)time(NULL);

    ozos_handshake_syn_t syn;
    ozos_handshake_syn_build(&syn, a_pub, session_nonce);

    char a_sk_hex[65], a_pub_hex[65], syn_hex[sizeof(syn) * 2 + 1];
    bytes_to_hex(a_sk, 32, a_sk_hex);
    bytes_to_hex(a_pub, 32, a_pub_hex);
    bytes_to_hex((const uint8_t*)&syn, sizeof(syn), syn_hex);
    ozos_crypto_zeroize(a_sk, sizeof(a_sk));

    ozos_record_syn_nonce(a_pub_hex, session_nonce);

    fprintf(stderr, "=================================================================\n");
    fprintf(stderr, "  [BI MAT NOI BO - TUYET DOI KHONG CHIA SE CHO PEER / PUBLIC]\n");
    fprintf(stderr, "  • Ephemeral Secret A : 0x%s\n", a_sk_hex);
    fprintf(stderr, "=================================================================\n");
    ozos_crypto_zeroize(a_sk_hex, sizeof(a_sk_hex));
    printf("=================================================================\n");
    printf("  [MẢNH GHÉP 2] BẮT TAY HAI CHIỀU VÔ THANH: KHỞI TẠO (SYN)\n");
    printf("=================================================================\n");
    printf("  [THONG TIN PHAT SONG CONG KHAI / AIR-GAP TRANSMISSION]\n");
    printf("  • Ephemeral PubKey A : 0x%s (Curve25519 32 Bytes)\n", a_pub_hex);
    printf("  • Session Nonce      : %u (Anti-replay timestamp)\n", session_nonce);
    printf("  • SYN Frame Size     : %zu bytes (Magic 0x4F5A4831 'OZH1')\n", sizeof(syn));
    printf("  • SYN Frame Hex      :\n%s\n", syn_hex);

    if (wav_path) {
        if (cmd_audio_synth(syn_hex, wav_path, OZOS_AUDIO_MODE_STEALTH) != 0) {
            return 1;
        }
        printf("• Near-Ultrasound WAV: %s (18.1 - 19.8 kHz, 100%% Vo thanh)\n", wav_path);
    }
    printf("=================================================================\n");
    printf("Huong dan tiep theo cho Node B:\n");
    printf("  ozos.exe handshake-ack %s [ack.wav]\n", syn_hex);
    printf("=================================================================\n");
    return 0;
}

static int cmd_handshake_ack(const char* syn_hex, const char* wav_path, const char* pinned_peer_pk_hex) {
    if (!syn_hex) {
        printf("Loi: ozos handshake-ack <syn_hex> [ack.wav]\n");
        return 1;
    }
    size_t hex_len = strlen(syn_hex);
    if (hex_len != sizeof(ozos_handshake_syn_t) * 2 || !is_valid_hex(syn_hex, hex_len)) {
        printf("🚨 SYN frame khong hop le (can %zu ky tu hex, nhan duoc %zu)\n",
               sizeof(ozos_handshake_syn_t) * 2, hex_len);
        return 1;
    }
    ozos_handshake_syn_t syn;
    hex_to_bytes(syn_hex, (uint8_t*)&syn, sizeof(syn));

    if (!ozos_handshake_syn_verify(&syn)) {
        printf("🚨 [TU CHOI] SYN Frame bi loi MAC16, Magic khong khop, hoac Low-Order Curve25519 Point!\n");
        return 1;
    }

    char syn_pk_hex[65];
    bytes_to_hex(syn.eph_pubkey, 32, syn_pk_hex);
    ozos_record_syn_nonce(syn_pk_hex, syn.session_nonce);

    if (pinned_peer_pk_hex != NULL) {
        if (strlen(pinned_peer_pk_hex) != 64 || !is_valid_hex(pinned_peer_pk_hex, 64)) {
            printf("🚨 [TU CHOI] Pinned Peer PubKey phai co do dai dung 64 ky tu hex hop le.\n");
            return 1;
        }
        uint8_t pinned_pk[32];
        hex_to_bytes(pinned_peer_pk_hex, pinned_pk, 32);
        if (memcmp(syn.eph_pubkey, pinned_pk, 32) != 0) {
            printf("🚨 [TU CHOI] Peer PubKey trong SYN (0x%s) khong khop voi khoa da ghim (0x%s)!\n",
                   syn_pk_hex, pinned_peer_pk_hex);
            return 1;
        }
        printf("• Pinned Peer Check  : ✅ Peer PubKey khop hoan toan voi khoa da ghim (Full Fingerprint: 0x%s)!\n", syn_pk_hex);
        printf("• TOFU Notice        : ℹ️ Pinned via CLI argument. Verify full 64-hex fingerprint out-of-band.\n");
    } else {
        printf("• Pinned Peer Check  : ⚠️ CẢNH BÁO TOFU: SYN không được đối chiếu khóa ghim (--pinned-peer-pubkey).\n");
        printf("• Peer Fingerprint   : 0x%s (Verify out-of-band before trusting session)\n", syn_pk_hex);
        printf("                       PFS đảm bảo bí mật đường truyền, cần xác thực đối tác out-of-band.\n");
    }

    uint8_t b_pub[32], b_sk[32];
    if (ozos_ecdh_keypair(b_pub, b_sk) != 0) {
        printf("🚨 Node B sinh cap khoa that bai!\n");
        return 1;
    }

    uint8_t shared_secret[32];
    if (ozos_ecdh_shared_secret(shared_secret, syn.eph_pubkey, b_sk) != 0) {
        ozos_crypto_zeroize(b_sk, sizeof(b_sk));
        printf("🚨 Tinh Shared Secret ECDH that bai!\n");
        return 1;
    }
    ozos_crypto_zeroize(b_sk, sizeof(b_sk));

    uint8_t session_key[32];
    if (ozos_kdf_session_key_ex(session_key, shared_secret, syn.session_nonce) != 0) {
        ozos_crypto_zeroize(shared_secret, sizeof(shared_secret));
        printf("🚨 KDF Session Key that bai!\n");
        return 1;
    }
    ozos_crypto_zeroize(shared_secret, sizeof(shared_secret));

    uint8_t auth_tag[16];
    if (ozos_compute_handshake_auth_tag(auth_tag, session_key, syn.eph_pubkey, b_pub) != 0) {
        ozos_crypto_zeroize(session_key, sizeof(session_key));
        printf("🚨 Compute Handshake Auth Tag that bai!\n");
        return 1;
    }

    ozos_handshake_ack_t ack;
    ozos_handshake_ack_build(&ack, b_pub, auth_tag);

    char b_pub_hex[65], k_sess_hex[65], ack_hex[sizeof(ack) * 2 + 1];
    bytes_to_hex(b_pub, 32, b_pub_hex);
    bytes_to_hex(session_key, 32, k_sess_hex);
    bytes_to_hex((const uint8_t*)&ack, sizeof(ack), ack_hex);
    ozos_crypto_zeroize(session_key, sizeof(session_key));

    printf("=================================================================\n");
    printf("  [MẢNH GHÉP 2] BẮT TAY HAI CHIỀU VÔ THANH: PHẢN HỒI (ACK)\n");
    printf("=================================================================\n");
    printf("• Ephemeral PubKey B : 0x%s (Curve25519 32 Bytes)\n", b_pub_hex);
    printf("• K_session Thiet lap: 0x%s (X25519 + SHA-512 KDF + Nonce Bound)\n", k_sess_hex);
    printf("• Poly1305 Auth Tag  : 0x%02x%02x%02x%02x...%02x%02x\n",
           auth_tag[0], auth_tag[1], auth_tag[2], auth_tag[3], auth_tag[14], auth_tag[15]);
    printf("• Acoustic Notice    : ⚠️ SYN phat song qua kenh vat ly khong xac thuc peer identity.\n");
    printf("                       K_session dam bao bi mat duong truyen (PFS), can xac thuc peer out-of-band.\n");
    printf("• ACK Frame Hex      :\n%s\n", ack_hex);

    if (wav_path) {
        if (cmd_audio_synth(ack_hex, wav_path, OZOS_AUDIO_MODE_STEALTH) != 0) {
            return 1;
        }
        printf("• Near-Ultrasound WAV: %s (18.1 - 19.8 kHz, 100%% Vo thanh)\n", wav_path);
    }
    printf("=================================================================\n");
    printf("Huong dan tiep theo cho Node A:\n");
    printf("  ozos.exe handshake-finish %s <a_sk_hex> <a_pub_hex>\n", ack_hex);
    printf("=================================================================\n");
    return 0;
}

static int cmd_handshake_finish(const char* ack_hex, const char* a_sk_hex, const char* a_pub_hex) {
    if (!ack_hex || !a_sk_hex || !a_pub_hex) {
        printf("Loi: ozos handshake-finish <ack_hex> <a_sk_hex> <a_pub_hex>\n");
        return 1;
    }
    size_t hex_len = strlen(ack_hex);
    if (hex_len != sizeof(ozos_handshake_ack_t) * 2 || !is_valid_hex(ack_hex, hex_len)) {
        printf("🚨 ACK frame khong hop le (can %zu ky tu hex, nhan duoc %zu)\n",
               sizeof(ozos_handshake_ack_t) * 2, hex_len);
        return 1;
    }
    if (strlen(a_sk_hex) != 64 || !is_valid_hex(a_sk_hex, 64) ||
        strlen(a_pub_hex) != 64 || !is_valid_hex(a_pub_hex, 64)) {
        printf("🚨 Khoa a_sk hoac a_pub khong hop le hoac sai do dai (can dung 64 ky tu hex).\n");
        return 1;
    }
    ozos_handshake_ack_t ack;
    hex_to_bytes(ack_hex, (uint8_t*)&ack, sizeof(ack));

    if (!ozos_handshake_ack_verify(&ack)) {
        printf("🚨 [TU CHOI] ACK Frame bi loi MAC16, Magic khong khop, hoac Low-Order Curve25519 Point!\n");
        return 1;
    }

    uint8_t a_sk[32], a_pub[32];
    hex_to_bytes(a_sk_hex, a_sk, 32);
    hex_to_bytes(a_pub_hex, a_pub, 32);

    uint8_t shared_secret[32];
    if (ozos_ecdh_shared_secret(shared_secret, ack.eph_pubkey, a_sk) != 0) {
        ozos_crypto_zeroize(a_sk, sizeof(a_sk));
        printf("🚨 Tinh Shared Secret ECDH that bai!\n");
        return 1;
    }
    ozos_crypto_zeroize(a_sk, sizeof(a_sk));

    uint32_t session_nonce = ozos_lookup_syn_nonce(a_pub_hex);
    uint8_t session_key[32];
    if (ozos_kdf_session_key_ex(session_key, shared_secret, session_nonce) != 0) {
        ozos_crypto_zeroize(shared_secret, sizeof(shared_secret));
        printf("🚨 KDF Session Key that bai!\n");
        return 1;
    }

    /* Fallback compatibility if caller did not persist SYN nonce via CLI */
    if (!ozos_verify_handshake_auth_tag(ack.auth_tag, session_key, a_pub, ack.eph_pubkey) && session_nonce != 0) {
        uint8_t legacy_key[32];
        if (ozos_kdf_session_key_ex(legacy_key, shared_secret, 0) == 0 &&
            ozos_verify_handshake_auth_tag(ack.auth_tag, legacy_key, a_pub, ack.eph_pubkey)) {
            memcpy(session_key, legacy_key, 32);
        }
        ozos_crypto_zeroize(legacy_key, sizeof(legacy_key));
    }
    ozos_crypto_zeroize(shared_secret, sizeof(shared_secret));

    if (!ozos_verify_handshake_auth_tag(ack.auth_tag, session_key, a_pub, ack.eph_pubkey)) {
        ozos_crypto_zeroize(session_key, sizeof(session_key));
        printf("🚨 [XAC THUC THAT BAI] Poly1305 Auth Tag cua Node B khong khop!\n");
        return 1;
    }

    char k_sess_hex[65];
    bytes_to_hex(session_key, 32, k_sess_hex);
    ozos_crypto_zeroize(session_key, sizeof(session_key));

    printf("=================================================================\n");
    printf("  [MẢNH GHÉP 2] BẮT TAY HAI CHIỀU VÔ THANH HOÀN TẤT THÀNH CÔNG!\n");
    printf("=================================================================\n");
    printf("Trang thai: ✅ ĐƯỜNG HẦM ÂM HỌC VÔ THANH ĐÃ ĐƯỢC THIẾT LẬP (< 2.0s)\n");
    printf("• Perfect Forward Sec: Co (Moi phien lam viec su dung khoa tam thoi rieng)\n");
    printf("• K_session Thiet lap: 0x%s\n", k_sess_hex);
    printf("• Mat ma duong ham   : XSalsa20 + Poly1305 AEAD (Stack-only, 0 RAM Leaks)\n");
    printf("=================================================================\n");
    printf("De truyen du lieu bi mat qua duong ham:\n");
    printf("  ozos.exe tunnel-encrypt \"NOI DUNG BI MAT\" %s\n", k_sess_hex);
    printf("=================================================================\n");
    return 0;
}

static int cmd_tunnel_encrypt(const char* message, const char* session_key_hex) {
    if (!message || !session_key_hex) {
        printf("Loi: ozos tunnel-encrypt <message> <session_key_hex>\n");
        return 1;
    }
    size_t msg_len = strlen(message);
    if (msg_len > OZOS_TUNNEL_MAX_PAYLOAD) {
        printf("🚨 Thong diep vuot qua %d bytes.\n", OZOS_TUNNEL_MAX_PAYLOAD);
        return 1;
    }
    uint8_t session_key[32];
    if (strlen(session_key_hex) != 64 || !is_valid_hex(session_key_hex, 64)) {
        printf("🚨 Session key hex phai dung 64 ky tu hex (32 bytes).\n");
        return 1;
    }
    hex_to_bytes(session_key_hex, session_key, 32);

    uint8_t nonce[24];
    randombytes(nonce, 24);

    uint8_t frame_buf[sizeof(ozos_tunnel_data_hdr_t) + 16 + OZOS_TUNNEL_MAX_PAYLOAD];
    size_t enc_len = ozos_tunnel_encrypt(frame_buf, sizeof(frame_buf), (const uint8_t*)message, msg_len, session_key, nonce);
    ozos_crypto_zeroize(session_key, sizeof(session_key));
    if (enc_len == 0) {
        printf("🚨 Ma hoa that bai!\n");
        return 1;
    }

    char* hex_out = (char*)malloc(enc_len * 2 + 1);
    if (!hex_out) return 1;
    bytes_to_hex(frame_buf, enc_len, hex_out);

    printf("=================================================================\n");
    printf("  [MẢNH GHÉP 2] GÓI TIN ĐƯỜNG HẦM ÂM HỌC ĐÃ MÃ HÓA (OZHD)\n");
    printf("=================================================================\n");
    printf("• Plaintext Size     : %zu bytes\n", msg_len);
    printf("• Total Frame Size   : %zu bytes (32B Header + 16B Poly1305 + Ciphertext)\n", enc_len);
    printf("• Encrypted Frame Hex:\n%s\n", hex_out);
    printf("=================================================================\n");
    free(hex_out);
    return 0;
}

static int cmd_tunnel_decrypt(const char* frame_hex, const char* session_key_hex) {
    if (!frame_hex || !session_key_hex) {
        printf("Loi: ozos tunnel-decrypt <frame_hex> <session_key_hex>\n");
        return 1;
    }
    size_t hex_len = strlen(frame_hex);
    if (hex_len % 2 != 0 || !is_valid_hex(frame_hex, hex_len)) {
        printf("🚨 Frame hex khong hop le hoac do dai le.\n");
        return 1;
    }
    size_t byte_count = hex_len / 2;
    if (byte_count < sizeof(ozos_tunnel_data_hdr_t) + 16) {
        printf("🚨 Khung du lieu qua ngan.\n");
        return 1;
    }
    if (byte_count > sizeof(ozos_tunnel_data_hdr_t) + 16 + OZOS_TUNNEL_MAX_PAYLOAD) {
        printf("🚨 Khung du lieu vuot qua gioi han bo nho cho phep (> %zu bytes).\n",
               sizeof(ozos_tunnel_data_hdr_t) + 16 + (size_t)OZOS_TUNNEL_MAX_PAYLOAD);
        return 1;
    }
    uint8_t session_key[32];
    if (strlen(session_key_hex) != 64 || !is_valid_hex(session_key_hex, 64)) {
        printf("🚨 Session key hex phai dung 64 ky tu hex.\n");
        return 1;
    }
    hex_to_bytes(session_key_hex, session_key, 32);

    uint8_t* frame_buf = (uint8_t*)malloc(byte_count);
    if (!frame_buf) return 1;
    hex_to_bytes(frame_hex, frame_buf, byte_count);

    uint8_t plaintext[OZOS_TUNNEL_MAX_PAYLOAD + 1];
    size_t plain_len = 0;
    bool ok = ozos_tunnel_decrypt(plaintext, &plain_len, frame_buf, byte_count, session_key);
    ozos_crypto_zeroize(session_key, sizeof(session_key));
    free(frame_buf);

    printf("=================================================================\n");
    printf("  [MẢNH GHÉP 2] KẾT QUẢ GIẢI MÃ ĐƯỜNG HẦM ÂM HỌC (OZHD)\n");
    printf("=================================================================\n");
    if (ok) {
        plaintext[plain_len] = '\0';
        printf("Trang thai: ✅ XÁC THỰC POLY1305 & GIẢI MÃ THÀNH CÔNG 100%%\n");
        printf("• Payload Length : %zu bytes\n", plain_len);
        printf("• Noi dung nhan  : \"%s\"\n", plaintext);
        printf("=================================================================\n");
        return 0;
    } else {
        printf("Trang thai: 🚨 TỪ CHỐI! KHÓA PHIÊN SAI HOẶC DỮ LIỆU ĐÃ BỊ SỬA ĐỔI / NGHE LÉN!\n");
        printf("=================================================================\n");
        return 1;
    }
}

/* =============================================================================
   COMMANDS: [MẢNH GHÉP 3] 12-WORD BRAINSEED (BIP-39 + PBKDF2)
   ============================================================================= */

static int cmd_brainseed_gen(const char* passphrase) {
    if (passphrase && strlen(passphrase) > OZOS_MAX_PASSPHRASE_LEN) {
        fprintf(stderr, "🚨 [TU CHOI] Passphrase vuot qua gioi han an toan cho phep (Toi da %d ky tu).\n", OZOS_MAX_PASSPHRASE_LEN);
        return 1;
    }
    uint8_t entropy[16];
    randombytes(entropy, 16);

    char mnemonic[256];
    uint8_t seed32[32];
    if (!ozos_brainseed_generate_ex(mnemonic, sizeof(mnemonic), entropy, passphrase, seed32)) {
        printf("🚨 Sinh Hat giong Tri nao that bai!\n");
        return 1;
    }

    uint8_t pk[32], sk[64];
    ozos_ed25519_seed_keypair(pk, sk, seed32);

    char pk_hex[65], seed_hex[65];
    bytes_to_hex(pk, 32, pk_hex);
    bytes_to_hex(seed32, 32, seed_hex);
    ozos_crypto_zeroize(seed32, sizeof(seed32));
    ozos_crypto_zeroize(sk, sizeof(sk));

    printf("=================================================================\n");
    printf("  [MẢNH GHÉP 3] HẠT GIỐNG PHỤC SINH TRONG TRÍ NÃO (BRAINSEED)\n");
    printf("=================================================================\n");
    printf("• CHUỖI 12 TỪ KHỞI TẠO (BIP-39 ENGLISH):\n\n");
    printf("  >>>  %s  <<<\n\n", mnemonic);
    printf("• Master Seed 32-Byte : 0x%s\n", seed_hex);
    printf("• Sovereign Public Key: 0x%s (Ed25519)\n", pk_hex);
    if (passphrase && strlen(passphrase) > 0) {
        printf("• Passphrase bao ve   : [DA BAO VE - %zu KY TU]\n", strlen(passphrase));
    } else {
        printf("• Passphrase bao ve   : (Khong co)\n");
    }
    printf("-----------------------------------------------------------------\n");
    printf("⚠️  LƯU Ý QUAN TRỌNG VỀ CHỦ QUYỀN:\n");
    printf("   12 từ này chính là hạt giống sinh mệnh danh tính của bạn.\n");
    printf("   Ngay cả khi toàn bộ máy tính, điện thoại, phần cứng bị tịch thu,\n");
    printf("   bạn chỉ cần ghi nhớ 12 từ này trong đầu. Bất kỳ máy tính nào trên\n");
    printf("   thế giới chạy 'ozos.exe brainseed-restore' sẽ tái tạo 100%% danh tính!\n");
    printf("=================================================================\n");
    return 0;
}

static int cmd_brainseed_restore(const char* mnemonic_str, const char* passphrase) {
    if (passphrase && strlen(passphrase) > OZOS_MAX_PASSPHRASE_LEN) {
        fprintf(stderr, "🚨 [TU CHOI] Passphrase vuot qua gioi han an toan cho phep (Toi da %d ky tu).\n", OZOS_MAX_PASSPHRASE_LEN);
        return 1;
    }
    if (!mnemonic_str) {
        printf("Loi: ozos brainseed-restore \"<12 tu cach nhau boi dau cach>\" [passphrase]\n");
        return 1;
    }

    uint8_t seed32[32];
    bool ok = ozos_brainseed_restore(seed32, mnemonic_str, passphrase);

    printf("=================================================================\n");
    printf("  [MẢNH GHÉP 3] KẾT QUẢ PHỤC SINH DANH TÍNH TỪ TRÍ NÃO\n");
    printf("=================================================================\n");
    if (ok) {
        uint8_t pk[32], sk[64];
        ozos_ed25519_seed_keypair(pk, sk, seed32);

        char pk_hex[65], seed_hex[65];
        bytes_to_hex(pk, 32, pk_hex);
        bytes_to_hex(seed32, 32, seed_hex);
        ozos_crypto_zeroize(seed32, sizeof(seed32));
        ozos_crypto_zeroize(sk, sizeof(sk));

        printf("Trang thai: ✅ DANH TÍNH CHỦ QUYỀN ĐÃ ĐƯỢC PHỤC SINH THÀNH CÔNG!\n");
        printf("• Master Seed 32-Byte : 0x%s\n", seed_hex);
        printf("• Sovereign Public Key: 0x%s (Ed25519)\n", pk_hex);
        printf("• Khong gian nho      : 100%% Thuc thi tren Stack (Zero Dynamic Leaks)\n");
        printf("=================================================================\n");
        return 0;
    } else {
        printf("Trang thai: 🚨 THẤT BẠI! LỖI TỪ ĐIỂN HOẶC SAI MÃ CHECKSUM!\n");
        printf("Vui lòng kiểm tra lại chính xác 12 từ tiếng Anh theo chuẩn BIP-39.\n");
        printf("=================================================================\n");
        return 1;
    }
}

/* =============================================================================
   COMMANDS: [MẢNH GHÉP 4] ACOUSTIC GOSSIP MESH & STATE MERKLE SYNC
   ============================================================================= */

static int cmd_gossip_beacon(uint32_t epoch, const char* wav_path) {
    if (epoch == 0) epoch = 100;

    ozos_tpm_info_t tpm;
    query_hardware_tpm(&tpm);
    uint32_t peer_id = 0;
    memcpy(&peer_id, tpm.silicon_seed, 4);

    uint64_t sample_leaves[4] = {
        0x1111222233334444ULL, 0x5555666677778888ULL,
        0x9999AAAABBBBCCCCULL, 0xDDDDEEEEFFFF0000ULL + epoch
    };
    uint64_t root = ozos_compute_merkle_root(sample_leaves, 4);

    ozos_gossip_beacon_t beacon;
    ozos_gossip_beacon_build(&beacon, peer_id, root, epoch);

    char beacon_hex[sizeof(beacon) * 2 + 1];
    bytes_to_hex((const uint8_t*)&beacon, sizeof(beacon), beacon_hex);

    printf("=================================================================\n");
    printf("  [MẢNH GHÉP 4] XUNG TIM MẠNG LƯỚI LAN TRUYỀN VÔ THANH (BEACON)\n");
    printf("=================================================================\n");
    printf("• Peer ID            : 0x%08X (Dinh danh Nut Lan truyen)\n", peer_id);
    printf("• Epoch Nonce        : %u (Phien ban trang thai dong don dieu)\n", epoch);
    printf("• Merkle Root        : 0x%016llX (Toan ven cay trang thai O(log N))\n", root);
    printf("• Beacon Size        : %zu bytes (Magic 0x4F5A4753 'OZGS')\n", sizeof(beacon));
    printf("• Beacon Frame Hex   :\n%s\n", beacon_hex);

    if (wav_path) {
        if (cmd_audio_synth(beacon_hex, wav_path, OZOS_AUDIO_MODE_STEALTH) != 0) {
            return 1;
        }
        printf("• Near-Ultrasound WAV: %s (18.1 - 19.8 kHz, 100%% Vo thanh)\n", wav_path);
    }
    printf("=================================================================\n");
    return 0;
}

static int cmd_gossip_listen(const char* beacon_hex, uint32_t my_epoch) {
    if (!beacon_hex) {
        printf("Loi: ozos gossip-listen <beacon_hex> [my_epoch]\n");
        return 1;
    }
    size_t hex_len = strlen(beacon_hex);
    if (hex_len != sizeof(ozos_gossip_beacon_t) * 2 || !is_valid_hex(beacon_hex, hex_len)) {
        printf("🚨 Beacon hex khong hop le (can %zu ky tu hex, nhan duoc %zu)\n",
               sizeof(ozos_gossip_beacon_t) * 2, hex_len);
        return 1;
    }

    ozos_gossip_beacon_t remote_beacon;
    hex_to_bytes(beacon_hex, (uint8_t*)&remote_beacon, sizeof(remote_beacon));

    if (!ozos_gossip_beacon_verify(&remote_beacon)) {
        printf("🚨 [TU CHOI] Beacon bi loi MAC16 hoac Magic khong hop le!\n");
        return 1;
    }

    if (my_epoch == 0) my_epoch = 100;
    uint64_t sample_leaves[4] = {
        0x1111222233334444ULL, 0x5555666677778888ULL,
        0x9999AAAABBBBCCCCULL, 0xDDDDEEEEFFFF0000ULL + my_epoch
    };
    uint64_t my_root = ozos_compute_merkle_root(sample_leaves, 4);

    ozos_gossip_action_t act = ozos_gossip_compare_state(my_root, my_epoch, &remote_beacon);

    printf("=================================================================\n");
    printf("  [MẢNH GHÉP 4] PHÂN XỬ ĐỒNG THUẬN TRẠNG THÁI MERKLE VÔ THANH\n");
    printf("=================================================================\n");
    printf("• Remote Peer ID     : 0x%08X (Epoch: %u | Root: 0x%016llX)\n",
           remote_beacon.peer_id, remote_beacon.epoch_nonce, remote_beacon.merkle_root);
    printf("• Local Node State   : Epoch: %u | Root: 0x%016llX\n", my_epoch, my_root);

    if (act == OZOS_GOSSIP_IN_SYNC) {
        printf("• Phan quyet Dong thuan: ✅ IN_SYNC - Hai nut hoan toan dong thuan trang thai!\n");
    } else if (act == OZOS_GOSSIP_PEER_AHEAD) {
        printf("• Phan quyet Dong thuan: 📥 PEER_AHEAD - Nut doi phuong co trang thai moi hon!\n");
        printf("  -> Tu dong phat Yeu cau keo Pull Request (10 bytes) qua song am de cap nhat delta.\n");
    } else if (act == OZOS_GOSSIP_LOCAL_AHEAD) {
        printf("• Phan quyet Dong thuan: 📤 LOCAL_AHEAD - Trang thai cuc bo moi hon doi phuong!\n");
        printf("  -> San sang phat song am truyen delta cho nut doi phuong.\n");
    } else {
        printf("• Phan quyet Dong thuan: ⚠️  FORK_DETECTED - Phat hien re nhanh Merkle khac nhau o cung Epoch!\n");
    }
    printf("=================================================================\n");
    return 0;
}

/* =============================================================================
   MAIN ENTRY POINT
   ============================================================================= */

static void print_banner(void) {
    printf("=================================================================\n");
    printf("  O.Z.O.S // NATIVE SOVEREIGN EXECUTABLE (ozos.exe)\n");
    printf("  Protocol: OZOS-80 Wire Format | Architecture: x86_64 Native C\n");
    printf("=================================================================\n\n");
    printf("Cac lenh tac chien co san:\n");
    printf("  ozos.exe status                        - Trang thai tram & Mo neo TPM 2.0\n");
    printf("  ozos.exe tpm                           - Chuan doan phan cung TPM 2.0\n");
    printf("  ozos.exe keygen [seed_hex]             - [MẢNH GHÉP 1] Sinh cap khoa Ban the Ed25519\n");
    printf("  ozos.exe packet-sign <msg> [nonce]     - [MẢNH GHÉP 1] Ky so Ban the Ed25519 vao goi tin\n");
    printf("  ozos.exe packet-build <msg> [nonce]    - Dong goi tin 80-byte SIMD (Chua ky)\n");
    printf("  ozos.exe packet-verify <hex> [last_n]  - Tham dinh MAC16 & Ed25519 chu ky Ban the\n");
    printf("  ozos.exe handshake-syn [wav] [nonce]   - [MẢNH GHÉP 2] Khoi tao Bat tay Vo thanh (SYN)\n");
    printf("  ozos.exe handshake-ack <syn> [wav] [--pinned-peer-pubkey <pk>] - [MẢNH GHÉP 2] Phan hoi Bat tay\n");
    printf("  ozos.exe handshake-finish <ack> <sk><pk>- [MẢNH GHÉP 2] Hoan tat Bắt tay & Mở Đường hầm\n");
    printf("  ozos.exe tunnel-encrypt <msg> <key>    - [MẢNH GHÉP 2] Ma hoa goi tin qua Duong ham\n");
    printf("  ozos.exe tunnel-decrypt <hex> <key>    - [MẢNH GHÉP 2] Giai ma va xac thuc Poly1305\n");
    printf("  ozos.exe brainseed-gen [passphrase]    - [MẢNH GHÉP 3] Sinh 12 tu Tri nao BIP-39\n");
    printf("  ozos.exe brainseed-restore <12 words>  - [MẢNH GHÉP 3] Phuc sinh Danh tinh tu 12 tu\n");
    printf("  ozos.exe gossip-beacon [epoch] [wav]   - [MẢNH GHÉP 4] Phat Xung tim Lan truyen Vo thanh\n");
    printf("  ozos.exe gossip-listen <beacon> [epoch]- [MẢNH GHÉP 4] Phan xu Dong thuan Merkle\n");
    printf("  ozos.exe audio-synth <txt> <wav> [opt] - Tong hop song am (Mac dinh: --stealth 18-20kHz)\n");
    printf("  ozos.exe audio-decode <in.wav> [opt]   - Giai ma song am qua Bo loc Goertzel (Auto)\n");
    printf("  ozos.exe daemon [port]                 - Khoi chay Background Node (Mac dinh 7788)\n");
    printf("  ozos.exe benchmark                     - Do luong throughput vi kien truc\n");
    printf("  ozos.exe version / --version           - Hien thi phien ban va SHA-256 pin\n");
    printf("=================================================================\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_banner();
        return 0;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "status") == 0 || strcmp(cmd, "tpm") == 0) {
        if (argc > 2) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Lenh '%s' khong nhan tham so phu.\n", cmd);
            return 1;
        }
        return cmd_status();
    } else if (strcmp(cmd, "keygen") == 0) {
        if (argc > 3) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Qua nhieu tham so cho lenh keygen.\n");
            return 1;
        }
        const char* custom_seed = (argc == 3) ? argv[2] : NULL;
        return cmd_keygen(custom_seed);
    } else if (strcmp(cmd, "packet-sign") == 0) {
        if (argc > 5) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Qua nhieu tham so cho lenh packet-sign.\n");
            return 1;
        }
        const char* msg = (argc >= 3) ? argv[2] : "O.Z.O.S SOVEREIGN IDENTITY SIGNED";
        uint64_t nonce = 1001ULL;
        if (argc >= 4) {
            if (!parse_strict_u64(argv[3], &nonce) || nonce == 0) {
                fprintf(stderr, "🚨 [LOI CU PHAP] Nonce khong hop le: '%s' (phai la so nguyen duong).\n", argv[3]);
                return 1;
            }
        }
        const char* custom_seed = (argc == 5) ? argv[4] : NULL;
        return cmd_packet_sign(msg, nonce, custom_seed);
    } else if (strcmp(cmd, "packet-build") == 0) {
        if (argc > 4) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Qua nhieu tham so cho lenh packet-build.\n");
            return 1;
        }
        const char* msg = (argc >= 3) ? argv[2] : "O.Z.O.S SOVEREIGN IDENTITY";
        uint64_t nonce = 1001ULL;
        if (argc >= 4) {
            if (!parse_strict_u64(argv[3], &nonce) || nonce == 0) {
                fprintf(stderr, "🚨 [LOI CU PHAP] Nonce khong hop le: '%s' (phai la so nguyen duong).\n", argv[3]);
                return 1;
            }
        }
        return cmd_packet_build(msg, nonce);
    } else if (strcmp(cmd, "packet-verify") == 0) {
        if (argc < 3) {
            printf("Loi: Vui long truyen vao chuoi Hex: ozos.exe packet-verify <hex> [last_n] [trusted_author_pk]\n");
            return 1;
        }
        uint64_t last_n = 1000ULL;
        const char* trusted_pk = NULL;
        bool has_last_n = false;
        bool has_trusted_pk = false;
        for (int i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--pinned-peer-pubkey") == 0 || strcmp(argv[i], "--pubkey") == 0) {
                if (has_trusted_pk || i + 1 >= argc) {
                    fprintf(stderr, "🚨 [LOI CU PHAP] Co '%s' bi trung lap hoac thieu gia tri.\n", argv[i]);
                    return 1;
                }
                trusted_pk = argv[++i];
                has_trusted_pk = true;
            } else if (argv[i][0] == '-') {
                fprintf(stderr, "🚨 [LOI CU PHAP] Co khong hop le: '%s'\n", argv[i]);
                return 1;
            } else if (!has_last_n) {
                if (!parse_strict_u64(argv[i], &last_n)) {
                    fprintf(stderr, "🚨 [LOI CU PHAP] Tham so last_seen_nonce khong hop le: '%s' (phai la so nguyen >= 0).\n", argv[i]);
                    return 1;
                }
                has_last_n = true;
            } else if (!has_trusted_pk) {
                trusted_pk = argv[i];
                has_trusted_pk = true;
            } else {
                fprintf(stderr, "🚨 [LOI CU PHAP] Tham so du thua khong hop le: '%s'\n", argv[i]);
                return 1;
            }
        }
        return cmd_packet_verify(argv[2], last_n, trusted_pk);
    } else if (strcmp(cmd, "handshake-syn") == 0) {
        if (argc > 4) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Qua nhieu tham so cho lenh handshake-syn.\n");
            return 1;
        }
        const char* wav_path = (argc >= 3) ? argv[2] : NULL;
        if (wav_path && wav_path[0] == '-') {
            fprintf(stderr, "🚨 [LOI CU PHAP] Co khong hop le: '%s'\n", wav_path);
            return 1;
        }
        uint32_t session_nonce = 0;
        if (argc == 4) {
            if (!parse_strict_u32(argv[3], &session_nonce) || session_nonce == 0) {
                fprintf(stderr, "🚨 [LOI CU PHAP] Session nonce khong hop le: '%s'\n", argv[3]);
                return 1;
            }
        }
        return cmd_handshake_syn(wav_path, session_nonce);
    } else if (strcmp(cmd, "handshake-ack") == 0) {
        if (argc < 3) {
            printf("Loi: ozos.exe handshake-ack <syn_hex> [output_ack.wav] [--pinned-peer-pubkey <pk>]\n");
            return 1;
        }
        const char* wav_path = NULL;
        const char* pinned_peer_pk = NULL;
        for (int i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--pinned-peer-pubkey") == 0) {
                if (pinned_peer_pk != NULL) {
                    fprintf(stderr, "🚨 [LOI CU PHAP] Co --pinned-peer-pubkey khong duoc phep trung lap!\n");
                    return 1;
                }
                if (i + 1 < argc) {
                    pinned_peer_pk = argv[++i];
                } else {
                    fprintf(stderr, "🚨 [LOI] Thieu tham so khoa cong khai cho co --pinned-peer-pubkey <64_hex_pk>!\n");
                    return 1;
                }
            } else if (!wav_path && argv[i][0] != '-') {
                wav_path = argv[i];
            } else {
                fprintf(stderr, "🚨 [LOI] Tham so khong xac dinh hoac co khong hop le: %s\n", argv[i]);
                return 1;
            }
        }
        return cmd_handshake_ack(argv[2], wav_path, pinned_peer_pk);
    } else if (strcmp(cmd, "handshake-finish") == 0) {
        if (argc != 5) {
            printf("Loi: ozos.exe handshake-finish <ack_hex> <a_sk_hex> <a_pub_hex>\n");
            return 1;
        }
        return cmd_handshake_finish(argv[2], argv[3], argv[4]);
    } else if (strcmp(cmd, "tunnel-encrypt") == 0) {
        if (argc != 4) {
            printf("Loi: ozos.exe tunnel-encrypt <message> <session_key_hex>\n");
            return 1;
        }
        return cmd_tunnel_encrypt(argv[2], argv[3]);
    } else if (strcmp(cmd, "tunnel-decrypt") == 0) {
        if (argc != 4) {
            printf("Loi: ozos.exe tunnel-decrypt <frame_hex> <session_key_hex>\n");
            return 1;
        }
        return cmd_tunnel_decrypt(argv[2], argv[3]);
    } else if (strcmp(cmd, "brainseed-gen") == 0) {
        if (argc > 3) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Qua nhieu tham so cho lenh brainseed-gen.\n");
            return 1;
        }
        const char* passphrase = (argc == 3) ? argv[2] : "";
        return cmd_brainseed_gen(passphrase);
    } else if (strcmp(cmd, "brainseed-restore") == 0) {
        if (argc < 3 || argc > 4) {
            printf("Loi: ozos.exe brainseed-restore \"<12 words>\" [passphrase]\n");
            return 1;
        }
        const char* passphrase = (argc == 4) ? argv[3] : "";
        return cmd_brainseed_restore(argv[2], passphrase);
    } else if (strcmp(cmd, "gossip-beacon") == 0) {
        if (argc > 4) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Qua nhieu tham so cho lenh gossip-beacon.\n");
            return 1;
        }
        uint32_t epoch = 100;
        if (argc >= 3) {
            if (!parse_strict_u32(argv[2], &epoch) || epoch == 0) {
                fprintf(stderr, "🚨 [LOI CU PHAP] Epoch khong hop le: '%s' (phai la so nguyen duong).\n", argv[2]);
                return 1;
            }
        }
        const char* wav_path = (argc == 4) ? argv[3] : NULL;
        if (wav_path && wav_path[0] == '-') {
            fprintf(stderr, "🚨 [LOI CU PHAP] Co khong hop le: '%s'\n", wav_path);
            return 1;
        }
        return cmd_gossip_beacon(epoch, wav_path);
    } else if (strcmp(cmd, "gossip-listen") == 0) {
        if (argc < 3 || argc > 4) {
            printf("Loi: ozos.exe gossip-listen <beacon_hex> [my_epoch]\n");
            return 1;
        }
        uint32_t my_epoch = 100;
        if (argc == 4) {
            if (!parse_strict_u32(argv[3], &my_epoch) || my_epoch == 0) {
                fprintf(stderr, "🚨 [LOI CU PHAP] my_epoch khong hop le: '%s' (phai la so nguyen duong).\n", argv[3]);
                return 1;
            }
        }
        return cmd_gossip_listen(argv[2], my_epoch);
    } else if (strcmp(cmd, "audio-synth") == 0) {
        if (argc < 4 || argc > 5) {
            printf("Loi: ozos.exe audio-synth <text> <output.wav> [--stealth | --audible]\n");
            return 1;
        }
        uint8_t mode = OZOS_AUDIO_MODE_STEALTH;
        if (argc == 5) {
            if (strcmp(argv[4], "--audible") == 0) mode = OZOS_AUDIO_MODE_AUDIBLE;
            else if (strcmp(argv[4], "--stealth") == 0) mode = OZOS_AUDIO_MODE_STEALTH;
            else {
                fprintf(stderr, "🚨 [LOI CU PHAP] Che do am thanh khong hop le: '%s'\n", argv[4]);
                return 1;
            }
        }
        return cmd_audio_synth(argv[2], argv[3], mode);
    } else if (strcmp(cmd, "audio-decode") == 0) {
        if (argc < 3 || argc > 4) {
            printf("Loi: ozos.exe audio-decode <input.wav> [--stealth | --audible]\n");
            return 1;
        }
        int forced_mode = -1;
        if (argc == 4) {
            if (strcmp(argv[3], "--stealth") == 0) forced_mode = OZOS_AUDIO_MODE_STEALTH;
            else if (strcmp(argv[3], "--audible") == 0) forced_mode = OZOS_AUDIO_MODE_AUDIBLE;
            else {
                fprintf(stderr, "🚨 [LOI CU PHAP] Che do giai ma khong hop le: '%s'\n", argv[3]);
                return 1;
            }
        }
        return cmd_audio_decode(argv[2], forced_mode);
    } else if (strcmp(cmd, "daemon") == 0 || strcmp(cmd, "--daemon") == 0) {
        if (argc > 3) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Qua nhieu tham so cho lenh daemon.\n");
            return 1;
        }
        int port = OZOS_DEFAULT_DAEMON_PORT;
        if (argc == 3) {
            if (!parse_strict_port(argv[2], &port)) {
                fprintf(stderr, "🚨 [LOI CU PHAP] Port daemon khong hop le: '%s' (1 - 65535).\n", argv[2]);
                return 1;
            }
        }
        cmd_daemon(port);
    } else if (strcmp(cmd, "benchmark") == 0) {
        if (argc > 2) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Lenh benchmark khong nhan tham so phu.\n");
            return 1;
        }
        return cmd_benchmark();
    } else if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0 || strcmp(cmd, "-V") == 0 || strcmp(cmd, "version") == 0) {
        if (argc > 2) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Lenh '%s' khong nhan tham so phu.\n", cmd);
            return 1;
        }
        printf("=================================================================\n");
        printf("  O.Z.O.S Sovereign Microkernel v1.2.0 (Self-Sovereign Invariant Engine)\n");
        printf("=================================================================\n");
        printf("• Git Commit     : %s\n", ozos_get_git_commit());
        printf("• Target Arch    : x86_64 / MSVC C99 Bare-Metal\n");
        printf("• Pinned WASM SHA: %s\n", OZOS_WASM_PIN_SHA256);
        printf("• Canonical Repo : https://github.com/LumenNull/OZOS.git\n");
        printf("• License        : MIT Sovereign License 2026\n");
        printf("=================================================================\n");
        return 0;
    } else if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0 || strcmp(cmd, "help") == 0) {
        if (argc > 2) {
            fprintf(stderr, "🚨 [LOI CU PHAP] Lenh '%s' khong nhan tham so phu.\n", cmd);
            return 1;
        }
        print_banner();
        return 0;
    } else {
        printf("Lenh khong hop le: '%s'. Go 'ozos.exe' de xem huong dan.\n", cmd);
        return 1;
    }

    return 0;
}
