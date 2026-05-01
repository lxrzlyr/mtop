#include "monitor/config.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

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

std::filesystem::path default_config_path() {
  const char* home = std::getenv("HOME");
  if (!home) {
    return {};
  }
  return std::filesystem::path(home) / ".config" / "mtop" / "config";
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
    config.refresh_ms = std::max(100, std::atoi(value.c_str()));
  } else if (key == "process_limit" && !value.empty()) {
    config.process_limit = std::max(4, std::atoi(value.c_str()));
  } else if (key == "demo_mode") {
    config.demo_mode = (value == "1" || value == "true" || value == "yes");
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

}  // namespace monitor
