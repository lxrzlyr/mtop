#include "monitor/model/workload_model.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace monitor {
namespace {

struct DetectedProcess {
  const ProcessSnapshot* process = nullptr;
  WorkloadDetection detection;
};

std::string grouping_key(const DetectedProcess& item,
                         const std::map<int, DetectedProcess>& detected_by_pid) {
  const ProcessSnapshot& process = *item.process;
  if (item.detection.kind == WorkloadKind::Ollama) {
    for (const auto& [pid, parent] : detected_by_pid) {
      (void)pid;
      if (parent.detection.kind == WorkloadKind::Ollama &&
          parent.detection.role == WorkloadRole::Manager &&
          (process.parent_pid == parent.process->pid || process.pid == parent.process->pid)) {
        return std::string(workload_kind_name(item.detection.kind)) + ":manager:" +
               std::to_string(parent.process->pid);
      }
    }
  }

  if (auto parent = detected_by_pid.find(process.parent_pid); parent != detected_by_pid.end()) {
    if (parent->second.detection.kind == item.detection.kind) {
      return std::string(workload_kind_name(item.detection.kind)) + ":parent:" +
             std::to_string(parent->second.process->pid);
    }
  }

  if (!item.detection.model_hint.empty()) {
    return std::string(workload_kind_name(item.detection.kind)) + ":model:" + item.detection.model_hint;
  }

  return std::string(workload_kind_name(item.detection.kind)) + ":pid:" + std::to_string(process.pid);
}

bool better_representative(const DetectedProcess& candidate, const DetectedProcess& current) {
  if (!current.process) {
    return true;
  }
  if (candidate.detection.role == WorkloadRole::Manager && current.detection.role != WorkloadRole::Manager) {
    return true;
  }
  if (candidate.detection.role == WorkloadRole::Server && current.detection.role != WorkloadRole::Manager &&
      current.detection.role != WorkloadRole::Server) {
    return true;
  }
  return candidate.detection.confidence > current.detection.confidence;
}

std::string first_available_signal(const std::string& current, const std::string& next) {
  if (current.empty() || current == "-" || current == "n/a" || current == "root" ||
      current == "wait" || current == "stale" || current == "parse" || current == "denied") {
    return next.empty() ? current : next;
  }
  return current;
}

std::string make_id(const WorkloadSnapshot& workload) {
  std::ostringstream out;
  out << workload_kind_name(workload.kind) << ":";
  if (!workload.model_hint.empty()) {
    out << workload.model_hint;
  } else if (!workload.processes.empty()) {
    out << workload.processes.front().pid;
  } else {
    out << "unknown";
  }
  return out.str();
}

}  // namespace

std::vector<WorkloadSnapshot> build_workloads(const SystemSnapshot& snapshot) {
  std::vector<DetectedProcess> detected;
  std::map<int, DetectedProcess> detected_by_pid;
  for (const auto& process : snapshot.processes) {
    WorkloadDetection detection = detect_workload_process(process);
    if (detection.confidence < 0.60) {
      continue;
    }
    DetectedProcess item{&process, std::move(detection)};
    detected_by_pid[process.pid] = item;
    detected.push_back(std::move(item));
  }

  std::map<std::string, WorkloadSnapshot> grouped;
  std::map<std::string, DetectedProcess> representative;
  for (const auto& item : detected) {
    const std::string key = grouping_key(item, detected_by_pid);
    WorkloadSnapshot& workload = grouped[key];
    const ProcessSnapshot& process = *item.process;
    if (workload.processes.empty()) {
      workload.kind = item.detection.kind;
      workload.model_hint = item.detection.model_hint;
      workload.name = item.detection.label.empty() ? process.name : item.detection.label;
      workload.core_mix = process.core_mix;
      workload.io = process.io;
      workload.power = process.power;
      workload.detection_status.availability = MetricAvailability::Available;
      workload.detection_status.reason = item.detection.reason;
    } else {
      if (workload.model_hint.empty()) {
        workload.model_hint = item.detection.model_hint;
      }
      workload.io = first_available_signal(workload.io, process.io);
      workload.power = first_available_signal(workload.power, process.power);
      if (workload.core_mix.empty() || workload.core_mix == "-") {
        workload.core_mix = process.core_mix;
      }
      if (workload.detection_status.reason.find(item.detection.reason) == std::string::npos) {
        workload.detection_status.reason += "; " + item.detection.reason;
      }
    }

    workload.processes.push_back({process.pid,
                                  process.parent_pid,
                                  item.detection.role,
                                  item.detection.confidence,
                                  process.command});
    workload.cpu_percent += process.cpu_percent;
    workload.memory_percent += process.memory_percent;
    workload.resident_bytes += process.resident_bytes;
    workload.virtual_bytes += process.virtual_bytes;
    workload.gpu_active = workload.gpu_active || process.gpu_active;

    if (better_representative(item, representative[key])) {
      representative[key] = item;
      workload.name = item.detection.label.empty() ? process.name : item.detection.label;
      if (!item.detection.model_hint.empty()) {
        workload.model_hint = item.detection.model_hint;
      }
      workload.detection_status.reason = item.detection.reason;
    }
  }

  std::vector<WorkloadSnapshot> workloads;
  workloads.reserve(grouped.size());
  for (auto& [key, workload] : grouped) {
    (void)key;
    std::sort(workload.processes.begin(), workload.processes.end(),
              [](const WorkloadProcessRef& lhs, const WorkloadProcessRef& rhs) {
                return lhs.pid < rhs.pid;
              });
    workload.id = make_id(workload);
    workloads.push_back(std::move(workload));
  }
  std::sort(workloads.begin(), workloads.end(), [](const WorkloadSnapshot& lhs, const WorkloadSnapshot& rhs) {
    if (lhs.resident_bytes != rhs.resident_bytes) {
      return lhs.resident_bytes > rhs.resident_bytes;
    }
    return lhs.name < rhs.name;
  });
  return workloads;
}

std::vector<AiProcessRow> build_ai_process_rows(const SystemSnapshot& snapshot) {
  std::vector<AiProcessRow> rows;
  rows.reserve(snapshot.processes.size());
  for (const auto& process : snapshot.processes) {
    WorkloadDetection detection = detect_workload_process(process);
    const bool is_ai = detection.confidence >= 0.60;
    if (!is_ai) {
      detection = {};
      detection.kind = WorkloadKind::Unknown;
      detection.role = WorkloadRole::Unknown;
      detection.label = process.name;
      detection.reason = "no AI workload signal";
    }
    rows.push_back({&process, std::move(detection), is_ai});
  }
  std::sort(rows.begin(), rows.end(), [](const AiProcessRow& lhs, const AiProcessRow& rhs) {
    if (lhs.is_ai != rhs.is_ai) {
      return lhs.is_ai && !rhs.is_ai;
    }
    if (lhs.is_ai && rhs.is_ai && lhs.process->resident_bytes != rhs.process->resident_bytes) {
      return lhs.process->resident_bytes > rhs.process->resident_bytes;
    }
    if (lhs.process->cpu_percent != rhs.process->cpu_percent) {
      return lhs.process->cpu_percent > rhs.process->cpu_percent;
    }
    return lhs.process->pid < rhs.process->pid;
  });
  return rows;
}

}  // namespace monitor
