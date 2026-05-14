#pragma once

#include <string>

#include "monitor/snapshot.hpp"

namespace monitor {

enum class WorkloadKind {
  Ollama,
  LlamaCpp,
  Mlx,
  PythonInference,
  LmStudio,
  ComfyUi,
  GenericModel,
  Unknown,
};

enum class WorkloadRole {
  Manager,
  Server,
  Runner,
  Worker,
  Frontend,
  Unknown,
};

struct WorkloadDetection {
  WorkloadKind kind = WorkloadKind::Unknown;
  WorkloadRole role = WorkloadRole::Unknown;
  std::string label;
  std::string model_hint;
  std::string reason;
  double confidence = 0.0;
};

const char* workload_kind_name(WorkloadKind kind);
const char* workload_kind_label(WorkloadKind kind);
const char* workload_role_name(WorkloadRole role);
WorkloadDetection detect_workload_process(const ProcessSnapshot& process);

}  // namespace monitor
