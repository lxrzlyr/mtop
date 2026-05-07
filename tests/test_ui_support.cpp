#include <cassert>
#include <string>

#include "monitor/ui_support.hpp"

int main() {
  using monitor::ViewMode;

  assert(std::string(monitor::view_mode_label(ViewMode::Overview)) == "Overview");
  assert(std::string(monitor::view_mode_label(ViewMode::SystemIo)) == "System I/O");
  assert(std::string(monitor::view_mode_label(ViewMode::GpuActive)) == "GPU Active");

  assert(monitor::cycle_view_mode(ViewMode::Overview, 1) == ViewMode::SystemIo);
  assert(monitor::cycle_view_mode(ViewMode::SystemIo, 1) == ViewMode::GpuActive);
  assert(monitor::cycle_view_mode(ViewMode::GpuActive, 1) == ViewMode::Overview);
  assert(monitor::cycle_view_mode(ViewMode::Overview, -1) == ViewMode::GpuActive);

  assert(monitor::format_throughput_rate(false, 0) == "n/a");
  assert(monitor::format_throughput_rate(true, 0) == "0B/s");
  assert(monitor::format_throughput_rate(true, 2048) == "2.0Ki/s");
  assert(monitor::format_throughput_pair(false, 0, 0) == "n/a");
  assert(monitor::format_throughput_pair(true, 0, 0) == "0B/s / 0B/s");
  assert(monitor::format_labeled_throughput_summary("Disk", "R", 0, "W", 0, false, false) == "Disk unavailable");
  assert(monitor::format_labeled_throughput_summary("Disk", "R", 0, "W", 0, false, true) == "Disk n/a");
  assert(monitor::format_labeled_throughput_summary("Net", "RX", 2048, "TX", 4096, true, true) == "Net RX 2.0Ki/s TX 4.0Ki/s");
  assert(monitor::format_labeled_throughput_summary("Net", "RX", 2048, "TX", 4096, true, false) == "Net  RX 2.0Ki/s  TX 4.0Ki/s");
  return 0;
}
