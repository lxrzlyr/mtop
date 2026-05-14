#include "monitor/process_sort.hpp"

#include <cctype>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <string>

namespace monitor {
namespace {

bool is_missing_value(const std::string& value) {
  return value.empty() || value == "-" || value == "root" || value == "wait" ||
         value == "stale" || value == "parse" || value == "denied" || value == "n/a";
}

double parse_scaled_number(const std::string& text) {
  if (text.empty()) {
    return 0.0;
  }
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  double scale = 1.0;
  if (end && *end != '\0') {
    const char suffix = static_cast<char>(std::tolower(static_cast<unsigned char>(*end)));
    if (suffix == 'k') scale = 1024.0;
    else if (suffix == 'm') scale = 1024.0 * 1024.0;
    else if (suffix == 'g') scale = 1024.0 * 1024.0 * 1024.0;
    else if (suffix == 't') scale = 1024.0 * 1024.0 * 1024.0 * 1024.0;
  }
  return value * scale;
}

double missing_sort_score(int direction) {
  return direction > 0 ? std::numeric_limits<double>::infinity()
                       : -std::numeric_limits<double>::infinity();
}

double io_score(const std::string& value, int direction) {
  if (is_missing_value(value)) {
    return missing_sort_score(direction);
  }
  const std::size_t slash = value.find('/');
  if (slash == std::string::npos) {
    const double score = parse_scaled_number(value);
    return std::isfinite(score) ? score : missing_sort_score(direction);
  }
  const double score = parse_scaled_number(value.substr(0, slash)) +
                       parse_scaled_number(value.substr(slash + 1));
  return std::isfinite(score) ? score : missing_sort_score(direction);
}

double power_score(const std::string& value, int direction) {
  if (is_missing_value(value)) {
    return missing_sort_score(direction);
  }
  const double score = parse_scaled_number(value);
  return std::isfinite(score) ? score : missing_sort_score(direction);
}

template <typename T>
bool compare_value(const T& lhs, const T& rhs, int direction) {
  if (lhs == rhs) {
    return false;
  }
  return direction > 0 ? lhs < rhs : lhs > rhs;
}

}  // namespace

bool process_sort_less(const ProcessSnapshot& lhs,
                       const ProcessSnapshot& rhs,
                       SortMode mode,
                       int direction) {
  switch (mode) {
    case SortMode::Pid:
      if (lhs.pid != rhs.pid) return compare_value(lhs.pid, rhs.pid, direction);
      break;
    case SortMode::Cpu:
      if (lhs.cpu_percent != rhs.cpu_percent) return compare_value(lhs.cpu_percent, rhs.cpu_percent, direction);
      break;
    case SortMode::Mem:
      if (lhs.memory_percent != rhs.memory_percent) return compare_value(lhs.memory_percent, rhs.memory_percent, direction);
      break;
    case SortMode::Time:
      if (lhs.total_cpu_time_ns != rhs.total_cpu_time_ns) return compare_value(lhs.total_cpu_time_ns, rhs.total_cpu_time_ns, direction);
      break;
    case SortMode::Name:
      if (lhs.command != rhs.command) return compare_value(lhs.command, rhs.command, direction);
      break;
    case SortMode::GpuActive:
      if (lhs.gpu_active != rhs.gpu_active) return direction > 0 ? !lhs.gpu_active && rhs.gpu_active : lhs.gpu_active && !rhs.gpu_active;
      break;
    case SortMode::Io: {
      const double lhs_score = io_score(lhs.io, direction);
      const double rhs_score = io_score(rhs.io, direction);
      if (lhs_score != rhs_score) return compare_value(lhs_score, rhs_score, direction);
      break;
    }
    case SortMode::Power: {
      const double lhs_score = power_score(lhs.power, direction);
      const double rhs_score = power_score(rhs.power, direction);
      if (lhs_score != rhs_score) return compare_value(lhs_score, rhs_score, direction);
      break;
    }
  }
  return lhs.pid < rhs.pid;
}

}  // namespace monitor
