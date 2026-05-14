#pragma once

#include "monitor/snapshot.hpp"

namespace monitor {

class Sampler {
 public:
  virtual ~Sampler() = default;
  virtual SystemSnapshot sample() = 0;
};

Sampler* create_darwin_sampler(int root_sample_ms = 1000);
Sampler* create_demo_sampler();

}  // namespace monitor
