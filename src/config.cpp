#include "monitor/config.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace monitor {
namespace {

std::string trim(const std::string& input) {
  const std::size_t start = input.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }
  const std::size_t end = input.find_last_not_of(" \t\r\n");
  return input.substr(start, end - start + 1);
}

std::string lowercase(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return text;
}

std::filesystem::path default_config_path() {
  const char* home = std::getenv("HOME");
  if (!home) {
    return {};
  }
  return std::filesystem::path(home) / ".config" / "mtop" / "config";
}

std::optional<int> parse_int(const std::string& value) {
  errno = 0;
  char* end = nullptr;
  const long parsed = std::strtol(value.c_str(), &end, 10);
  if (end == value.c_str() || errno == ERANGE ||
      parsed < std::numeric_limits<int>::min() ||
      parsed > std::numeric_limits<int>::max()) {
    return std::nullopt;
  }
  while (end && *end != '\0') {
    if (!std::isspace(static_cast<unsigned char>(*end))) {
      return std::nullopt;
    }
    ++end;
  }
  return static_cast<int>(parsed);
}

bool parse_bool(const std::string& value, bool fallback) {
  const std::string lowered = lowercase(trim(value));
  if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
    return true;
  }
  if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
    return false;
  }
  return fallback;
}

ViewMode parse_view_mode(const std::string& value, ViewMode fallback) {
  const std::string lowered = lowercase(trim(value));
  if (lowered == "overview" || lowered == "main") return ViewMode::Overview;
  if (lowered == "system_io" || lowered == "system-io" || lowered == "io") return ViewMode::SystemIo;
  if (lowered == "gpu_active" || lowered == "gpu-active" || lowered == "gpu") return ViewMode::GpuActive;
  return fallback;
}

SortMode parse_sort_mode(const std::string& value, SortMode fallback) {
  const std::string lowered = lowercase(trim(value));
  if (lowered == "pid") return SortMode::Pid;
  if (lowered == "cpu" || lowered == "pcpu") return SortMode::Cpu;
  if (lowered == "mem" || lowered == "memory") return SortMode::Mem;
  if (lowered == "time") return SortMode::Time;
  if (lowered == "name" || lowered == "command") return SortMode::Name;
  if (lowered == "gpu" || lowered == "gpu_active" || lowered == "gpu-active") return SortMode::GpuActive;
  if (lowered == "io") return SortMode::Io;
  if (lowered == "pwr" || lowered == "power") return SortMode::Power;
  return fallback;
}

int parse_sort_direction(const std::string& value, int fallback) {
  const std::string lowered = lowercase(trim(value));
  if (lowered == "asc" || lowered == "ascending" || lowered == "1") return 1;
  if (lowered == "desc" || lowered == "descending" || lowered == "-1") return -1;
  return fallback;
}

void parse_line(const std::string& line, AppConfig& config) {
  if (line.empty() || line[0] == '#') {
    return;
  }
  const std::size_t eq = line.find('=');
  if (eq == std::string::npos) {
    return;
  }
  const std::string key = trim(line.substr(0, eq));
  const std::string value = trim(line.substr(eq + 1));
  if (key == "theme" && !value.empty()) {
    config.theme = value;
  } else if (key == "refresh_ms" && !value.empty()) {
    config.refresh_ms = std::clamp(parse_int(value).value_or(config.refresh_ms), 100, 60000);
  } else if (key == "process_limit" && !value.empty()) {
    config.process_limit = std::clamp(parse_int(value).value_or(config.process_limit), 4, 1000);
  } else if (key == "demo_mode") {
    config.demo_mode = parse_bool(value, config.demo_mode);
  } else if (key == "root_sample_ms" && !value.empty()) {
    config.root_sample_ms = std::clamp(parse_int(value).value_or(config.root_sample_ms), 250, 60000);
  } else if (key == "snapshot_interval_ms" && !value.empty()) {
    config.snapshot_interval_ms = std::clamp(parse_int(value).value_or(config.snapshot_interval_ms), 100, 60000);
  } else if (key == "show_cached_memory") {
    config.show_cached_memory = parse_bool(value, config.show_cached_memory);
  } else if (key == "default_view" && !value.empty()) {
    config.default_view = parse_view_mode(value, config.default_view);
  } else if (key == "sort" && !value.empty()) {
    config.sort = parse_sort_mode(value, config.sort);
    config.sort_direction = default_sort_direction(config.sort);
  } else if (key == "sort_direction" && !value.empty()) {
    config.sort_direction = parse_sort_direction(value, config.sort_direction);
  } else if (key == "view_profile" && !value.empty()) {
    config.view_profile = parse_view_profile(value).value_or(ViewProfile::Alpha);
  }
}

}  // namespace

AppConfig load_config(const std::string& explicit_path) {
  AppConfig config;
  std::filesystem::path path = explicit_path.empty() ? default_config_path() : std::filesystem::path(explicit_path);
  if (path.empty() || !std::filesystem::exists(path)) {
    return config;
  }

  std::ifstream input(path);
  std::string line;
  while (std::getline(input, line)) {
    parse_line(trim(line), config);
  }
  return config;
}

std::string config_default_path() {
  return default_config_path().string();
}

bool persist_view_profile(const std::string& explicit_path, ViewProfile profile, std::string* error) {
  const std::filesystem::path path = explicit_path.empty() ? default_config_path() : std::filesystem::path(explicit_path);
  if (path.empty()) {
    if (error) {
      *error = "could not determine config path";
    }
    return false;
  }

  std::vector<std::string> lines;
  if (std::filesystem::exists(path)) {
    std::ifstream input(path);
    if (!input) {
      if (error) {
        *error = "could not read config file: " + path.string();
      }
      return false;
    }
    std::string line;
    while (std::getline(input, line)) {
      lines.push_back(line);
    }
  }

  const std::string desired = std::string("view_profile=") + view_profile_name(profile);
  bool updated = false;
  for (std::string& line : lines) {
    const std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
      continue;
    }
    const std::size_t eq = trimmed.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    if (trim(trimmed.substr(0, eq)) == "view_profile") {
      line = desired;
      updated = true;
      break;
    }
  }
  if (!updated) {
    lines.push_back(desired);
  }

  std::error_code ec;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      if (error) {
        *error = "could not create config directory: " + path.parent_path().string() + ": " + ec.message();
      }
      return false;
    }
  }

  std::ofstream output(path, std::ios::trunc);
  if (!output) {
    if (error) {
      *error = "could not write config file: " + path.string();
    }
    return false;
  }
  for (const std::string& line : lines) {
    output << line << '\n';
  }
  if (!output) {
    if (error) {
      *error = "could not finish writing config file: " + path.string();
    }
    return false;
  }
  return true;
}

}  // namespace monitor
