/*
 * ============================================================================
 * O.Z.O.S Bare-Metal UEFI x86_64 Enclave Kernel (BOOTX64.EFI)
 * ============================================================================
 * Executes directly in Ring-0 UEFI Boot Services context prior to OS load.
 * Zero host OS, zero CRT, zero heap allocation (malloc = 0), 0 DLL imports.
 * Capabilities: CPU RDRAND entropy, BIP-39 12-word restore, RFC 8032 Ed25519,
 * X25519 + XSalsa20-Poly1305 handshake, and GOP 32x32 Optical Glyph output.
 * ============================================================================
 */

#include "ozos_uefi.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

int _fltused = 0;

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#pragma function(memset, memcpy, memcmp, strlen, strcmp, strcpy, strcat, strncpy)
#endif

/* ============================================================================
 * FREESTANDING MEMORY & STRING PRIMITIVES (ZERO CRT)
 * ============================================================================ */

void* memset(void* dst, int val, UINTN count) {
    volatile UINT8* p = (volatile UINT8*)dst;
    while (count--) *p++ = (UINT8)val;
    return dst;
}

void* memcpy(void* dst, const void* src, UINTN count) {
    UINT8* d = (UINT8*)dst;
    const UINT8* s = (const UINT8*)src;
    while (count--) *d++ = *s++;
    return dst;
}

int memcmp(const void* a, const void* b, UINTN count) {
    const UINT8* pa = (const UINT8*)a;
    const UINT8* pb = (const UINT8*)b;
    for (UINTN i = 0; i < count; ++i) {
        if (pa[i] != pb[i]) return (int)pa[i] - (int)pb[i];
    }
    return 0;
}

UINTN strlen(const char* s) {
    UINTN n = 0;
    while (s && s[n] != '\0') n++;
    return n;
}

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char* strcpy(char* dst, const char* src) {
    char* d = dst;
    while ((*d++ = *src++) != '\0');
    return dst;
}

char* strcat(char* dst, const char* src) {
    char* d = dst + strlen(dst);
    while ((*d++ = *src++) != '\0');
    return dst;
}

char* strncat(char* dst, const char* src, UINTN n) {
    char* d = dst + strlen(dst);
    while (n-- && *src) *d++ = *src++;
    *d = '\0';
    return dst;
}

char* strncpy(char* dst, const char* src, UINTN n) {
    UINTN i = 0;
    for (; i < n && src[i] != '\0'; ++i) dst[i] = src[i];
    for (; i < n; ++i) dst[i] = '\0';
    return dst;
}

#include "tweetnacl.h"
#include "ozos_crypto.h"
#include "ozos_brainseed.h"
#include "ozos_omni_phy.h"
#include "ozos_pqc_hybrid.h"

/* ============================================================================
 * PORTABLE x86_64 SILICON INTRINSICS (MSVC & CLANG FREESTANDING)
 * ============================================================================ */

static void hw_cpuid(int info[4], int leaf) {
#if defined(_MSC_VER) && !defined(__clang__)
    __cpuid(info, leaf);
#else
    UINT32 eax, ebx, ecx, edx;
    __asm__ __volatile__("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(leaf), "c"(0));
    info[0] = (int)eax; info[1] = (int)ebx; info[2] = (int)ecx; info[3] = (int)edx;
#endif
}

static int hw_rdrand64(UINT64* out) {
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned __int64 tmp = 0;
    int ok = _rdrand64_step(&tmp);
    *out = (UINT64)tmp;
    return ok;
#else
    UINT64 tmp = 0;
    UINT8 cf = 0;
    __asm__ __volatile__("rdrand %0; setc %1" : "=r"(tmp), "=qm"(cf));
    *out = tmp;
    return (int)cf;
#endif
}

static UINT64 hw_rdtsc(void) {
#if defined(_MSC_VER) && !defined(__clang__)
    return (UINT64)__rdtsc();
#else
    UINT32 lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((UINT64)hi << 32) | (UINT64)lo;
#endif
}

static void hw_halt(void) {
#if defined(_MSC_VER) && !defined(__clang__)
    __halt();
#else
    __asm__ __volatile__("hlt");
#endif
}

static int cpu_has_rdrand(void) {
    int info[4];
    hw_cpuid(info, 1);
    return (info[2] & (1 << 30)) != 0;
}

void randombytes(unsigned char* buf, unsigned long long len) {
    if (cpu_has_rdrand()) {
        unsigned long long idx = 0;
        while (idx < len) {
            UINT64 rnd = 0;
            int retries = 16;
            while (retries > 0 && !hw_rdrand64(&rnd)) {
                retries--;
            }
            for (int b = 0; b < 8 && idx < len; ++b) {
                buf[idx++] = (unsigned char)((rnd >> (b * 8)) & 0xFFU);
            }
        }
    } else {
        for (unsigned long long i = 0; i < len; ++i) {
            UINT64 t = hw_rdtsc();
            buf[i] = (unsigned char)((t ^ (t >> 13) ^ (t >> 27)) & 0xFFU);
        }
    }
}

static void uefi_zeroize(volatile void* ptr, UINTN size) {
    volatile UINT8* p = (volatile UINT8*)ptr;
    for (UINTN pass = 0; pass < 3; ++pass) {
        UINT8 val = (pass == 0) ? 0xAAU : (pass == 1 ? 0x55U : 0x00U);
        for (UINTN i = 0; i < size; ++i) p[i] = val;
    }
}

/* ============================================================================
 * UEFI CONSOLE & GRAPHICS OUTPUT PROTOCOL (GOP) HELPERS
 * ============================================================================ */

static EFI_SYSTEM_TABLE* g_st = 0;
static EFI_GRAPHICS_OUTPUT_PROTOCOL* g_gop = 0;

static void print(const CHAR16* str) {
    if (g_st && g_st->ConOut) {
        g_st->ConOut->OutputString(g_st->ConOut, (CHAR16*)str);
    }
}

static void set_color(UINTN attr) {
    if (g_st && g_st->ConOut) {
        g_st->ConOut->SetAttribute(g_st->ConOut, attr);
    }
}

static void print_ascii(const char* s) {
    CHAR16 ch[2];
    ch[1] = 0;
    while (s && *s) {
        if (*s == '\n') {
            print(L"\r\n");
        } else {
            ch[0] = (CHAR16)(unsigned char)(*s);
            print(ch);
        }
        s++;
    }
}

static void print_hex_bytes(const UINT8* buf, UINTN len) {
    static const CHAR16 hex_tbl[] = L"0123456789abcdef";
    CHAR16 out[3];
    out[2] = 0;
    for (UINTN i = 0; i < len; ++i) {
        out[0] = hex_tbl[(buf[i] >> 4) & 0x0FU];
        out[1] = hex_tbl[buf[i] & 0x0FU];
        print(out);
    }
}

static void print_u64(UINT64 val) {
    CHAR16 buf[24];
    int idx = 0;
    if (val == 0) {
        print(L"0");
        return;
    }
    while (val > 0) {
        buf[idx++] = (CHAR16)(L'0' + (val % 10));
        val /= 10;
    }
    while (idx > 0) {
        CHAR16 ch[2] = { buf[--idx], 0 };
        print(ch);
    }
}

static void wait_key(EFI_INPUT_KEY* key) {
    UINTN idx = 0;
    g_st->BootServices->WaitForEvent(1, &g_st->ConIn->WaitForKey, &idx);
    g_st->ConIn->ReadKeyStroke(g_st->ConIn, key);
}

static UINTN read_line_ascii(char* buf, UINTN max_len, int mask_echo) {
    UINTN len = 0;
    for (;;) {
        EFI_INPUT_KEY k;
        wait_key(&k);
        if (k.UnicodeChar == L'\r' || k.UnicodeChar == L'\n') {
            print(L"\r\n");
            break;
        }
        if (k.UnicodeChar == 0x0008) {
            if (len > 0) {
                len--;
                print(L"\b \b");
            }
            continue;
        }
        if (k.UnicodeChar >= 0x20 && k.UnicodeChar <= 0x7E && len + 1 < max_len) {
            buf[len++] = (char)k.UnicodeChar;
            CHAR16 echo[2] = { mask_echo ? L'*' : k.UnicodeChar, 0 };
            print(echo);
        }
    }
    buf[len] = '\0';
    return len;
}

static int parse_hex_bytes(const char* hex, UINT8* out, UINTN max_len, UINTN* out_len) {
    if (!hex || !out || !out_len) return 0;
    if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) hex += 2;
    UINTN hlen = strlen(hex);
    if (hlen == 0 || (hlen % 2) != 0 || (hlen / 2) > max_len) return 0;
    UINTN bytes = hlen / 2;
    for (UINTN i = 0; i < bytes; ++i) {
        char hi = hex[i * 2];
        char lo = hex[i * 2 + 1];
        int v_hi = (hi >= '0' && hi <= '9') ? (hi - '0') :
                   (hi >= 'a' && hi <= 'f') ? (hi - 'a' + 10) :
                   (hi >= 'A' && hi <= 'F') ? (hi - 'A' + 10) : -1;
        int v_lo = (lo >= '0' && lo <= '9') ? (lo - '0') :
                   (lo >= 'a' && lo <= 'f') ? (lo - 'a' + 10) :
                   (lo >= 'A' && lo <= 'F') ? (lo - 'A' + 10) : -1;
        if (v_hi < 0 || v_lo < 0) return 0;
        out[i] = (UINT8)((v_hi << 4) | v_lo);
    }
    *out_len = bytes;
    return 1;
}

static void init_gop(void) {
    if (!g_st || !g_st->BootServices || !g_st->BootServices->LocateProtocol) return;
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    void* iface = 0;
    if (g_st->BootServices->LocateProtocol(&gop_guid, 0, &iface) == EFI_SUCCESS && iface) {
        g_gop = (EFI_GRAPHICS_OUTPUT_PROTOCOL*)iface;
    }
}

/* Render 32x32 Optical Glyph onto top-right GOP framebuffer (if graphics available) */
static void render_glyph_gop(const UINT8* frame, UINTN len) {
    if (!g_gop || !g_gop->Blt || !g_gop->Mode || !g_gop->Mode->Info) return;
    UINT8 matrix[OZOS_OMNI_GLYPH_DIM][OZOS_OMNI_GLYPH_DIM];
    if (ozos_glyph_encode(frame, len, matrix) != 0) return;

    UINT32 h_res = g_gop->Mode->Info->HorizontalResolution;
    UINT32 v_res = g_gop->Mode->Info->VerticalResolution;
    UINTN cell_px = (h_res >= 1024 && v_res >= 768) ? 6 : 4;
    UINTN quiet = 2 * cell_px;
    UINTN box_px = (OZOS_OMNI_GLYPH_DIM * cell_px) + (2 * quiet);
    if (h_res < box_px + 20 || v_res < box_px + 20) return;

    UINTN ox = (UINTN)h_res - box_px - 20;
    UINTN oy = 20;

    EFI_GRAPHICS_OUTPUT_BLT_PIXEL bg_px = { 0x00, 0x00, 0x00, 0x00 };
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL on_px = { 0xFF, 0xFF, 0xFF, 0x00 };

    /* Solid dark quiet zone border */
    g_gop->Blt(g_gop, &bg_px, EfiBltVideoFill, 0, 0, ox, oy, box_px, box_px, 0);

    for (UINTN r = 0; r < OZOS_OMNI_GLYPH_DIM; ++r) {
        for (UINTN c = 0; c < OZOS_OMNI_GLYPH_DIM; ++c) {
            if (matrix[r][c]) {
                g_gop->Blt(
                    g_gop, &on_px, EfiBltVideoFill,
                    0, 0,
                    ox + quiet + (c * cell_px),
                    oy + quiet + (r * cell_px),
                    cell_px, cell_px, 0
                );
            }
        }
    }
}

/* Console Half-Block Optical Glyph renderer (works in pure text / serial consoles) */
static void render_glyph_console(const UINT8* frame, UINTN len) {
    UINT8 matrix[OZOS_OMNI_GLYPH_DIM][OZOS_OMNI_GLYPH_DIM];
    if (ozos_glyph_encode(frame, len, matrix) != 0) return;

    print(L"\r\n  [OPTICAL 32x32 GLYPH MATRIX]\r\n");
    for (int r = 0; r < OZOS_OMNI_GLYPH_DIM; r += 2) {
        print(L"  ");
        for (int c = 0; c < OZOS_OMNI_GLYPH_DIM; ++c) {
            UINT8 top = matrix[r][c];
            UINT8 bot = matrix[r + 1][c];
            CHAR16 glyph[3] = { L' ', L' ', 0 };
            if (top && bot) { glyph[0] = 0x2588; glyph[1] = 0x2588; }
            else if (top)   { glyph[0] = 0x2580; glyph[1] = 0x2580; }
            else if (bot)   { glyph[0] = 0x2584; glyph[1] = 0x2584; }
            print(glyph);
        }
        print(L"\r\n");
    }
}

/* ============================================================================
 * WIRE CRC16-CCITT & EPHEMERAL KEY STATE (VOLATILE SRAM ONLY)
 * ============================================================================ */

static UINT16 calc_crc16_ccitt(const UINT8* data, UINTN len) {
    UINT16 crc = 0xFFFF;
    for (UINTN i = 0; i < len; ++i) {
        crc ^= ((UINT16)data[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000) crc = (UINT16)((crc << 1) ^ 0x1021);
            else crc <<= 1;
        }
    }
    return crc;
}

static UINT8 g_pk[32];
static UINT8 g_sk[64];
static UINT8 g_last_frame[80];
static UINTN g_last_frame_len = 0;
static int   g_key_initialized = 0;

static void cmd_keygen(void) {
    set_color(EFI_LIGHTCYAN);
    print(L"\r\n[RING-0 KEYGEN] Generating Ed25519 keypair from CPU RDRAND...\r\n");
    crypto_sign_keypair(g_pk, g_sk);
    g_key_initialized = 1;
    set_color(EFI_LIGHTGREEN);
    print(L"  * Ed25519 Public Key: 0x");
    print_hex_bytes(g_pk, 32);
    print(L"\r\n  * Secret Key Status : Pinned in volatile Ring-0 SRAM\r\n");
    set_color(EFI_WHITE);
}

static void cmd_sign_verify(void) {
    if (!g_key_initialized) cmd_keygen();

    UINT8 frame[80];
    memset(frame, 0, sizeof(frame));
    UINT64 nonce = hw_rdtsc();
    memcpy(frame + 64, &nonce, 8);
    /* Magic 0x47454E01 ("GEN\x01") at 0x48..0x4B */
    frame[72] = 0x01; frame[73] = 0x4E; frame[74] = 0x45; frame[75] = 0x47;
    frame[76] = 0x00; frame[77] = 0x00;
    UINT16 mac = calc_crc16_ccitt(frame, 78);
    frame[78] = (UINT8)(mac & 0xFFU);
    frame[79] = (UINT8)((mac >> 8) & 0xFFU);

    UINT8 signed_msg[144];
    unsigned long long smlen = 0;
    UINT64 t0 = hw_rdtsc();
    crypto_sign(signed_msg, &smlen, frame, 80, g_sk);
    UINT64 t1 = hw_rdtsc();

    UINT8 verified_out[144];
    unsigned long long mlen = 0;
    int rc = crypto_sign_open(verified_out, &mlen, signed_msg, smlen, g_pk);

    memcpy(g_last_frame, frame, 80);
    g_last_frame_len = 80;
    render_glyph_gop(g_last_frame, g_last_frame_len);

    set_color(EFI_LIGHTCYAN);
    print(L"\r\n[RING-0 CRYPTOGRAPHIC SIGN & VERIFY]\r\n");
    set_color(EFI_WHITE);
    print(L"  * Signed Frame Len  : "); print_u64(smlen); print(L" bytes\r\n");
    print(L"  * Tripwire MAC16    : 0x"); print_hex_bytes(frame + 78, 2); print(L"\r\n");
    print(L"  * Sign Latency      : "); print_u64(t1 - t0); print(L" CPU cycles\r\n");
    if (g_gop) {
        print(L"  * Optical Glyph     : Rendered 32x32 frame via UEFI GOP\r\n");
    }
    if (rc == 0 && mlen == 80) {
        set_color(EFI_LIGHTGREEN);
        print(L"  * Verification      : [PASS] BIT-EXACT RING-0 ED25519 VALIDATED\r\n");
    } else {
        set_color(EFI_RED);
        print(L"  * Verification      : [FAIL] SIGNATURE MISMATCH\r\n");
    }
    set_color(EFI_WHITE);
}

static void cmd_benchmark(void) {
    UINT8 pkt[80];
    memset(pkt, 0x5A, sizeof(pkt));
    UINT64 t0 = hw_rdtsc();
    volatile UINT16 acc = 0;
    for (int i = 0; i < 100000; ++i) {
        pkt[0] = (UINT8)(i & 0xFF);
        acc ^= calc_crc16_ccitt(pkt, 78);
    }
    UINT64 t1 = hw_rdtsc();
    (void)acc;

    set_color(EFI_LIGHTCYAN);
    print(L"\r\n[RING-0 BARE-METAL BENCHMARK (100,000 FRAMES)]\r\n");
    set_color(EFI_WHITE);
    print(L"  * Total CPU Cycles  : "); print_u64(t1 - t0); print(L"\r\n");
    print(L"  * Cycles Per Frame  : "); print_u64((t1 - t0) / 100000ULL); print(L" cycles/packet\r\n");
}

static void cmd_brainseed_restore(void) {
    char mnemonic[512];
    char passphrase[256];
    UINT8 seed32[32];

    set_color(EFI_LIGHTCYAN);
    print(L"\r\n[BIP-39 RING-0 BRAINSEED RESTORE]\r\n");
    set_color(EFI_WHITE);
    print(L"Enter 12 BIP-39 words (or press Enter for test vector 'abandon..about'):\r\n> ");
    UINTN mlen = read_line_ascii(mnemonic, sizeof(mnemonic), 0);
    if (mlen == 0) {
        strcpy(mnemonic, "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about");
        print(L"  * Using canonical test vector (abandon x11 about)\r\n");
    }

    print(L"Enter optional passphrase (or press Enter for none):\r\n> ");
    read_line_ascii(passphrase, sizeof(passphrase), 1);

    UINT64 t0 = hw_rdtsc();
    bool ok = ozos_brainseed_restore(seed32, mnemonic, passphrase);
    UINT64 t1 = hw_rdtsc();

    uefi_zeroize(mnemonic, sizeof(mnemonic));
    uefi_zeroize(passphrase, sizeof(passphrase));

    if (!ok) {
        uefi_zeroize(seed32, sizeof(seed32));
        set_color(EFI_RED);
        print(L"[REJECTED] Invalid 12-word BIP-39 mnemonic or SHA-256 checksum failed.\r\n");
        set_color(EFI_WHITE);
        return;
    }

    crypto_sign_seed_keypair(g_pk, g_sk, seed32);
    g_key_initialized = 1;

    /* Encode 32B Public Key in OZOS-80 frame for immediate GOP Optical display */
    memset(g_last_frame, 0, sizeof(g_last_frame));
    memcpy(g_last_frame, g_pk, 32);
    UINT64 n = hw_rdtsc();
    memcpy(g_last_frame + 64, &n, 8);
    g_last_frame[72] = 0x01; g_last_frame[73] = 0x4E; g_last_frame[74] = 0x45; g_last_frame[75] = 0x47;
    g_last_frame[76] = 0x20; g_last_frame[77] = 0x00;
    UINT16 mac = calc_crc16_ccitt(g_last_frame, 78);
    g_last_frame[78] = (UINT8)(mac & 0xFFU);
    g_last_frame[79] = (UINT8)((mac >> 8) & 0xFFU);
    g_last_frame_len = 80;
    render_glyph_gop(g_last_frame, g_last_frame_len);

    set_color(EFI_LIGHTGREEN);
    print(L"  * PBKDF2-HMAC-SHA512 (2048 rounds): COMPLETE in ");
    print_u64(t1 - t0); print(L" cycles\r\n");
    print(L"  * Restored Ed25519 PubKey : 0x");
    print_hex_bytes(g_pk, 32);
    print(L"\r\n  * Active Ring-0 Keypair   : UPDATED (mnemonic zeroized from SRAM)\r\n");
    set_color(EFI_WHITE);

    uefi_zeroize(seed32, sizeof(seed32));
}

static void cmd_ecdh_optical(void) {
    uint8_t a_pk[32], a_sk[32];
    ozos_ecdh_keypair(a_pk, a_sk);

    UINT32 nonce = (UINT32)(hw_rdtsc() ^ ((UINT64)a_pk[0] << 16));
    if (nonce == 0) nonce = 1;

    ozos_handshake_syn_t syn;
    ozos_handshake_syn_build(&syn, a_pk, nonce);

    memcpy(g_last_frame, &syn, sizeof(syn));
    memset(g_last_frame + sizeof(syn), 0, 80 - sizeof(syn));
    g_last_frame_len = 80;
    render_glyph_gop((const UINT8*)&syn, sizeof(syn));

    set_color(EFI_LIGHTCYAN);
    print(L"\r\n[RING-0 X25519 AIR-GAP HANDSHAKE INITIATOR (SYN)]\r\n");
    set_color(EFI_WHITE);
    print(L"  * SYN Frame (42B)   : ");
    print_hex_bytes((const UINT8*)&syn, sizeof(syn));
    print(L"\r\n  * Ephemeral A_Pub   : 0x");
    print_hex_bytes(a_pk, 32);
    if (g_gop) {
        print(L"\r\n  * Optical Glyph     : Rendered 32x32 SYN Glyph via UEFI GOP");
    }
    print(L"\r\n\r\nRun on Android/PC: ./ozos handshake-ack <SYN_HEX>\r\n");
    print(L"Paste 108-hex ACK frame (or Enter for self-contained Ring-0 loopback test):\r\n> ");

    char ack_line[256];
    UINTN alen = read_line_ascii(ack_line, sizeof(ack_line), 0);

    ozos_handshake_ack_t ack;
    uint8_t b_pk[32], b_sk[32], b_shared[32], b_sess[32];
    int used_loopback = 0;

    if (alen == 0) {
        used_loopback = 1;
        ozos_ecdh_keypair(b_pk, b_sk);
        if (ozos_ecdh_shared_secret(b_shared, syn.eph_pubkey, b_sk) != 0 ||
            ozos_kdf_session_key_ex(b_sess, b_shared, syn.session_nonce) != 0) {
            set_color(EFI_RED);
            print(L"[FAIL] Loopback ECDH Responder failed.\r\n");
            set_color(EFI_WHITE);
            uefi_zeroize(a_sk, sizeof(a_sk));
            uefi_zeroize(b_sk, sizeof(b_sk));
            return;
        }
        uint8_t tag[16];
        ozos_compute_handshake_auth_tag_ex(tag, b_sess, syn.eph_pubkey, b_pk, syn.session_nonce);
        ozos_handshake_ack_build(&ack, b_pk, tag);
        uefi_zeroize(b_sk, sizeof(b_sk));
        uefi_zeroize(b_shared, sizeof(b_shared));
    } else {
        UINTN parsed = 0;
        if (!parse_hex_bytes(ack_line, (UINT8*)&ack, sizeof(ack), &parsed) ||
            parsed != sizeof(ack)) {
            set_color(EFI_RED);
            print(L"[REJECTED] ACK must be exactly 108 hex chars (54 bytes).\r\n");
            set_color(EFI_WHITE);
            uefi_zeroize(a_sk, sizeof(a_sk));
            return;
        }
    }

    if (!ozos_handshake_ack_verify(&ack)) {
        set_color(EFI_RED);
        print(L"[REJECTED] ACK invalid magic, MAC16 mismatch, or low-order Curve25519 point.\r\n");
        set_color(EFI_WHITE);
        uefi_zeroize(a_sk, sizeof(a_sk));
        if (used_loopback) uefi_zeroize(b_sess, sizeof(b_sess));
        return;
    }

    uint8_t shared[32], k_sess[32];
    if (ozos_ecdh_shared_secret(shared, ack.eph_pubkey, a_sk) != 0 ||
        ozos_kdf_session_key_ex(k_sess, shared, syn.session_nonce) != 0) {
        set_color(EFI_RED);
        print(L"[REJECTED] X25519 ECDH key derivation failed.\r\n");
        set_color(EFI_WHITE);
        uefi_zeroize(a_sk, sizeof(a_sk));
        uefi_zeroize(shared, sizeof(shared));
        uefi_zeroize(k_sess, sizeof(k_sess));
        if (used_loopback) uefi_zeroize(b_sess, sizeof(b_sess));
        return;
    }

    if (!ozos_verify_handshake_auth_tag_ex(ack.auth_tag, k_sess, syn.eph_pubkey, ack.eph_pubkey, syn.session_nonce)) {
        set_color(EFI_RED);
        print(L"[REJECTED] Poly1305 mutual auth tag verification FAILED!\r\n");
        set_color(EFI_WHITE);
        uefi_zeroize(a_sk, sizeof(a_sk));
        uefi_zeroize(shared, sizeof(shared));
        uefi_zeroize(k_sess, sizeof(k_sess));
        if (used_loopback) uefi_zeroize(b_sess, sizeof(b_sess));
        return;
    }

    set_color(EFI_LIGHTGREEN);
    print(L"  * Handshake Status  : [PASS] MUTUAL POLY1305 TAG VERIFIED\r\n");
    print(L"  * Derived K_session : 0x");
    print_hex_bytes(k_sess, 32);
    print(L"\r\n");

    /* Encrypt & test-verify an XSalsa20-Poly1305 tunnel payload directly in Ring-0 */
    const char* msg = "OZOS-UEFI-RING0-VERIFIED";
    UINT8 nonce24[24];
    randombytes(nonce24, sizeof(nonce24));
    UINT8 cipher_frame[128];
    size_t clen = ozos_tunnel_encrypt(cipher_frame, sizeof(cipher_frame), (const uint8_t*)msg, strlen(msg), k_sess, nonce24);
    if (clen > 0) {
        set_color(EFI_WHITE);
        print(L"  * Cipher Frame (OZHD): ");
        print_hex_bytes(cipher_frame, clen);
        print(L"\r\n");

        uint8_t dec_buf[128];
        size_t dec_len = 0;
        const uint8_t* dec_key = used_loopback ? b_sess : k_sess;
        if (ozos_tunnel_decrypt(dec_buf, &dec_len, cipher_frame, clen, dec_key)) {
            dec_buf[dec_len] = '\0';
            set_color(EFI_LIGHTGREEN);
            print(L"  * AEAD Self-Check   : [PASS] XSalsa20-Poly1305 -> \"");
            print_ascii((const char*)dec_buf);
            print(L"\"\r\n");
        }
        uefi_zeroize(dec_buf, sizeof(dec_buf));
    }

    set_color(EFI_WHITE);
    uefi_zeroize(a_sk, sizeof(a_sk));
    uefi_zeroize(shared, sizeof(shared));
    uefi_zeroize(k_sess, sizeof(k_sess));
    if (used_loopback) uefi_zeroize(b_sess, sizeof(b_sess));
}

static void cmd_pqc_hybrid(void) {
    static ozos_pqc_hybrid_pk_t pk;
    static ozos_pqc_hybrid_sk_t sk;
    static ozos_pqc_hybrid_ct_t ct;
    UINT8 seed[32];
    UINT8 ss_enc[32];
    UINT8 ss_dec[32];
    UINT8 ss_tamper[32];

    set_color(EFI_YELLOW);
    print(L"\r\n[RING-0 PQC] NIST FIPS 203 ML-KEM-768 + X25519 HYBRID ENCLAVE...\r\n");
    set_color(EFI_WHITE);

    randombytes(seed, 32ULL);
    UINT64 t0 = hw_rdtsc();
    int rc = ozos_pqc_hybrid_keypair(&pk, &sk, seed);
    if (rc == 0) rc = ozos_pqc_hybrid_encaps(&pk, &ct, ss_enc);
    if (rc == 0) rc = ozos_pqc_hybrid_decaps(&sk, &ct, ss_dec);
    UINT64 t1 = hw_rdtsc();

    int eq_diff = 0;
    for (UINTN i = 0; i < 32; ++i) {
        eq_diff |= (int)(ss_enc[i] ^ ss_dec[i]);
    }

    ct.quantum_ciphertext[17] ^= 0x04U;
    int rc_tamper = ozos_pqc_hybrid_decaps(&sk, &ct, ss_tamper);
    int rej_diff = 0;
    for (UINTN i = 0; i < 32; ++i) {
        rej_diff |= (int)(ss_enc[i] ^ ss_tamper[i]);
    }

    print(L"  * Composite PK/CT   : ");
    print_u64((UINT64)sizeof(pk));
    print(L"B / ");
    print_u64((UINT64)sizeof(ct));
    print(L"B (RDRAND-seeded)\r\n");
    print(L"  * Agreed PQC Key    : ");
    print_hex_bytes(ss_dec, 16);
    print(L"...\r\n");
    print(L"  * Cycle Latency     : ");
    print_u64(t1 - t0);
    print(L" CPU cycles\r\n");

    if (rc == 0 && eq_diff == 0) {
        set_color(EFI_LIGHTGREEN);
        print(L"  * Hybrid Agreement  : [PASS] BIT-EXACT RING-0 PQC SHARED SECRET\r\n");
    } else {
        set_color(EFI_RED);
        print(L"  * Hybrid Agreement  : [FAIL] PQC ENCAPS/DECAPS MISMATCH\r\n");
    }

    if (rc_tamper == 0 && rej_diff != 0) {
        set_color(EFI_LIGHTGREEN);
        print(L"  * 1-Bit Cipher Tamper: [PASS] FO IMPLICIT REJECTION VERIFIED\r\n");
    } else {
        set_color(EFI_RED);
        print(L"  * 1-Bit Cipher Tamper: [FAIL] FO IMPLICIT REJECTION FAILED\r\n");
    }

    set_color(EFI_WHITE);
    uefi_zeroize(seed, sizeof(seed));
    uefi_zeroize(ss_enc, sizeof(ss_enc));
    uefi_zeroize(ss_dec, sizeof(ss_dec));
    uefi_zeroize(ss_tamper, sizeof(ss_tamper));
    uefi_zeroize(&pk, sizeof(pk));
    uefi_zeroize(&sk, sizeof(sk));
    uefi_zeroize(&ct, sizeof(ct));
}

/* ============================================================================
 * UEFI FIRMWARE ENTRYPOINT
 * ============================================================================ */

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {
    (void)ImageHandle;
    g_st = SystemTable;
    init_gop();

    g_st->ConOut->ClearScreen(g_st->ConOut);
    set_color(EFI_LIGHTGREEN);
    print(L"=====================================================================\r\n");
    print(L"  O.Z.O.S // BARE-METAL UEFI x86_64 ENCLAVE (RING 0)\r\n");
    print(L"  Zero Host OS | Zero CRT | RDRAND | BIP-39 | ML-KEM-768 | X25519\r\n");
    print(L"=====================================================================\r\n\r\n");

    set_color(EFI_WHITE);
    print(L"  * Firmware Vendor : ");
    print(g_st->FirmwareVendor ? g_st->FirmwareVendor : L"Unknown");
    print(L"\r\n  * UEFI Graphics   : ");
    if (g_gop && g_gop->Mode && g_gop->Mode->Info) {
        print_u64(g_gop->Mode->Info->HorizontalResolution);
        print(L"x");
        print_u64(g_gop->Mode->Info->VerticalResolution);
        print(L" (GOP Ready - 32x32 Optical Glyph)\r\n");
    } else {
        print(L"Text Console Only (Use [G] for Block-Glyph)\r\n");
    }
    print(L"  * Hardware RDRAND : ");
    if (cpu_has_rdrand()) {
        set_color(EFI_LIGHTGREEN);
        print(L"SUPPORTED (Hardware Silicon Entropy)\r\n");
    } else {
        set_color(EFI_YELLOW);
        print(L"UNAVAILABLE (RDTSC Jitter Fallback)\r\n");
    }

    cmd_keygen();
    cmd_sign_verify();

    for (;;) {
        set_color(EFI_YELLOW);
        print(L"\r\n[SHELL] [1] RDRAND Key  [2] Sign+Glyph  [3] Bench  [4] Wipe+Halt\r\n");
        print(L"        [5] BIP-39  [6] X25519  [7] ML-KEM-768 PQC  [G] Glyph  [R] Reboot\r\n> ");
        set_color(EFI_WHITE);

        EFI_INPUT_KEY k;
        wait_key(&k);

        CHAR16 echo[4] = { k.UnicodeChar, L'\r', L'\n', 0 };
        print(echo);

        if (k.UnicodeChar == L'1') {
            cmd_keygen();
        } else if (k.UnicodeChar == L'2') {
            cmd_sign_verify();
        } else if (k.UnicodeChar == L'3') {
            cmd_benchmark();
        } else if (k.UnicodeChar == L'5') {
            cmd_brainseed_restore();
        } else if (k.UnicodeChar == L'6') {
            cmd_ecdh_optical();
        } else if (k.UnicodeChar == L'7') {
            cmd_pqc_hybrid();
        } else if (k.UnicodeChar == L'g' || k.UnicodeChar == L'G') {
            if (g_last_frame_len > 0) render_glyph_console(g_last_frame, g_last_frame_len);
        } else if (k.UnicodeChar == L'4') {
            set_color(EFI_RED);
            print(L"\r\n[NIST SP 800-88] Scrubbing all volatile keys from SRAM...\r\n");
            uefi_zeroize(g_sk, sizeof(g_sk));
            uefi_zeroize(g_pk, sizeof(g_pk));
            uefi_zeroize(g_last_frame, sizeof(g_last_frame));
            g_key_initialized = 0;
            print(L"[PURGED] Halting system via EFI ResetSystem(Shutdown)...\r\n");
            g_st->RuntimeServices->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, 0);
            for (;;) hw_halt();
        } else if (k.UnicodeChar == L'r' || k.UnicodeChar == L'R') {
            uefi_zeroize(g_sk, sizeof(g_sk));
            uefi_zeroize(g_pk, sizeof(g_pk));
            g_st->RuntimeServices->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, 0);
        }
    }

    return EFI_SUCCESS;
}
