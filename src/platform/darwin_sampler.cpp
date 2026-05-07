#include "monitor/sampler.hpp"
#include "monitor/apple_gpu.hpp"
#include "monitor/metrics.hpp"
#include "monitor/root_metrics_parser.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <ifaddrs.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/vm_statistics.h>
#include <net/if.h>
#include <fcntl.h>
#include <pwd.h>
#include <spawn.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace monitor {
namespace {

template <typename T>
std::optional<T> sysctl_scalar(const char* name) {
  T value{};
  size_t size = sizeof(value);
  if (sysctlbyname(name, &value, &size, nullptr, 0) != 0) {
    return std::nullopt;
  }
  return value;
}

std::string sysctl_string(const char* name, const std::string& fallback = "") {
  size_t size = 0;
  if (sysctlbyname(name, nullptr, &size, nullptr, 0) != 0 || size == 0) {
    return fallback;
  }
  std::string value(size, '\0');
  if (sysctlbyname(name, value.data(), &size, nullptr, 0) != 0) {
    return fallback;
  }
  if (!value.empty() && value.back() == '\0') {
    value.pop_back();
  }
  return value;
}

std::uint64_t current_uptime_seconds() {
  timeval boot_time{};
  size_t size = sizeof(boot_time);
  int mib[2] = {CTL_KERN, KERN_BOOTTIME};
  if (sysctl(mib, 2, &boot_time, &size, nullptr, 0) != 0) {
    return 0;
  }
  const std::time_t now = std::time(nullptr);
  return now > boot_time.tv_sec ? static_cast<std::uint64_t>(now - boot_time.tv_sec) : 0;
}

std::string run_command_capture(const std::vector<std::string>& args, bool discard_stderr) {
  if (args.empty()) {
    return "";
  }

  int pipe_fds[2] = {-1, -1};
  if (pipe(pipe_fds) != 0) {
    return "";
  }

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
  posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
  posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);

  int devnull_fd = -1;
  if (discard_stderr) {
    devnull_fd = open("/dev/null", O_WRONLY);
    if (devnull_fd >= 0) {
      posix_spawn_file_actions_adddup2(&actions, devnull_fd, STDERR_FILENO);
      posix_spawn_file_actions_addclose(&actions, devnull_fd);
    }
  }

  std::vector<char*> argv;
  argv.reserve(args.size() + 1);
  for (const auto& arg : args) {
    argv.push_back(const_cast<char*>(arg.c_str()));
  }
  argv.push_back(nullptr);

  static char path_env[] = "PATH=/usr/bin:/bin:/usr/sbin:/sbin";
  static char lang_env[] = "LANG=C";
  static char lc_all_env[] = "LC_ALL=C";
  char* const safe_env[] = {path_env, lang_env, lc_all_env, nullptr};

  pid_t pid = 0;
  const int spawn_rc = posix_spawn(&pid, args[0].c_str(), &actions, nullptr, argv.data(), safe_env);
  posix_spawn_file_actions_destroy(&actions);
  if (devnull_fd >= 0) close(devnull_fd);
  close(pipe_fds[1]);
  if (spawn_rc != 0) {
    close(pipe_fds[0]);
    return "";
  }

  std::string output;
  std::array<char, 4096> buffer{};
  ssize_t bytes = 0;
  while ((bytes = read(pipe_fds[0], buffer.data(), buffer.size())) > 0) {
    output.append(buffer.data(), static_cast<std::size_t>(bytes));
  }
  close(pipe_fds[0]);
  int status = 0;
  waitpid(pid, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    return "";
  }
  return output;
}

bool run_command_quiet(const std::vector<std::string>& args) {
  if (args.empty()) {
    return false;
  }

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  const int devnull_fd = open("/dev/null", O_WRONLY);
  if (devnull_fd >= 0) {
    posix_spawn_file_actions_adddup2(&actions, devnull_fd, STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, devnull_fd, STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, devnull_fd);
  }

  std::vector<char*> argv;
  argv.reserve(args.size() + 1);
  for (const auto& arg : args) {
    argv.push_back(const_cast<char*>(arg.c_str()));
  }
  argv.push_back(nullptr);

  static char path_env[] = "PATH=/usr/bin:/bin:/usr/sbin:/sbin";
  static char lang_env[] = "LANG=C";
  static char lc_all_env[] = "LC_ALL=C";
  char* const safe_env[] = {path_env, lang_env, lc_all_env, nullptr};

  pid_t pid = 0;
  const int spawn_rc = posix_spawn(&pid, args[0].c_str(), &actions, nullptr, argv.data(), safe_env);
  posix_spawn_file_actions_destroy(&actions);
  if (devnull_fd >= 0) close(devnull_fd);
  if (spawn_rc != 0) {
    return false;
  }
  int status = 0;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int gpu_core_count() {
  const std::string output = run_command_capture(
      {"/usr/sbin/system_profiler", "-json", "SPDisplaysDataType"},
      true);
  if (output.empty()) {
    return 0;
  }
  const std::string token = "\"sppci_cores\" : \"";
  const std::size_t pos = output.find(token);
  if (pos == std::string::npos) {
    return 0;
  }
  const std::size_t start = pos + token.size();
  const std::size_t end = output.find('"', start);
  if (end == std::string::npos) {
    return 0;
  }
  return std::atoi(output.substr(start, end - start).c_str());
}

struct PerfLevelInfo {
  std::string name;
  int logicalcpu = 0;
};

std::vector<PerfLevelInfo> perf_levels() {
  std::vector<PerfLevelInfo> levels;
  for (int index = 0;; ++index) {
    const std::string logical_key = "hw.perflevel" + std::to_string(index) + ".logicalcpu";
    const auto logical = sysctl_scalar<int>(logical_key.c_str());
    if (!logical.has_value()) {
      break;
    }
    const std::string name_key = "hw.perflevel" + std::to_string(index) + ".name";
    levels.push_back({sysctl_string(name_key.c_str(), "Unknown"), *logical});
  }
  std::sort(levels.begin(), levels.end(), [](const PerfLevelInfo& lhs, const PerfLevelInfo& rhs) {
    auto order = [](const std::string& name) {
      if (name == "Super") return 0;
      if (name == "Performance") return 1;
      if (name == "Efficiency") return 2;
      return 3;
    };
    return order(lhs.name) < order(rhs.name);
  });
  return levels;
}

std::vector<CpuCoreSnapshot> build_core_topology() {
  std::vector<CpuCoreSnapshot> cores;
  int global_cpu = 0;
  for (const auto& level : perf_levels()) {
    const char prefix = level.name == "Super" ? 'S' : level.name == "Performance" ? 'P' : level.name == "Efficiency" ? 'E' : 'C';
    for (int index = 0; index < level.logicalcpu; ++index) {
      CpuCoreSnapshot core{};
      core.cpu_id = global_cpu++;
      core.cluster_type = level.name;
      core.label = std::string(1, prefix) + std::to_string(index);
      cores.push_back(core);
    }
  }
  return cores;
}

double percent_from_delta(std::uint64_t old_value, std::uint64_t new_value, std::uint64_t total_delta) {
  if (new_value < old_value || total_delta == 0) {
    return 0.0;
  }
  const std::uint64_t delta = new_value - old_value;
  return static_cast<double>(delta) * 100.0 / static_cast<double>(total_delta);
}

std::string battery_description() {
  CFTypeRef info = IOPSCopyPowerSourcesInfo();
  if (!info) {
    return "N/A";
  }
  CFArrayRef list = IOPSCopyPowerSourcesList(info);
  if (!list) {
    CFRelease(info);
    return "N/A";
  }
  std::string result = "N/A";
  if (CFArrayGetCount(list) > 0) {
    CFDictionaryRef dict = IOPSGetPowerSourceDescription(info, CFArrayGetValueAtIndex(list, 0));
    if (dict) {
      CFNumberRef percent_num = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, CFSTR(kIOPSCurrentCapacityKey)));
      CFStringRef state = static_cast<CFStringRef>(CFDictionaryGetValue(dict, CFSTR(kIOPSPowerSourceStateKey)));
      int percent = 0;
      bool on_ac = false;
      if (percent_num) {
        CFNumberGetValue(percent_num, kCFNumberIntType, &percent);
      }
      if (state) {
        on_ac = CFStringCompare(state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo;
      }
      result = std::to_string(percent) + "% " + (on_ac ? "AC" : "BAT");
    }
  }
  CFRelease(list);
  CFRelease(info);
  return result;
}

std::string username_for_uid(uid_t uid) {
  if (const passwd* entry = getpwuid(uid)) {
    return entry->pw_name;
  }
  return std::to_string(uid);
}

char process_state_char(uint32_t status) {
  switch (status) {
    case SIDL: return 'I';
    case SRUN: return 'R';
    case SSLEEP: return 'S';
    case SSTOP: return 'T';
    case SZOMB: return 'Z';
    default: return '?';
  }
}

std::string process_command(pid_t pid, const proc_bsdinfo& bsd_info) {
  char path_buffer[PROC_PIDPATHINFO_MAXSIZE] = {};
  if (proc_pidpath(pid, path_buffer, sizeof(path_buffer)) > 0) {
    return path_buffer;
  }
  if (bsd_info.pbi_name[0] != '\0') {
    return bsd_info.pbi_name;
  }
  if (bsd_info.pbi_comm[0] != '\0') {
    return bsd_info.pbi_comm;
  }
  return "unknown";
}

std::string compact_count(double value) {
  static const char* units[] = {"", "K", "M", "G", "T"};
  int unit = 0;
  while (std::fabs(value) >= 1000.0 && unit < 4) {
    value /= 1000.0;
    ++unit;
  }
  char buffer[16];
  if (std::fabs(value) >= 100.0 || unit == 0) {
    std::snprintf(buffer, sizeof(buffer), "%.0f%s", value, units[unit]);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%.1f%s", value, units[unit]);
  }
  return buffer;
}

std::string compact_binary_bytes(std::uint64_t value) {
  static const char* units[] = {"B", "Ki", "Mi", "Gi", "Ti"};
  double current = static_cast<double>(value);
  int unit = 0;
  while (current >= 1024.0 && unit < 4) {
    current /= 1024.0;
    ++unit;
  }
  char buffer[16];
  if (unit == 0) {
    std::snprintf(buffer, sizeof(buffer), "%llu%s",
                  static_cast<unsigned long long>(value),
                  units[unit]);
  } else if (current >= 10.0) {
    std::snprintf(buffer, sizeof(buffer), "%.0f%s", current, units[unit]);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%.1f%s", current, units[unit]);
  }
  return buffer;
}

struct RootMetrics {
  bool sampled = false;
  bool available = false;
  bool ane_available = false;
  std::string thermal = "N/A";
  std::string ane = "N/A";
  double cpu_power_watts = 0.0;
  double gpu_power_watts = 0.0;
  double total_power_watts = 0.0;
  int gpu_frequency_mhz = 0;
   std::map<std::string, int> cluster_frequency_mhz;
   std::map<std::string, double> cluster_power_watts;
  std::map<int, std::string> process_core_mix;
  std::map<int, std::string> process_io;
  std::map<int, std::string> process_power;
};

struct HostNetworkCounters {
  std::uint64_t rx_bytes = 0;
  std::uint64_t tx_bytes = 0;
  bool available = false;
};

std::optional<HostNetworkCounters> sample_host_network_counters() {
  ifaddrs* interfaces = nullptr;
  if (getifaddrs(&interfaces) != 0 || interfaces == nullptr) {
    return std::nullopt;
  }

  HostNetworkCounters counters;
  for (ifaddrs* current = interfaces; current != nullptr; current = current->ifa_next) {
    if (!current->ifa_name || !current->ifa_data) {
      continue;
    }
    if ((current->ifa_flags & IFF_UP) == 0 || (current->ifa_flags & IFF_LOOPBACK) != 0) {
      continue;
    }
    const auto* data = reinterpret_cast<const if_data*>(current->ifa_data);
    counters.rx_bytes += data->ifi_ibytes;
    counters.tx_bytes += data->ifi_obytes;
    counters.available = true;
  }

  freeifaddrs(interfaces);
  return counters.available ? std::optional<HostNetworkCounters>(counters) : std::nullopt;
}

class DarwinSampler final : public Sampler {
 public:
  DarwinSampler() {
    snapshot_.soc_name = sysctl_string("machdep.cpu.brand_string", "Apple Silicon");
    snapshot_.cpu_core_count = sysctl_scalar<int>("hw.logicalcpu").value_or(0);
    snapshot_.gpu_core_count = gpu_core_count();
    snapshot_.capabilities.root_mode = geteuid() == 0;
    snapshot_.cpu_cores = build_core_topology();
    rebuild_cpu_clusters();
    for (const auto& level : perf_levels()) {
      snapshot_.perf_levels.push_back(level.name + ":" + std::to_string(level.logicalcpu));
    }
    if (snapshot_.capabilities.root_mode) {
      root_metrics_thread_ = std::thread([this] { root_metrics_loop(); });
    }
  }

  ~DarwinSampler() override {
    stop_root_metrics_.store(true);
    if (root_metrics_thread_.joinable()) {
      root_metrics_thread_.join();
    }
    if (previous_cpu_info_ != nullptr) {
      vm_deallocate(mach_task_self(),
                    reinterpret_cast<vm_address_t>(previous_cpu_info_),
                    previous_cpu_info_count_ * sizeof(integer_t));
    }
  }

  SystemSnapshot sample() override {
    sample_load();
    sample_memory();
    sample_system_io();
    sample_battery();
    sample_cpu();
    sample_root_metrics();
    sample_gpu();
    sample_processes();
    snapshot_.uptime_seconds = current_uptime_seconds();
    return snapshot_;
  }

 private:
  void sample_load() {
    double loads[3] = {0.0, 0.0, 0.0};
    if (getloadavg(loads, 3) == 3) {
      snapshot_.load_1 = loads[0];
      snapshot_.load_5 = loads[1];
      snapshot_.load_15 = loads[2];
    }
  }

  void sample_memory() {
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vm_stat{};
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm_stat), &count) == KERN_SUCCESS) {
      const auto page_size = sysctl_scalar<int>("hw.pagesize").value_or(16384);
      const std::uint64_t total = sysctl_scalar<std::uint64_t>("hw.memsize").value_or(0);
      const std::uint64_t free_pages = vm_stat.free_count + vm_stat.inactive_count + vm_stat.speculative_count;
      const std::uint64_t free_bytes = free_pages * static_cast<std::uint64_t>(page_size);
      snapshot_.memory_total_bytes = total;
      snapshot_.memory_used_bytes = total > free_bytes ? total - free_bytes : 0;
      snapshot_.memory_wired_bytes = vm_stat.wire_count * static_cast<std::uint64_t>(page_size);
      snapshot_.memory_speculative_bytes = vm_stat.speculative_count * static_cast<std::uint64_t>(page_size);
      snapshot_.memory_active_bytes = vm_stat.active_count * static_cast<std::uint64_t>(page_size);
      snapshot_.memory_purgeable_bytes = vm_stat.purgeable_count * static_cast<std::uint64_t>(page_size);
      snapshot_.memory_compressed_bytes = vm_stat.compressor_page_count * static_cast<std::uint64_t>(page_size);
      snapshot_.memory_inactive_bytes = vm_stat.inactive_count * static_cast<std::uint64_t>(page_size);
    }
    xsw_usage swap{};
    size_t size = sizeof(swap);
    if (sysctlbyname("vm.swapusage", &swap, &size, nullptr, 0) == 0) {
      snapshot_.swap_total_bytes = swap.xsu_total;
      snapshot_.swap_used_bytes = swap.xsu_used;
    }
    snapshot_.memory_pressure = derive_memory_pressure(snapshot_);
    snapshot_.swap_history_bytes.push_back(snapshot_.swap_used_bytes);
    constexpr std::size_t kSwapHistoryLimit = 60;
    if (snapshot_.swap_history_bytes.size() > kSwapHistoryLimit) {
      snapshot_.swap_history_bytes.erase(
          snapshot_.swap_history_bytes.begin(),
          snapshot_.swap_history_bytes.begin() + (snapshot_.swap_history_bytes.size() - kSwapHistoryLimit));
    }
  }

  void sample_system_io() {
    const auto now = std::chrono::steady_clock::now();
    const double elapsed_seconds = last_io_sample_time_.time_since_epoch().count() == 0
                                       ? 0.0
                                       : std::chrono::duration<double>(now - last_io_sample_time_).count();
    last_io_sample_time_ = now;

    snapshot_.disk_io.available = false;
    snapshot_.disk_io.read_bytes_per_sec = 0;
    snapshot_.disk_io.write_bytes_per_sec = 0;
    snapshot_.network_io.available = false;
    snapshot_.network_io.rx_bytes_per_sec = 0;
    snapshot_.network_io.tx_bytes_per_sec = 0;

    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vm_stat{};
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                          reinterpret_cast<host_info64_t>(&vm_stat), &count) == KERN_SUCCESS) {
      const std::uint64_t page_size = static_cast<std::uint64_t>(sysctl_scalar<int>("hw.pagesize").value_or(16384));
      const std::uint64_t cumulative_read_bytes = vm_stat.pageins * page_size;
      const std::uint64_t cumulative_write_bytes = vm_stat.pageouts * page_size;
      if (elapsed_seconds > 0.0 && previous_disk_read_bytes_.has_value() && previous_disk_write_bytes_.has_value() &&
          cumulative_read_bytes >= *previous_disk_read_bytes_ && cumulative_write_bytes >= *previous_disk_write_bytes_) {
        snapshot_.disk_io.available = true;
        snapshot_.disk_io.read_bytes_per_sec = static_cast<std::uint64_t>(
            std::llround(static_cast<double>(cumulative_read_bytes - *previous_disk_read_bytes_) / elapsed_seconds));
        snapshot_.disk_io.write_bytes_per_sec = static_cast<std::uint64_t>(
            std::llround(static_cast<double>(cumulative_write_bytes - *previous_disk_write_bytes_) / elapsed_seconds));
      }
      previous_disk_read_bytes_ = cumulative_read_bytes;
      previous_disk_write_bytes_ = cumulative_write_bytes;
    }

    const std::optional<HostNetworkCounters> network_counters = sample_host_network_counters();
    if (network_counters.has_value()) {
      if (elapsed_seconds > 0.0 && previous_network_rx_bytes_.has_value() && previous_network_tx_bytes_.has_value() &&
          network_counters->rx_bytes >= *previous_network_rx_bytes_ &&
          network_counters->tx_bytes >= *previous_network_tx_bytes_) {
        snapshot_.network_io.available = true;
        snapshot_.network_io.rx_bytes_per_sec = static_cast<std::uint64_t>(
            std::llround(static_cast<double>(network_counters->rx_bytes - *previous_network_rx_bytes_) / elapsed_seconds));
        snapshot_.network_io.tx_bytes_per_sec = static_cast<std::uint64_t>(
            std::llround(static_cast<double>(network_counters->tx_bytes - *previous_network_tx_bytes_) / elapsed_seconds));
      }
      previous_network_rx_bytes_ = network_counters->rx_bytes;
      previous_network_tx_bytes_ = network_counters->tx_bytes;
    }
  }

  void sample_battery() {
    snapshot_.battery.available = battery_description() != "N/A";
    snapshot_.battery.description = battery_description();
  }

  void sample_gpu() {
    const AppleGpuProbeResult probe = sample_apple_gpu();
    snapshot_.capabilities.gpu_total_available = probe.available;
    snapshot_.capabilities.gpu_per_process_available = !probe.active_pids.empty();
    snapshot_.gpu_utilization_percent = probe.utilization_percent;
    snapshot_.gpu_memory_used_bytes = probe.used_memory_bytes;
    snapshot_.gpu_memory_total_bytes = probe.total_memory_bytes;
    gpu_active_pid_set_.clear();
    for (int pid : probe.active_pids) {
      gpu_active_pid_set_.insert(pid);
    }
    if (!probe.available) {
      snapshot_.gpu_summary = "N/A without root";
      return;
    }
    char buffer[128];
    if (snapshot_.capabilities.root_mode && cached_root_metrics_.available) {
      std::snprintf(buffer, sizeof(buffer),
                    "%.0f%% @ %d MHz | GPU %.2fW | SoC %.2fW",
                    probe.utilization_percent,
                    cached_root_metrics_.gpu_frequency_mhz,
                    cached_root_metrics_.gpu_power_watts,
                    cached_root_metrics_.total_power_watts);
    } else {
      std::snprintf(buffer, sizeof(buffer),
                    "%.0f%% util | Mem %s/%s",
                    probe.utilization_percent,
                    compact_binary_bytes(probe.used_memory_bytes).c_str(),
                    compact_binary_bytes(probe.total_memory_bytes).c_str());
    }
    snapshot_.gpu_summary = buffer;
    snapshot_.gpu_power_watts = cached_root_metrics_.gpu_power_watts;
    snapshot_.system_power_watts = cached_root_metrics_.total_power_watts;
  }

  void sample_cpu() {
    natural_t cpu_count = 0;
    processor_info_array_t cpu_info = nullptr;
    mach_msg_type_number_t cpu_info_count = 0;
    kern_return_t kr = host_processor_info(mach_host_self(),
                                           PROCESSOR_CPU_LOAD_INFO,
                                           &cpu_count,
                                           &cpu_info,
                                           &cpu_info_count);
    if (kr != KERN_SUCCESS || cpu_info == nullptr) {
      return;
    }

    const auto* current = reinterpret_cast<processor_cpu_load_info_data_t*>(cpu_info);
    const auto* previous = reinterpret_cast<processor_cpu_load_info_data_t*>(previous_cpu_info_);
    const bool have_previous = previous_cpu_info_ != nullptr && previous_cpu_count_ == cpu_count;

    for (natural_t cpu = 0; cpu < cpu_count && cpu < snapshot_.cpu_cores.size(); ++cpu) {
      double percent = 0.0;
      if (have_previous) {
        std::uint64_t user_delta = current[cpu].cpu_ticks[CPU_STATE_USER] - previous[cpu].cpu_ticks[CPU_STATE_USER];
        std::uint64_t system_delta = current[cpu].cpu_ticks[CPU_STATE_SYSTEM] - previous[cpu].cpu_ticks[CPU_STATE_SYSTEM];
        std::uint64_t nice_delta = current[cpu].cpu_ticks[CPU_STATE_NICE] - previous[cpu].cpu_ticks[CPU_STATE_NICE];
        std::uint64_t idle_delta = current[cpu].cpu_ticks[CPU_STATE_IDLE] - previous[cpu].cpu_ticks[CPU_STATE_IDLE];
        std::uint64_t total_delta = user_delta + system_delta + nice_delta + idle_delta;
        if (total_delta > 0) {
          percent = static_cast<double>(user_delta + system_delta + nice_delta) * 100.0 / static_cast<double>(total_delta);
        }
      }
      snapshot_.cpu_cores[cpu].utilization_percent = percent;
    }
    refresh_cpu_clusters();

    if (previous_cpu_info_ != nullptr) {
      vm_deallocate(mach_task_self(),
                    reinterpret_cast<vm_address_t>(previous_cpu_info_),
                    previous_cpu_info_count_ * sizeof(integer_t));
    }
    previous_cpu_info_ = cpu_info;
    previous_cpu_info_count_ = cpu_info_count;
    previous_cpu_count_ = cpu_count;
  }

  void rebuild_cpu_clusters() {
    snapshot_.cpu_clusters.clear();
    std::map<std::string, int> core_counts;
    for (const auto& core : snapshot_.cpu_cores) {
      core_counts[core.cluster_type] += 1;
    }
    for (const auto& level : perf_levels()) {
      CpuClusterSnapshot cluster{};
      cluster.name = level.name;
      cluster.label = level.name == "Super" ? 'S' : level.name == "Performance" ? 'P' : level.name == "Efficiency" ? 'E' : 'C';
      cluster.core_count = core_counts[level.name];
      snapshot_.cpu_clusters.push_back(cluster);
    }
  }

  void refresh_cpu_clusters() {
    if (snapshot_.cpu_clusters.empty()) {
      rebuild_cpu_clusters();
    }

    for (auto& cluster : snapshot_.cpu_clusters) {
      double total_util = 0.0;
      int counted = 0;
      for (const auto& core : snapshot_.cpu_cores) {
        if (core.cluster_type != cluster.name) {
          continue;
        }
        total_util += core.utilization_percent;
        ++counted;
      }
      cluster.core_count = counted;
      cluster.utilization_percent = counted > 0 ? total_util / static_cast<double>(counted) : 0.0;
      cluster.frequency_available = false;
      cluster.frequency_mhz = 0;
      cluster.power_available = false;
      cluster.power_watts = 0.0;
    }

    if (!snapshot_.capabilities.root_mode) {
      return;
    }

    for (auto& cluster : snapshot_.cpu_clusters) {
      const auto freq_it = cached_root_metrics_.cluster_frequency_mhz.find(cluster.name);
      if (freq_it != cached_root_metrics_.cluster_frequency_mhz.end() && freq_it->second > 0) {
        cluster.frequency_available = true;
        cluster.frequency_mhz = freq_it->second;
      }
      const auto power_it = cached_root_metrics_.cluster_power_watts.find(cluster.name);
      if (power_it != cached_root_metrics_.cluster_power_watts.end() && power_it->second > 0.0) {
        cluster.power_available = true;
        cluster.power_watts = power_it->second;
      }
    }
  }

  void sample_processes() {
    const char* missing_root_detail = !snapshot_.capabilities.root_mode
                                          ? "root"
                                          : (!cached_root_metrics_.sampled ? "wait" : "n/a");
    const auto now = std::chrono::steady_clock::now();
    const double elapsed_seconds =
        last_process_sample_time_.time_since_epoch().count() == 0
            ? 0.0
            : std::chrono::duration<double>(now - last_process_sample_time_).count();
    last_process_sample_time_ = now;

    std::vector<pid_t> pids(4096);
    const int bytes = proc_listpids(PROC_ALL_PIDS, 0, pids.data(), static_cast<int>(pids.size() * sizeof(pid_t)));
    if (bytes <= 0) {
      snapshot_.processes.clear();
      return;
    }

    const int count = bytes / static_cast<int>(sizeof(pid_t));
    std::vector<ProcessSnapshot> processes;
    processes.reserve(count);
    std::set<pid_t> live_pids;

    for (int i = 0; i < count; ++i) {
      const pid_t pid = pids[i];
      if (pid <= 0) {
        continue;
      }

      proc_taskallinfo task_all_info{};
      const int info_bytes = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &task_all_info, PROC_PIDTASKALLINFO_SIZE);
      if (info_bytes != PROC_PIDTASKALLINFO_SIZE) {
        continue;
      }
      live_pids.insert(pid);

      const proc_bsdinfo& bsd_info = task_all_info.pbsd;
      const proc_taskinfo& task_info = task_all_info.ptinfo;

      const std::uint64_t total_time = task_info.pti_total_user + task_info.pti_total_system;
      double cpu_percent = 0.0;
      auto previous_iter = previous_process_times_.find(pid);
      if (elapsed_seconds > 0.0 && previous_iter != previous_process_times_.end() && total_time >= previous_iter->second) {
        const std::uint64_t delta_ns = total_time - previous_iter->second;
        cpu_percent = static_cast<double>(delta_ns) / elapsed_seconds / 1.0e7;
      }
      previous_process_times_[pid] = total_time;

      ProcessSnapshot process{};
      process.pid = pid;
      process.parent_pid = static_cast<int>(bsd_info.pbi_ppid);
      process.name = bsd_info.pbi_name[0] != '\0' ? bsd_info.pbi_name : bsd_info.pbi_comm;
      process.command = process_command(pid, bsd_info);
      process.user = username_for_uid(bsd_info.pbi_uid);
      process.cpu_percent = cpu_percent;
      process.gpu_active = gpu_active_pid_set_.find(pid) != gpu_active_pid_set_.end();
      process.gpu_percent = process.gpu_active ? 0.0 : -1.0;
      process.resident_bytes = task_info.pti_resident_size;
      process.virtual_bytes = task_info.pti_virtual_size;
      process.total_cpu_time_ns = total_time;
      process.priority = task_info.pti_priority;
      process.nice_value = bsd_info.pbi_nice;
      process.state = process_state_char(bsd_info.pbi_status);
      process.memory_percent = snapshot_.memory_total_bytes > 0
                                 ? static_cast<double>(task_info.pti_resident_size) * 100.0 / static_cast<double>(snapshot_.memory_total_bytes)
                                 : 0.0;
      auto core_mix_it = cached_root_metrics_.process_core_mix.find(pid);
      process.core_mix = core_mix_it != cached_root_metrics_.process_core_mix.end() ? core_mix_it->second : missing_root_detail;
      auto io_it = cached_root_metrics_.process_io.find(pid);
      process.io = io_it != cached_root_metrics_.process_io.end() ? io_it->second : missing_root_detail;
      auto power_it = cached_root_metrics_.process_power.find(pid);
      process.power = power_it != cached_root_metrics_.process_power.end() ? power_it->second : missing_root_detail;
      processes.push_back(process);
    }

    for (auto it = previous_process_times_.begin(); it != previous_process_times_.end();) {
      if (live_pids.find(it->first) == live_pids.end()) {
        it = previous_process_times_.erase(it);
      } else {
        ++it;
      }
    }

    std::sort(processes.begin(), processes.end(), [](const ProcessSnapshot& lhs, const ProcessSnapshot& rhs) {
      return lhs.cpu_percent > rhs.cpu_percent;
    });
    snapshot_.processes = std::move(processes);
  }

  void sample_root_metrics() {
    if (!snapshot_.capabilities.root_mode) {
      snapshot_.thermal = "N/A";
      snapshot_.ane = "N/A without root";
      snapshot_.capabilities.thermal_available = false;
      snapshot_.capabilities.ane_available = false;
      return;
    }

    {
      std::lock_guard<std::mutex> lock(root_metrics_mutex_);
      cached_root_metrics_ = shared_root_metrics_;
    }
    apply_root_metrics();
  }

  void apply_root_metrics() {
    if (cached_root_metrics_.available) {
      snapshot_.thermal = cached_root_metrics_.thermal;
      snapshot_.ane = cached_root_metrics_.ane_available ? cached_root_metrics_.ane : "N/A";
      snapshot_.capabilities.thermal_available = true;
      snapshot_.capabilities.ane_available = cached_root_metrics_.ane_available;
    } else {
      snapshot_.thermal = "N/A";
      snapshot_.ane = "N/A";
      snapshot_.capabilities.thermal_available = false;
      snapshot_.capabilities.ane_available = false;
    }
  }

  RootMetrics collect_root_metrics() {
    RootMetrics metrics;
    metrics.sampled = true;
    char plist_template[] = "/tmp/mtop-powermetrics-XXXXXX";
    int fd = mkstemp(plist_template);
    if (fd >= 0) {
      close(fd);
      const bool rc = run_command_quiet({
          "/usr/bin/powermetrics",
          "--samplers", "cpu_power,gpu_power,thermal,ane_power",
          "-n", "1",
          "-i", "1000",
          "-f", "plist",
          "-o", plist_template,
      });
      if (rc) {
        std::ifstream input(plist_template, std::ios::binary);
        std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (!bytes.empty()) {
          std::size_t segment_start = 0;
          for (std::size_t i = 0; i <= bytes.size(); ++i) {
            if (i == bytes.size() || bytes[i] == '\0') {
              if (i > segment_start) {
                CFDataRef data = CFDataCreate(kCFAllocatorDefault,
                                              reinterpret_cast<const UInt8*>(bytes.data() + segment_start),
                                              static_cast<CFIndex>(i - segment_start));
                if (data) {
                  CFErrorRef error = nullptr;
                  CFPropertyListRef plist = CFPropertyListCreateWithData(kCFAllocatorDefault, data, kCFPropertyListImmutable, nullptr, &error);
                  if (plist && CFGetTypeID(plist) == CFDictionaryGetTypeID()) {
                    CFDictionaryRef dict = static_cast<CFDictionaryRef>(plist);
                    CFStringRef thermal = static_cast<CFStringRef>(CFDictionaryGetValue(dict, CFSTR("thermal_pressure")));
                    if (thermal) {
                      char buffer[128];
                      if (CFStringGetCString(thermal, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
                        metrics.thermal = buffer;
                      }
                    }
                    CFDictionaryRef processor = static_cast<CFDictionaryRef>(CFDictionaryGetValue(dict, CFSTR("processor")));
                    if (processor) {
                      CFNumberRef cpu_energy = static_cast<CFNumberRef>(CFDictionaryGetValue(processor, CFSTR("cpu_energy")));
                      CFNumberRef ane_energy = static_cast<CFNumberRef>(CFDictionaryGetValue(processor, CFSTR("ane_energy")));
                      CFNumberRef gpu_energy = static_cast<CFNumberRef>(CFDictionaryGetValue(processor, CFSTR("gpu_energy")));
                      double cpu_mw = 0.0;
                      double ane_mw = 0.0;
                      double gpu_mw = 0.0;
                      if (cpu_energy) {
                        CFNumberGetValue(cpu_energy, kCFNumberDoubleType, &cpu_mw);
                      }
                      if (ane_energy) {
                        CFNumberGetValue(ane_energy, kCFNumberDoubleType, &ane_mw);
                      }
                      if (gpu_energy) {
                        CFNumberGetValue(gpu_energy, kCFNumberDoubleType, &gpu_mw);
                      }
                      metrics.cpu_power_watts = cpu_mw / 1000.0;
                      metrics.gpu_power_watts = gpu_mw / 1000.0;
                      metrics.total_power_watts = (cpu_mw + gpu_mw) / 1000.0;
                      if (ane_energy) {
                        char buffer[128];
                        metrics.ane_available = true;
                        if (ane_mw <= 1.0) {
                          metrics.ane = "idle";
                        } else {
                          std::snprintf(buffer, sizeof(buffer), "%.0f%% @ %.1fW",
                                        std::min(100.0, ane_mw / 8000.0 * 100.0),
                                        ane_mw / 1000.0);
                          metrics.ane = buffer;
                        }
                        metrics.total_power_watts += ane_mw / 1000.0;
                      }
                    }
                    CFDictionaryRef gpu = static_cast<CFDictionaryRef>(CFDictionaryGetValue(dict, CFSTR("gpu")));
                    if (gpu) {
                      CFNumberRef freq = static_cast<CFNumberRef>(CFDictionaryGetValue(gpu, CFSTR("freq_hz")));
                      double freq_value = 0.0;
                      if (freq) {
                        CFNumberGetValue(freq, kCFNumberDoubleType, &freq_value);
                        metrics.gpu_frequency_mhz = freq_value > 100000.0 ? static_cast<int>(freq_value / 1.0e6) : static_cast<int>(freq_value);
                      }
                    }
                    metrics.available = true;
                    CFRelease(plist);
                    CFRelease(data);
                    if (error) {
                      CFRelease(error);
                    }
                    break;
                  }
                  if (plist) {
                    CFRelease(plist);
                  }
                  if (error) {
                    CFRelease(error);
                  }
                  CFRelease(data);
                }
              }
              segment_start = i + 1;
            }
          }
        }
      }
      std::remove(plist_template);
    }

    const bool has_super = [&] {
      for (const auto& level : perf_levels()) {
        if (level.name == "Super") return true;
      }
      return false;
    }();
    const std::string amp_text = run_command_capture({
        "/usr/bin/powermetrics",
        "--samplers", "tasks,cpu_power,disk",
        "--show-process-amp",
        "--show-process-io",
        "--show-process-energy",
        "--show-process-ipc",
        "-n", "1",
        "-i", "1000",
    }, true);
    AmpData amp = parse_amp_data(amp_text, has_super);
    if (amp.core_mix.empty() && amp.io.empty() && amp.power.empty()) {
      AmpData fallback = parse_amp_data(run_command_capture({
          "/usr/bin/powermetrics",
          "--samplers", "tasks,cpu_power",
          "--show-process-amp",
          "--show-process-energy",
          "-n", "1",
          "-i", "1000",
      }, true), has_super);
      if (amp.core_mix.empty()) amp.core_mix = std::move(fallback.core_mix);
      if (amp.power.empty()) amp.power = std::move(fallback.power);
    }
    metrics.process_core_mix = std::move(amp.core_mix);
    metrics.process_io = std::move(amp.io);
    metrics.process_power = std::move(amp.power);
    return metrics;
  }

  void root_metrics_loop() {
    while (!stop_root_metrics_.load()) {
      RootMetrics metrics = collect_root_metrics();
      {
        std::lock_guard<std::mutex> lock(root_metrics_mutex_);
        shared_root_metrics_ = std::move(metrics);
      }
      for (int i = 0; i < 10 && !stop_root_metrics_.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
    }
  }

  SystemSnapshot snapshot_{};
  processor_info_array_t previous_cpu_info_ = nullptr;
  mach_msg_type_number_t previous_cpu_info_count_ = 0;
  natural_t previous_cpu_count_ = 0;
  std::map<pid_t, std::uint64_t> previous_process_times_{};
  std::chrono::steady_clock::time_point last_process_sample_time_{};
  std::chrono::steady_clock::time_point last_io_sample_time_{};
  std::optional<std::uint64_t> previous_disk_read_bytes_{};
  std::optional<std::uint64_t> previous_disk_write_bytes_{};
  std::optional<std::uint64_t> previous_network_rx_bytes_{};
  std::optional<std::uint64_t> previous_network_tx_bytes_{};
  std::set<int> gpu_active_pid_set_{};
  RootMetrics cached_root_metrics_{};
  RootMetrics shared_root_metrics_{};
  std::mutex root_metrics_mutex_{};
  std::atomic<bool> stop_root_metrics_{false};
  std::thread root_metrics_thread_{};
};

}  // namespace

Sampler* create_darwin_sampler() {
  return new DarwinSampler();
}

}  // namespace monitor
