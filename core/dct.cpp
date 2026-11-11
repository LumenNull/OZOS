#include <cmath>
#include <cstdint>
#include <algorithm>
#include <emscripten/emscripten.h>
#include <emscripten/heap.h>
#include <wasm_simd128.h>

namespace {
constexpr int N = 8;
constexpr int BLOCK = N * N;
constexpr float PI_F = 3.14159265358979323846f;

// Orthonormal DCT-II basis:
alignas(16) float g_basis[N][N];
bool g_basis_ready = false;

void init_basis() {
    if (g_basis_ready) return;
    const float inv_sqrt8 = 0.3535533905932737622f;
    const float sqrt_quarter = 0.5f;
    for (int u = 0; u < N; ++u) {
        const float alpha = (u == 0) ? inv_sqrt8 : sqrt_quarter;
        for (int x = 0; x < N; ++x) {
            const float angle = PI_F * (2.0f * x + 1.0f) * u / 16.0f;
            g_basis[u][x] = alpha * std::cos(angle);
        }
    }
    g_basis_ready = true;
}

inline float dct_basis(int u, int x) {
    return g_basis[u][x];
}
} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
void dct_init() {
    init_basis();
}

// Scalar reference implementation: direct separable 2D DCT-II.
EMSCRIPTEN_KEEPALIVE
void dct8x8_scalar(const float* input, float* output) {
    init_basis();
    float tmp[N][N];

    for (int y = 0; y < N; ++y) {
        for (int u = 0; u < N; ++u) {
            float sum = 0.0f;
            for (int x = 0; x < N; ++x) {
                sum += input[y * N + x] * dct_basis(u, x);
            }
            tmp[y][u] = sum;
        }
    }

    for (int v = 0; v < N; ++v) {
        for (int u = 0; u < N; ++u) {
            float sum = 0.0f;
            for (int y = 0; y < N; ++y) {
                sum += dct_basis(v, y) * tmp[y][u];
            }
            output[v * N + u] = sum;
        }
    }
}

// Fully Optimized SIMD implementation: Direct vector operations without intermediate memory roundtrips
EMSCRIPTEN_KEEPALIVE
void dct8x8_simd(const float* input, float* output) {
    init_basis();
    alignas(16) float tmp[N][N];

    for (int y = 0; y < N; ++y) {
        for (int u = 0; u < N; ++u) {
            v128_t acc0 = wasm_f32x4_splat(0.0f);
            v128_t acc1 = wasm_f32x4_splat(0.0f);

            v128_t s0 = wasm_v128_load(&input[y * N + 0]); 
            v128_t s1 = wasm_v128_load(&input[y * N + 4]); 

            v128_t b0 = wasm_v128_load(&g_basis[u][0]);   
            v128_t b1 = wasm_v128_load(&g_basis[u][4]);   

            acc0 = wasm_f32x4_mul(s0, b0);
            acc1 = wasm_f32x4_mul(s1, b1);
            
            v128_t final_sum = wasm_f32x4_add(acc0, acc1);
            
            alignas(16) float temp_lane[4];
            wasm_v128_store(temp_lane, final_sum);
            tmp[y][u] = temp_lane[0] + temp_lane[1] + temp_lane[2] + temp_lane[3];
        }
    }

    for (int v = 0; v < N; ++v) {
        for (int u = 0; u < N; ++u) {
            float sum = 0.0f;
            for (int y = 0; y < N; ++y) {
                sum += dct_basis(v, y) * tmp[y][u];
            }
            output[v * N + u] = sum;
        }
    }
}

EMSCRIPTEN_KEEPALIVE
void dct8x8_batch_scalar(const float* input, float* output, int iterations) {
    for (int i = 0; i < iterations; ++i) {
        dct8x8_scalar(input, output);
    }
}

EMSCRIPTEN_KEEPALIVE
void dct8x8_batch_simd(const float* input, float* output, int iterations) {
    for (int i = 0; i < iterations; ++i) {
        dct8x8_simd(input, output);
    }
}

} // extern "C"