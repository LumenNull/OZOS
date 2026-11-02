#include <cstdint>
#include <cstring>
#include <emscripten/emscripten.h>

#ifdef __cplusplus
extern "C" {
#endif

// Định danh chuẩn cho hệ thống Genesis Blueprint (GENS v1)
const uint32_t GENESIS_MAGIC_HEADER = 0x47454E01; 

// Cấu trúc gói tin nhị phân chuẩn hóa (Binary Layout)
#pragma pack(push, 1)
struct PacketHeader {
    uint32_t magic;              // 4 bytes: Nhận diện hệ thống
    uint64_t nonce;              // 8 bytes: Chống Replay Attack (Sequence/Timestamp)
    float fingerprint[16];       // 64 bytes: 16 hệ số vân tay nhận thức 4x4 (Tầng 1)
    uint32_t payload_length;     // 4 bytes: Độ dài phần dữ liệu thực tế
};
#pragma pack(pop)

/**
 * Kiểm tra nhanh gói tin nhị phân khi vừa cập bến Tầng 2
 * @param buffer: Con trỏ trỏ trực tiếp đến vùng nhớ chứa gói tin trong WASM Heap
 * @param buffer_size: Tổng số bytes nhận được từ mạng
 * @param last_seen_nonce: Nonce lớn nhất đã ghi nhận trước đó để chống Replay Attack
 * @return Mã trạng thái:
 *         0: Hợp lệ tuyệt đối
 *         1: Sai Magic Header (Gói tin rác / Giả mạo)
 *         2: Lỗi kích thước gói tin (Buffer quá ngắn)
 *         3: Phát hiện Replay Attack (Nonce cũ hoặc trùng lặp)
 */
EMSCRIPTEN_KEEPALIVE
int parse_and_validate_packet(const uint8_t* buffer, size_t buffer_size, uint64_t last_seen_nonce) {
    // 1. Kiểm tra kích thước tối thiểu của Header
    if (buffer_size < sizeof(PacketHeader)) {
        return 2; 
    }

    PacketHeader header;
    std::memcpy(&header, buffer, sizeof(PacketHeader));

    // 2. Kiểm tra Magic Header
    if (header.magic != GENESIS_MAGIC_HEADER) {
        return 1; 
    }

    // 3. Chống tràn số học (WASM32 integer overflow)
    if (header.payload_length > 65535 || (sizeof(PacketHeader) + header.payload_length) < sizeof(PacketHeader)) {
        return 2;
    }

    // 4. Kiểm tra tính tuần tự & chống Replay Attack
    if (header.nonce <= last_seen_nonce) {
        return 3; 
    }

    // 5. Kiểm tra tính toàn vẹn kích thước: Chống cắt ngắn payload
    if (buffer_size < sizeof(PacketHeader) + header.payload_length) {
        return 2; // Payload bị cụt / thiếu dữ liệu
    }

    // 6. Chống rác nối đuôi (Trailing garbage)
    if (buffer_size > sizeof(PacketHeader) + header.payload_length) {
        return 4; // Rác nối đuôi
    }

    // 7. Kiểm tra tính toàn vẹn Payload (Chống sửa đổi 1 byte)
    if (header.payload_length > 0) {
        const uint8_t* payload_ptr = buffer + sizeof(PacketHeader);
        uint32_t calc_sum = 0x811C9DC5U;
        for (uint32_t i = 0; i < header.payload_length; ++i) {
            calc_sum = (calc_sum ^ payload_ptr[i]) * 0x01000193U;
        }
        union { float f; uint32_t u; } pun;
        pun.f = header.fingerprint[15];
        if (pun.u != 0 && pun.u != calc_sum) {
            return 5; // Dữ liệu payload bị sửa đổi
        }
    }

    return 0; // Gói tin vượt qua mọi vòng kiểm duyệt của Tầng 2!
}

#ifdef __cplusplus
}
#endif