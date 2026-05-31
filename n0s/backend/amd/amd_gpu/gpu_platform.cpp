/*
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

#include "gpu_platform.hpp"
#include "n0s/misc/console.hpp"
#include "n0s/params.hpp"

#include <cstdlib>
#include <string>
#include <vector>

namespace n0s
{
namespace amd
{

uint32_t getNumPlatforms()
{
	cl_uint num_platforms = 0;
	cl_int clStatus;

	// Get platform and device information
	clStatus = clGetPlatformIDs(0, nullptr, &num_platforms);
	if(clStatus != CL_SUCCESS)
	{
		printer::inst()->print_msg(L1, "WARNING: %s when calling clGetPlatformIDs for number of platforms.", err_to_str(clStatus));
		return 0u;
	}

	return num_platforms;
}

std::vector<GpuContext> getAMDDevices(int index)
{
	std::vector<GpuContext> ctxVec;
	std::vector<cl_platform_id> platforms;
	std::vector<cl_device_id> device_list;

	cl_int clStatus;
	cl_uint num_devices;
	uint32_t numPlatforms = getNumPlatforms();

	if(numPlatforms == 0)
		return ctxVec;

	platforms.resize(numPlatforms);
	if((clStatus = clGetPlatformIDs(numPlatforms, platforms.data(), nullptr)) != CL_SUCCESS)
	{
		printer::inst()->print_msg(L1, "WARNING: %s when calling clGetPlatformIDs for platform information.", err_to_str(clStatus));
		return ctxVec;
	}

	if((clStatus = clGetDeviceIDs(platforms[index], CL_DEVICE_TYPE_GPU, 0, nullptr, &num_devices)) != CL_SUCCESS)
	{
		printer::inst()->print_msg(L1, "WARNING: %s when calling clGetDeviceIDs for of devices.", err_to_str(clStatus));
		return ctxVec;
	}

	device_list.resize(num_devices);
	if((clStatus = clGetDeviceIDs(platforms[index], CL_DEVICE_TYPE_GPU, num_devices, device_list.data(), nullptr)) != CL_SUCCESS)
	{
		printer::inst()->print_msg(L1, "WARNING: %s when calling clGetDeviceIDs for device information.", err_to_str(clStatus));
		return ctxVec;
	}

	for(size_t k = 0; k < num_devices; k++)
	{
		std::vector<char> devVendorVec(1024);
		if((clStatus = clGetDeviceInfo(device_list[k], CL_DEVICE_VENDOR, devVendorVec.size(), devVendorVec.data(), nullptr)) != CL_SUCCESS)
		{
			printer::inst()->print_msg(L1, "WARNING: %s when calling clGetDeviceInfo to get the device vendor name for device %u.", err_to_str(clStatus), k);
			continue;
		}

		std::string devVendor(devVendorVec.data());

		bool isAMDDevice = devVendor.find("Advanced Micro Devices") != std::string::npos || devVendor.find("AMD") != std::string::npos;
		bool isNVIDIADevice = devVendor.find("NVIDIA Corporation") != std::string::npos || devVendor.find("NVIDIA") != std::string::npos;

		std::string selectedOpenCLVendor = n0s::params::inst().openCLVendor;
		if((isAMDDevice && selectedOpenCLVendor == "AMD") || (isNVIDIADevice && selectedOpenCLVendor == "NVIDIA"))
		{
			GpuContext ctx;
			std::vector<char> devNameVec(1024);

			ctx.isNVIDIA = isNVIDIADevice;
			ctx.isAMD = isAMDDevice;

			if((clStatus = clGetDeviceInfo(device_list[k], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(int), &(ctx.computeUnits), nullptr)) != CL_SUCCESS)
			{
				printer::inst()->print_msg(L1, "WARNING: %s when calling clGetDeviceInfo to get CL_DEVICE_MAX_COMPUTE_UNITS for device %u.", err_to_str(clStatus), k);
				continue;
			}

			if((clStatus = clGetDeviceInfo(device_list[k], CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(size_t), &(ctx.maxMemPerAlloc), nullptr)) != CL_SUCCESS)
			{
				printer::inst()->print_msg(L1, "WARNING: %s when calling clGetDeviceInfo to get CL_DEVICE_MAX_MEM_ALLOC_SIZE for device %u.", err_to_str(clStatus), k);
				continue;
			}

			if((clStatus = clGetDeviceInfo(device_list[k], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(size_t), &(ctx.freeMem), nullptr)) != CL_SUCCESS)
			{
				printer::inst()->print_msg(L1, "WARNING: %s when calling clGetDeviceInfo to get CL_DEVICE_GLOBAL_MEM_SIZE for device %u.", err_to_str(clStatus), k);
				continue;
			}

			// the allocation for NVIDIA OpenCL is not limited to 1/4 of the GPU memory per allocation
			if(isNVIDIADevice)
				ctx.maxMemPerAlloc = ctx.freeMem;

			if((clStatus = clGetDeviceInfo(device_list[k], CL_DEVICE_NAME, devNameVec.size(), devNameVec.data(), nullptr)) != CL_SUCCESS)
			{
				printer::inst()->print_msg(L1, "WARNING: %s when calling clGetDeviceInfo to get CL_DEVICE_NAME for device %u.", err_to_str(clStatus), k);
				continue;
			}

			std::vector<char> openCLDriverVer(1024);
			if((clStatus = clGetDeviceInfo(device_list[k], CL_DRIVER_VERSION, openCLDriverVer.size(), openCLDriverVer.data(), nullptr)) != CL_SUCCESS)
			{
				printer::inst()->print_msg(L1, "WARNING: %s when calling clGetDeviceInfo to get CL_DRIVER_VERSION for device %u.", err_to_str(clStatus), k);
				continue;
			}

			// if environment variable GPU_SINGLE_ALLOC_PERCENT is not set we can not allocate the full memory
			ctx.deviceIdx = k;
			ctx.name = std::string(devNameVec.data());
			ctx.DeviceID = device_list[k];
			ctx.interleave = 40;
			printer::inst()->print_msg(L0, "Found OpenCL GPU %s.", ctx.name.c_str());
			ctxVec.push_back(ctx);
		}
	}

	return ctxVec;
}

int getAMDPlatformIdx()
{
	uint32_t numPlatforms = getNumPlatforms();

	if(numPlatforms == 0)
	{
		printer::inst()->print_msg(L0, "WARNING: No OpenCL platform found.");
		return -1;
	}
	cl_platform_id* platforms = nullptr;
	cl_int clStatus;

	platforms = (cl_platform_id*)malloc(sizeof(cl_platform_id) * numPlatforms);
	clStatus = clGetPlatformIDs(numPlatforms, platforms, nullptr);

	int platformIndex = -1;
	// Mesa OpenCL is the fallback if no AMD or Apple OpenCL is found
	int mesaPlatform = -1;

	if(clStatus == CL_SUCCESS)
	{
		for(uint32_t i = 0; i < numPlatforms; i++)
		{
			size_t infoSize;
			clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, 0, nullptr, &infoSize);
			std::vector<char> platformNameVec(infoSize);

			clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, infoSize, platformNameVec.data(), nullptr);
			std::string platformName(platformNameVec.data());

			bool isAMDOpenCL = platformName.find("Advanced Micro Devices") != std::string::npos ||
							   platformName.find("Apple") != std::string::npos ||
							   platformName.find("Mesa") != std::string::npos;
			bool isNVIDIADevice = platformName.find("NVIDIA Corporation") != std::string::npos || platformName.find("NVIDIA") != std::string::npos;
			std::string selectedOpenCLVendor = n0s::params::inst().openCLVendor;
			if((isAMDOpenCL && selectedOpenCLVendor == "AMD") || (isNVIDIADevice && selectedOpenCLVendor == "NVIDIA"))
			{
				printer::inst()->print_msg(L0, "Found %s platform index id = %i, name = %s", selectedOpenCLVendor.c_str(), i, platformName.c_str());
				if(platformName.find("Mesa") != std::string::npos)
					mesaPlatform = i;
				else
				{
					// exit if AMD or Apple platform is found
					platformIndex = i;
					break;
				}
			}
		}
		// fall back to Mesa OpenCL
		if(platformIndex == -1 && mesaPlatform != -1)
		{
			printer::inst()->print_msg(L0, "No AMD platform found select Mesa as OpenCL platform");
			platformIndex = mesaPlatform;
		}
	}
	else
		printer::inst()->print_msg(L1, "WARNING: %s when calling clGetPlatformIDs for platform information.", err_to_str(clStatus));

	free(platforms);
	return platformIndex;
}

} // namespace amd
} // namespace n0s
