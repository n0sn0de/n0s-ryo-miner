#pragma once
#include <inttypes.h>
#include <stddef.h>
#include <string>

enum xmrstak_algo_id
{
	invalid_algo = 0,
	cryptonight_gpu = 1
};

inline std::string get_algo_name(xmrstak_algo_id algo_id)
{
	switch(algo_id)
	{
	case cryptonight_gpu:
		return "cryptonight_gpu";
	default:
		return "invalid_algo";
	}
}

struct xmrstak_algo
{
	xmrstak_algo(xmrstak_algo_id name_id) :
		algo_name(name_id),
		base_algo(name_id)
	{
	}
	xmrstak_algo(xmrstak_algo_id name_id, xmrstak_algo_id algorithm) :
		algo_name(name_id),
		base_algo(algorithm)
	{
	}
	xmrstak_algo(xmrstak_algo_id name_id, xmrstak_algo_id algorithm, uint32_t iteration) :
		algo_name(name_id),
		base_algo(algorithm),
		iter(iteration)
	{
	}
	xmrstak_algo(xmrstak_algo_id name_id, xmrstak_algo_id algorithm, uint32_t iteration, size_t memory) :
		algo_name(name_id),
		base_algo(algorithm),
		iter(iteration),
		mem(memory)
	{
	}
	xmrstak_algo(xmrstak_algo_id name_id, xmrstak_algo_id algorithm, uint32_t iteration, size_t memory, uint32_t mem_mask) :
		algo_name(name_id),
		base_algo(algorithm),
		iter(iteration),
		mem(memory),
		mask(mem_mask)
	{
	}

	bool operator==(const xmrstak_algo& other) const
	{
		return other.Id() == Id() && other.Mem() == Mem() && other.Iter() == Iter() && other.Mask() == Mask();
	}

	bool operator==(const xmrstak_algo_id& id) const
	{
		return base_algo == id;
	}

	operator xmrstak_algo_id() const
	{
		return base_algo;
	}

	xmrstak_algo_id Id() const
	{
		return base_algo;
	}

	size_t Mem() const
	{
		if(base_algo == invalid_algo)
			return 0;
		else
			return mem;
	}

	uint32_t Iter() const
	{
		return iter;
	}

	std::string Name() const
	{
		return get_algo_name(algo_name);
	}

	std::string BaseName() const
	{
		return get_algo_name(base_algo);
	}

	uint32_t Mask() const
	{
		if(mask == 0)
			return ((mem - 1u) / 16) * 16;
		else
			return mask;
	}

	xmrstak_algo_id algo_name = invalid_algo;
	xmrstak_algo_id base_algo = invalid_algo;
	uint32_t iter = 0u;
	size_t mem = 0u;
	uint32_t mask = 0u;
};

// cryptonight-gpu constants
constexpr size_t CN_MEMORY = 2 * 1024 * 1024;
constexpr uint32_t CN_GPU_MASK = 0x1FFFC0;
constexpr uint32_t CN_GPU_ITER = 0xC000;

inline xmrstak_algo POW(xmrstak_algo_id algo_id)
{
	if(algo_id == cryptonight_gpu)
		return {cryptonight_gpu, cryptonight_gpu, CN_GPU_ITER, CN_MEMORY, CN_GPU_MASK};
	return {invalid_algo, invalid_algo};
}
