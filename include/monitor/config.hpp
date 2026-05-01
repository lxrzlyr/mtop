#pragma once

#include <string>

namespace monitor {

struct AppConfig {
  std::string theme = "apple";
  int refresh_ms = 1000;
  int process_limit = 12;
  bool demo_mode = false;
};

AppConfig load_config(const std::string& explicit_path = "");
std::string config_default_path();

}  // namespace monitor
