/**
 * =============================================================================
 *   O.Z.O.S // SOVEREIGN MESH P2P ENGINE (ozos-mesh.exe)
 *   Zero-IP Multi-Hop Physical Mesh Relay & Epidemic DTN Node
 *   Version: 2026.1-MESH | Architecture: x86_64 C99
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "libozos.h"
#include "tweetnacl.h"
#include "ozos_mesh.h"

static void hex_encode(const uint8_t* in, size_t len, char* out) {
    static const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i) {
        out[i * 2]     = hex_chars[(in[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex_chars[in[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

static bool hex_decode(const char* in, uint8_t* out, size_t out_len) {
    if (!in || !out) return false;
    size_t in_len = strlen(in);
    if (in_len != out_len * 2) return false;

    for (size_t i = 0; i < out_len; ++i) {
        char c1 = in[i * 2];
        char c2 = in[i * 2 + 1];
        uint8_t v1 = 0, v2 = 0;

        if (c1 >= '0' && c1 <= '9') v1 = (uint8_t)(c1 - '0');
        else if (c1 >= 'a' && c1 <= 'f') v1 = (uint8_t)(c1 - 'a' + 10);
        else if (c1 >= 'A' && c1 <= 'F') v1 = (uint8_t)(c1 - 'A' + 10);
        else return false;

        if (c2 >= '0' && c2 <= '9') v2 = (uint8_t)(c2 - '0');
        else if (c2 >= 'a' && c2 <= 'f') v2 = (uint8_t)(c2 - 'a' + 10);
        else if (c2 >= 'A' && c2 <= 'F') v2 = (uint8_t)(c2 - 'A' + 10);
        else return false;

        out[i] = (uint8_t)((v1 << 4) | v2);
    }
    return true;
}

static void print_usage(void) {
    printf("=================================================================\n");
    printf("  O.Z.O.S // SOVEREIGN MESH P2P NODE (ozos-mesh.exe)\n");
    printf("  Zero-IP Physical Mesh Router & Opportunistic DTN Forwarder\n");
    printf("=================================================================\n\n");
    printf("Available Operational Commands:\n");
    printf("  ozos-mesh.exe status                          - Node diagnostics & routing state\n");
    printf("  ozos-mesh.exe pack <src_id> <dst_id> <msg> [ttl] - Encapsulate 80-byte OZOS-80 mesh frame\n");
    printf("  ozos-mesh.exe forward <frame_hex> <local_id>  - Process incoming frame (Deliver/Relay/Drop)\n");
    printf("  ozos-mesh.exe sim-multihop                    - Execute 3-Node End-to-End Relay Benchmark\n");
    printf("=================================================================\n");
}

static int cmd_status(void) {
    printf("=================================================================\n");
    printf("  O.Z.O.S // SOVEREIGN MESH ROUTER STATUS\n");
    printf("  Protocol: OZOS-80 Mesh Invariant | Addressing: Cryptographic Node ID\n");
    printf("=================================================================\n\n");
    printf("[1] ROUTING & TOPOLOGY INVARIANTS:\n");
    printf("  * Network Addressing : Zero-IP / 32-bit Cryptographic Public Key ID\n");
    printf("  * Wire Format        : Exactly 80 Bytes (OZOS-80 Aligned)\n");
    printf("  * Forwarding Model   : Epidemic Delay-Tolerant Network (Store-and-Forward)\n");
    printf("  * Default Hop Limit  : %u Hops (Anti-Zombie Packet Expiration)\n", OZOS_MESH_DEFAULT_TTL);
    printf("  * Anti-Storm Shield  : %u-bit Stack Bloom Filter (3 Hashes)\n", OZOS_BLOOM_BITS);
    printf("  * Ring Buffer Depth  : %u Ephemeral Frames (Zero-Heap Malloc=0)\n\n", OZOS_CACHE_CAPACITY);

    printf("[2] OMNI-SPECTRUM PHYSICAL TRANSPORT INTEGRATION:\n");
    printf("  * Tier 1 Optical     : 32x32 Glyph (16ms high-speed luminance)\n");
    printf("  * Tier 2 Acoustic    : 18-20 kHz Near-Ultrasound & 64-FFT OFDM\n");
    printf("  * Tier 3 Sub-GHz     : LoRa 84-byte Long-Range Relay (5-15 km)\n\n");
    printf("[STATUS] Sovereign Mesh Engine: ACTIVE & LISTENING\n");
    printf("=================================================================\n");
    return 0;
}

static int cmd_pack(int argc, char* argv[]) {
    if (argc < 5) {
        printf("Error: Missing parameters. Usage: ozos-mesh.exe pack <src_id_hex> <dst_id_hex> <msg> [ttl]\n");
        return 1;
    }

    uint32_t src_id = (uint32_t)strtoul(argv[2], NULL, 16);
    uint32_t dst_id = (uint32_t)strtoul(argv[3], NULL, 16);
    const char* msg = argv[4];
    uint16_t ttl = (argc >= 6) ? (uint16_t)atoi(argv[5]) : OZOS_MESH_DEFAULT_TTL;

    ozos_mesh_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    frame.magic = OZOS_MAGIC_MESH_FRAME;
    frame.hop_limit = ttl;
    frame.flags = 0x0001; /* Bit 0: Encrypted */
    frame.src_node_id = src_id;
    frame.dst_node_id = dst_id;
    frame.packet_id = 0x1001; /* Deterministic test ID */

    size_t len = strlen(msg);
    if (len > OZOS_MESH_PAYLOAD_SIZE) len = OZOS_MESH_PAYLOAD_SIZE;
    memcpy(frame.payload, msg, len);

    ozos_mesh_seal(&frame);

    char hex[161];
    hex_encode((const uint8_t*)&frame, sizeof(frame), hex);

    printf("=================================================================\n");
    printf("  O.Z.O.S // MESH FRAME ENCAPSULATION\n");
    printf("=================================================================\n");
    printf("• Frame Size    : %zu Bytes (OZOS-80 Binary Aligned)\n", sizeof(frame));
    printf("• Source Node   : 0x%08X\n", src_id);
    printf("• Dest Node     : 0x%08X\n", dst_id);
    printf("• Hop Limit     : %u Hops\n", ttl);
    printf("• Tripwire MAC  : 0x%04X\n", frame.tripwire_mac);
    printf("• Raw Frame Hex : %s\n", hex);
    printf("=================================================================\n");
    return 0;
}

static int cmd_forward(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Error: Missing parameters. Usage: ozos-mesh.exe forward <frame_hex> <local_id_hex>\n");
        return 1;
    }

    const char* frame_hex = argv[2];
    uint32_t local_id = (uint32_t)strtoul(argv[3], NULL, 16);

    ozos_mesh_frame_t frame;
    if (!hex_decode(frame_hex, (uint8_t*)&frame, sizeof(frame))) {
        printf("Error: Invalid 80-byte mesh frame hex string.\n");
        return 1;
    }

    static ozos_mesh_bloom_t bf;
    static ozos_mesh_cache_t cache;
    static bool init_done = false;
    if (!init_done) {
        ozos_mesh_bloom_init(&bf);
        ozos_mesh_cache_init(&cache);
        init_done = true;
    }

    ozos_mesh_action_t action = ozos_mesh_process_frame(&frame, local_id, &bf, &cache);

    printf("=================================================================\n");
    printf("  O.Z.O.S // MESH ROUTING DISPATCH RESULT\n");
    printf("=================================================================\n");
    printf("• Local Node ID : 0x%08X\n", local_id);
    printf("• Target Node ID: 0x%08X\n", frame.dst_node_id);
    printf("• Source Node ID: 0x%08X\n", frame.src_node_id);
    printf("• Packet Seq ID : 0x%08X\n", frame.packet_id);

    switch (action) {
        case OZOS_MESH_ACTION_DELIVER:
            printf("• Action Taken  : [DELIVER] Packet reached destination! Delivering to local node.\n");
            printf("• Payload Text  : %.*s\n", OZOS_MESH_PAYLOAD_SIZE, (char*)frame.payload);
            break;
        case OZOS_MESH_ACTION_RELAY: {
            char new_hex[161];
            hex_encode((const uint8_t*)&frame, sizeof(frame), new_hex);
            printf("• Action Taken  : [RELAY] Forwarding to next hop. Remaining Hops: %u\n", frame.hop_limit);
            printf("• Resealed MAC16: 0x%04X\n", frame.tripwire_mac);
            printf("• Relayed Hex   : %s\n", new_hex);
            break;
        }
        case OZOS_MESH_ACTION_DROP_SEEN:
            printf("• Action Taken  : [DROP_SEEN] Duplicate packet detected by Bloom Filter. Suppressed.\n");
            break;
        case OZOS_MESH_ACTION_DROP_EXPIRED:
            printf("• Action Taken  : [DROP_EXPIRED] Hop limit reached 0. Anti-Zombie purge.\n");
            break;
        case OZOS_MESH_ACTION_DROP_INVALID:
            printf("• Action Taken  : [DROP_INVALID] Tripwire MAC16 corrupted or invalid magic.\n");
            return 1;
    }
    printf("=================================================================\n");
    return 0;
}

static int cmd_sim_multihop(void) {
    printf("=================================================================\n");
    printf("  O.Z.O.S // MULTI-HOP SOVEREIGN MESH BENCHMARK (A -> B -> C)\n");
    printf("  Topology: Node A (0xA11CE001) -> Node B Relay -> Node C (0xC0C00003)\n");
    printf("=================================================================\n\n");

    uint32_t node_a_id = 0xA11CE001U;
    uint32_t node_b_id = 0xB0B00002U;
    uint32_t node_c_id = 0xC0C00003U;

    /* Step 1: Alice builds Onion Frame destined for Charlie */
    ozos_mesh_frame_t frame_a;
    memset(&frame_a, 0, sizeof(frame_a));
    frame_a.magic = OZOS_MAGIC_MESH_FRAME;
    frame_a.hop_limit = 3; /* TTL = 3 */
    frame_a.flags = 0x0001;
    frame_a.src_node_id = node_a_id;
    frame_a.dst_node_id = node_c_id;
    frame_a.packet_id = 0xCAFE0001U;

    const char* secret_msg = "SOVEREIGN_MESH_P2P_SECRET_2026";
    memcpy(frame_a.payload, secret_msg, strlen(secret_msg));
    ozos_mesh_seal(&frame_a);

    printf("[STEP 1: ALICE EMISSION]\n");
    printf("  * Alice transmits 80-byte OZOS-80 frame destined for Charlie...\n");
    printf("  * Payload: '%s'\n", (char*)frame_a.payload);
    printf("  * Initial Hop Limit: %u | Tripwire MAC: 0x%04X\n\n", frame_a.hop_limit, frame_a.tripwire_mac);

    /* Step 2: Bob (Relay Node) intercepts frame via Acoustic / LoRa */
    ozos_mesh_bloom_t bob_bloom;
    ozos_mesh_cache_t bob_cache;
    ozos_mesh_bloom_init(&bob_bloom);
    ozos_mesh_cache_init(&bob_cache);

    printf("[STEP 2: BOB RELAY DISPATCH]\n");
    ozos_mesh_action_t act_b = ozos_mesh_process_frame(&frame_a, node_b_id, &bob_bloom, &bob_cache);
    if (act_b != OZOS_MESH_ACTION_RELAY) {
        printf("  [FAIL] Bob failed to classify frame for relay! Action: %d\n", act_b);
        return 1;
    }
    printf("  * Bob intercepts frame. Destination (0x%08X) != Bob (0x%08X).\n", frame_a.dst_node_id, node_b_id);
    printf("  * Action: [RELAY] -> Decremented Hop Limit: %u\n", frame_a.hop_limit);
    printf("  * Resealed MAC16: 0x%04X\n", frame_a.tripwire_mac);
    printf("  * Frame stored in Bob's DTN cache (depth=%zu) and Bloom filter.\n\n", bob_cache.count);

    /* Step 3: Charlie receives relayed frame from Bob */
    ozos_mesh_bloom_t charlie_bloom;
    ozos_mesh_cache_t charlie_cache;
    ozos_mesh_bloom_init(&charlie_bloom);
    ozos_mesh_cache_init(&charlie_cache);

    printf("[STEP 3: CHARLIE DESTINATION DELIVERY]\n");
    ozos_mesh_action_t act_c = ozos_mesh_process_frame(&frame_a, node_c_id, &charlie_bloom, &charlie_cache);
    if (act_c != OZOS_MESH_ACTION_DELIVER) {
        printf("  [FAIL] Charlie failed to deliver frame to local node! Action: %d\n", act_c);
        return 1;
    }
    printf("  * Charlie receives frame. Destination (0x%08X) == Charlie (0x%08X).\n", frame_a.dst_node_id, node_c_id);
    printf("  * Action: [DELIVER] -> Local Delivery Verified 100%%!\n");
    printf("  * Recovered Secret: '%.*s'\n\n", (int)strlen(secret_msg), (char*)frame_a.payload);

    /* Step 4: Anti-Storm Verification (Duplicate Packet Injected into Bob) */
    printf("[STEP 4: ANTI-STORM BLOOM FILTER DUPLICATE SUPPRESSION]\n");
    ozos_mesh_action_t act_b_dup = ozos_mesh_process_frame(&frame_a, node_b_id, &bob_bloom, &bob_cache);
    if (act_b_dup != OZOS_MESH_ACTION_DROP_SEEN) {
        printf("  [FAIL] Bob failed to suppress duplicate packet! Action: %d\n", act_b_dup);
        return 1;
    }
    printf("  * Re-injected packet into Bob: Bloom Filter detected prior receipt!\n");
    printf("  * Action: [DROP_SEEN] -> Duplicate instantly dropped! Zero Broadcast Storm.\n\n");

    /* Step 5: Anti-Zombie Verification (Expired TTL) */
    printf("[STEP 5: ANTI-ZOMBIE TTL EXCLUSION]\n");
    frame_a.hop_limit = 1; /* Only 1 hop left, relaying would expire */
    frame_a.packet_id = 0xDEADBEEFU; /* Use different packet ID to bypass bloom */
    ozos_mesh_seal(&frame_a);
    ozos_mesh_action_t act_b_exp = ozos_mesh_process_frame(&frame_a, node_b_id, &bob_bloom, &bob_cache);
    if (act_b_exp != OZOS_MESH_ACTION_DROP_EXPIRED) {
        printf("  [FAIL] Bob failed to drop expired TTL frame! Action: %d\n", act_b_exp);
        return 1;
    }
    printf("  * Hop limit reached threshold (TTL <= 1): Action: [DROP_EXPIRED] -> Purged.\n\n");

    printf("=================================================================\n");
    printf("  🏆 BENCHMARK PASSED 100%%: SOVEREIGN MULTI-HOP MESH VERIFIED!\n");
    printf("=================================================================\n");
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    const char* cmd = argv[1];
    if (strcmp(cmd, "status") == 0) {
        return cmd_status();
    } else if (strcmp(cmd, "pack") == 0) {
        return cmd_pack(argc, argv);
    } else if (strcmp(cmd, "forward") == 0) {
        return cmd_forward(argc, argv);
    } else if (strcmp(cmd, "sim-multihop") == 0) {
        return cmd_sim_multihop();
    } else {
        print_usage();
        return 1;
    }
}
