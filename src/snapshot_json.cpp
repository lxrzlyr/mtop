#include "monitor/snapshot_json.hpp"

#include "monitor/metrics.hpp"
#include "monitor/ui_support.hpp"

#include <cstdio>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

namespace monitor {
namespace {

class JsonWriter {
 public:
  void begin_object() {
    value_prefix();
    out_ << "{";
    first_stack_.push_back(true);
  }

  void end_object() {
    out_ << "}";
    first_stack_.pop_back();
  }

  void begin_array() {
    value_prefix();
    out_ << "[";
    first_stack_.push_back(true);
  }

  void end_array() {
    out_ << "]";
    first_stack_.pop_back();
  }

  void key(const char* name) {
    member_prefix();
    out_ << '"' << json_escape(name) << "\":";
    expecting_value_ = true;
  }

  void string_value(const std::string& value) {
    value_prefix();
    out_ << '"' << json_escape(value) << '"';
  }

  void bool_value(bool value) {
    value_prefix();
    out_ << (value ? "true" : "false");
  }

  void uint_value(std::uint64_t value) {
    value_prefix();
    out_ << value;
  }

  void int_value(int value) {
    value_prefix();
    out_ << value;
  }

  void double_value(double value) {
    value_prefix();
    if (!std::isfinite(value)) {
      out_ << "0.000";
      return;
    }
    std::ostringstream formatted;
    formatted.imbue(std::locale::classic());
    formatted << std::fixed << std::setprecision(3) << value;
    out_ << formatted.str();
  }

  std::string str() const {
    return out_.str();
  }

 private:
  void member_prefix() {
    if (!first_stack_.empty()) {
      if (!first_stack_.back()) {
        out_ << ",";
      }
      first_stack_.back() = false;
    }
  }

  void value_prefix() {
    if (expecting_value_) {
      expecting_value_ = false;
      return;
    }
    if (!first_stack_.empty()) {
      if (!first_stack_.back()) {
        out_ << ",";
      }
      first_stack_.back() = false;
    }
  }

  std::ostringstream out_;
  std::vector<bool> first_stack_;
  bool expecting_value_ = false;
};

void write_status(JsonWriter& json, const MetricStatus& status) {
  json.begin_object();
  json.key("availability");
  json.string_value(metric_availability_label(status.availability));
  json.key("reason");
  json.string_value(status.reason);
  json.key("age_ms");
  json.uint_value(status.age_ms);
  json.end_object();
}

void write_cpu(JsonWriter& json, const SystemSnapshot& snapshot) {
  json.key("cpu");
  json.begin_object();
  json.key("per_core_utilization");
  json.begin_array();
  for (const auto& core : snapshot.cpu_cores) {
    json.begin_object();
    json.key("cpu_id");
    json.int_value(core.cpu_id);
    json.key("cluster_type");
    json.string_value(core.cluster_type);
    json.key("label");
    json.string_value(core.label);
    json.key("utilization_percent");
    json.double_value(core.utilization_percent);
    json.end_object();
  }
  json.end_array();
  json.key("cluster_summary");
  json.begin_array();
  for (const auto& cluster : snapshot.cpu_clusters) {
    json.begin_object();
    json.key("name");
    json.string_value(cluster.name);
    json.key("label");
    json.string_value(std::string(1, cluster.label));
    json.key("core_count");
    json.int_value(cluster.core_count);
    json.key("utilization_percent");
    json.double_value(cluster.utilization_percent);
    json.key("frequency_mhz");
    json.int_value(cluster.frequency_mhz);
    json.key("frequency_available");
    json.bool_value(cluster.frequency_available);
    json.key("power_watts");
    json.double_value(cluster.power_watts);
    json.key("power_available");
    json.bool_value(cluster.power_available);
    json.end_object();
  }
  json.end_array();
  json.end_object();
}

void write_memory(JsonWriter& json, const SystemSnapshot& snapshot) {
  json.key("memory");
  json.begin_object();
  json.key("total_bytes");
  json.uint_value(snapshot.memory_total_bytes);
  json.key("used_bytes");
  json.uint_value(snapshot.memory_used_bytes);
  json.key("wired_bytes");
  json.uint_value(snapshot.memory_wired_bytes);
  json.key("active_bytes");
  json.uint_value(snapshot.memory_active_bytes);
  json.key("inactive_bytes");
  json.uint_value(snapshot.memory_inactive_bytes);
  json.key("compressed_bytes");
  json.uint_value(snapshot.memory_compressed_bytes);
  json.key("purgeable_bytes");
  json.uint_value(snapshot.memory_purgeable_bytes);
  json.key("speculative_bytes");
  json.uint_value(snapshot.memory_speculative_bytes);
  json.key("swap_total_bytes");
  json.uint_value(snapshot.swap_total_bytes);
  json.key("swap_used_bytes");
  json.uint_value(snapshot.swap_used_bytes);
  json.key("swapins_bytes_per_sec");
  json.uint_value(snapshot.swapins_bytes_per_sec);
  json.key("swapouts_bytes_per_sec");
  json.uint_value(snapshot.swapouts_bytes_per_sec);
  json.key("pressure_level");
  json.string_value(memory_pressure_label(snapshot.memory_pressure));
  json.key("pressure_status");
  write_status(json, snapshot.memory_pressure_status);
  json.end_object();
}

void write_gpu(JsonWriter& json, const SystemSnapshot& snapshot) {
  json.key("gpu");
  json.begin_object();
  json.key("utilization_percent");
  json.double_value(snapshot.gpu_utilization_percent);
  json.key("memory_used_bytes");
  json.uint_value(snapshot.gpu_memory_used_bytes);
  json.key("memory_total_bytes");
  json.uint_value(snapshot.gpu_memory_total_bytes);
  json.key("power_watts");
  json.double_value(snapshot.gpu_power_watts);
  json.key("frequency_mhz");
  json.int_value(snapshot.gpu_frequency_mhz);
  json.key("active_pids");
  json.begin_array();
  for (int pid : snapshot.gpu_active_pids) {
    json.int_value(pid);
  }
  json.end_array();
  json.key("status");
  write_status(json, snapshot.capabilities.gpu_total_status);
  json.end_object();
}

void write_io(JsonWriter& json, const SystemSnapshot& snapshot) {
  json.key("io");
  json.begin_object();
  json.key("disk");
  json.begin_object();
  json.key("read_bytes_per_sec");
  json.uint_value(snapshot.disk_io.read_bytes_per_sec);
  json.key("write_bytes_per_sec");
  json.uint_value(snapshot.disk_io.write_bytes_per_sec);
  json.key("status");
  write_status(json, snapshot.disk_io.status);
  json.end_object();
  json.key("paging");
  json.begin_object();
  json.key("pageins_bytes_per_sec");
  json.uint_value(snapshot.paging_io.pageins_bytes_per_sec);
  json.key("pageouts_bytes_per_sec");
  json.uint_value(snapshot.paging_io.pageouts_bytes_per_sec);
  json.key("swapins_bytes_per_sec");
  json.uint_value(snapshot.paging_io.swapins_bytes_per_sec);
  json.key("swapouts_bytes_per_sec");
  json.uint_value(snapshot.paging_io.swapouts_bytes_per_sec);
  json.key("status");
  write_status(json, snapshot.paging_io.status);
  json.end_object();
  json.key("network");
  json.begin_object();
  json.key("rx_bytes_per_sec");
  json.uint_value(snapshot.network_io.rx_bytes_per_sec);
  json.key("tx_bytes_per_sec");
  json.uint_value(snapshot.network_io.tx_bytes_per_sec);
  json.key("status");
  write_status(json, snapshot.network_io.status);
  json.end_object();
  json.end_object();
}

void write_memory_risk(JsonWriter& json, const MemoryRiskSnapshot& risk) {
  json.key("memory_risk");
  json.begin_object();
  json.key("level");
  json.string_value(memory_risk_name(risk.level));
  json.key("reason");
  json.string_value(risk.reason);
  json.key("estimated_headroom_bytes");
  json.uint_value(risk.estimated_headroom_bytes);
  json.key("reclaimable_bytes");
  json.uint_value(risk.reclaimable_bytes);
  json.key("compressed_bytes");
  json.uint_value(risk.compressed_bytes);
  json.key("swap_used_bytes");
  json.uint_value(risk.swap_used_bytes);
  json.key("swap_rate_bytes_per_sec");
  json.uint_value(risk.swap_rate_bytes_per_sec);
  json.end_object();
}

void write_workloads(JsonWriter& json, const std::vector<WorkloadSnapshot>& workloads) {
  json.key("workloads");
  json.begin_array();
  for (const auto& workload : workloads) {
    json.begin_object();
    json.key("id");
    json.string_value(workload.id);
    json.key("name");
    json.string_value(workload.name);
    json.key("kind");
    json.string_value(workload_kind_name(workload.kind));
    json.key("kind_label");
    json.string_value(workload_kind_label(workload.kind));
    json.key("model_hint");
    json.string_value(workload.model_hint);
    json.key("cpu_percent");
    json.double_value(workload.cpu_percent);
    json.key("memory_percent");
    json.double_value(workload.memory_percent);
    json.key("resident_bytes");
    json.uint_value(workload.resident_bytes);
    json.key("virtual_bytes");
    json.uint_value(workload.virtual_bytes);
    json.key("gpu_active");
    json.bool_value(workload.gpu_active);
    json.key("core_mix");
    json.string_value(workload.core_mix);
    json.key("io");
    json.string_value(workload.io);
    json.key("power");
    json.string_value(workload.power);
    json.key("detection_status");
    write_status(json, workload.detection_status);
    json.key("processes");
    json.begin_array();
    for (const auto& process : workload.processes) {
      json.begin_object();
      json.key("pid");
      json.int_value(process.pid);
      json.key("ppid");
      json.int_value(process.parent_pid);
      json.key("role");
      json.string_value(workload_role_name(process.role));
      json.key("detection_confidence");
      json.double_value(process.detection_confidence);
      json.key("command");
      json.string_value(process.command);
      json.end_object();
    }
    json.end_array();
    json.end_object();
  }
  json.end_array();
}

}  // namespace

std::string json_escape(const std::string& value) {
  std::ostringstream out;
  for (unsigned char ch : value) {
    switch (ch) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (ch < 0x20) {
          char buffer[8];
          std::snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
          out << buffer;
        } else {
          out << static_cast<char>(ch);
        }
    }
  }
  return out.str();
}

std::string snapshot_to_json(const SystemSnapshot& snapshot, const SnapshotJsonOptions& options) {
  JsonWriter json;
  json.begin_object();
  json.key("schema_version");
  json.int_value(options.include_v2_models ? 2 : 1);
  if (options.include_v2_models) {
    json.key("view_profile");
    json.string_value(view_profile_name(options.view_profile));
  }
  json.key("timestamp_unix_ms");
  json.uint_value(snapshot.timestamp_unix_ms);
  json.key("sample_interval_ms");
  json.uint_value(snapshot.sample_interval_ms);
  json.key("host");
  json.begin_object();
  json.key("macos_version");
  json.string_value(snapshot.macos_version);
  json.key("soc_name");
  json.string_value(snapshot.soc_name);
  json.key("cpu_core_count");
  json.int_value(snapshot.cpu_core_count);
  json.key("gpu_core_count");
  json.int_value(snapshot.gpu_core_count);
  json.key("memory_total_bytes");
  json.uint_value(snapshot.memory_total_bytes);
  json.end_object();
  json.key("capabilities");
  json.begin_object();
  json.key("root_mode");
  json.bool_value(snapshot.capabilities.root_mode);
  json.key("gpu_total_status");
  write_status(json, snapshot.capabilities.gpu_total_status);
  json.key("gpu_process_status");
  write_status(json, snapshot.capabilities.gpu_per_process_status);
  json.key("thermal_status");
  write_status(json, snapshot.capabilities.thermal_status);
  json.key("ane_status");
  write_status(json, snapshot.capabilities.ane_status);
  json.key("root_process_status");
  write_status(json, snapshot.capabilities.root_process_status);
  json.end_object();
  write_cpu(json, snapshot);
  write_memory(json, snapshot);
  write_gpu(json, snapshot);
  json.key("ane");
  json.begin_object();
  json.key("summary");
  json.string_value(snapshot.ane);
  json.key("status");
  write_status(json, snapshot.capabilities.ane_status);
  json.end_object();
  write_io(json, snapshot);
  json.key("processes");
  json.begin_array();
  for (const auto& process : snapshot.processes) {
    json.begin_object();
    json.key("pid");
    json.int_value(process.pid);
    json.key("ppid");
    json.int_value(process.parent_pid);
    json.key("name");
    json.string_value(process.name);
    json.key("command");
    json.string_value(process.command);
    json.key("user");
    json.string_value(process.user);
    json.key("state");
    json.string_value(std::string(1, process.state));
    json.key("cpu_percent");
    json.double_value(process.cpu_percent);
    json.key("memory_percent");
    json.double_value(process.memory_percent);
    json.key("resident_bytes");
    json.uint_value(process.resident_bytes);
    json.key("virtual_bytes");
    json.uint_value(process.virtual_bytes);
    json.key("gpu_active");
    json.bool_value(process.gpu_active);
    json.key("core_mix");
    json.string_value(process.core_mix);
    json.key("io");
    json.string_value(process.io);
    json.key("power");
    json.string_value(process.power);
    json.end_object();
  }
  json.end_array();
  if (options.include_v2_models) {
    write_workloads(json, options.workloads);
    write_memory_risk(json, options.memory_risk);
  }
  json.end_object();
  return json.str();
}

std::string snapshot_to_json(const SystemSnapshot& snapshot) {
  SnapshotJsonOptions options;
  options.workloads = build_workloads(snapshot);
  options.memory_risk = derive_memory_risk(snapshot, {}, options.workloads);
  return snapshot_to_json(snapshot, options);
}

}  // namespace monitor
