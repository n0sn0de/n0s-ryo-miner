#pragma once

#include "crypto/cryptonight.h"
#include "xmrstak/backend/iBackend.hpp"
#include "xmrstak/backend/miner_work.hpp"
#include "xmrstak/jconf.hpp"

#include <atomic>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

namespace xmrstak
{
namespace cpu
{

class minethd : public iBackend
{
  public:
	static std::vector<iBackend*> thread_starter(uint32_t threadOffset, miner_work& pWork);
	static bool self_test();

	typedef void (*cn_on_new_job)(const miner_work&, cryptonight_ctx**);

	static void func_selector(cryptonight_ctx**, bool bHaveAes, bool bNoPrefetch, const xmrstak_algo& algo);
	static bool thd_setaffinity(std::thread::native_handle_type h, uint64_t cpu_id);

	static cryptonight_ctx* minethd_alloc_ctx();

  private:
	minethd(miner_work& pWork, size_t iNo, int iMultiway, bool no_prefetch, int64_t affinity, const std::string& asm_version);

	void work_main();

	uint64_t iJobNo;

	miner_work oWork;

	std::promise<void> order_fix;
	std::mutex thd_aff_set;

	std::thread oWorkThd;
	int64_t affinity;

	bool bQuit;
	bool bNoPrefetch;
	std::string asm_version_str = "off";
};

} // namespace cpu
} // namespace xmrstak
