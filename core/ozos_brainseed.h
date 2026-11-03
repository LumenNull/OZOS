#ifndef OZOS_BRAINSEED_H
#define OZOS_BRAINSEED_H

#define _CRT_SECURE_NO_WARNINGS

#define OZOS_MAX_PASSPHRASE_LEN 500

#include "tweetnacl.h"
#include "bip39_words.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/*   PURE C99 FIPS 180-4 SHA-256 (STACK-ONLY, ZERO MALLOC, 100% PORTABLE)    */
/* ========================================================================= */

static inline uint32_t ozos_sha256_rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

static inline void ozos_sha256_transform(uint32_t state[8], const uint8_t data[64]) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = ((uint32_t)data[i * 4] << 24) |
               ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) |
               ((uint32_t)data[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = ozos_sha256_rotr(w[i - 15], 7) ^ ozos_sha256_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ozos_sha256_rotr(w[i - 2], 17) ^ ozos_sha256_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = ozos_sha256_rotr(e, 6) ^ ozos_sha256_rotr(e, 11) ^ ozos_sha256_rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = ozos_sha256_rotr(a, 2) ^ ozos_sha256_rotr(a, 13) ^ ozos_sha256_rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static inline void ozos_sha256(const uint8_t* msg, size_t len, uint8_t out32[32]) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    uint8_t block[64];
    size_t offset = 0;
    while (len - offset >= 64) {
        ozos_sha256_transform(state, msg + offset);
        offset += 64;
    }
    size_t rem = len - offset;
    memcpy(block, msg + offset, rem);
    block[rem++] = 0x80;
    if (rem > 56) {
        memset(block + rem, 0, 64 - rem);
        ozos_sha256_transform(state, block);
        memset(block, 0, 56);
    } else {
        memset(block + rem, 0, 56 - rem);
    }
    uint64_t total_bits = (uint64_t)len * 8ULL;
    for (int i = 0; i < 8; ++i) {
        block[56 + i] = (uint8_t)(total_bits >> (56 - i * 8));
    }
    ozos_sha256_transform(state, block);

    for (int i = 0; i < 8; ++i) {
        out32[i * 4 + 0] = (uint8_t)(state[i] >> 24);
        out32[i * 4 + 1] = (uint8_t)(state[i] >> 16);
        out32[i * 4 + 2] = (uint8_t)(state[i] >> 8);
        out32[i * 4 + 3] = (uint8_t)(state[i]);
    }
}

static inline void ozos_sha256_single_block(const uint8_t in16[16], uint8_t out32[32]) {
    ozos_sha256(in16, 16, out32);
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

    /* Inner hash buffer on stack - Sized safely for up to 2048 bytes (Salt + Long Passphrase + Counter) */
    uint8_t inner_buf[128 + 2048 + 4];
    size_t copy_len = msg_len;
    if (copy_len > (2048 + 4)) copy_len = (2048 + 4);
    memcpy(inner_buf, ipad, 128);
    memcpy(inner_buf + 128, msg, copy_len);

    uint8_t inner_hash[64];
    crypto_hash(inner_hash, inner_buf, 128 + copy_len);

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

    /* Salt = "mnemonic" + passphrase (Hỗ trợ passphrase an toàn tuyệt đối tới 500 ký tự, kẹp biên chống identity fork) */
    char salt[2048] = "mnemonic";
    if (passphrase && strlen(passphrase) > 0) {
        size_t rem = sizeof(salt) - strlen(salt) - 1;
        strncat(salt, passphrase, rem);
    }
    size_t salt_len = strlen(salt);

    /* Initial salt + 0x00000001 (Block 1) */
    uint8_t salt_block[2048 + 4];
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
    if (passphrase && strlen(passphrase) > OZOS_MAX_PASSPHRASE_LEN) return false;

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
    if (passphrase && strlen(passphrase) > OZOS_MAX_PASSPHRASE_LEN) return false;

    /* Tách 12 từ */
    char copy[512];
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
    if (token != NULL) return false; /* Nghiêm ngặt: Thừa từ thứ 13 trở đi -> Bị từ chối */

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

    /* Tính toán Master Seed 64B -> Chuẩn hóa 12 từ với khoảng trắng đơn chống phân nhánh danh tính */
    char normalized_mnemonic[256];
    normalized_mnemonic[0] = 0;
    for (int i = 0; i < 12; ++i) {
        if (i > 0) strcat(normalized_mnemonic, " ");
        strcat(normalized_mnemonic, words[i]);
    }

    uint8_t seed64[64];
    ozos_pbkdf2_sha512_bip39(seed64, normalized_mnemonic, passphrase ? passphrase : "");
    memcpy(seed32_out, seed64, 32);

    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* OZOS_BRAINSEED_H */
