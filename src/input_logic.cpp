#include "monitor/input_logic.hpp"

namespace monitor {

SortMode cycle_sort(SortMode mode) {
  switch (mode) {
    case SortMode::Pid: return SortMode::Cpu;
    case SortMode::Cpu: return SortMode::Mem;
    case SortMode::Mem: return SortMode::Time;
    case SortMode::Time: return SortMode::Name;
    case SortMode::Name: return SortMode::GpuActive;
    case SortMode::GpuActive: return SortMode::Io;
    case SortMode::Io: return SortMode::Power;
    case SortMode::Power: return SortMode::Pid;
  }
  return SortMode::Cpu;
}

int default_sort_direction(SortMode mode) {
  switch (mode) {
    case SortMode::Pid: return 1;
    case SortMode::Name: return 1;
    case SortMode::Cpu:
    case SortMode::Mem:
    case SortMode::Time:
    case SortMode::GpuActive:
    case SortMode::Io:
    case SortMode::Power:
      return -1;
  }
  return -1;
}

SortState apply_header_sort_click(SortState current, SortMode clicked_mode) {
  if (current.mode == clicked_mode) {
    current.direction *= -1;
    return current;
  }
  current.mode = clicked_mode;
  current.direction = default_sort_direction(clicked_mode);
  return current;
}

}  // namespace monitor
