#include <cassert>

#include "monitor/model/memory_risk_model.hpp"

namespace {

monitor::SystemSnapshot base_snapshot() {
  monitor::SystemSnapshot snapshot;
  snapshot.memory_total_bytes = 1000;
  snapshot.memory_wired_bytes = 100;
  snapshot.memory_active_bytes = 250;
  snapshot.memory_inactive_bytes = 100;
  snapshot.memory_speculative_bytes = 50;
  snapshot.memory_purgeable_bytes = 50;
  snapshot.memory_compressed_bytes = 50;
  snapshot.memory_used_bytes = 400;
  snapshot.memory_pressure = monitor::MemoryPressureLevel::Normal;
  return snapshot;
}

}  // namespace

int main() {
  std::vector<monitor::SystemSnapshot> history;

  monitor::SystemSnapshot unknown;
  auto risk = monitor::derive_memory_risk(unknown, history);
  assert(risk.level == monitor::MemoryRiskLevel::Unknown);

  monitor::SystemSnapshot ok = base_snapshot();
  risk = monitor::derive_memory_risk(ok, history);
  assert(risk.level == monitor::MemoryRiskLevel::Ok);

  monitor::SystemSnapshot watch = base_snapshot();
  watch.memory_compressed_bytes = 120;
  risk = monitor::derive_memory_risk(watch, history);
  assert(risk.level == monitor::MemoryRiskLevel::Watch);

  monitor::SystemSnapshot warn = base_snapshot();
  warn.memory_pressure = monitor::MemoryPressureLevel::Warn;
  risk = monitor::derive_memory_risk(warn, history);
  assert(risk.level == monitor::MemoryRiskLevel::Warn);

  monitor::SystemSnapshot critical = base_snapshot();
  critical.memory_pressure = monitor::MemoryPressureLevel::Critical;
  risk = monitor::derive_memory_risk(critical, history);
  assert(risk.level == monitor::MemoryRiskLevel::Critical);

  monitor::SystemSnapshot workload_case = base_snapshot();
  monitor::WorkloadSnapshot workload;
  workload.resident_bytes = 600;
  risk = monitor::derive_memory_risk(workload_case, history, {workload});
  assert(risk.level == monitor::MemoryRiskLevel::Watch);
  return 0;
}
