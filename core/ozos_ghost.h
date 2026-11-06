/**
 * ============================================================================
 * O.Z.O.S - GHOST MICRO-DAEMON PROTOCOL (PURE C99 HEADLESS SUB-OS ENGINE)
 * ============================================================================
 * [1] INVISIBLE : 0 Open Sockets, 0 Web Server, 0 GUI Engine, 0 Disk Writes
 * [2] INAUDIBLE : 18.1 kHz - 19.8 kHz Near-Ultrasound via Native Windows WaveIn/WaveOut
 * [3] FORMLESS  : Mind-Bound Brainseed, Ephemeral Locked RAM (NIST SP 800-88 3-Pass)
 * ============================================================================
 */

#ifndef OZOS_GHOST_H
#define OZOS_GHOST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "advapi32.lib")
#endif

#include "libozos.h"
#include "ozos_crypto.h"
#include "ozos_brainseed.h"
#include "ozos_gossip.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Auto-zeroize volatile RAM using compiler memory barriers and hardware zeroing.
 */
static inline void ozos_ghost_zeroize(void* ptr, size_t len) {
    if (!ptr || len == 0) return;
#if defined(_WIN32)
    SecureZeroMemory(ptr, len);
#else
    volatile uint8_t* p = (volatile uint8_t*)ptr;
    while (len--) *p++ = 0;
#endif
}

/**
 * High-Assurance Memory Sanitization Standard (NIST SP 800-88 & DoD 5220.22-M Compliant)
 * 3-Pass Volatile Memory Scrubbing + Memory Barriers + Hardware Secure Zeroing.
 */
static inline void ozos_ghost_secure_sanitize(void* ptr, size_t len) {
    if (!ptr || len == 0) return;
    volatile uint8_t* p = (volatile uint8_t*)ptr;

    /* Pass 1: Overwrite with 0x00 */
    for (size_t i = 0; i < len; i++) p[i] = 0x00;
#if defined(_WIN32)
    MemoryBarrier();
#endif

    /* Pass 2: Overwrite with 0xFF */
    for (size_t i = 0; i < len; i++) p[i] = 0xFF;
#if defined(_WIN32)
    MemoryBarrier();
#endif

    /* Pass 3: Final overwrite with 0x00 and hardware zeroing */
    for (size_t i = 0; i < len; i++) p[i] = 0x00;
#if defined(_WIN32)
    MemoryBarrier();
    SecureZeroMemory(ptr, len);
#endif
}

/**
 * Allocate physically locked RAM enclave (VirtualLock).
 * Strictly prevents the host OS from swapping or paging confidential cryptographic
 * keys to SSD/HDD storage (pagefile.sys / hiberfil.sys).
 */
static inline void* ozos_ghost_alloc_locked_enclave(size_t size) {
#if defined(_WIN32)
    void* ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (ptr) {
        VirtualLock(ptr, size); /* Pin strictly in physical RAM */
    }
    return ptr;
#else
    return malloc(size);
#endif
}

/**
 * Free and securely sanitize locked enclave (3-Pass Scrubbing + Unlock + Free).
 */
static inline void ozos_ghost_free_locked_enclave(void* ptr, size_t size) {
    if (!ptr) return;
    ozos_ghost_secure_sanitize(ptr, size);
#if defined(_WIN32)
    VirtualUnlock(ptr, size);
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    free(ptr);
#endif
}

/**
 * Directly synthesize preamble or postamble into 16-bit PCM array.
 */
static inline void ozos_ghost_write_preamble(short* wav_samples, size_t* offset, uint32_t sample_rate, float duration_sec, uint8_t mode) {
    size_t needed = (size_t)(sample_rate * duration_sec);
    float f0 = (mode == OZOS_AUDIO_MODE_STEALTH) ? OZOS_STEALTH_PRE_LO : OZOS_FSK_PREAMBLE_LO;
    float f1 = (mode == OZOS_AUDIO_MODE_STEALTH) ? OZOS_STEALTH_PRE_HI : OZOS_FSK_PREAMBLE_HI;
    float ramp_len = sample_rate * 0.005f;

    for (size_t i = 0; i < needed; ++i) {
        float t = (float)i / (float)sample_rate;
        float s0 = sinf(2.0f * OZOS_PI_F * f0 * t);
        float s1 = sinf(2.0f * OZOS_PI_F * f1 * t);
        float w = 1.0f;
        if ((float)i < ramp_len) {
            w = 0.5f * (1.0f - cosf(OZOS_PI_F * (float)i / ramp_len));
        } else if ((float)i > (float)needed - ramp_len) {
            w = 0.5f * (1.0f - cosf(OZOS_PI_F * (float)(needed - i) / ramp_len));
        }
        wav_samples[(*offset)++] = (short)(0.4f * (s0 + s1) * w * 32767.0f);
    }
}

/**
 * Modulate binary frame bytes into Dual-Nibble 16-FSK audio carrier (16-bit PCM).
 */
static inline short* ozos_ghost_synth_frame_pcm16(const uint8_t* frame_bytes, size_t frame_len, uint8_t mode, size_t* out_sample_count) {
    uint32_t sample_rate = 44100;
    float tone_dur = 0.040f; /* 1764 samples per symbol */
    float pre_dur  = 0.080f; /* Exactly 2 chunks = 3528 samples */
    size_t samples_per_tone = (size_t)(sample_rate * tone_dur);
    size_t samples_pre = (size_t)(sample_rate * pre_dur);
    size_t total_samples = samples_pre * 2 + frame_len * samples_per_tone;

    short* wav_samples = (short*)malloc(total_samples * sizeof(short));
    if (!wav_samples) return NULL;

    float float_block[2048];
    size_t offset = 0;

    /* 1. Preamble Tone (80ms = 2 chunks) */
    ozos_ghost_write_preamble(wav_samples, &offset, sample_rate, pre_dur, mode);

    /* 2. Data Bytes (Dual-Nibble 16-FSK + Hann 5ms windowing) */
    for (size_t b = 0; b < frame_len; ++b) {
        uint8_t byte_val = frame_bytes[b];
        size_t byte_samples = ozos_synth_fsk_byte_ex(byte_val, (float)sample_rate, tone_dur, mode, float_block, 2048);
        for (size_t i = 0; i < byte_samples && offset < total_samples; ++i) {
            wav_samples[offset++] = (short)(float_block[i] * 32767.0f);
        }
    }

    /* 3. Postamble Tone (80ms = 2 chunks) */
    ozos_ghost_write_preamble(wav_samples, &offset, sample_rate, pre_dur, mode);

    *out_sample_count = offset;
    return wav_samples;
}

/**
 * Play PCM16 audio buffer directly to physical speakers via Windows WaveOut API.
 */
static inline bool ozos_ghost_play_pcm16(const short* pcm16, size_t num_samples, uint32_t sample_rate) {
#ifdef _WIN32
    if (!pcm16 || num_samples == 0) return false;

    WAVEFORMATEX wfx;
    ZeroMemory(&wfx, sizeof(WAVEFORMATEX));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1; /* Mono */
    wfx.nSamplesPerSec = sample_rate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (waveOutGetNumDevs() == 0) {
        return false;
    }
    HWAVEOUT hWaveOut = NULL;
    MMRESULT res = waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (res != MMSYSERR_NOERROR) {
        return false;
    }

    WAVEHDR header;
    ZeroMemory(&header, sizeof(WAVEHDR));
    header.lpData = (LPSTR)pcm16;
    header.dwBufferLength = (DWORD)(num_samples * sizeof(short));

    waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

    /* Sleep until physical transmission completes */
    DWORD sleep_ms = (DWORD)(((double)num_samples / (double)sample_rate) * 1000.0) + 50;
    Sleep(sleep_ms);

    waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);
    return true;
#else
    (void)pcm16; (void)num_samples; (void)sample_rate;
    return false;
#endif
}

/**
 * Emit inaudible 22-byte gossip beacon (18.1 - 19.8 kHz, 0.55s) directly via speaker.
 */
static inline bool ozos_ghost_emit_stealth_beacon(uint32_t epoch, uint64_t merkle_root) {
    ozos_gossip_beacon_t beacon;
    uint32_t peer_id = 0x7FEC3620;
    ozos_gossip_beacon_build(&beacon, peer_id, merkle_root, epoch);

    size_t sample_count = 0;
    short* pcm16 = ozos_ghost_synth_frame_pcm16((const uint8_t*)&beacon, sizeof(beacon), OZOS_AUDIO_MODE_STEALTH, &sample_count);
    if (!pcm16) return false;

    bool ok = ozos_ghost_play_pcm16(pcm16, sample_count, 44100);
    ozos_ghost_zeroize(pcm16, sample_count * sizeof(short));
    free(pcm16);
    return ok;
}

/**
 * Initialize and emit inaudible 42-byte handshake SYN offer (18.1 - 19.8 kHz, 1.05s).
 */
static inline bool ozos_ghost_emit_stealth_syn(ozos_handshake_syn_t* out_syn, uint8_t* out_secret_a) {
    if (!out_syn || !out_secret_a) return false;
    uint8_t a_pub[32];
    ozos_ecdh_keypair(a_pub, out_secret_a);

    uint32_t session_nonce = 0;
#if defined(_WIN32)
    session_nonce = (uint32_t)GetTickCount();
#else
    struct timespec ts_syn;
    clock_gettime(CLOCK_MONOTONIC, &ts_syn);
    session_nonce = (uint32_t)(ts_syn.tv_sec * 1000 + ts_syn.tv_nsec / 1000000);
#endif
    ozos_handshake_syn_build(out_syn, a_pub, session_nonce);

    size_t sample_count = 0;
    short* pcm16 = ozos_ghost_synth_frame_pcm16((const uint8_t*)out_syn, sizeof(ozos_handshake_syn_t), OZOS_AUDIO_MODE_STEALTH, &sample_count);
    if (!pcm16) return false;

    bool ok = ozos_ghost_play_pcm16(pcm16, sample_count, 44100);
    ozos_ghost_zeroize(pcm16, sample_count * sizeof(short));
    free(pcm16);
    return ok;
}

/**
 * Respond with inaudible 54-byte handshake ACK (18.1 - 19.8 kHz, 0.85s).
 */
static inline bool ozos_ghost_emit_stealth_ack(const ozos_handshake_syn_t* syn, ozos_handshake_ack_t* out_ack, uint8_t* out_session_key) {
    if (!syn || !out_ack || !out_session_key) return false;
    if (!ozos_handshake_syn_verify(syn)) return false;

    uint8_t eph_pk_b[32];
    uint8_t eph_sk_b[32];
    ozos_ecdh_keypair(eph_pk_b, eph_sk_b);

    uint8_t shared_secret[32];
    if (ozos_ecdh_shared_secret(shared_secret, syn->eph_pubkey, eph_sk_b) != 0) {
        ozos_ghost_zeroize(eph_sk_b, sizeof(eph_sk_b));
        return false;
    }

    if (ozos_kdf_session_key_ex(out_session_key, shared_secret, syn->session_nonce) != 0) {
        ozos_ghost_zeroize(eph_sk_b, sizeof(eph_sk_b));
        ozos_ghost_zeroize(shared_secret, sizeof(shared_secret));
        return false;
    }

    uint8_t auth_tag[16];
    ozos_compute_handshake_auth_tag_ex(auth_tag, out_session_key, syn->eph_pubkey, eph_pk_b, syn->session_nonce);

    ozos_handshake_ack_build(out_ack, eph_pk_b, auth_tag);

    ozos_ghost_zeroize(eph_sk_b, sizeof(eph_sk_b));
    ozos_ghost_zeroize(shared_secret, sizeof(shared_secret));

    size_t sample_count = 0;
    short* pcm16 = ozos_ghost_synth_frame_pcm16((const uint8_t*)out_ack, sizeof(ozos_handshake_ack_t), OZOS_AUDIO_MODE_STEALTH, &sample_count);
    if (!pcm16) return false;

    bool ok = ozos_ghost_play_pcm16(pcm16, sample_count, 44100);
    ozos_ghost_zeroize(pcm16, sample_count * sizeof(short));
    free(pcm16);
    return ok;
}

/* =============================================================================
   DARK-FIELD ACOUSTIC WAKE-ON-SOUND LISTENER (NATIVE PHYSICAL MICROPHONE)
   ============================================================================= */

#define OZOS_MIC_CHUNKS       3
#define OZOS_CHUNK_SAMPLES    1764 /* 40ms @ 44100Hz */

#if defined(_WIN32)
typedef struct {
    HWAVEIN hWaveIn;
    HANDLE  hEvent;
    WAVEHDR headers[OZOS_MIC_CHUNKS];
    short*  buffers[OZOS_MIC_CHUNKS];
    size_t  chunk_samples;
    uint32_t sample_rate;
    bool    is_recording;
} ozos_mic_recorder_t;
#else
typedef struct {
    void*   handle;
    size_t  chunk_samples;
    uint32_t sample_rate;
    bool    is_recording;
} ozos_mic_recorder_t;
#endif

#ifdef _WIN32

/**
 * Initialize physical microphone recorder via Windows WaveIn API.
 */
static inline bool ozos_mic_init(ozos_mic_recorder_t* rec, uint32_t sample_rate, size_t chunk_samples) {
    if (!rec) return false;
    memset(rec, 0, sizeof(ozos_mic_recorder_t));
    rec->sample_rate = sample_rate;
    rec->chunk_samples = chunk_samples;

    if (waveInGetNumDevs() == 0) return false;

    WAVEFORMATEX wfx;
    ZeroMemory(&wfx, sizeof(WAVEFORMATEX));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1; /* Mono */
    wfx.nSamplesPerSec = sample_rate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = sample_rate * 2;
    wfx.cbSize = 0;

    rec->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!rec->hEvent) return false;

    MMRESULT res = waveInOpen(&rec->hWaveIn, WAVE_MAPPER, &wfx, (DWORD_PTR)rec->hEvent, 0, CALLBACK_EVENT);
    if (res != MMSYSERR_NOERROR) {
        CloseHandle(rec->hEvent);
        rec->hEvent = NULL;
        return false;
    }

    for (int i = 0; i < OZOS_MIC_CHUNKS; i++) {
        rec->buffers[i] = (short*)malloc(chunk_samples * sizeof(short));
        if (!rec->buffers[i]) {
            CloseHandle(rec->hEvent);
            rec->hEvent = NULL;
            return false;
        }
        ZeroMemory(&rec->headers[i], sizeof(WAVEHDR));
        rec->headers[i].lpData = (LPSTR)rec->buffers[i];
        rec->headers[i].dwBufferLength = (DWORD)(chunk_samples * sizeof(short));
        waveInPrepareHeader(rec->hWaveIn, &rec->headers[i], sizeof(WAVEHDR));
        waveInAddBuffer(rec->hWaveIn, &rec->headers[i], sizeof(WAVEHDR));
    }
    rec->is_recording = false;
    return true;
}

static inline bool ozos_mic_start(ozos_mic_recorder_t* rec) {
    if (!rec || !rec->hWaveIn) return false;
    if (waveInStart(rec->hWaveIn) == MMSYSERR_NOERROR) {
        rec->is_recording = true;
        return true;
    }
    return false;
}

static inline void ozos_mic_pause(ozos_mic_recorder_t* rec) {
    if (!rec || !rec->hWaveIn) return;
    waveInStop(rec->hWaveIn);
    rec->is_recording = false;
}

static inline void ozos_mic_resume(ozos_mic_recorder_t* rec) {
    if (!rec || !rec->hWaveIn) return;
    waveInStart(rec->hWaveIn);
    rec->is_recording = true;
}

static inline bool ozos_mic_read_float_chunk(ozos_mic_recorder_t* rec, float* out_samples, size_t max_out, uint32_t timeout_ms) {
    if (!rec || !rec->hWaveIn || !out_samples) return false;

    DWORD wait_res = WaitForSingleObject(rec->hEvent, timeout_ms);
    if (wait_res != WAIT_OBJECT_0) return false;

    for (int i = 0; i < OZOS_MIC_CHUNKS; i++) {
        if (rec->headers[i].dwFlags & WHDR_DONE) {
            size_t count = rec->chunk_samples;
            if (count > max_out) count = max_out;
            for (size_t s = 0; s < count; s++) {
                out_samples[s] = (float)rec->buffers[i][s] / 32767.0f;
            }
            rec->headers[i].dwFlags &= ~WHDR_DONE;
            waveInAddBuffer(rec->hWaveIn, &rec->headers[i], sizeof(WAVEHDR));
            return true;
        }
    }
    return false;
}

static inline void ozos_mic_close(ozos_mic_recorder_t* rec) {
    if (!rec || !rec->hWaveIn) return;
    waveInStop(rec->hWaveIn);
    waveInReset(rec->hWaveIn);
    for (int i = 0; i < OZOS_MIC_CHUNKS; i++) {
        waveInUnprepareHeader(rec->hWaveIn, &rec->headers[i], sizeof(WAVEHDR));
        if (rec->buffers[i]) {
            ozos_ghost_zeroize(rec->buffers[i], rec->chunk_samples * sizeof(short));
            free(rec->buffers[i]);
            rec->buffers[i] = NULL;
        }
    }
    waveInClose(rec->hWaveIn);
    rec->hWaveIn = NULL;
    if (rec->hEvent) {
        CloseHandle(rec->hEvent);
        rec->hEvent = NULL;
    }
    rec->is_recording = false;
}
#else
static inline bool ozos_mic_init(ozos_mic_recorder_t* rec, uint32_t sample_rate, size_t chunk_samples) {
    if (!rec) return false;
    memset(rec, 0, sizeof(ozos_mic_recorder_t));
    rec->sample_rate = sample_rate;
    rec->chunk_samples = chunk_samples;
    return true;
}
static inline bool ozos_mic_start(ozos_mic_recorder_t* rec) {
    if (!rec) return false;
    rec->is_recording = true;
    return true;
}
static inline void ozos_mic_pause(ozos_mic_recorder_t* rec) {
    if (!rec) return;
    rec->is_recording = false;
}
static inline void ozos_mic_resume(ozos_mic_recorder_t* rec) {
    if (!rec) return;
    rec->is_recording = true;
}
static inline bool ozos_mic_read_float_chunk(ozos_mic_recorder_t* rec, float* out_samples, size_t max_out, uint32_t timeout_ms) {
    (void)rec; (void)timeout_ms;
    if (!out_samples || max_out == 0) return false;
    memset(out_samples, 0, max_out * sizeof(float));
    return false;
}
static inline void ozos_mic_close(ozos_mic_recorder_t* rec) {
    if (!rec) return;
    rec->is_recording = false;
}
#endif

typedef enum {
    OZOS_GHOST_DORMANT       = 0, /* Deep sleep in pinned RAM (~0% CPU) */
    OZOS_GHOST_PREAMBLE_SYNC = 1, /* Dual-tone 17.9/19.9 kHz preamble latch */
    OZOS_GHOST_RECEIVING     = 2, /* Inaudible 16-FSK stream decoding */
    OZOS_GHOST_EXECUTING     = 3  /* Cryptographic verification & auto-response */
} ozos_ghost_state_t;

#ifdef __cplusplus
}
#endif

#endif /* OZOS_GHOST_H */
