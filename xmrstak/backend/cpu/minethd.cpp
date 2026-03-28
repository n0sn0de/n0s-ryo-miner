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
  * Additional permission under GNU GPL version 3 section 7
  *
  * If you modify this Program, or any covered work, by linking or combining
  * it with OpenSSL (or a modified version of that library), containing parts
  * covered by the terms of OpenSSL License and SSLeay License, the licensors
  * of this Program grant you additional permission to convey the resulting work.
  *
  */

#include "crypto/cryptonight_aesni.h"

#include "jconf.hpp"
#include "xmrstak/backend/cpu/cpuType.hpp"
#include "xmrstak/backend/globalStates.hpp"
#include "xmrstak/backend/iBackend.hpp"
#include "xmrstak/misc/configEditor.hpp"
#include "xmrstak/misc/console.hpp"
#include "xmrstak/params.hpp"

#include "minethd.hpp"
#include "xmrstak/jconf.hpp"
#include "xmrstak/misc/executor.hpp"

#include "hwlocMemory.hpp"
#include "xmrstak/backend/miner_work.hpp"

#ifndef CONF_NO_HWLOC
#include "autoAdjustHwloc.hpp"
#include "autoAdjust.hpp"
#else
#include "autoAdjust.hpp"
#endif

#include <assert.h>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>

#if defined(__APPLE__)
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#define SYSCTL_CORE_COUNT "machdep.cpu.core_count"
#elif defined(__FreeBSD__)
#include <pthread_np.h>
#endif //__APPLE__

#endif //_WIN32

namespace xmrstak
{
namespace cpu
{

bool minethd::thd_setaffinity(std::thread::native_handle_type h, uint64_t cpu_id)
{
#if defined(_WIN32)
	if(cpu_id < 64)
	{
		return SetThreadAffinityMask(h, 1ULL << cpu_id) != 0;
	}
	else
	{
		printer::inst()->print_msg(L0, "WARNING: Windows supports only affinity up to 63.");
		return false;
	}
#elif defined(__APPLE__)
	thread_port_t mach_thread;
	thread_affinity_policy_data_t policy = {static_cast<integer_t>(cpu_id)};
	mach_thread = pthread_mach_thread_np(h);
	return thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1) == KERN_SUCCESS;
#elif defined(__FreeBSD__)
	cpuset_t mn;
	CPU_ZERO(&mn);
	CPU_SET(cpu_id, &mn);
	return pthread_setaffinity_np(h, sizeof(cpuset_t), &mn) == 0;
#elif defined(__OpenBSD__)
	printer::inst()->print_msg(L0, "WARNING: thread pinning is not supported under OPENBSD.");
	return true;
#else
	cpu_set_t mn;
	CPU_ZERO(&mn);
	CPU_SET(cpu_id, &mn);
	return pthread_setaffinity_np(h, sizeof(cpu_set_t), &mn) == 0;
#endif
}

minethd::minethd(miner_work& pWork, size_t iNo, int iMultiway, bool no_prefetch, int64_t affinity, const std::string& asm_version)
{
	this->backendType = iBackend::CPU;
	oWork = pWork;
	bQuit = 0;
	iThreadNo = (uint8_t)iNo;
	iJobNo = 0;
	bNoPrefetch = no_prefetch;
	this->affinity = affinity;
	asm_version_str = asm_version;

	std::unique_lock<std::mutex> lck(thd_aff_set);
	std::future<void> order_guard = order_fix.get_future();

	// cryptonight_gpu only supports single-way hashing
	oWorkThd = std::thread(&minethd::work_main, this);

	order_guard.wait();

	if(affinity >= 0)
		if(!thd_setaffinity(oWorkThd.native_handle(), affinity))
			printer::inst()->print_msg(L1, "WARNING setting affinity failed.");
}

cryptonight_ctx* minethd::minethd_alloc_ctx()
{
	cryptonight_ctx* ctx;
	alloc_msg msg = {0};

	switch(::jconf::inst()->GetSlowMemSetting())
	{
	case ::jconf::never_use:
		ctx = cryptonight_alloc_ctx(1, 1, &msg);
		if(ctx == NULL)
			printer::inst()->print_msg(L0, "MEMORY ALLOC FAILED: %s", msg.warning);
		else
		{
			ctx->hash_fn = nullptr;
			ctx->loop_fn = nullptr;
			ctx->fun_data = nullptr;
			ctx->asm_version = 0;
			ctx->last_algo = invalid_algo;
		}
		return ctx;

	case ::jconf::no_mlck:
		ctx = cryptonight_alloc_ctx(1, 0, &msg);
		if(ctx == NULL)
			printer::inst()->print_msg(L0, "MEMORY ALLOC FAILED: %s", msg.warning);
		else
		{
			ctx->hash_fn = nullptr;
			ctx->loop_fn = nullptr;
			ctx->fun_data = nullptr;
			ctx->asm_version = 0;
			ctx->last_algo = invalid_algo;
		}
		return ctx;

	case ::jconf::print_warning:
		ctx = cryptonight_alloc_ctx(1, 1, &msg);
		if(msg.warning != NULL)
			printer::inst()->print_msg(L0, "MEMORY ALLOC FAILED: %s", msg.warning);
		if(ctx == NULL)
			ctx = cryptonight_alloc_ctx(0, 0, NULL);

		if(ctx != NULL)
		{
			ctx->hash_fn = nullptr;
			ctx->loop_fn = nullptr;
			ctx->fun_data = nullptr;
			ctx->asm_version = 0;
			ctx->last_algo = invalid_algo;
		}
		return ctx;

	case ::jconf::always_use:
		ctx = cryptonight_alloc_ctx(0, 0, NULL);

		ctx->hash_fn = nullptr;
		ctx->loop_fn = nullptr;
		ctx->fun_data = nullptr;
		ctx->asm_version = 0;
		ctx->last_algo = invalid_algo;

		return ctx;

	case ::jconf::unknown_value:
		return NULL;
	}

	return nullptr;
}

static constexpr size_t MAX_N = 5;
bool minethd::self_test()
{
	alloc_msg msg = {0};
	size_t res;
	bool fatal = false;

	switch(::jconf::inst()->GetSlowMemSetting())
	{
	case ::jconf::never_use:
		res = cryptonight_init(1, 1, &msg);
		fatal = true;
		break;

	case ::jconf::no_mlck:
		res = cryptonight_init(1, 0, &msg);
		fatal = true;
		break;

	case ::jconf::print_warning:
		res = cryptonight_init(1, 1, &msg);
		break;

	case ::jconf::always_use:
		res = cryptonight_init(0, 0, &msg);
		break;

	case ::jconf::unknown_value:
	default:
		return false;
	}

	if(msg.warning != nullptr)
		printer::inst()->print_msg(L0, "MEMORY INIT ERROR: %s", msg.warning);

	if(res == 0 && fatal)
		return false;

	cryptonight_ctx* ctx[MAX_N] = {0};
	for(int i = 0; i < MAX_N; i++)
	{
		if((ctx[i] = minethd_alloc_ctx()) == nullptr)
		{
			printer::inst()->print_msg(L0, "ERROR: miner was not able to allocate memory.");
			for(int j = 0; j < i; j++)
				cryptonight_free_ctx(ctx[j]);
			return false;
		}
	}

	bool bResult = true;

	unsigned char out[32 * MAX_N];

	auto neededAlgorithms = ::jconf::inst()->GetCurrentCoinSelection().GetAllAlgorithms();

	for(const auto algo : neededAlgorithms)
	{
		if(algo == POW(cryptonight_gpu))
		{
			func_selector(ctx, ::jconf::inst()->HaveHardwareAes(), false, algo);
			ctx[0]->hash_fn("", 0, out, ctx, algo);
			bResult = bResult && memcmp(out, "\x55\x5e\x0a\xee\x78\x79\x31\x6d\x7d\xef\xf7\x72\x97\x3c\xb9\x11\x8e\x38\x95\x70\x9d\xb2\x54\x7a\xc0\x72\xd5\xb9\x13\x10\x01\xd8", 32) == 0;

			func_selector(ctx, ::jconf::inst()->HaveHardwareAes(), true, algo);
			ctx[0]->hash_fn("", 0, out, ctx, algo);
			bResult = bResult && memcmp(out, "\x55\x5e\x0a\xee\x78\x79\x31\x6d\x7d\xef\xf7\x72\x97\x3c\xb9\x11\x8e\x38\x95\x70\x9d\xb2\x54\x7a\xc0\x72\xd5\xb9\x13\x10\x01\xd8", 32) == 0;
		}
		else
			printer::inst()->print_msg(L0,
				"Cryptonight hash self-test NOT defined for POW %s", algo.Name().c_str());

		if(!bResult)
			printer::inst()->print_msg(L0,
				"Cryptonight hash self-test failed. This might be caused by bad compiler optimizations.");
	}

	for(int i = 0; i < MAX_N; i++)
		cryptonight_free_ctx(ctx[i]);

	return bResult;
}

std::vector<iBackend*> minethd::thread_starter(uint32_t threadOffset, miner_work& pWork)
{
	std::vector<iBackend*> pvThreads;

	if(!configEditor::file_exist(params::inst().configFileCPU))
	{
#ifndef CONF_NO_HWLOC
		autoAdjustHwloc adjustHwloc;
		if(!adjustHwloc.printConfig())
		{
			autoAdjust adjust;
			if(!adjust.printConfig())
			{
				return pvThreads;
			}
		}
#else
		autoAdjust adjust;
		if(!adjust.printConfig())
		{
			return pvThreads;
		}
#endif
	}

	if(!jconf::inst()->parse_config())
	{
		win_exit();
	}

	size_t i, n = jconf::inst()->GetThreadCount();
	pvThreads.reserve(n);

	jconf::thd_cfg cfg;
	for(i = 0; i < n; i++)
	{
		jconf::inst()->GetThreadConfig(i, cfg);

		if(cfg.iCpuAff >= 0)
		{
#if defined(__APPLE__)
			printer::inst()->print_msg(L1, "WARNING on macOS thread affinity is only advisory.");
#endif

			printer::inst()->print_msg(L1, "Starting %dx thread, affinity: %d.", cfg.iMultiway, (int)cfg.iCpuAff);
		}
		else
			printer::inst()->print_msg(L1, "Starting %dx thread, no affinity.", cfg.iMultiway);

		minethd* thd = new minethd(pWork, i + threadOffset, cfg.iMultiway, cfg.bNoPrefetch, cfg.iCpuAff, cfg.asm_version_str);
		pvThreads.push_back(thd);
	}

	return pvThreads;
}

void minethd::func_selector(cryptonight_ctx** ctx, bool bHaveAes, bool bNoPrefetch, const xmrstak_algo& algo)
{
	// cryptonight_gpu is the only supported algorithm — always use Cryptonight_hash_gpu
	if(bNoPrefetch)
	{
		if(!bHaveAes)
			ctx[0]->hash_fn = Cryptonight_hash_gpu::template hash<cryptonight_gpu, true, false>;
		else
			ctx[0]->hash_fn = Cryptonight_hash_gpu::template hash<cryptonight_gpu, false, false>;
	}
	else
	{
		if(!bHaveAes)
			ctx[0]->hash_fn = Cryptonight_hash_gpu::template hash<cryptonight_gpu, true, true>;
		else
			ctx[0]->hash_fn = Cryptonight_hash_gpu::template hash<cryptonight_gpu, false, true>;
	}
}

void minethd::work_main()
{
	if(affinity >= 0)
		bindMemoryToNUMANode(affinity);

	order_fix.set_value();
	std::unique_lock<std::mutex> lck(thd_aff_set);
	lck.unlock();
	std::this_thread::yield();

	constexpr uint32_t N = 1;
	cryptonight_ctx* ctx[MAX_N];
	uint64_t iCount = 0;
	uint64_t iLastCount = 0;
	uint64_t* piHashVal[MAX_N];
	uint32_t* piNonce[MAX_N];
	uint8_t bHashOut[MAX_N * 32];
	uint8_t bWorkBlob[sizeof(miner_work::bWorkBlob) * MAX_N];
	uint32_t iNonce;
	job_result res;

	ctx[0] = minethd_alloc_ctx();
	if(ctx[0] == nullptr)
	{
		printer::inst()->print_msg(L0, "ERROR: miner was not able to allocate memory.");
		win_exit(1);
	}
	piHashVal[0] = (uint64_t*)(bHashOut + 24);
	piNonce[0] = (uint32_t*)(bWorkBlob + 39);

	if(!oWork.bStall)
	{
		memcpy(bWorkBlob, oWork.bWorkBlob, oWork.iWorkSize);
	}

	globalStates::inst().iConsumeCnt++;

	auto miner_algo = ::jconf::inst()->GetCurrentCoinSelection().GetDescription(1).GetMiningAlgoRoot();
	uint8_t version = 0;
	size_t lastPoolId = 0;

	func_selector(ctx, ::jconf::inst()->HaveHardwareAes(), bNoPrefetch, miner_algo);
	while(bQuit == 0)
	{
		if(oWork.bStall)
		{
			while(globalStates::inst().iGlobalJobNo.load(std::memory_order_relaxed) == iJobNo)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));

			globalStates::inst().consume_work(oWork, iJobNo);
			memcpy(bWorkBlob, oWork.bWorkBlob, oWork.iWorkSize);
			continue;
		}

		constexpr uint32_t nonce_chunk = 4096;
		int64_t nonce_ctr = 0;

		assert(sizeof(job_result::sJobID) == sizeof(pool_job::sJobID));

		if(oWork.bNiceHash)
			iNonce = *piNonce[0];

		uint8_t new_version = oWork.getVersion();
		if(new_version != version || oWork.iPoolId != lastPoolId)
		{
			coinDescription coinDesc = ::jconf::inst()->GetCurrentCoinSelection().GetDescription(oWork.iPoolId);
			if(new_version >= coinDesc.GetMiningForkVersion())
				miner_algo = coinDesc.GetMiningAlgo();
			else
				miner_algo = coinDesc.GetMiningAlgoRoot();
			func_selector(ctx, ::jconf::inst()->HaveHardwareAes(), bNoPrefetch, miner_algo);
			lastPoolId = oWork.iPoolId;
			version = new_version;
		}

		while(globalStates::inst().iGlobalJobNo.load(std::memory_order_relaxed) == iJobNo)
		{
			if((iCount++ & 0x7) == 0)
			{
				updateStats((iCount - iLastCount) * N, oWork.iPoolId);
				iLastCount = iCount;
			}

			nonce_ctr -= N;
			if(nonce_ctr <= 0)
			{
				globalStates::inst().calc_start_nonce(iNonce, oWork.bNiceHash, nonce_chunk);
				nonce_ctr = nonce_chunk;
				if(globalStates::inst().iGlobalJobNo.load(std::memory_order_relaxed) != iJobNo)
					break;
			}

			*piNonce[0] = iNonce++;

			ctx[0]->hash_fn(bWorkBlob, oWork.iWorkSize, bHashOut, ctx, miner_algo);

			if(*piHashVal[0] < oWork.iTarget)
			{
				executor::inst()->push_event(
					ex_event(job_result(oWork.sJobID, iNonce - N, bHashOut, iThreadNo, miner_algo),
						oWork.iPoolId));
			}

			std::this_thread::yield();
		}

		globalStates::inst().consume_work(oWork, iJobNo);
		memcpy(bWorkBlob, oWork.bWorkBlob, oWork.iWorkSize);
	}

	cryptonight_free_ctx(ctx[0]);
}

} // namespace cpu
} // namespace xmrstak
