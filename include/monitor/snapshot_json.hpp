#pragma once

#include <string>
#include <vector>

#include "monitor/model/memory_risk_model.hpp"
#include "monitor/model/workload_model.hpp"
#include "monitor/snapshot.hpp"
#include "monitor/ui/view_profile.hpp"

namespace monitor {

struct SnapshotJsonOptions {
  ViewProfile view_profile = ViewProfile::Alpha;
  bool include_v2_models = true;
  std::vector<WorkloadSnapshot> workloads;
  MemoryRiskSnapshot memory_risk;
};

std::string json_escape(const std::string& value);
std::string snapshot_to_json(const SystemSnapshot& snapshot);
std::string snapshot_to_json(const SystemSnapshot& snapshot, const SnapshotJsonOptions& options);

}  // namespace monitor
