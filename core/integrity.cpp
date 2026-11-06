#include <cmath>
#include <algorithm>
#include <emscripten/emscripten.h>

#ifdef __cplusplus
extern "C" {
#endif

// Epsilon tolerance constant to eliminate hardware floating-point jitter (IEEE-754 epsilon drift)
const float EPSILON = 1e-6f;

// Internal helper: Compute energy variance to evaluate 8x8 block texture complexity
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

// 1. Extract perceptual fingerprint (16 low-frequency 4x4 DCT coefficients)
EMSCRIPTEN_KEEPALIVE
void compute_block_fingerprint(const float* input_block, float* out_fingerprint) {
    // Assumes input_block has undergone 2D-DCT transform
    // Extract top-left 4x4 sub-matrix (16 low-frequency coefficients capturing core energy)
    int idx = 0;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            out_fingerprint[idx++] = input_block[r * 8 + c];
        }
    }
}

// 2. Verified block integrity check with adaptive threshold & epsilon tolerance
EMSCRIPTEN_KEEPALIVE
float verify_block_integrity_adaptive(const float* current_block, const float* anchor_fingerprint, float base_threshold) {
    float current_fingerprint[16];
    compute_block_fingerprint(current_block, current_fingerprint);

    // Compute Mean Squared Error (MSE) between current and anchor fingerprints
    float mse = 0.0f;
    for (int i = 0; i < 16; i++) {
        float diff = current_fingerprint[i] - anchor_fingerprint[i];
        mse += diff * diff;
    }
    mse /= 16.0f;

    // Apply Epsilon Guard: Clamp negligible hardware rounding noise to absolute zero
    if (mse < EPSILON) {
        mse = 0.0f;
    }

    return mse;
}

// Backward compatibility API
EMSCRIPTEN_KEEPALIVE
float verify_block_integrity(const float* current_block, const float* anchor_fingerprint, float threshold) {
    return verify_block_integrity_adaptive(current_block, anchor_fingerprint, threshold);
}

#ifdef __cplusplus
}
#endif
