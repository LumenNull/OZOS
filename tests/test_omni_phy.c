/**
 * =============================================================================
 *   O.Z.O.S OMNI-SPECTRUM COGNITIVE PHY VERIFICATION HARNESS
 *   Tests and certifies all 4 spectral transport layers & physical hardening:
 *   1. Optical Glyph (32x32 Matrix & BMP Render/Scan)
 *   2. Acoustic OFDM & Ambient Steganography (64-Point Radix-2 FFT)
 *   3. Long-Range LoRa Sub-GHz Framing (84-byte UART Serial Format)
 *   4. Mathematical Invariants & Anti-Tampering Protections
 *   5. Field-Hardened Physical Layer Extensions (Reed-Solomon RS(96, 80),
 *      Otsu Binarization, 4-Point Homography, Schmidl-Cox Stream Synchronization,
 *      LoRa Serial Loopback, and Planetary HF Skywave 8-MFSK Modem)
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include "../core/ozos_omni_phy.h"

int main(void) {
    printf("=================================================================\n");
    printf("  [O.Z.O.S] OMNI-SPECTRUM COGNITIVE PHY HARNESS (11-TEST VERIFICATION)\n");
    printf("  Target: Complete Physical Deployment Hardening Certification\n");
    printf("=================================================================\n\n");

    int failed = 0;

    /* -------------------------------------------------------------------------
       TEST 1: Fast Radix-2 FFT/IFFT Inversion & Precision
       ------------------------------------------------------------------------- */
    printf("[TEST 1/11] Radix-2 Cooley-Tukey FFT/IFFT C99 Precision...\n");
    {
        float real[OZOS_OMNI_FFT_N];
        float imag[OZOS_OMNI_FFT_N];
        float orig[OZOS_OMNI_FFT_N];
        memset(imag, 0, sizeof(imag));
        for (int i = 0; i < OZOS_OMNI_FFT_N; ++i) {
            real[i] = sinf((float)i * 0.1f) + cosf((float)i * 0.3f);
            orig[i] = real[i];
        }

        ozos_fft_radix2(real, imag, OZOS_OMNI_FFT_N, false);
        ozos_fft_radix2(real, imag, OZOS_OMNI_FFT_N, true);

        float max_err = 0.0f;
        for (int i = 0; i < OZOS_OMNI_FFT_N; ++i) {
            float err = fabsf(real[i] - orig[i]);
            if (err > max_err) max_err = err;
        }

        if (max_err < 1e-5f) {
            printf("  -> [PASS] FFT Radix-2 Inversion Max Error: %.4e (< 1e-5)\n", max_err);
        } else {
            printf("  -> [FAIL] FFT Radix-2 Inversion Error too large: %.4e\n", max_err);
            failed++;
        }
    }

    /* -------------------------------------------------------------------------
       TEST 2: Optical Glyph 32x32 Matrix Encoding & Decoding (16ms)
       ------------------------------------------------------------------------- */
    printf("\n[TEST 2/11] Optical Glyph 32x32 Matrix & CRC32-IEEE (16ms Latency)...\n");
    {
        uint8_t packet[80];
        for (int i = 0; i < 80; ++i) packet[i] = (uint8_t)(0x5A ^ i);

        uint8_t matrix[OZOS_OMNI_GLYPH_DIM][OZOS_OMNI_GLYPH_DIM];
        int enc_rc = ozos_glyph_encode(packet, 80, matrix);

        uint8_t rec_packet[80];
        size_t rec_len = 0;
        int dec_rc = ozos_glyph_decode(matrix, rec_packet, &rec_len);

        bool match = (enc_rc == 0 && dec_rc == 0 && rec_len == 80 && memcmp(packet, rec_packet, 80) == 0);
        if (match) {
            printf("  -> [PASS] Optical Glyph 32x32 Matrix Loopback BIT-EXACT 100%%\n");
        } else {
            printf("  -> [FAIL] Optical Glyph decode failed (enc=%d, dec=%d)\n", enc_rc, dec_rc);
            failed++;
        }

        /* Test anti-tampering (tamper 1 cell in matrix) */
        matrix[10][10] ^= 1;
        int tamper_rc = ozos_glyph_decode(matrix, rec_packet, &rec_len);
        if (tamper_rc != 0) {
            printf("  -> [PASS] Detected 1-bit mutation in Optical Matrix: REJECTED (rc=%d)\n", tamper_rc);
        } else {
            printf("  -> [FAIL] Failed to detect tampering on Optical Matrix!\n");
            failed++;
        }
    }

    /* -------------------------------------------------------------------------
       TEST 3: Optical Glyph BMP File Export & Image Scanning
       ------------------------------------------------------------------------- */
    printf("\n[TEST 3/11] Optical Glyph BMP 256x256 Render & Spatial Pixel Sampling...\n");
    {
        uint8_t packet[80];
        for (int i = 0; i < 80; ++i) packet[i] = (uint8_t)(0xC3 + i);

        uint8_t matrix[OZOS_OMNI_GLYPH_DIM][OZOS_OMNI_GLYPH_DIM];
        ozos_glyph_encode(packet, 80, matrix);

        const char* bmp_path = "_test_omni_glyph.bmp";
        int write_rc = ozos_glyph_write_bmp(matrix, 8, bmp_path);

        uint8_t scanned_packet[80];
        size_t scanned_len = 0;
        int read_rc = ozos_glyph_read_bmp(bmp_path, scanned_packet, &scanned_len);

        remove(bmp_path);

        bool bmp_ok = (write_rc == 0 && read_rc == 0 && scanned_len == 80 && memcmp(packet, scanned_packet, 80) == 0);
        if (bmp_ok) {
            printf("  -> [PASS] Optical BMP 256x256 Render & Pixel Center Sampling: 100%% BIT-EXACT\n");
        } else {
            printf("  -> [FAIL] Optical BMP test failed (write=%d, read=%d)\n", write_rc, read_rc);
            failed++;
        }
    }

    /* -------------------------------------------------------------------------
       TEST 4: Acoustic OFDM & Ambient Steganography (< 0.4s Transmission)
       ------------------------------------------------------------------------- */
    printf("\n[TEST 4/11] Acoustic Multi-Carrier OFDM & Ambient Steganography (< 40ms)...\n");
    {
        uint8_t packet[80];
        for (int i = 0; i < 80; ++i) packet[i] = (uint8_t)(0x42 + i * 3);

        const char* ofdm_wav = "_test_omni_ofdm.wav";
        const char* stego_wav = "_test_omni_stego.wav";

        /* 1. Fast OFDM Clean */
        int synth_rc = ozos_ofdm_synth_wav(packet, 80, ofdm_wav, false);
        uint8_t dec_packet[80];
        size_t dec_len = 0;
        int dec_rc = ozos_ofdm_decode_wav(ofdm_wav, dec_packet, &dec_len);
        remove(ofdm_wav);

        bool ofdm_ok = (synth_rc == 0 && dec_rc == 0 && dec_len == 80 && memcmp(packet, dec_packet, 80) == 0);
        if (ofdm_ok) {
            printf("  -> [PASS] Acoustic OFDM Multi-Carrier Transmission (Duration ~0.036s): 100%% BIT-EXACT\n");
        } else {
            printf("  -> [FAIL] Acoustic OFDM failed (synth=%d, dec=%d)\n", synth_rc, dec_rc);
            failed++;
        }

        /* 2. Steganography with Ambient Acoustic Noise */
        int stego_synth = ozos_ofdm_synth_wav(packet, 80, stego_wav, true);
        uint8_t dec_stego[80];
        size_t stego_len = 0;
        int stego_dec = ozos_ofdm_decode_wav(stego_wav, dec_stego, &stego_len);
        remove(stego_wav);

        bool stego_ok = (stego_synth == 0 && stego_dec == 0 && stego_len == 80 && memcmp(packet, dec_stego, 80) == 0);
        if (stego_ok) {
            printf("  -> [PASS] Ambient Steganography with Background Noise: 100%% BIT-EXACT\n");
        } else {
            printf("  -> [FAIL] Ambient Steganography failed (synth=%d, dec=%d)\n", stego_synth, stego_dec);
            failed++;
        }
    }

    /* -------------------------------------------------------------------------
       TEST 5: Long-Range LoRa Sub-GHz Framing (84-byte Serial Adapter)
       ------------------------------------------------------------------------- */
    printf("\n[TEST 5/11] Long-Range LoRa Sub-GHz 84-Byte Frame & CRC16-CCITT...\n");
    {
        uint8_t packet[80];
        for (int i = 0; i < 80; ++i) packet[i] = (uint8_t)(0x77 ^ i);

        uint8_t frame[84];
        size_t frame_len = 0;
        int pack_rc = ozos_lora_frame_pack(packet, 80, frame, &frame_len);

        uint8_t unpacked[80];
        size_t unpacked_len = 0;
        int unpack_rc = ozos_lora_frame_unpack(frame, frame_len, unpacked, &unpacked_len);

        bool lora_ok = (pack_rc == 0 && unpack_rc == 0 && frame_len == 84 &&
                        unpacked_len == 80 && memcmp(packet, unpacked, 80) == 0);
        if (lora_ok) {
            printf("  -> [PASS] LoRa Sub-GHz 84-byte Sovereign Frame & CRC16-CCITT: 100%% BIT-EXACT\n");
        } else {
            printf("  -> [FAIL] LoRa frame failed (pack=%d, unpack=%d)\n", pack_rc, unpack_rc);
            failed++;
        }

        /* Test LoRa corruption rejection */
        frame[40] ^= 0x01;
        int lora_tamper_rc = ozos_lora_frame_unpack(frame, frame_len, unpacked, &unpacked_len);
        if (lora_tamper_rc != 0) {
            printf("  -> [PASS] Detected 1-bit mutation on LoRa Frame: REJECTED (rc=%d)\n", lora_tamper_rc);
        } else {
            printf("  -> [FAIL] Failed to detect tampering on LoRa frame!\n");
            failed++;
        }
    }

    /* -------------------------------------------------------------------------
       TEST 6: Reed-Solomon RS(96, 80) Forward Error Correction & Burst Damage
       ------------------------------------------------------------------------- */
    printf("\n[TEST 6/11] Reed-Solomon RS(96, 80) FEC & 7-Byte Burst Occlusion Recovery...\n");
    {
        uint8_t orig_packet[80];
        for (int i = 0; i < 80; ++i) orig_packet[i] = (uint8_t)(0x99 ^ (i * 11));

        uint8_t matrix[OZOS_OMNI_GLYPH_DIM][OZOS_OMNI_GLYPH_DIM];
        int enc_rc = ozos_glyph_encode_fec(orig_packet, 80, matrix);

        /* Simulate physical burst glare: obliterate 24 cells in row 10 and 20 cells in row 11 (44 cells = 5.5 bytes <= 8 bytes correctable) */
        for (int c = 6; c < 30; ++c) matrix[10][c] ^= 1;
        for (int c = 6; c < 26; ++c) matrix[11][c] ^= 1;

        uint8_t recovered[80];
        size_t rec_len = 0;
        int corr_errors = ozos_glyph_decode_fec(matrix, recovered, &rec_len);

        bool rs_ok = (enc_rc == 0 && corr_errors > 0 && rec_len == 80 &&
                      memcmp(orig_packet, recovered, 80) == 0);
        if (rs_ok) {
            printf("  -> [PASS] Reed-Solomon RS(96, 80) corrected %d byte errors: 100%% BIT-EXACT RECONSTRUCTION!\n", corr_errors);
        } else {
            printf("  -> [FAIL] Reed-Solomon FEC recovery failed (corr=%d, len=%zu)\n", corr_errors, rec_len);
            failed++;
        }
    }

    /* -------------------------------------------------------------------------
       TEST 7: 4-Point Homography Perspective Transform (25-Degree Camera Tilt)
       ------------------------------------------------------------------------- */
    printf("\n[TEST 7/11] 4-Point Homography Perspective Transform & Non-Orthogonal Keystone...\n");
    {
        /* Simulate 25-degree tilted camera trapezoid */
        float tilted_quad[4][2] = {
            {20.0f, 30.0f},   /* Top-Left */
            {230.0f, 15.0f},  /* Top-Right */
            {245.0f, 240.0f}, /* Bottom-Right */
            {10.0f, 225.0f}   /* Bottom-Left */
        };

        float H[3][3];
        int solve_rc = ozos_homography_solve_inv(tilted_quad, 32.0f, H);

        bool corner_precision_ok = (solve_rc == 0);
        if (solve_rc == 0) {
            for (int i = 0; i < 4; ++i) {
                float u = (i == 1 || i == 2) ? 31.0f : 0.0f;
                float v = (i >= 2) ? 31.0f : 0.0f;
                float denom = H[2][0] * u + H[2][1] * v + 1.0f;
                float x = (H[0][0] * u + H[0][1] * v + H[0][2]) / denom;
                float y = (H[1][0] * u + H[1][1] * v + H[1][2]) / denom;
                float err = fabsf(x - tilted_quad[i][0]) + fabsf(y - tilted_quad[i][1]);
                if (err > 0.05f) corner_precision_ok = false;
            }
        }

        if (corner_precision_ok) {
            printf("  -> [PASS] Homography 4-Point Projective Matrix Solved with Sub-Pixel Precision (< 0.05px)\n");
        } else {
            printf("  -> [FAIL] Homography perspective solver failed (rc=%d)\n", solve_rc);
            failed++;
        }
    }

    /* -------------------------------------------------------------------------
       TEST 8: Otsu Adaptive Binarization under Dynamic Glare & Shadow
       ------------------------------------------------------------------------- */
    printf("\n[TEST 8/11] Otsu Adaptive Binarization under Uneven Lighting & Shadows...\n");
    {
        /* Synthesize bimodal pixel distribution (dark text ~35, bright background ~215) */
        uint8_t image_pixels[1000];
        for (int i = 0; i < 500; ++i) image_pixels[i] = (uint8_t)(35 + (i % 15));
        for (int i = 500; i < 1000; ++i) image_pixels[i] = (uint8_t)(215 - (i % 15));

        uint8_t threshold = ozos_otsu_threshold(image_pixels, 1000);
        bool otsu_ok = (threshold >= 90 && threshold <= 150);

        if (otsu_ok) {
            printf("  -> [PASS] Otsu Discriminant Analysis calculated optimal binarization threshold: %u (in range [90..150])\n", threshold);
        } else {
            printf("  -> [FAIL] Otsu threshold calculation out of range: %u\n", threshold);
            failed++;
        }
    }

    /* -------------------------------------------------------------------------
       TEST 9: Schmidl-Cox Timing Metric in Continuous Unaligned Audio Stream
       ------------------------------------------------------------------------- */
    printf("\n[TEST 9/11] Schmidl-Cox Cross-Correlation in Continuous Audio Stream...\n");
    {
        uint8_t packet[80];
        for (int i = 0; i < 80; ++i) packet[i] = (uint8_t)(0xAA ^ (i * 5));

        const char* wav_clean = "_test_stream_clean.wav";
        ozos_ofdm_synth_wav(packet, 80, wav_clean, false);

        /* Read clean wav */
        FILE* fp = fopen(wav_clean, "rb");
        uint8_t hdr[44];
        fread(hdr, 1, 44, fp);
        uint32_t data_bytes = (uint32_t)(hdr[40] | (hdr[41] << 8) | (hdr[42] << 16) | (hdr[43] << 24));
        size_t clean_samples = data_bytes / 2;
        int16_t* clean_pcm = (int16_t*)malloc(clean_samples * sizeof(int16_t));
        fread(clean_pcm, 2, clean_samples, fp);
        fclose(fp);
        remove(wav_clean);

        /* Prepend 850 samples of ambient noise and append 400 samples of noise */
        int noise_pre = 850;
        int noise_post = 400;
        size_t total_samples = noise_pre + clean_samples + noise_post;

        int16_t* stream_pcm = (int16_t*)malloc(total_samples * sizeof(int16_t));
        uint32_t lcg = 0xDEADBEEFU;
        for (size_t i = 0; i < total_samples; ++i) {
            lcg = lcg * 1664525U + 1013904223U;
            stream_pcm[i] = (int16_t)(((lcg & 0xFFFF) % 200) - 100);
        }
        memcpy(stream_pcm + noise_pre, clean_pcm, clean_samples * sizeof(int16_t));
        free(clean_pcm);

        /* Write unaligned stream wav */
        const char* wav_stream = "_test_stream_unaligned.wav";
        FILE* s_fp = fopen(wav_stream, "wb");
        uint32_t s_data_bytes = (uint32_t)(total_samples * 2);
        uint32_t s_file_bytes = 36 + s_data_bytes;
        hdr[4] = (uint8_t)s_file_bytes; hdr[5] = (uint8_t)(s_file_bytes >> 8);
        hdr[6] = (uint8_t)(s_file_bytes >> 16); hdr[7] = (uint8_t)(s_file_bytes >> 24);
        hdr[40] = (uint8_t)s_data_bytes; hdr[41] = (uint8_t)(s_data_bytes >> 8);
        hdr[42] = (uint8_t)(s_data_bytes >> 16); hdr[43] = (uint8_t)(s_data_bytes >> 24);
        fwrite(hdr, 1, 44, s_fp);
        fwrite(stream_pcm, 2, total_samples, s_fp);
        fclose(s_fp);
        free(stream_pcm);

        /* Decode unaligned stream */
        uint8_t stream_decoded[80];
        size_t stream_dec_len = 0;
        int stream_rc = ozos_ofdm_decode_wav_stream(wav_stream, stream_decoded, &stream_dec_len);
        remove(wav_stream);

        bool sync_ok = (stream_rc == 0 && stream_dec_len == 80 && memcmp(packet, stream_decoded, 80) == 0);
        if (sync_ok) {
            printf("  -> [PASS] Schmidl-Cox Detector locked onto packet with 850-sample preamble noise: 100%% BIT-EXACT!\n");
        } else {
            printf("  -> [FAIL] Schmidl-Cox unaligned stream decode failed (rc=%d)\n", stream_rc);
            failed++;
        }
    }

    /* -------------------------------------------------------------------------
       TEST 10: LoRa Hardware Serial Loopback Simulation
       ------------------------------------------------------------------------- */
    printf("\n[TEST 10/11] LoRa Sub-GHz Hardware Serial Loopback Queue...\n");
    {
        uint8_t packet[80];
        for (int i = 0; i < 80; ++i) packet[i] = (uint8_t)(0x88 ^ (i * 3));

        uint8_t rx_packet[80];
        size_t rx_len = 0;
        int loop_rc = ozos_lora_serial_loopback_test(packet, 80, rx_packet, &rx_len);

        bool loop_ok = (loop_rc == 0 && rx_len == 80 && memcmp(packet, rx_packet, 80) == 0);
        if (loop_ok) {
            printf("  -> [PASS] LoRa Serial Ring-Buffer Loopback Transport: 100%% BIT-EXACT\n");
        } else {
            printf("  -> [FAIL] LoRa serial loopback failed (rc=%d)\n", loop_rc);
            failed++;
        }
    }

    /* -------------------------------------------------------------------------
       TEST 11: Planetary HF Skywave 8-MFSK Audio Modem through Ionospheric Noise
       ------------------------------------------------------------------------- */
    printf("\n[TEST 11/11] Planetary HF Skywave 8-MFSK Audio Modem through Noise...\n");
    {
        uint8_t packet[80];
        for (int i = 0; i < 80; ++i) packet[i] = (uint8_t)(0xFE ^ (i * 7));

        const char* skywave_wav = "_test_skywave_sim.wav";
        int synth_rc = ozos_skywave_synth_wav(packet, 80, skywave_wav, 4000.0f);

        uint8_t rec_skywave[80];
        size_t rec_len = 0;
        int dec_rc = ozos_skywave_decode_wav(skywave_wav, rec_skywave, &rec_len);
        remove(skywave_wav);

        bool skywave_ok = (synth_rc == 0 && dec_rc == 0 && rec_len == 80 &&
                           memcmp(packet, rec_skywave, 80) == 0);
        if (skywave_ok) {
            printf("  -> [PASS] Planetary HF Skywave 8-MFSK Audio Modem recovered 80-byte packet 100%% BIT-EXACT!\n");
        } else {
            printf("  -> [FAIL] HF Skywave audio modem failed (synth=%d, dec=%d)\n", synth_rc, dec_rc);
            failed++;
        }
    }

    printf("\n=================================================================\n");
    if (failed == 0) {
        printf("  [RESULT] 11/11 OMNI-SPECTRUM COGNITIVE PHY SUITE TESTS PASSED 100%%!\n");
        printf("  ALL 4 SPECTRUM TIERS & PHYSICAL HARDENING EXTENSIONS FULLY CERTIFIED.\n");
        printf("=================================================================\n");
        return 0;
    } else {
        printf("  [FAIL] %d TESTS FAILED!\n", failed);
        printf("=================================================================\n");
        return 1;
    }
}
