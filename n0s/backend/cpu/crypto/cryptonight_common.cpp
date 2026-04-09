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

namespace
{

void set_alloc_warning(alloc_msg* msg, const std::string& warning)
{
	if(msg == nullptr || warning.empty())
		return;

	if(msg->warning.empty())
		msg->warning = warning;
	else if(msg->warning.find(warning) == std::string::npos)
		msg->warning += "; " + warning;
}

} // namespace

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

cryptonight_ctx* cryptonight_alloc_ctx(size_t use_fast_mem, size_t use_mlock, bool allow_slow_fallback, alloc_msg* msg)
{
	const size_t hashMemSize = ::jconf::inst()->GetMiningMemSize();
	if(msg != nullptr)
		msg->warning.clear();

	cryptonight_ctx* ptr = static_cast<cryptonight_ctx*>(_mm_malloc(sizeof(cryptonight_ctx), 4096));
	if(ptr == nullptr)
	{
		set_alloc_warning(msg, "context allocation failed");
		return nullptr;
	}

	if(use_fast_mem == 0)
	{
		// Fallback: standard aligned allocation (no huge pages)
		ptr->long_state = static_cast<uint8_t*>(_mm_malloc(hashMemSize, hashMemSize));
		ptr->ctx_info[0] = 0;
		ptr->ctx_info[1] = 0;
		if(ptr->long_state == nullptr)
		{
			set_alloc_warning(msg, "_mm_malloc was not able to allocate " + std::to_string(hashMemSize) + " byte");
			_mm_free(ptr);
			return nullptr;
		}
		return ptr;
	}

#ifdef _WIN32
	// Windows: VirtualAlloc with large page support
	ptr->long_state = static_cast<uint8_t*>(VirtualAlloc(nullptr, hashMemSize,
		MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE));

	if(ptr->long_state == nullptr)
	{
		if(!allow_slow_fallback)
		{
			set_alloc_warning(msg, "large pages requested but unavailable on this Windows session");
			_mm_free(ptr);
			return nullptr;
		}

		set_alloc_warning(msg, "large pages unavailable on this Windows session, continuing with standard memory");
		ptr->long_state = static_cast<uint8_t*>(VirtualAlloc(nullptr, hashMemSize,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	}

	if(ptr->long_state == nullptr)
	{
		_mm_free(ptr);
		set_alloc_warning(msg, "VirtualAlloc failed");
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
			set_alloc_warning(msg, "memory lock unavailable on this Windows session, continuing without locked pages");
	}
#else
	// Linux: mmap with huge pages (MAP_HUGETLB)
	ptr->long_state = static_cast<uint8_t*>(mmap(nullptr, hashMemSize, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_POPULATE, -1, 0));

	if(ptr->long_state == MAP_FAILED)
	{
		if(!allow_slow_fallback)
		{
			set_alloc_warning(msg, "huge pages requested but unavailable on this host");
			_mm_free(ptr);
			return nullptr;
		}

		// Fallback: mmap without huge pages
		set_alloc_warning(msg, "huge pages unavailable, continuing with standard pages");
		ptr->long_state = static_cast<uint8_t*>(mmap(nullptr, hashMemSize, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0));
	}

	if(ptr->long_state == MAP_FAILED)
	{
		_mm_free(ptr);
		set_alloc_warning(msg, "mmap failed, check available RAM or 'use_slow_memory' in 'config.txt'");
		return nullptr;
	}

	ptr->ctx_info[0] = 1;

	(void)madvise(ptr->long_state, hashMemSize, MADV_RANDOM | MADV_WILLNEED);

	ptr->ctx_info[1] = 0;
	if(use_mlock != 0 && mlock(ptr->long_state, hashMemSize) != 0)
		set_alloc_warning(msg, "memory lock unavailable, continuing without locked pages");
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
