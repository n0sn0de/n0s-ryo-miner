/**
 * cuda_phase1.hpp — Phase 1: Hash state preparation
 *
 * Prepares the initial hash state from the input blob via Keccak-1600 and AES key expansion.
 */
#pragma once

#include "cuda_context.hpp"
#include "n0s/backend/cryptonight.hpp"

#include <cuda_runtime.h>

// Phase 1 kernel launch wrapper (called from dispatch)
template <n0s_algo_id ALGO>
__global__ void cryptonight_extra_gpu_prepare(
	int threads,
	uint32_t* __restrict__ d_input,
	uint32_t len,
	uint32_t startNonce,
	uint32_t* __restrict__ d_ctx_state,
	uint32_t* __restrict__ d_ctx_state2,
	uint32_t* __restrict__ d_ctx_a,
	uint32_t* __restrict__ d_ctx_b,
	uint32_t* __restrict__ d_ctx_key1,
	uint32_t* __restrict__ d_ctx_key2);
