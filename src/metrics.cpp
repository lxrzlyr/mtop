#include "monitor/metrics.hpp"

#include <algorithm>

namespace monitor {
namespace {

constexpr std::uint64_t kMiB = 1024ULL * 1024ULL;
constexpr std::uint64_t kGiB = 1024ULL * 1024ULL * 1024ULL;

double fraction_of_total(std::uint64_t bytes, std::uint64_t total) {
  if (total == 0) {
    return 0.0;
  }
  return static_cast<double>(bytes) / static_cast<double>(total);
}

}  // namespace

MemoryPressureLevel derive_memory_pressure(const SystemSnapshot& snapshot) {
  if (snapshot.memory_total_bytes == 0) {
    return MemoryPressureLevel::Unknown;
  }

  const std::uint64_t reclaimable_bytes = snapshot.memory_inactive_bytes +
                                          snapshot.memory_speculative_bytes +
                                          snapshot.memory_purgeable_bytes;
  const double compressed_ratio = fraction_of_total(snapshot.memory_compressed_bytes, snapshot.memory_total_bytes);
  const double swap_ratio = fraction_of_total(snapshot.swap_used_bytes, snapshot.memory_total_bytes);
  const double active_ratio = fraction_of_total(snapshot.memory_active_bytes + snapshot.memory_wired_bytes,
                                                snapshot.memory_total_bytes);
  const double reclaimable_ratio = fraction_of_total(reclaimable_bytes, snapshot.memory_total_bytes);

  const bool severe_swap = snapshot.swap_used_bytes >= std::max<std::uint64_t>(2 * kGiB, snapshot.memory_total_bytes / 20);
  const bool moderate_swap = snapshot.swap_used_bytes >= std::max<std::uint64_t>(512 * kMiB, snapshot.memory_total_bytes / 100);
  const bool severe_compression = compressed_ratio >= 0.20 && reclaimable_ratio <= 0.08 && active_ratio >= 0.50;
  const bool moderate_compression = compressed_ratio >= 0.10 && reclaimable_ratio <= 0.16 && active_ratio >= 0.45;

  if (severe_swap || severe_compression) {
    return MemoryPressureLevel::Critical;
  }
  if (moderate_swap || moderate_compression) {
    return MemoryPressureLevel::Warn;
  }
  return MemoryPressureLevel::Normal;
}

const char* memory_pressure_label(MemoryPressureLevel level) {
  switch (level) {
    case MemoryPressureLevel::Unknown: return "Unknown";
    case MemoryPressureLevel::Normal: return "Normal";
    case MemoryPressureLevel::Warn: return "Warn";
    case MemoryPressureLevel::Critical: return "Critical";
  }
  return "Unknown";
}

}  // namespace monitor
