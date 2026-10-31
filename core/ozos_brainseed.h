#ifndef OZOS_BRAINSEED_H
#define OZOS_BRAINSEED_H

#define _CRT_SECURE_NO_WARNINGS

#include "tweetnacl.h"
#include "bip39_words.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/*   HARDWARE/OS-ACCELERATED SHA-256 (STACK-ONLY, ZERO MALLOC)               */
/* ========================================================================= */

static inline void ozos_sha256_single_block(const uint8_t in16[16], uint8_t out32[32]) {
#if defined(_WIN32)
    HCRYPTPROV prov = 0;
    HCRYPTHASH hash = 0;
    DWORD len = 32;
    if (CryptAcquireContextA(&prov, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        if (CryptCreateHash(prov, CALG_SHA_256, 0, 0, &hash)) {
            CryptHashData(hash, in16, 16, 0);
            CryptGetHashParam(hash, HP_HASHVAL, out32, &len, 0);
            CryptDestroyHash(hash);
        }
        CryptReleaseContext(prov, 0);
    }
#endif
}

/* ========================================================================= */
/*   HMAC-SHA512 & PBKDF2 (RFC 2898 / BIP-39) - 100% STACK MEMORY           */
/* ========================================================================= */

static inline void ozos_hmac_sha512(
    uint8_t out[64],
    const uint8_t* key, size_t key_len,
    const uint8_t* msg, size_t msg_len
) {
    uint8_t k0[128] = {0};
    if (key_len > 128) {
        crypto_hash(k0, key, key_len);
    } else {
        memcpy(k0, key, key_len);
    }

    uint8_t ipad[128], opad[128];
    for (int i = 0; i < 128; ++i) {
        ipad[i] = k0[i] ^ 0x36;
        opad[i] = k0[i] ^ 0x5c;
    }

    /* Inner hash buffer on stack (Max message <= 256 bytes) */
    uint8_t inner_buf[128 + 256];
    size_t in_len = 128 + msg_len;
    if (in_len > sizeof(inner_buf)) in_len = sizeof(inner_buf);
    memcpy(inner_buf, ipad, 128);
    memcpy(inner_buf + 128, msg, msg_len);

    uint8_t inner_hash[64];
    crypto_hash(inner_hash, inner_buf, in_len);

    /* Outer hash */
    uint8_t outer_buf[128 + 64];
    memcpy(outer_buf, opad, 128);
    memcpy(outer_buf + 128, inner_hash, 64);

    crypto_hash(out, outer_buf, 128 + 64);
}

static inline void ozos_pbkdf2_sha512_bip39(
    uint8_t out64[64],
    const char* mnemonic,
    const char* passphrase
) {
    size_t pwd_len = strlen(mnemonic);

    /* Salt = "mnemonic" + passphrase */
    char salt[128] = "mnemonic";
    if (passphrase && strlen(passphrase) > 0) {
        size_t rem = sizeof(salt) - strlen(salt) - 1;
        strncat(salt, passphrase, rem);
    }
    size_t salt_len = strlen(salt);

    /* Initial salt + 0x00000001 (Block 1) */
    uint8_t salt_block[128 + 4];
    memcpy(salt_block, salt, salt_len);
    salt_block[salt_len + 0] = 0x00;
    salt_block[salt_len + 1] = 0x00;
    salt_block[salt_len + 2] = 0x00;
    salt_block[salt_len + 3] = 0x01;

    uint8_t u[64];
    ozos_hmac_sha512(u, (const uint8_t*)mnemonic, pwd_len, salt_block, salt_len + 4);

    uint8_t t[64];
    memcpy(t, u, 64);

    /* 2048 rounds of PBKDF2 */
    for (int iter = 1; iter < 2048; ++iter) {
        uint8_t u_next[64];
        ozos_hmac_sha512(u_next, (const uint8_t*)mnemonic, pwd_len, u, 64);
        for (int i = 0; i < 64; ++i) {
            t[i] ^= u_next[i];
        }
        memcpy(u, u_next, 64);
    }

    memcpy(out64, t, 64);
}

/* ========================================================================= */
/*   [MẢNH GHÉP 3]: HẠT GIỐNG PHỤC SINH TRONG TRÍ NÃO (12-WORD BRAINSEED)    */
/* ========================================================================= */

/**
 * Sinh Mnemonic 12 từ ngẫu nhiên từ Entropy 16-byte (128-bit) và trích xuất Master Seed (32B).
 */
static inline bool ozos_brainseed_generate_ex(
    char* mnemonic_out,
    size_t max_mnemonic_len,
    const uint8_t entropy16[16],
    const char* passphrase,
    uint8_t seed32_out[32]
) {
    if (!mnemonic_out || max_mnemonic_len < 120 || !entropy16) return false;

    /* 1. Tính mã kiểm tra Checksum (4 bits đầu của SHA-256) */
    uint8_t hash[32] = {0};
    ozos_sha256_single_block(entropy16, hash);
    uint8_t checksum = (hash[0] >> 4) & 0x0F;

    /* 2. Ghép 128 bit entropy + 4 bit checksum = 132 bits */
    uint8_t bits132[17];
    memcpy(bits132, entropy16, 16);
    bits132[16] = (uint8_t)(checksum << 4);

    /* 3. Tách thành 12 phần tử, mỗi phần tử 11 bit (0 .. 2047) */
    uint16_t word_indices[12];
    for (int i = 0; i < 12; ++i) {
        int bit_idx = i * 11;
        int byte_idx = bit_idx / 8;
        int bit_offset = bit_idx % 8;

        uint32_t chunk = ((uint32_t)bits132[byte_idx] << 16) |
                         ((uint32_t)bits132[byte_idx + 1] << 8);
        if (byte_idx + 2 < 17) {
            chunk |= (uint32_t)bits132[byte_idx + 2];
        }
        word_indices[i] = (uint16_t)((chunk >> (24 - 11 - bit_offset)) & 0x7FF);
    }

    /* 4. Tra từ điển BIP-39 tạo chuỗi Mnemonic */
    mnemonic_out[0] = '\0';
    for (int i = 0; i < 12; ++i) {
        uint16_t idx = word_indices[i];
        if (idx >= 2048) return false;
        strcat(mnemonic_out, ozos_bip39_wordlist[idx]);
        if (i < 11) strcat(mnemonic_out, " ");
    }

    /* 5. Phục sinh Master Seed 32-byte bằng PBKDF2 */
    if (seed32_out) {
        uint8_t seed64[64];
        ozos_pbkdf2_sha512_bip39(seed64, mnemonic_out, passphrase ? passphrase : "");
        memcpy(seed32_out, seed64, 32);
    }

    return true;
}

static inline bool ozos_brainseed_generate(
    char* mnemonic_out,
    size_t max_mnemonic_len,
    const uint8_t entropy16[16],
    uint8_t seed32_out[32]
) {
    return ozos_brainseed_generate_ex(mnemonic_out, max_mnemonic_len, entropy16, "", seed32_out);
}

/**
 * Phục sinh Danh tính và Master Seed từ 12 Từ Trí Não (12-Word Brainseed).
 */
static inline bool ozos_brainseed_restore(
    uint8_t seed32_out[32],
    const char* mnemonic_str,
    const char* passphrase
) {
    if (!seed32_out || !mnemonic_str) return false;

    /* Tách 12 từ */
    char copy[256];
    strncpy(copy, mnemonic_str, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    char* words[12];
    int count = 0;
    char* token = strtok(copy, " \t\r\n");
    while (token && count < 12) {
        words[count++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    if (count != 12) return false; /* Bắt buộc đúng 12 từ */

    /* Tìm index của từng từ trong từ điển BIP-39 */
    uint16_t indices[12];
    for (int i = 0; i < 12; ++i) {
        int found = -1;
        for (int w = 0; w < 2048; ++w) {
            if (strcmp(words[i], ozos_bip39_wordlist[w]) == 0) {
                found = w;
                break;
            }
        }
        if (found < 0) return false; /* Từ không thuộc chuẩn BIP-39 */
        indices[i] = (uint16_t)found;
    }

    /* Tái tạo 132 bits: 128 bit entropy + 4 bit checksum */
    uint8_t entropy[16] = {0};
    uint8_t bits132[17] = {0};

    int bit_cursor = 0;
    for (int i = 0; i < 12; ++i) {
        uint16_t val = indices[i];
        for (int b = 10; b >= 0; --b) {
            int bit = (val >> b) & 1;
            int byte_pos = bit_cursor / 8;
            int bit_pos = 7 - (bit_cursor % 8);
            if (bit) bits132[byte_pos] |= (1 << bit_pos);
            bit_cursor++;
        }
    }

    memcpy(entropy, bits132, 16);
    uint8_t provided_checksum = (bits132[16] >> 4) & 0x0F;

    /* Kiểm tra Checksum SHA-256 */
    uint8_t hash[32] = {0};
    ozos_sha256_single_block(entropy, hash);
    uint8_t expected_checksum = (hash[0] >> 4) & 0x0F;

    if (provided_checksum != expected_checksum) {
        return false; /* Sai checksum -> người dùng gõ nhầm từ */
    }

    /* Tính toán Master Seed 64B -> Lấy 32B đầu cho Ed25519 & X25519 */
    uint8_t seed64[64];
    ozos_pbkdf2_sha512_bip39(seed64, mnemonic_str, passphrase ? passphrase : "");
    memcpy(seed32_out, seed64, 32);

    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* OZOS_BRAINSEED_H */
