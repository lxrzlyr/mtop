#pragma once

#include "monitor/input_logic.hpp"
#include "monitor/snapshot.hpp"

namespace monitor {

bool process_sort_less(const ProcessSnapshot& lhs,
                       const ProcessSnapshot& rhs,
                       SortMode mode,
                       int direction);

}  // namespace monitor
