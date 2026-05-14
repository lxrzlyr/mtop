#pragma once

namespace monitor {

enum class SortMode {
  Pid,
  Cpu,
  Mem,
  Time,
  Name,
  GpuActive,
  Io,
  Power,
};

struct SortState {
  SortMode mode = SortMode::Cpu;
  int direction = -1;
};

int default_sort_direction(SortMode mode);
SortMode cycle_sort(SortMode mode);
SortState apply_header_sort_click(SortState current, SortMode clicked_mode);

}  // namespace monitor
