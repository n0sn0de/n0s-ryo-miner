#pragma once

#include "autotune_types.hpp"

#include <string>

namespace n0s
{
namespace autotune
{

/// Subprocess-based candidate evaluator.
///
/// Evaluates a single autotune candidate by spawning the miner in benchmark
/// mode with a temporary config file, then parsing the JSON results.
///
/// This approach provides:
///   - Full process isolation (GPU context cleanup, crash safety)
///   - Reuse of proven benchmark infrastructure (--benchmark --benchmark-json)
///   - No in-process backend lifecycle complexity
class SubprocessRunner
{
public:
	/// @param miner_path  Path to the n0s-ryo-miner binary
	SubprocessRunner(const std::string& miner_path);

	/// Evaluate an AMD/OpenCL candidate.
	/// Generates a temp amd.txt, runs benchmark subprocess, parses results.
	///
	/// @param device_index  GPU index
	/// @param candidate     Candidate with amd params set
	/// @param benchmark_sec Benchmark duration in seconds
	/// @param metrics       [out] Filled with benchmark results
	/// @return true if benchmark ran successfully
	bool evaluateAmd(
		uint32_t device_index,
		const CandidateRecord& candidate,
		int benchmark_sec,
		BenchmarkMetrics& metrics);

	/// Evaluate an NVIDIA/CUDA candidate.
	/// Generates a temp nvidia.txt, runs benchmark subprocess, parses results.
	bool evaluateNvidia(
		uint32_t device_index,
		const CandidateRecord& candidate,
		int benchmark_sec,
		BenchmarkMetrics& metrics);

	/// Collect device fingerprint for an AMD/OpenCL device.
	/// Queries OpenCL device properties.
	/// @param device_index  GPU index
	/// @param fingerprint   [out] Device fingerprint
	/// @return true on success
	bool collectAmdFingerprint(uint32_t device_index, DeviceFingerprint& fingerprint);

	/// Collect device fingerprint for an NVIDIA/CUDA device.
	/// Runs nvidia-smi to get device properties.
	bool collectNvidiaFingerprint(uint32_t device_index, DeviceFingerprint& fingerprint);

private:
	/// Generate a temporary AMD config file for a candidate
	std::string generateAmdConfig(uint32_t device_index, const AmdCandidate& candidate);

	/// Generate a temporary NVIDIA config file for a candidate
	std::string generateNvidiaConfig(uint32_t device_index, const NvidiaCandidate& candidate);

	/// Run the miner benchmark subprocess and parse JSON results
	bool runBenchmark(
		const std::string& config_flag,
		const std::string& config_path,
		const std::string& disable_other_flag,
		int benchmark_sec,
		BenchmarkMetrics& metrics);

	/// Parse benchmark JSON output into metrics
	bool parseBenchmarkJson(const std::string& json_path, BenchmarkMetrics& metrics);

	std::string miner_path_;
};

} // namespace autotune
} // namespace n0s
