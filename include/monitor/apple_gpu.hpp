#pragma once

#include <cstdint>
#include <vector>

namespace monitor {

struct AppleGpuProbeResult {
  bool available = false;
  double utilization_percent = 0.0;
  std::uint64_t used_memory_bytes = 0;
  std::uint64_t total_memory_bytes = 0;
  std::vector<int> active_pids;
};

AppleGpuProbeResult sample_apple_gpu();

}  // namespace monitor
