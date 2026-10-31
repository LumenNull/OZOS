#ifndef OZOS_GOSSIP_H
#define OZOS_GOSSIP_H

#include "libozos.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/*   [MẢNH GHÉP 4]: MẠNG LƯỚI LAN TRUYỀN TIN ĐỒN VÔ THANH                   */
/*   (ACOUSTIC GOSSIP MESH & MERKLE STATE SYNCHRONIZATION)                   */
/* ========================================================================= */

#define OZOS_MAGIC_GOSSIP_BEACON 0x4F5A4753 /* "OZGS" */
#define OZOS_MAGIC_GOSSIP_PULL   0x4F5A5055 /* "OZPU" */
#define OZOS_MAGIC_GOSSIP_DATA   0x4F5A4454 /* "OZDT" */

typedef enum {
    OZOS_GOSSIP_IN_SYNC = 0,         /* Hai nút hoàn toàn đồng thuận trạng thái */
    OZOS_GOSSIP_PEER_AHEAD = 1,      /* Nút láng giềng có trạng thái mới hơn (Cần PULL) */
    OZOS_GOSSIP_LOCAL_AHEAD = 2,     /* Trạng thái cục bộ mới hơn (Có thể PUSH) */
    OZOS_GOSSIP_FORK_DETECTED = 3    /* Phát hiện rẽ nhánh Merkle khác nhau ở cùng Epoch */
} ozos_gossip_action_t;

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;          /* 0x4F5A4753 ("OZGS") */
    uint32_t peer_id;        /* Mã định danh 4-byte của tác giả (CRC32 từ Ed25519 PK) */
    uint64_t merkle_root;    /* Gốc Merkle 64-bit đại diện cho toàn bộ trạng thái */
    uint32_t epoch_nonce;    /* Phiên bản số thứ tự tăng dần đơn điệu */
    uint16_t beacon_mac16;   /* Cổng gác kiểm tra toàn vẹn Tripwire MAC16 */
} ozos_gossip_beacon_t;      /* 22 Bytes */

typedef struct {
    uint32_t magic;          /* 0x4F5A5055 ("OZPU") */
    uint32_t requester_id;   /* ID của nút yêu cầu đồng bộ */
    uint32_t from_epoch;     /* Epoch hiện tại của nút yêu cầu */
    uint16_t pull_mac16;     /* Cổng gác kiểm tra toàn vẹn */
} ozos_gossip_pull_t;        /* 10 Bytes */
#pragma pack(pop)

/**
 * Đóng gói Xung Tim Lan Truyền Vô Thanh (Acoustic Gossip Beacon 22 Bytes)
 */
static inline void ozos_gossip_beacon_build(
    ozos_gossip_beacon_t* beacon,
    uint32_t peer_id,
    uint64_t merkle_root,
    uint32_t epoch_nonce
) {
    if (!beacon) return;
    beacon->magic = OZOS_MAGIC_GOSSIP_BEACON;
    beacon->peer_id = peer_id;
    beacon->merkle_root = merkle_root;
    beacon->epoch_nonce = epoch_nonce;

    uint16_t crc = 0xFFFF;
    const uint8_t* p = (const uint8_t*)beacon;
    for (size_t i = 0; i < 20; ++i) {
        crc ^= (uint16_t)p[i];
        for (int b = 0; b < 8; ++b) crc = (crc & 1) ? ((crc >> 1) ^ 0x8408) : (crc >> 1);
    }
    beacon->beacon_mac16 = crc;
}

/**
 * Thẩm định Xung Tim Lan Truyền Vô Thanh
 */
static inline bool ozos_gossip_beacon_verify(const ozos_gossip_beacon_t* beacon) {
    if (!beacon) return false;
    if (beacon->magic != OZOS_MAGIC_GOSSIP_BEACON) return false;

    uint16_t crc = 0xFFFF;
    const uint8_t* p = (const uint8_t*)beacon;
    for (size_t i = 0; i < 20; ++i) {
        crc ^= (uint16_t)p[i];
        for (int b = 0; b < 8; ++b) crc = (crc & 1) ? ((crc >> 1) ^ 0x8408) : (crc >> 1);
    }
    return (beacon->beacon_mac16 == crc);
}

/**
 * Thuật toán Phân xử Đồng thuận Trạng thái Merkle (Anti-Entropy Comparison)
 */
static inline ozos_gossip_action_t ozos_gossip_compare_state(
    uint64_t local_merkle_root,
    uint32_t local_epoch,
    const ozos_gossip_beacon_t* remote_beacon
) {
    if (!remote_beacon) return OZOS_GOSSIP_IN_SYNC;

    if (remote_beacon->epoch_nonce == local_epoch) {
        if (remote_beacon->merkle_root == local_merkle_root) {
            return OZOS_GOSSIP_IN_SYNC;
        } else {
            return OZOS_GOSSIP_FORK_DETECTED;
        }
    } else if (remote_beacon->epoch_nonce > local_epoch) {
        return OZOS_GOSSIP_PEER_AHEAD;
    } else {
        return OZOS_GOSSIP_LOCAL_AHEAD;
    }
}

/**
 * Đóng gói Yêu cầu Kéo Cập Nhật (Gossip Pull Request 10 Bytes)
 */
static inline void ozos_gossip_pull_build(
    ozos_gossip_pull_t* pull,
    uint32_t my_peer_id,
    uint32_t my_current_epoch
) {
    if (!pull) return;
    pull->magic = OZOS_MAGIC_GOSSIP_PULL;
    pull->requester_id = my_peer_id;
    pull->from_epoch = my_current_epoch;

    uint16_t crc = 0xFFFF;
    const uint8_t* p = (const uint8_t*)pull;
    for (size_t i = 0; i < 8; ++i) {
        crc ^= (uint16_t)p[i];
        for (int b = 0; b < 8; ++b) crc = (crc & 1) ? ((crc >> 1) ^ 0x8408) : (crc >> 1);
    }
    pull->pull_mac16 = crc;
}

#ifdef __cplusplus
}
#endif

#endif /* OZOS_GOSSIP_H */
