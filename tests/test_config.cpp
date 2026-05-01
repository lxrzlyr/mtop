#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "monitor/config.hpp"

int main() {
  const std::filesystem::path temp = std::filesystem::temp_directory_path() / "mtop_test_config";
  {
    std::ofstream output(temp);
    output << "theme=green\n";
    output << "refresh_ms=250\n";
    output << "process_limit=18\n";
    output << "demo_mode=true\n";
  }

  monitor::AppConfig config = monitor::load_config(temp.string());
  assert(config.theme == "green");
  assert(config.refresh_ms == 250);
  assert(config.process_limit == 18);
  assert(config.demo_mode);

  std::filesystem::remove(temp);
  return 0;
}
