#include <cassert>
#include <limits>
#include <string>

#include "monitor/model/workload_model.hpp"
#include "monitor/snapshot_json.hpp"

int main() {
  assert(monitor::json_escape("a\"b\\c\n") == "a\\\"b\\\\c\\n");

  monitor::SystemSnapshot snapshot;
  snapshot.timestamp_unix_ms = 123456789;
  snapshot.sample_interval_ms = 1000;
  snapshot.macos_version = "macOS test";
  snapshot.soc_name = "Apple \"Test\"";
  snapshot.cpu_core_count = 2;
  snapshot.gpu_core_count = 4;
  snapshot.memory_total_bytes = 1024;
  snapshot.memory_used_bytes = 512;
  snapshot.memory_pressure = monitor::MemoryPressureLevel::Warn;
  snapshot.memory_pressure_status.availability = monitor::MetricAvailability::Available;
  snapshot.memory_pressure_status.reason = "fixture pressure";
  snapshot.capabilities.root_mode = false;
  snapshot.capabilities.gpu_total_status.availability = monitor::MetricAvailability::Unavailable;
  snapshot.capabilities.gpu_total_status.reason = "fixture unavailable";
  snapshot.capabilities.gpu_per_process_status.availability = monitor::MetricAvailability::RequiresRoot;
  snapshot.capabilities.gpu_per_process_status.reason = "root required";
  snapshot.capabilities.thermal_status.availability = monitor::MetricAvailability::RequiresRoot;
  snapshot.capabilities.thermal_status.reason = "powermetrics requires root";
  snapshot.capabilities.ane_status.availability = monitor::MetricAvailability::RequiresRoot;
  snapshot.capabilities.ane_status.reason = "powermetrics requires root";
  snapshot.capabilities.root_process_status.availability = monitor::MetricAvailability::RequiresRoot;
  snapshot.capabilities.root_process_status.reason = "powermetrics requires root";
  snapshot.disk_io.status.availability = monitor::MetricAvailability::Waiting;
  snapshot.disk_io.status.reason = "waiting for second sample";
  snapshot.paging_io.status.availability = monitor::MetricAvailability::Available;
  snapshot.paging_io.status.reason = "fixture paging";
  snapshot.network_io.status.availability = monitor::MetricAvailability::Unavailable;
  snapshot.network_io.status.reason = "no active interface";
  snapshot.cpu_cores.push_back({0, "Performance", "P0", 12.5});
  snapshot.cpu_clusters.push_back({"Performance", 'P', 1, std::numeric_limits<double>::quiet_NaN(), 3200, 1.25, true, true});
  monitor::ProcessSnapshot process;
  process.pid = 42;
  process.parent_pid = 1;
  process.name = "quoted";
  process.command = "/bin/echo \"hi\"";
  process.user = "tester";
  process.state = 'R';
  process.cpu_percent = 5.5;
  process.memory_percent = 1.0;
  process.resident_bytes = 256;
  process.virtual_bytes = 512;
  process.gpu_active = true;
  process.core_mix = "P:80% E:20%";
  process.io = "4.0K/2.0K";
  process.power = "7";
  snapshot.processes.push_back(process);

  monitor::SnapshotJsonOptions options;
  options.view_profile = monitor::ViewProfile::Alpha;
  options.workloads = monitor::build_workloads(snapshot);
  options.memory_risk.level = monitor::MemoryRiskLevel::Warn;
  options.memory_risk.reason = "fixture risk";
  options.memory_risk.estimated_headroom_bytes = 128;
  options.memory_risk.reclaimable_bytes = 64;
  options.memory_risk.compressed_bytes = 32;
  options.memory_risk.swap_used_bytes = 16;
  options.memory_risk.swap_rate_bytes_per_sec = 8;

  const std::string json = monitor::snapshot_to_json(snapshot, options);
  assert(json.find("\"schema_version\":2") != std::string::npos);
  assert(json.find("\"view_profile\":\"alpha\"") != std::string::npos);
  assert(json.find("\"timestamp_unix_ms\":123456789") != std::string::npos);
  assert(json.find("\"soc_name\":\"Apple \\\"Test\\\"\"") != std::string::npos);
  assert(json.find("\"availability\":\"root\"") != std::string::npos);
  assert(json.find("\"reason\":\"fixture unavailable\"") != std::string::npos);
  assert(json.find("\"resident_bytes\":256") != std::string::npos);
  assert(json.find("\"command\":\"/bin/echo \\\"hi\\\"\"") != std::string::npos);
  assert(json.find("\"workloads\"") != std::string::npos);
  assert(json.find("\"memory_risk\"") != std::string::npos);
  assert(json.find("\"level\":\"warn\"") != std::string::npos);
  assert(json.find("\"estimated_headroom_bytes\":128") != std::string::npos);
  assert(json.find("nan") == std::string::npos);
  assert(json.find("inf") == std::string::npos);

  options.include_v2_models = false;
  const std::string compat_json = monitor::snapshot_to_json(snapshot, options);
  assert(compat_json.find("\"schema_version\":1") != std::string::npos);
  assert(compat_json.find("\"workloads\"") == std::string::npos);
  return 0;
}
