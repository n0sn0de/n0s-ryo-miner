/**
 * test_constants.cpp — Verify n0s::cn_gpu constants match original code
 *
 * This test ensures the extracted constants in n0s/algorithm/cn_gpu.hpp
 * are bit-exact with the values used in the original xmr-stak code (now n0s-ryo-miner).
 */
#include <cassert>
#include <cstdio>
#include <cstring>

#include "n0s/algorithm/cn_gpu.hpp"
#include "xmrstak/backend/cryptonight.hpp"

// Original CUDA __constant__ arrays (copied from cuda_cryptonight_gpu.hpp)
static const uint32_t orig_look[16][4] = {
    {0, 1, 2, 3}, {0, 2, 3, 1}, {0, 3, 1, 2}, {0, 3, 2, 1},
    {1, 0, 2, 3}, {1, 2, 3, 0}, {1, 3, 0, 2}, {1, 3, 2, 0},
    {2, 1, 0, 3}, {2, 0, 3, 1}, {2, 3, 1, 0}, {2, 3, 0, 1},
    {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 0, 1, 2}, {3, 0, 2, 1},
};

static const float orig_ccnt[16] = {
    1.34375f, 1.28125f, 1.359375f, 1.3671875f,
    1.4296875f, 1.3984375f, 1.3828125f, 1.3046875f,
    1.4140625f, 1.2734375f, 1.2578125f, 1.2890625f,
    1.3203125f, 1.3515625f, 1.3359375f, 1.4609375f,
};

int main() {
    using namespace n0s::cn_gpu;

    printf("=== Constants Verification ===\n\n");

    // Core constants
    assert(SCRATCHPAD_SIZE == CN_MEMORY);
    printf("✅ SCRATCHPAD_SIZE = %zu (matches CN_MEMORY)\n", SCRATCHPAD_SIZE);

    assert(ITERATIONS == CN_GPU_ITER);
    printf("✅ ITERATIONS = 0x%X (matches CN_GPU_ITER)\n", ITERATIONS);

    assert(ADDRESS_MASK == CN_GPU_MASK);
    printf("✅ ADDRESS_MASK = 0x%X (matches CN_GPU_MASK)\n", ADDRESS_MASK);

    assert(ALGO_ID == cryptonight_gpu);
    printf("✅ ALGO_ID = %d (matches cryptonight_gpu enum)\n", ALGO_ID);

    // Shuffle pattern
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 4; j++) {
            assert(SHUFFLE_PATTERN[i][j] == orig_look[i][j]);
        }
    }
    printf("✅ SHUFFLE_PATTERN matches orig look[16][4]\n");

    // Thread constants
    for (int i = 0; i < 16; i++) {
        uint32_t a, b;
        memcpy(&a, &THREAD_CONSTANTS[i], 4);
        memcpy(&b, &orig_ccnt[i], 4);
        assert(a == b);
    }
    printf("✅ THREAD_CONSTANTS matches orig ccnt[16] (bit-exact IEEE 754)\n");

    // Derived constants
    assert(EXPANSION_CHUNKS == 4096);
    printf("✅ EXPANSION_CHUNKS = %zu\n", EXPANSION_CHUNKS);

    // Float constants
    assert(FP_FEEDBACK_CONSTANT == 0.734375f);
    printf("✅ FP_FEEDBACK_CONSTANT = %.6f\n", FP_FEEDBACK_CONSTANT);
    assert(FP_RESULT_SCALE == 536870880.0f);
    printf("✅ FP_RESULT_SCALE = %.1f\n", FP_RESULT_SCALE);

    // Bit masks
    assert(FMA_BREAK_AND_MASK == 0xFEFFFFFF);
    assert(FMA_BREAK_OR_MASK == 0x00800000);
    assert(MANTISSA_MASK == 0x807FFFFF);
    assert(RANGE_CLAMP_MASK == 0x40000000);
    assert(DIV_SAFE_AND_MASK == 0xFF7FFFFF);
    printf("✅ All IEEE 754 manipulation masks verified\n");

    printf("\n=== All constants verified ✅ ===\n");
    return 0;
}
