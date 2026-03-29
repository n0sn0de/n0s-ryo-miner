/**
 * n0s/algorithm/cn_gpu.hpp — CryptoNight-GPU Algorithm Constants & Types
 *
 * This is the single source of truth for all CN-GPU algorithm parameters.
 * Both CUDA and OpenCL backends should reference these constants.
 *
 * The algorithm is documented in detail in docs/CN-GPU-WHITEPAPER.md
 *
 * CRITICAL: These values are protocol-level constants. Changing any of them
 * will produce different hashes and break consensus. The only exception is
 * ALGO_ID which is an internal ABI contract (see static_assert below).
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace n0s::cn_gpu {

// ============================================================
// Core Algorithm Constants
// ============================================================

/// Scratchpad size: 2 MiB (2 * 1024 * 1024 bytes)
constexpr size_t SCRATCHPAD_SIZE = 2 * 1024 * 1024;

/// Number of Phase 3 iterations (the GPU-heavy floating-point loop)
constexpr uint32_t ITERATIONS = 0xC000;  // 49,152

/// Address mask for 64-byte aligned scratchpad access in Phase 3
constexpr uint32_t ADDRESS_MASK = 0x1FFFC0;

/// Threads cooperating on a single hash (Phase 3 GPU kernel)
constexpr uint32_t THREADS_PER_HASH = 16;

/// Groups of 4 threads within the 16-thread hash team
constexpr uint32_t GROUPS_PER_HASH = 4;

/// Keccak hash state size in bytes (25 x 8 = 200)
constexpr size_t HASH_STATE_SIZE = 200;

/// Final output hash size in bytes
constexpr size_t OUTPUT_HASH_SIZE = 32;

/// Internal algorithm ID — ABI contract with OpenCL kernel (#define ALGO)
/// and CUDA kernel (-DALGO=). MUST remain 13. NEVER change this.
constexpr int ALGO_ID = 13;
static_assert(ALGO_ID == 13, "ALGO_ID must be 13 — ABI contract with GPU kernels");

// ============================================================
// Scratchpad Expansion Constants
// ============================================================

/// Bytes produced per keccak-based expansion round (160 + 176 + 176 = 512)
constexpr size_t EXPANSION_CHUNK_SIZE = 512;

/// Number of expansion chunks to fill the scratchpad
constexpr size_t EXPANSION_CHUNKS = SCRATCHPAD_SIZE / EXPANSION_CHUNK_SIZE;
static_assert(SCRATCHPAD_SIZE % EXPANSION_CHUNK_SIZE == 0,
    "Scratchpad size must be evenly divisible by expansion chunk size");

// ============================================================
// Phase 3: Floating-Point Computation Constants
// ============================================================

/**
 * SHUFFLE_PATTERN — Cross-thread data dependency for Phase 3
 *
 * Each of the 16 threads reads data from 4 specific threads (including itself)
 * determined by this lookup table. This creates the cross-thread dependency
 * chain that makes the algorithm GPU-friendly (warp-level data sharing).
 *
 * Index: thread ID [0-15]
 * Value: array of 4 source thread IDs to read from
 */
constexpr std::array<std::array<uint32_t, 4>, 16> SHUFFLE_PATTERN = {{
    {0, 1, 2, 3},
    {0, 2, 3, 1},
    {0, 3, 1, 2},
    {0, 3, 2, 1},

    {1, 0, 2, 3},
    {1, 2, 3, 0},
    {1, 3, 0, 2},
    {1, 3, 2, 0},

    {2, 1, 0, 3},
    {2, 0, 3, 1},
    {2, 3, 1, 0},
    {2, 3, 0, 1},

    {3, 1, 2, 0},
    {3, 2, 0, 1},
    {3, 0, 1, 2},
    {3, 0, 2, 1},
}};

/**
 * THREAD_CONSTANTS — Per-thread initial counter values for Phase 3
 *
 * These exact IEEE 754 float32 values seed each thread's floating-point
 * accumulator chain. They break symmetry between threads and ensure
 * the computation is non-trivial even for uniform inputs.
 *
 * All values are in the range [1.25, 1.5] — chosen to keep the mantissa
 * in a numerically stable region.
 */
constexpr std::array<float, 16> THREAD_CONSTANTS = {{
    1.3437500f,
    1.2812500f,
    1.3593750f,
    1.3671875f,

    1.4296875f,
    1.3984375f,
    1.3828125f,
    1.3046875f,

    1.4140625f,
    1.2734375f,
    1.2578125f,
    1.2890625f,

    1.3203125f,
    1.3515625f,
    1.3359375f,
    1.4609375f,
}};

/**
 * FP_FEEDBACK_CONSTANT — Added to accumulator each sub_round
 *
 * This constant (0.734375 = 47/64) provides a steady-state drift to the
 * floating-point accumulator chain, preventing convergence to zero.
 */
constexpr float FP_FEEDBACK_CONSTANT = 0.734375f;

/**
 * FP_RESULT_SCALE — Converts normalized float result to integer
 *
 * After the floating-point computation produces a value in [2.0, 4.0),
 * it's multiplied by this constant (536870880 ≈ 2^29 - 32) to produce
 * a 32-bit integer index. The specific value avoids exact powers of 2
 * to prevent degenerate bit patterns.
 */
constexpr float FP_RESULT_SCALE = 536870880.0f;

/**
 * FP_NORMALIZE_SCALE — Converts accumulator sum to index contribution
 *
 * The sum of accumulators is multiplied by 16777216 (2^24) to extract
 * the significant mantissa bits into an integer range.
 */
constexpr float FP_NORMALIZE_SCALE = 16777216.0f;

/**
 * FP_RANGE_DIVISOR — Normalizes accumulator to [0, 1) range
 *
 * After absolute value, the accumulator is divided by 64.0 to bring
 * it into the [0, 1) range for the next iteration's seed value.
 */
constexpr float FP_RANGE_DIVISOR = 64.0f;

// ============================================================
// Phase 4: Implode Constants
// ============================================================

/// Number of AES rounds per scratchpad pass
constexpr int AES_ROUNDS_PER_PASS = 10;

/// Number of extra AES+mix rounds after scratchpad compression
/// (cn_gpu uses HEAVY_MIX: 2 passes over scratchpad + 16 extra rounds)
constexpr int HEAVY_MIX_EXTRA_ROUNDS = 16;

/// Number of full scratchpad passes in implode (HEAVY_MIX mode)
constexpr int IMPLODE_PASSES = 2;

// ============================================================
// Bit Manipulation Masks (IEEE 754 float32)
// ============================================================

/// Mask to clear exponent bit 24 (break FMA dependency chain)
constexpr uint32_t FMA_BREAK_AND_MASK = 0xFEFFFFFF;

/// Mask to set exponent to 01xxxxxx (ensure value in [1.0, 2.0))
constexpr uint32_t FMA_BREAK_OR_MASK = 0x00800000;

/// Mask to extract sign + mantissa (clear exponent)
constexpr uint32_t MANTISSA_MASK = 0x807FFFFF;

/// Mask to set exponent to 10000000 (ensure value in [2.0, 4.0))
constexpr uint32_t RANGE_CLAMP_MASK = 0x40000000;

/// Mask to clear sign bit and set minimum abs value > 2.0
/// (prevents division by zero in round_compute)
constexpr uint32_t DIV_SAFE_AND_MASK = 0xFF7FFFFF;

/// Mask for absolute value (clear float sign bit)
constexpr uint32_t ABS_MASK = 0x7FFFFFFF;

}  // namespace n0s::cn_gpu
