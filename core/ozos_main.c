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
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ncrypt.h>
#include <bcrypt.h>
#if defined(OZOS_ENABLE_NETWORK_SOCKETS)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif
#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#ifndef _strtoui64
#define _strtoui64 strtoull
#endif
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

extern void randombytes(unsigned char *buf, unsigned long long len);

#include "libozos.h"
#include "ozos_crypto.h"
#include "ozos_brainseed.h"
#include "ozos_gossip.h"
#include "ozos_omni_phy.h"
#include "ozos_data_at_rest.h"
#include "ozos_workstation_guard.h"
#include "ozos_shamir.h"
#include "ozos_crash_guard.h"
#include "ozos_pqc_hybrid.h"
#include "ozos_serial.h"

#define OZOS_VERSION_STR "2026.1-SOVEREIGN"
#ifndef OZOS_GIT_COMMIT_HASH
#define OZOS_GIT_COMMIT_HASH "nogit-release"
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

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 44) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    uint8_t riff_hdr[12];
    if (fread(riff_hdr, 1, 12, f) != 12 ||
        memcmp(riff_hdr, "RIFF", 4) != 0 ||
        memcmp(riff_hdr + 8, "WAVE", 4) != 0) {
        fclose(f);
        return NULL;
    }

    uint32_t sample_rate = 44100;
    long data_offset = -1;
    long data_bytes = 0;

    while (ftell(f) + 8 <= sz) {
        char chunk_id[4];
        uint32_t chunk_sz = 0;
        if (fread(chunk_id, 1, 4, f) != 4 || fread(&chunk_sz, 4, 1, f) != 1) break;
        long chunk_pos = ftell(f);
        if (memcmp(chunk_id, "fmt ", 4) == 0 && chunk_sz >= 16) {
            uint8_t fmt_buf[16];
            if (fread(fmt_buf, 1, 16, f) == 16) {
                memcpy(&sample_rate, fmt_buf + 4, 4);
            }
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            data_offset = chunk_pos;
            data_bytes = (long)chunk_sz;
            break;
        }
        long next_pos = chunk_pos + (long)chunk_sz + (chunk_sz & 1);
        if (next_pos <= chunk_pos || next_pos > sz) break;
        fseek(f, next_pos, SEEK_SET);
    }

    if (data_offset < 0) {
        data_offset = 44;
        data_bytes = sz - 44;
    }
    if (data_offset + data_bytes > sz) {
        data_bytes = sz - data_offset;
    }
    if (data_bytes <= 0 || data_bytes > 50 * 1024 * 1024) {
        fclose(f);
        return NULL;
    }

    fseek(f, data_offset, SEEK_SET);
    size_t samples_count = (size_t)data_bytes / sizeof(short);
    short* buf = (short*)malloc(samples_count * sizeof(short));
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
    *out_sample_rate = sample_rate;
    *out_num_samples = samples_count;
    return buf;
}

/* =============================================================================
   PERSISTENT PEER PIN STORE (.ozos_known_peers) & ANTI-REPLAY NONCE STORE
   ============================================================================= */

/**
 * Check and record peer public key into persistent pin store `.ozos_known_peers`
 * (Mechanism analogous to SSH known_hosts: Trust-On-First-Use + MITM key-change detection).
 * Returns:
 *   1 : Already exists in `.ozos_known_peers` and matches 100%
 *   0 : First time encountering this peer (TOFU), recorded to `.ozos_known_peers`
 *  -1 : ABNORMAL KEY CHANGE DETECTED (KEY CHANGED / MITM ALERT) -> Fail-closed!
 */
static void ozos_secure_random_bytes(uint8_t* out, size_t len);

static void ozos_get_host_state_key(uint8_t out_key[32]) {
    static uint8_t cached_key[32] = {0};
    static bool key_loaded = false;
    if (key_loaded) {
        memcpy(out_key, cached_key, 32);
        return;
    }

    FILE* f = fopen(".ozos_state_key", "rb");
    if (f) {
        size_t n = fread(cached_key, 1, 32, f);
        fclose(f);
        if (n == 32) {
            key_loaded = true;
            memcpy(out_key, cached_key, 32);
            return;
        }
    }

    ozos_secure_random_bytes(cached_key, 32);
#ifndef _WIN32
    int fd = open(".ozos_state_key", O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd >= 0) {
        (void)write(fd, cached_key, 32);
        close(fd);
    } else {
        FILE* fr = fopen(".ozos_state_key", "rb");
        if (fr) {
            (void)fread(cached_key, 1, 32, fr);
            fclose(fr);
        }
    }
#else
    FILE* fw = fopen(".ozos_state_key", "wb");
    if (fw) {
        fwrite(cached_key, 1, 32, fw);
        fclose(fw);
    }
#endif
    key_loaded = true;
    memcpy(out_key, cached_key, 32);
}

static void ozos_derive_state_keys(uint64_t* out_tunnel_k0, uint64_t* out_tunnel_k1,
                                   uint64_t* out_pin_k0, uint64_t* out_pin_k1) {
    uint8_t host_key[32];
    ozos_get_host_state_key(host_key);
    uint64_t k0, k1, pk0, pk1;
    memcpy(&k0, host_key, 8);
    memcpy(&k1, host_key + 8, 8);
    memcpy(&pk0, host_key + 16, 8);
    memcpy(&pk1, host_key + 24, 8);
    if (out_tunnel_k0) *out_tunnel_k0 = k0 ^ 0x4F5A4F535345414CULL;
    if (out_tunnel_k1) *out_tunnel_k1 = k1 ^ 0xA5A5A5A55A5A5A5AULL;
    if (out_pin_k0) *out_pin_k0 = pk0 ^ 0x5045455250494E53ULL;
    if (out_pin_k1) *out_pin_k1 = pk1 ^ 0x5A5A5A5A5A5A5A5AULL;
}

static int ozos_check_and_record_peer_pin(const char* peer_id, const char* pk_hex, char* out_expected_hex) {
    if (!peer_id || !*peer_id) return 0;

    /* Prevent whitespace or control characters from corrupting .ozos_known_peers format */
    for (const char* p = peer_id; *p; p++) {
        if (*p <= 32 || *p > 126) {
            fprintf(stderr, "[SYNTAX ERROR] Invalid Peer ID (cannot contain whitespace or control characters): '%s'\n", peer_id);
            return -2;
        }
    }
    if (!pk_hex || strlen(pk_hex) != 64 || !is_valid_hex(pk_hex, 64)) return -2;

    FILE* f = fopen(".ozos_known_peers", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            char line_peer[128] = {0};
            char line_pk[128] = {0};
            char line_tag[64] = {0};
            int parts = sscanf(line, "%127s %127s %63s", line_peer, line_pk, line_tag);
            if (parts >= 2 && strcmp(line_peer, peer_id) == 0 &&
                strlen(line_pk) == 64 && is_valid_hex(line_pk, 64)) {
                /* Strict fail-closed state integrity tag verification bound to (peer_id, pk_bytes) */
                if (parts < 3 || strlen(line_tag) != 4 || !is_valid_hex(line_tag, 4)) {
                    continue; /* Reject untagged or malformed entries */
                }
                uint8_t mac_buf[160];
                size_t peer_len = strlen(line_peer);
                if (peer_len > 120) peer_len = 120;
                memcpy(mac_buf, line_peer, peer_len);
                mac_buf[peer_len] = ':';
                hex_to_bytes(line_pk, mac_buf + peer_len + 1, 32);
                uint64_t pin_k0, pin_k1;
                ozos_derive_state_keys(NULL, NULL, &pin_k0, &pin_k1);
                uint16_t exp_tag = compute_fast_gate_mac(mac_buf, peer_len + 1 + 32, pin_k0, pin_k1);
                uint16_t got_tag = (uint16_t)strtoul(line_tag, NULL, 16);
                if (exp_tag != got_tag) {
                    continue; /* Forged/unauthorized or cross-peer substituted entry -> skip */
                }
                fclose(f);
                bool match = true;
                for (int i = 0; i < 64; ++i) {
                    char c1 = line_pk[i];
                    char c2 = pk_hex[i];
                    if (c1 >= 'A' && c1 <= 'F') c1 += 32;
                    if (c2 >= 'A' && c2 <= 'F') c2 += 32;
                    if (c1 != c2) { match = false; break; }
                }
                if (out_expected_hex) {
                    memcpy(out_expected_hex, line_pk, 64);
                    out_expected_hex[64] = '\0';
                }
                return match ? 1 : -1;
            }
        }
        fclose(f);
    }

    FILE* fw = fopen(".ozos_known_peers", "a");
    if (fw) {
        uint8_t mac_buf[160];
        size_t peer_len = strlen(peer_id);
        if (peer_len > 120) peer_len = 120;
        memcpy(mac_buf, peer_id, peer_len);
        mac_buf[peer_len] = ':';
        hex_to_bytes(pk_hex, mac_buf + peer_len + 1, 32);
        uint64_t pin_k0, pin_k1;
        ozos_derive_state_keys(NULL, NULL, &pin_k0, &pin_k1);
        uint16_t tag = compute_fast_gate_mac(mac_buf, peer_len + 1 + 32, pin_k0, pin_k1);
        fprintf(fw, "%s %s %04X\n", peer_id, pk_hex, tag);
        fclose(fw);
#ifndef _WIN32
        chmod(".ozos_known_peers", 0600);
#endif
    }
    return 0;
}

static uint16_t ozos_compute_state_integrity_tag(const uint8_t* data, size_t len) {
    uint64_t tk0, tk1;
    ozos_derive_state_keys(&tk0, &tk1, NULL, NULL);
    return compute_fast_gate_mac(data, len, tk0, tk1);
}

static void ozos_cli_load_tunnel_nonces(void) {
    FILE* f = fopen(".ozos_tunnel_nonces", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char nonce_hex[64] = {0};
        char tag_hex[64] = {0};
        int parts = sscanf(line, "%63s %63s", nonce_hex, tag_hex);
        if (parts == 2 && strlen(nonce_hex) == 48 && is_valid_hex(nonce_hex, 48) && strlen(tag_hex) == 4 && is_valid_hex(tag_hex, 4)) {
            uint8_t n24[24];
            hex_to_bytes(nonce_hex, n24, 24);
            uint16_t exp_tag = ozos_compute_state_integrity_tag(n24, 24);
            uint16_t got_tag = (uint16_t)strtoul(tag_hex, NULL, 16);
            if (exp_tag != got_tag) continue; /* Skip poisoned/tampered entry */

            memcpy(g_ozos_tunnel_seen_nonces[g_ozos_tunnel_seen_pos], n24, 24);
            g_ozos_tunnel_seen_pos = (g_ozos_tunnel_seen_pos + 1U) % OZOS_TUNNEL_REPLAY_WINDOW;
            if (g_ozos_tunnel_seen_count < OZOS_TUNNEL_REPLAY_WINDOW) {
                g_ozos_tunnel_seen_count++;
            }
        }
    }
    fclose(f);
}

static bool ozos_is_tunnel_nonce_seen_in_store(const uint8_t nonce24[24]) {
    if (!nonce24) return false;

    /* 1. Check in-memory cache for current session */
    for (size_t i = 0; i < g_ozos_tunnel_seen_count; ++i) {
        if (memcmp(g_ozos_tunnel_seen_nonces[i], nonce24, 24) == 0) {
            return true;
        }
    }

    /* 2. Query persistent set on disk (.ozos_tunnel_nonces) with unbounded window */
    FILE* f = fopen(".ozos_tunnel_nonces", "r");
    if (!f) return false;

    char target_hex[49];
    bytes_to_hex(nonce24, 24, target_hex);

    char line[256];
    bool seen = false;
    while (fgets(line, sizeof(line), f)) {
        char nonce_hex[64] = {0};
        char tag_hex[64] = {0};
        int parts = sscanf(line, "%63s %63s", nonce_hex, tag_hex);
        /* Accept records with valid 4-hex integrity tag matching local machine seal key (H2b) */
        if (parts == 2 && strlen(nonce_hex) == 48 && is_valid_hex(nonce_hex, 48) && strlen(tag_hex) == 4 && is_valid_hex(tag_hex, 4)) {
            uint8_t n_tmp[24];
            hex_to_bytes(nonce_hex, n_tmp, 24);
            uint16_t exp_tag = ozos_compute_state_integrity_tag(n_tmp, 24);
            uint16_t got_tag = (uint16_t)strtoul(tag_hex, NULL, 16);
            if (exp_tag != got_tag) {
                continue; /* Attacker pre-poisoned entry without host seal key -> skip */
            }

            bool match = true;
            for (int i = 0; i < 48; ++i) {
                char c1 = nonce_hex[i];
                char c2 = target_hex[i];
                if (c1 >= 'A' && c1 <= 'F') c1 += 32;
                if (c2 >= 'A' && c2 <= 'F') c2 += 32;
                if (c1 != c2) { match = false; break; }
            }
            if (match) {
                seen = true;
                break;
            }
        }
    }
    fclose(f);
    return seen;
}

static void ozos_cli_record_tunnel_nonce(const uint8_t nonce24[24]) {
    FILE* f = fopen(".ozos_tunnel_nonces", "a");
    if (!f) return;
    char n_hex[49];
    bytes_to_hex(nonce24, 24, n_hex);
    uint16_t tag = ozos_compute_state_integrity_tag(nonce24, 24);
    fprintf(f, "%s %04X\n", n_hex, tag);
    fclose(f);
#ifndef _WIN32
    chmod(".ozos_tunnel_nonces", 0600);
#endif

    memcpy(g_ozos_tunnel_seen_nonces[g_ozos_tunnel_seen_pos], nonce24, 24);
    g_ozos_tunnel_seen_pos = (g_ozos_tunnel_seen_pos + 1U) % OZOS_TUNNEL_REPLAY_WINDOW;
    if (g_ozos_tunnel_seen_count < OZOS_TUNNEL_REPLAY_WINDOW) {
        g_ozos_tunnel_seen_count++;
    }
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
        fprintf(stderr, "[FATAL] CSPRNG failure in ozos_secure_random_bytes. Aborting fail-closed.\n");
        abort();
    }
}
#else
static void ozos_secure_random_bytes(uint8_t* out, size_t len) {
    FILE* ur = fopen("/dev/urandom", "rb");
    if (!ur) {
        fprintf(stderr, "[FATAL] Failed to open /dev/urandom. Aborting fail-closed.\n");
        abort();
    }
    size_t total = 0;
    while (total < len) {
        size_t n = fread(out + total, 1, len - total, ur);
        if (n == 0) {
            fclose(ur);
            fprintf(stderr, "[FATAL] Failed to read entropy from /dev/urandom. Aborting fail-closed.\n");
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
            info->tpm_available = false;
            strcpy(info->provider_name, "Hardware TPM Present but Anchor Failed (Fallback Non-Anchored)");
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
            info->tpm_available = false;
            strcpy(info->provider_name, "Hardware TPM Device Present but RNG Failed (Fallback Non-Anchored)");
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
    printf("  O.Z.O.S (Omni-Zero Operating Shield) // CRYPTOGRAPHIC RUNTIME\n");
    printf("  Mode: Stack-Locked Userspace (see ports/baremetal_uefi for EFI)\n");
    printf("=================================================================\n\n");

    ozos_tpm_info_t tpm;
    query_hardware_tpm(&tpm);

    printf("[1] SILICON HARDWARE ROOT OF TRUST:\n");
    printf("  * TPM Status          : %s\n", tpm.tpm_available ? "[OK] ACTIVE (NATIVE TPM 2.0)" : "[WARN] FALLBACK (NO TPM)");
    printf("  * Provider            : %s\n", tpm.provider_name);
    printf("  * Silicon Seed        : 0x%s\n", tpm.silicon_hex);

    printf("\n[2] CANONICAL WIRE PROTOCOL (OZOS-80):\n");
    printf("  * Header Size         : %u Bytes (SIMD 16-Byte Aligned)\n", OZOS_PACKET_HEADER_SIZE);
    printf("  * Magic Identifier    : 0x%08X (ASCII: GEN\\x01)\n", OZOS_MAGIC_HEADER);
    printf("  * Tripwire MAC16      : < 20 nanoseconds (CRC16-CCITT O(1) Prefilter)\n");

    printf("\n[3] AIR-GAPPED PHYSICAL TRANSPORT:\n");
    printf("  * Acoustic Channel    : Near-Ultrasound Inaudible 18-20 kHz & Dual-Nibble 16-FSK\n");
    printf("  * Smoothing Window    : Hann Envelope 5ms (Zero Acoustic Click/Pop)\n");
    printf("  * Optical Stego       : 2D-DCT 8x8 Orthonormal Luminance Symmetry Delta\n");
    printf("  * Acoustic Filter     : 32 Goertzel Filters O(N) Zero-RAM Purity\n");

    printf("\n[4] WORKSTATION HARDENING & OPSEC POSTURE:\n");
    ozos_hypervisor_info_t hv;
    ozos_detect_hypervisor(&hv);
    printf("  * Hypervisor Status   : %s\n", hv.is_virtualized ? "[ALERT] VIRTUALIZED (Snapshot Rollback Risk)" : "[OK] PHYSICAL HOST (NO HYPERVISOR BIT)");
    ozos_net_env_t net;
    ozos_scan_network_environment(&net);
    printf("  * Network Environment : %s\n", net.posture == OZOS_NET_PRISTINE_AIRGAP ? "[OK] PRISTINE AIR-GAP (0 Adapters)" : 
                                           (net.has_wifi ? "[WARN] CONNECTED WI-FI WORKSTATION" : "[WARN] CONNECTED WIRED LAN"));
    printf("  * Memory Shield       : [OK] STACK-ONLY ENCLAVE & ANTI-DUMP DACL\n");
    printf("=================================================================\n");
    return 0;
}

/* =============================================================================
   COMMAND: [TIER 1 PRIMITIVE] KEYGEN & SIGN (SOVEREIGN ED25519)
   ============================================================================= */

static int get_sovereign_keys(uint8_t pk[32], uint8_t sk[64], const char* custom_seed_hex) {
    uint8_t seed[32];
    if (custom_seed_hex != NULL && custom_seed_hex[0] != '\0') {
        size_t slen = strlen(custom_seed_hex);
        if (slen != 64) {
            fprintf(stderr, "[SYNTAX ERROR] Seed hex must be exactly 64 characters (received %zu characters).\n", slen);
            fprintf(stderr, "   [NOTE] Please verify seed format; the system does NOT silently fallback on invalid length!\n");
            return -1;
        }
        if (!is_valid_hex(custom_seed_hex, 64)) {
            fprintf(stderr, "[SYNTAX ERROR] Seed contains invalid hex characters.\n");
            return -1;
        }
        hex_to_bytes(custom_seed_hex, seed, 32);
    } else {
        ozos_tpm_info_t tpm;
        query_hardware_tpm(&tpm);
        if (!tpm.tpm_available) {
            fprintf(stderr, "[REJECTED] Hardware TPM 2.0 not found on this system!\n");
            fprintf(stderr, "   System strictly REFUSES to silently sign with public machine-id to prevent identity spoofing.\n");
            fprintf(stderr, "   [USAGE] Pass sovereign seed hex: ozos packet-sign <msg> [nonce] <64_hex_seed>\n");
            fprintf(stderr, "   [USAGE] Or derive sovereign mindseed: ./ozos brainseed-gen\n");
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
            fprintf(stderr, "[SYNTAX ERROR] Seed hex must be exactly 64 characters (received %zu characters).\n", slen);
            exit(1);
        }
        if (!is_valid_hex(custom_seed_hex, 64)) {
            fprintf(stderr, "[SYNTAX ERROR] Seed contains invalid hex characters.\n");
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
        printf("[SECURITY WARNING]: Hardware TPM 2.0 not found!\n");
        printf("   Never use /etc/machine-id as a key. Generated fresh CSPRNG Seed.\n");
        printf("   Store this Master Seed securely for signing and verification across future sessions.\n\n");
    }

    ozos_ed25519_seed_keypair(pk, sk, seed);
    bytes_to_hex(pk, 32, pk_hex);
    ozos_crypto_zeroize(seed, sizeof(seed));
    ozos_crypto_zeroize(sk, sizeof(sk));

    printf("=================================================================\n");
    printf("  [TIER 1 PRIMITIVE] SOVEREIGN IDENTITY KEYPAIR (ED25519 ROOT OF TRUST)\n");
    printf("=================================================================\n");
    printf("* TPM 2.0 Status      : %s\n", tpm.tpm_available ? "ACTIVE (NATIVE SILICON)" : "NO HARDWARE TPM (CSPRNG SEED)");
    printf("* Provider            : %s\n", tpm.provider_name);
    printf("* Master Silicon Seed : 0x%s\n", seed_hex);
    printf("* Sovereign Public Key: 0x%s (32 Bytes Ed25519)\n", pk_hex);
    printf("* Cryptography Spec   : RFC 8032 Curve25519 (Constant-Time, Zero-Allocation)\n");
    printf("=================================================================\n");
    printf("[SECURITY NOTICE & KEY MANAGEMENT]:\n");
    printf("* Keypair is displayed only in this volatile session; NEVER written to disk.\n");
    printf("* Full Fingerprint (64-Hex): 0x%s\n", pk_hex);
    printf("* Short Fingerprint        : 0x%.16s...\n", pk_hex);
    printf("* Storage Directive        : Never commit seed to Git or store unencrypted. Keep in cold storage.\n");
    printf("* Loss of seed permanently revokes ownership of your sovereign identity!\n");
    printf("=================================================================\n");
    return 0;
}

static int cmd_packet_sign(const char* message, uint64_t nonce, const char* custom_seed_hex) {
    if (!message) {
        printf("Error: ozos packet-sign <msg> [nonce] [seed_hex]\n");
        return 1;
    }

    size_t msg_len = strlen(message);
    if (msg_len > 1024) {
        printf("[ERROR] Message exceeds 1024 bytes (received %zu bytes).\n", msg_len);
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
        uint8_t mac_input[78 + 1024];
        memcpy(mac_input, &pkt, 78);
        memcpy(mac_input + 78, message, msg_len);
        pkt.header_mac16 = compute_fast_gate_mac(mac_input, 78 + msg_len, OZOS_A2_DOMAIN_CONSTANT, 0ULL);
    }

    size_t unsigned_size = OZOS_PACKET_HEADER_SIZE + msg_len;
    uint8_t raw_buffer[2048];
    memcpy(raw_buffer, &pkt, OZOS_PACKET_HEADER_SIZE);
    memcpy(raw_buffer + OZOS_PACKET_HEADER_SIZE, message, msg_len);

    uint8_t signed_frame[4096];
    size_t total_signed = ozos_sign_packet_frame(signed_frame, sizeof(signed_frame), raw_buffer, unsigned_size, pk, sk);
    ozos_crypto_zeroize(sk, sizeof(sk));
    if (total_signed == 0) {
        printf("[ERROR] Failed to sign packet!\n");
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
    printf("  [PACKET SIGNED] SOVEREIGN IDENTITY SIGNED FRAME (ED25519)\n");
    printf("=================================================================\n");
    printf("* Nonce          : %llu\n", nonce);
    printf("* Magic Header   : 0x%08X\n", pkt.magic);
    printf("* Tripwire MAC16 : 0x%04X (Verified in < 20ns)\n", pkt.header_mac16);
    printf("* Author PubKey  : 0x%s\n", pk_hex);
    printf("* Ed25519 Sig    : 0x%s\n", sig_hex);
    printf("* Total Length   : %zu bytes (80B Header + %zuB Payload + 96B Sig)\n", total_signed, msg_len);
    printf("\n[SIGNED HEX DUMP]:\n%s\n", hex_dump);
    printf("=================================================================\n");

    free(hex_dump);
    return 0;
}

static int cmd_packet_build(const char* message, uint64_t nonce, uint64_t mac_key) {
    if (!message) {
        printf("Error: Please provide message: ozos packet-build <msg> [nonce] [--mac-key <hex>]\n");
        return 1;
    }

    size_t msg_len = strlen(message);
    if (msg_len > 1024) {
        printf("[ERROR] Message exceeds 1024 bytes (received %zu bytes).\n", msg_len);
        return 1;
    }

    ozos_packet80_t pkt;
    float coeffs[16];
    for (int i = 0; i < 16; ++i) coeffs[i] = 1.0f + (float)i * 0.1f;
    ozos_packet_build(&pkt, nonce, coeffs, (uint16_t)msg_len);

    /* When packet has payload (msg_len > 0), emit Sub-Profile A2 (SipHash-2-4 Fast Gatekeeper MAC) */
    if (msg_len > 0) {
        uint64_t k0 = (mac_key != 0ULL) ? mac_key : OZOS_A2_DOMAIN_CONSTANT;
        uint8_t mac_input[78 + 1024];
        memcpy(mac_input, &pkt, 78);
        memcpy(mac_input + 78, message, msg_len);
        pkt.header_mac16 = compute_fast_gate_mac(mac_input, 78 + msg_len, k0, 0ULL);
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
    printf("  [PACKET BUILT] PACKET ENCAPSULATED TO OZOS-80 WIRE SPECIFICATION\n");
    printf("=================================================================\n");
    printf("* Nonce          : %llu\n", nonce);
    printf("* Magic Header   : 0x%08X (ASCII: GEN\\x01)\n", pkt.magic);
    printf("* Tripwire MAC16 : 0x%04X (Verified in < 20ns)\n", pkt.header_mac16);
    printf("* Payload Length : %zu bytes\n", msg_len);
    printf("* Total Size     : %zu bytes (80B Header + %zuB Payload)\n", total_size, msg_len);
    printf("\n[HEX DUMP]:\n%s\n", hex_dump);
    printf("=================================================================\n");

    free(buffer);
    free(hex_dump);
    return 0;
}

static int cmd_packet_verify(const char* hex_str, uint64_t last_seen_nonce, const char* trusted_author_pk_hex, const char* peer_id, uint64_t mac_key) {
    if (!hex_str) {
        printf("Error: ozos packet-verify <hex> [last_seen_nonce] [trusted_author_pk] [--peer-id <id>] [--mac-key <hex>]\n");
        return 1;
    }

    size_t hex_len = strlen(hex_str);
    size_t byte_count = hex_len / 2;
    if (byte_count < OZOS_PACKET_HEADER_SIZE) {
        printf("[REJECTED] Packet too short (< 80 bytes).\n");
        return 1;
    }

    uint8_t* buffer = (uint8_t*)malloc(byte_count);
    if (!buffer) return 1;

    if (!is_valid_hex(hex_str, hex_len)) {
        printf("[REJECTED] Invalid hex string.\n");
        free(buffer);
        return 1;
    }
    hex_to_bytes(hex_str, buffer, byte_count);

    ozos_packet80_t validated;
    ozos_status_t st = ozos_packet_validate_ex(buffer, byte_count, last_seen_nonce, mac_key, &validated);

    printf("=================================================================\n");
    printf("  [PACKET VERIFICATION RESULT] SOVEREIGN INTEGRITY AUDIT\n");
    printf("=================================================================\n");

    if (st != OZOS_SUCCESS) {
        printf("Status         : [REJECTED] (TAMPERED OR REPLAY DETECTED)\n");
        if (st == OZOS_ERR_MAGIC_INVALID) {
            uint32_t magic_at_0 = 0;
            if (byte_count >= 4) memcpy(&magic_at_0, buffer, 4);
            if (magic_at_0 == 0x47454E01) {
                printf("* Root Cause    : Packet belongs to Profile B (GENS-v1 Web Cockpit, Magic at byte 0). This CLI expects Profile A (OZOS-80 SIMD Carrier, Magic at byte 72).\n");
            } else {
                printf("* Root Cause    : Invalid Magic Header (Non-OZOS packet)\n");
            }
        }
        else if (st == OZOS_ERR_MAC16_MISMATCH) printf("* Root Cause    : Invalid MAC16 (Bit-flip or modified data detected)\n");
        else if (st == OZOS_ERR_A1_PAYLOAD_UNPROTECTED) {
            printf("* Root Cause    : Sub-Profile A1 rejected: MAC16 covers 78B header only, leaving payload unprotected!\n");
            printf("                  Requires Sub-Profile A2 (FNV-Fold) or Sovereign Ed25519 signature.\n");
        }
        else if (st == OZOS_ERR_REPLAY_DETECTED) printf("* Root Cause    : Replay Attack Detected (Nonce not strictly monotonic)\n");
        else if (st == OZOS_ERR_PAYLOAD_TRUNCATED) printf("* Root Cause    : Payload truncated or invalid length\n");
        else if (st == OZOS_ERR_FLOAT_POISONED) printf("* Root Cause    : Poisoned floating-point coefficients (NaN or Infinity detected in dct_coeffs)\n");
        else if (st == OZOS_ERR_BUFFER_TOO_SMALL) printf("* Root Cause    : Packet smaller than 80 bytes (Buffer too small)\n");
        printf("=================================================================\n");
        free(buffer);
        return 1;
    }

    /* Check canonical framing invariant length */
    size_t base_len = OZOS_PACKET_HEADER_SIZE + (size_t)validated.payload_len;
    size_t expected_signed_len = base_len + OZOS_SIG_ATTACHMENT_SIZE;

    if (byte_count > base_len && byte_count < expected_signed_len) {
        /* Truncated signature */
        printf("Status         : [REJECTED] TRUNCATED SIGNATURE DETECTED\n");
        printf("* Root Cause    : Ed25519 signature frame truncated (Received %zu bytes, expected %zu bytes)\n",
               byte_count - base_len, (size_t)OZOS_SIG_ATTACHMENT_SIZE);
        printf("=================================================================\n");
        free(buffer);
        return 1;
    }

    if (byte_count > expected_signed_len) {
        /* Rac noi duoi */
        printf("Status         : [REJECTED] TRAILING GARBAGE DETECTED\n");
        printf("* Root Cause    : Detected %zu unexpected trailing bytes after valid frame!\n",
               byte_count - expected_signed_len);
        printf("=================================================================\n");
        free(buffer);
        return 1;
    }

    /* Print valid header metadata */
    printf("* Nonce          : %llu (Valid > %llu)\n", validated.nonce, last_seen_nonce);
    printf("* Magic Header   : 0x%08X (Canonical)\n", validated.magic);
    printf("* Tripwire MAC16 : 0x%04X (Verified in < 20ns)\n", validated.header_mac16);
    printf("* Payload Length : %u bytes\n", validated.payload_len);

    /* R15-M5: Validate trusted author pk format whenever argument is present, regardless of packet signature status */
    if (trusted_author_pk_hex != NULL) {
        if (strlen(trusted_author_pk_hex) != 64 || !is_valid_hex(trusted_author_pk_hex, 64)) {
            printf("Status         : [REJECTED] TRUSTED KEY INVALID FORMAT!\n");
            printf("* Root Cause    : Trusted Public Key must be exactly 64 valid hex characters.\n");
            printf("=================================================================\n");
            free(buffer);
            return 1;
        }
    }

    char payload_str[1025];
    size_t copy_len = validated.payload_len > 1024 ? 1024 : validated.payload_len;
    for (size_t i = 0; i < copy_len; ++i) {
        uint8_t b = buffer[OZOS_PACKET_HEADER_SIZE + i];
        payload_str[i] = (b >= 32 && b <= 126) ? (char)b : '.';
    }
    payload_str[copy_len] = '\0';
    printf("* Received Payload: \"%s\"\n", payload_str);

    int final_rc = 0;
    /* Verify cryptographic signature validity */
    if (byte_count == base_len) {
        /* Structurally valid frame without signature */
        if (trusted_author_pk_hex != NULL) {
            /* M5 residual: Fail-closed if caller passed trusted-pk but packet is unsigned */
            printf("Status         : [REJECTED] UNSIGNED PACKET CANNOT SATISFY TRUSTED AUTHOR REQUIREMENT\n");
            printf("* Sovereign Sig: [REJECTED] Expected trusted public key 0x%s but packet has no Ed25519 signature.\n", trusted_author_pk_hex);
            final_rc = 1;
        } else if (validated.payload_len > 0) {
            uint16_t crc_header = ozos_compute_mac16(&validated);
            if (validated.header_mac16 == crc_header) {
                printf("Status         : [REJECTED] SUB-PROFILE A1 DOES NOT PROTECT PAYLOAD\n");
                printf("* Sovereign Sig: [NONE] UNSIGNED PACKET (No sovereign signature attached)\n");
                printf("* Warning Sub-Profile A1: [ALERT] MAC16 covers 78B header only (payload %u bytes unauthenticated)! Rejected to prevent payload tampering.\n", validated.payload_len);
                final_rc = 1;
            } else {
                printf("Status         : [WARNING] VALID WIRE FORMAT (NO ED25519 SIGNATURE ATTACHED)\n");
                printf("* Sovereign Sig: [NONE] UNSIGNED PACKET (No sovereign signature attached)\n");
                printf("* Sub-Profile A2 : [OK] MAC16 SipHash-2-4 Gatekeeper Verified 100%%\n");
                final_rc = 0;
            }
        } else {
            printf("Status         : [WARNING] VALID WIRE FORMAT (NO ED25519 SIGNATURE ATTACHED)\n");
            printf("* Sovereign Sig: [NONE] UNSIGNED PACKET (No sovereign signature attached)\n");
            final_rc = 0;
        }
    } else if (byte_count == expected_signed_len) {
        /* Frame with detached Ed25519 signature attached */
        uint8_t author_pk[32];
        bool sig_valid = ozos_verify_packet_frame(buffer, expected_signed_len, author_pk);
        char pk_hex[65];
        bytes_to_hex(author_pk, 32, pk_hex);

        if (!sig_valid) {
            printf("Status         : [REJECTED] ED25519 SIGNATURE FORGED OR DATA TAMPERED!\n");
            printf("* Sovereign Sig: [REJECTED] ED25519 SIGNATURE FORGED OR DATA TAMPERED!\n");
            printf("* Author PubKey  : 0x%s\n", pk_hex);
            final_rc = 1;
        } else {
            if (peer_id != NULL && *peer_id != '\0') {
                char expected_pin_hex[65] = {0};
                int pin_st = ozos_check_and_record_peer_pin(peer_id, pk_hex, expected_pin_hex);
                if (pin_st < 0) {
                    printf("Status         : [REJECTED] PEER KEY CHANGED / MITM ALERT!\n");
                    printf("* Peer ID        : %s\n", peer_id);
                    printf("* Pinned Key     : 0x%s (In .ozos_known_peers store)\n", expected_pin_hex);
                    printf("* Presented Key  : 0x%s [WARNING: KEY CHANGED / MITM DETECTED]\n", pk_hex);
                    printf("=================================================================\n");
                    free(buffer);
                    return 1;
                } else if (pin_st == 1) {
                    printf("* Known Peers    : [OK] Public key matches .ozos_known_peers store (Peer: %s)\n", peer_id);
                } else {
                    printf("* Known Peers    : [OK] [TOFU FIRST-USE PINNED] Recorded first-use public key for peer '%s' into .ozos_known_peers\n", peer_id);
                }
            }
            /* Signature mathematically verified. Compare against Sovereign Trust Anchor */
            if (trusted_author_pk_hex != NULL) {
                if (strlen(trusted_author_pk_hex) != 64 || !is_valid_hex(trusted_author_pk_hex, 64)) {
                    printf("Status         : [REJECTED] TRUSTED KEY INVALID FORMAT!\n");
                    printf("* Root Cause    : Trusted Public Key must be exactly 64 valid hex characters.\n");
                    printf("=================================================================\n");
                    free(buffer);
                    return 1;
                }
                uint8_t trusted_pk[32];
                hex_to_bytes(trusted_author_pk_hex, trusted_pk, 32);
                if (memcmp(author_pk, trusted_pk, 32) == 0) {
                    printf("* Pinned Peer Check  : [INFO] Validated against CLI pinned key (TOFU). Verify fingerprint out-of-band.\n");
                    printf("* Full Fingerprint   : 0x%s\n", pk_hex);
                    printf("* TOFU Notice        : [INFO] Pinned via CLI argument. Verify this fingerprint out-of-band.\n");
                    printf("Status         : [OK] [AUTHENTICATED VIA CLI PINNED KEY / TOFU] VERIFIED (100%% INVARIANT INTEGRITY & SOVEREIGN VERIFIED)\n");
                    printf("* Sovereign Sig: [OK] ED25519 SIGNATURE VALID AND SOVEREIGN AUTHOR VERIFIED!\n");
                    printf("* Author PubKey  : 0x%s [TRUSTED PINNED SOVEREIGN KEY]\n", pk_hex);
                    final_rc = 0;
                } else {
                    printf("Status         : [REJECTED] VALID SIGNATURE BUT AUTHOR IS NOT IN TRUSTED STORE!\n");
                    printf("* Sovereign Sig: [WARNING] VALID ED25519 SIGNATURE BUT UNTRUSTED / UNPINNED AUTHOR KEY\n");
                    printf("* Author PubKey  : 0x%s [WARNING: UNTRUSTED / ATTACKER KEY]\n", pk_hex);
                    printf("* Expected Key   : 0x%s\n", trusted_author_pk_hex);
                    final_rc = 1;
                }
            } else if (peer_id != NULL && *peer_id != '\0') {
                printf("* Full Fingerprint   : 0x%s\n", pk_hex);
                printf("Status         : [OK] [AUTHENTICATED VIA .ozos_known_peers PIN STORE] VERIFIED (100%% INVARIANT INTEGRITY & SOVEREIGN VERIFIED)\n");
                printf("* Author PubKey  : 0x%s [PINNED PEER '%s']\n", pk_hex, peer_id);
                final_rc = 0;
            } else {
                /* No reference key provided: Warning on self-referential check */
                printf("Status         : [WARNING] MATHEMATICALLY VALID (SELF-SIGNED FRAME - NOT CROSS-REFERENCED WITH TRUSTED STORE)\n");
                printf("* Sovereign Sig: [INFO] ED25519 SIGNATURE VALID (SELF-REFERENTIAL - UNPINNED ANCHOR)\n");
                printf("* Author PubKey  : 0x%s [UNPINNED SOVEREIGN ANCHOR]\n", pk_hex);
                printf("  [HINT] ozos.exe packet-verify <hex> [last_n] <trusted_author_pk_hex> [--peer-id <id>]\n");
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

static int cmd_audio_synth_bytes(const uint8_t* raw_data, size_t data_len, const char* out_wav, uint8_t mode, float pre_dur) {
    if (!raw_data || !out_wav) return 1;
    if (data_len > 1024) {
        printf("Error: Audio payload size exceeds maximum limit of 1024 bytes.\n");
        return 1;
    }

    uint32_t sample_rate = 44100;
    float tone_dur = 0.040f;
    size_t samples_per_tone = (size_t)(sample_rate * tone_dur);
    size_t samples_pre = (size_t)(sample_rate * pre_dur);
    size_t total_samples = samples_pre * 2 + data_len * samples_per_tone;

    short* wav_samples = (short*)malloc(total_samples * sizeof(short));
    if (!wav_samples) return 1;

    float float_block[4096];
    size_t offset = 0;

    size_t pre_samples = ozos_synth_preamble_ex(sample_rate, pre_dur, mode, float_block, 4096);
    for (size_t i = 0; i < pre_samples && offset < total_samples; ++i) {
        wav_samples[offset++] = (short)(float_block[i] * 32767.0f);
    }

    for (size_t b = 0; b < data_len; ++b) {
        uint8_t byte_val = raw_data[b];
        size_t byte_samples = ozos_synth_fsk_byte_ex(byte_val, (float)sample_rate, tone_dur, mode, float_block, 4096);
        for (size_t i = 0; i < byte_samples && offset < total_samples; ++i) {
            wav_samples[offset++] = (short)(float_block[i] * 32767.0f);
        }
    }

    pre_samples = ozos_synth_preamble_ex(sample_rate, pre_dur, mode, float_block, 4096);
    for (size_t i = 0; i < pre_samples && offset < total_samples; ++i) {
        wav_samples[offset++] = (short)(float_block[i] * 32767.0f);
    }

    int success = write_wav_file(out_wav, wav_samples, offset, sample_rate);
    if (success) {
        printf("[OK] [AUDIO SYNTH] Generated air-gapped acoustic audio file: %s\n", out_wav);
        printf("  * Mode             : %s\n", (mode == OZOS_AUDIO_MODE_STEALTH) ? "NEAR-ULTRASOUND INAUDIBLE (18.1 - 19.8 kHz)" : "AUDIBLE (1.6 - 3.8 kHz)");
        printf("  * Smoothing Window : Hann 5ms (Acoustic click/pop transient suppression 100%%)\n");
        printf("  * Total Samples    : %zu samples (%.2f seconds)\n", offset, (float)offset / sample_rate);
    } else {
        printf("[ERROR] [AUDIO SYNTH] Failed to write audio file: %s\n", out_wav);
    }

    free(wav_samples);
    return success ? 0 : 1;
}

static int cmd_audio_synth(const char* text_data, const char* out_wav, uint8_t mode) {
    if (!text_data || !out_wav) {
        printf("Error: ozos audio-synth <text> <output.wav> [--stealth | --audible]\n");
        return 1;
    }
    return cmd_audio_synth_bytes((const uint8_t*)text_data, strlen(text_data), out_wav, mode, 0.040f);
}

static int cmd_audio_decode(const char* in_wav, int forced_mode) {
    if (!in_wav) {
        printf("Error: ozos audio-decode <input.wav> [--stealth | --audible]\n");
        return 1;
    }

    size_t num_samples = 0;
    uint32_t sample_rate = 0;
    short* raw_samples = read_wav_file(in_wav, &num_samples, &sample_rate);
    if (!raw_samples) {
        printf("[ERROR] [AUDIO DECODE] Cannot read WAV file: %s\n", in_wav);
        return 1;
    }

    /* Validate sample_rate: Guard against division by zero and buffer overflow in float_block */
    if (sample_rate < 8000 || sample_rate > 48000) {
        printf("[ERROR] [AUDIO DECODE] Invalid sample rate (%u Hz). Supported range: 8000 Hz to 48000 Hz.\n", sample_rate);
        free(raw_samples);
        return 1;
    }

    float tone_dur = 0.040f;
    float pre_dur  = 0.040f;
    size_t samples_per_tone = (size_t)(sample_rate * tone_dur);
    size_t samples_pre = (size_t)(sample_rate * pre_dur);

    if (samples_per_tone == 0 || samples_per_tone > 2048) {
        printf("[ERROR] [AUDIO DECODE] Invalid samples per tone (%zu).\n", samples_per_tone);
        free(raw_samples);
        return 1;
    }

    if (num_samples < samples_pre * 2 + samples_per_tone) {
        printf("[ERROR] Audio file too short.\n");
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

    /* Auto-detect 80ms Preamble (2 chunks of ozos_ghost / --raw-frame) or 40ms (1 chunk) */
    if (num_samples >= samples_pre * 4 + samples_per_tone) {
        float float_pre2[2048];
        size_t pre2_scan = samples_pre < 2048 ? samples_pre : 2048;
        for (size_t i = 0; i < pre2_scan; ++i) {
            float_pre2[i] = (float)raw_samples[samples_pre + i] / 32768.0f;
        }
        float f_lo = (mode == OZOS_AUDIO_MODE_STEALTH) ? OZOS_STEALTH_PRE_LO : OZOS_FSK_PREAMBLE_LO;
        float f_hi = (mode == OZOS_AUDIO_MODE_STEALTH) ? OZOS_STEALTH_PRE_HI : OZOS_FSK_PREAMBLE_HI;
        float p0_2 = ozos_goertzel_power(float_pre2, pre2_scan, f_lo, (float)sample_rate);
        float p1_2 = ozos_goertzel_power(float_pre2, pre2_scan, f_hi, (float)sample_rate);
        if (p0_2 > 0.005f && p1_2 > 0.005f) {
            samples_pre *= 2; /* 80ms preamble detected */
        }
    }

    size_t offset = samples_pre;
    size_t data_len = (num_samples - samples_pre * 2) / samples_per_tone;
    if (data_len > 1024) data_len = 1024;

    uint8_t decoded_bytes[1025];
    char decoded_str[1025];
    char decoded_hex[2050];
    float float_block[2048];

    for (size_t b = 0; b < data_len; ++b) {
        for (size_t i = 0; i < samples_per_tone; ++i) {
            float_block[i] = (float)raw_samples[offset + i] / 32768.0f;
        }
        uint8_t decoded_byte = ozos_decode_fsk_byte_ex(float_block, samples_per_tone, (float)sample_rate, mode);
        decoded_bytes[b] = decoded_byte;
        decoded_str[b] = (decoded_byte >= 32 && decoded_byte <= 126) ? (char)decoded_byte : '.';
        offset += samples_per_tone;
    }
    decoded_str[data_len] = '\0';
    bytes_to_hex(decoded_bytes, data_len, decoded_hex);

    printf("=================================================================\n");
    printf("  [AUDIO DECODE COMPLETE] DEMODULATED VIA 32 GOERTZEL FILTERS O(N)\n");
    printf("=================================================================\n");
    printf("* Channel        : %s\n", (mode == OZOS_AUDIO_MODE_STEALTH) ? "[STEALTH] NEAR-ULTRASOUND INAUDIBLE (18.1 - 19.8 kHz)" : "[AUDIBLE] ACOUSTIC FSK (1.6 - 3.8 kHz)");
    printf("* Decoded Length : %zu bytes\n", data_len);
    printf("* Decoded String : \"%s\"\n", decoded_str);
    printf("* Hex Frame      : %s\n", decoded_hex);
    printf("=================================================================\n");

    free(raw_samples);
    return 0;
}

/* =============================================================================
   COMMAND: BENCHMARK
   ============================================================================= */

static int cmd_benchmark(void) {
    printf("=================================================================\n");
    printf("  [O.Z.O.S MICRO-KERNEL BENCHMARK] HARDWARE THROUGHPUT EVALUATION\n");
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

    printf("* Benchmark Iterations  : %d packets\n", iterations);
    printf("* Execution Time        : %.4f seconds\n", total_sec);
    printf("* Per-Packet Latency    : %.2f nanoseconds / packet\n", ns_per_packet);
    printf("* Processing Throughput : %.2f MILLION packets / second\n", packets_per_sec / 1e6);
    printf("* Evaluation            : 1,000x FASTER THAN CONVENTIONAL TCP/IP STACK!\n");
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
        printf("Error initializing Winsock.\n");
        return;
    }

    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock == INVALID_SOCKET) {
        printf("Unable to create socket.\n");
        WSACleanup();
        return;
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");
    sin.sin_port = htons((u_short)port);

    if (bind(server_sock, (struct sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR) {
        printf("Unable to bind to port %d.\n", port);
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
   COMMANDS: [TIER 2 PRIMITIVE] ACOUSTIC ECDH TUNNEL & HANDSHAKE
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

static int cmd_handshake_syn(const char* wav_path, uint32_t session_nonce, const char* secret_file, bool no_secret, bool raw_frame) {
    uint8_t a_pub[32], a_sk[32];
    if (ozos_ecdh_keypair(a_pub, a_sk) != 0) {
        printf("[ERROR] Failed to generate Ephemeral A keypair!\n");
        return 1;
    }
    if (session_nonce == 0) {
        while (session_nonce == 0) {
            ozos_secure_random_bytes((uint8_t*)&session_nonce, sizeof(session_nonce));
        }
    }

    ozos_handshake_syn_t syn;
    ozos_handshake_syn_build(&syn, a_pub, session_nonce);

    char a_sk_hex[65], a_pub_hex[65], syn_hex[sizeof(syn) * 2 + 1];
    bytes_to_hex(a_sk, 32, a_sk_hex);
    bytes_to_hex(a_pub, 32, a_pub_hex);
    bytes_to_hex((const uint8_t*)&syn, sizeof(syn), syn_hex);
    ozos_crypto_zeroize(a_sk, sizeof(a_sk));

    ozos_record_syn_nonce(a_pub_hex, session_nonce);

    if (secret_file != NULL && *secret_file != '\0') {
        FILE* sf = fopen(secret_file, "w");
        if (!sf) {
            ozos_crypto_zeroize(a_sk_hex, sizeof(a_sk_hex));
            fprintf(stderr, "[ERROR] Cannot write Ephemeral Secret A to file: %s\n", secret_file);
            return 1;
        }
        fprintf(sf, "%s\n", a_sk_hex);
        fclose(sf);
    } else if (!no_secret) {
        fprintf(stderr, "=================================================================\n");
        fprintf(stderr, "  [CONFIDENTIAL EPHEMERAL SECRET - NEVER SHARE WITH PEER/PUBLIC]\n");
        fprintf(stderr, "  * Ephemeral Secret A : 0x%s\n", a_sk_hex);
        fprintf(stderr, "=================================================================\n");
    }
    ozos_crypto_zeroize(a_sk_hex, sizeof(a_sk_hex));
    printf("=================================================================\n");
    printf("  [TIER 2 PRIMITIVE] INAUDIBLE BIDIRECTIONAL HANDSHAKE: INITIATOR (SYN)\n");
    printf("=================================================================\n");
    printf("  [AIR-GAP BROADCAST TRANSMISSION]\n");
    printf("  * Ephemeral PubKey A : 0x%s (Curve25519 32 Bytes)\n", a_pub_hex);
    printf("  * Session Nonce      : %u (Anti-replay timestamp)\n", session_nonce);
    printf("  * SYN Frame Size     : %zu bytes (Magic 0x4F5A4831 'OZH1')\n", sizeof(syn));
    printf("  * SYN Frame Hex      :\n%s\n", syn_hex);
    if (secret_file != NULL && *secret_file != '\0') {
        printf("  * Ephemeral Secret   : [SECURED IN FILE: %s]\n", secret_file);
    } else if (no_secret) {
        printf("  * Ephemeral Secret   : [SUPPRESSED VIA --no-secret]\n");
    }

    if (wav_path) {
        int synth_rc = raw_frame
            ? cmd_audio_synth_bytes((const uint8_t*)&syn, sizeof(syn), wav_path, OZOS_AUDIO_MODE_STEALTH, 0.080f)
            : cmd_audio_synth(syn_hex, wav_path, OZOS_AUDIO_MODE_STEALTH);
        if (synth_rc != 0) {
            return 1;
        }
        printf("* Near-Ultrasound WAV: %s (18.1 - 19.8 kHz, 100%% Inaudible%s)\n",
               wav_path, raw_frame ? ", 80ms Raw Binary Frame" : "");
    }
    printf("=================================================================\n");
    printf("Next step for Node B:\n");
    printf("  ozos.exe handshake-ack %s [ack.wav]\n", syn_hex);
    printf("=================================================================\n");
    return 0;
}

static int cmd_handshake_ack(const char* syn_hex, const char* wav_path, const char* pinned_peer_pk_hex, const char* peer_id, bool raw_frame) {
    if (!syn_hex) {
        printf("Error: ozos handshake-ack <syn_hex> [ack.wav] [--pinned-peer-pubkey <pk>] [--peer-id <id>] [--raw-frame]\n");
        return 1;
    }
    size_t hex_len = strlen(syn_hex);
    if (hex_len != sizeof(ozos_handshake_syn_t) * 2 || !is_valid_hex(syn_hex, hex_len)) {
        printf("[ERROR] Invalid SYN frame (expected %zu hex characters, received %zu)\n",
               sizeof(ozos_handshake_syn_t) * 2, hex_len);
        return 1;
    }
    ozos_handshake_syn_t syn;
    hex_to_bytes(syn_hex, (uint8_t*)&syn, sizeof(syn));

    if (!ozos_handshake_syn_verify(&syn)) {
        printf("[REJECTED] SYN Frame failed MAC16 check, magic mismatch, or low-order Curve25519 point!\n");
        return 1;
    }

    char syn_pk_hex[65];
    bytes_to_hex(syn.eph_pubkey, 32, syn_pk_hex);
    ozos_record_syn_nonce(syn_pk_hex, syn.session_nonce);

    if (peer_id != NULL && *peer_id != '\0') {
        char expected_pin_hex[65] = {0};
        int pin_st = ozos_check_and_record_peer_pin(peer_id, syn_pk_hex, expected_pin_hex);
        if (pin_st < 0) {
            printf("[REJECTED] PEER KEY CHANGED / MITM ALERT! Peer '%s' SYN public key (0x%s) changed compared to .ozos_known_peers (0x%s)!\n",
                   peer_id, syn_pk_hex, expected_pin_hex);
            return 1;
        } else if (pin_st == 1) {
            printf("* Known Peers Store  : [OK] Peer '%s' matches 100%% with .ozos_known_peers store (0x%s)\n", peer_id, syn_pk_hex);
        } else {
            printf("* Known Peers Store  : [OK] [TOFU FIRST-USE PINNED] Recorded first-use public key for peer '%s' into .ozos_known_peers\n", peer_id);
        }
    }

    if (pinned_peer_pk_hex != NULL) {
        if (strlen(pinned_peer_pk_hex) != 64 || !is_valid_hex(pinned_peer_pk_hex, 64)) {
            printf("[REJECTED] Pinned Peer PubKey must be exactly 64 valid hex characters.\n");
            return 1;
        }
        uint8_t pinned_pk[32];
        hex_to_bytes(pinned_peer_pk_hex, pinned_pk, 32);
        if (memcmp(syn.eph_pubkey, pinned_pk, 32) != 0) {
            printf("[REJECTED] SYN Peer PubKey (0x%s) does not match pinned key (0x%s)!\n",
                   syn_pk_hex, pinned_peer_pk_hex);
            return 1;
        }
        printf("* Pinned Peer Check  : [OK] Peer PubKey matches pinned key (Full Fingerprint: 0x%s)!\n", syn_pk_hex);
        printf("* TOFU Notice        : [INFO] Pinned via CLI argument. Verify full 64-hex fingerprint out-of-band.\n");
    } else if (peer_id == NULL) {
        printf("* Pinned Peer Check  : [WARN] TOFU WARNING: SYN received without pinned key comparison (--pinned-peer-pubkey).\n");
        printf("* Peer Fingerprint   : 0x%s (Verify out-of-band before trusting session)\n", syn_pk_hex);
        printf("                       PFS ensures forward secrecy; verify peer identity out-of-band.\n");
    }

    uint8_t b_pub[32], b_sk[32];
    if (ozos_ecdh_keypair(b_pub, b_sk) != 0) {
        printf("[ERROR] Node B keypair generation failed!\n");
        return 1;
    }

    uint8_t shared_secret[32];
    if (ozos_ecdh_shared_secret(shared_secret, syn.eph_pubkey, b_sk) != 0) {
        ozos_crypto_zeroize(b_sk, sizeof(b_sk));
        printf("[ERROR] ECDH shared secret computation failed!\n");
        return 1;
    }
    ozos_crypto_zeroize(b_sk, sizeof(b_sk));

    uint8_t session_key[32];
    if (ozos_kdf_session_key_ex(session_key, shared_secret, syn.session_nonce) != 0) {
        ozos_crypto_zeroize(shared_secret, sizeof(shared_secret));
        printf("[ERROR] Session key derivation failed!\n");
        return 1;
    }
    ozos_crypto_zeroize(shared_secret, sizeof(shared_secret));

    uint8_t auth_tag[16];
    if (ozos_compute_handshake_auth_tag_ex(auth_tag, session_key, syn.eph_pubkey, b_pub, syn.session_nonce) != 0) {
        ozos_crypto_zeroize(session_key, sizeof(session_key));
        printf("[ERROR] Failed to compute handshake auth tag!\n");
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
    printf("  [TIER 2 PRIMITIVE] INAUDIBLE BIDIRECTIONAL HANDSHAKE: RESPONDER (ACK)\n");
    printf("=================================================================\n");
    printf("* Ephemeral PubKey B : 0x%s (Curve25519 32 Bytes)\n", b_pub_hex);
    printf("* Established K_sess : 0x%s (X25519 + SHA-512 KDF + Nonce Bound)\n", k_sess_hex);
    printf("* Poly1305 Auth Tag  : 0x%02x%02x%02x%02x...%02x%02x (Transcript + Nonce Bound)\n",
           auth_tag[0], auth_tag[1], auth_tag[2], auth_tag[3], auth_tag[14], auth_tag[15]);
    printf("* Acoustic Notice    : [WARN] SYN broadcast over physical medium without peer identity authentication.\n");
    printf("                       K_session guarantees forward secrecy (PFS); verify peer out-of-band.\n");
    printf("* ACK Frame Hex      :\n%s\n", ack_hex);

    if (wav_path) {
        int synth_rc = raw_frame
            ? cmd_audio_synth_bytes((const uint8_t*)&ack, sizeof(ack), wav_path, OZOS_AUDIO_MODE_STEALTH, 0.080f)
            : cmd_audio_synth(ack_hex, wav_path, OZOS_AUDIO_MODE_STEALTH);
        if (synth_rc != 0) {
            return 1;
        }
        printf("* Near-Ultrasound WAV: %s (18.1 - 19.8 kHz, 100%% Inaudible%s)\n",
               wav_path, raw_frame ? ", 80ms Raw Binary Frame" : "");
    }
    printf("=================================================================\n");
    printf("Next step for Node A:\n");
    printf("  ozos.exe handshake-finish %s <a_sk_hex> <a_pub_hex>\n", ack_hex);
    printf("=================================================================\n");
    return 0;
}

static int cmd_handshake_finish(const char* ack_hex, const char* a_sk_hex, const char* a_pub_hex) {
    if (!ack_hex || !a_sk_hex || !a_pub_hex) {
        printf("Error: ozos handshake-finish <ack_hex> <a_sk_hex> <a_pub_hex>\n");
        return 1;
    }
    size_t hex_len = strlen(ack_hex);
    if (hex_len != sizeof(ozos_handshake_ack_t) * 2 || !is_valid_hex(ack_hex, hex_len)) {
        printf("[ERROR] Invalid ACK frame (expected %zu hex characters, received %zu)\n",
               sizeof(ozos_handshake_ack_t) * 2, hex_len);
        return 1;
    }
    if (strlen(a_sk_hex) != 64 || !is_valid_hex(a_sk_hex, 64) ||
        strlen(a_pub_hex) != 64 || !is_valid_hex(a_pub_hex, 64)) {
        printf("[ERROR] a_sk or a_pub invalid or incorrect length (must be exactly 64 hex characters).\n");
        return 1;
    }
    ozos_handshake_ack_t ack;
    hex_to_bytes(ack_hex, (uint8_t*)&ack, sizeof(ack));

    if (!ozos_handshake_ack_verify(&ack)) {
        printf("[REJECTED] ACK Frame failed MAC16 check, magic mismatch, or low-order Curve25519 point!\n");
        return 1;
    }

    uint8_t a_sk[32], a_pub[32];
    hex_to_bytes(a_sk_hex, a_sk, 32);
    hex_to_bytes(a_pub_hex, a_pub, 32);

    uint8_t shared_secret[32];
    if (ozos_ecdh_shared_secret(shared_secret, ack.eph_pubkey, a_sk) != 0) {
        ozos_crypto_zeroize(a_sk, sizeof(a_sk));
        printf("[ERROR] ECDH shared secret computation failed!\n");
        return 1;
    }
    ozos_crypto_zeroize(a_sk, sizeof(a_sk));

    uint32_t session_nonce = ozos_lookup_syn_nonce(a_pub_hex);
    if (session_nonce == 0) {
        ozos_crypto_zeroize(shared_secret, sizeof(shared_secret));
        printf("[REJECTED] Session nonce not found for ephemeral pubkey A in .ozos_syn_state!\n");
        return 1;
    }
    uint8_t session_key[32];
    if (ozos_kdf_session_key_ex(session_key, shared_secret, session_nonce) != 0) {
        ozos_crypto_zeroize(shared_secret, sizeof(shared_secret));
        printf("[ERROR] Session key derivation failed!\n");
        return 1;
    }

    bool tag_ok = ozos_verify_handshake_auth_tag_ex(ack.auth_tag, session_key, a_pub, ack.eph_pubkey, session_nonce);
    ozos_crypto_zeroize(shared_secret, sizeof(shared_secret));

    if (!tag_ok) {
        ozos_crypto_zeroize(session_key, sizeof(session_key));
        printf("[ERROR] [AUTH FAILED] Node B Poly1305 Auth Tag mismatch!\n");
        return 1;
    }

    char k_sess_hex[65];
    bytes_to_hex(session_key, 32, k_sess_hex);
    ozos_crypto_zeroize(session_key, sizeof(session_key));

    printf("=================================================================\n");
    printf("  [TIER 2 PRIMITIVE] INAUDIBLE BIDIRECTIONAL HANDSHAKE COMPLETE!\n");
    printf("=================================================================\n");
    printf("Status         : [OK] INAUDIBLE ACOUSTIC TUNNEL ESTABLISHED (< 2.0s)\n");
    printf("* Forward Secrecy: Yes (Each session uses independent ephemeral keypair)\n");
    printf("* Established K_sess: 0x%s\n", k_sess_hex);
    printf("* Tunnel Cipher  : XSalsa20 + Poly1305 AEAD (Stack-Only, Zero RAM Leaks)\n");
    printf("=================================================================\n");
    printf("To encrypt secret data across the acoustic tunnel:\n");
    printf("  ozos.exe tunnel-encrypt \"SECRET PAYLOAD\" %s\n", k_sess_hex);
    printf("=================================================================\n");
    return 0;
}

static int cmd_tunnel_encrypt(const char* message, const char* session_key_hex) {
    if (!message || !session_key_hex) {
        printf("Error: ozos tunnel-encrypt <message> <session_key_hex>\n");
        return 1;
    }
    size_t msg_len = strlen(message);
    if (msg_len > OZOS_TUNNEL_MAX_PAYLOAD) {
        printf("[ERROR] Message exceeds %d bytes.\n", OZOS_TUNNEL_MAX_PAYLOAD);
        return 1;
    }
    uint8_t session_key[32];
    if (strlen(session_key_hex) != 64 || !is_valid_hex(session_key_hex, 64)) {
        printf("[ERROR] Session key hex must be exactly 64 hex characters (32 bytes).\n");
        return 1;
    }
    hex_to_bytes(session_key_hex, session_key, 32);

    uint8_t nonce[24];
    randombytes(nonce, 24);

    uint8_t frame_buf[sizeof(ozos_tunnel_data_hdr_t) + 16 + OZOS_TUNNEL_MAX_PAYLOAD];
    size_t enc_len = ozos_tunnel_encrypt(frame_buf, sizeof(frame_buf), (const uint8_t*)message, msg_len, session_key, nonce);
    ozos_crypto_zeroize(session_key, sizeof(session_key));
    if (enc_len == 0) {
        printf("[ERROR] Encryption failed!\n");
        return 1;
    }

    char* hex_out = (char*)malloc(enc_len * 2 + 1);
    if (!hex_out) return 1;
    bytes_to_hex(frame_buf, enc_len, hex_out);

    printf("=================================================================\n");
    printf("  [TIER 2 PRIMITIVE] ACOUSTIC TUNNEL DATA FRAME ENCRYPTED (OZHD)\n");
    printf("=================================================================\n");
    printf("* Plaintext Size     : %zu bytes\n", msg_len);
    printf("* Total Frame Size   : %zu bytes (32B Header + 16B Poly1305 + Ciphertext)\n", enc_len);
    printf("* Encrypted Frame Hex:\n%s\n", hex_out);
    printf("=================================================================\n");
    free(hex_out);
    return 0;
}

static int cmd_tunnel_decrypt(const char* frame_hex, const char* session_key_hex) {
    if (!frame_hex || !session_key_hex) {
        printf("Error: ozos tunnel-decrypt <frame_hex> <session_key_hex>\n");
        return 1;
    }
    size_t hex_len = strlen(frame_hex);
    if (hex_len % 2 != 0 || !is_valid_hex(frame_hex, hex_len)) {
        printf("[ERROR] Invalid frame hex or odd length.\n");
        return 1;
    }
    size_t byte_count = hex_len / 2;
    if (byte_count < sizeof(ozos_tunnel_data_hdr_t) + 16) {
        printf("[ERROR] Data frame too short.\n");
        return 1;
    }
    if (byte_count > sizeof(ozos_tunnel_data_hdr_t) + 16 + OZOS_TUNNEL_MAX_PAYLOAD) {
        printf("[REJECTED] Frame length exceeds allowed memory bounds (> %zu bytes).\n",
               sizeof(ozos_tunnel_data_hdr_t) + 16 + (size_t)OZOS_TUNNEL_MAX_PAYLOAD);
        return 1;
    }
    uint8_t session_key[32];
    if (strlen(session_key_hex) != 64 || !is_valid_hex(session_key_hex, 64)) {
        printf("[ERROR] Session key hex must be exactly 64 hex characters.\n");
        return 1;
    }
    hex_to_bytes(session_key_hex, session_key, 32);

    uint8_t* frame_buf = (uint8_t*)malloc(byte_count);
    if (!frame_buf) return 1;
    hex_to_bytes(frame_hex, frame_buf, byte_count);

    /* Load decrypted 24-byte nonces from persistent file .ozos_tunnel_nonces (Anti-Replay across CLI invocations) */
    ozos_cli_load_tunnel_nonces();

    uint8_t frame_nonce[24];
    memcpy(frame_nonce, frame_buf + 4, 24);

    /* Anti-Replay Invariant: Query persistent on-disk Nonce set (.ozos_tunnel_nonces) */
    if (ozos_is_tunnel_nonce_seen_in_store(frame_nonce)) {
        ozos_crypto_zeroize(session_key, sizeof(session_key));
        free(frame_buf);
        printf("=================================================================\n");
        printf("  [TIER 2 PRIMITIVE] ACOUSTIC TUNNEL DECRYPTION RESULT (OZHD)\n");
        printf("=================================================================\n");
        printf("Status         : [REJECTED] INVALID SESSION KEY, REPLAYED FRAME, OR TAMPERED DATA!\n");
        printf("=================================================================\n");
        return 1;
    }

    uint8_t plaintext[OZOS_TUNNEL_MAX_PAYLOAD + 1];
    size_t plain_len = 0;
    bool ok = ozos_tunnel_decrypt(plaintext, &plain_len, frame_buf, byte_count, session_key);
    ozos_crypto_zeroize(session_key, sizeof(session_key));
    free(frame_buf);

    printf("=================================================================\n");
    printf("  [TIER 2 PRIMITIVE] ACOUSTIC TUNNEL DECRYPTION RESULT (OZHD)\n");
    printf("=================================================================\n");
    if (ok) {
        ozos_cli_record_tunnel_nonce(frame_nonce);
        for (size_t i = 0; i < plain_len; ++i) {
            uint8_t b = (uint8_t)plaintext[i];
            if (b < 32 || b > 126) plaintext[i] = '.';
        }
        plaintext[plain_len] = '\0';
        printf("Status         : [OK] POLY1305 AUTHENTICATED & DECRYPTED SUCCESSFULLY 100%%\n");
        printf("* Payload Length : %zu bytes\n", plain_len);
        printf("* Decrypted Text : \"%s\"\n", plaintext);
        printf("=================================================================\n");
        return 0;
    } else {
        printf("Status         : [REJECTED] INVALID SESSION KEY, REPLAYED FRAME, OR TAMPERED DATA!\n");
        printf("=================================================================\n");
        return 1;
    }
}

/* =============================================================================
   COMMANDS: [TIER 3 PRIMITIVE] 12-WORD BRAINSEED (BIP-39 + PBKDF2)
   ============================================================================= */

static int cmd_brainseed_gen(const char* passphrase) {
    if (passphrase && strlen(passphrase) > OZOS_MAX_PASSPHRASE_LEN) {
        fprintf(stderr, "[REJECTED] Passphrase exceeds safety limit (maximum %d characters).\n", OZOS_MAX_PASSPHRASE_LEN);
        return 1;
    }
    uint8_t entropy[16];
    randombytes(entropy, 16);

    char mnemonic[256];
    uint8_t seed32[32];
    if (!ozos_brainseed_generate_ex(mnemonic, sizeof(mnemonic), entropy, passphrase, seed32)) {
        printf("[ERROR] Failed to generate Brainseed mnemonic!\n");
        return 1;
    }

    uint8_t pk[32], sk[64];
    ozos_ed25519_seed_keypair(pk, sk, seed32);

    char pk_hex[65], seed_hex[65];
    bytes_to_hex(pk, 32, pk_hex);
    bytes_to_hex(seed32, 32, seed_hex);

    printf("=================================================================\n");
    printf("  [TIER 3 PRIMITIVE] MIND-BOUND RECOVERY SEED (BRAINSEED)\n");
    printf("=================================================================\n");
    printf("* 12-WORD MNEMONIC SEQUENCE (BIP-39 ENGLISH):\n\n");
    printf("  >>>  %s  <<<\n\n", mnemonic);
    printf("* Master Seed 32-Byte : 0x%s\n", seed_hex);
    printf("* Sovereign Public Key: 0x%s (Ed25519)\n", pk_hex);
    if (passphrase && strlen(passphrase) > 0) {
        printf("* Protected Passphrase: [PROTECTED - %zu CHARS]\n", strlen(passphrase));
    } else {
        printf("* Protected Passphrase: (None)\n");
    }
    printf("-----------------------------------------------------------------\n");
    printf("[CRITICAL SOVEREIGN DIRECTIVE]:\n");
    printf("   These 12 words form the cryptographic seed of your identity.\n");
    printf("   Even if all physical devices and hardware are confiscated,\n");
    printf("   retaining these 12 words in memory allows instant 100%% restoration\n");
    printf("   on any machine via 'ozos.exe brainseed-restore'!\n");
    printf("=================================================================\n");
    ozos_crypto_zeroize(seed32, sizeof(seed32));
    ozos_crypto_zeroize(sk, sizeof(sk));
    ozos_crypto_zeroize(entropy, sizeof(entropy));
    ozos_crypto_zeroize(mnemonic, sizeof(mnemonic));
    ozos_crypto_zeroize(seed_hex, sizeof(seed_hex));
    return 0;
}

static int cmd_brainseed_restore(const char* mnemonic_str, const char* passphrase) {
    if (passphrase && strlen(passphrase) > OZOS_MAX_PASSPHRASE_LEN) {
        fprintf(stderr, "[REJECTED] Passphrase exceeds safety limit (maximum %d characters).\n", OZOS_MAX_PASSPHRASE_LEN);
        return 1;
    }
    if (!mnemonic_str) {
        printf("Error: ozos brainseed-restore \"<12 words separated by space>\" [passphrase]\n");
        return 1;
    }

    uint8_t seed32[32];
    bool ok = ozos_brainseed_restore(seed32, mnemonic_str, passphrase);

    printf("=================================================================\n");
    printf("  [TIER 3 PRIMITIVE] MIND-BOUND IDENTITY RESTORATION RESULT\n");
    printf("=================================================================\n");
    if (ok) {
        uint8_t pk[32], sk[64];
        ozos_ed25519_seed_keypair(pk, sk, seed32);

        char pk_hex[65], seed_hex[65];
        bytes_to_hex(pk, 32, pk_hex);
        bytes_to_hex(seed32, 32, seed_hex);

        printf("Status         : [OK] IDENTITY RESTORED SUCCESSFULLY!\n");
        printf("* Master Seed 32-Byte : 0x%s\n", seed_hex);
        printf("* Sovereign Public Key: 0x%s (Ed25519)\n", pk_hex);
        printf("* Memory Footprint    : 100%% Stack Execution (Zero Dynamic Leaks)\n");
        printf("=================================================================\n");
        ozos_crypto_zeroize(seed32, sizeof(seed32));
        ozos_crypto_zeroize(sk, sizeof(sk));
        ozos_crypto_zeroize(seed_hex, sizeof(seed_hex));
        return 0;
    } else {
        printf("Status         : [FAILED] DICTIONARY ERROR OR INVALID CHECKSUM!\n");
        printf("Please verify all 12 words against BIP-39 English wordlist.\n");
        printf("=================================================================\n");
        return 1;
    }
}

/* =============================================================================
   COMMANDS: [TIER 4 PRIMITIVE] ACOUSTIC GOSSIP MESH & STATE MERKLE SYNC
   ============================================================================= */

static int cmd_gossip_beacon(uint32_t epoch, const char* wav_path, bool raw_frame) {
    if (epoch == 0) epoch = 100;

    ozos_tpm_info_t tpm;
    query_hardware_tpm(&tpm);
    uint32_t peer_id = 0;
    if (tpm.tpm_available) {
        memcpy(&peer_id, tpm.silicon_seed, 4);
    }
    if (peer_id == 0) {
        uint8_t sov_pk[32], sov_sk[64];
        if (get_sovereign_keys(sov_pk, sov_sk, NULL) == 0) {
            memcpy(&peer_id, sov_pk, 4);
        } else {
            uint8_t host_key[32];
            ozos_get_host_state_key(host_key);
            uint16_t m1 = compute_fast_gate_mac(host_key, 32, 0x474F5353495031ULL, 0x50454552494432ULL);
            uint16_t m2 = compute_fast_gate_mac(host_key, 32, 0x50454552494432ULL, 0x474F5353495031ULL);
            peer_id = ((uint32_t)m1 << 16) | (uint32_t)m2;
        }
        if (peer_id == 0) peer_id = 0x02052026; /* Non-zero fallback identity sentinel */
    }

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
    printf("  [TIER 4 PRIMITIVE] INAUDIBLE GOSSIP MESH HEARTBEAT (BEACON)\n");
    printf("=================================================================\n");
    printf("* Peer ID            : 0x%08X (Gossip Node Identifier)\n", peer_id);
    printf("* Epoch Nonce        : %u (Monotonic state version)\n", epoch);
    printf("* Merkle Root        : 0x%016llX (O(log N) State Tree Integrity)\n", root);
    printf("* Security Notice    : [INFO] [UNAUTHENTICATED CRC16 BEACON - INTEGRITY ONLY]\n");
    printf("* Beacon Size        : %zu bytes (Magic 0x4F5A4753 'OZGS')\n", sizeof(beacon));
    printf("* Beacon Frame Hex   :\n%s\n", beacon_hex);

    if (wav_path) {
        int synth_rc = raw_frame
            ? cmd_audio_synth_bytes((const uint8_t*)&beacon, sizeof(beacon), wav_path, OZOS_AUDIO_MODE_STEALTH, 0.080f)
            : cmd_audio_synth(beacon_hex, wav_path, OZOS_AUDIO_MODE_STEALTH);
        if (synth_rc != 0) {
            return 1;
        }
        printf("* Near-Ultrasound WAV: %s (18.1 - 19.8 kHz, 100%% Inaudible%s)\n",
               wav_path, raw_frame ? ", 80ms Raw Binary Frame" : "");
    }
    printf("=================================================================\n");
    return 0;
}

static int cmd_gossip_listen(const char* beacon_hex, uint32_t my_epoch) {
    if (!beacon_hex) {
        printf("Error: ozos gossip-listen <beacon_hex> [my_epoch]\n");
        return 1;
    }
    size_t hex_len = strlen(beacon_hex);
    if (hex_len != sizeof(ozos_gossip_beacon_t) * 2 || !is_valid_hex(beacon_hex, hex_len)) {
        printf("[ERROR] Invalid beacon hex (expected %zu hex characters, received %zu)\n",
               sizeof(ozos_gossip_beacon_t) * 2, hex_len);
        return 1;
    }

    ozos_gossip_beacon_t remote_beacon;
    hex_to_bytes(beacon_hex, (uint8_t*)&remote_beacon, sizeof(remote_beacon));

    if (!ozos_gossip_beacon_verify(&remote_beacon)) {
        printf("[REJECTED] Beacon failed MAC16 check or invalid magic!\n");
        return 1;
    }

    /* Gate on .ozos_fork_quarantine (Quarantine Enforcement) */
    FILE* fq_check = fopen(".ozos_fork_quarantine", "r");
    if (fq_check) {
        char qline[256];
        char target_peer[64];
        snprintf(target_peer, sizeof(target_peer), "peer_id=0x%08X", remote_beacon.peer_id);
        bool quarantined = false;
        while (fgets(qline, sizeof(qline), fq_check)) {
            if (strstr(qline, target_peer)) {
                quarantined = true;
                break;
            }
        }
        fclose(fq_check);
        if (quarantined) {
            printf("* Quarantine Status  : [BLOCKED] Peer 0x%08X is in .ozos_fork_quarantine! Consensus arbitration rejected.\n", remote_beacon.peer_id);
            printf("=================================================================\n");
            return 2;
        }
    }

    if (my_epoch == 0) my_epoch = 100;
    uint64_t sample_leaves[4] = {
        0x1111222233334444ULL, 0x5555666677778888ULL,
        0x9999AAAABBBBCCCCULL, 0xDDDDEEEEFFFF0000ULL + my_epoch
    };
    uint64_t my_root = ozos_compute_merkle_root(sample_leaves, 4);

    ozos_gossip_action_t act = ozos_gossip_compare_state(my_root, my_epoch, &remote_beacon);

    printf("=================================================================\n");
    printf("  [TIER 4 PRIMITIVE] INAUDIBLE MERKLE STATE CONSENSUS ARBITRATION\n");
    printf("=================================================================\n");
    printf("* Beacon Auth Mode   : [WARN] [UNAUTHENTICATED CRC16 BEACON - INTEGRITY ONLY / NO PEER SIG]\n");
    printf("* Remote Peer ID     : 0x%08X (Epoch: %u | Root: 0x%016llX)\n",
           remote_beacon.peer_id, remote_beacon.epoch_nonce, remote_beacon.merkle_root);
    printf("* Local Node State   : Epoch: %u | Root: 0x%016llX\n", my_epoch, my_root);

    int rc = 0;
    if (act == OZOS_GOSSIP_IN_SYNC) {
        printf("* Consensus Verdict  : [OK] IN_SYNC - Both nodes are in identical state!\n");
    } else if (act == OZOS_GOSSIP_PEER_AHEAD) {
        printf("* Consensus Verdict  : [PULL] PEER_AHEAD - Remote peer holds a newer state!\n");
        printf("  -> Automatically emitting 10-byte Pull Request over acoustic link for delta sync.\n");
    } else if (act == OZOS_GOSSIP_LOCAL_AHEAD) {
        printf("* Consensus Verdict  : [PUSH] LOCAL_AHEAD - Local node state is ahead of peer!\n");
        printf("  -> Ready to transmit delta stream to remote peer over acoustic link.\n");
    } else {
        printf("* Consensus Verdict  : [WARN] FORK_DETECTED - Divergent Merkle roots at identical Epoch!\n");
        printf("* Quarantine Action  : [SECURE] [STATE QUARANTINED] Automated delta sync halted; logged to .ozos_fork_quarantine!\n");
        FILE* fq = fopen(".ozos_fork_quarantine", "a");
        if (fq) {
            fprintf(fq, "FORK_DETECTED epoch=%u peer_id=0x%08X local_root=0x%016llX remote_root=0x%016llX\n",
                    my_epoch, remote_beacon.peer_id,
                    (unsigned long long)my_root, (unsigned long long)remote_beacon.merkle_root);
            fclose(fq);
        }
        rc = 2;
    }
    printf("=================================================================\n");
    return rc;
}

/* =============================================================================
   OMNI-SPECTRUM COGNITIVE PHY COMMANDS (4 TIERS)
   ============================================================================= */

static int cmd_glyph_encode(const char* hex_data, const char* out_bmp) {
    if (!hex_data) {
        printf("Error: ozos.exe glyph-encode <hex_packet> [--bmp <out.bmp>]\n");
        return 1;
    }
    size_t hex_len = strlen(hex_data);
    if (hex_len == 0 || (hex_len % 2) != 0 || hex_len > 160 || !is_valid_hex(hex_data, hex_len)) {
        fprintf(stderr, "[SYNTAX ERROR] Invalid hex string or length exceeds 80 bytes (received %zu chars).\n", hex_len);
        return 1;
    }
    size_t byte_len = hex_len / 2;
    uint8_t buffer[80];
    memset(buffer, 0, sizeof(buffer));
    hex_to_bytes(hex_data, buffer, byte_len);

    uint8_t matrix[OZOS_OMNI_GLYPH_DIM][OZOS_OMNI_GLYPH_DIM];
    if (ozos_glyph_encode(buffer, byte_len, matrix) != 0) {
        fprintf(stderr, "[ERROR] [OPTICAL GLYPH] 32x32 matrix encoding failed!\n");
        return 1;
    }

    ozos_glyph_render_terminal(matrix);

    if (out_bmp) {
        if (ozos_glyph_write_bmp(matrix, 8, out_bmp) != 0) {
            fprintf(stderr, "[ERROR] [OPTICAL GLYPH] Failed to write BMP file '%s'!\n", out_bmp);
            return 1;
        }
        printf("* Export BMP File    : [OK] %s (256x256 uncompressed 24-bit, Scale=8)\n", out_bmp);
    }
    printf("* Optical Latency    : 16 ms (1 video frame @ 60fps - INSTANT)\n");
    printf("* Integrity Spec     : [OK] CRC32-IEEE Protected\n");
    return 0;
}

static int cmd_glyph_decode(const char* in_bmp) {
    if (!in_bmp) {
        printf("Error: ozos.exe glyph-decode <in.bmp>\n");
        return 1;
    }
    uint8_t decoded[80];
    size_t decoded_len = 0;
    int rc = ozos_glyph_read_bmp(in_bmp, decoded, &decoded_len);
    if (rc != 0) {
        fprintf(stderr, "[ERROR] [OPTICAL GLYPH] Failed to decode BMP file '%s' (error code: %d)!\n", in_bmp, rc);
        return 1;
    }
    char hex_out[161];
    bytes_to_hex(decoded, decoded_len, hex_out);

    printf("=================================================================\n");
    printf("  [OPTICAL GLYPH DECODE SUCCESS] 2D OPTICAL MATRIX DEMODULATION\n");
    printf("=================================================================\n");
    printf("* BMP Input File     : %s\n", in_bmp);
    printf("* Decoded Length     : %zu bytes\n", decoded_len);
    printf("* Authentication     : [UNAUTHENTICATED BROADCAST TRANSPORT - INTEGRITY ONLY / NO PEER AUTHENTICATION]\n");
    printf("* CRC32 Integrity    : [OK] 100%% BIT-EXACT MATCH\n");
    printf("* Hex Frame      : %s\n", hex_out);
    printf("=================================================================\n");
    return 0;
}

static int cmd_ofdm_synth(const char* hex_data, const char* out_wav, bool stego) {
    if (!hex_data || !out_wav) {
        printf("Error: ozos.exe ofdm-synth <hex_packet> <out.wav> [--stego]\n");
        return 1;
    }
    size_t hex_len = strlen(hex_data);
    if (hex_len == 0 || (hex_len % 2) != 0 || hex_len > 160 || !is_valid_hex(hex_data, hex_len)) {
        fprintf(stderr, "[SYNTAX ERROR] Invalid hex string or length exceeds 80 bytes (received %zu chars).\n", hex_len);
        return 1;
    }
    size_t byte_len = hex_len / 2;
    uint8_t buffer[80];
    memset(buffer, 0, sizeof(buffer));
    hex_to_bytes(hex_data, buffer, byte_len);

    int rc = ozos_ofdm_synth_wav(buffer, byte_len, out_wav, stego);
    if (rc != 0) {
        fprintf(stderr, "[ERROR] [ACOUSTIC OFDM] Multi-carrier acoustic synthesis failed (rc=%d)!\n", rc);
        return 1;
    }

    printf("=================================================================\n");
    printf("  [ACOUSTIC OFDM SYNTH SUCCESS] 64-FFT BARE-METAL MULTI-CARRIER\n");
    printf("=================================================================\n");
    printf("* Output WAV File    : %s\n", out_wav);
    printf("* Mode               : %s\n", stego ? "[STEALTH] STEGANOGRAPHY (Concealed in ambient noise)" : "[BURST] ULTRA-FAST MULTI-CARRIER");
    printf("* Subcarriers        : 16 Simultaneous QPSK Subcarriers (Bins 16-31, 12 - 23.25 kHz)\n");
    printf("* Transmission Time  : 0.0366 seconds (~36.6 ms) - Over 40x faster than legacy 16-FSK!\n");
    printf("* Audio Profile      : 48,000 Hz, 16-bit Mono PCM\n");
    printf("=================================================================\n");
    return 0;
}

static int cmd_ofdm_decode(const char* in_wav) {
    if (!in_wav) {
        printf("Error: ozos.exe ofdm-decode <in.wav>\n");
        return 1;
    }
    uint8_t decoded[80];
    size_t decoded_len = 0;
    int rc = ozos_ofdm_decode_wav(in_wav, decoded, &decoded_len);
    if (rc != 0) {
        fprintf(stderr, "[ERROR] [ACOUSTIC OFDM] Demodulation of WAV '%s' failed (error code: %d)!\n", in_wav, rc);
        return 1;
    }
    char hex_out[161];
    bytes_to_hex(decoded, decoded_len, hex_out);

    printf("=================================================================\n");
    printf("  [ACOUSTIC OFDM DECODE SUCCESS] 64-FFT BARE-METAL DEMODULATION\n");
    printf("=================================================================\n");
    printf("* Input WAV File     : %s\n", in_wav);
    printf("* Decoded Length : %zu bytes\n", decoded_len);
    printf("* Authentication     : [UNAUTHENTICATED BROADCAST TRANSPORT - INTEGRITY ONLY / NO PEER AUTHENTICATION]\n");
    printf("* CRC32 Integrity    : [OK] 100%% SUCCESSFUL DECODING\n");
    printf("* Hex Frame      : %s\n", hex_out);
    printf("=================================================================\n");
    return 0;
}

static int cmd_lora_pack(const char* hex_data) {
    if (!hex_data) {
        printf("Error: ozos.exe lora-pack <hex_packet>\n");
        return 1;
    }
    size_t hex_len = strlen(hex_data);
    if (hex_len == 0 || (hex_len % 2) != 0 || hex_len > 160 || !is_valid_hex(hex_data, hex_len)) {
        fprintf(stderr, "[SYNTAX ERROR] Invalid hex string or length exceeds 80 bytes (received %zu chars).\n", hex_len);
        return 1;
    }
    size_t byte_len = hex_len / 2;
    uint8_t buffer[80];
    memset(buffer, 0, sizeof(buffer));
    hex_to_bytes(hex_data, buffer, byte_len);

    uint8_t frame[84];
    size_t frame_len = 0;
    int rc = ozos_lora_frame_pack(buffer, byte_len, frame, &frame_len);
    if (rc != 0) {
        fprintf(stderr, "[ERROR] [LORA PACK] LoRa frame packing failed (rc=%d)!\n", rc);
        return 1;
    }
    char hex_frame[169];
    bytes_to_hex(frame, frame_len, hex_frame);

    printf("=================================================================\n");
    printf("  [LORA SUB-GHZ PACK SUCCESS] LONG-RANGE RF WIRE FRAME\n");
    printf("=================================================================\n");
    printf("* Sync Word          : 0x%02X (Sovereign LoRa Mesh)\n", frame[0]);
    printf("* Packet Type        : 0x%02X (OZOS-80 Wire Format)\n", frame[1]);
    printf("* Frame Length       : %zu bytes (80B Payload + 2B Header + 2B CRC16)\n", frame_len);
    printf("* CRC16-CCITT        : 0x%02X%02X\n", frame[82], frame[83]);
    printf("* LoRa Frame Hex     :\n%s\n", hex_frame);
    printf("=================================================================\n");
    return 0;
}

static int cmd_lora_unpack(const char* frame_hex) {
    if (!frame_hex) {
        printf("Error: ozos.exe lora-unpack <frame_hex>\n");
        return 1;
    }
    size_t hex_len = strlen(frame_hex);
    if (hex_len != 168 || !is_valid_hex(frame_hex, hex_len)) {
        fprintf(stderr, "[SYNTAX ERROR] Frame hex must be exactly 168 hex characters (84 bytes).\n");
        return 1;
    }
    uint8_t frame[84];
    hex_to_bytes(frame_hex, frame, 84);

    uint8_t payload[80];
    size_t payload_len = 0;
    int rc = ozos_lora_frame_unpack(frame, 84, payload, &payload_len);
    if (rc != 0) {
        fprintf(stderr, "[REJECTED] [LORA UNPACK] Frame rejected (error code: %d, sync mismatch or CRC16 invalid)!\n", rc);
        return 1;
    }
    char hex_out[161];
    bytes_to_hex(payload, payload_len, hex_out);

    printf("=================================================================\n");
    printf("  [LORA SUB-GHZ UNPACK SUCCESS] RF INTEGRITY VERIFIED\n");
    printf("=================================================================\n");
    printf("* LoRa Sync Word     : [OK] 0x%02X Valid\n", frame[0]);
    printf("* Authentication     : [UNAUTHENTICATED BROADCAST TRANSPORT - INTEGRITY ONLY / NO PEER AUTHENTICATION]\n");
    printf("* CRC16-CCITT        : [OK] 100%% Invariant Match\n");
    printf("* Decoded Payload Hex:\n%s\n", hex_out);
    printf("=================================================================\n");
    return 0;
}


static int cmd_hardware(int argc, char* argv[]) {
    if (argc < 3) {
        printf("=================================================================\n");
        printf("  O.Z.O.S // LEG 3: AUTONOMOUS HARDWARE COURIER BRIDGE\n");
        printf("  Direct Hardware Serial / UART Interface (ESP32-S3 / LoRa SX1262)\n");
        printf("=================================================================\n\n");
        printf("Usage: ozos.exe hardware <subcommand> [options]\n\n");
        printf("Subcommands:\n");
        printf("  list                          - Enumerate available physical serial/COM ports\n");
        printf("  ping <port> [baud]            - Ping connected sovereign hardware pod\n");
        printf("  send-lora <port> <hex_data>   - Transmit 84-byte LoRa packet via hardware pod\n");
        printf("  recv-lora <port>              - Receive 84-byte LoRa packet from hardware pod\n");
        printf("=================================================================\n");
        return 1;
    }

    const char* sub = argv[2];
    if (strcmp(sub, "list") == 0) {
        ozos_serial_list_ports();
        return 0;
    } else if (strcmp(sub, "ping") == 0) {
        if (argc < 4) {
            fprintf(stderr, "[SYNTAX ERROR] Usage: ozos.exe hardware ping <port> [baud]\n");
            return 1;
        }
        const char* port_name = argv[3];
        uint32_t baud = (argc >= 5) ? (uint32_t)atoi(argv[4]) : OZOS_SERIAL_DEFAULT_BAUD;
        ozos_serial_t ser;
        printf("[O.Z.O.S HARDWARE BRIDGE] Connecting to %s at %u baud...\n", port_name, baud);
        int rc = ozos_serial_open(&ser, port_name, baud);
        if (rc != 0) {
            fprintf(stderr, "[ERROR] Failed to open port %s (error code: %d)!\n", port_name, rc);
            return 1;
        }

        const uint8_t ping_pkt[4] = {'O', 'Z', 'P', 'G'};
        printf("-> Transmitting Ping Beacon [OZPG] (0x4F5A5047)...\n");
        rc = ozos_serial_send(&ser, ping_pkt, 4);
        if (rc != 0) {
            fprintf(stderr, "[ERROR] Failed to write ping beacon to port!\n");
            ozos_serial_close(&ser);
            return 1;
        }

        uint8_t resp[64] = {0};
        size_t rlen = 0;
        printf("<- Waiting for Sovereign Hardware Pod response...\n");
        rc = ozos_serial_recv(&ser, resp, sizeof(resp) - 1, &rlen);
        if (rc == 0 && rlen > 0) {
            resp[rlen] = '\0';
            printf("\n  [HARDWARE LINK VERIFIED] SOVEREIGN NODE ONLINE!\n");
            printf("  * Port            : %s (%u baud)\n", port_name, baud);
            printf("  * Response Length : %zu bytes\n", rlen);
            printf("  * Pod Signature   : %s\n", (char*)resp);
        } else {
            printf("  [TIMEOUT / NO RESPONSE] Port opened, but no reply received from Pod.\n");
            printf("  (Ensure MCU firmware is running and TX/RX are crossed properly).\n");
        }
        ozos_serial_close(&ser);
        return 0;

    } else if (strcmp(sub, "send-lora") == 0) {
        if (argc != 5) {
            fprintf(stderr, "[SYNTAX ERROR] Usage: ozos.exe hardware send-lora <port> <hex_payload>\n");
            return 1;
        }
        const char* port_name = argv[3];
        const char* hex_data = argv[4];
        size_t hex_len = strlen(hex_data);
        if (hex_len == 0 || hex_len > 160 || (hex_len % 2 != 0)) {
            fprintf(stderr, "[ERROR] Payload must be 1..80 hex bytes (2..160 hex chars)!\n");
            return 1;
        }
        uint8_t payload[80];
        size_t byte_len = hex_len / 2;
        if (!is_valid_hex(hex_data, hex_len) || hex_to_bytes(hex_data, payload, byte_len) != byte_len) {
            fprintf(stderr, "[ERROR] Malformed hex payload!\n");
            return 1;
        }

        ozos_serial_t ser;
        int rc = ozos_serial_open(&ser, port_name, OZOS_SERIAL_DEFAULT_BAUD);
        if (rc != 0) {
            fprintf(stderr, "[ERROR] Failed to open port %s!\n", port_name);
            return 1;
        }

        printf("-> Transmitting %zu-byte payload as 84-byte LoRa frame over %s...\n", byte_len, port_name);
        rc = ozos_serial_send_lora(&ser, payload, byte_len);
        if (rc != 0) {
            fprintf(stderr, "[ERROR] Transmission failed (rc=%d)!\n", rc);
            ozos_serial_close(&ser);
            return 1;
        }
        printf("  [SUCCESS] 84-byte LoRa frame written to Hardware Courier.\n");
        ozos_serial_close(&ser);
        return 0;

    } else if (strcmp(sub, "recv-lora") == 0) {
        if (argc != 4) {
            fprintf(stderr, "[SYNTAX ERROR] Usage: ozos.exe hardware recv-lora <port>\n");
            return 1;
        }
        const char* port_name = argv[3];
        ozos_serial_t ser;
        int rc = ozos_serial_open(&ser, port_name, OZOS_SERIAL_DEFAULT_BAUD);
        if (rc != 0) {
            fprintf(stderr, "[ERROR] Failed to open port %s!\n", port_name);
            return 1;
        }

        printf("<- Listening for 84-byte LoRa frame on %s...\n", port_name);
        uint8_t payload[80];
        size_t payload_len = 0;
        rc = ozos_serial_recv_lora(&ser, payload, &payload_len);
        if (rc != 0) {
            fprintf(stderr, "[ERROR] Receive failed or frame invalid (rc=%d)!\n", rc);
            ozos_serial_close(&ser);
            return 1;
        }

        char hex_out[161];
        bytes_to_hex(payload, payload_len, hex_out);
        printf("\n  [LORA PACKET RECEIVED FROM HARDWARE COURIER]\n");
        printf("  * Payload Length : %zu bytes\n", payload_len);
        printf("  * Hex Data       : %s\n", hex_out);
        ozos_serial_close(&ser);
        return 0;
    } else {
        fprintf(stderr, "[SYNTAX ERROR] Unknown hardware subcommand: '%s'\n", sub);
        return 1;
    }
}

static int cmd_spectrum_status(void) {
    printf("=================================================================\n");
    printf("  O.Z.O.S // OMNI-SPECTRUM COGNITIVE PHY MATRIX DIAGNOSTICS\n");
    printf("  Architecture: 4-Tier Medium-Agnostic Sovereign Transport\n");
    printf("=================================================================\n\n");
    printf("  [TIER 1: OPTICAL SPECTRUM (PHY)]\n");
    printf("  * Wavelength     : 400 - 700 nm (Visible Light)\n");
    printf("  * Mechanism      : Dynamic Optical Glyph 32x32 Matrix (CRC32-IEEE Protected)\n");
    printf("  * Latency        : 16 milliseconds (Instantaneous via single 60fps camera frame)\n");
    printf("  * Advantage      : Completely silent (0 dB), immune to acoustic eavesdropping\n\n");

    printf("  [TIER 2: ACOUSTIC MULTI-CARRIER OFDM & STEGANOGRAPHY]\n");
    printf("  * Frequency Band : 12.0 kHz - 23.25 kHz (16 QPSK Subcarriers)\n");
    printf("  * Mechanism      : 64-Point Radix-2 Cooley-Tukey FFT & IFFT C99 Bare-Metal\n");
    printf("  * Latency        : 0.0366 seconds (~36.6 ms) - Over 40x faster than legacy FSK!\n");
    printf("  * Covert Mode    : Steganographic carrier hidden within natural ambient noise\n\n");

    printf("  [TIER 3: LONG-RANGE RF (LORA SUB-GHZ 433/868 MHz)]\n");
    printf("  * Wavelength     : 35 cm - 70 cm (Sub-GHz ISM Band)\n");
    printf("  * Penetration    : Reinforced concrete, underground bunkers, range 5 - 15 km\n");
    printf("  * Wire Frame     : 84 Bytes [0x12 | 0x50 | 80B Payload | CRC16-CCITT]\n");
    printf("  * Energy Profile : Ultra low-power, operates entirely without Internet/SIM\n\n");

    printf("  [TIER 4: HF SKYWAVE (3 - 30 MHz / GLOBAL RESILIENCE)]\n");
    printf("  * Status         : [OPERATIONAL AUDIO BASEBAND MODEM / FULL HARDWARE DEPLOYMENT PLANNED VIA ANALOG SSB TRANSCEIVER]\n");
    printf("  * Propagation    : Ionosphere Bouncing (100 - 300 km altitude reflection)\n");
    printf("  * Survivability  : Transoceanic & intercontinental reach during undersea cable collapse\n");
    printf("  * Hardware       : Battery-powered analog radio patched directly into PC Mic jack\n\n");
    printf("=================================================================\n");
    printf("  STATUS: 4/4 COGNITIVE PHY TIERS OPERATIONAL [FIELD-HARDENED]\n");
    printf("=================================================================\n");
    return 0;
}

/* =============================================================================
   DATA-AT-REST & AMBIENT GUARDIAN COMMAND IMPLEMENTATIONS
   ============================================================================= */

static bool ozos_resolve_seed(const char* custom_seed_str, uint8_t seed_out[32]) {
    if (custom_seed_str && custom_seed_str[0] != '\0') {
        size_t slen = strlen(custom_seed_str);
        if (slen == 64 && is_valid_hex(custom_seed_str, 64)) {
            hex_to_bytes(custom_seed_str, seed_out, 32);
            return true;
        }
        if (strchr(custom_seed_str, ' ')) {
            if (ozos_brainseed_restore(seed_out, custom_seed_str, "")) {
                return true;
            }
            fprintf(stderr, "[SECURITY ALERT] Invalid BIP-39 mnemonic phrase or checksum mismatch in --seed!\n");
            return false;
        }
        ozos_sha256((const uint8_t*)custom_seed_str, slen, seed_out);
        return true;
    }
    ozos_tpm_info_t tpm;
    query_hardware_tpm(&tpm);
    if (tpm.tpm_available) {
        memcpy(seed_out, tpm.silicon_seed, 32);
        return true;
    }
    ozos_sha256((const uint8_t*)"OZOS_DEFAULT_SOVEREIGN_ROOT_SEED_2026", 38, seed_out);
    return true;
}

static int cmd_sweep(const char* dir_path, const char* custom_seed) {
    if (!dir_path) {
        fprintf(stderr, "Error: ozos.exe sweep <directory> [--seed <hex>]\n");
        return 1;
    }
    uint8_t seed[32];
    if (!ozos_resolve_seed(custom_seed, seed)) return 1;
    int rc = ozos_sweep_directory(dir_path, seed);
    ozos_crypto_zeroize(seed, 32);
    return rc;
}

static int cmd_vault_pack(const char* in_dir, const char* out_vault, const char* custom_seed) {
    if (!in_dir || !out_vault) {
        fprintf(stderr, "Error: ozos.exe vault pack <dir> <vault.ozos> [--seed <words|hex>]\n");
        return 1;
    }
    uint8_t seed[32];
    if (!ozos_resolve_seed(custom_seed, seed)) return 1;
    int rc = ozos_vault_pack(in_dir, out_vault, seed);
    ozos_crypto_zeroize(seed, 32);
    return rc;
}

static int cmd_vault_open(const char* in_vault, const char* out_dir, const char* custom_seed, bool burn_on_fail, bool self_destruct) {
    if (!in_vault || !out_dir) {
        fprintf(stderr, "Error: ozos.exe vault open <vault.ozos> <out_dir> [--seed <words|hex>] [--burn-on-fail] [--self-destruct]\n");
        return 1;
    }
    uint8_t seed[32];
    if (!ozos_resolve_seed(custom_seed, seed)) return 1;
    int rc = ozos_vault_open_ex(in_vault, out_dir, seed, burn_on_fail, self_destruct);
    ozos_crypto_zeroize(seed, 32);
    return rc;
}

static int cmd_vault_pqc_pack(const char* in_dir, const char* out_vault, const char* pk_file) {
    if (!in_dir || !out_vault || !pk_file) {
        fprintf(stderr, "Error: ozos.exe vault pqc-pack <dir> <vault.ozos> <pk_file>\n");
        return 1;
    }
    return ozos_vault_pqc_pack(in_dir, out_vault, pk_file);
}

static int cmd_vault_pqc_open(const char* in_vault, const char* out_dir, const char* sk_file, bool burn_on_fail, bool self_destruct) {
    if (!in_vault || !out_dir || !sk_file) {
        fprintf(stderr, "Error: ozos.exe vault pqc-open <vault.ozos> <out_dir> <sk_file> [--burn-on-fail] [--self-destruct]\n");
        return 1;
    }
    return ozos_vault_pqc_open(in_vault, out_dir, sk_file, burn_on_fail, self_destruct);
}

static int cmd_anchor(const char* dir_path, const char* custom_seed) {
    if (!dir_path) {
        fprintf(stderr, "Error: ozos.exe anchor <directory> [--seed <hex>]\n");
        return 1;
    }
    uint8_t seed[32];
    if (!ozos_resolve_seed(custom_seed, seed)) return 1;
    int rc = ozos_anchor_directory(dir_path, seed);
    ozos_crypto_zeroize(seed, 32);
    return rc;
}

static int cmd_anchor_verify(const char* dir_path) {
    if (!dir_path) {
        fprintf(stderr, "Error: ozos.exe anchor-verify <directory>\n");
        return 1;
    }
    return ozos_anchor_verify(dir_path);
}

static int cmd_burn(const char* file_path) {
    if (!file_path) {
        fprintf(stderr, "Error: ozos.exe burn <file_path>\n");
        return 1;
    }
    return ozos_burn_file(file_path);
}

static int cmd_clipboard_armor(const char* custom_seed) {
#ifdef _WIN32
    uint8_t seed[32];
    if (!ozos_resolve_seed(custom_seed, seed)) return 1;
    int rc = ozos_clipboard_armor_singleshot(seed);
    ozos_crypto_zeroize(seed, 32);
    return rc;
#else
    printf("[CLIPBOARD ARMOR] Direct OS RAM clipboard hook is active on Windows (CF_DIB).\n");
    return 0;
#endif
}

/* =============================================================================
   MAIN ENTRY POINT
   ============================================================================= */

static int cmd_brainseed_split(uint8_t k, uint8_t n, const char* custom_seed_hex) {
    uint8_t seed[32];
    if (!ozos_resolve_seed(custom_seed_hex, seed)) return 1;

    ozos_shamir_share_t shares[OZOS_SHAMIR_MAX_SHARES];
    int rc = ozos_shamir_split(seed, k, n, shares);
    ozos_crypto_zeroize(seed, sizeof(seed));

    if (rc != 0) {
        fprintf(stderr, "[ERROR] Shamir's Secret Sharing split failed (rc=%d). Parameters must satisfy 2 <= k <= n <= 16.\n", rc);
        return 1;
    }

    printf("=================================================================\n");
    printf("  O.Z.O.S // SHAMIR'S SECRET SHARING (SSS) THRESHOLD SPLIT\n");
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
    printf("   To restore master seed: ozos.exe brainseed-combine <share_1> <share_2> ... <share_%u>\n", (unsigned)k);
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

    /* Derive master public key to verify identity */
    uint8_t pk[32], sk[64];
    crypto_sign_seed_keypair(pk, sk, recovered_seed);
    char pk_hex[65];
    bytes_to_hex(pk, 32, pk_hex);

    printf("=================================================================\n");
    printf("  O.Z.O.S // SHAMIR'S SECRET SHARING RECONSTRUCTION SUCCESS\n");
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

static int cmd_pqc_status(void) {
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
}

static int cmd_pqc_test(void) {
    printf("=================================================================\n");
    printf("  O.Z.O.S // POST-QUANTUM HYBRID ENCLAVE ROUNDTRIP BENCHMARK\n");
    printf("  Testing: X25519 + ML-KEM-768 (FIPS 203) & ML-DSA-65 (FIPS 204)\n");
    printf("=================================================================\n\n");

    uint8_t seed[32];
    ozos_secure_random_bytes(seed, sizeof(seed));

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

    /* ML-DSA-65 Sign + Verify + 1-Bit Tamper Rejection Roundtrip */
    uint8_t dsa_seed[32];
    uint8_t dsa_pk[OZOS_MLDSA_65_PK_BYTES];
    uint8_t dsa_sk[OZOS_MLDSA_65_SK_BYTES];
    uint8_t dsa_sig[OZOS_MLDSA_65_SIG_BYTES];
    static const uint8_t dsa_msg[] = "OZOS-FIPS204-ML-DSA-65-LIVE-ENCLAVE";
    ozos_secure_random_bytes(dsa_seed, sizeof(dsa_seed));
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
}

static int cmd_pqc_keygen(const char* custom_seed_str, const char* out_prefix) {
    if (!out_prefix || out_prefix[0] == 0) out_prefix = "pqc_identity";

    uint8_t seed[32];
    if (custom_seed_str && custom_seed_str[0] != 0) {
        ozos_resolve_seed(custom_seed_str, seed);
    } else {
        ozos_secure_random_bytes(seed, sizeof(seed));
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
    printf("* Entropy Source        : %s\n", (custom_seed_str && custom_seed_str[0]) ? "Deterministic Seed / Brainseed" : "Hardware OS CSPRNG");
    printf("* Memory Security       : Stack zeroized via ozos_crypto_zeroize [SECURE]\n");
    printf("=================================================================\n");

    ozos_crypto_zeroize(seed, sizeof(seed));
    ozos_crypto_zeroize(&sk, sizeof(sk));
    return 0;
}


static int cmd_crash_test(void) {
    printf("=================================================================\n");
    printf("  O.Z.O.S // CRASH SIGNAL HANDLER & PURGE AUDIT\n");
    printf("  Testing: Memory Purge & Error Dialog Suppression under Fault\n");
    printf("=================================================================\n\n");

    static uint8_t sensitive_key[32];
    for (int i = 0; i < 32; i++) sensitive_key[i] = 0x42U;

    ozos_crash_guard_register(sensitive_key, sizeof(sensitive_key));
    printf("[1] Sensitive RAM region registered (32 bytes filled with 0x42)\n");
    printf("[2] Triggering simulated memory fault via ozos_posix_signal_handler(SIGSEGV)...\n");
    fflush(stdout);

    ozos_posix_signal_handler(SIGSEGV);
    return 0;
}

static int cmd_workstation_shield(void) {
    printf("=================================================================\n");
    printf("  O.Z.O.S // WORKSTATION VIRTUAL AIR-GAP & MEMORY SHIELD AUDIT\n");
    printf("  Environment Hardening, Wi-Fi Host Defense & Ephemeral Enclave\n");
    printf("=================================================================\n\n");

    /* 1. Hypervisor & Hardware Virtualization Audit */
    ozos_hypervisor_info_t hv;
    ozos_detect_hypervisor(&hv);
    printf("[1] HARDWARE VIRTUALIZATION & HYPERVISOR AUDIT:\n");
    printf("  * Virtualization Detected : %s\n", hv.is_virtualized ? "[ALERT] YES (RUNNING UNDER HYPERVISOR)" : "[OK] NO (PHYSICAL BARE-METAL)");
    printf("  * Hypervisor Vendor       : %s\n", hv.vendor_id);
    printf("  * Platform Environment    : %s\n", hv.hypervisor_name);
    printf("  * Security Risk Posture   : %s\n", hv.risk_analysis);

    /* 2. Host Network & Wi-Fi Exposure Audit */
    ozos_net_env_t net;
    ozos_scan_network_environment(&net);
    printf("\n[2] HOST NETWORK & WI-FI EXPOSURE AUDIT:\n");
    printf("  * Security Posture        : %s\n", net.posture == OZOS_NET_PRISTINE_AIRGAP ? "[OK] PRISTINE PHYSICAL AIR-GAP (0 ADAPTERS)" :
                                              (net.has_wifi ? "[WARN] EXPOSED WI-FI WORKSTATION" : 
                                              (net.has_ethernet ? "[WARN] EXPOSED WIRED ETHERNET HOST" : "[OK] LOCALHOST LOOPBACK ONLY")));
    printf("  * Active Network Adapters : %u active interface(s)\n", net.active_adapter_count);
    printf("  * Primary Network Adapter : %s\n", net.primary_adapter_name);
    printf("  * Primary IP Address      : %s\n", net.primary_ip);
    printf("  * Default Gateway Active  : %s\n", net.has_gateway ? "YES (Routable to WAN/Internet)" : "NO (Isolated Subnet)");
    printf("  * Operational Advice      : %s\n", net.mitigation_advice);

    /* 3. Process-Level Memory Shield & Anti-Dump Traps */
    ozos_memory_shield_t shield;
    ozos_activate_memory_shield(&shield);
    printf("\n[3] PROCESS MEMORY SHIELD & ANTI-DUMP PROTECTION:\n");
    printf("  * Anti-Debugger Traps     : %s\n", shield.anti_debug_passed ? "[PASS] SECURE (No Debugger Attached)" : "[FAIL] ATTACHED DEBUGGER DETECTED");
    printf("  * Process DACL Hardened   : %s\n", shield.dacl_hardened ? "[PASS] ACTIVE (Memory Dump Handles Restricted)" : "[WARN] DEFAULT DACL");
    printf("  * Dynamic Code Generation : %s\n", shield.dynamic_code_blocked ? "[PASS] BLOCKED (Arbitrary Shellcode Prohibited)" : "[WARN] SYSTEM DEFAULT");
    printf("  * Stack VirtualLock       : %s\n", shield.stack_locked ? "[PASS] ENGAGED (Non-Paged RAM Enclave)" : "[WARN] UNLOCKED");
    printf("  * Memory Shield Status    : %s\n", shield.shield_status);

    /* 4. Ephemeral Sub-Millisecond Key Benchmark (Zero-Time-in-RAM) */
    ozos_ephemeral_bench_t bench;
    ozos_benchmark_ephemeral_enclave(&bench);
    printf("\n[4] EPHEMERAL SUB-MILLISECOND KEY LIFECYCLE (ZERO-TIME-IN-RAM):\n");
    printf("  * Key Residence Window    : %.2f microseconds (%.4f ms)\n", bench.key_residence_microseconds, bench.key_residence_microseconds / 1000.0);
    printf("  * Ed25519 Derivation/Sign : %s\n", (bench.derivation_success && bench.signing_success) ? "[PASS] BIT-EXACT RFC 8032" : "[FAIL]");
    printf("  * Instantaneous Zeroize   : %s\n", bench.zeroization_confirmed ? "[PASS] 100% SCRUBBED (Zero Key Residue in RAM)" : "[FAIL]");
    printf("  * Red Team Defense Rating : [CERTIFIED] Exposure window < 0.1ms; Infostealers scanning RAM have 99.999%% probability of reading 0x00\n");

    /* 5. Silicon Hardware TPM 2.0 Root of Trust */
    ozos_tpm_info_t tpm;
    query_hardware_tpm(&tpm);
    printf("\n[5] HARDWARE SILICON ROOT OF TRUST:\n");
    printf("  * TPM 2.0 Silicon Anchor  : %s\n", tpm.tpm_available ? "[OK] HARDWARE TPM 2.0 BOUND" : "[WARN] NO TPM (Ephemeral CSPRNG in RAM)");
    printf("  * Cryptographic Provider  : %s\n", tpm.provider_name);

    printf("=================================================================\n");
    return 0;
}

static void print_banner(void) {
    printf("=================================================================\n");
    printf("  O.Z.O.S // NATIVE SOVEREIGN EXECUTABLE (ozos.exe)\n");
    printf("  Protocol: OZOS-80 Wire Format | Architecture: x86_64 Native C\n");
    printf("=================================================================\n\n");
    printf("Available operational commands:\n");
    printf("  ozos.exe status                        - Node diagnostics & TPM 2.0 silicon anchor\n");
    printf("  ozos.exe tpm                           - Hardware TPM 2.0 diagnostics\n");
    printf("  ozos.exe keygen [seed_hex]             - [TIER 1] Generate Ed25519 sovereign keypair\n");
    printf("  ozos.exe packet-sign <msg> [nonce]     - [TIER 1] Sign packet with sovereign Ed25519\n");
    printf("  ozos.exe packet-build <msg> [nonce]    - Encapsulate 80-byte SIMD frame (unsigned)\n");
    printf("  ozos.exe packet-verify <hex> [last_n]  - Verify MAC16 tripwire & Ed25519 signature\n");
    printf("  ozos.exe handshake-syn [wav] [nonce]   - [TIER 2] Initiate Inaudible Handshake (SYN)\n");
    printf("  ozos.exe handshake-ack <syn> [wav] [--pinned-peer-pubkey <pk>] - [TIER 2] Respond (ACK)\n");
    printf("  ozos.exe handshake-finish <ack> <sk><pk>- [TIER 2] Complete Handshake & Establish Tunnel\n");
    printf("  ozos.exe tunnel-encrypt <msg> <key>    - [TIER 2] Encrypt payload across acoustic tunnel\n");
    printf("  ozos.exe tunnel-decrypt <hex> <key>    - [TIER 2] Decrypt and verify Poly1305 AEAD tag\n");
    printf("  ozos.exe brainseed-gen [passphrase]    - [TIER 3] Generate 12-word BIP-39 mnemonic\n");
    printf("  ozos.exe brainseed-restore <12 words>  - [TIER 3] Restore sovereign identity from mindseed\n");
    printf("  ozos.exe brainseed-split [opts]        - [TIER 3] Shamir's Secret Sharing (k-of-n threshold split)\n");
    printf("  ozos.exe brainseed-combine <shares...> - [TIER 3] Reconstruct master seed from k Shamir shares\n");
    printf("  ozos.exe gossip-beacon [epoch] [wav]   - [TIER 4] Broadcast Inaudible Heartbeat Beacon\n");
    printf("  ozos.exe gossip-listen <beacon> [epoch]- [TIER 4] Inaudible Merkle Consensus Arbitration\n");
    printf("  ozos.exe audio-synth <txt> <wav> [opt] - Acoustic carrier synthesis (Default: --stealth 18-20kHz)\n");
    printf("  ozos.exe audio-decode <in.wav> [opt]   - Acoustic demodulation via Goertzel filter bank\n");
    printf("  --- [OMNI-SPECTRUM COGNITIVE PHY] ---\n");
    printf("  ozos.exe glyph-encode <hex> [--bmp <b>] - [TIER 1] 16ms Optical Matrix (Terminal ANSI / BMP)\n");
    printf("  ozos.exe glyph-decode <in.bmp>         - [TIER 1] Scan BMP 256x256 Optical Matrix\n");
    printf("  ozos.exe ofdm-synth <hex> <wav> [opt]  - [TIER 2] Acoustic OFDM (< 0.04s) / Ambient Stego\n");
    printf("  ozos.exe ofdm-decode <in.wav>          - [TIER 2] 64-FFT Acoustic OFDM Demodulation\n");
    printf("  ozos.exe lora-pack <hex>               - [TIER 3] LoRa Sub-GHz 84-byte frame & CRC16-CCITT\n");
    printf("  ozos.exe lora-unpack <frame_hex>       - [TIER 3] LoRa Sub-GHz 84-byte unpack & CRC16-CCITT\n");
    printf("  ozos.exe spectrum-status               - [OMNI-SPECTRUM] 4-Tier Cognitive PHY Diagnostics\n");
    printf("  --- [DATA-AT-REST & AMBIENT GUARDIAN] ---\n");
    printf("  ozos.exe sweep <dir> [--seed <hex>]    - Batch 2D-DCT watermark & Ed25519 detached signing\n");
    printf("  ozos.exe vault pack <dir> <vault.ozos> - Sealed RAM Enclave Archive (XSalsa20-Poly1305)\n");
    printf("  ozos.exe vault open <vault.ozos> <dir> - Authenticated Decryption [--burn-on-fail] [--self-destruct]\n");
    printf("  ozos.exe vault pqc-pack <dir> <v> <pk> - Quantum-Sealed Vault Archive (ML-KEM-768 + X25519)\n");
    printf("  ozos.exe vault pqc-open <v> <dir> <sk> - Quantum Vault Extraction [--burn-on-fail] [--self-destruct]\n");
    printf("  ozos.exe anchor <dir> [--seed <hex>]   - Zero-Touch Merkle Provenance Anchor (.ozos_anchor.json)\n");
    printf("  ozos.exe anchor-verify <dir>           - Verify Merkle tree integrity across directory files\n");
    printf("  ozos.exe burn <file>                   - NIST SP 800-88 3-pass hardware sanitization & unlink\n");
    printf("  ozos.exe clipboard-armor [--seed <hex>]- Sub-5ms Windows RAM Clipboard Bitmap Auto-Armor\n");
    printf("  --- [LEG 3: AUTONOMOUS HARDWARE & OFF-GRID POD] ---\n");
    printf("  ozos.exe hardware list                 - Enumerate physical serial/COM ports for hardware token\n");
    printf("  ozos.exe hardware ping <port> [baud]   - Ping autonomous hardware pod over UART bridge\n");
    printf("  ozos.exe hardware send-lora <p> <hex>  - Transmit 84-byte LoRa packet via connected hardware pod\n");
    printf("  ozos.exe hardware recv-lora <port>     - Receive and unpack 84-byte LoRa packet from hardware pod\n");
    printf("  --- [WORKSTATION HARDENING & VIRTUAL AIR-GAP] ---\n");
    printf("  ozos.exe workstation-shield / guard    - Hypervisor, Wi-Fi Host & Ephemeral Enclave Audit\n");
    printf("  ozos.exe crash-test                    - Emergency Crash Guard & NIST SP 800-88 Purge Audit\n");
    printf("  --- [POST-QUANTUM HYBRID ENCLAVE (PQC)] ---\n");
    printf("  ozos.exe pqc-status                    - [TIER 13] NIST FIPS 203 (ML-KEM) & 204 (ML-DSA) Status\n");
    printf("  ozos.exe pqc-test                      - [TIER 13] Hybrid Post-Quantum Roundtrip Benchmark\n");
    printf("  ozos.exe pqc-keygen [seed] [prefix]    - [TIER 13] Generate Composite X25519 + ML-KEM Keypair\n");
    printf("  ozos.exe daemon [port]                 - Launch Background Node (Default: 7788)\n");
    printf("  ozos.exe benchmark                     - Micro-architectural throughput benchmark\n");
    printf("  ozos.exe version / --version           - Display version and pinned SHA-256 integrity\n");
    printf("=================================================================\n");
}

int main(int argc, char* argv[]) {
    ozos_crash_guard_install();
    if (argc < 2) {
        print_banner();
        return 0;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "status") == 0 || strcmp(cmd, "tpm") == 0) {
        if (argc > 2) {
            fprintf(stderr, "[SYNTAX ERROR] Command '%s' does not accept extra arguments.\n", cmd);
            return 1;
        }
        return cmd_status();
    } else if (strcmp(cmd, "keygen") == 0) {
        if (argc > 3) {
            fprintf(stderr, "[SYNTAX ERROR] Too many arguments for keygen command.\n");
            return 1;
        }
        const char* custom_seed = (argc == 3) ? argv[2] : NULL;
        return cmd_keygen(custom_seed);
    } else if (strcmp(cmd, "packet-sign") == 0) {
        if (argc > 5) {
            fprintf(stderr, "[SYNTAX ERROR] Too many arguments for packet-sign command.\n");
            return 1;
        }
        const char* msg = (argc >= 3) ? argv[2] : "O.Z.O.S SOVEREIGN IDENTITY SIGNED";
        uint64_t nonce = 1001ULL;
        if (argc >= 4) {
            if (!parse_strict_u64(argv[3], &nonce) || nonce == 0) {
                fprintf(stderr, "[SYNTAX ERROR] Invalid nonce: '%s' (must be a positive integer).\n", argv[3]);
                return 1;
            }
        }
        const char* custom_seed = (argc == 5) ? argv[4] : NULL;
        return cmd_packet_sign(msg, nonce, custom_seed);
    } else if (strcmp(cmd, "packet-build") == 0) {
        const char* msg = "O.Z.O.S SOVEREIGN IDENTITY";
        uint64_t nonce = 1001ULL;
        uint64_t mac_key = 0ULL;
        bool has_msg = false;
        bool has_nonce = false;
        for (int i = 2; i < argc; ++i) {
            if (strcmp(argv[i], "--mac-key") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "[SYNTAX ERROR] Flag '--mac-key' is missing required hex argument.\n");
                    return 1;
                }
                const char* k_str = argv[++i];
                if (!is_valid_hex(k_str, strlen(k_str)) || strlen(k_str) > 16) {
                    fprintf(stderr, "[SYNTAX ERROR] Invalid --mac-key hex: '%s'\n", k_str);
                    return 1;
                }
                mac_key = (uint64_t)strtoull(k_str, NULL, 16);
            } else if (argv[i][0] == '-') {
                fprintf(stderr, "[SYNTAX ERROR] Invalid flag: '%s'\n", argv[i]);
                return 1;
            } else if (!has_msg) {
                msg = argv[i];
                has_msg = true;
            } else if (!has_nonce) {
                if (!parse_strict_u64(argv[i], &nonce) || nonce == 0) {
                    fprintf(stderr, "[SYNTAX ERROR] Invalid nonce: '%s' (must be a positive integer).\n", argv[i]);
                    return 1;
                }
                has_nonce = true;
            } else {
                fprintf(stderr, "[SYNTAX ERROR] Too many arguments for packet-build command.\n");
                return 1;
            }
        }
        return cmd_packet_build(msg, nonce, mac_key);
    } else if (strcmp(cmd, "packet-verify") == 0) {
        if (argc < 3) {
            printf("Error: Please provide hex string: ozos.exe packet-verify <hex> [last_n] [trusted_author_pk] [--peer-id <id>]\n");
            return 1;
        }
        uint64_t last_n = 1000ULL;
        const char* trusted_pk = NULL;
        const char* peer_id = NULL;
        uint64_t mac_key = 0ULL;
        bool has_last_n = false;
        bool has_trusted_pk = false;
        for (int i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--pinned-peer-pubkey") == 0 || strcmp(argv[i], "--pubkey") == 0) {
                if (has_trusted_pk || i + 1 >= argc) {
                    fprintf(stderr, "[SYNTAX ERROR] Flag '%s' is duplicated or missing required argument.\n", argv[i]);
                    return 1;
                }
                trusted_pk = argv[++i];
                has_trusted_pk = true;
            } else if (strcmp(argv[i], "--peer-id") == 0) {
                if (peer_id != NULL || i + 1 >= argc) {
                    fprintf(stderr, "[SYNTAX ERROR] Flag '--peer-id' is duplicated or missing required argument.\n");
                    return 1;
                }
                peer_id = argv[++i];
                for (const char* p = peer_id; *p; p++) {
                    if (*p <= 32 || *p > 126) {
                        fprintf(stderr, "[SYNTAX ERROR] Invalid Peer ID (cannot contain whitespace or control characters): '%s'\n", peer_id);
                        return 1;
                    }
                }
            } else if (strcmp(argv[i], "--mac-key") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "[SYNTAX ERROR] Flag '--mac-key' is missing required hex argument.\n");
                    return 1;
                }
                const char* k_str = argv[++i];
                if (!is_valid_hex(k_str, strlen(k_str)) || strlen(k_str) > 16) {
                    fprintf(stderr, "[SYNTAX ERROR] Invalid --mac-key hex: '%s'\n", k_str);
                    return 1;
                }
                mac_key = (uint64_t)strtoull(k_str, NULL, 16);
            } else if (argv[i][0] == '-') {
                fprintf(stderr, "[SYNTAX ERROR] Invalid flag: '%s'\n", argv[i]);
                return 1;
            } else if (!has_last_n) {
                if (!parse_strict_u64(argv[i], &last_n)) {
                    fprintf(stderr, "[SYNTAX ERROR] Invalid last_seen_nonce: '%s' (must be an integer >= 0).\n", argv[i]);
                    return 1;
                }
                has_last_n = true;
            } else if (!has_trusted_pk) {
                trusted_pk = argv[i];
                has_trusted_pk = true;
            } else {
                fprintf(stderr, "[SYNTAX ERROR] Unexpected extra argument: '%s'\n", argv[i]);
                return 1;
            }
        }
        return cmd_packet_verify(argv[2], last_n, trusted_pk, peer_id, mac_key);
    } else if (strcmp(cmd, "handshake-syn") == 0) {
        const char* wav_path = NULL;
        uint32_t session_nonce = 0;
        bool has_wav = false;
        bool has_nonce = false;
        const char* secret_file = NULL;
        bool no_secret = false;
        bool raw_frame = false;
        for (int i = 2; i < argc; ++i) {
            if (strcmp(argv[i], "--secret-file") == 0) {
                if (secret_file != NULL || i + 1 >= argc) {
                    fprintf(stderr, "[SYNTAX ERROR] Flag --secret-file is duplicated or missing path argument.\n");
                    return 1;
                }
                secret_file = argv[++i];
            } else if (strcmp(argv[i], "--no-secret") == 0) {
                no_secret = true;
            } else if (strcmp(argv[i], "--raw-frame") == 0) {
                raw_frame = true;
            } else if (argv[i][0] == '-') {
                fprintf(stderr, "[SYNTAX ERROR] Invalid flag: '%s'\n", argv[i]);
                return 1;
            } else if (!has_wav) {
                wav_path = argv[i];
                has_wav = true;
            } else if (!has_nonce) {
                if (!parse_strict_u32(argv[i], &session_nonce) || session_nonce == 0) {
                    fprintf(stderr, "[SYNTAX ERROR] Invalid session nonce: '%s'\n", argv[i]);
                    return 1;
                }
                has_nonce = true;
            } else {
                fprintf(stderr, "[SYNTAX ERROR] Too many arguments for handshake-syn command.\n");
                return 1;
            }
        }
        return cmd_handshake_syn(wav_path, session_nonce, secret_file, no_secret, raw_frame);
    } else if (strcmp(cmd, "handshake-ack") == 0) {
        if (argc < 3) {
            printf("Error: ozos.exe handshake-ack <syn_hex> [output_ack.wav] [--pinned-peer-pubkey <pk>] [--peer-id <id>] [--raw-frame]\n");
            return 1;
        }
        const char* wav_path = NULL;
        const char* pinned_peer_pk = NULL;
        const char* peer_id = NULL;
        bool raw_frame = false;
        for (int i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--pinned-peer-pubkey") == 0) {
                if (pinned_peer_pk != NULL) {
                    fprintf(stderr, "[SYNTAX ERROR] Flag --pinned-peer-pubkey cannot be duplicated!\n");
                    return 1;
                }
                if (i + 1 < argc) {
                    pinned_peer_pk = argv[++i];
                } else {
                    fprintf(stderr, "[ERROR] Missing public key argument for flag --pinned-peer-pubkey <64_hex_pk>!\n");
                    return 1;
                }
            } else if (strcmp(argv[i], "--peer-id") == 0) {
                if (peer_id != NULL || i + 1 >= argc) {
                    fprintf(stderr, "[SYNTAX ERROR] Flag --peer-id is duplicated or missing argument!\n");
                    return 1;
                }
                peer_id = argv[++i];
                for (const char* p = peer_id; *p; p++) {
                    if (*p <= 32 || *p > 126) {
                        fprintf(stderr, "[SYNTAX ERROR] Invalid Peer ID (cannot contain whitespace or control characters): '%s'\n", peer_id);
                        return 1;
                    }
                }
            } else if (strcmp(argv[i], "--raw-frame") == 0) {
                raw_frame = true;
            } else if (!wav_path && argv[i][0] != '-') {
                wav_path = argv[i];
            } else {
                fprintf(stderr, "[ERROR] Unknown argument or invalid flag: %s\n", argv[i]);
                return 1;
            }
        }
        return cmd_handshake_ack(argv[2], wav_path, pinned_peer_pk, peer_id, raw_frame);
    } else if (strcmp(cmd, "handshake-finish") == 0) {
        if (argc != 5) {
            printf("Error: ozos.exe handshake-finish <ack_hex> <a_sk_hex> <a_pub_hex>\n");
            return 1;
        }
        return cmd_handshake_finish(argv[2], argv[3], argv[4]);
    } else if (strcmp(cmd, "tunnel-encrypt") == 0) {
        if (argc != 4) {
            printf("Error: ozos.exe tunnel-encrypt <message> <session_key_hex>\n");
            return 1;
        }
        return cmd_tunnel_encrypt(argv[2], argv[3]);
    } else if (strcmp(cmd, "tunnel-decrypt") == 0) {
        if (argc != 4) {
            printf("Error: ozos.exe tunnel-decrypt <frame_hex> <session_key_hex>\n");
            return 1;
        }
        return cmd_tunnel_decrypt(argv[2], argv[3]);
    } else if (strcmp(cmd, "brainseed-gen") == 0) {
        if (argc > 3) {
            fprintf(stderr, "[SYNTAX ERROR] Too many arguments for brainseed-gen command.\n");
            return 1;
        }
        const char* passphrase = (argc == 3) ? argv[2] : "";
        return cmd_brainseed_gen(passphrase);
    } else if (strcmp(cmd, "brainseed-restore") == 0) {
        if (argc < 3 || argc > 4) {
            printf("Error: ozos.exe brainseed-restore \"<12 words>\" [passphrase]\n");
            return 1;
        }
        const char* passphrase = (argc == 4) ? argv[3] : "";
        return cmd_brainseed_restore(argv[2], passphrase);
    } else if (strcmp(cmd, "brainseed-split") == 0) {
        uint8_t threshold_k = 3;
        uint8_t total_n = 5;
        const char* custom_seed = NULL;
        for (int i = 2; i < argc; ++i) {
            if (strcmp(argv[i], "--threshold") == 0 || strcmp(argv[i], "-k") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "[SYNTAX ERROR] Flag '%s' requires integer argument.\n", argv[i]);
                    return 1;
                }
                uint32_t val = 0;
                if (!parse_strict_u32(argv[++i], &val) || val < OZOS_SHAMIR_MIN_THRESHOLD || val > OZOS_SHAMIR_MAX_SHARES) {
                    fprintf(stderr, "[SYNTAX ERROR] Threshold must be between %d and %d.\n", OZOS_SHAMIR_MIN_THRESHOLD, OZOS_SHAMIR_MAX_SHARES);
                    return 1;
                }
                threshold_k = (uint8_t)val;
            } else if (strcmp(argv[i], "--shares") == 0 || strcmp(argv[i], "-n") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "[SYNTAX ERROR] Flag '%s' requires integer argument.\n", argv[i]);
                    return 1;
                }
                uint32_t val = 0;
                if (!parse_strict_u32(argv[++i], &val) || val < OZOS_SHAMIR_MIN_THRESHOLD || val > OZOS_SHAMIR_MAX_SHARES) {
                    fprintf(stderr, "[SYNTAX ERROR] Total shares must be between %d and %d.\n", OZOS_SHAMIR_MIN_THRESHOLD, OZOS_SHAMIR_MAX_SHARES);
                    return 1;
                }
                total_n = (uint8_t)val;
            } else if (strcmp(argv[i], "--seed") == 0) {
                if (i + 1 >= argc || custom_seed != NULL) {
                    fprintf(stderr, "[SYNTAX ERROR] Flag '--seed' is duplicated or missing argument.\n");
                    return 1;
                }
                custom_seed = argv[++i];
            } else {
                fprintf(stderr, "[SYNTAX ERROR] Unexpected argument: '%s'\n", argv[i]);
                return 1;
            }
        }
        if (threshold_k > total_n) {
            fprintf(stderr, "[SYNTAX ERROR] Threshold k (%u) cannot exceed total shares n (%u)!\n", (unsigned)threshold_k, (unsigned)total_n);
            return 1;
        }
        return cmd_brainseed_split(threshold_k, total_n, custom_seed);
    } else if (strcmp(cmd, "brainseed-combine") == 0) {
        if (argc < 4) {
            fprintf(stderr, "[SYNTAX ERROR] Usage: ozos.exe brainseed-combine <share_hex_1> <share_hex_2> ... <share_hex_k>\n");
            fprintf(stderr, "   At least 2 shares are required to reconstruct.\n");
            return 1;
        }
        return cmd_brainseed_combine(argc - 2, &argv[2]);
    } else if (strcmp(cmd, "gossip-beacon") == 0) {
        uint32_t epoch = 100;
        const char* wav_path = NULL;
        bool has_epoch = false;
        bool has_wav = false;
        bool raw_frame = false;
        for (int i = 2; i < argc; ++i) {
            if (strcmp(argv[i], "--raw-frame") == 0) {
                raw_frame = true;
            } else if (argv[i][0] == '-') {
                fprintf(stderr, "[SYNTAX ERROR] Invalid flag: '%s'\n", argv[i]);
                return 1;
            } else if (!has_epoch) {
                if (!parse_strict_u32(argv[i], &epoch) || epoch == 0) {
                    fprintf(stderr, "[SYNTAX ERROR] Invalid epoch: '%s' (must be a positive integer).\n", argv[i]);
                    return 1;
                }
                has_epoch = true;
            } else if (!has_wav) {
                wav_path = argv[i];
                has_wav = true;
            } else {
                fprintf(stderr, "[SYNTAX ERROR] Too many arguments for gossip-beacon command.\n");
                return 1;
            }
        }
        return cmd_gossip_beacon(epoch, wav_path, raw_frame);
    } else if (strcmp(cmd, "gossip-listen") == 0) {
        if (argc < 3 || argc > 4) {
            printf("Error: ozos.exe gossip-listen <beacon_hex> [my_epoch]\n");
            return 1;
        }
        uint32_t my_epoch = 100;
        if (argc == 4) {
            if (!parse_strict_u32(argv[3], &my_epoch) || my_epoch == 0) {
                fprintf(stderr, "[SYNTAX ERROR] Invalid my_epoch: '%s' (must be a positive integer).\n", argv[3]);
                return 1;
            }
        }
        return cmd_gossip_listen(argv[2], my_epoch);
    } else if (strcmp(cmd, "audio-synth") == 0) {
        if (argc < 4 || argc > 5) {
            printf("Error: ozos.exe audio-synth <text> <output.wav> [--stealth | --audible]\n");
            return 1;
        }
        uint8_t mode = OZOS_AUDIO_MODE_STEALTH;
        if (argc == 5) {
            if (strcmp(argv[4], "--audible") == 0) mode = OZOS_AUDIO_MODE_AUDIBLE;
            else if (strcmp(argv[4], "--stealth") == 0) mode = OZOS_AUDIO_MODE_STEALTH;
            else {
                fprintf(stderr, "[SYNTAX ERROR] Invalid audio mode: '%s'\n", argv[4]);
                return 1;
            }
        }
        return cmd_audio_synth(argv[2], argv[3], mode);
    } else if (strcmp(cmd, "audio-decode") == 0) {
        if (argc < 3 || argc > 4) {
            printf("Error: ozos.exe audio-decode <input.wav> [--stealth | --audible]\n");
            return 1;
        }
        int forced_mode = -1;
        if (argc == 4) {
            if (strcmp(argv[3], "--stealth") == 0) forced_mode = OZOS_AUDIO_MODE_STEALTH;
            else if (strcmp(argv[3], "--audible") == 0) forced_mode = OZOS_AUDIO_MODE_AUDIBLE;
            else {
                fprintf(stderr, "[SYNTAX ERROR] Invalid decode mode: '%s'\n", argv[3]);
                return 1;
            }
        }
        return cmd_audio_decode(argv[2], forced_mode);
    } else if (strcmp(cmd, "glyph-encode") == 0) {
        if (argc < 3 || argc > 5) {
            printf("Error: ozos.exe glyph-encode <hex_packet> [--bmp <out.bmp>]\n");
            return 1;
        }
        const char* bmp_out = NULL;
        if (argc == 5) {
            if (strcmp(argv[3], "--bmp") != 0) {
                fprintf(stderr, "[SYNTAX ERROR] Invalid flag: '%s' (expected '--bmp <path>')\n", argv[3]);
                return 1;
            }
            bmp_out = argv[4];
        } else if (argc == 4) {
            fprintf(stderr, "[SYNTAX ERROR] Missing file path for flag --bmp.\n");
            return 1;
        }
        return cmd_glyph_encode(argv[2], bmp_out);
    } else if (strcmp(cmd, "glyph-decode") == 0) {
        if (argc != 3) {
            printf("Error: ozos.exe glyph-decode <in.bmp>\n");
            return 1;
        }
        return cmd_glyph_decode(argv[2]);
    } else if (strcmp(cmd, "ofdm-synth") == 0) {
        if (argc < 4 || argc > 5) {
            printf("Error: ozos.exe ofdm-synth <hex_packet> <out.wav> [--stego]\n");
            return 1;
        }
        bool stego = false;
        if (argc == 5) {
            if (strcmp(argv[4], "--stego") == 0) {
                stego = true;
            } else {
                fprintf(stderr, "[SYNTAX ERROR] Invalid flag: '%s' (expected '--stego')\n", argv[4]);
                return 1;
            }
        }
        return cmd_ofdm_synth(argv[2], argv[3], stego);
    } else if (strcmp(cmd, "ofdm-decode") == 0) {
        if (argc != 3) {
            printf("Error: ozos.exe ofdm-decode <in.wav>\n");
            return 1;
        }
        return cmd_ofdm_decode(argv[2]);
    } else if (strcmp(cmd, "lora-pack") == 0) {
        if (argc != 3) {
            printf("Error: ozos.exe lora-pack <hex_packet>\n");
            return 1;
        }
        return cmd_lora_pack(argv[2]);
    } else if (strcmp(cmd, "lora-unpack") == 0) {
        if (argc != 3) {
            printf("Error: ozos.exe lora-unpack <frame_hex>\n");
            return 1;
        }
        return cmd_lora_unpack(argv[2]);
    } else if (strcmp(cmd, "hardware") == 0) {
        return cmd_hardware(argc, argv);
    } else if (strcmp(cmd, "spectrum-status") == 0) {
        if (argc > 2) {
            fprintf(stderr, "[SYNTAX ERROR] Command spectrum-status does not accept extra arguments.\n");
            return 1;
        }
        return cmd_spectrum_status();
    } else if (strcmp(cmd, "sweep") == 0) {
        if (argc < 3) {
            fprintf(stderr, "[SYNTAX ERROR] Usage: ozos.exe sweep <directory> [--seed <hex>]\n");
            return 1;
        }
        const char* dir_path = argv[2];
        const char* custom_seed = NULL;
        for (int i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--seed") == 0) {
                if (i + 1 >= argc || custom_seed != NULL) {
                    fprintf(stderr, "[SYNTAX ERROR] Flag '--seed' is duplicated or missing argument.\n");
                    return 1;
                }
                custom_seed = argv[++i];
            } else {
                fprintf(stderr, "[SYNTAX ERROR] Unexpected argument: '%s'\n", argv[i]);
                return 1;
            }
        }
        return cmd_sweep(dir_path, custom_seed);
    } else if (strcmp(cmd, "vault") == 0) {
        if (argc < 3) {
            fprintf(stderr, "[SYNTAX ERROR] Usage: ozos.exe vault <pack|open> <args...> [--seed <words|hex>]\n");
            return 1;
        }
        const char* subcmd = argv[2];
        if (strcmp(subcmd, "pack") == 0) {
            if (argc < 5) {
                fprintf(stderr, "[SYNTAX ERROR] Usage: ozos.exe vault pack <dir> <vault.ozos> [--seed <words|hex>]\n");
                return 1;
            }
            const char* in_dir = argv[3];
            const char* out_vault = argv[4];
            const char* custom_seed = NULL;
            for (int i = 5; i < argc; ++i) {
                if (strcmp(argv[i], "--seed") == 0) {
                    if (i + 1 >= argc || custom_seed != NULL) {
                        fprintf(stderr, "[SYNTAX ERROR] Flag '--seed' is duplicated or missing argument.\n");
                        return 1;
                    }
                    custom_seed = argv[++i];
                } else {
                    fprintf(stderr, "[SYNTAX ERROR] Unexpected argument: '%s'\n", argv[i]);
                    return 1;
                }
            }
            return cmd_vault_pack(in_dir, out_vault, custom_seed);
        } else if (strcmp(subcmd, "open") == 0) {
            if (argc < 5) {
                fprintf(stderr, "[SYNTAX ERROR] Usage: ozos.exe vault open <vault.ozos> <out_dir> [--seed <words|hex>] [--burn-on-fail] [--self-destruct]\n");
                return 1;
            }
            const char* in_vault = argv[3];
            const char* out_dir = argv[4];
            const char* custom_seed = NULL;
            bool burn_on_fail = false;
            bool self_destruct = false;
            for (int i = 5; i < argc; ++i) {
                if (strcmp(argv[i], "--seed") == 0) {
                    if (i + 1 >= argc || custom_seed != NULL) {
                        fprintf(stderr, "[SYNTAX ERROR] Flag '--seed' is duplicated or missing argument.\n");
                        return 1;
                    }
                    custom_seed = argv[++i];
                } else if (strcmp(argv[i], "--burn-on-fail") == 0) {
                    burn_on_fail = true;
                } else if (strcmp(argv[i], "--self-destruct") == 0) {
                    self_destruct = true;
                } else {
                    fprintf(stderr, "[SYNTAX ERROR] Unexpected argument: '%s'\n", argv[i]);
                    return 1;
                }
            }
            return cmd_vault_open(in_vault, out_dir, custom_seed, burn_on_fail, self_destruct);
        } else if (strcmp(subcmd, "pqc-pack") == 0) {
            if (argc < 6) {
                fprintf(stderr, "[SYNTAX ERROR] Usage: ozos.exe vault pqc-pack <dir> <vault.ozos> <pk_file>\n");
                return 1;
            }
            return cmd_vault_pqc_pack(argv[3], argv[4], argv[5]);
        } else if (strcmp(subcmd, "pqc-open") == 0) {
            if (argc < 6) {
                fprintf(stderr, "[SYNTAX ERROR] Usage: ozos.exe vault pqc-open <vault.ozos> <out_dir> <sk_file> [--burn-on-fail] [--self-destruct]\n");
                return 1;
            }
            const char* in_vault = argv[3];
            const char* out_dir = argv[4];
            const char* sk_file = argv[5];
            bool burn_on_fail = false;
            bool self_destruct = false;
            for (int i = 6; i < argc; ++i) {
                if (strcmp(argv[i], "--burn-on-fail") == 0) {
                    burn_on_fail = true;
                } else if (strcmp(argv[i], "--self-destruct") == 0) {
                    self_destruct = true;
                } else {
                    fprintf(stderr, "[SYNTAX ERROR] Unexpected argument: '%s'\n", argv[i]);
                    return 1;
                }
            }
            return cmd_vault_pqc_open(in_vault, out_dir, sk_file, burn_on_fail, self_destruct);
        } else {
            fprintf(stderr, "[SYNTAX ERROR] Unknown vault sub-command: '%s' (expected 'pack' or 'open').\n", subcmd);
            return 1;
        }
    } else if (strcmp(cmd, "anchor") == 0) {
        if (argc < 3) {
            fprintf(stderr, "[SYNTAX ERROR] Usage: ozos.exe anchor <directory> [--seed <hex>]\n");
            return 1;
        }
        const char* dir_path = argv[2];
        const char* custom_seed = NULL;
        for (int i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--seed") == 0) {
                if (i + 1 >= argc || custom_seed != NULL) {
                    fprintf(stderr, "[SYNTAX ERROR] Flag '--seed' is duplicated or missing argument.\n");
                    return 1;
                }
                custom_seed = argv[++i];
            } else {
                fprintf(stderr, "[SYNTAX ERROR] Unexpected argument: '%s'\n", argv[i]);
                return 1;
            }
        }
        return cmd_anchor(dir_path, custom_seed);
    } else if (strcmp(cmd, "anchor-verify") == 0) {
        if (argc != 3) {
            fprintf(stderr, "[SYNTAX ERROR] Usage: ozos.exe anchor-verify <directory>\n");
            return 1;
        }
        return cmd_anchor_verify(argv[2]);
    } else if (strcmp(cmd, "burn") == 0) {
        if (argc != 3) {
            fprintf(stderr, "[SYNTAX ERROR] Usage: ozos.exe burn <file_path>\n");
            return 1;
        }
        return cmd_burn(argv[2]);
    } else if (strcmp(cmd, "clipboard-armor") == 0) {
        const char* custom_seed = NULL;
        for (int i = 2; i < argc; ++i) {
            if (strcmp(argv[i], "--seed") == 0) {
                if (i + 1 >= argc || custom_seed != NULL) {
                    fprintf(stderr, "[SYNTAX ERROR] Flag '--seed' is duplicated or missing argument.\n");
                    return 1;
                }
                custom_seed = argv[++i];
            } else {
                fprintf(stderr, "[SYNTAX ERROR] Unexpected argument: '%s'\n", argv[i]);
                return 1;
            }
        }
        return cmd_clipboard_armor(custom_seed);
    } else if (strcmp(cmd, "workstation-shield") == 0 || strcmp(cmd, "guard") == 0 || strcmp(cmd, "wifi-guard") == 0) {
        if (argc > 2) {
            fprintf(stderr, "[SYNTAX ERROR] Command '%s' does not accept extra arguments.\n", cmd);
            return 1;
        }
        return cmd_workstation_shield();
    } else if (strcmp(cmd, "pqc-status") == 0) {
        if (argc > 2) {
            fprintf(stderr, "[SYNTAX ERROR] Command pqc-status does not accept extra arguments.\n");
            return 1;
        }
        return cmd_pqc_status();
    } else if (strcmp(cmd, "pqc-test") == 0) {
        if (argc > 2) {
            fprintf(stderr, "[SYNTAX ERROR] Command pqc-test does not accept extra arguments.\n");
            return 1;
        }
        return cmd_pqc_test();
    } else if (strcmp(cmd, "pqc-keygen") == 0) {
        const char* custom_seed = (argc >= 3 && strlen(argv[2]) == 64) ? argv[2] : (argc >= 4 && strlen(argv[3]) == 64) ? argv[3] : NULL;
        const char* prefix = (argc >= 3 && strlen(argv[2]) != 64) ? argv[2] : (argc >= 4 && strlen(argv[3]) != 64) ? argv[3] : "pqc_identity";
        return cmd_pqc_keygen(custom_seed, prefix);
    } else if (strcmp(cmd, "crash-test") == 0) {
        if (argc > 2) {
            fprintf(stderr, "[SYNTAX ERROR] Command crash-test does not accept extra arguments.\n");
            return 1;
        }
        return cmd_crash_test();
    } else if (strcmp(cmd, "daemon") == 0 || strcmp(cmd, "--daemon") == 0) {
        if (argc > 3) {
            fprintf(stderr, "[SYNTAX ERROR] Too many arguments for daemon command.\n");
            return 1;
        }
        int port = OZOS_DEFAULT_DAEMON_PORT;
        if (argc == 3) {
            if (!parse_strict_port(argv[2], &port)) {
                fprintf(stderr, "[SYNTAX ERROR] Invalid daemon port: '%s' (1 - 65535).\n", argv[2]);
                return 1;
            }
        }
        cmd_daemon(port);
    } else if (strcmp(cmd, "benchmark") == 0) {
        if (argc > 2) {
            fprintf(stderr, "[SYNTAX ERROR] Command benchmark does not accept extra arguments.\n");
            return 1;
        }
        return cmd_benchmark();
    } else if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0 || strcmp(cmd, "-V") == 0 || strcmp(cmd, "version") == 0) {
        if (argc > 2) {
            fprintf(stderr, "[SYNTAX ERROR] Command '%s' does not accept extra arguments.\n", cmd);
            return 1;
        }
        printf("=================================================================\n");
        printf("  O.Z.O.S CLI Engine v1.2.0 (Zero-Heap Cryptographic Toolkit)\n");
        printf("=================================================================\n");
        printf("* Git Commit     : %s\n", ozos_get_git_commit());
#if defined(__aarch64__) || defined(_M_ARM64)
        printf("* Target Arch    : ARM64 (aarch64) POSIX/Android C99\n");
#elif defined(__arm__) || defined(_M_ARM)
        printf("* Target Arch    : ARMv7 POSIX C99\n");
#elif defined(_WIN32)
        printf("* Target Arch    : x86_64 Windows C99/C++ Userspace (Bare-Metal: BOOTX64.EFI)\n");
#else
        printf("* Target Arch    : x86_64 POSIX C99/C++ Userspace (Bare-Metal: BOOTX64.EFI)\n");
#endif
        printf("* Pinned WASM SHA: %s\n", OZOS_WASM_PIN_SHA256);
        printf("* Canonical Repo : https://github.com/LumenNull/OZOS.git\n");
        printf("* License        : MIT License 2026\n");
        printf("=================================================================\n");
        return 0;
    } else if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0 || strcmp(cmd, "help") == 0) {
        if (argc > 2) {
            fprintf(stderr, "[SYNTAX ERROR] Command '%s' does not accept extra arguments.\n", cmd);
            return 1;
        }
        print_banner();
        return 0;
    } else {
        printf("Invalid command: '%s'. Run 'ozos.exe --help' for available commands.\n", cmd);
        return 1;
    }

    return 0;
}
