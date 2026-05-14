#include "monitor/sampler.hpp"

#include "monitor/metrics.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace monitor {
namespace {

ProcessSnapshot make_process(int pid,
                             int ppid,
                             const std::string& name,
                             const std::string& command,
                             double cpu,
                             double mem,
                             std::uint64_t resident,
                             std::uint64_t runtime,
                             bool gpu,
                             const std::string& mix,
                             const std::string& io,
                             const std::string& power) {
  ProcessSnapshot process;
  process.pid = pid;
  process.parent_pid = ppid;
  process.name = name;
  process.command = command;
  process.user = "demo";
  process.cpu_percent = cpu;
  process.memory_percent = mem;
  process.resident_bytes = resident;
  process.virtual_bytes = resident * 5;
  process.total_cpu_time_ns = runtime;
  process.priority = 20;
  process.nice_value = 0;
  process.state = 'S';
  process.gpu_active = gpu;
  process.gpu_percent = gpu ? 0.0 : -1.0;
  process.core_mix = mix;
  process.io = io;
  process.power = power;
  return process;
}

MetricStatus demo_status(MetricAvailability availability, const std::string& reason) {
  MetricStatus status;
  status.availability = availability;
  status.reason = reason;
  return status;
}

std::uint64_t unix_time_ms() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

SystemSnapshot make_demo_snapshot(int tick) {
  SystemSnapshot snapshot;
  snapshot.timestamp_unix_ms = unix_time_ms();
  snapshot.sample_interval_ms = tick == 0 ? 0 : 1000;
  snapshot.macos_version = "macOS demo";
  snapshot.soc_name = "Apple M3 Pro";
  snapshot.cpu_core_count = 12;
  snapshot.gpu_core_count = 18;
  snapshot.perf_levels = {"Performance:6", "Efficiency:6"};
  for (int i = 0; i < 6; ++i) {
    snapshot.cpu_cores.push_back({i, "Performance", "P" + std::to_string(i), 0.0});
  }
  for (int i = 0; i < 6; ++i) {
    snapshot.cpu_cores.push_back({6 + i, "Efficiency", "E" + std::to_string(i), 0.0});
  }
  snapshot.cpu_clusters = {
      {"Performance", 'P', 6, 0.0, 2700, 5.4, true, true},
      {"Efficiency", 'E', 6, 0.0, 1700, 1.6, true, true},
  };

  snapshot.load_1 = 3.18;
  snapshot.load_5 = 2.42;
  snapshot.load_15 = 1.86;
  snapshot.memory_total_bytes = 36ULL << 30;
  snapshot.memory_wired_bytes = 5ULL << 30;
  snapshot.memory_active_bytes = 17ULL << 30;
  snapshot.memory_inactive_bytes = 6ULL << 30;
  snapshot.memory_speculative_bytes = 2ULL << 30;
  snapshot.memory_purgeable_bytes = 2ULL << 30;
  snapshot.memory_compressed_bytes = 5ULL << 30;
  snapshot.memory_used_bytes = snapshot.memory_wired_bytes + snapshot.memory_active_bytes +
                                snapshot.memory_compressed_bytes;
  snapshot.swap_total_bytes = 8ULL << 30;
  snapshot.swap_used_bytes = 768ULL << 20;
  snapshot.swapins_bytes_per_sec = 24ULL << 10;
  snapshot.swapouts_bytes_per_sec = 8ULL << 10;
  snapshot.uptime_seconds = 3ULL * 24ULL * 60ULL * 60ULL + 5ULL * 60ULL * 60ULL + 42ULL * 60ULL;
  snapshot.battery.available = true;
  snapshot.battery.description = "82% AC";

  snapshot.capabilities.root_mode = false;
  snapshot.capabilities.root_process_status = demo_status(MetricAvailability::RequiresRoot, "demo non-root mode");
  snapshot.capabilities.gpu_total_available = true;
  snapshot.capabilities.gpu_per_process_available = true;
  snapshot.capabilities.gpu_total_status = demo_status(MetricAvailability::Available, "demo GPU counters");
  snapshot.capabilities.gpu_per_process_status = demo_status(MetricAvailability::Available, "demo active PID list");
  snapshot.capabilities.thermal_status = demo_status(MetricAvailability::Available, "demo thermal state");
  snapshot.capabilities.ane_status = demo_status(MetricAvailability::RequiresRoot, "demo non-root mode");

  snapshot.thermal = "Nominal";
  snapshot.ane = "N/A without root";
  snapshot.gpu_utilization_percent = 38.0 + 14.0 * std::sin(static_cast<double>(tick) / 5.0);
  snapshot.gpu_summary = std::to_string(static_cast<int>(snapshot.gpu_utilization_percent)) + "% util";
  snapshot.system_power_watts = 22.0 + 5.0 * std::sin(static_cast<double>(tick) / 6.0);
  snapshot.gpu_power_watts = 4.2 + 1.5 * std::max(0.0, std::sin(static_cast<double>(tick) / 4.0));
  snapshot.gpu_frequency_mhz = 720 + static_cast<int>(120.0 * std::max(0.0, std::sin(static_cast<double>(tick) / 5.0)));
  snapshot.gpu_memory_total_bytes = snapshot.memory_total_bytes;
  snapshot.gpu_memory_used_bytes = static_cast<std::uint64_t>(
      (10.0 + 2.0 * std::max(0.0, std::sin(static_cast<double>(tick) / 6.0))) *
      1024.0 * 1024.0 * 1024.0);

  snapshot.disk_io.available = true;
  snapshot.disk_io.status = demo_status(MetricAvailability::Available, "demo block storage counters");
  snapshot.disk_io.read_bytes_per_sec = static_cast<std::uint64_t>(
      2.5 * 1024.0 * 1024.0 * std::max(0.0, 1.0 + std::sin(static_cast<double>(tick) / 7.0)));
  snapshot.disk_io.write_bytes_per_sec = static_cast<std::uint64_t>(
      1.2 * 1024.0 * 1024.0 * std::max(0.0, 1.0 + std::cos(static_cast<double>(tick) / 8.0)));
  snapshot.paging_io.available = true;
  snapshot.paging_io.status = demo_status(MetricAvailability::Available, "demo VM page activity");
  snapshot.paging_io.pageins_bytes_per_sec = static_cast<std::uint64_t>(
      128.0 * 1024.0 * std::max(0.0, 1.0 + std::sin(static_cast<double>(tick) / 11.0)));
  snapshot.paging_io.pageouts_bytes_per_sec = static_cast<std::uint64_t>(
      64.0 * 1024.0 * std::max(0.0, 1.0 + std::cos(static_cast<double>(tick) / 12.0)));
  snapshot.paging_io.swapins_bytes_per_sec = snapshot.swapins_bytes_per_sec;
  snapshot.paging_io.swapouts_bytes_per_sec = snapshot.swapouts_bytes_per_sec;
  snapshot.network_io.available = true;
  snapshot.network_io.status = demo_status(MetricAvailability::Available, "demo network counters");
  snapshot.network_io.rx_bytes_per_sec = static_cast<std::uint64_t>(
      6.0 * 1024.0 * 1024.0 * std::max(0.0, 0.5 + std::sin(static_cast<double>(tick) / 9.0)));
  snapshot.network_io.tx_bytes_per_sec = static_cast<std::uint64_t>(
      1.5 * 1024.0 * 1024.0 * std::max(0.0, 0.5 + std::cos(static_cast<double>(tick) / 10.0)));

  for (std::size_t i = 0; i < snapshot.cpu_cores.size(); ++i) {
    const double base = 10.0 + (i % 6) * 8.0;
    snapshot.cpu_cores[i].utilization_percent =
        std::clamp(base + 20.0 * std::sin((tick + static_cast<int>(i)) / 3.0), 0.0, 100.0);
  }
  for (auto& cluster : snapshot.cpu_clusters) {
    double total = 0.0;
    int count = 0;
    for (const auto& core : snapshot.cpu_cores) {
      if (core.cluster_type == cluster.name) {
        total += core.utilization_percent;
        ++count;
      }
    }
    cluster.core_count = count;
    cluster.utilization_percent = count > 0 ? total / static_cast<double>(count) : 0.0;
  }

  snapshot.memory_pressure = derive_memory_pressure(snapshot);
  snapshot.memory_pressure_status = demo_status(MetricAvailability::Available, "demo VM pressure model");
  snapshot.processes = {
      make_process(4100, 1, "ollama", "ollama serve", 4.2, 1.2, 442ULL << 20, 918060000000ULL, false, "P:42% E:58%", "820K/260K", "9"),
      make_process(4107, 4100, "ollama", "ollama runner --model llama3.1:8b --ctx-size 8192", 138.0, 27.4, 10100ULL << 20, 762450000000ULL, true, "P:88% E:12%", "32M/5.0M", "188"),
      make_process(4201, 1, "mlx-lm", "python -m mlx_lm.server --model /demo/models/mistral-7b", 64.2, 21.4, 7890ULL << 20, 918060000000ULL, true, "P:82% E:18%", "18M/3.0M", "76"),
      make_process(4318, 1, "uvicorn", "/demo/workloads/rag-api/.venv/bin/uvicorn app:api --host 127.0.0.1 --port 8000", 18.6, 9.8, 3610ULL << 20, 421170000000ULL, false, "P:74% E:26%", "7.8M/1.2M", "34"),
      make_process(4370, 1, "python", "/demo/ComfyUI/main.py --ckpt /demo/models/flux-dev.safetensors --listen", 34.2, 11.3, 4210ULL << 20, 221170000000ULL, true, "P:79% E:21%", "12M/2.5M", "42"),
      make_process(4388, 4318, "sqlite-worker", "/demo/workloads/rag-api/bin/sqlite-worker", 1.4, 0.8, 295ULL << 20, 54120000000ULL, false, "P:28% E:72%", "92K/740K", "5"),
      make_process(4482, 1, "Xcode", "/demo/apps/Xcode.app/Contents/MacOS/Xcode", 7.8, 4.6, 1690ULL << 20, 238440000000ULL, false, "P:68% E:32%", "2.5M/980K", "34"),
      make_process(4530, 1, "WindowServer", "/demo/system/WindowServer", 5.1, 2.3, 846ULL << 20, 611220000000ULL, true, "P:61% E:39%", "820K/260K", "29"),
      make_process(4584, 1, "mtop", "/demo/tools/mtop --demo", 3.6, 0.4, 154ULL << 20, 36150000000ULL, false, "P:42% E:58%", "64K/16K", "8"),
      make_process(4622, 1, "Safari", "/demo/apps/Safari.app/Contents/MacOS/Safari", 2.9, 3.2, 1180ULL << 20, 180770000000ULL, true, "P:47% E:53%", "540K/180K", "15"),
  };
  snapshot.gpu_active_pids = {4107, 4201, 4370, 4530, 4622};
  return snapshot;
}

class DemoSampler final : public Sampler {
 public:
  SystemSnapshot sample() override {
    return make_demo_snapshot(tick_++);
  }

 private:
  int tick_ = 0;
};

}  // namespace

Sampler* create_demo_sampler() {
  return new DemoSampler();
}

}  // namespace monitor
