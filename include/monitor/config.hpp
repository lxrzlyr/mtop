#pragma once

#include <string>

#include "monitor/input_logic.hpp"
#include "monitor/ui/view_profile.hpp"
#include "monitor/ui_support.hpp"

namespace monitor {

struct AppConfig {
  std::string theme = "apple";
  int refresh_ms = 1000;
  int process_limit = 12;
  bool demo_mode = false;
  int root_sample_ms = 1000;
  int snapshot_interval_ms = 1000;
  bool show_cached_memory = false;
  ViewMode default_view = ViewMode::Overview;
  SortMode sort = SortMode::Cpu;
  int sort_direction = -1;
  ViewProfile view_profile = ViewProfile::Alpha;
};

AppConfig load_config(const std::string& explicit_path = "");
std::string config_default_path();
bool persist_view_profile(const std::string& explicit_path, ViewProfile profile, std::string* error);

}  // namespace monitor
