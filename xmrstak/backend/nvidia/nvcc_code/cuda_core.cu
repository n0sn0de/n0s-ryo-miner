/*
 * Copyright (C) 2017-2019 fireice-uk, psychocrypt
 * Copyright (C) 2026 n0sn0de contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "xmrstak/backend/cryptonight.hpp"

#include <bitset>
#include <cuda.h>
#include <cuda_runtime.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "xmrstak/backend/nvidia/nvcc_code/cuda_cryptonight_gpu.hpp"
#include "xmrstak/backend/nvidia/nvcc_code/cuda_fast_int_math_v2.hpp"
#include "xmrstak/jconf.hpp"

#ifdef _WIN32
#include <windows.h>
extern "C" void compat_usleep(uint64_t waitTime)
{
	if(waitTime > 0)
	{
		if(waitTime > 100)
		{
			HANDLE timer;
			LARGE_INTEGER ft;

			ft.QuadPart = -10ll * int64_t(waitTime);

			timer = CreateWaitableTimer(NULL, TRUE, NULL);
			SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
			WaitForSingleObject(timer, INFINITE);
			CloseHandle(timer);
		}
		else
		{
			LARGE_INTEGER perfCnt, start, now;
			__int64 elapsed;

			QueryPerformanceFrequency(&perfCnt);
			QueryPerformanceCounter(&start);
			do
			{
				SwitchToThread();
				QueryPerformanceCounter((LARGE_INTEGER*)&now);
				elapsed = (__int64)((now.QuadPart - start.QuadPart) / (float)perfCnt.QuadPart * 1000 * 1000);
			} while(elapsed < waitTime);
		}
	}
}
#else
#include <unistd.h>
extern "C" void compat_usleep(uint64_t waitTime)
{
	usleep(waitTime);
}
#endif

#include "cryptonight.hpp"
#include "cuda_aes.hpp"
#include "cuda_device.hpp"
#include "cuda_extra.hpp"

/* sm_2X is limited to 2GB due to the small TLB
 * therefore we never use 64bit indices
 */
#if defined(XMR_STAK_LARGEGRID) && (__CUDA_ARCH__ >= 300)
typedef uint64_t IndexType;
#else
typedef int IndexType;
#endif

__device__ __forceinline__ uint64_t cuda_mul128(uint64_t multiplier, uint64_t multiplicand, uint64_t& product_hi)
{
	product_hi = __umul64hi(multiplier, multiplicand);
	return (multiplier * multiplicand);
}

__global__ void cryptonight_core_gpu_phase1(
	const uint32_t ITERATIONS, const size_t MEMORY,
	int threads, int bfactor, int partidx, uint32_t* __restrict__ long_state, uint32_t* __restrict__ ctx_state2, uint32_t* __restrict__ ctx_key1)
{
	__shared__ uint32_t sharedMemory[1024];

	cn_aes_gpu_init(sharedMemory);
	__syncthreads();

	const int thread = (blockDim.x * blockIdx.x + threadIdx.x) >> 3;
	const int sub = (threadIdx.x & 7) << 2;

	const int batchsize = MEMORY >> bfactor;
	const int start = partidx * batchsize;
	const int end = start + batchsize;

	if(thread >= threads)
		return;

	uint32_t key[40], text[4];

	MEMCPY8(key, ctx_key1 + thread * 40, 20);

	if(partidx == 0)
	{
		// first round
		MEMCPY8(text, ctx_state2 + thread * 50 + sub + 16, 2);
	}
	else
	{
		// load previous text data
		MEMCPY8(text, &long_state[((uint64_t)thread * MEMORY) + sub + start - 32], 2);
	}
	__syncthreads();
	for(int i = start; i < end; i += 32)
	{
		cn_aes_pseudo_round_mut(sharedMemory, text, key);
		MEMCPY8(&long_state[((uint64_t)thread * MEMORY) + (sub + i)], text, 2);
	}
}

/** avoid warning `unused parameter` */
template <typename T>
__forceinline__ __device__ void unusedVar(const T&)
{
}

/** shuffle data for all compute architectures */
template <size_t group_n>
__forceinline__ __device__ uint32_t shuffle(volatile uint32_t* ptr, const uint32_t sub, const int val, const uint32_t src)
{
#if(__CUDA_ARCH__ < 300)
	ptr[sub] = val;
	return ptr[src & (group_n - 1)];
#else
	unusedVar(ptr);
	unusedVar(sub);
#if(__CUDACC_VER_MAJOR__ >= 9)
	return __shfl_sync(__activemask(), val, src, group_n);
#else
	return __shfl(val, src, group_n);
#endif
#endif
}

template <size_t group_n>
__forceinline__ __device__ uint64_t shuffle64(volatile uint32_t* ptr, const uint32_t sub, const int val, const uint32_t src, const uint32_t src2)
{
	uint64_t tmp;
	((uint32_t*)&tmp)[0] = shuffle<group_n>(ptr, sub, val, src);
	((uint32_t*)&tmp)[1] = shuffle<group_n>(ptr, sub, val, src2);
	return tmp;
}

/** cryptonight_gpu phase3 — final hash calculation */
__global__ void cryptonight_core_gpu_phase3(
	const uint32_t ITERATIONS, const size_t MEMORY,
	int threads, int bfactor, int partidx, uint32_t* long_stateIn, const uint32_t* const __restrict__ d_ctx_stateIn, uint32_t* __restrict__ d_ctx_key2)
{
	__shared__ uint32_t sharedMemoryX[256 * 32];

	const int twidx = (threadIdx.x * 4) % 128;
	char* sharedMemory = (char*)sharedMemoryX + twidx;

	cn_aes_gpu_init32(sharedMemoryX);
	__syncthreads();

	int thread = (blockDim.x * blockIdx.x + threadIdx.x) >> 3;
	int subv = (threadIdx.x & 7);
	int sub = subv << 2;

	const int batchsize = MEMORY >> bfactor;
	const int start = (partidx % (1 << bfactor)) * batchsize;
	const int end = start + batchsize;

	if(thread >= threads)
		return;

	const uint32_t* const long_state = long_stateIn + ((IndexType)thread * MEMORY) + sub;

	uint32_t key[40], text[4];
	#pragma unroll 10
	for(int j = 0; j < 10; ++j)
		((ulonglong4*)key)[j] = ((ulonglong4*)(d_ctx_key2 + thread * 40))[j];

	uint64_t* d_ctx_state = (uint64_t*)(d_ctx_stateIn + thread * 50 + sub + 16);
	#pragma unroll 2
	for(int j = 0; j < 2; ++j)
		((uint64_t*)text)[j] = loadGlobal64<uint64_t>(d_ctx_state + j);

	__syncthreads();

#if(__CUDA_ARCH__ < 300)
	extern __shared__ uint32_t shuffleMem[];
	volatile uint32_t* sPtr = (volatile uint32_t*)(shuffleMem + (threadIdx.x & 0xFFFFFFF8));
#else
	volatile uint32_t* sPtr = NULL;
#endif

	for(int i = start; i < end; i += 32)
	{
		uint32_t tmp[4];
		((ulonglong2*)(tmp))[0] =  ((ulonglong2*)(long_state + i))[0];
		#pragma unroll 4
		for(int j = 0; j < 4; ++j)
			text[j] ^= tmp[j];

		((uint4*)text)[0] = cn_aes_pseudo_round_mut32((uint32_t*)sharedMemory, ((uint4*)text)[0], (uint4*)key);

		// cryptonight_gpu requires the extra shuffle step
		{
			uint32_t tmp[4];
			#pragma unroll 4
			for(int j = 0; j < 4; ++j)
				tmp[j] = shuffle<8>(sPtr, subv, text[j], (subv + 1) & 7);
			#pragma unroll 4
			for(int j = 0; j < 4; ++j)
				text[j] ^= tmp[j];
		}
	}

	#pragma unroll 2
	for(int j = 0; j < 2; ++j)
		storeGlobal64<uint64_t>(d_ctx_state + j, ((uint64_t*)text)[j]);
}

/** cryptonight_gpu hash — uses the GPU-specific kernel pipeline */
template <uint32_t MEM_MODE>
void cryptonight_core_gpu_hash_gpu(nvid_ctx* ctx, uint32_t nonce, const xmrstak_algo& algo)
{
	const uint32_t MASK = algo.Mask();
	const uint32_t ITERATIONS = algo.Iter();
	const size_t MEM = algo.Mem();

	dim3 grid(ctx->device_blocks);
	dim3 block(ctx->device_threads);
	dim3 block8(ctx->device_threads << 3);

	size_t intensity = ctx->device_blocks * ctx->device_threads;

	CUDA_CHECK_KERNEL(
		ctx->device_id,
		xmrstak::nvidia::cn_explode_gpu<<<intensity, 128>>>(MEM, (int*)ctx->d_ctx_state, (int*)ctx->d_long_state));

	int partcount = 1 << ctx->device_bfactor;
	for(int i = 0; i < partcount; i++)
	{
		CUDA_CHECK_KERNEL(
			ctx->device_id,
			// 36 x 16byte x numThreads
			xmrstak::nvidia::cryptonight_core_gpu_phase2_gpu<<<ctx->device_blocks, ctx->device_threads * 16, 33 * 16 * ctx->device_threads>>>(
				ITERATIONS,
				MEM,
				MASK,
				(int*)ctx->d_ctx_state,
				(int*)ctx->d_long_state,
				ctx->device_bfactor,
				i,
				ctx->d_ctx_a,
				ctx->d_ctx_b));
	}

	/* bfactor for phase 3 — phase 3 is lighter, start splitting at bfactor >= 8 */
	int bfactorOneThree = ctx->device_bfactor - 8;
	if(bfactorOneThree < 0)
		bfactorOneThree = 0;

	int partcountOneThree = 1 << bfactorOneThree;
	// cryptonight_gpu uses two full rounds over the scratchpad memory
	int roundsPhase3 = partcountOneThree * 2;

	int blockSizePhase3 = block8.x;
	int gridSizePhase3 = grid.x;
	if(blockSizePhase3 * 2 <= ctx->device_maxThreadsPerBlock)
	{
		blockSizePhase3 *= 2;
		gridSizePhase3 = (gridSizePhase3 + 1) / 2;
	}

	for(int i = 0; i < roundsPhase3; i++)
	{
		CUDA_CHECK_KERNEL(ctx->device_id, cryptonight_core_gpu_phase3<<<
											  gridSizePhase3,
											  blockSizePhase3,
											  blockSizePhase3 * sizeof(uint32_t) * static_cast<int>(ctx->device_arch[0] < 3)>>>(
											  ITERATIONS,
											  MEM / 4,
											  ctx->device_blocks * ctx->device_threads,
											  bfactorOneThree, i,
											  ctx->d_long_state,
											  ctx->d_ctx_state, ctx->d_ctx_key2));
	}
}

void cryptonight_core_cpu_hash(nvid_ctx* ctx, const xmrstak_algo& miner_algo, uint32_t startNonce, uint64_t chain_height)
{
	if(miner_algo == invalid_algo)
		return;

	// Only cryptonight_gpu is supported
	if(ctx->memMode == 1)
		cryptonight_core_gpu_hash_gpu<1>(ctx, startNonce, miner_algo);
	else
		cryptonight_core_gpu_hash_gpu<0>(ctx, startNonce, miner_algo);
}
