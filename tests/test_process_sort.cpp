#include <algorithm>
#include <cassert>
#include <vector>

#include "monitor/process_sort.hpp"

int main() {
  using monitor::ProcessSnapshot;
  using monitor::SortMode;

  ProcessSnapshot heavy;
  heavy.pid = 10;
  heavy.io = "4.0K/2.0K";
  heavy.power = "42";
  heavy.gpu_active = false;

  ProcessSnapshot missing;
  missing.pid = 11;
  missing.io = "root";
  missing.power = "n/a";
  missing.gpu_active = false;

  ProcessSnapshot gpu;
  gpu.pid = 12;
  gpu.io = "1.0K/1.0K";
  gpu.power = "3";
  gpu.gpu_active = true;

  std::vector<ProcessSnapshot> rows = {missing, gpu, heavy};
  std::sort(rows.begin(), rows.end(), [](const auto& lhs, const auto& rhs) {
    return monitor::process_sort_less(lhs, rhs, SortMode::Io, -1);
  });
  assert(rows[0].pid == 10);
  assert(rows[1].pid == 12);
  assert(rows[2].pid == 11);

  std::sort(rows.begin(), rows.end(), [](const auto& lhs, const auto& rhs) {
    return monitor::process_sort_less(lhs, rhs, SortMode::Io, 1);
  });
  assert(rows[0].pid == 12);
  assert(rows[1].pid == 10);
  assert(rows[2].pid == 11);

  std::sort(rows.begin(), rows.end(), [](const auto& lhs, const auto& rhs) {
    return monitor::process_sort_less(lhs, rhs, SortMode::Power, -1);
  });
  assert(rows[0].pid == 10);
  assert(rows[2].pid == 11);

  std::sort(rows.begin(), rows.end(), [](const auto& lhs, const auto& rhs) {
    return monitor::process_sort_less(lhs, rhs, SortMode::GpuActive, -1);
  });
  assert(rows[0].pid == 12);
  return 0;
}
