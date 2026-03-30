/**
 * cuda_phase4_5.hpp — Phases 4 & 5: Scratchpad compression and finalization
 *
 * Phase 4: Implode scratchpad back into hash state via AES + mix_and_propagate
 * Phase 5: Final AES rounds + Keccak + target check
 */
#pragma once

#include "cuda_context.hpp"
#include "n0s/backend/cryptonight.hpp"

#include <cuda_runtime.h>

// Phase 4 kernel launch wrapper (called from dispatch)
template <n0s_algo_id ALGO>
__global__ void kernel_implode_scratchpad(
	const uint32_t ITERATIONS,
	const size_t MEMORY,
	int threads,
	int bfactor,
	int partidx,
	uint32_t* scratchpad_in,
	const uint32_t* const __restrict__ state_buffer_in,
	uint32_t* __restrict__ d_ctx_key2);

// Phase 5 kernel launch wrapper (called from dispatch)
template <n0s_algo_id ALGO>
__global__ void cryptonight_extra_gpu_final(
	int threads,
	uint64_t target,
	uint32_t* __restrict__ d_res_count,
	uint32_t* __restrict__ d_res_nonce,
	uint32_t* __restrict__ d_ctx_state,
	uint32_t* __restrict__ d_ctx_key2);
