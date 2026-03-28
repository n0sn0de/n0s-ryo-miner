#pragma once
#include <inttypes.h>
#include <stddef.h>

// STRIPPED: variant4_random_math.h removed (CryptonightR not supported)

#if defined _MSC_VER
#define ABI_ATTRIBUTE
#else
#define ABI_ATTRIBUTE __attribute__((ms_abi))
#endif

struct cryptonight_ctx;
struct xmrstak_algo; // Forward declaration

typedef void (*cn_mainloop_fun)(cryptonight_ctx* ctx);
typedef void (*cn_double_mainloop_fun)(cryptonight_ctx*, cryptonight_ctx*);
typedef void (*cn_hash_fun)(const void*, size_t, void*, cryptonight_ctx**, const xmrstak_algo&);

// STUB: CryptonightR support removed, v4_compile_code not needed for cn_gpu
// Stub implementation to satisfy compile-time requirements
inline void v4_compile_code(size_t N, cryptonight_ctx* ctx, int code_size) {
	// Empty stub - CryptonightR not supported
}

// STUB: extra_ctx_r kept for compile-time compatibility only (unused by cn_gpu)
// Minimal stub to satisfy cryptonight_aesni.h requirements
struct V4_Instruction_stub { uint8_t dummy[8]; };
#define NUM_INSTRUCTIONS_MAX 256
using V4_Instruction = V4_Instruction_stub;

struct extra_ctx_r
{
	uint64_t height = 0;
	V4_Instruction code[NUM_INSTRUCTIONS_MAX + 1]; // Stub array
};

// STUB: v4_random_math functions - empty stubs for compile-time compatibility
template<typename T>
inline void v4_random_math(V4_Instruction* code, const void* data) {
	// Empty stub - CryptonightR not supported
}

template<typename T>
inline int v4_random_math_init(V4_Instruction* code, uint64_t height) {
	// Empty stub - CryptonightR not supported
	return 0;
}

struct cryptonight_ctx
{
	uint8_t hash_state[224]; // Need only 200, explicit align
	uint8_t* long_state;
	uint8_t ctx_info[24]; //Use some of the extra memory for flags
	cn_mainloop_fun loop_fn = nullptr;
	cn_hash_fun hash_fn = nullptr;
	uint8_t* fun_data = nullptr;
	int asm_version = 0;
	xmrstak_algo last_algo = invalid_algo;

	union {
		extra_ctx_r cn_r_ctx;
	};

};

struct alloc_msg
{
	const char* warning;
};

size_t cryptonight_init(size_t use_fast_mem, size_t use_mlock, alloc_msg* msg);
cryptonight_ctx* cryptonight_alloc_ctx(size_t use_fast_mem, size_t use_mlock, alloc_msg* msg);
void cryptonight_free_ctx(cryptonight_ctx* ctx);
