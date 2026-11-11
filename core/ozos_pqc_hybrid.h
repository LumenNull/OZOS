/**
 * OZOS POST-QUANTUM CRYPTOGRAPHY (PQC) HYBRID KERNEL ENCLAVE
 * Specification       : NIST FIPS 203 (ML-KEM-768) & Curve25519 X25519 Hybrid KEM
 * Security Model      : Zero-Heap Stack Execution + Constant-Time Implicit Rejection
 * Hybrid Protection   : X25519 ECDH + Lattice Ring ML-KEM-768 (Dual-Layer KDF Combiner)
 */

#ifndef OZOS_PQC_HYBRID_H
#define OZOS_PQC_HYBRID_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifndef OZOS_BARE_METAL
#include <string.h>
#endif
#include "ozos_crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void randombytes(unsigned char *buf, unsigned long long len);

#define OZOS_PQC_MAGIC                     0x50514301U /* "PQC1" */
#define OZOS_PQC_HYBRID_SECRET_BYTES       32

/* Classical Layer Sizes (Curve25519) */
#define OZOS_PQC_CLASSICAL_PK_BYTES        32
#define OZOS_PQC_CLASSICAL_SK_BYTES        64
#define OZOS_PQC_CLASSICAL_SHARED_BYTES    32
#define OZOS_PQC_CLASSICAL_SIG_BYTES       64

/* NIST FIPS 203 ML-KEM-768 Constants */
#define OZOS_MLKEM_768_K                   3
#define OZOS_MLKEM_768_N                   256
#define OZOS_MLKEM_768_Q                   3329U
#define OZOS_MLKEM_768_ETA1                2
#define OZOS_MLKEM_768_ETA2                2
#define OZOS_MLKEM_768_DU                  10
#define OZOS_MLKEM_768_DV                  4
#define OZOS_MLKEM_768_PK_BYTES            1184
#define OZOS_MLKEM_768_SK_BYTES            2400
#define OZOS_MLKEM_768_CIPHERTEXT_BYTES    1088
#define OZOS_MLKEM_768_SHARED_BYTES        32

/* NIST FIPS 204 ML-DSA-65 Reserved Size Constants */
#define OZOS_MLDSA_65_PK_BYTES             1952
#define OZOS_MLDSA_65_SK_BYTES             4032
#define OZOS_MLDSA_65_SIG_BYTES            3309

/* ========================================================================= */
/*   HYBRID POST-QUANTUM COMPOSITE PACKETS & ENCAPSULATION STRUCTURES        */
/* ========================================================================= */

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;                                      /* OZOS_PQC_MAGIC */
    uint8_t  classical_pk[OZOS_PQC_CLASSICAL_PK_BYTES];  /* Curve25519 X25519 Public Key (32 B) */
    uint8_t  quantum_pk[OZOS_MLKEM_768_PK_BYTES];        /* ML-KEM-768 Encapsulation Key (1184 B) */
} ozos_pqc_hybrid_pk_t;

typedef struct {
    uint32_t magic;                                           /* OZOS_PQC_MAGIC */
    uint8_t  classical_ephemeral_pk[OZOS_PQC_CLASSICAL_PK_BYTES]; /* Ephemeral X25519 PK (32 B) */
    uint8_t  quantum_ciphertext[OZOS_MLKEM_768_CIPHERTEXT_BYTES]; /* ML-KEM-768 Ciphertext (1088 B) */
} ozos_pqc_hybrid_ct_t;

typedef struct {
    uint8_t  classical_sk[OZOS_PQC_CLASSICAL_SK_BYTES];  /* Curve25519 Secret Key */
    uint8_t  quantum_sk[OZOS_MLKEM_768_SK_BYTES];        /* ML-KEM-768 Decapsulation Key (2400 B) */
} ozos_pqc_hybrid_sk_t;
#pragma pack(pop)

/* ========================================================================= */
/*   FIPS 202 KECCAK-F[1600] / SHA3-256 / SHA3-512 / SHAKE128 / SHAKE256     */
/* ========================================================================= */

typedef struct {
    uint64_t state[25];
    uint8_t  buf[168];
    size_t   pos;
    size_t   rate;
} ozos_keccak_sponge_t;

static inline uint64_t ozos_pqc_load64_le(const uint8_t* p) {
    uint64_t r = 0;
    for (unsigned int i = 0; i < 8U; ++i) {
        r |= ((uint64_t)p[i]) << (8U * i);
    }
    return r;
}

static inline void ozos_pqc_store64_le(uint8_t* p, uint64_t v) {
    for (unsigned int i = 0; i < 8U; ++i) {
        p[i] = (uint8_t)(v >> (8U * i));
    }
}

static inline uint64_t ozos_pqc_rotl64(uint64_t x, unsigned int n) {
    return (n == 0U) ? x : ((x << n) | (x >> (64U - n)));
}

static inline void ozos_keccak_f1600(uint64_t st[25]) {
    static const uint64_t rc[24] = {
        0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL, 0x8000000080008000ULL,
        0x000000000000808bULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
        0x000000000000008aULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
        0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
        0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800aULL, 0x800000008000000aULL,
        0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
    };
    static const unsigned int rotc[24] = {
        1U,  3U,  6U, 10U, 15U, 21U, 28U, 36U, 45U, 55U,  2U, 14U,
        27U, 41U, 56U,  8U, 25U, 43U, 62U, 18U, 39U, 61U, 20U, 44U
    };
    static const unsigned int piln[24] = {
        10U,  7U, 11U, 17U, 18U,  3U,  5U, 16U,  8U, 21U, 24U,  4U,
        15U, 23U, 19U, 13U, 12U,  2U, 20U, 14U, 22U,  9U,  6U,  1U
    };

    for (unsigned int round = 0; round < 24U; ++round) {
        uint64_t bc[5];
        for (unsigned int i = 0; i < 5U; ++i) {
            bc[i] = st[i] ^ st[i + 5U] ^ st[i + 10U] ^ st[i + 15U] ^ st[i + 20U];
        }
        for (unsigned int i = 0; i < 5U; ++i) {
            uint64_t t = bc[(i + 4U) % 5U] ^ ozos_pqc_rotl64(bc[(i + 1U) % 5U], 1U);
            for (unsigned int j = 0; j < 25U; j += 5U) {
                st[j + i] ^= t;
            }
        }
        uint64_t t = st[1];
        for (unsigned int i = 0; i < 24U; ++i) {
            unsigned int j = piln[i];
            bc[0] = st[j];
            st[j] = ozos_pqc_rotl64(t, rotc[i]);
            t = bc[0];
        }
        for (unsigned int j = 0; j < 25U; j += 5U) {
            for (unsigned int i = 0; i < 5U; ++i) {
                bc[i] = st[j + i];
            }
            for (unsigned int i = 0; i < 5U; ++i) {
                st[j + i] ^= (~bc[(i + 1U) % 5U]) & bc[(i + 2U) % 5U];
            }
        }
        st[0] ^= rc[round];
    }
}

static inline void ozos_keccak_init(ozos_keccak_sponge_t* ctx, size_t rate) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->rate = rate;
}

static inline void ozos_keccak_absorb(ozos_keccak_sponge_t* ctx, const uint8_t* in, size_t inlen) {
    size_t idx = 0;
    while (idx < inlen) {
        size_t take = ctx->rate - ctx->pos;
        if (take > (inlen - idx)) take = inlen - idx;
        for (size_t i = 0; i < take; ++i) {
            ctx->buf[ctx->pos + i] ^= in[idx + i];
        }
        ctx->pos += take;
        idx += take;
        if (ctx->pos == ctx->rate) {
            for (size_t w = 0; w < ctx->rate / 8U; ++w) {
                ctx->state[w] ^= ozos_pqc_load64_le(ctx->buf + 8U * w);
            }
            ozos_keccak_f1600(ctx->state);
            memset(ctx->buf, 0, ctx->rate);
            ctx->pos = 0;
        }
    }
}

static inline void ozos_keccak_finalize(ozos_keccak_sponge_t* ctx, uint8_t domain_pad) {
    ctx->buf[ctx->pos] ^= domain_pad;
    ctx->buf[ctx->rate - 1U] ^= 0x80U;
    for (size_t w = 0; w < ctx->rate / 8U; ++w) {
        ctx->state[w] ^= ozos_pqc_load64_le(ctx->buf + 8U * w);
    }
    ozos_keccak_f1600(ctx->state);
    for (size_t w = 0; w < ctx->rate / 8U; ++w) {
        ozos_pqc_store64_le(ctx->buf + 8U * w, ctx->state[w]);
    }
    ctx->pos = 0U;
}

static inline void ozos_keccak_squeeze(ozos_keccak_sponge_t* ctx, uint8_t* out, size_t outlen) {
    size_t idx = 0;
    while (idx < outlen) {
        if (ctx->pos == ctx->rate) {
            ozos_keccak_f1600(ctx->state);
            for (size_t w = 0; w < ctx->rate / 8U; ++w) {
                ozos_pqc_store64_le(ctx->buf + 8U * w, ctx->state[w]);
            }
            ctx->pos = 0U;
        }
        size_t give = ctx->rate - ctx->pos;
        if (give > (outlen - idx)) give = outlen - idx;
        memcpy(out + idx, ctx->buf + ctx->pos, give);
        ctx->pos += give;
        idx += give;
    }
}

static inline void ozos_sha3_256(uint8_t out[32], const uint8_t* in, size_t inlen) {
    ozos_keccak_sponge_t ctx;
    ozos_keccak_init(&ctx, 136U);
    if (in && inlen > 0U) ozos_keccak_absorb(&ctx, in, inlen);
    ozos_keccak_finalize(&ctx, 0x06U);
    ozos_keccak_squeeze(&ctx, out, 32U);
    ozos_crypto_zeroize(&ctx, sizeof(ctx));
}

static inline void ozos_sha3_512(uint8_t out[64], const uint8_t* in, size_t inlen) {
    ozos_keccak_sponge_t ctx;
    ozos_keccak_init(&ctx, 72U);
    if (in && inlen > 0U) ozos_keccak_absorb(&ctx, in, inlen);
    ozos_keccak_finalize(&ctx, 0x06U);
    ozos_keccak_squeeze(&ctx, out, 64U);
    ozos_crypto_zeroize(&ctx, sizeof(ctx));
}

static inline void ozos_shake256(uint8_t* out, size_t outlen, const uint8_t* in, size_t inlen) {
    ozos_keccak_sponge_t ctx;
    ozos_keccak_init(&ctx, 136U);
    if (in && inlen > 0U) ozos_keccak_absorb(&ctx, in, inlen);
    ozos_keccak_finalize(&ctx, 0x1FU);
    ozos_keccak_squeeze(&ctx, out, outlen);
    ozos_crypto_zeroize(&ctx, sizeof(ctx));
}

/* ========================================================================= */
/*   FIPS 203 CONSTANT-TIME MODULAR ARITHMETIC & NTT ENGINE (Q = 3329)       */
/* ========================================================================= */

/* Provably exact zero-error constant-time quotient floor(x / 3329) for all 0 <= x <= 22,500,000 */
static inline uint16_t ozos_mlkem_mod_q(uint32_t x) {
    uint32_t q_quot = (uint32_t)(((uint64_t)x * 20642679ULL) >> 36);
    uint32_t r = x - q_quot * OZOS_MLKEM_768_Q;
    uint32_t diff = r - OZOS_MLKEM_768_Q;
    uint32_t keep_r = 0U - (diff >> 31);
    return (uint16_t)((r & keep_r) | (diff & ~keep_r));
}

static inline uint16_t ozos_mlkem_add_q(uint16_t a, uint16_t b) {
    uint32_t s = (uint32_t)a + (uint32_t)b;
    uint32_t diff = s - OZOS_MLKEM_768_Q;
    uint32_t keep_s = 0U - (diff >> 31);
    return (uint16_t)((s & keep_s) | (diff & ~keep_s));
}

static inline uint16_t ozos_mlkem_sub_q(uint16_t a, uint16_t b) {
    uint32_t d = (uint32_t)a - (uint32_t)b;
    uint32_t borrow = d >> 31;
    return (uint16_t)(d + borrow * OZOS_MLKEM_768_Q);
}

static inline uint16_t ozos_mlkem_mul_q(uint16_t a, uint16_t b) {
    return ozos_mlkem_mod_q((uint32_t)a * (uint32_t)b);
}

/* zetas[k] = 17^{BitRev7(k)} mod 3329 */
static inline const uint16_t* ozos_mlkem_zetas(void) {
    static const uint16_t zetas[128] = {
           1U, 1729U, 2580U, 3289U, 2642U,  630U, 1897U,  848U,
        1062U, 1919U,  193U,  797U, 2786U, 3260U,  569U, 1746U,
         296U, 2447U, 1339U, 1476U, 3046U,   56U, 2240U, 1333U,
        1426U, 2094U,  535U, 2882U, 2393U, 2879U, 1974U,  821U,
         289U,  331U, 3253U, 1756U, 1197U, 2304U, 2277U, 2055U,
         650U, 1977U, 2513U,  632U, 2865U,   33U, 1320U, 1915U,
        2319U, 1435U,  807U,  452U, 1438U, 2868U, 1534U, 2402U,
        2647U, 2617U, 1481U,  648U, 2474U, 3110U, 1227U,  910U,
          17U, 2761U,  583U, 2649U, 1637U,  723U, 2288U, 1100U,
        1409U, 2662U, 3281U,  233U,  756U, 2156U, 3015U, 3050U,
        1703U, 1651U, 2789U, 1789U, 1847U,  952U, 1461U, 2687U,
         939U, 2308U, 2437U, 2388U,  733U, 2337U,  268U,  641U,
        1584U, 2298U, 2037U, 3220U,  375U, 2549U, 2090U, 1645U,
        1063U,  319U, 2773U,  757U, 2099U,  561U, 2466U, 2594U,
        2804U, 1092U,  403U, 1026U, 1143U, 2150U, 2775U,  886U,
        1722U, 1212U, 1874U, 1029U, 2110U, 2935U,  885U, 2154U
    };
    return zetas;
}

static inline void ozos_mlkem_ntt(uint16_t f[256]) {
    const uint16_t* zetas = ozos_mlkem_zetas();
    size_t idx = 1U;
    for (size_t len = 128U; len >= 2U; len >>= 1U) {
        for (size_t start = 0U; start < 256U; start += 2U * len) {
            uint16_t zeta = zetas[idx++];
            for (size_t j = start; j < start + len; ++j) {
                uint16_t t = ozos_mlkem_mul_q(zeta, f[j + len]);
                f[j + len] = ozos_mlkem_sub_q(f[j], t);
                f[j]       = ozos_mlkem_add_q(f[j], t);
            }
        }
    }
}

static inline void ozos_mlkem_invntt(uint16_t f[256]) {
    const uint16_t* zetas = ozos_mlkem_zetas();
    size_t idx = 127U;
    for (size_t len = 2U; len <= 128U; len <<= 1U) {
        for (size_t start = 0U; start < 256U; start += 2U * len) {
            uint16_t zeta = zetas[idx--];
            for (size_t j = start; j < start + len; ++j) {
                uint16_t t = f[j];
                f[j]       = ozos_mlkem_add_q(t, f[j + len]);
                f[j + len] = ozos_mlkem_mul_q(zeta, ozos_mlkem_sub_q(f[j + len], t));
            }
        }
    }
    /* Multiply by 128^{-1} mod 3329 = 3303 */
    for (size_t j = 0U; j < 256U; ++j) {
        f[j] = ozos_mlkem_mul_q(f[j], 3303U);
    }
}

static inline void ozos_mlkem_basemul_acc(
    uint16_t acc[256],
    const uint16_t a[256],
    const uint16_t b[256]
) {
    const uint16_t* zetas = ozos_mlkem_zetas();
    for (size_t i = 0U; i < 64U; ++i) {
        uint16_t gamma0 = zetas[64U + i];
        uint16_t gamma1 = (uint16_t)(OZOS_MLKEM_768_Q - gamma0);

        size_t i0 = 4U * i;
        uint32_t a1b1_g0 = (uint32_t)ozos_mlkem_mul_q(a[i0 + 1U], b[i0 + 1U]) * (uint32_t)gamma0;
        uint32_t c0 = (uint32_t)a[i0] * (uint32_t)b[i0] + a1b1_g0;
        uint32_t c1 = (uint32_t)a[i0] * (uint32_t)b[i0 + 1U] + (uint32_t)a[i0 + 1U] * (uint32_t)b[i0];
        acc[i0]      = ozos_mlkem_add_q(acc[i0],      ozos_mlkem_mod_q(c0));
        acc[i0 + 1U] = ozos_mlkem_add_q(acc[i0 + 1U], ozos_mlkem_mod_q(c1));

        size_t i1 = i0 + 2U;
        uint32_t a1b1_g1 = (uint32_t)ozos_mlkem_mul_q(a[i1 + 1U], b[i1 + 1U]) * (uint32_t)gamma1;
        uint32_t c2 = (uint32_t)a[i1] * (uint32_t)b[i1] + a1b1_g1;
        uint32_t c3 = (uint32_t)a[i1] * (uint32_t)b[i1 + 1U] + (uint32_t)a[i1 + 1U] * (uint32_t)b[i1];
        acc[i1]      = ozos_mlkem_add_q(acc[i1],      ozos_mlkem_mod_q(c2));
        acc[i1 + 1U] = ozos_mlkem_add_q(acc[i1 + 1U], ozos_mlkem_mod_q(c3));
    }
}

/* ========================================================================= */
/*   FIPS 203 SAMPLING, BYTE ENCODING/DECODING & COMPRESSION                 */
/* ========================================================================= */

static inline void ozos_mlkem_sample_ntt(uint16_t a_hat[256], const uint8_t rho[32], uint8_t i_idx, uint8_t j_idx) {
    uint8_t seed_in[34];
    memcpy(seed_in, rho, 32U);
    seed_in[32] = i_idx;
    seed_in[33] = j_idx;

    ozos_keccak_sponge_t ctx;
    ozos_keccak_init(&ctx, 168U);
    ozos_keccak_absorb(&ctx, seed_in, 34U);
    ozos_keccak_finalize(&ctx, 0x1FU);

    size_t j = 0U;
    uint8_t block[168];
    while (j < 256U) {
        ozos_keccak_squeeze(&ctx, block, 168U);
        for (size_t k = 0U; k + 3U <= 168U && j < 256U; k += 3U) {
            uint16_t d1 = (uint16_t)(block[k] | ((uint16_t)(block[k + 1U] & 0x0FU) << 8U));
            uint16_t d2 = (uint16_t)((block[k + 1U] >> 4U) | ((uint16_t)block[k + 2U] << 4U));
            if (d1 < OZOS_MLKEM_768_Q) a_hat[j++] = d1;
            if (d2 < OZOS_MLKEM_768_Q && j < 256U) a_hat[j++] = d2;
        }
    }
    ozos_crypto_zeroize(&ctx, sizeof(ctx));
    ozos_crypto_zeroize(block, sizeof(block));
    ozos_crypto_zeroize(seed_in, sizeof(seed_in));
}

/* FIPS 203 Algorithm 8: SamplePolyCBD_2 (eta = 2 -> 128 PRF bytes) */
static inline void ozos_mlkem_sample_cbd2(uint16_t f[256], const uint8_t seed[32], uint8_t nonce) {
    uint8_t prf_in[33];
    uint8_t buf[128];
    memcpy(prf_in, seed, 32U);
    prf_in[32] = nonce;
    ozos_shake256(buf, sizeof(buf), prf_in, sizeof(prf_in));

    for (size_t i = 0U; i < 64U; ++i) {
        uint32_t t = ((uint32_t)buf[2U * i]) | ((uint32_t)buf[2U * i + 1U] << 8U);
        uint32_t d = (t & 0x5555U) + ((t >> 1U) & 0x5555U);
        for (size_t j = 0U; j < 4U; ++j) {
            uint16_t a = (uint16_t)((d >> (4U * j)) & 0x03U);
            uint16_t b = (uint16_t)((d >> (4U * j + 2U)) & 0x03U);
            f[4U * i + j] = ozos_mlkem_sub_q(a, b);
        }
    }
    ozos_crypto_zeroize(prf_in, sizeof(prf_in));
    ozos_crypto_zeroize(buf, sizeof(buf));
}

static inline uint16_t ozos_mlkem_compress(uint16_t x, unsigned int d) {
    uint32_t u = ((uint32_t)x << d) + 1664U;
    uint32_t q_div = (uint32_t)(((uint64_t)u * 20642679ULL) >> 36);
    return (uint16_t)(q_div & ((1U << d) - 1U));
}

static inline uint16_t ozos_mlkem_decompress(uint16_t y, unsigned int d) {
    return (uint16_t)(((uint32_t)y * OZOS_MLKEM_768_Q + (1U << (d - 1U))) >> d);
}

static inline void ozos_mlkem_encode12(uint8_t out[384], const uint16_t f[256]) {
    for (size_t i = 0U; i < 128U; ++i) {
        uint16_t t0 = f[2U * i];
        uint16_t t1 = f[2U * i + 1U];
        out[3U * i]      = (uint8_t)(t0 & 0xFFU);
        out[3U * i + 1U] = (uint8_t)((t0 >> 8U) | ((t1 & 0x0FU) << 4U));
        out[3U * i + 2U] = (uint8_t)(t1 >> 4U);
    }
}

/* Returns 0 if every decoded 12-bit coefficient < 3329; -1 otherwise (FIPS 203 Section 7.2) */
static inline int ozos_mlkem_decode12_checked(uint16_t f[256], const uint8_t in[384]) {
    uint32_t bad = 0U;
    for (size_t i = 0U; i < 128U; ++i) {
        uint16_t t0 = (uint16_t)(in[3U * i] | ((uint16_t)(in[3U * i + 1U] & 0x0FU) << 8U));
        uint16_t t1 = (uint16_t)((in[3U * i + 1U] >> 4U) | ((uint16_t)in[3U * i + 2U] << 4U));
        bad |= (uint32_t)(t0 >= OZOS_MLKEM_768_Q);
        bad |= (uint32_t)(t1 >= OZOS_MLKEM_768_Q);
        f[2U * i]      = t0;
        f[2U * i + 1U] = t1;
    }
    return (bad != 0U) ? -1 : 0;
}

static inline void ozos_mlkem_encode10_compress(uint8_t out[320], const uint16_t f[256]) {
    for (size_t i = 0U; i < 64U; ++i) {
        uint16_t t0 = ozos_mlkem_compress(f[4U * i + 0U], 10U);
        uint16_t t1 = ozos_mlkem_compress(f[4U * i + 1U], 10U);
        uint16_t t2 = ozos_mlkem_compress(f[4U * i + 2U], 10U);
        uint16_t t3 = ozos_mlkem_compress(f[4U * i + 3U], 10U);
        out[5U * i + 0U] = (uint8_t)(t0 & 0xFFU);
        out[5U * i + 1U] = (uint8_t)((t0 >> 8U) | ((t1 & 0x3FU) << 2U));
        out[5U * i + 2U] = (uint8_t)((t1 >> 6U) | ((t2 & 0x0FU) << 4U));
        out[5U * i + 3U] = (uint8_t)((t2 >> 4U) | ((t3 & 0x03U) << 6U));
        out[5U * i + 4U] = (uint8_t)(t3 >> 2U);
    }
}

static inline void ozos_mlkem_decode10_decompress(uint16_t f[256], const uint8_t in[320]) {
    for (size_t i = 0U; i < 64U; ++i) {
        uint16_t t0 = (uint16_t)(in[5U * i + 0U] | (((uint16_t)in[5U * i + 1U] & 0x03U) << 8U));
        uint16_t t1 = (uint16_t)((in[5U * i + 1U] >> 2U) | (((uint16_t)in[5U * i + 2U] & 0x0FU) << 6U));
        uint16_t t2 = (uint16_t)((in[5U * i + 2U] >> 4U) | (((uint16_t)in[5U * i + 3U] & 0x3FU) << 4U));
        uint16_t t3 = (uint16_t)((in[5U * i + 3U] >> 6U) | ((uint16_t)in[5U * i + 4U] << 2U));
        f[4U * i + 0U] = ozos_mlkem_decompress(t0, 10U);
        f[4U * i + 1U] = ozos_mlkem_decompress(t1, 10U);
        f[4U * i + 2U] = ozos_mlkem_decompress(t2, 10U);
        f[4U * i + 3U] = ozos_mlkem_decompress(t3, 10U);
    }
}

static inline void ozos_mlkem_encode4_compress(uint8_t out[128], const uint16_t f[256]) {
    for (size_t i = 0U; i < 128U; ++i) {
        uint16_t t0 = ozos_mlkem_compress(f[2U * i], 4U);
        uint16_t t1 = ozos_mlkem_compress(f[2U * i + 1U], 4U);
        out[i] = (uint8_t)(t0 | (t1 << 4U));
    }
}

static inline void ozos_mlkem_decode4_decompress(uint16_t f[256], const uint8_t in[128]) {
    for (size_t i = 0U; i < 128U; ++i) {
        f[2U * i]      = ozos_mlkem_decompress((uint16_t)(in[i] & 0x0FU), 4U);
        f[2U * i + 1U] = ozos_mlkem_decompress((uint16_t)(in[i] >> 4U), 4U);
    }
}

static inline void ozos_mlkem_encode1_compress(uint8_t out[32], const uint16_t f[256]) {
    for (size_t i = 0U; i < 32U; ++i) {
        uint8_t byte_val = 0U;
        for (size_t b = 0U; b < 8U; ++b) {
            byte_val |= (uint8_t)(ozos_mlkem_compress(f[8U * i + b], 1U) << b);
        }
        out[i] = byte_val;
    }
}

static inline void ozos_mlkem_decode1_decompress(uint16_t f[256], const uint8_t in[32]) {
    for (size_t i = 0U; i < 32U; ++i) {
        for (size_t b = 0U; b < 8U; ++b) {
            uint16_t bit = (uint16_t)((in[i] >> b) & 1U);
            f[8U * i + b] = ozos_mlkem_decompress(bit, 1U);
        }
    }
}

/* ========================================================================= */
/*   FIPS 203 K-PKE & ML-KEM-768 INTERNAL KEYGEN / ENCAPS / DECAPS           */
/* ========================================================================= */

static inline void ozos_kpke768_keygen(
    const uint8_t d[32],
    uint8_t ek_pke[OZOS_MLKEM_768_PK_BYTES],
    uint8_t dk_pke[1152]
) {
    uint8_t d_k[33];
    uint8_t rho_sigma[64];
    memcpy(d_k, d, 32U);
    d_k[32] = OZOS_MLKEM_768_K; /* FIPS 203 Alg 13 step 1: G(d || k) */
    ozos_sha3_512(rho_sigma, d_k, sizeof(d_k));

    const uint8_t* rho   = rho_sigma;
    const uint8_t* sigma = rho_sigma + 32U;

    uint16_t s_hat[3][256];
    for (uint8_t j = 0U; j < 3U; ++j) {
        ozos_mlkem_sample_cbd2(s_hat[j], sigma, j);
        ozos_mlkem_ntt(s_hat[j]);
        ozos_mlkem_encode12(dk_pke + (size_t)j * 384U, s_hat[j]);
    }

    uint16_t a_ij[256];
    uint16_t t_i[256];
    uint16_t e_i[256];
    for (uint8_t i = 0U; i < 3U; ++i) {
        memset(t_i, 0, sizeof(t_i));
        for (uint8_t j = 0U; j < 3U; ++j) {
            ozos_mlkem_sample_ntt(a_ij, rho, j, i); /* A_hat[i][j] = SampleNTT(rho || j || i) */
            ozos_mlkem_basemul_acc(t_i, a_ij, s_hat[j]);
        }
        ozos_mlkem_sample_cbd2(e_i, sigma, (uint8_t)(3U + i));
        ozos_mlkem_ntt(e_i);
        for (size_t n = 0U; n < 256U; ++n) {
            t_i[n] = ozos_mlkem_add_q(t_i[n], e_i[n]);
        }
        ozos_mlkem_encode12(ek_pke + (size_t)i * 384U, t_i);
    }
    memcpy(ek_pke + 1152U, rho, 32U);

    ozos_crypto_zeroize(d_k, sizeof(d_k));
    ozos_crypto_zeroize(rho_sigma, sizeof(rho_sigma));
    ozos_crypto_zeroize(s_hat, sizeof(s_hat));
    ozos_crypto_zeroize(a_ij, sizeof(a_ij));
    ozos_crypto_zeroize(t_i, sizeof(t_i));
    ozos_crypto_zeroize(e_i, sizeof(e_i));
}

static inline int ozos_kpke768_encrypt(
    const uint8_t ek_pke[OZOS_MLKEM_768_PK_BYTES],
    const uint8_t m[32],
    const uint8_t r_seed[32],
    uint8_t c_out[OZOS_MLKEM_768_CIPHERTEXT_BYTES]
) {
    uint16_t t_hat[3][256];
    for (size_t i = 0U; i < 3U; ++i) {
        if (ozos_mlkem_decode12_checked(t_hat[i], ek_pke + i * 384U) != 0) {
            ozos_crypto_zeroize(t_hat, sizeof(t_hat));
            return -3;
        }
    }
    const uint8_t* rho = ek_pke + 1152U;

    uint16_t y_hat[3][256];
    for (uint8_t j = 0U; j < 3U; ++j) {
        ozos_mlkem_sample_cbd2(y_hat[j], r_seed, j);
        ozos_mlkem_ntt(y_hat[j]);
    }

    uint16_t a_ji[256];
    uint16_t u_i[256];
    uint16_t e1_i[256];
    for (uint8_t i = 0U; i < 3U; ++i) {
        memset(u_i, 0, sizeof(u_i));
        for (uint8_t j = 0U; j < 3U; ++j) {
            ozos_mlkem_sample_ntt(a_ji, rho, i, j); /* A_hat^T[i][j] = SampleNTT(rho || i || j) */
            ozos_mlkem_basemul_acc(u_i, a_ji, y_hat[j]);
        }
        ozos_mlkem_invntt(u_i);
        ozos_mlkem_sample_cbd2(e1_i, r_seed, (uint8_t)(3U + i));
        for (size_t n = 0U; n < 256U; ++n) {
            u_i[n] = ozos_mlkem_add_q(u_i[n], e1_i[n]);
        }
        ozos_mlkem_encode10_compress(c_out + (size_t)i * 320U, u_i);
    }

    uint16_t v[256];
    uint16_t e2[256];
    uint16_t mu[256];
    memset(v, 0, sizeof(v));
    for (uint8_t j = 0U; j < 3U; ++j) {
        ozos_mlkem_basemul_acc(v, t_hat[j], y_hat[j]);
    }
    ozos_mlkem_invntt(v);
    ozos_mlkem_sample_cbd2(e2, r_seed, 6U);
    ozos_mlkem_decode1_decompress(mu, m);
    for (size_t n = 0U; n < 256U; ++n) {
        v[n] = ozos_mlkem_add_q(v[n], ozos_mlkem_add_q(e2[n], mu[n]));
    }
    ozos_mlkem_encode4_compress(c_out + 960U, v);

    ozos_crypto_zeroize(t_hat, sizeof(t_hat));
    ozos_crypto_zeroize(y_hat, sizeof(y_hat));
    ozos_crypto_zeroize(a_ji, sizeof(a_ji));
    ozos_crypto_zeroize(u_i, sizeof(u_i));
    ozos_crypto_zeroize(e1_i, sizeof(e1_i));
    ozos_crypto_zeroize(v, sizeof(v));
    ozos_crypto_zeroize(e2, sizeof(e2));
    ozos_crypto_zeroize(mu, sizeof(mu));
    return 0;
}

static inline void ozos_kpke768_decrypt(
    const uint8_t dk_pke[1152],
    const uint8_t c[OZOS_MLKEM_768_CIPHERTEXT_BYTES],
    uint8_t m_out[32]
) {
    uint16_t u_j[256];
    uint16_t s_j[256];
    uint16_t su_acc[256];
    memset(su_acc, 0, sizeof(su_acc));

    for (size_t j = 0U; j < 3U; ++j) {
        ozos_mlkem_decode10_decompress(u_j, c + j * 320U);
        ozos_mlkem_ntt(u_j);
        /* Secret key dk_pke was generated cleanly; still decode all 12-bit lanes */
        for (size_t i = 0U; i < 128U; ++i) {
            const uint8_t* p = dk_pke + j * 384U + 3U * i;
            s_j[2U * i]      = (uint16_t)(p[0] | (((uint16_t)p[1] & 0x0FU) << 8U));
            s_j[2U * i + 1U] = (uint16_t)((p[1] >> 4U) | ((uint16_t)p[2] << 4U));
        }
        ozos_mlkem_basemul_acc(su_acc, s_j, u_j);
    }
    ozos_mlkem_invntt(su_acc);

    uint16_t v[256];
    ozos_mlkem_decode4_decompress(v, c + 960U);
    for (size_t n = 0U; n < 256U; ++n) {
        v[n] = ozos_mlkem_sub_q(v[n], su_acc[n]);
    }
    ozos_mlkem_encode1_compress(m_out, v);

    ozos_crypto_zeroize(u_j, sizeof(u_j));
    ozos_crypto_zeroize(s_j, sizeof(s_j));
    ozos_crypto_zeroize(su_acc, sizeof(su_acc));
    ozos_crypto_zeroize(v, sizeof(v));
}

static inline int ozos_mlkem768_keygen_internal(
    const uint8_t d[32],
    const uint8_t z[32],
    uint8_t ek[OZOS_MLKEM_768_PK_BYTES],
    uint8_t dk[OZOS_MLKEM_768_SK_BYTES]
) {
    if (!d || !z || !ek || !dk) return -1;
    ozos_kpke768_keygen(d, ek, dk);
    memcpy(dk + 1152U, ek, OZOS_MLKEM_768_PK_BYTES);
    ozos_sha3_256(dk + 1152U + OZOS_MLKEM_768_PK_BYTES, ek, OZOS_MLKEM_768_PK_BYTES);
    memcpy(dk + 1152U + OZOS_MLKEM_768_PK_BYTES + 32U, z, 32U);
    return 0;
}

static inline int ozos_mlkem768_encaps_internal(
    const uint8_t ek[OZOS_MLKEM_768_PK_BYTES],
    const uint8_t m[32],
    uint8_t c[OZOS_MLKEM_768_CIPHERTEXT_BYTES],
    uint8_t K[OZOS_MLKEM_768_SHARED_BYTES]
) {
    if (!ek || !m || !c || !K) return -1;
    uint8_t mh[64];
    uint8_t kr[64];
    memcpy(mh, m, 32U);
    ozos_sha3_256(mh + 32U, ek, OZOS_MLKEM_768_PK_BYTES);
    ozos_sha3_512(kr, mh, 64U);

    int rc = ozos_kpke768_encrypt(ek, m, kr + 32U, c);
    if (rc == 0) {
        memcpy(K, kr, 32U);
    } else {
        ozos_crypto_zeroize(K, 32U);
    }
    ozos_crypto_zeroize(mh, sizeof(mh));
    ozos_crypto_zeroize(kr, sizeof(kr));
    return rc;
}

static inline int ozos_mlkem768_decaps_internal(
    const uint8_t dk[OZOS_MLKEM_768_SK_BYTES],
    const uint8_t c[OZOS_MLKEM_768_CIPHERTEXT_BYTES],
    uint8_t K[OZOS_MLKEM_768_SHARED_BYTES]
) {
    if (!dk || !c || !K) return -1;
    const uint8_t* dk_pke = dk;
    const uint8_t* ek_pke = dk + 1152U;
    const uint8_t* h_stored = dk + 2336U;
    const uint8_t* z = dk + 2368U;

    /* FIPS 203 Section 7.3 dk hash check */
    uint8_t h_calc[32];
    ozos_sha3_256(h_calc, ek_pke, OZOS_MLKEM_768_PK_BYTES);
    uint8_t h_diff = 0U;
    for (size_t i = 0U; i < 32U; ++i) {
        h_diff |= (uint8_t)(h_calc[i] ^ h_stored[i]);
    }
    if (h_diff != 0U) {
        ozos_crypto_zeroize(K, 32U);
        return -2;
    }

    uint8_t m_prime[32];
    ozos_kpke768_decrypt(dk_pke, c, m_prime);

    uint8_t mh[64];
    uint8_t kr_prime[64];
    memcpy(mh, m_prime, 32U);
    memcpy(mh + 32U, h_stored, 32U);
    ozos_sha3_512(kr_prime, mh, 64U);

    /* Implicit rejection shared key K_bar = SHAKE256(z || c, 32) */
    uint8_t z_c[32U + OZOS_MLKEM_768_CIPHERTEXT_BYTES];
    uint8_t k_bar[32];
    memcpy(z_c, z, 32U);
    memcpy(z_c + 32U, c, OZOS_MLKEM_768_CIPHERTEXT_BYTES);
    ozos_shake256(k_bar, 32U, z_c, sizeof(z_c));

    uint8_t c_prime[OZOS_MLKEM_768_CIPHERTEXT_BYTES];
    int enc_rc = ozos_kpke768_encrypt(ek_pke, m_prime, kr_prime + 32U, c_prime);

    uint32_t neq = (enc_rc != 0) ? 1U : 0U;
    for (size_t i = 0U; i < OZOS_MLKEM_768_CIPHERTEXT_BYTES; ++i) {
        neq |= (uint32_t)(c[i] ^ c_prime[i]);
    }
    /* Constant-time mask: 0x00 if c == c_prime else 0xFF */
    uint32_t nonzero = (neq | (0U - neq)) >> 31;
    uint8_t reject_mask = (uint8_t)(0U - nonzero);
    uint8_t accept_mask = (uint8_t)(~reject_mask);
    for (size_t i = 0U; i < 32U; ++i) {
        K[i] = (uint8_t)((kr_prime[i] & accept_mask) | (k_bar[i] & reject_mask));
    }

    ozos_crypto_zeroize(m_prime, sizeof(m_prime));
    ozos_crypto_zeroize(mh, sizeof(mh));
    ozos_crypto_zeroize(kr_prime, sizeof(kr_prime));
    ozos_crypto_zeroize(z_c, sizeof(z_c));
    ozos_crypto_zeroize(k_bar, sizeof(k_bar));
    ozos_crypto_zeroize(c_prime, sizeof(c_prime));
    return 0;
}

/* ========================================================================= */
/*   HYBRID POST-QUANTUM KEM COMBINER & KEYPAIR / ENCAPS / DECAPS API        */
/* ========================================================================= */

/**
 * Combines classical X25519 shared secret and quantum ML-KEM-768 shared secret
 * using domain-separated SHA-512 KDF:
 * K_final = SHA-512("OZOS-FIPS203-HYBRID-KDF-V1" || SS_X25519 || SS_MLKEM)[0..31]
 */
static inline void ozos_pqc_combiner_kdf(
    const uint8_t classical_shared[32],
    const uint8_t quantum_shared[32],
    uint8_t final_session_key[32]
) {
    static const char domain[] = "OZOS-FIPS203-HYBRID-KDF-V1"; /* 26 bytes */
    uint8_t kdf_in[26 + 32 + 32];
    uint8_t sha512_out[64];

    memcpy(kdf_in, domain, 26U);
    memcpy(kdf_in + 26U, classical_shared, 32U);
    memcpy(kdf_in + 58U, quantum_shared, 32U);

    crypto_hash(sha512_out, kdf_in, sizeof(kdf_in));
    memcpy(final_session_key, sha512_out, 32U);

    ozos_crypto_zeroize(kdf_in, sizeof(kdf_in));
    ozos_crypto_zeroize(sha512_out, sizeof(sha512_out));
}

/**
 * Generates a deterministic Hybrid Post-Quantum Keypair (X25519 + NIST FIPS 203 ML-KEM-768)
 * from a 32-byte master entropy seed.
 */
static inline int ozos_pqc_hybrid_keypair(
    ozos_pqc_hybrid_pk_t* pk,
    ozos_pqc_hybrid_sk_t* sk,
    const uint8_t entropy_seed[32]
) {
    if (!pk || !sk || !entropy_seed) return -1;

    static const char x_dom[] = "OZOS-X25519-SEED-V1";     /* 19 bytes */
    static const char q_dom[] = "OZOS-MLKEM768-DZ-V1";     /* 19 bytes */
    uint8_t kdf_in[19 + 32];
    uint8_t digest64[64];

    /* 1. Classical Component: Deterministic X25519 clamped scalar + pubkey */
    memcpy(kdf_in, x_dom, 19U);
    memcpy(kdf_in + 19U, entropy_seed, 32U);
    crypto_hash(digest64, kdf_in, sizeof(kdf_in));

    memset(sk->classical_sk, 0, sizeof(sk->classical_sk));
    memcpy(sk->classical_sk, digest64, 32U);
    sk->classical_sk[0]  &= 248U;
    sk->classical_sk[31] &= 127U;
    sk->classical_sk[31] |= 64U;
    crypto_scalarmult_curve25519_base(pk->classical_pk, sk->classical_sk);

    /* 2. Quantum Component: Genuine NIST FIPS 203 ML-KEM-768 KeyGen */
    memcpy(kdf_in, q_dom, 19U);
    memcpy(kdf_in + 19U, entropy_seed, 32U);
    ozos_sha3_512(digest64, kdf_in, sizeof(kdf_in));

    pk->magic = OZOS_PQC_MAGIC;
    int rc = ozos_mlkem768_keygen_internal(digest64, digest64 + 32U, pk->quantum_pk, sk->quantum_sk);

    ozos_crypto_zeroize(kdf_in, sizeof(kdf_in));
    ozos_crypto_zeroize(digest64, sizeof(digest64));
    return rc;
}

/**
 * Hybrid KEM Encapsulation: X25519 Ephemeral ECDH + NIST FIPS 203 ML-KEM-768.
 * Enforces Curve25519 low-order validation and ML-KEM-768 encapsulation key modulus checks.
 */
static inline int ozos_pqc_hybrid_encaps(
    const ozos_pqc_hybrid_pk_t* pk,
    ozos_pqc_hybrid_ct_t* ct,
    uint8_t session_key_out[32]
) {
    if (!pk || !ct || !session_key_out) return -1;
    if (pk->magic != OZOS_PQC_MAGIC) return -2;
    if (ozos_is_low_order_curve25519_point(pk->classical_pk)) return -3;

    ct->magic = OZOS_PQC_MAGIC;

    /* 1. Classical Ephemeral X25519 */
    uint8_t ephemeral_sk[32];
    uint8_t classical_shared[32];
    randombytes(ephemeral_sk, 32ULL);
    ephemeral_sk[0]  &= 248U;
    ephemeral_sk[31] &= 127U;
    ephemeral_sk[31] |= 64U;
    crypto_scalarmult_curve25519_base(ct->classical_ephemeral_pk, ephemeral_sk);

    if (ozos_ecdh_shared_secret(classical_shared, pk->classical_pk, ephemeral_sk) != 0) {
        ozos_crypto_zeroize(ephemeral_sk, sizeof(ephemeral_sk));
        ozos_crypto_zeroize(classical_shared, sizeof(classical_shared));
        return -3;
    }

    /* 2. Quantum NIST FIPS 203 ML-KEM-768 Encapsulation */
    uint8_t m[32];
    uint8_t quantum_shared[32];
    randombytes(m, 32ULL);

    int rc = ozos_mlkem768_encaps_internal(pk->quantum_pk, m, ct->quantum_ciphertext, quantum_shared);
    if (rc == 0) {
        ozos_pqc_combiner_kdf(classical_shared, quantum_shared, session_key_out);
    } else {
        ozos_crypto_zeroize(session_key_out, 32U);
    }

    ozos_crypto_zeroize(ephemeral_sk, sizeof(ephemeral_sk));
    ozos_crypto_zeroize(classical_shared, sizeof(classical_shared));
    ozos_crypto_zeroize(m, sizeof(m));
    ozos_crypto_zeroize(quantum_shared, sizeof(quantum_shared));
    return rc;
}

/**
 * Hybrid KEM Decapsulation: X25519 ECDH + NIST FIPS 203 ML-KEM-768 Implicit Rejection.
 */
static inline int ozos_pqc_hybrid_decaps(
    const ozos_pqc_hybrid_sk_t* sk,
    const ozos_pqc_hybrid_ct_t* ct,
    uint8_t session_key_out[32]
) {
    if (!sk || !ct || !session_key_out) return -1;
    if (ct->magic != OZOS_PQC_MAGIC) return -2;

    /* 1. Classical X25519 ECDH with RFC 7748 low-order point & zero-secret checks */
    uint8_t classical_shared[32];
    if (ozos_ecdh_shared_secret(classical_shared, ct->classical_ephemeral_pk, sk->classical_sk) != 0) {
        ozos_crypto_zeroize(classical_shared, sizeof(classical_shared));
        ozos_crypto_zeroize(session_key_out, 32U);
        return -3;
    }

    /* 2. Quantum NIST FIPS 203 ML-KEM-768 Decapsulation (Fujisaki-Okamoto constant-time) */
    uint8_t quantum_shared[32];
    int rc = ozos_mlkem768_decaps_internal(sk->quantum_sk, ct->quantum_ciphertext, quantum_shared);
    if (rc == 0) {
        ozos_pqc_combiner_kdf(classical_shared, quantum_shared, session_key_out);
    } else {
        ozos_crypto_zeroize(session_key_out, 32U);
    }

    ozos_crypto_zeroize(classical_shared, sizeof(classical_shared));
    ozos_crypto_zeroize(quantum_shared, sizeof(quantum_shared));
    return rc;
}

#include "ozos_mldsa65.h"

#ifdef __cplusplus
}
#endif

#endif /* OZOS_PQC_HYBRID_H */
