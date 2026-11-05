#ifndef OZOS_CRYPTO_H
#define OZOS_CRYPTO_H

#include "tweetnacl.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define OZOS_ED25519_PK_BYTES   32
#define OZOS_ED25519_SK_BYTES   64
#define OZOS_ED25519_SIG_BYTES  64
#define OZOS_MAX_MSG_LEN        4096

#ifdef __cplusplus
extern "C" {
#endif

/**
 * [MẢNH GHÉP 1]: CHỮ KÝ BẢN THỂ (ED25519 SOVEREIGN SIGNATURE)
 * Tái tạo cặp khóa Ed25519 từ Hạt giống 32-byte (TPM Silicon Seed hoặc Brainseed)
 */
static inline int ozos_ed25519_seed_keypair(
    uint8_t pk[OZOS_ED25519_PK_BYTES],
    uint8_t sk[OZOS_ED25519_SK_BYTES],
    const uint8_t seed[32]
) {
    if (!pk || !sk || !seed) return -1;
    return crypto_sign_seed_keypair(pk, sk, seed);
}

/**
 * Ký số Ed25519 trên một thông điệp (Detached Signature 64 Bytes).
 */
static inline int ozos_ed25519_sign_detached(
    uint8_t sig[OZOS_ED25519_SIG_BYTES],
    const uint8_t* msg,
    size_t msg_len,
    const uint8_t sk[OZOS_ED25519_SK_BYTES]
) {
    if (!sig || !msg || !sk || msg_len > OZOS_MAX_MSG_LEN) return -1;

    uint8_t sm[OZOS_MAX_MSG_LEN + 64];
    unsigned long long smlen = 0;

    int r = crypto_sign(sm, &smlen, msg, (unsigned long long)msg_len, sk);
    if (r != 0) return r;

    memcpy(sig, sm, OZOS_ED25519_SIG_BYTES);
    return 0;
}

/**
 * Thẩm định chữ ký số Ed25519 (Detached Verification) bằng khóa công khai bản thể.
 * Chạy 100% trên stack, không cấp phát heap RAM (no_std compliant).
 */
/**
 * Kiem tra tinh chuan tac (Canonicity) cua phan tu vo huong S theo RFC 8032 Section 5.1.7:
 * S phai thoa man nghiem ngat: 0 <= S < L (Triet tieu hoan toan Ed25519 S+L Malleability).
 * L = 2^252 + 27742317777372353535851937790883648493 (Little-endian byte array).
 */
static inline bool ozos_ed25519_is_canonical_s(const uint8_t s[32]) {
    static const uint8_t L[32] = {
        0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
        0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
    };
    for (int i = 31; i >= 0; --i) {
        if (s[i] < L[i]) return true;
        if (s[i] > L[i]) return false;
    }
    return false; /* s == L la phi chuan (non-canonical) */
}

static inline bool ozos_ed25519_verify_detached(
    const uint8_t sig[OZOS_ED25519_SIG_BYTES],
    const uint8_t* msg,
    size_t msg_len,
    const uint8_t pk[OZOS_ED25519_PK_BYTES]
) {
    if (!sig || !msg || !pk || msg_len > OZOS_MAX_MSG_LEN) return false;

    /* Bat buoc S < L de loai bo loi Malleability S+L (RFC 8032 Section 5.1.7) */
    if (!ozos_ed25519_is_canonical_s(sig + 32)) {
        return false;
    }

    uint8_t sm[OZOS_MAX_MSG_LEN + 64];
    uint8_t m_out[OZOS_MAX_MSG_LEN + 64];
    unsigned long long mlen_out = 0;

    memcpy(sm, sig, OZOS_ED25519_SIG_BYTES);
    memcpy(sm + OZOS_ED25519_SIG_BYTES, msg, msg_len);

    int r = crypto_sign_open(
        m_out,
        &mlen_out,
        sm,
        OZOS_ED25519_SIG_BYTES + (unsigned long long)msg_len,
        pk
    );

    return (r == 0 && mlen_out == (unsigned long long)msg_len);
}

/**
 * Cấu trúc Gói Tin Đã Ký Bản Thể (Signed Sovereign Frame):
 * [OZOS-80 Header (80B)] + [Payload (L Bytes)] + [Public Key (32B)] + [Signature (64B)]
 */
#define OZOS_SIG_ATTACHMENT_SIZE (OZOS_ED25519_PK_BYTES + OZOS_ED25519_SIG_BYTES) /* 96 Bytes */

static inline size_t ozos_sign_packet_frame(
    uint8_t* out_buffer,
    size_t max_out,
    const uint8_t* header_and_payload,
    size_t header_plus_payload_len,
    const uint8_t pk[32],
    const uint8_t sk[64]
) {
    size_t total = header_plus_payload_len + OZOS_SIG_ATTACHMENT_SIZE;
    if (total > max_out) return 0;

    memcpy(out_buffer, header_and_payload, header_plus_payload_len);
    if (header_plus_payload_len > 80) {
        uint64_t h = 0xA5A5A5A5ULL;
        for (size_t i = 0; i < 78; ++i) {
            h = ((h ^ (uint64_t)out_buffer[i]) * 0x100000001B3ULL);
        }
        for (size_t i = 80; i < header_plus_payload_len; ++i) {
            h = ((h ^ (uint64_t)out_buffer[i]) * 0x100000001B3ULL);
        }
        uint16_t fnv_mac = (uint16_t)((h ^ (h >> 16) ^ (h >> 32)) & 0xFFFFU);
        out_buffer[78] = (uint8_t)(fnv_mac & 0xFFU);
        out_buffer[79] = (uint8_t)((fnv_mac >> 8) & 0xFFU);
    }
    memcpy(out_buffer + header_plus_payload_len, pk, OZOS_ED25519_PK_BYTES);

    uint8_t sig[OZOS_ED25519_SIG_BYTES];
    if (ozos_ed25519_sign_detached(sig, out_buffer, header_plus_payload_len, sk) != 0) {
        return 0;
    }

    memcpy(out_buffer + header_plus_payload_len + OZOS_ED25519_PK_BYTES, sig, OZOS_ED25519_SIG_BYTES);
    return total;
}

static inline bool ozos_verify_packet_frame(
    const uint8_t* frame_buffer,
    size_t total_frame_len,
    uint8_t out_pk[32]
) {
    if (total_frame_len < (80 + OZOS_SIG_ATTACHMENT_SIZE)) return false;

    size_t msg_len = total_frame_len - OZOS_SIG_ATTACHMENT_SIZE;
    const uint8_t* pk_ptr = frame_buffer + msg_len;
    const uint8_t* sig_ptr = frame_buffer + msg_len + OZOS_ED25519_PK_BYTES;

    if (out_pk) memcpy(out_pk, pk_ptr, OZOS_ED25519_PK_BYTES);

    return ozos_ed25519_verify_detached(sig_ptr, frame_buffer, msg_len, pk_ptr);
}

/* ========================================================================= */
/*   [MẢNH GHÉP 2]: BẮT TAY HAI CHIỀU VÔ THANH (ACOUSTIC ECDH TUNNEL)       */
/*   Giao thức thiết lập đường hầm mã hóa âm học không dây trong < 2.0s      */
/*   với Perfect Forward Secrecy (X25519 + SHA-512 + XSalsa20-Poly1305)       */
/* ========================================================================= */

#define OZOS_ECDH_PK_BYTES       32
#define OZOS_ECDH_SK_BYTES       32
#define OZOS_ECDH_SHARED_BYTES   32
#define OZOS_SESSION_KEY_BYTES   32
#define OZOS_TUNNEL_NONCE_BYTES  24
#define OZOS_AUTH_TAG_BYTES      16
#define OZOS_TUNNEL_MAX_PAYLOAD  512

#define OZOS_MAGIC_HANDSHAKE_SYN 0x4F5A4831 /* "OZH1" */
#define OZOS_MAGIC_HANDSHAKE_ACK 0x4F5A4832 /* "OZH2" */
#define OZOS_MAGIC_TUNNEL_DATA   0x4F5A4844 /* "OZHD" */

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;                    /* 0x4F5A4831 */
    uint8_t  eph_pubkey[32];           /* Initiator Curve25519 Ephemeral PubKey */
    uint32_t session_nonce;            /* Anti-replay / Session freshness nonce */
    uint16_t mac16;                    /* Fast Tripwire CRC16 / MAC16 */
} ozos_handshake_syn_t;                /* 42 Bytes */

typedef struct {
    uint32_t magic;                    /* 0x4F5A4832 */
    uint8_t  eph_pubkey[32];           /* Responder Curve25519 Ephemeral PubKey */
    uint8_t  auth_tag[16];             /* Poly1305(K_session, A_pub || B_pub) */
    uint16_t mac16;                    /* Fast Tripwire CRC16 / MAC16 */
} ozos_handshake_ack_t;                /* 54 Bytes */

typedef struct {
    uint32_t magic;                    /* 0x4F5A4844 */
    uint8_t  nonce[24];                /* 24-byte XSalsa20 nonce */
    uint16_t payload_len;              /* Plaintext length */
    uint16_t header_mac16;             /* Tripwire header check */
    /* Theo sau la: [16B Poly1305 Auth Tag] + [payload_len Bytes Ciphertext] */
} ozos_tunnel_data_hdr_t;              /* 32 Bytes */
#pragma pack(pop)

/**
 * Sinh cặp khóa Curve25519 Ephemeral cho Bắt tay Âm học Vô thanh.
 */
static inline int ozos_ecdh_keypair(
    uint8_t pk[OZOS_ECDH_PK_BYTES],
    uint8_t sk[OZOS_ECDH_SK_BYTES]
) {
    if (!pk || !sk) return -1;
    return crypto_box_keypair(pk, sk);
}

static inline void ozos_crypto_zeroize(void* ptr, size_t len) {
    if (!ptr || len == 0) return;
    volatile uint8_t* p = (volatile uint8_t*)ptr;
    for (size_t i = 0; i < len; ++i) p[i] = 0x00U;
}

/**
 * Kiểm tra thời gian hằng định (constant-time) các điểm bậc thấp trên đường cong Curve25519
 * (RFC 7748 Section 6.1 / Bernstein 2006 - 8 canonical low-order points).
 */
static inline bool ozos_is_low_order_curve25519_point(const uint8_t pk[32]) {
    if (!pk) return true;
    static const uint8_t low_order_points[8][32] = {
        /* 0 (Order 1) */
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        /* 1 (Order 4) */
        {0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        /* 325606250916557431795983626356110631294008115727848805560023387167927233504 (Order 8) */
        {0xe0,0xeb,0x7a,0x7c,0x3b,0x41,0xb8,0xae,0x16,0x56,0xe3,0xfa,0xf1,0x9f,0xc4,0x6a,
         0xda,0x09,0x8d,0xeb,0x9c,0x32,0xb1,0xfd,0x86,0x62,0x05,0x16,0x5f,0x49,0xb8,0x00},
        /* 39382357235489614581723060781553021112529911719440698176882885853963445705823 (Order 8) */
        {0x5f,0x9c,0x95,0xbc,0xa3,0x50,0x8c,0x24,0xb1,0xd0,0xb1,0x55,0x9c,0x83,0xef,0x5b,
         0x04,0x44,0x5c,0xc4,0x58,0x1c,0x8e,0x86,0xd8,0x22,0x4e,0xdd,0xd0,0x9f,0x11,0x57},
        /* p - 1 (Order 2) */
        {0xec,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
         0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f},
        /* p (Order 1, non-canonical) */
        {0xed,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
         0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f},
        /* p + 1 (Order 4, non-canonical) */
        {0xee,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
         0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f},
        /* 0 with MSB set */
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80}
    };
    uint8_t canonical_pk[32];
    memcpy(canonical_pk, pk, 32);
    canonical_pk[31] &= 0x7FU;
    int match = 0;
    for (int i = 0; i < 8; ++i) {
        if (crypto_verify_32(canonical_pk, low_order_points[i]) == 0 ||
            crypto_verify_32(pk, low_order_points[i]) == 0) {
            match = 1;
        }
    }
    return (match != 0);
}

/**
 * Tính toán Điểm bí mật chung ECDH: Shared = Peer_PK ^ My_SK (mod 2^255 - 19)
 * Bắt buộc: Từ chối điểm bậc thấp (Small-Subgroup Confinement Attack RFC 7748 Section 6.1)
 * và từ chối tuyệt đối khi kết quả X25519 suy biến về toàn số 0 (all-zero output).
 */
static inline int ozos_ecdh_shared_secret(
    uint8_t shared_out[OZOS_ECDH_SHARED_BYTES],
    const uint8_t peer_pk[OZOS_ECDH_PK_BYTES],
    const uint8_t my_sk[OZOS_ECDH_SK_BYTES]
) {
    if (!shared_out || !peer_pk || !my_sk) return -1;
    if (ozos_is_low_order_curve25519_point(peer_pk)) {
        ozos_crypto_zeroize(shared_out, OZOS_ECDH_SHARED_BYTES);
        return -1;
    }
    if (crypto_scalarmult_curve25519(shared_out, my_sk, peer_pk) != 0) {
        ozos_crypto_zeroize(shared_out, OZOS_ECDH_SHARED_BYTES);
        return -1;
    }
    /* Kiểm tra thời gian hằng định (constant-time) chống khóa chung toàn 0 */
    static const uint8_t zero32[32] = {0};
    if (crypto_verify_32(shared_out, zero32) == 0) {
        ozos_crypto_zeroize(shared_out, OZOS_ECDH_SHARED_BYTES);
        return -1;
    }
    return 0;
}

/**
 * Hàm phân rã khóa KDF (Key Derivation Function) qua SHA-512 có gắn kết Session Nonce:
 * K_session (32B) = SHA-512(Shared_Secret || "OZOS_ACOUSTIC_TUNNEL_V1" || session_nonce_le)[0:32]
 */
static inline int ozos_kdf_session_key_ex(
    uint8_t session_key_out[OZOS_SESSION_KEY_BYTES],
    const uint8_t shared_secret[OZOS_ECDH_SHARED_BYTES],
    uint32_t session_nonce
) {
    if (!session_key_out || !shared_secret) return -1;
    static const uint8_t zero32[32] = {0};
    if (crypto_verify_32(shared_secret, zero32) == 0) {
        ozos_crypto_zeroize(session_key_out, OZOS_SESSION_KEY_BYTES);
        return -1;
    }
    const char salt[] = "OZOS_ACOUSTIC_TUNNEL_V1";
    uint8_t kdf_input[32 + sizeof(salt) + 4];
    memcpy(kdf_input, shared_secret, 32);
    memcpy(kdf_input + 32, salt, sizeof(salt));
    kdf_input[32 + sizeof(salt) + 0] = (uint8_t)(session_nonce & 0xFFU);
    kdf_input[32 + sizeof(salt) + 1] = (uint8_t)((session_nonce >> 8) & 0xFFU);
    kdf_input[32 + sizeof(salt) + 2] = (uint8_t)((session_nonce >> 16) & 0xFFU);
    kdf_input[32 + sizeof(salt) + 3] = (uint8_t)((session_nonce >> 24) & 0xFFU);

    size_t kdf_len = (session_nonce != 0U) ? (32 + sizeof(salt) + 4) : (32 + sizeof(salt));
    uint8_t hash_out[64];
    crypto_hash(hash_out, kdf_input, kdf_len);
    memcpy(session_key_out, hash_out, OZOS_SESSION_KEY_BYTES);

    ozos_crypto_zeroize(kdf_input, sizeof(kdf_input));
    ozos_crypto_zeroize(hash_out, sizeof(hash_out));
    return 0;
}

static inline int ozos_kdf_session_key(
    uint8_t session_key_out[OZOS_SESSION_KEY_BYTES],
    const uint8_t shared_secret[OZOS_ECDH_SHARED_BYTES]
) {
    return ozos_kdf_session_key_ex(session_key_out, shared_secret, 0U);
}

/**
 * Dẫn xuất khóa con một lần (One-Time Auth Subkey) tách biệt miền (Domain-Separated)
 * dành riêng cho bước xác thực bắt tay Poly1305, tuyệt đối không tái sử dụng trực tiếp
 * khóa phiên K_session của đường hầm XSalsa20.
 */
static inline void ozos_derive_handshake_poly1305_subkey(
    uint8_t subkey_out[32],
    const uint8_t session_key[32],
    const uint8_t a_pub[32],
    const uint8_t b_pub[32]
) {
    const char domain_label[] = "OZOS_POLY1305_HANDSHAKE_SUBKEY_V1";
    uint8_t buf[32 + 32 + 32 + sizeof(domain_label)];
    memcpy(buf, session_key, 32);
    memcpy(buf + 32, a_pub, 32);
    memcpy(buf + 64, b_pub, 32);
    memcpy(buf + 96, domain_label, sizeof(domain_label));

    uint8_t digest[64];
    crypto_hash(digest, buf, sizeof(buf));
    memcpy(subkey_out, digest, 32);

    ozos_crypto_zeroize(buf, sizeof(buf));
    ozos_crypto_zeroize(digest, sizeof(digest));
}

/**
 * Tính toán Thẻ xác nhận bắt tay Auth Tag (16 Bytes Poly1305):
 * AuthTag = Poly1305(SubKey_Handshake, A_pub || B_pub)
 */
static inline int ozos_compute_handshake_auth_tag(
    uint8_t tag_out[16],
    const uint8_t session_key[32],
    const uint8_t a_pub[32],
    const uint8_t b_pub[32]
) {
    if (!tag_out || !session_key || !a_pub || !b_pub) return -1;
    uint8_t transcript[64];
    memcpy(transcript, a_pub, 32);
    memcpy(transcript + 32, b_pub, 32);

    uint8_t poly_subkey[32];
    ozos_derive_handshake_poly1305_subkey(poly_subkey, session_key, a_pub, b_pub);
    int rc = crypto_onetimeauth_poly1305(tag_out, transcript, 64, poly_subkey);
    ozos_crypto_zeroize(poly_subkey, sizeof(poly_subkey));
    return rc;
}

/**
 * Xác thực Thẻ bắt tay Auth Tag:
 */
static inline bool ozos_verify_handshake_auth_tag(
    const uint8_t expected_tag[16],
    const uint8_t session_key[32],
    const uint8_t a_pub[32],
    const uint8_t b_pub[32]
) {
    if (!expected_tag || !session_key || !a_pub || !b_pub) return false;
    uint8_t transcript[64];
    memcpy(transcript, a_pub, 32);
    memcpy(transcript + 32, b_pub, 32);

    uint8_t poly_subkey[32];
    ozos_derive_handshake_poly1305_subkey(poly_subkey, session_key, a_pub, b_pub);
    int rc = crypto_onetimeauth_poly1305_verify(expected_tag, transcript, 64, poly_subkey);
    ozos_crypto_zeroize(poly_subkey, sizeof(poly_subkey));
    return (rc == 0);
}

/**
 * Đóng gói Gói tin Bắt tay SYN (Node A -> Node B)
 */
static inline void ozos_handshake_syn_build(
    ozos_handshake_syn_t* syn,
    const uint8_t a_pub[32],
    uint32_t session_nonce
) {
    if (!syn || !a_pub) return;
    syn->magic = OZOS_MAGIC_HANDSHAKE_SYN;
    memcpy(syn->eph_pubkey, a_pub, 32);
    syn->session_nonce = session_nonce;

    /* Tính MAC16 cổng gác cho phần đầu */
    uint16_t crc = 0xFFFF;
    const uint8_t* p = (const uint8_t*)syn;
    for (size_t i = 0; i < 40; ++i) {
        crc ^= (uint16_t)p[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 1) ? ((crc >> 1) ^ 0x8408) : (crc >> 1);
        }
    }
    syn->mac16 = crc;
}

/**
 * Thẩm định Gói tin Bắt tay SYN
 */
static inline bool ozos_handshake_syn_verify(const ozos_handshake_syn_t* syn) {
    if (!syn) return false;
    if (syn->magic != OZOS_MAGIC_HANDSHAKE_SYN) return false;
    if (ozos_is_low_order_curve25519_point(syn->eph_pubkey)) return false;

    uint16_t crc = 0xFFFF;
    const uint8_t* p = (const uint8_t*)syn;
    for (size_t i = 0; i < 40; ++i) {
        crc ^= (uint16_t)p[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 1) ? ((crc >> 1) ^ 0x8408) : (crc >> 1);
        }
    }
    return (syn->mac16 == crc);
}

/**
 * Đóng gói Gói tin Bắt tay ACK (Node B -> Node A)
 */
static inline void ozos_handshake_ack_build(
    ozos_handshake_ack_t* ack,
    const uint8_t b_pub[32],
    const uint8_t auth_tag[16]
) {
    if (!ack || !b_pub || !auth_tag) return;
    ack->magic = OZOS_MAGIC_HANDSHAKE_ACK;
    memcpy(ack->eph_pubkey, b_pub, 32);
    memcpy(ack->auth_tag, auth_tag, 16);

    uint16_t crc = 0xFFFF;
    const uint8_t* p = (const uint8_t*)ack;
    for (size_t i = 0; i < 52; ++i) {
        crc ^= (uint16_t)p[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 1) ? ((crc >> 1) ^ 0x8408) : (crc >> 1);
        }
    }
    ack->mac16 = crc;
}

/**
 * Thẩm định Gói tin Bắt tay ACK
 */
static inline bool ozos_handshake_ack_verify(const ozos_handshake_ack_t* ack) {
    if (!ack) return false;
    if (ack->magic != OZOS_MAGIC_HANDSHAKE_ACK) return false;
    if (ozos_is_low_order_curve25519_point(ack->eph_pubkey)) return false;

    uint16_t crc = 0xFFFF;
    const uint8_t* p = (const uint8_t*)ack;
    for (size_t i = 0; i < 52; ++i) {
        crc ^= (uint16_t)p[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 1) ? ((crc >> 1) ^ 0x8408) : (crc >> 1);
        }
    }
    return (ack->mac16 == crc);
}

/**
 * Mã hóa dữ liệu truyền qua Đường hầm Âm học (Acoustic Tunnel Encryption):
 * Output frame: [32B Header] + [16B Poly1305 Tag] + [payload_len Bytes Ciphertext]
 */
static inline size_t ozos_tunnel_encrypt(
    uint8_t* out_frame,
    size_t max_out,
    const uint8_t* plaintext,
    size_t plaintext_len,
    const uint8_t session_key[32],
    const uint8_t nonce24[24]
) {
    if (!out_frame || !plaintext || !session_key || !nonce24) return 0;
    if (plaintext_len > OZOS_TUNNEL_MAX_PAYLOAD) return 0;

    size_t total_frame_len = sizeof(ozos_tunnel_data_hdr_t) + 16 + plaintext_len;
    if (total_frame_len > max_out) return 0;

    /* Build header */
    ozos_tunnel_data_hdr_t* hdr = (ozos_tunnel_data_hdr_t*)out_frame;
    hdr->magic = OZOS_MAGIC_TUNNEL_DATA;
    memcpy(hdr->nonce, nonce24, 24);
    hdr->payload_len = (uint16_t)plaintext_len;

    uint16_t crc = 0xFFFF;
    const uint8_t* p = (const uint8_t*)hdr;
    for (size_t i = 0; i < 30; ++i) {
        crc ^= (uint16_t)p[i];
        for (int b = 0; b < 8; ++b) crc = (crc & 1) ? ((crc >> 1) ^ 0x8408) : (crc >> 1);
    }
    hdr->header_mac16 = crc;

    /* NaCl Secretbox authenticated encryption (Stack only, zero heap malloc) */
    uint8_t pad_m[32 + OZOS_TUNNEL_MAX_PAYLOAD] = {0};
    uint8_t pad_c[32 + OZOS_TUNNEL_MAX_PAYLOAD] = {0};
    memcpy(pad_m + 32, plaintext, plaintext_len);

    if (crypto_secretbox(pad_c, pad_m, 32 + plaintext_len, nonce24, session_key) != 0) {
        ozos_crypto_zeroize(pad_m, sizeof(pad_m));
        ozos_crypto_zeroize(pad_c, sizeof(pad_c));
        return 0;
    }

    /* pad_c + 16 chứa: 16 bytes Poly1305 MAC + plaintext_len bytes Ciphertext */
    memcpy(out_frame + sizeof(ozos_tunnel_data_hdr_t), pad_c + 16, 16 + plaintext_len);

    ozos_crypto_zeroize(pad_m, sizeof(pad_m));
    ozos_crypto_zeroize(pad_c, sizeof(pad_c));
    return total_frame_len;
}

#define OZOS_TUNNEL_REPLAY_WINDOW 64U
static uint8_t g_ozos_tunnel_seen_nonces[OZOS_TUNNEL_REPLAY_WINDOW][24];
static size_t  g_ozos_tunnel_seen_count = 0;
static size_t  g_ozos_tunnel_seen_pos   = 0;

/**
 * Giải mã dữ liệu nhận được từ Đường hầm Âm học (Acoustic Tunnel Decryption)
 * Tích hợp bộ nhớ đệm chống phát lại (Anti-Replay 24-Byte Nonce Ring Window)
 */
static inline bool ozos_tunnel_decrypt(
    uint8_t* out_plaintext,
    size_t* out_len,
    const uint8_t* in_frame,
    size_t frame_len,
    const uint8_t session_key[32]
) {
    if (!out_plaintext || !out_len || !in_frame || !session_key) return false;
    if (frame_len < sizeof(ozos_tunnel_data_hdr_t) + 16) return false;

    const ozos_tunnel_data_hdr_t* hdr = (const ozos_tunnel_data_hdr_t*)in_frame;
    if (hdr->magic != OZOS_MAGIC_TUNNEL_DATA) return false;
    if (hdr->payload_len > OZOS_TUNNEL_MAX_PAYLOAD) return false; /* Bat buoc: Chan dung tran bo nho Stack Overflow */

    /* Verify header MAC16 */
    uint16_t crc = 0xFFFF;
    const uint8_t* p = (const uint8_t*)hdr;
    for (size_t i = 0; i < 30; ++i) {
        crc ^= (uint16_t)p[i];
        for (int b = 0; b < 8; ++b) crc = (crc & 1) ? ((crc >> 1) ^ 0x8408) : (crc >> 1);
    }
    if (hdr->header_mac16 != crc) return false;

    size_t expected_total = sizeof(ozos_tunnel_data_hdr_t) + 16 + (size_t)hdr->payload_len;
    if (frame_len != expected_total) return false;

    /* Anti-replay check over seen 24-byte nonces (constant-time scan) */
    int replay_match = 0;
    for (size_t i = 0; i < g_ozos_tunnel_seen_count; ++i) {
        uint32_t diff = 0;
        for (size_t k = 0; k < 24; ++k) {
            diff |= (uint32_t)(g_ozos_tunnel_seen_nonces[i][k] ^ hdr->nonce[k]);
        }
        if (diff == 0U) {
            replay_match = 1;
        }
    }
    if (replay_match != 0) {
        return false; /* Replay of previously decrypted tunnel frame rejected! */
    }

    /* Decrypt with crypto_secretbox_open */
    uint8_t pad_c[32 + OZOS_TUNNEL_MAX_PAYLOAD] = {0};
    uint8_t pad_m[32 + OZOS_TUNNEL_MAX_PAYLOAD] = {0};
    memcpy(pad_c + 16, in_frame + sizeof(ozos_tunnel_data_hdr_t), 16 + hdr->payload_len);

    if (crypto_secretbox_open(pad_m, pad_c, 32 + hdr->payload_len, hdr->nonce, session_key) != 0) {
        ozos_crypto_zeroize(pad_c, sizeof(pad_c));
        ozos_crypto_zeroize(pad_m, sizeof(pad_m));
        return false; /* Poly1305 MAC failure or key mismatch! */
    }

    /* Record seen nonce after successful authentication */
    memcpy(g_ozos_tunnel_seen_nonces[g_ozos_tunnel_seen_pos], hdr->nonce, 24);
    g_ozos_tunnel_seen_pos = (g_ozos_tunnel_seen_pos + 1U) % OZOS_TUNNEL_REPLAY_WINDOW;
    if (g_ozos_tunnel_seen_count < OZOS_TUNNEL_REPLAY_WINDOW) {
        g_ozos_tunnel_seen_count++;
    }

    memcpy(out_plaintext, pad_m + 32, hdr->payload_len);
    *out_len = hdr->payload_len;
    ozos_crypto_zeroize(pad_c, sizeof(pad_c));
    ozos_crypto_zeroize(pad_m, sizeof(pad_m));
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* OZOS_CRYPTO_H */
