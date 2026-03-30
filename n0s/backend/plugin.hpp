#pragma once

#include "n0s/misc/environment.hpp"
#include "n0s/params.hpp"

#include "iBackend.hpp"
#include <dlfcn.h>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <thread>
#include <vector>

namespace n0s
{

struct plugin
{

	plugin() = default;

	void load(const std::string backendName, const std::string libName)
	{
		m_backendName = backendName;

		// `.so` linux file extension for dynamic libraries
		const std::string fileExtension = ".so";

		// search library in working directory
		libBackend = dlopen(("./lib" + libName + fileExtension).c_str(), RTLD_NOW | RTLD_LAZY | RTLD_GLOBAL);
		// fallback to binary directory
		if(!libBackend)
			libBackend = dlopen((params::inst().executablePrefix + "lib" + libName + fileExtension).c_str(), RTLD_NOW | RTLD_LAZY | RTLD_GLOBAL);
		// try use LD_LIBRARY_PATH
		if(!libBackend)
			libBackend = dlopen(("lib" + libName + fileExtension).c_str(), RTLD_NOW | RTLD_LAZY | RTLD_GLOBAL);
		if(!libBackend)
		{
			std::cerr << "WARNING: " << m_backendName << " cannot load backend library: " << dlerror() << std::endl;
			return;
		}

		// reset last error
		dlerror();
		fn_startBackend = (startBackend_t)dlsym(libBackend, "n0s_start_backend");
		const char* dlsym_error = dlerror();
		if(dlsym_error)
		{
			std::cerr << "WARNING: backend plugin " << libName << " contains no entry 'n0s_start_backend': " << dlsym_error << std::endl;
		}
	}

	std::vector<iBackend*>* startBackend(uint32_t threadOffset, miner_work& pWork, environment& env)
	{
		if(fn_startBackend == nullptr)
		{
			std::vector<iBackend*>* pvThreads = new std::vector<iBackend*>();
			return pvThreads;
		}

		return fn_startBackend(threadOffset, pWork, env);
	}

	void unload()
	{
		if(libBackend)
		{
			dlclose(libBackend);
		}
		fn_startBackend = nullptr;
	}

	std::string m_backendName;

	typedef std::vector<iBackend*>* (*startBackend_t)(uint32_t threadOffset, miner_work& pWork, environment& env);

	startBackend_t fn_startBackend = nullptr;

	void* libBackend = nullptr;
};

} // namespace n0s
