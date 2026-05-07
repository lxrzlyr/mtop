#include <cassert>
#include <cstdint>

#include "monitor/metrics.hpp"

int main() {
  monitor::SystemSnapshot unknown;
  assert(monitor::derive_memory_pressure(unknown) == monitor::MemoryPressureLevel::Unknown);

  monitor::SystemSnapshot normal;
  normal.memory_total_bytes = 16ULL << 30;
  normal.memory_wired_bytes = 2ULL << 30;
  normal.memory_active_bytes = 5ULL << 30;
  normal.memory_inactive_bytes = 3ULL << 30;
  normal.memory_speculative_bytes = 1ULL << 30;
  normal.memory_purgeable_bytes = 1ULL << 30;
  normal.memory_compressed_bytes = 512ULL << 20;
  normal.swap_used_bytes = 0;
  assert(monitor::derive_memory_pressure(normal) == monitor::MemoryPressureLevel::Normal);

  monitor::SystemSnapshot warn;
  warn.memory_total_bytes = 16ULL << 30;
  warn.memory_wired_bytes = 3ULL << 30;
  warn.memory_active_bytes = 6ULL << 30;
  warn.memory_inactive_bytes = 512ULL << 20;
  warn.memory_speculative_bytes = 256ULL << 20;
  warn.memory_purgeable_bytes = 256ULL << 20;
  warn.memory_compressed_bytes = 2ULL << 30;
  warn.swap_used_bytes = 768ULL << 20;
  assert(monitor::derive_memory_pressure(warn) == monitor::MemoryPressureLevel::Warn);

  monitor::SystemSnapshot critical;
  critical.memory_total_bytes = 16ULL << 30;
  critical.memory_wired_bytes = 4ULL << 30;
  critical.memory_active_bytes = 8ULL << 30;
  critical.memory_inactive_bytes = 256ULL << 20;
  critical.memory_speculative_bytes = 128ULL << 20;
  critical.memory_purgeable_bytes = 128ULL << 20;
  critical.memory_compressed_bytes = 4ULL << 30;
  critical.swap_used_bytes = 3ULL << 30;
  assert(monitor::derive_memory_pressure(critical) == monitor::MemoryPressureLevel::Critical);

  monitor::SystemSnapshot conservative;
  conservative.memory_total_bytes = 16ULL << 30;
  conservative.memory_wired_bytes = 2ULL << 30;
  conservative.memory_active_bytes = 5ULL << 30;
  conservative.memory_inactive_bytes = 4ULL << 30;
  conservative.memory_speculative_bytes = 2ULL << 30;
  conservative.memory_purgeable_bytes = 1ULL << 30;
  conservative.memory_compressed_bytes = 1ULL << 30;
  conservative.swap_used_bytes = 0;
  assert(monitor::derive_memory_pressure(conservative) == monitor::MemoryPressureLevel::Normal);

  assert(std::string(monitor::memory_pressure_label(monitor::MemoryPressureLevel::Unknown)) == "Unknown");
  assert(std::string(monitor::memory_pressure_label(monitor::MemoryPressureLevel::Normal)) == "Normal");
  assert(std::string(monitor::memory_pressure_label(monitor::MemoryPressureLevel::Warn)) == "Warn");
  assert(std::string(monitor::memory_pressure_label(monitor::MemoryPressureLevel::Critical)) == "Critical");
  return 0;
}
