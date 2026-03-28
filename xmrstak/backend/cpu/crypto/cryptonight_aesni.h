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
  *
  */
#pragma once

#include "../../miner_work.hpp"
#include "cn_gpu.hpp"
#include "cryptonight.h"
#include "xmrstak/backend/cryptonight.hpp"
#include <cfenv>
#include <memory.h>
#include <stdio.h>
#include <utility>

#ifdef _WIN64
#include <winsock2.h>
// this comment disable clang include reordering
#include <ntsecapi.h>
#include <tchar.h>
// this comment disable clang include reordering for windows.h
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#ifdef __GNUC__
#include <x86intrin.h>
static inline uint64_t _umul128(uint64_t a, uint64_t b, uint64_t* hi)
{
	unsigned __int128 r = (unsigned __int128)a * (unsigned __int128)b;
	*hi = r >> 64;
	return (uint64_t)r;
}

#else
#include <intrin.h>
#endif // __GNUC__

#if !defined(_LP64) && !defined(_WIN64)
#error You are trying to do a 32-bit build. This will all end in tears. I know it.
#endif

#include "soft_aes.hpp"

extern "C"
{
	void keccak(const uint8_t* in, int inlen, uint8_t* md, int mdlen);
	void keccakf(uint64_t st[25], int rounds);
	extern void (*const extra_hashes[4])(const void*, uint32_t, char*);
}

// This will shift and xor tmp1 into itself as 4 32-bit vals such as
// sl_xor(a1 a2 a3 a4) = a1 (a2^a1) (a3^a2^a1) (a4^a3^a2^a1)
static inline __m128i sl_xor(__m128i tmp1)
{
	__m128i tmp4;
	tmp4 = _mm_slli_si128(tmp1, 0x04);
	tmp1 = _mm_xor_si128(tmp1, tmp4);
	tmp4 = _mm_slli_si128(tmp4, 0x04);
	tmp1 = _mm_xor_si128(tmp1, tmp4);
	tmp4 = _mm_slli_si128(tmp4, 0x04);
	tmp1 = _mm_xor_si128(tmp1, tmp4);
	return tmp1;
}

template <uint8_t rcon>
static inline void aes_genkey_sub(__m128i* xout0, __m128i* xout2)
{
	__m128i xout1 = _mm_aeskeygenassist_si128(*xout2, rcon);
	xout1 = _mm_shuffle_epi32(xout1, 0xFF);
	*xout0 = sl_xor(*xout0);
	*xout0 = _mm_xor_si128(*xout0, xout1);
	xout1 = _mm_aeskeygenassist_si128(*xout0, 0x00);
	xout1 = _mm_shuffle_epi32(xout1, 0xAA);
	*xout2 = sl_xor(*xout2);
	*xout2 = _mm_xor_si128(*xout2, xout1);
}

static inline void soft_aes_genkey_sub(__m128i* xout0, __m128i* xout2, uint8_t rcon)
{
	__m128i xout1 = soft_aeskeygenassist(*xout2, rcon);
	xout1 = _mm_shuffle_epi32(xout1, 0xFF);
	*xout0 = sl_xor(*xout0);
	*xout0 = _mm_xor_si128(*xout0, xout1);
	xout1 = soft_aeskeygenassist(*xout0, 0x00);
	xout1 = _mm_shuffle_epi32(xout1, 0xAA);
	*xout2 = sl_xor(*xout2);
	*xout2 = _mm_xor_si128(*xout2, xout1);
}

template <bool SOFT_AES>
static inline void aes_genkey(const __m128i* memory, __m128i* k0, __m128i* k1, __m128i* k2, __m128i* k3,
	__m128i* k4, __m128i* k5, __m128i* k6, __m128i* k7, __m128i* k8, __m128i* k9)
{
	__m128i xout0, xout2;

	xout0 = _mm_load_si128(memory);
	xout2 = _mm_load_si128(memory + 1);
	*k0 = xout0;
	*k1 = xout2;

	if(SOFT_AES)
		soft_aes_genkey_sub(&xout0, &xout2, 0x01);
	else
		aes_genkey_sub<0x01>(&xout0, &xout2);
	*k2 = xout0;
	*k3 = xout2;

	if(SOFT_AES)
		soft_aes_genkey_sub(&xout0, &xout2, 0x02);
	else
		aes_genkey_sub<0x02>(&xout0, &xout2);
	*k4 = xout0;
	*k5 = xout2;

	if(SOFT_AES)
		soft_aes_genkey_sub(&xout0, &xout2, 0x04);
	else
		aes_genkey_sub<0x04>(&xout0, &xout2);
	*k6 = xout0;
	*k7 = xout2;

	if(SOFT_AES)
		soft_aes_genkey_sub(&xout0, &xout2, 0x08);
	else
		aes_genkey_sub<0x08>(&xout0, &xout2);
	*k8 = xout0;
	*k9 = xout2;
}

static inline void aes_round(__m128i key, __m128i* x0, __m128i* x1, __m128i* x2, __m128i* x3, __m128i* x4, __m128i* x5, __m128i* x6, __m128i* x7)
{
	*x0 = _mm_aesenc_si128(*x0, key);
	*x1 = _mm_aesenc_si128(*x1, key);
	*x2 = _mm_aesenc_si128(*x2, key);
	*x3 = _mm_aesenc_si128(*x3, key);
	*x4 = _mm_aesenc_si128(*x4, key);
	*x5 = _mm_aesenc_si128(*x5, key);
	*x6 = _mm_aesenc_si128(*x6, key);
	*x7 = _mm_aesenc_si128(*x7, key);
}

static inline void soft_aes_round(__m128i key, __m128i* x0, __m128i* x1, __m128i* x2, __m128i* x3, __m128i* x4, __m128i* x5, __m128i* x6, __m128i* x7)
{
	*x0 = soft_aesenc(*x0, key);
	*x1 = soft_aesenc(*x1, key);
	*x2 = soft_aesenc(*x2, key);
	*x3 = soft_aesenc(*x3, key);
	*x4 = soft_aesenc(*x4, key);
	*x5 = soft_aesenc(*x5, key);
	*x6 = soft_aesenc(*x6, key);
	*x7 = soft_aesenc(*x7, key);
}

inline void mix_and_propagate(__m128i& x0, __m128i& x1, __m128i& x2, __m128i& x3, __m128i& x4, __m128i& x5, __m128i& x6, __m128i& x7)
{
	__m128i tmp0 = x0;
	x0 = _mm_xor_si128(x0, x1);
	x1 = _mm_xor_si128(x1, x2);
	x2 = _mm_xor_si128(x2, x3);
	x3 = _mm_xor_si128(x3, x4);
	x4 = _mm_xor_si128(x4, x5);
	x5 = _mm_xor_si128(x5, x6);
	x6 = _mm_xor_si128(x6, x7);
	x7 = _mm_xor_si128(x7, tmp0);
}

template <bool PREFETCH, xmrstak_algo_id ALGO>
void cn_explode_scratchpad_gpu(const uint8_t* input, uint8_t* output, const xmrstak_algo& algo)
{
	constexpr size_t hash_size = 200; // 25x8 bytes
	alignas(128) uint64_t hash[25];
	const size_t mem = algo.Mem();

	for(uint64_t i = 0; i < mem / 512; i++)
	{
		memcpy(hash, input, hash_size);
		hash[0] ^= i;

		keccakf(hash, 24);
		memcpy(output, hash, 160);
		output += 160;

		keccakf(hash, 24);
		memcpy(output, hash, 176);
		output += 176;

		keccakf(hash, 24);
		memcpy(output, hash, 176);
		output += 176;

		if(PREFETCH)
		{
			_mm_prefetch((const char*)output - 512, _MM_HINT_T2);
			_mm_prefetch((const char*)output - 384, _MM_HINT_T2);
			_mm_prefetch((const char*)output - 256, _MM_HINT_T2);
			_mm_prefetch((const char*)output - 128, _MM_HINT_T2);
		}
	}
}

template <bool SOFT_AES, bool PREFETCH, xmrstak_algo_id ALGO>
void cn_implode_scratchpad(const __m128i* input, __m128i* output, const xmrstak_algo& algo)
{
	// cryptonight_gpu uses the heavy mix path
	constexpr bool HEAVY_MIX = (ALGO == cryptonight_gpu);

	__m128i xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7;
	__m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

	aes_genkey<SOFT_AES>(output + 2, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);

	xout0 = _mm_load_si128(output + 4);
	xout1 = _mm_load_si128(output + 5);
	xout2 = _mm_load_si128(output + 6);
	xout3 = _mm_load_si128(output + 7);
	xout4 = _mm_load_si128(output + 8);
	xout5 = _mm_load_si128(output + 9);
	xout6 = _mm_load_si128(output + 10);
	xout7 = _mm_load_si128(output + 11);

	const size_t MEM = algo.Mem();
	for(size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
	{
		if(PREFETCH)
			_mm_prefetch((const char*)input + i + 0, _MM_HINT_NTA);

		xout0 = _mm_xor_si128(_mm_load_si128(input + i + 0), xout0);
		xout1 = _mm_xor_si128(_mm_load_si128(input + i + 1), xout1);
		xout2 = _mm_xor_si128(_mm_load_si128(input + i + 2), xout2);
		xout3 = _mm_xor_si128(_mm_load_si128(input + i + 3), xout3);

		if(PREFETCH)
			_mm_prefetch((const char*)input + i + 4, _MM_HINT_NTA);

		xout4 = _mm_xor_si128(_mm_load_si128(input + i + 4), xout4);
		xout5 = _mm_xor_si128(_mm_load_si128(input + i + 5), xout5);
		xout6 = _mm_xor_si128(_mm_load_si128(input + i + 6), xout6);
		xout7 = _mm_xor_si128(_mm_load_si128(input + i + 7), xout7);

		if(SOFT_AES)
		{
			soft_aes_round(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			soft_aes_round(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			soft_aes_round(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			soft_aes_round(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			soft_aes_round(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			soft_aes_round(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			soft_aes_round(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			soft_aes_round(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			soft_aes_round(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			soft_aes_round(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		}
		else
		{
			aes_round(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		}

		if(HEAVY_MIX)
			mix_and_propagate(xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7);
	}

	if(HEAVY_MIX)
	{
		for(size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
		{
			if(PREFETCH)
				_mm_prefetch((const char*)input + i + 0, _MM_HINT_NTA);

			xout0 = _mm_xor_si128(_mm_load_si128(input + i + 0), xout0);
			xout1 = _mm_xor_si128(_mm_load_si128(input + i + 1), xout1);
			xout2 = _mm_xor_si128(_mm_load_si128(input + i + 2), xout2);
			xout3 = _mm_xor_si128(_mm_load_si128(input + i + 3), xout3);

			if(PREFETCH)
				_mm_prefetch((const char*)input + i + 4, _MM_HINT_NTA);

			xout4 = _mm_xor_si128(_mm_load_si128(input + i + 4), xout4);
			xout5 = _mm_xor_si128(_mm_load_si128(input + i + 5), xout5);
			xout6 = _mm_xor_si128(_mm_load_si128(input + i + 6), xout6);
			xout7 = _mm_xor_si128(_mm_load_si128(input + i + 7), xout7);

			if(SOFT_AES)
			{
				soft_aes_round(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			}
			else
			{
				aes_round(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			}

			if(HEAVY_MIX)
				mix_and_propagate(xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7);
		}

		for(size_t i = 0; i < 16; i++)
		{
			if(SOFT_AES)
			{
				soft_aes_round(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				soft_aes_round(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			}
			else
			{
				aes_round(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
				aes_round(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			}

			mix_and_propagate(xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7);
		}
	}

	_mm_store_si128(output + 4, xout0);
	_mm_store_si128(output + 5, xout1);
	_mm_store_si128(output + 6, xout2);
	_mm_store_si128(output + 7, xout3);
	_mm_store_si128(output + 8, xout4);
	_mm_store_si128(output + 9, xout5);
	_mm_store_si128(output + 10, xout6);
	_mm_store_si128(output + 11, xout7);
}

inline void set_float_rounding_mode_nearest()
{
#ifdef _MSC_VER
	_control87(RC_NEAR, MCW_RC);
#else
	std::fesetround(FE_TONEAREST);
#endif
}

struct Cryptonight_hash_gpu
{
	static constexpr size_t N = 1;

	template <xmrstak_algo_id ALGO, bool SOFT_AES, bool PREFETCH>
	static void hash(const void* input, size_t len, void* output, cryptonight_ctx** ctx, const xmrstak_algo& algo)
	{
		set_float_rounding_mode_nearest();
		keccak((const uint8_t*)input, len, ctx[0]->hash_state, 200);
		cn_explode_scratchpad_gpu<PREFETCH, ALGO>(ctx[0]->hash_state, ctx[0]->long_state, algo);

		if(cngpu_check_avx2())
			cn_gpu_inner_avx(ctx[0]->hash_state, ctx[0]->long_state, algo);
		else
			cn_gpu_inner_ssse3(ctx[0]->hash_state, ctx[0]->long_state, algo);

		cn_implode_scratchpad<SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[0]->long_state, (__m128i*)ctx[0]->hash_state, algo);
		keccakf((uint64_t*)ctx[0]->hash_state, 24);
		memcpy(output, ctx[0]->hash_state, 32);
	}
};
