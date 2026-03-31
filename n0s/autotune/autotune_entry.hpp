#pragma once

namespace n0s
{
namespace autotune
{

/// Entry point for --autotune mode.
/// Called from cli-miner.cpp after config parsing.
///
/// Discovers GPUs, generates candidates, evaluates via subprocess benchmarks,
/// selects winners, writes results to autotune.json and backend config files.
///
/// @return 0 on success, non-zero on error
int do_autotune();

} // namespace autotune
} // namespace n0s
