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
    output << "root_sample_ms=1500\n";
    output << "snapshot_interval_ms=750\n";
    output << "show_cached_memory=yes\n";
    output << "default_view=gpu-active\n";
    output << "sort=io\n";
    output << "sort_direction=asc\n";
    output << "view_profile=beta\n";
  }

  monitor::AppConfig config = monitor::load_config(temp.string());
  assert(config.theme == "green");
  assert(config.refresh_ms == 250);
  assert(config.process_limit == 18);
  assert(config.demo_mode);
  assert(config.root_sample_ms == 1500);
  assert(config.snapshot_interval_ms == 750);
  assert(config.show_cached_memory);
  assert(config.default_view == monitor::ViewMode::GpuActive);
  assert(config.sort == monitor::SortMode::Io);
  assert(config.sort_direction == 1);
  assert(config.view_profile == monitor::ViewProfile::Beta);

  const std::filesystem::path invalid = std::filesystem::temp_directory_path() / "mtop_test_invalid_config";
  {
    std::ofstream output(invalid);
    output << "refresh_ms=0\n";
    output << "process_limit=-2\n";
    output << "demo_mode=no\n";
    output << "root_sample_ms=1\n";
    output << "snapshot_interval_ms=abc\n";
    output << "show_cached_memory=maybe\n";
    output << "default_view=unknown\n";
    output << "sort=wat\n";
    output << "sort_direction=sideways\n";
    output << "view_profile=classic\n";
  }

  config = monitor::load_config(invalid.string());
  assert(config.refresh_ms == 100);
  assert(config.process_limit == 4);
  assert(!config.demo_mode);
  assert(config.root_sample_ms == 250);
  assert(config.snapshot_interval_ms == 1000);
  assert(!config.show_cached_memory);
  assert(config.default_view == monitor::ViewMode::Overview);
  assert(config.sort == monitor::SortMode::Cpu);
  assert(config.sort_direction == -1);
  assert(config.view_profile == monitor::ViewProfile::Alpha);

  const std::filesystem::path missing = std::filesystem::temp_directory_path() / "mtop_test_missing_config";
  std::filesystem::remove(missing);
  config = monitor::load_config(missing.string());
  assert(config.view_profile == monitor::ViewProfile::Alpha);

  const std::filesystem::path append_path = std::filesystem::temp_directory_path() / "mtop_test_append_config";
  {
    std::ofstream output(append_path);
    output << "theme=mono\n";
    output << "refresh_ms=500\n";
  }
  std::string error;
  assert(monitor::persist_view_profile(append_path.string(), monitor::ViewProfile::Beta, &error));
  {
    std::ifstream input(append_path);
    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    assert(contents.find("theme=mono\n") != std::string::npos);
    assert(contents.find("refresh_ms=500\n") != std::string::npos);
    assert(contents.find("view_profile=beta\n") != std::string::npos);
  }

  const std::filesystem::path update_path = std::filesystem::temp_directory_path() / "mtop_test_update_config";
  {
    std::ofstream output(update_path);
    output << "# keep me\n";
    output << "view_profile=beta\n";
    output << "process_limit=22\n";
  }
  assert(monitor::persist_view_profile(update_path.string(), monitor::ViewProfile::Alpha, &error));
  {
    std::ifstream input(update_path);
    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    assert(contents.find("# keep me\n") != std::string::npos);
    assert(contents.find("view_profile=alpha\n") != std::string::npos);
    assert(contents.find("view_profile=beta\n") == std::string::npos);
    assert(contents.find("process_limit=22\n") != std::string::npos);
  }

  std::filesystem::remove(temp);
  std::filesystem::remove(invalid);
  std::filesystem::remove(append_path);
  std::filesystem::remove(update_path);
  return 0;
}
