#include "monitor/model/memory_risk_model.hpp"

#include <algorithm>

namespace monitor {
namespace {

std::uint64_t percent_of(std::uint64_t value, int percent) {
  return value / 100ULL * static_cast<std::uint64_t>(percent);
}

std::uint64_t workload_rss_total(const std::vector<WorkloadSnapshot>& workloads) {
  std::uint64_t total = 0;
  for (const auto& workload : workloads) {
    total += workload.resident_bytes;
  }
  return total;
}

bool compressed_rising(const SystemSnapshot& snapshot, const std::vector<SystemSnapshot>& recent_history) {
  if (recent_history.empty()) {
    return snapshot.memory_compressed_bytes > 0;
  }
  return snapshot.memory_compressed_bytes > recent_history.front().memory_compressed_bytes;
}

bool swap_used_increasing(const SystemSnapshot& snapshot, const std::vector<SystemSnapshot>& recent_history) {
  if (recent_history.empty()) {
    return snapshot.swap_used_bytes > 0;
  }
  return snapshot.swap_used_bytes > recent_history.front().swap_used_bytes;
}

}  // namespace

const char* memory_risk_name(MemoryRiskLevel level) {
  switch (level) {
    case MemoryRiskLevel::Unknown: return "unknown";
    case MemoryRiskLevel::Ok: return "ok";
    case MemoryRiskLevel::Watch: return "watch";
    case MemoryRiskLevel::Warn: return "warn";
    case MemoryRiskLevel::Critical: return "critical";
  }
  return "unknown";
}

const char* memory_risk_label(MemoryRiskLevel level) {
  switch (level) {
    case MemoryRiskLevel::Unknown: return "UNKNOWN";
    case MemoryRiskLevel::Ok: return "OK";
    case MemoryRiskLevel::Watch: return "WATCH";
    case MemoryRiskLevel::Warn: return "WARN";
    case MemoryRiskLevel::Critical: return "CRITICAL";
  }
  return "UNKNOWN";
}

MemoryRiskSnapshot derive_memory_risk(const SystemSnapshot& snapshot,
                                      const std::vector<SystemSnapshot>& recent_history,
                                      const std::vector<WorkloadSnapshot>& workloads) {
  MemoryRiskSnapshot risk;
  risk.reclaimable_bytes = snapshot.memory_inactive_bytes +
                           snapshot.memory_speculative_bytes +
                           snapshot.memory_purgeable_bytes;
  risk.compressed_bytes = snapshot.memory_compressed_bytes;
  risk.swap_used_bytes = snapshot.swap_used_bytes;
  risk.swap_rate_bytes_per_sec = snapshot.swapins_bytes_per_sec + snapshot.swapouts_bytes_per_sec;

  if (snapshot.memory_total_bytes == 0 ||
      (snapshot.memory_wired_bytes == 0 && snapshot.memory_active_bytes == 0 &&
       snapshot.memory_used_bytes == 0)) {
    risk.level = MemoryRiskLevel::Unknown;
    risk.reason = "memory totals unavailable";
    return risk;
  }

  const std::uint64_t reclaim_credit = std::min(risk.reclaimable_bytes, percent_of(snapshot.memory_total_bytes, 20));
  const std::uint64_t strict_used = snapshot.memory_wired_bytes +
                                    snapshot.memory_active_bytes +
                                    snapshot.memory_compressed_bytes;
  risk.estimated_headroom_bytes = snapshot.memory_total_bytes > strict_used
                                      ? snapshot.memory_total_bytes - strict_used + reclaim_credit
                                      : reclaim_credit;
  const std::uint64_t headroom_5 = percent_of(snapshot.memory_total_bytes, 5);
  const std::uint64_t headroom_10 = percent_of(snapshot.memory_total_bytes, 10);
  const std::uint64_t headroom_20 = percent_of(snapshot.memory_total_bytes, 20);
  const bool high_swapout = snapshot.swapouts_bytes_per_sec >= (16ULL << 20);
  const bool any_swap_rate = risk.swap_rate_bytes_per_sec > 0;

  if (snapshot.memory_pressure == MemoryPressureLevel::Critical) {
    risk.level = MemoryRiskLevel::Critical;
    risk.reason = "memory pressure critical";
  } else if (high_swapout) {
    risk.level = MemoryRiskLevel::Critical;
    risk.reason = "swapout rate rising";
  } else if (risk.estimated_headroom_bytes < headroom_5 && compressed_rising(snapshot, recent_history)) {
    risk.level = MemoryRiskLevel::Critical;
    risk.reason = "estimated headroom below 5% with compression rising";
  } else if (snapshot.memory_pressure == MemoryPressureLevel::Warn) {
    risk.level = MemoryRiskLevel::Warn;
    risk.reason = "memory pressure warn";
  } else if (swap_used_increasing(snapshot, recent_history) && any_swap_rate) {
    risk.level = MemoryRiskLevel::Warn;
    risk.reason = "swap used increasing with active paging";
  } else if (risk.estimated_headroom_bytes < headroom_10) {
    risk.level = MemoryRiskLevel::Warn;
    risk.reason = "estimated headroom below 10%";
  } else if (risk.compressed_bytes > headroom_10) {
    risk.level = MemoryRiskLevel::Watch;
    risk.reason = "compressed memory above 10%";
  } else if (workload_rss_total(workloads) > snapshot.memory_total_bytes / 2ULL) {
    risk.level = MemoryRiskLevel::Watch;
    risk.reason = "workload RSS above 50% of memory";
  } else if (risk.estimated_headroom_bytes < headroom_20) {
    risk.level = MemoryRiskLevel::Watch;
    risk.reason = "estimated headroom below 20%";
  } else {
    risk.level = MemoryRiskLevel::Ok;
    risk.reason = "memory headroom healthy";
  }
  return risk;
}

}  // namespace monitor
