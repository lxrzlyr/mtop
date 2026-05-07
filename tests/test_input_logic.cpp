#include <cassert>

#include "monitor/input_logic.hpp"

int main() {
  using monitor::SortMode;
  using monitor::SortState;

  assert(monitor::default_sort_direction(SortMode::Pid) == 1);
  assert(monitor::default_sort_direction(SortMode::Name) == 1);
  assert(monitor::default_sort_direction(SortMode::Cpu) == -1);
  assert(monitor::default_sort_direction(SortMode::Mem) == -1);
  assert(monitor::default_sort_direction(SortMode::Time) == -1);

  SortState state{SortMode::Cpu, -1};
  state = monitor::apply_header_sort_click(state, SortMode::Mem);
  assert(state.mode == SortMode::Mem);
  assert(state.direction == -1);

  state = monitor::apply_header_sort_click(state, SortMode::Mem);
  assert(state.mode == SortMode::Mem);
  assert(state.direction == 1);

  state = monitor::apply_header_sort_click(state, SortMode::Cpu);
  assert(state.mode == SortMode::Cpu);
  assert(state.direction == -1);

  state = monitor::apply_header_sort_click(state, SortMode::Cpu);
  assert(state.direction == 1);

  assert(monitor::cycle_sort(SortMode::Pid) == SortMode::Cpu);
  assert(monitor::cycle_sort(SortMode::Cpu) == SortMode::Mem);
  assert(monitor::cycle_sort(SortMode::Mem) == SortMode::Time);
  assert(monitor::cycle_sort(SortMode::Time) == SortMode::Name);
  assert(monitor::cycle_sort(SortMode::Name) == SortMode::Pid);
  return 0;
}
