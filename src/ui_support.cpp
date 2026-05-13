#include "monitor/ui_support.hpp"

#include <array>
#include <cstdio>

namespace monitor {
namespace {

std::string compact_rate(std::uint64_t value) {
  static constexpr std::array<const char*, 5> kUnits = {"B/s", "Ki/s", "Mi/s", "Gi/s", "Ti/s"};
  double current = static_cast<double>(value);
  std::size_t unit = 0;
  while (current >= 1024.0 && unit + 1 < kUnits.size()) {
    current /= 1024.0;
    ++unit;
  }

  char buffer[32];
  if (unit == 0) {
    std::snprintf(buffer, sizeof(buffer), "%llu%s",
                  static_cast<unsigned long long>(value),
                  kUnits[unit]);
  } else if (current >= 10.0) {
    std::snprintf(buffer, sizeof(buffer), "%.0f%s", current, kUnits[unit]);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%.1f%s", current, kUnits[unit]);
  }
  return buffer;
}

}  // namespace

const char* view_mode_label(ViewMode mode) {
  switch (mode) {
    case ViewMode::Overview: return "Overview";
    case ViewMode::SystemIo: return "System I/O";
    case ViewMode::GpuActive: return "GPU Active";
  }
  return "Overview";
}

ViewMode cycle_view_mode(ViewMode mode, int delta) {
  constexpr std::array<ViewMode, 3> kModes = {
      ViewMode::Overview,
      ViewMode::SystemIo,
      ViewMode::GpuActive,
  };
  int index = 0;
  for (int i = 0; i < static_cast<int>(kModes.size()); ++i) {
    if (kModes[i] == mode) {
      index = i;
      break;
    }
  }
  const int size = static_cast<int>(kModes.size());
  index = (index + delta) % size;
  if (index < 0) {
    index += size;
  }
  return kModes[index];
}

const char* metric_availability_label(MetricAvailability availability) {
  switch (availability) {
    case MetricAvailability::Available: return "ok";
    case MetricAvailability::RequiresRoot: return "root";
    case MetricAvailability::UnsupportedHardware: return "hw";
    case MetricAvailability::UnsupportedOS: return "os";
    case MetricAvailability::PermissionDenied: return "denied";
    case MetricAvailability::Waiting: return "wait";
    case MetricAvailability::Stale: return "stale";
    case MetricAvailability::ParseFailed: return "parse";
    case MetricAvailability::Unavailable: return "n/a";
  }
  return "n/a";
}

std::string metric_status_label(const MetricStatus& status) {
  const std::string label = metric_availability_label(status.availability);
  if (!status.reason.empty()) {
    return label + ": " + status.reason;
  }
  return label;
}

std::string format_throughput_rate(bool available, std::uint64_t bytes_per_sec) {
  if (!available) {
    return "n/a";
  }
  return compact_rate(bytes_per_sec);
}

std::string format_throughput_pair(bool available,
                                   std::uint64_t first_bytes_per_sec,
                                   std::uint64_t second_bytes_per_sec) {
  if (!available) {
    return "n/a";
  }
  return format_throughput_rate(true, first_bytes_per_sec) + " / " +
         format_throughput_rate(true, second_bytes_per_sec);
}

std::string format_labeled_throughput_summary(const std::string& prefix,
                                              const std::string& first_label,
                                              std::uint64_t first_bytes_per_sec,
                                              const std::string& second_label,
                                              std::uint64_t second_bytes_per_sec,
                                              bool available,
                                              bool compact) {
  if (!available) {
    return prefix + (compact ? " n/a" : " unavailable");
  }
  if (compact) {
    return prefix + " " + first_label + " " + format_throughput_rate(true, first_bytes_per_sec) +
           " " + second_label + " " + format_throughput_rate(true, second_bytes_per_sec);
  }
  return prefix + "  " + first_label + " " + format_throughput_rate(true, first_bytes_per_sec) +
         "  " + second_label + " " + format_throughput_rate(true, second_bytes_per_sec);
}

}  // namespace monitor
