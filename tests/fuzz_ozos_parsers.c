/**
 * O.Z.O.S // LLVM libFuzzer Parser & Post-Quantum Hardness Target
 * Covers:
 *   0 - ozos_packet_validate_ex (Sub-Profile A1/A2 packet parsing & anti-poisoning)
 *   1 - ozos_vault_extract_archive (Header magic, count, plen, traversal & uint64 bounds)
 *   2 - ozos_watermark_bmp_buffer_ex (24/32-bpp stride, bounds & 2D-DCT embedding)
 *   3 - ozos_mlkem768_encaps_internal (NIST FIPS 203 Section 7.2 modulus validation)
 *   4 - ozos_mlkem768_decaps_internal (NIST FIPS 203 Fujisaki-Okamoto implicit rejection)
 *   5 - ozos_mldsa65_verify_internal (NIST FIPS 204 Algorithm 21 hint/norm verifier)
 *   6 - ozos_brainseed_restore + ozos_lora_frame_unpack (BIP-39 tokenizer & LoRa RX)
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OZOS_FUZZ_QUIET 1
#include "../core/libozos.h"
#include "../core/ozos_crypto.h"
#include "../core/ozos_brainseed.h"
#include "../core/ozos_omni_phy.h"
#include "../core/ozos_pqc_hybrid.h"
#include "../core/ozos_data_at_rest.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (!data || size < 2U) return 0;

    uint8_t selector = data[0] % 7U;
    const uint8_t* payload = data + 1U;
    size_t len = size - 1U;

    switch (selector) {
        case 0U: {
            ozos_packet80_t hdr;
            (void)ozos_packet_validate_ex(payload, len, 0ULL, 0ULL, &hdr);
            break;
        }
        case 1U: {
            uint32_t fc = 0U;
#ifdef _WIN32
            (void)ozos_vault_extract_archive(payload, len, "NUL\\ozos_fuzz", &fc);
#else
            (void)ozos_vault_extract_archive(payload, len, "/dev/null/ozos_fuzz", &fc);
#endif
            break;
        }
        case 2U: {
            if (len >= 5U) {
                int w = (int)((int8_t)payload[0]);
                int h = (int)((int8_t)payload[1]);
                int stride = (int)payload[2] * 4;
                int bpp = (int)(payload[3] % 6U);
                size_t p_len = (size_t)payload[4];
                if (w > 0 && w <= 64 && h > 0 && h <= 64 && stride >= 0 && stride <= 512 &&
                    (bpp == 3 || bpp == 4) && stride >= w * bpp && len >= 6U) {
                    size_t alloc_sz = (size_t)h * (size_t)stride;
                    uint8_t* buf = (uint8_t*)calloc(alloc_sz, 1U);
                    if (buf) {
                        size_t avail = len - 5U;
                        size_t use_len = (p_len < avail) ? p_len : avail;
                        if (use_len > 128U) use_len = 128U;
                        (void)ozos_watermark_bmp_buffer_ex(buf, w, h, stride, bpp, payload + 5U, use_len);
                        free(buf);
                    }
                } else if (w < 8 || h < 8 || (bpp != 3 && bpp != 4) || stride < w * bpp) {
                    uint8_t dummy[64] = {0};
                    (void)ozos_watermark_bmp_buffer_ex(dummy, w, h, stride, bpp, payload, (len > 32U) ? 32U : len);
                }
            }
            break;
        }
        case 3U: {
            uint8_t ek[OZOS_MLKEM_768_PK_BYTES];
            uint8_t m[32];
            uint8_t ct[OZOS_MLKEM_768_CIPHERTEXT_BYTES];
            uint8_t ss[32];
            memset(ek, 0, sizeof(ek));
            memset(m, 0xA5, sizeof(m));
            size_t copy_n = (len < sizeof(ek)) ? len : sizeof(ek);
            memcpy(ek, payload, copy_n);
            (void)ozos_mlkem768_encaps_internal(ek, m, ct, ss);
            break;
        }
        case 4U: {
            uint8_t dk[OZOS_MLKEM_768_SK_BYTES];
            uint8_t ct[OZOS_MLKEM_768_CIPHERTEXT_BYTES];
            uint8_t ss[32];
            memset(dk, 0, sizeof(dk));
            memset(ct, 0, sizeof(ct));
            size_t half = len / 2U;
            size_t c1 = (half < sizeof(dk)) ? half : sizeof(dk);
            size_t c2 = ((len - half) < sizeof(ct)) ? (len - half) : sizeof(ct);
            if (c1 > 0U) memcpy(dk, payload, c1);
            if (c2 > 0U) memcpy(ct, payload + half, c2);
            (void)ozos_mlkem768_decaps_internal(dk, ct, ss);
            break;
        }
        case 5U: {
            uint8_t pk[OZOS_MLDSA_65_PK_BYTES];
            uint8_t mu[64];
            uint8_t sig[OZOS_MLDSA_65_SIG_BYTES];
            memset(pk, 0, sizeof(pk));
            memset(mu, 0x5A, sizeof(mu));
            memset(sig, 0, sizeof(sig));
            if (len >= sizeof(sig)) {
                memcpy(sig, payload, sizeof(sig));
                size_t rem = len - sizeof(sig);
                if (rem > 0U) memcpy(pk, payload + sizeof(sig), (rem < sizeof(pk)) ? rem : sizeof(pk));
            } else {
                memcpy(sig, payload, len);
            }
            (void)ozos_mldsa65_verify_internal(pk, mu, sig);
            break;
        }
        case 6U:
        default: {
            char mnem[256];
            size_t mlen = (len < sizeof(mnem) - 1U) ? len : (sizeof(mnem) - 1U);
            memcpy(mnem, payload, mlen);
            mnem[mlen] = '\0';
            uint8_t seed_out[32];
            (void)ozos_brainseed_restore(seed_out, mnem, "");

            uint8_t lora_out[80];
            size_t lora_len = 0U;
            (void)ozos_lora_frame_unpack(payload, len, lora_out, &lora_len);
            break;
        }
    }
    return 0;
}

#ifdef OZOS_FUZZ_STANDALONE
int main(void) {
    uint8_t buf[8192];
    for (uint8_t sel = 0U; sel < 7U; ++sel) {
        memset(buf, (int)(sel * 37U + 13U), sizeof(buf));
        buf[0] = sel;
        LLVMFuzzerTestOneInput(buf, 4096U);
        LLVMFuzzerTestOneInput(buf, 12U);
    }
    printf("[OK] fuzz_ozos_parsers standalone smoke check passed.\n");
    return 0;
}
#endif
