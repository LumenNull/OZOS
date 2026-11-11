/**
 * OZOS POST-QUANTUM SIGNATURE ENGINE (NIST FIPS 204 ML-DSA-65)
 * Parameter Set  : ML-DSA-65 (Security Category 3: k=6, l=5, eta=4, tau=49, omega=55)
 * Arithmetic     : Zero-Heap Stack Execution + Constant-Time Montgomery NTT (q=8380417)
 */

#ifndef OZOS_MLDSA65_H
#define OZOS_MLDSA65_H

#ifndef OZOS_PQC_HYBRID_H
#error "Include ozos_pqc_hybrid.h instead of ozos_mldsa65.h directly."
#endif

#define OZOS_MLDSA_Q              8380417
#define OZOS_MLDSA_QINV           58728449U /* q * QINV ≡ 1 mod 2^32 */
#define OZOS_MLDSA_N              256
#define OZOS_MLDSA_K              6
#define OZOS_MLDSA_L              5
#define OZOS_MLDSA_ETA            4
#define OZOS_MLDSA_TAU            49
#define OZOS_MLDSA_BETA           196
#define OZOS_MLDSA_D              13
#define OZOS_MLDSA_GAMMA1         (1 << 19) /* 524288 */
#define OZOS_MLDSA_GAMMA2         ((OZOS_MLDSA_Q - 1) / 32) /* 261888 */
#define OZOS_MLDSA_OMEGA          55
#define OZOS_MLDSA_CTILDE_BYTES   48

/* ========================================================================= */
/*   CONSTANT-TIME MODULAR ARITHMETIC & MONTGOMERY NTT (Q = 8380417)         */
/* ========================================================================= */

static inline int32_t ozos_mldsa_montgomery_reduce(int64_t a) {
    int32_t t = (int32_t)((uint32_t)(uint64_t)a * OZOS_MLDSA_QINV);
    int64_t u = a - (int64_t)t * (int64_t)OZOS_MLDSA_Q;
    return (int32_t)(uint32_t)((uint64_t)u >> 32);
}

static inline int32_t ozos_mldsa_reduce32(int32_t a) {
    int64_t shifted = (int64_t)a + (1LL << 22) + (1LL << 31);
    int32_t t = (int32_t)((uint64_t)shifted >> 23) - (1 << 8);
    return (int32_t)((int64_t)a - (int64_t)t * (int64_t)OZOS_MLDSA_Q);
}

static inline int32_t ozos_mldsa_caddq(int32_t a) {
    uint32_t ua = (uint32_t)a;
    uint32_t mask = 0U - (ua >> 31);
    return (int32_t)(ua + (mask & (uint32_t)OZOS_MLDSA_Q));
}

static inline int32_t ozos_mldsa_freeze(int32_t a) {
    int32_t r = ozos_mldsa_caddq(ozos_mldsa_reduce32(a));
    /* Belt-and-suspenders branchless normalization into [0, Q-1] */
    int32_t diff = r - OZOS_MLDSA_Q;
    uint32_t keep_r = 0U - ((uint32_t)diff >> 31);
    return (int32_t)(((uint32_t)r & keep_r) | ((uint32_t)diff & ~keep_r));
}

static inline int32_t ozos_mldsa_center_q(int32_t a) {
    int32_t r = ozos_mldsa_freeze(a);
    int32_t diff = (OZOS_MLDSA_Q - 1) / 2 - r;
    uint32_t neg = 0U - ((uint32_t)diff >> 31);
    return (int32_t)((uint32_t)r - (neg & (uint32_t)OZOS_MLDSA_Q));
}

static inline const int32_t* ozos_mldsa_zetas(void) {
    static const int32_t zetas[256] = {
         -4186625,     25847,  -2608894,   -518909,    237124,   -777960,   -876248,    466468,
          1826347,   2353451,   -359251,  -2091905,   3119733,  -2884855,   3111497,   2680103,
          2725464,   1024112,  -1079900,   3585928,   -549488,  -1119584,   2619752,  -2108549,
         -2118186,  -3859737,  -1399561,  -3277672,   1757237,    -19422,   4010497,    280005,
          2706023,     95776,   3077325,   3530437,  -1661693,  -3592148,  -2537516,   3915439,
         -3861115,  -3043716,   3574422,  -2867647,   3539968,   -300467,   2348700,   -539299,
         -1699267,  -1643818,   3505694,  -3821735,   3507263,  -2140649,  -1600420,   3699596,
           811944,    531354,    954230,   3881043,   3900724,  -2556880,   2071892,  -2797779,
         -3930395,  -1528703,  -3677745,  -3041255,  -1452451,   3475950,   2176455,  -1585221,
         -1257611,   1939314,  -4083598,  -1000202,  -3190144,  -3157330,  -3632928,    126922,
          3412210,   -983419,   2147896,   2715295,  -2967645,  -3693493,   -411027,  -2477047,
          -671102,  -1228525,    -22981,  -1308169,   -381987,   1349076,   1852771,  -1430430,
         -3343383,    264944,    508951,   3097992,     44288,  -1100098,    904516,   3958618,
         -3724342,     -8578,   1653064,  -3249728,   2389356,   -210977,    759969,  -1316856,
           189548,  -3553272,   3159746,  -1851402,  -2409325,   -177440,   1315589,   1341330,
          1285669,  -1584928,   -812732,  -1439742,  -3019102,  -3881060,  -3628969,   3839961,
          2091667,   3407706,   2316500,   3817976,  -3342478,   2244091,  -2446433,  -3562462,
           266997,   2434439,  -1235728,   3513181,  -3520352,  -3759364,  -1197226,  -3193378,
           900702,   1859098,    909542,    819034,    495491,  -1613174,    -43260,   -522500,
          -655327,  -3122442,   2031748,   3207046,  -3556995,   -525098,   -768622,  -3595838,
           342297,    286988,  -2437823,   4108315,   3437287,  -3342277,   1735879,    203044,
          2842341,   2691481,  -2590150,   1265009,   4055324,   1247620,   2486353,   1595974,
         -3767016,   1250494,   2635921,  -3548272,  -2994039,   1869119,   1903435,  -1050970,
         -1333058,   1237275,  -3318210,  -1430225,   -451100,   1312455,   3306115,  -1962642,
         -1279661,   1917081,  -2546312,  -1374803,   1500165,    777191,   2235880,   3406031,
          -542412,  -2831860,  -1671176,  -1846953,  -2584293,  -3724270,    594136,  -3776993,
         -2013608,   2432395,   2454455,   -164721,   1957272,   3369112,    185531,  -1207385,
         -3183426,    162844,   1616392,   3014001,    810149,   1652634,  -3694233,  -1799107,
         -3038916,   3523897,   3866901,    269760,   2213111,   -975884,   1717735,    472078,
          -426683,   1723600,  -1803090,   1910376,  -1667432,  -1104333,   -260646,  -3833893,
         -2939036,  -2235985,   -420899,  -2286327,    183443,   -976891,   1612842,  -3545687,
          -554416,   3919660,    -48306,  -1362209,   3937738,   1400424,   -846154,   1976782
    };
    return zetas;
}

static inline void ozos_mldsa_ntt(int32_t a[256]) {
    const int32_t* zetas = ozos_mldsa_zetas();
    size_t k = 0U;
    for (size_t len = 128U; len >= 1U; len >>= 1U) {
        for (size_t start = 0U; start < 256U; start += 2U * len) {
            int32_t zeta = zetas[++k];
            for (size_t j = start; j < start + len; ++j) {
                int32_t t = ozos_mldsa_montgomery_reduce((int64_t)zeta * (int64_t)a[j + len]);
                a[j + len] = a[j] - t;
                a[j]       = a[j] + t;
            }
        }
    }
}

static inline void ozos_mldsa_invntt_tomont(int32_t a[256]) {
    const int32_t* zetas = ozos_mldsa_zetas();
    const int32_t f_r2 = 41978; /* 256^{-1} * 2^{64} mod Q */
    size_t k = 256U;
    for (size_t len = 1U; len < 256U; len <<= 1U) {
        for (size_t start = 0U; start < 256U; start += 2U * len) {
            int32_t zeta = -zetas[--k];
            for (size_t j = start; j < start + len; ++j) {
                int32_t t = a[j];
                a[j]       = t + a[j + len];
                a[j + len] = ozos_mldsa_montgomery_reduce((int64_t)zeta * (int64_t)(t - a[j + len]));
            }
        }
    }
    for (size_t j = 0U; j < 256U; ++j) {
        a[j] = ozos_mldsa_montgomery_reduce((int64_t)f_r2 * (int64_t)a[j]);
    }
}

static inline void ozos_mldsa_poly_pointwise_acc_mont(
    int32_t acc[256],
    const int32_t a[256],
    const int32_t b[256]
) {
    for (size_t i = 0U; i < 256U; ++i) {
        acc[i] += ozos_mldsa_montgomery_reduce((int64_t)a[i] * (int64_t)b[i]);
    }
}

/* ========================================================================= */
/*   ROUNDING, DECOMPOSITION & HINTS (CONSTANT-TIME, DIVISION-FREE)          */
/* ========================================================================= */

static inline int32_t ozos_mldsa_power2round(int32_t a, int32_t* a0) {
    int32_t ap = ozos_mldsa_freeze(a);
    int32_t a1 = (ap + (1 << (OZOS_MLDSA_D - 1)) - 1) >> OZOS_MLDSA_D;
    *a0 = ap - (a1 << OZOS_MLDSA_D);
    return a1;
}

static inline int32_t ozos_mldsa_decompose(int32_t a, int32_t* a0) {
    int32_t ap = ozos_mldsa_freeze(a);
    int32_t a1 = (ap + 127) >> 7;
    a1 = (a1 * 1025 + (1 << 21)) >> 22;
    a1 &= 15;
    int32_t r0 = ap - a1 * 2 * OZOS_MLDSA_GAMMA2;
    int32_t diff = (OZOS_MLDSA_Q - 1) / 2 - r0;
    uint32_t wrap = 0U - ((uint32_t)diff >> 31);
    *a0 = (int32_t)((uint32_t)r0 - (wrap & (uint32_t)OZOS_MLDSA_Q));
    return a1;
}

static inline unsigned int ozos_mldsa_make_hint(int32_t z, int32_t r) {
    int32_t dummy0 = 0;
    int32_t r1 = ozos_mldsa_decompose(r, &dummy0);
    int32_t v1 = ozos_mldsa_decompose(r + z, &dummy0);
    uint32_t diff = (uint32_t)(r1 ^ v1);
    return (unsigned int)((diff | (0U - diff)) >> 31);
}

static inline int32_t ozos_mldsa_use_hint(int32_t a, unsigned int hint) {
    int32_t a0 = 0;
    int32_t a1 = ozos_mldsa_decompose(a, &a0);
    if (hint == 0U) return a1;
    if (a0 > 0) return (a1 == 15) ? 0 : (a1 + 1);
    return (a1 == 0) ? 15 : (a1 - 1);
}

static inline int ozos_mldsa_poly_chknorm(const int32_t a[256], int32_t bound) {
    if (bound > (OZOS_MLDSA_Q - 1) / 8) return 1;
    for (size_t i = 0U; i < 256U; ++i) {
        int32_t t = ozos_mldsa_center_q(a[i]);
        uint32_t ut = (uint32_t)t;
        uint32_t sign = 0U - (ut >> 31);
        int32_t abs_t = (int32_t)((ut ^ sign) - sign);
        if (abs_t >= bound) return 1;
    }
    return 0;
}

/* ========================================================================= */
/*   SAMPLING & BIT PACKING                                                  */
/* ========================================================================= */

static inline void ozos_mldsa_sample_a_ij(
    int32_t a[256],
    const uint8_t rho[32],
    uint8_t i_idx,
    uint8_t j_idx
) {
    uint8_t seed[34];
    memcpy(seed, rho, 32U);
    seed[32] = j_idx;
    seed[33] = i_idx;

    ozos_keccak_sponge_t ctx;
    ozos_keccak_init(&ctx, 168U);
    ozos_keccak_absorb(&ctx, seed, 34U);
    ozos_keccak_finalize(&ctx, 0x1FU);

    size_t ctr = 0U;
    uint8_t block[168];
    while (ctr < 256U) {
        ozos_keccak_squeeze(&ctx, block, 168U);
        for (size_t pos = 0U; pos + 3U <= 168U && ctr < 256U; pos += 3U) {
            uint32_t val = (uint32_t)block[pos] |
                           ((uint32_t)block[pos + 1U] << 8U) |
                           (((uint32_t)block[pos + 2U] & 0x7FU) << 16U);
            if (val < (uint32_t)OZOS_MLDSA_Q) {
                a[ctr++] = (int32_t)val;
            }
        }
    }
    ozos_crypto_zeroize(&ctx, sizeof(ctx));
    ozos_crypto_zeroize(block, sizeof(block));
    ozos_crypto_zeroize(seed, sizeof(seed));
}

static inline void ozos_mldsa_sample_eta4(
    int32_t a[256],
    const uint8_t seed[64],
    uint16_t nonce
) {
    uint8_t in[66];
    memcpy(in, seed, 64U);
    in[64] = (uint8_t)(nonce & 0xFFU);
    in[65] = (uint8_t)(nonce >> 8U);

    ozos_keccak_sponge_t ctx;
    ozos_keccak_init(&ctx, 136U);
    ozos_keccak_absorb(&ctx, in, 66U);
    ozos_keccak_finalize(&ctx, 0x1FU);

    size_t ctr = 0U;
    uint8_t block[136];
    while (ctr < 256U) {
        ozos_keccak_squeeze(&ctx, block, 136U);
        for (size_t pos = 0U; pos < 136U && ctr < 256U; ++pos) {
            uint32_t t0 = block[pos] & 0x0FU;
            uint32_t t1 = block[pos] >> 4U;
            if (t0 < 9U) {
                a[ctr++] = (int32_t)OZOS_MLDSA_ETA - (int32_t)t0;
            }
            if (t1 < 9U && ctr < 256U) {
                a[ctr++] = (int32_t)OZOS_MLDSA_ETA - (int32_t)t1;
            }
        }
    }
    ozos_crypto_zeroize(&ctx, sizeof(ctx));
    ozos_crypto_zeroize(block, sizeof(block));
    ozos_crypto_zeroize(in, sizeof(in));
}

static inline void ozos_mldsa_sample_in_ball(
    int32_t c[256],
    const uint8_t c_tilde[OZOS_MLDSA_CTILDE_BYTES]
) {
    ozos_keccak_sponge_t ctx;
    ozos_keccak_init(&ctx, 136U);
    ozos_keccak_absorb(&ctx, c_tilde, OZOS_MLDSA_CTILDE_BYTES);
    ozos_keccak_finalize(&ctx, 0x1FU);

    uint8_t buf[136];
    ozos_keccak_squeeze(&ctx, buf, 136U);
    uint64_t signs = ozos_pqc_load64_le(buf);
    size_t pos = 8U;

    memset(c, 0, sizeof(int32_t) * 256U);
    for (size_t i = 256U - OZOS_MLDSA_TAU; i < 256U; ++i) {
        uint8_t b = 0U;
        for (;;) {
            if (pos >= 136U) {
                ozos_keccak_squeeze(&ctx, buf, 136U);
                pos = 0U;
            }
            b = buf[pos++];
            if (b <= i) break;
        }
        c[i] = c[b];
        c[b] = ((signs & 1ULL) == 0ULL) ? 1 : -1;
        signs >>= 1U;
    }
    ozos_crypto_zeroize(&ctx, sizeof(ctx));
    ozos_crypto_zeroize(buf, sizeof(buf));
}

static inline void ozos_mldsa_expand_mask_poly(
    int32_t y[256],
    const uint8_t rhop[64],
    uint16_t nonce
) {
    uint8_t in[66];
    uint8_t buf[640];
    memcpy(in, rhop, 64U);
    in[64] = (uint8_t)(nonce & 0xFFU);
    in[65] = (uint8_t)(nonce >> 8U);
    ozos_shake256(buf, sizeof(buf), in, sizeof(in));

    for (size_t i = 0U; i < 128U; ++i) {
        const uint8_t* p = buf + 5U * i;
        uint32_t v0 = (uint32_t)p[0] | ((uint32_t)p[1] << 8U) | (((uint32_t)p[2] & 0x0FU) << 16U);
        uint32_t v1 = ((uint32_t)p[2] >> 4U) | ((uint32_t)p[3] << 4U) | ((uint32_t)p[4] << 12U);
        y[2U * i]      = OZOS_MLDSA_GAMMA1 - (int32_t)v0;
        y[2U * i + 1U] = OZOS_MLDSA_GAMMA1 - (int32_t)v1;
    }
    ozos_crypto_zeroize(in, sizeof(in));
    ozos_crypto_zeroize(buf, sizeof(buf));
}

static inline void ozos_mldsa_pack_t1(uint8_t out[320], const int32_t t1[256]) {
    for (size_t i = 0U; i < 64U; ++i) {
        uint32_t c0 = (uint32_t)t1[4U * i + 0U];
        uint32_t c1 = (uint32_t)t1[4U * i + 1U];
        uint32_t c2 = (uint32_t)t1[4U * i + 2U];
        uint32_t c3 = (uint32_t)t1[4U * i + 3U];
        out[5U * i + 0U] = (uint8_t)(c0 & 0xFFU);
        out[5U * i + 1U] = (uint8_t)((c0 >> 8U) | ((c1 & 0x3FU) << 2U));
        out[5U * i + 2U] = (uint8_t)((c1 >> 6U) | ((c2 & 0x0FU) << 4U));
        out[5U * i + 3U] = (uint8_t)((c2 >> 4U) | ((c3 & 0x03U) << 6U));
        out[5U * i + 4U] = (uint8_t)(c3 >> 2U);
    }
}

static inline void ozos_mldsa_unpack_t1(int32_t t1[256], const uint8_t in[320]) {
    for (size_t i = 0U; i < 64U; ++i) {
        const uint8_t* b = in + 5U * i;
        t1[4U * i + 0U] = (int32_t)((uint32_t)b[0] | (((uint32_t)b[1] & 0x03U) << 8U));
        t1[4U * i + 1U] = (int32_t)(((uint32_t)b[1] >> 2U) | (((uint32_t)b[2] & 0x0FU) << 6U));
        t1[4U * i + 2U] = (int32_t)(((uint32_t)b[2] >> 4U) | (((uint32_t)b[3] & 0x3FU) << 4U));
        t1[4U * i + 3U] = (int32_t)(((uint32_t)b[3] >> 6U) | ((uint32_t)b[4] << 2U));
    }
}

static inline void ozos_mldsa_pack_t0(uint8_t out[416], const int32_t t0[256]) {
    const int32_t base = 1 << (OZOS_MLDSA_D - 1);
    for (size_t i = 0U; i < 32U; ++i) {
        uint32_t t[8];
        for (size_t k = 0U; k < 8U; ++k) {
            t[k] = (uint32_t)(base - t0[8U * i + k]);
        }
        uint8_t* r = out + 13U * i;
        r[0]  = (uint8_t)(t[0]);
        r[1]  = (uint8_t)((t[0] >> 8U) | (t[1] << 5U));
        r[2]  = (uint8_t)(t[1] >> 3U);
        r[3]  = (uint8_t)((t[1] >> 11U) | (t[2] << 2U));
        r[4]  = (uint8_t)((t[2] >> 6U) | (t[3] << 7U));
        r[5]  = (uint8_t)(t[3] >> 1U);
        r[6]  = (uint8_t)((t[3] >> 9U) | (t[4] << 4U));
        r[7]  = (uint8_t)(t[4] >> 4U);
        r[8]  = (uint8_t)((t[4] >> 12U) | (t[5] << 1U));
        r[9]  = (uint8_t)((t[5] >> 7U) | (t[6] << 6U));
        r[10] = (uint8_t)(t[6] >> 2U);
        r[11] = (uint8_t)((t[6] >> 10U) | (t[7] << 3U));
        r[12] = (uint8_t)(t[7] >> 5U);
    }
}

static inline void ozos_mldsa_unpack_t0(int32_t t0[256], const uint8_t in[416]) {
    const int32_t base = 1 << (OZOS_MLDSA_D - 1);
    for (size_t i = 0U; i < 32U; ++i) {
        const uint8_t* a = in + 13U * i;
        uint32_t v[8];
        v[0] = ((uint32_t)a[0] | ((uint32_t)a[1] << 8U)) & 0x1FFFU;
        v[1] = (((uint32_t)a[1] >> 5U) | ((uint32_t)a[2] << 3U) | ((uint32_t)a[3] << 11U)) & 0x1FFFU;
        v[2] = (((uint32_t)a[3] >> 2U) | ((uint32_t)a[4] << 6U)) & 0x1FFFU;
        v[3] = (((uint32_t)a[4] >> 7U) | ((uint32_t)a[5] << 1U) | ((uint32_t)a[6] << 9U)) & 0x1FFFU;
        v[4] = (((uint32_t)a[6] >> 4U) | ((uint32_t)a[7] << 4U) | ((uint32_t)a[8] << 12U)) & 0x1FFFU;
        v[5] = (((uint32_t)a[8] >> 1U) | ((uint32_t)a[9] << 7U)) & 0x1FFFU;
        v[6] = (((uint32_t)a[9] >> 6U) | ((uint32_t)a[10] << 2U) | ((uint32_t)a[11] << 10U)) & 0x1FFFU;
        v[7] = (((uint32_t)a[11] >> 3U) | ((uint32_t)a[12] << 5U)) & 0x1FFFU;
        for (size_t k = 0U; k < 8U; ++k) {
            t0[8U * i + k] = base - (int32_t)v[k];
        }
    }
}

static inline void ozos_mldsa_pack_eta4(uint8_t out[128], const int32_t a[256]) {
    for (size_t i = 0U; i < 128U; ++i) {
        uint32_t t0 = (uint32_t)(OZOS_MLDSA_ETA - ozos_mldsa_center_q(a[2U * i]));
        uint32_t t1 = (uint32_t)(OZOS_MLDSA_ETA - ozos_mldsa_center_q(a[2U * i + 1U]));
        out[i] = (uint8_t)((t0 & 0x0FU) | ((t1 & 0x0FU) << 4U));
    }
}

static inline void ozos_mldsa_unpack_eta4(int32_t a[256], const uint8_t in[128]) {
    for (size_t i = 0U; i < 128U; ++i) {
        a[2U * i]      = (int32_t)OZOS_MLDSA_ETA - (int32_t)(in[i] & 0x0FU);
        a[2U * i + 1U] = (int32_t)OZOS_MLDSA_ETA - (int32_t)(in[i] >> 4U);
    }
}

static inline void ozos_mldsa_pack_z(uint8_t out[640], const int32_t z[256]) {
    for (size_t i = 0U; i < 128U; ++i) {
        uint32_t t0 = (uint32_t)(OZOS_MLDSA_GAMMA1 - ozos_mldsa_center_q(z[2U * i]));
        uint32_t t1 = (uint32_t)(OZOS_MLDSA_GAMMA1 - ozos_mldsa_center_q(z[2U * i + 1U]));
        uint8_t* r = out + 5U * i;
        r[0] = (uint8_t)(t0);
        r[1] = (uint8_t)(t0 >> 8U);
        r[2] = (uint8_t)((t0 >> 16U) | ((t1 & 0x0FU) << 4U));
        r[3] = (uint8_t)(t1 >> 4U);
        r[4] = (uint8_t)(t1 >> 12U);
    }
}

static inline void ozos_mldsa_unpack_z(int32_t z[256], const uint8_t in[640]) {
    for (size_t i = 0U; i < 128U; ++i) {
        const uint8_t* p = in + 5U * i;
        uint32_t v0 = (uint32_t)p[0] | ((uint32_t)p[1] << 8U) | (((uint32_t)p[2] & 0x0FU) << 16U);
        uint32_t v1 = ((uint32_t)p[2] >> 4U) | ((uint32_t)p[3] << 4U) | ((uint32_t)p[4] << 12U);
        z[2U * i]      = OZOS_MLDSA_GAMMA1 - (int32_t)v0;
        z[2U * i + 1U] = OZOS_MLDSA_GAMMA1 - (int32_t)v1;
    }
}

/* ========================================================================= */
/*   FIPS 204 INTERNAL & PURE EXTERNAL API (KEYGEN / SIGN / VERIFY)          */
/* ========================================================================= */

static inline int ozos_mldsa65_keygen_internal(
    const uint8_t seed[32],
    uint8_t pk[OZOS_MLDSA_65_PK_BYTES],
    uint8_t sk[OZOS_MLDSA_65_SK_BYTES]
) {
    if (!seed || !pk || !sk) return -1;

    uint8_t seed_kl[34];
    uint8_t buf128[128];
    memcpy(seed_kl, seed, 32U);
    seed_kl[32] = OZOS_MLDSA_K;
    seed_kl[33] = OZOS_MLDSA_L;
    ozos_shake256(buf128, sizeof(buf128), seed_kl, sizeof(seed_kl));

    const uint8_t* rho  = buf128;
    const uint8_t* rhop = buf128 + 32U;
    const uint8_t* key_k = buf128 + 96U;

    int32_t s1_hat[OZOS_MLDSA_L][256];
    int32_t tmp_poly[256];

    memcpy(sk, rho, 32U);
    memcpy(sk + 32U, key_k, 32U);
    uint8_t* sk_s1 = sk + 128U;
    uint8_t* sk_s2 = sk + 128U + OZOS_MLDSA_L * 128U;
    uint8_t* sk_t0 = sk_s2 + OZOS_MLDSA_K * 128U;

    for (uint16_t j = 0U; j < OZOS_MLDSA_L; ++j) {
        ozos_mldsa_sample_eta4(tmp_poly, rhop, j);
        ozos_mldsa_pack_eta4(sk_s1 + (size_t)j * 128U, tmp_poly);
        memcpy(s1_hat[j], tmp_poly, sizeof(tmp_poly));
        ozos_mldsa_ntt(s1_hat[j]);
    }

    memcpy(pk, rho, 32U);
    int32_t a_ij[256];
    int32_t t_hat[256];
    int32_t t0_poly[256];

    for (uint8_t i = 0U; i < OZOS_MLDSA_K; ++i) {
        memset(t_hat, 0, sizeof(t_hat));
        for (uint8_t j = 0U; j < OZOS_MLDSA_L; ++j) {
            ozos_mldsa_sample_a_ij(a_ij, rho, i, j);
            ozos_mldsa_poly_pointwise_acc_mont(t_hat, a_ij, s1_hat[j]);
        }
        ozos_mldsa_invntt_tomont(t_hat);

        ozos_mldsa_sample_eta4(tmp_poly, rhop, (uint16_t)(OZOS_MLDSA_L + i));
        ozos_mldsa_pack_eta4(sk_s2 + (size_t)i * 128U, tmp_poly);

        for (size_t n = 0U; n < 256U; ++n) {
            int32_t t_val = ozos_mldsa_freeze(t_hat[n] + tmp_poly[n]);
            tmp_poly[n] = ozos_mldsa_power2round(t_val, &t0_poly[n]);
        }
        ozos_mldsa_pack_t1(pk + 32U + (size_t)i * 320U, tmp_poly);
        ozos_mldsa_pack_t0(sk_t0 + (size_t)i * 416U, t0_poly);
    }

    /* tr = H(pk, 64) stored at sk[64..127] */
    ozos_shake256(sk + 64U, 64U, pk, OZOS_MLDSA_65_PK_BYTES);

    ozos_crypto_zeroize(seed_kl, sizeof(seed_kl));
    ozos_crypto_zeroize(buf128, sizeof(buf128));
    ozos_crypto_zeroize(s1_hat, sizeof(s1_hat));
    ozos_crypto_zeroize(tmp_poly, sizeof(tmp_poly));
    ozos_crypto_zeroize(a_ij, sizeof(a_ij));
    ozos_crypto_zeroize(t_hat, sizeof(t_hat));
    ozos_crypto_zeroize(t0_poly, sizeof(t0_poly));
    return 0;
}

static inline int ozos_mldsa65_sign_internal(
    const uint8_t sk[OZOS_MLDSA_65_SK_BYTES],
    const uint8_t mu[64],
    const uint8_t rnd[32],
    uint8_t sig[OZOS_MLDSA_65_SIG_BYTES]
) {
    if (!sk || !mu || !rnd || !sig) return -1;

    const uint8_t* rho   = sk;
    const uint8_t* key_k = sk + 32U;
    const uint8_t* sk_s1 = sk + 128U;
    const uint8_t* sk_s2 = sk_s1 + OZOS_MLDSA_L * 128U;
    const uint8_t* sk_t0 = sk_s2 + OZOS_MLDSA_K * 128U;

    uint8_t rhop_in[32 + 32 + 64];
    uint8_t rhop[64];
    memcpy(rhop_in, key_k, 32U);
    memcpy(rhop_in + 32U, rnd, 32U);
    memcpy(rhop_in + 64U, mu, 64U);
    ozos_shake256(rhop, sizeof(rhop), rhop_in, sizeof(rhop_in));

    int32_t s1_hat[OZOS_MLDSA_L][256];
    int32_t s2_hat[OZOS_MLDSA_K][256];
    int32_t t0_hat[OZOS_MLDSA_K][256];

    for (size_t j = 0U; j < OZOS_MLDSA_L; ++j) {
        ozos_mldsa_unpack_eta4(s1_hat[j], sk_s1 + j * 128U);
        ozos_mldsa_ntt(s1_hat[j]);
    }
    for (size_t i = 0U; i < OZOS_MLDSA_K; ++i) {
        ozos_mldsa_unpack_eta4(s2_hat[i], sk_s2 + i * 128U);
        ozos_mldsa_ntt(s2_hat[i]);
        ozos_mldsa_unpack_t0(t0_hat[i], sk_t0 + i * 416U);
        ozos_mldsa_ntt(t0_hat[i]);
    }

    int32_t y_vec[OZOS_MLDSA_L][256];
    int32_t y_hat[OZOS_MLDSA_L][256];
    int32_t w_vec[OZOS_MLDSA_K][256];
    int32_t z_vec[OZOS_MLDSA_L][256];
    int32_t tmp_poly[256];
    int32_t a_ij[256];
    int32_t c_hat[256];
    uint8_t w1_packed[OZOS_MLDSA_K * 128U];
    uint8_t c_tilde[OZOS_MLDSA_CTILDE_BYTES];
    uint8_t hints[OZOS_MLDSA_OMEGA + OZOS_MLDSA_K];

    int rc = -2;
    for (uint32_t iter = 0U; iter < 1000U; ++iter) {
        uint16_t kappa = (uint16_t)(iter * OZOS_MLDSA_L);
        for (uint16_t j = 0U; j < OZOS_MLDSA_L; ++j) {
            ozos_mldsa_expand_mask_poly(y_vec[j], rhop, (uint16_t)(kappa + j));
            memcpy(y_hat[j], y_vec[j], sizeof(y_vec[j]));
            ozos_mldsa_ntt(y_hat[j]);
        }

        for (uint8_t i = 0U; i < OZOS_MLDSA_K; ++i) {
            memset(w_vec[i], 0, sizeof(w_vec[i]));
            for (uint8_t j = 0U; j < OZOS_MLDSA_L; ++j) {
                ozos_mldsa_sample_a_ij(a_ij, rho, i, j);
                ozos_mldsa_poly_pointwise_acc_mont(w_vec[i], a_ij, y_hat[j]);
            }
            ozos_mldsa_invntt_tomont(w_vec[i]);
            for (size_t n = 0U; n < 128U; ++n) {
                int32_t dummy0 = 0;
                int32_t w1_0 = ozos_mldsa_decompose(w_vec[i][2U * n], &dummy0);
                int32_t w1_1 = ozos_mldsa_decompose(w_vec[i][2U * n + 1U], &dummy0);
                w1_packed[(size_t)i * 128U + n] = (uint8_t)((w1_0 & 0x0F) | ((w1_1 & 0x0F) << 4));
            }
        }

        ozos_keccak_sponge_t c_ctx;
        ozos_keccak_init(&c_ctx, 136U);
        ozos_keccak_absorb(&c_ctx, mu, 64U);
        ozos_keccak_absorb(&c_ctx, w1_packed, sizeof(w1_packed));
        ozos_keccak_finalize(&c_ctx, 0x1FU);
        ozos_keccak_squeeze(&c_ctx, c_tilde, sizeof(c_tilde));
        ozos_crypto_zeroize(&c_ctx, sizeof(c_ctx));

        ozos_mldsa_sample_in_ball(c_hat, c_tilde);
        ozos_mldsa_ntt(c_hat);

        int rejected = 0;
        for (size_t j = 0U; j < OZOS_MLDSA_L; ++j) {
            memset(tmp_poly, 0, sizeof(tmp_poly));
            ozos_mldsa_poly_pointwise_acc_mont(tmp_poly, c_hat, s1_hat[j]);
            ozos_mldsa_invntt_tomont(tmp_poly);
            for (size_t n = 0U; n < 256U; ++n) {
                z_vec[j][n] = y_vec[j][n] + tmp_poly[n];
            }
            if (ozos_mldsa_poly_chknorm(z_vec[j], OZOS_MLDSA_GAMMA1 - OZOS_MLDSA_BETA) != 0) {
                rejected = 1;
                break;
            }
        }
        if (rejected) continue;

        /* Check r0 = LowBits(w - cs2) and ct0, and construct hints simultaneously */
        memset(hints, 0, sizeof(hints));
        size_t hint_ones = 0U;
        for (size_t i = 0U; i < OZOS_MLDSA_K; ++i) {
            int32_t w_minus_cs2[256];
            memset(tmp_poly, 0, sizeof(tmp_poly));
            ozos_mldsa_poly_pointwise_acc_mont(tmp_poly, c_hat, s2_hat[i]);
            ozos_mldsa_invntt_tomont(tmp_poly);
            for (size_t n = 0U; n < 256U; ++n) {
                w_minus_cs2[n] = w_vec[i][n] - tmp_poly[n];
                int32_t r0 = 0;
                (void)ozos_mldsa_decompose(w_minus_cs2[n], &r0);
                tmp_poly[n] = r0;
            }
            if (ozos_mldsa_poly_chknorm(tmp_poly, OZOS_MLDSA_GAMMA2 - OZOS_MLDSA_BETA) != 0) {
                rejected = 1;
                break;
            }

            memset(tmp_poly, 0, sizeof(tmp_poly));
            ozos_mldsa_poly_pointwise_acc_mont(tmp_poly, c_hat, t0_hat[i]);
            ozos_mldsa_invntt_tomont(tmp_poly);
            if (ozos_mldsa_poly_chknorm(tmp_poly, OZOS_MLDSA_GAMMA2) != 0) {
                rejected = 1;
                break;
            }

            for (size_t n = 0U; n < 256U; ++n) {
                int32_t shifted = w_minus_cs2[n] + tmp_poly[n];
                unsigned int hb = ozos_mldsa_make_hint(-tmp_poly[n], shifted);
                if (hb != 0U) {
                    if (hint_ones >= OZOS_MLDSA_OMEGA) {
                        rejected = 1;
                        break;
                    }
                    hints[hint_ones++] = (uint8_t)n;
                }
            }
            if (rejected) break;
            hints[OZOS_MLDSA_OMEGA + i] = (uint8_t)hint_ones;
        }
        if (rejected) continue;

        memcpy(sig, c_tilde, OZOS_MLDSA_CTILDE_BYTES);
        for (size_t j = 0U; j < OZOS_MLDSA_L; ++j) {
            ozos_mldsa_pack_z(sig + OZOS_MLDSA_CTILDE_BYTES + j * 640U, z_vec[j]);
        }
        memcpy(sig + OZOS_MLDSA_CTILDE_BYTES + OZOS_MLDSA_L * 640U, hints, sizeof(hints));
        rc = 0;
        break;
    }

    ozos_crypto_zeroize(rhop_in, sizeof(rhop_in));
    ozos_crypto_zeroize(rhop, sizeof(rhop));
    ozos_crypto_zeroize(s1_hat, sizeof(s1_hat));
    ozos_crypto_zeroize(s2_hat, sizeof(s2_hat));
    ozos_crypto_zeroize(t0_hat, sizeof(t0_hat));
    ozos_crypto_zeroize(y_vec, sizeof(y_vec));
    ozos_crypto_zeroize(y_hat, sizeof(y_hat));
    ozos_crypto_zeroize(w_vec, sizeof(w_vec));
    ozos_crypto_zeroize(z_vec, sizeof(z_vec));
    ozos_crypto_zeroize(tmp_poly, sizeof(tmp_poly));
    ozos_crypto_zeroize(a_ij, sizeof(a_ij));
    ozos_crypto_zeroize(c_hat, sizeof(c_hat));
    ozos_crypto_zeroize(w1_packed, sizeof(w1_packed));
    return rc;
}

static inline int ozos_mldsa65_verify_internal(
    const uint8_t pk[OZOS_MLDSA_65_PK_BYTES],
    const uint8_t mu[64],
    const uint8_t sig[OZOS_MLDSA_65_SIG_BYTES]
) {
    if (!pk || !mu || !sig) return -1;

    const uint8_t* rho     = pk;
    const uint8_t* c_tilde = sig;
    const uint8_t* z_bytes = sig + OZOS_MLDSA_CTILDE_BYTES;
    const uint8_t* h_bytes = z_bytes + OZOS_MLDSA_L * 640U;

    /* Validate hint encoding strictly per FIPS 204 Algorithm 21 */
    uint8_t hints[OZOS_MLDSA_K][256];
    memset(hints, 0, sizeof(hints));
    size_t idx = 0U;
    for (size_t i = 0U; i < OZOS_MLDSA_K; ++i) {
        size_t limit = h_bytes[OZOS_MLDSA_OMEGA + i];
        if (limit < idx || limit > OZOS_MLDSA_OMEGA) return -2;
        int prev = -1;
        for (size_t j = idx; j < limit; ++j) {
            int pos = h_bytes[j];
            if (pos <= prev) return -2;
            hints[i][pos] = 1U;
            prev = pos;
        }
        idx = limit;
    }
    for (size_t j = idx; j < OZOS_MLDSA_OMEGA; ++j) {
        if (h_bytes[j] != 0U) return -2;
    }

    int32_t z_hat[OZOS_MLDSA_L][256];
    for (size_t j = 0U; j < OZOS_MLDSA_L; ++j) {
        ozos_mldsa_unpack_z(z_hat[j], z_bytes + j * 640U);
        if (ozos_mldsa_poly_chknorm(z_hat[j], OZOS_MLDSA_GAMMA1 - OZOS_MLDSA_BETA) != 0) {
            return -3;
        }
        ozos_mldsa_ntt(z_hat[j]);
    }

    int32_t c_hat[256];
    ozos_mldsa_sample_in_ball(c_hat, c_tilde);
    ozos_mldsa_ntt(c_hat);

    int32_t a_ij[256];
    int32_t t1_i[256];
    int32_t w_i[256];
    uint8_t w1_packed[OZOS_MLDSA_K * 128U];

    for (uint8_t i = 0U; i < OZOS_MLDSA_K; ++i) {
        memset(w_i, 0, sizeof(w_i));
        for (uint8_t j = 0U; j < OZOS_MLDSA_L; ++j) {
            ozos_mldsa_sample_a_ij(a_ij, rho, i, j);
            ozos_mldsa_poly_pointwise_acc_mont(w_i, a_ij, z_hat[j]);
        }
        ozos_mldsa_unpack_t1(t1_i, pk + 32U + (size_t)i * 320U);
        for (size_t n = 0U; n < 256U; ++n) {
            t1_i[n] = (int32_t)((uint32_t)t1_i[n] << OZOS_MLDSA_D);
        }
        ozos_mldsa_ntt(t1_i);
        for (size_t n = 0U; n < 256U; ++n) {
            w_i[n] -= ozos_mldsa_montgomery_reduce((int64_t)c_hat[n] * (int64_t)t1_i[n]);
        }
        ozos_mldsa_invntt_tomont(w_i);

        for (size_t n = 0U; n < 128U; ++n) {
            int32_t w1_0 = ozos_mldsa_use_hint(w_i[2U * n], hints[i][2U * n]);
            int32_t w1_1 = ozos_mldsa_use_hint(w_i[2U * n + 1U], hints[i][2U * n + 1U]);
            w1_packed[(size_t)i * 128U + n] = (uint8_t)((w1_0 & 0x0F) | ((w1_1 & 0x0F) << 4));
        }
    }

    uint8_t c_tilde_prime[OZOS_MLDSA_CTILDE_BYTES];
    ozos_keccak_sponge_t v_ctx;
    ozos_keccak_init(&v_ctx, 136U);
    ozos_keccak_absorb(&v_ctx, mu, 64U);
    ozos_keccak_absorb(&v_ctx, w1_packed, sizeof(w1_packed));
    ozos_keccak_finalize(&v_ctx, 0x1FU);
    ozos_keccak_squeeze(&v_ctx, c_tilde_prime, sizeof(c_tilde_prime));

    uint8_t diff = 0U;
    for (size_t i = 0U; i < OZOS_MLDSA_CTILDE_BYTES; ++i) {
        diff |= (uint8_t)(c_tilde[i] ^ c_tilde_prime[i]);
    }
    return (diff == 0U) ? 0 : -4;
}

static inline void ozos_mldsa65_compute_pure_mu(
    uint8_t mu_out[64],
    const uint8_t tr[64],
    const uint8_t* msg,
    size_t msg_len,
    const uint8_t* ctx,
    size_t ctx_len
) {
    uint8_t domain_hdr[2];
    domain_hdr[0] = 0x00U; /* pure ML-DSA mode */
    domain_hdr[1] = (uint8_t)ctx_len;

    ozos_keccak_sponge_t kctx;
    ozos_keccak_init(&kctx, 136U);
    ozos_keccak_absorb(&kctx, tr, 64U);
    ozos_keccak_absorb(&kctx, domain_hdr, 2U);
    if (ctx && ctx_len > 0U) ozos_keccak_absorb(&kctx, ctx, ctx_len);
    if (msg && msg_len > 0U) ozos_keccak_absorb(&kctx, msg, msg_len);
    ozos_keccak_finalize(&kctx, 0x1FU);
    ozos_keccak_squeeze(&kctx, mu_out, 64U);
    ozos_crypto_zeroize(&kctx, sizeof(kctx));
}

static inline int ozos_mldsa65_keygen(
    uint8_t pk[OZOS_MLDSA_65_PK_BYTES],
    uint8_t sk[OZOS_MLDSA_65_SK_BYTES],
    const uint8_t seed_opt[32]
) {
    if (!pk || !sk) return -1;
    uint8_t local_seed[32];
    if (seed_opt) {
        memcpy(local_seed, seed_opt, 32U);
    } else {
        randombytes(local_seed, 32ULL);
    }
    int rc = ozos_mldsa65_keygen_internal(local_seed, pk, sk);
    ozos_crypto_zeroize(local_seed, sizeof(local_seed));
    return rc;
}

static inline int ozos_mldsa65_sign(
    uint8_t sig_out[OZOS_MLDSA_65_SIG_BYTES],
    const uint8_t* msg,
    size_t msg_len,
    const uint8_t* ctx,
    size_t ctx_len,
    const uint8_t sk[OZOS_MLDSA_65_SK_BYTES],
    bool deterministic
) {
    if (!sig_out || !sk || ctx_len > 255U || (!msg && msg_len > 0U) || (!ctx && ctx_len > 0U)) {
        return -1;
    }
    uint8_t mu[64];
    uint8_t rnd[32];
    ozos_mldsa65_compute_pure_mu(mu, sk + 64U, msg, msg_len, ctx, ctx_len);
    if (deterministic) {
        memset(rnd, 0, sizeof(rnd));
    } else {
        randombytes(rnd, 32ULL);
    }
    int rc = ozos_mldsa65_sign_internal(sk, mu, rnd, sig_out);
    ozos_crypto_zeroize(mu, sizeof(mu));
    ozos_crypto_zeroize(rnd, sizeof(rnd));
    return rc;
}

static inline bool ozos_mldsa65_verify(
    const uint8_t sig[OZOS_MLDSA_65_SIG_BYTES],
    const uint8_t* msg,
    size_t msg_len,
    const uint8_t* ctx,
    size_t ctx_len,
    const uint8_t pk[OZOS_MLDSA_65_PK_BYTES]
) {
    if (!sig || !pk || ctx_len > 255U || (!msg && msg_len > 0U) || (!ctx && ctx_len > 0U)) {
        return false;
    }
    uint8_t tr[64];
    uint8_t mu[64];
    ozos_shake256(tr, 64U, pk, OZOS_MLDSA_65_PK_BYTES);
    ozos_mldsa65_compute_pure_mu(mu, tr, msg, msg_len, ctx, ctx_len);
    return (ozos_mldsa65_verify_internal(pk, mu, sig) == 0);
}

#endif /* OZOS_MLDSA65_H */
