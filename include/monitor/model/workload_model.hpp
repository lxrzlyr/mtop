#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "monitor/model/workload_detector.hpp"
#include "monitor/snapshot.hpp"

namespace monitor {

struct WorkloadProcessRef {
  int pid = -1;
  int parent_pid = -1;
  WorkloadRole role = WorkloadRole::Unknown;
  double detection_confidence = 0.0;
  std::string command;
};

struct WorkloadSnapshot {
  std::string id;
  std::string name;
  WorkloadKind kind = WorkloadKind::Unknown;
  std::string model_hint;
  std::vector<WorkloadProcessRef> processes;

  double cpu_percent = 0.0;
  double memory_percent = 0.0;
  std::uint64_t resident_bytes = 0;
  std::uint64_t virtual_bytes = 0;

  bool gpu_active = false;
  std::string core_mix = "-";
  std::string io = "-";
  std::string power = "-";

  MetricStatus detection_status;
};

struct AiProcessRow {
  const ProcessSnapshot* process = nullptr;
  WorkloadDetection detection;
  bool is_ai = false;
};

std::vector<WorkloadSnapshot> build_workloads(const SystemSnapshot& snapshot);
std::vector<AiProcessRow> build_ai_process_rows(const SystemSnapshot& snapshot);

}  // namespace monitor
