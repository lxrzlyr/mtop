#pragma once

#include "monitor/snapshot.hpp"

namespace monitor {

MemoryPressureLevel derive_memory_pressure(const SystemSnapshot& snapshot);
const char* memory_pressure_label(MemoryPressureLevel level);

}  // namespace monitor
