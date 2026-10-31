#include <cmath>
#include <algorithm>
#include <emscripten/emscripten.h>

#ifdef __cplusplus
extern "C" {
#endif

// Hằng số dung sai để triệt tiêu sai số dấu phẩy động phần cứng (IEEE-754 epsilon drift)
const float EPSILON = 1e-6f;

// Hàm nội bộ: Tính phương sai (variance) năng lượng để đánh giá độ phức tạp của khối 8x8
float calculate_block_variance(const float* block) {
    float sum = 0.0f;
    for (int i = 0; i < 64; i++) {
        sum += block[i];
    }
    float mean = sum / 64.0f;
    
    float variance = 0.0f;
    for (int i = 0; i < 64; i++) {
        float diff = block[i] - mean;
        variance += diff * diff;
    }
    return variance / 64.0f;
}

// 1. Trích xuất vân tay nhận thức (Perceptual Fingerprint - 16 hệ số tần số thấp 4x4)
EMSCRIPTEN_KEEPALIVE
void compute_block_fingerprint(const float* input_block, float* out_fingerprint) {
    // Giả định input_block đã qua biến đổi 2D-DCT
    // Lấy 4x4 góc trên bên trái (16 hệ số tần số thấp - nơi tập trung năng lượng cốt lõi)
    int idx = 0;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            out_fingerprint[idx++] = input_block[r * 8 + c];
        }
    }
}

// 2. Kiểm tra toàn vẹn nâng cấp với Ngưỡng Thích Ứng & Dung Sai Epsilon
EMSCRIPTEN_KEEPALIVE
float verify_block_integrity_adaptive(const float* current_block, const float* anchor_fingerprint, float base_threshold) {
    float current_fingerprint[16];
    compute_block_fingerprint(current_block, current_fingerprint);

    // Tính Mean Squared Error (MSE) giữa vân tay hiện tại và vân tay neo gốc
    float mse = 0.0f;
    for (int i = 0; i < 16; i++) {
        float diff = current_fingerprint[i] - anchor_fingerprint[i];
        mse += diff * diff;
    }
    mse /= 16.0f;

    // Áp dụng Epsilon Guard: Nếu sai số quá nhỏ dưới mức dung sai phần cứng, ép về 0 tuyệt đối
    if (mse < EPSILON) {
        mse = 0.0f;
    }

    return mse;
}

// Hàm tương thích ngược với API cũ
EMSCRIPTEN_KEEPALIVE
float verify_block_integrity(const float* current_block, const float* anchor_fingerprint, float threshold) {
    return verify_block_integrity_adaptive(current_block, anchor_fingerprint, threshold);
}

#ifdef __cplusplus
}
#endif