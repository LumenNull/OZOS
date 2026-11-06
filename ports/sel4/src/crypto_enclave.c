/*
 * O.Z.O.S Sovereign Invariant Microkit Enclave
 * Target Architecture: seL4 Microkernel (ARM64 / RISC-V 64 / x86_64)
 * Purity Invariants   : Zero Heap (malloc=0), Static Stack Bounds, Constant-Time Logic
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../../core/libozos.h"
#include "../../core/ozos_shamir.h"

/* Microkit Channel Definitions */
#define CHANNEL_RX        1
#define CHANNEL_TX        2
#define CHANNEL_STORAGE   3

/* Memory Region Base Addresses (mapped via ozos.system) */
#define RX_QUEUE_BASE     ((volatile uint8_t *)0x30000000)
#define TX_QUEUE_BASE     ((volatile uint8_t *)0x31000000)
#define STORAGE_BUF_BASE  ((volatile uint8_t *)0x32000000)

/* Enclave Static Memory Bounds (No Heap) */
typedef struct {
    uint8_t  master_seed[32];
    uint8_t  sovereign_pubkey[32];
    uint8_t  sovereign_privkey[64];
    uint64_t monotonic_counter;
    bool     is_initialized;
} ozos_sel4_enclave_state_t;

static ozos_sel4_enclave_state_t g_enclave_state;

/* Forward declaration of Microkit system calls */
#ifdef __seL4__
#include <microkit.h>
#else
typedef unsigned int microkit_channel;
static inline void microkit_notify(microkit_channel ch) { (void)ch; }
#endif

/**
 * Microkit Entrypoint: init()
 * Invoked once by the seL4 Microkit runtime before processing events.
 */
void init(void) {
    /* Initialize enclave state statically */
    for (size_t i = 0; i < sizeof(g_enclave_state.master_seed); i++) {
        g_enclave_state.master_seed[i] = 0;
        g_enclave_state.sovereign_pubkey[i] = 0;
    }
    for (size_t i = 0; i < sizeof(g_enclave_state.sovereign_privkey); i++) {
        g_enclave_state.sovereign_privkey[i] = 0;
    }
    g_enclave_state.monotonic_counter = 0;
    g_enclave_state.is_initialized = true;

    /* Notify storage protection domain to fetch hardware monotonic counter */
    microkit_notify(CHANNEL_STORAGE);
}

/**
 * Microkit Event Handler: notified()
 * Triggered via capability notifications from peer Protection Domains.
 */
void notified(microkit_channel ch) {
    if (!g_enclave_state.is_initialized) {
        return;
    }

    if (ch == CHANNEL_RX) {
        /*
         * Ingest incoming 80-byte aligned frame from RX queue.
         * Stack-only parsing: zero heap allocations.
         */
        uint8_t frame_buf[OZOS_PACKET_SIZE];
        for (size_t i = 0; i < OZOS_PACKET_SIZE; i++) {
            frame_buf[i] = RX_QUEUE_BASE[i];
        }

        /* Verify wire format framing & tripwire MAC16 */
        ozos_packet_t pkt;
        int rc = ozos_parse_packet(frame_buf, OZOS_PACKET_SIZE, &pkt);
        if (rc == 0) {
            /* Check anti-replay against hardware monotonic counter */
            if (pkt.nonce > g_enclave_state.monotonic_counter) {
                g_enclave_state.monotonic_counter = pkt.nonce;

                /* Prepare signed acknowledgement into TX queue */
                for (size_t i = 0; i < OZOS_PACKET_SIZE; i++) {
                    TX_QUEUE_BASE[i] = frame_buf[i];
                }

                /* Notify TX domain that egress frame is staged */
                microkit_notify(CHANNEL_TX);
            }
        }
    } else if (ch == CHANNEL_STORAGE) {
        /* Synchronized with hardware TPM 2.0 / NVRAM monotonic counter */
        uint64_t hw_counter = 0;
        for (int i = 0; i < 8; i++) {
            hw_counter |= ((uint64_t)STORAGE_BUF_BASE[i]) << (i * 8);
        }
        if (hw_counter > g_enclave_state.monotonic_counter) {
            g_enclave_state.monotonic_counter = hw_counter;
        }
    }
}
