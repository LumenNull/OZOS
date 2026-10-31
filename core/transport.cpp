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
    // Kiểm tra kích thước tối thiểu của Header
    if (buffer_size < sizeof(PacketHeader)) {
        return 2; 
    }

    PacketHeader header;
    // Đọc trực tiếp từ bộ nhớ đệm (Zero-copy layout mapping)
    std::memcpy(&header, buffer, sizeof(PacketHeader));

    // 1. Kiểm tra biên giới hệ thống (Magic Header)
    if (header.magic != GENESIS_MAGIC_HEADER) {
        return 1; 
    }

    // 2. Kiểm tra tính tuần tự & chống Replay Attack
    if (header.nonce <= last_seen_nonce) {
        return 3; 
    }

    // 3. Kiểm tra tính toàn vẹn tổng thể của Payload thực tế theo chiều dài khai báo
    if (buffer_size < sizeof(PacketHeader) + header.payload_length) {
        return 2; // Payload bị cụt / thiếu dữ liệu truyền tải
    }

    return 0; // Gói tin vượt qua mọi vòng kiểm duyệt của Tầng 2!
}

#ifdef __cplusplus
}
#endif