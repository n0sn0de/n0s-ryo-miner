/**
 * cryptonight_common.cpp — Memory allocation for CryptoNight-GPU
 *
 * Handles scratchpad memory allocation with huge page support:
 *   Linux:   mmap + MAP_HUGETLB (2 MB pages)
 *   Windows: VirtualAlloc + MEM_LARGE_PAGES
 *
 * cn_gpu uses a 2MB scratchpad per hash.
 *
 * extra_hashes (Blake/Groestl/JH/Skein) removed — cn_gpu outputs first
 * 32 bytes of Keccak state directly without branch dispatch.
 */

#include "cryptonight.h"
#include "cryptonight_aesni.h"
#include "n0s/backend/cryptonight.hpp"
#include "n0s/jconf.hpp"
#include "n0s/misc/console.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <sys/mman.h>
#endif

#ifdef __GNUC__
#include <mm_malloc.h>
#endif

#ifdef _MSC_VER
// MSVC: _mm_malloc is in <malloc.h>
#include <malloc.h>
#endif

size_t cryptonight_init([[maybe_unused]] size_t use_fast_mem, [[maybe_unused]] size_t use_mlock, [[maybe_unused]] alloc_msg* msg)
{
#ifdef _WIN32
	if(use_fast_mem)
	{
		// Enable SeLockMemoryPrivilege for large pages on Windows
		HANDLE hToken;
		if(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		{
			TOKEN_PRIVILEGES tp;
			tp.PrivilegeCount = 1;
			tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			if(LookupPrivilegeValue(nullptr, SE_LOCK_MEMORY_NAME, &tp.Privileges[0].Luid))
				AdjustTokenPrivileges(hToken, FALSE, &tp, 0, nullptr, nullptr);
			CloseHandle(hToken);
		}
	}
#endif
	return 1;
}

cryptonight_ctx* cryptonight_alloc_ctx(size_t use_fast_mem, size_t use_mlock, alloc_msg* msg)
{
	const size_t hashMemSize = ::jconf::inst()->GetMiningMemSize();

	cryptonight_ctx* ptr = static_cast<cryptonight_ctx*>(_mm_malloc(sizeof(cryptonight_ctx), 4096));

	if(use_fast_mem == 0)
	{
		// Fallback: standard aligned allocation (no huge pages)
		ptr->long_state = static_cast<uint8_t*>(_mm_malloc(hashMemSize, hashMemSize));
		ptr->ctx_info[0] = 0;
		ptr->ctx_info[1] = 0;
		if(ptr->long_state == nullptr)
			printer::inst()->print_msg(L0, "MEMORY ALLOC FAILED: _mm_malloc was not able to allocate %s byte",
				std::to_string(hashMemSize).c_str());
		return ptr;
	}

#ifdef _WIN32
	// Windows: VirtualAlloc with large page support
	ptr->long_state = static_cast<uint8_t*>(VirtualAlloc(nullptr, hashMemSize,
		MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE));

	if(ptr->long_state == nullptr)
	{
		msg->warning = "VirtualAlloc with large pages failed, attempting without";
		ptr->long_state = static_cast<uint8_t*>(VirtualAlloc(nullptr, hashMemSize,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	}

	if(ptr->long_state == nullptr)
	{
		_mm_free(ptr);
		msg->warning = "VirtualAlloc failed";
		return nullptr;
	}

	ptr->ctx_info[0] = 1;

	// VirtualLock (equivalent of mlock)
	ptr->ctx_info[1] = 0;
	if(use_mlock != 0)
	{
		if(VirtualLock(ptr->long_state, hashMemSize))
			ptr->ctx_info[1] = 1;
		else
			msg->warning = "VirtualLock failed";
	}
#else
	// Linux: mmap with huge pages (MAP_HUGETLB)
	ptr->long_state = static_cast<uint8_t*>(mmap(nullptr, hashMemSize, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_POPULATE, -1, 0));

	if(ptr->long_state == MAP_FAILED)
	{
		// Fallback: mmap without huge pages
		msg->warning = "mmap with HUGETLB failed, attempting without it (you should fix your kernel)";
		ptr->long_state = static_cast<uint8_t*>(mmap(nullptr, hashMemSize, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0));
	}

	if(ptr->long_state == MAP_FAILED)
	{
		_mm_free(ptr);
		msg->warning = "mmap failed, check attribute 'use_slow_memory' in 'config.txt'";
		return nullptr;
	}

	ptr->ctx_info[0] = 1;

	if(madvise(ptr->long_state, hashMemSize, MADV_RANDOM | MADV_WILLNEED) != 0)
		msg->warning = "madvise failed";

	ptr->ctx_info[1] = 0;
	if(use_mlock != 0 && mlock(ptr->long_state, hashMemSize) != 0)
		msg->warning = "mlock failed";
	else
		ptr->ctx_info[1] = 1;
#endif

	return ptr;
}

void cryptonight_free_ctx(cryptonight_ctx* ctx)
{
	const size_t hashMemSize = ::jconf::inst()->GetMiningMemSize();

	if(ctx->ctx_info[0] != 0)
	{
#ifdef _WIN32
		if(ctx->ctx_info[1] != 0)
			VirtualUnlock(ctx->long_state, hashMemSize);
		VirtualFree(ctx->long_state, 0, MEM_RELEASE);
#else
		if(ctx->ctx_info[1] != 0)
			munlock(ctx->long_state, hashMemSize);
		munmap(ctx->long_state, hashMemSize);
#endif
	}
	else
	{
		_mm_free(ctx->long_state);
	}

	_mm_free(ctx);
}
