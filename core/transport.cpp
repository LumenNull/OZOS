#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

extern "C" {

#define GENESIS_MAGIC 0x47454E01 // "GEN\x01"
#define HEADER_SIZE 80 // 4 (magic) + 8 (nonce) + 60 (15 floats) + 4 (slot 15 FNV-1a) + 4 (payload_len)

/**
 * Validates and parses a raw binary packet directly in WASM memory.
 * Returns:
 *   0 = Valid packet
 *   1 = Invalid Magic Header
 *   2 = Packet too short, truncated, or payload_len > 65535
 *   3 = Replay Attack Detected (nonce <= last_seen_nonce)
 *   4 = Trailing garbage detected (buffer_size > 80 + payload_len)
 *   5 = Payload FNV-1a checksum mismatch (or zeroed slot 15)
 *   7 = Poisoned float (NaN / Infinity) in fingerprint slots 0..14
 */
EMSCRIPTEN_KEEPALIVE
int parse_and_validate_packet(
    const uint8_t* buffer,
    size_t buffer_size,
    uint64_t last_seen_nonce
) {
    if (!buffer || buffer_size < HEADER_SIZE) {
        return 2; // Too short
    }

    // 1. Check Magic (Offset 0..3)
    uint32_t magic = 0;
    memcpy(&magic, buffer, 4);
    if (magic != GENESIS_MAGIC) {
        return 1; // Invalid magic
    }

    // 2. Check Nonce (Offset 4..11) for Replay Protection
    uint64_t nonce = 0;
    memcpy(&nonce, buffer + 4, 8);
    if (nonce <= last_seen_nonce) {
        return 3; // Replay attack detected!
    }

    // 3. Check Float Finiteness in fingerprint slots 0..14 (Offset 12..71)
    for (size_t i = 0; i < 15; ++i) {
        uint32_t bits = 0;
        memcpy(&bits, buffer + 12 + i * 4, 4);
        if (((bits >> 23) & 0xFFU) == 0xFFU) {
            return 7; // NaN or Infinity poison detected!
        }
    }

    // 4. Check Payload Length (Offset 76..79)
    uint32_t payload_len = 0;
    memcpy(&payload_len, buffer + 76, 4);

    if (payload_len > 65535U || buffer_size < HEADER_SIZE + (size_t)payload_len) {
        return 2; // Truncated packet or overflow
    }
    if (buffer_size > HEADER_SIZE + (size_t)payload_len) {
        return 4; // Trailing garbage detected!
    }

    // 5. Check Slot 15 FNV-1a 32-bit Payload Checksum (Offset 72..75)
    uint32_t expected_fnv = 0;
    memcpy(&expected_fnv, buffer + 72, 4);
    if (payload_len > 0U || expected_fnv == 0U) {
        uint32_t fnv = 0x811C9DC5U;
        for (uint32_t i = 0; i < payload_len; ++i) {
            fnv ^= (uint32_t)buffer[HEADER_SIZE + i];
            fnv *= 0x01000193U;
        }
        if (fnv != expected_fnv) {
            return 5; // Payload tampered or zero-slot bypass attempt!
        }
    }

    return 0; // Valid
}

} // extern "C"
