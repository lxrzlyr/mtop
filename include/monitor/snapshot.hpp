#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace monitor {

enum class MemoryPressureLevel {
  Unknown,
  Normal,
  Warn,
  Critical,
};

struct BatterySnapshot {
  bool available = false;
  bool on_ac_power = false;
  double percent = 0.0;
  std::string description = "N/A";
};

struct CpuClusterSnapshot {
  std::string name = "Unknown";
  char label = 'C';
  int core_count = 0;
  double utilization_percent = 0.0;
  int frequency_mhz = 0;
  double power_watts = 0.0;
  bool frequency_available = false;
  bool power_available = false;
};

struct DiskIoSnapshot {
  bool available = false;
  std::uint64_t read_bytes_per_sec = 0;
  std::uint64_t write_bytes_per_sec = 0;
};

struct NetworkIoSnapshot {
  bool available = false;
  std::uint64_t rx_bytes_per_sec = 0;
  std::uint64_t tx_bytes_per_sec = 0;
};

struct CpuCoreSnapshot {
  int cpu_id = -1;
  std::string cluster_type = "Unknown";
  std::string label = "C?";
  double utilization_percent = 0.0;
};

struct ProcessSnapshot {
  int pid = -1;
  int parent_pid = -1;
  std::string name = "unknown";
  std::string command = "unknown";
  std::string user = "unknown";
  double cpu_percent = 0.0;
  double memory_percent = 0.0;
  double gpu_percent = -1.0;
  std::uint64_t resident_bytes = 0;
  std::uint64_t virtual_bytes = 0;
  std::uint64_t total_cpu_time_ns = 0;
  int priority = 0;
  int nice_value = 0;
  char state = '?';
  bool gpu_active = false;
  std::string core_mix = "-";
  std::string io = "-";
  std::string power = "-";
};

struct CapabilitySnapshot {
  bool gpu_total_available = false;
  bool gpu_per_process_available = false;
  bool thermal_available = false;
  bool ane_available = false;
  bool root_mode = false;
};

struct SystemSnapshot {
  std::string soc_name = "Apple Silicon";
  int cpu_core_count = 0;
  int gpu_core_count = 0;
  std::vector<std::string> perf_levels;

  double load_1 = 0.0;
  double load_5 = 0.0;
  double load_15 = 0.0;

  std::uint64_t memory_used_bytes = 0;
  std::uint64_t memory_total_bytes = 0;
  std::uint64_t memory_wired_bytes = 0;
  std::uint64_t memory_speculative_bytes = 0;
  std::uint64_t memory_active_bytes = 0;
  std::uint64_t memory_purgeable_bytes = 0;
  std::uint64_t memory_compressed_bytes = 0;
  std::uint64_t memory_inactive_bytes = 0;
  MemoryPressureLevel memory_pressure = MemoryPressureLevel::Unknown;
  std::uint64_t swap_used_bytes = 0;
  std::uint64_t swap_total_bytes = 0;
  std::vector<std::uint64_t> swap_history_bytes;
  std::uint64_t uptime_seconds = 0;

  BatterySnapshot battery;
  CapabilitySnapshot capabilities;

  std::string thermal = "N/A";
  std::string ane = "N/A";
  std::string gpu_summary = "N/A";
  double gpu_utilization_percent = 0.0;
  double gpu_power_watts = 0.0;
  double system_power_watts = 0.0;
  int gpu_frequency_mhz = 0;
  std::uint64_t gpu_memory_used_bytes = 0;
  std::uint64_t gpu_memory_total_bytes = 0;
  DiskIoSnapshot disk_io;
  NetworkIoSnapshot network_io;

  std::vector<CpuCoreSnapshot> cpu_cores;
  std::vector<CpuClusterSnapshot> cpu_clusters;
  std::vector<ProcessSnapshot> processes;
};

}  // namespace monitor
