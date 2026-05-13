#pragma once

#include <cstdint>
#include <string>

#include "monitor/snapshot.hpp"

namespace monitor {

enum class ViewMode {
  Overview,
  SystemIo,
  GpuActive,
};

const char* view_mode_label(ViewMode mode);
ViewMode cycle_view_mode(ViewMode mode, int delta);
const char* metric_availability_label(MetricAvailability availability);
std::string metric_status_label(const MetricStatus& status);
std::string format_throughput_rate(bool available, std::uint64_t bytes_per_sec);
std::string format_throughput_pair(bool available,
                                   std::uint64_t first_bytes_per_sec,
                                   std::uint64_t second_bytes_per_sec);
std::string format_labeled_throughput_summary(const std::string& prefix,
                                              const std::string& first_label,
                                              std::uint64_t first_bytes_per_sec,
                                              const std::string& second_label,
                                              std::uint64_t second_bytes_per_sec,
                                              bool available,
                                              bool compact);

}  // namespace monitor
