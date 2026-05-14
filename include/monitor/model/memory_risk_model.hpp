#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "monitor/model/workload_model.hpp"
#include "monitor/snapshot.hpp"

namespace monitor {

enum class MemoryRiskLevel {
  Unknown,
  Ok,
  Watch,
  Warn,
  Critical,
};

struct MemoryRiskSnapshot {
  MemoryRiskLevel level = MemoryRiskLevel::Unknown;
  std::string reason;
  std::uint64_t estimated_headroom_bytes = 0;
  std::uint64_t reclaimable_bytes = 0;
  std::uint64_t compressed_bytes = 0;
  std::uint64_t swap_used_bytes = 0;
  std::uint64_t swap_rate_bytes_per_sec = 0;
};

const char* memory_risk_name(MemoryRiskLevel level);
const char* memory_risk_label(MemoryRiskLevel level);
MemoryRiskSnapshot derive_memory_risk(const SystemSnapshot& snapshot,
                                      const std::vector<SystemSnapshot>& recent_history,
                                      const std::vector<WorkloadSnapshot>& workloads = {});

}  // namespace monitor
