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
    memcpy(out_buffer + header_plus_payload_len, pk, OZOS_ED25519_PK_BYTES);

    uint8_t sig[OZOS_ED25519_SIG_BYTES];
    if (ozos_ed25519_sign_detached(sig, header_and_payload, header_plus_payload_len, sk) != 0) {
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

/**
 * Tính toán Điểm bí mật chung ECDH: Shared = Peer_PK ^ My_SK (mod 2^255 - 19)
 */
static inline int ozos_ecdh_shared_secret(
    uint8_t shared_out[OZOS_ECDH_SHARED_BYTES],
    const uint8_t peer_pk[OZOS_ECDH_PK_BYTES],
    const uint8_t my_sk[OZOS_ECDH_SK_BYTES]
) {
    if (!shared_out || !peer_pk || !my_sk) return -1;
    return crypto_scalarmult_curve25519(shared_out, my_sk, peer_pk);
}

/**
 * Hàm phân rã khóa KDF (Key Derivation Function) qua SHA-512:
 * K_session (32B) = SHA-512(Shared_Secret || "OZOS_ACOUSTIC_TUNNEL_V1")[0:32]
 */
static inline int ozos_kdf_session_key(
    uint8_t session_key_out[OZOS_SESSION_KEY_BYTES],
    const uint8_t shared_secret[OZOS_ECDH_SHARED_BYTES]
) {
    if (!session_key_out || !shared_secret) return -1;
    const char salt[] = "OZOS_ACOUSTIC_TUNNEL_V1";
    uint8_t kdf_input[32 + sizeof(salt)];
    memcpy(kdf_input, shared_secret, 32);
    memcpy(kdf_input + 32, salt, sizeof(salt));

    uint8_t hash_out[64];
    crypto_hash(hash_out, kdf_input, sizeof(kdf_input));
    memcpy(session_key_out, hash_out, OZOS_SESSION_KEY_BYTES);
    return 0;
}

/**
 * Tính toán Thẻ xác nhận bắt tay Auth Tag (16 Bytes Poly1305):
 * AuthTag = Poly1305(K_session, A_pub || B_pub)
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
    return crypto_onetimeauth_poly1305(tag_out, transcript, 64, session_key);
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
    return (crypto_onetimeauth_poly1305_verify(expected_tag, transcript, 64, session_key) == 0);
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
        return 0;
    }

    /* pad_c + 16 chứa: 16 bytes Poly1305 MAC + plaintext_len bytes Ciphertext */
    memcpy(out_frame + sizeof(ozos_tunnel_data_hdr_t), pad_c + 16, 16 + plaintext_len);

    return total_frame_len;
}

/**
 * Giải mã dữ liệu nhận được từ Đường hầm Âm học (Acoustic Tunnel Decryption)
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

    /* Decrypt with crypto_secretbox_open */
    uint8_t pad_c[32 + OZOS_TUNNEL_MAX_PAYLOAD] = {0};
    uint8_t pad_m[32 + OZOS_TUNNEL_MAX_PAYLOAD] = {0};
    memcpy(pad_c + 16, in_frame + sizeof(ozos_tunnel_data_hdr_t), 16 + hdr->payload_len);

    if (crypto_secretbox_open(pad_m, pad_c, 32 + hdr->payload_len, hdr->nonce, session_key) != 0) {
        return false; /* Poly1305 MAC failure or key mismatch! */
    }

    memcpy(out_plaintext, pad_m + 32, hdr->payload_len);
    *out_len = hdr->payload_len;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* OZOS_CRYPTO_H */
