# mtop API And Architecture

This document describes the repository layout, core data model, and internal C++ APIs used by mtop.

## Repository Layout

- `src/main.cpp`: ncurses application shell, CLI parsing, layout, view rendering, input dispatch, and demo snapshot shaping.
- `src/platform/darwin_sampler.cpp`: Darwin/macOS system sampler implementation.
- `src/platform/apple_gpu.mm`: Objective-C++ Apple GPU best-effort sampler.
- `src/root_metrics_parser.cpp`: parser for `powermetrics` task text output.
- `src/metrics.cpp`: derived metric helpers such as memory pressure.
- `src/ui_support.cpp`: UI-independent formatting and view helper functions.
- `src/input_logic.cpp`: pure sort/input state transitions.
- `src/input_parser.cpp`: terminal input normalization around curses.
- `include/monitor/*.hpp`: public internal module interfaces used by the executable and tests.
- `tests/*.cpp`: small executable tests registered by CTest.
- `tools/input_diag.cpp`: standalone terminal input diagnostic utility.

## Runtime Flow

1. `main()` loads config, applies CLI overrides, creates a `Sampler`, and starts the curses UI.
2. `monitor::create_darwin_sampler()` returns a Darwin sampler instance for normal mode.
3. `monitor::create_demo_sampler()` returns a synthetic sampler for `--demo` and does not call Darwin sampling APIs.
4. Each UI tick calls `Sampler::sample()` and receives a `SystemSnapshot`.
5. The UI renders CPU, GPU, memory, process, System I/O, and detail views from the snapshot.
6. In root mode, the Darwin sampler runs `powermetrics` on a background thread and merges the latest root-enhanced sample into the main snapshot.

Snapshot mode is not implemented in 1.4.

## Core Data Model

`include/monitor/snapshot.hpp` is the central contract between sampling and UI.

- `SystemSnapshot`: full sample for one UI frame.
- `CpuCoreSnapshot`: per logical CPU utilization and cluster label.
- `CpuClusterSnapshot`: grouped utilization plus optional frequency/power.
- `ProcessSnapshot`: process table row data.
- `DiskIoSnapshot`: IOKit block storage throughput.
- `PagingIoSnapshot`: Mach VM pagein/pageout and swapin/swapout rates.
- `NetworkIoSnapshot`: active non-loopback interface throughput.
- `CapabilitySnapshot`: availability booleans plus detailed metric status.
- `MetricStatus`: availability enum, short reason, and optional sample age.

`MetricAvailability` values are intentionally UI-friendly:

- `Available`
- `RequiresRoot`
- `UnsupportedHardware`
- `UnsupportedOS`
- `PermissionDenied`
- `Waiting`
- `Stale`
- `ParseFailed`
- `Unavailable`

## Sampler API

```cpp
namespace monitor {

class Sampler {
 public:
  virtual ~Sampler() = default;
  virtual SystemSnapshot sample() = 0;
};

Sampler* create_darwin_sampler();
Sampler* create_demo_sampler();

}  // namespace monitor
```

`sample()` must not block on slow root telemetry. Root-only data is collected asynchronously and copied into the returned snapshot when available.

`create_demo_sampler()` is the screenshot and preview data source. It returns fixed demo hardware and process data, including `Apple M3 Pro`, 12 CPU cores, 18 GPU cores, 36 GB unified memory, `demo` users, and `/demo/...` paths.

## Metrics API

```cpp
MemoryPressureLevel derive_memory_pressure(const SystemSnapshot& snapshot);
const char* memory_pressure_label(MemoryPressureLevel level);
```

Memory pressure is derived from total memory, wired+active memory, reclaimable memory, compressed memory, and swap usage.

## Root Metrics Parser API

```cpp
struct AmpData {
  std::map<int, std::string> core_mix;
  std::map<int, std::string> io;
  std::map<int, std::string> power;
};

AmpData parse_amp_data(const std::string& text, bool has_super);
```

The parser accepts `powermetrics` task text output and extracts process core-class mix, process IO, and Energy Impact. It is tolerant of missing columns and merged numeric fields.

## Apple GPU API

```cpp
struct AppleGpuProbeResult {
  bool available = false;
  double utilization_percent = 0.0;
  std::uint64_t used_memory_bytes = 0;
  std::uint64_t total_memory_bytes = 0;
  std::vector<int> active_pids;
};

AppleGpuProbeResult sample_apple_gpu();
```

This sampler is best-effort. Callers must use `CapabilitySnapshot` / `MetricStatus` to explain missing GPU data.

## Config API

```cpp
struct AppConfig {
  std::string theme = "apple";
  int refresh_ms = 1000;
  int process_limit = 12;
  bool demo_mode = false;
};

AppConfig load_config(const std::string& explicit_path = "");
std::string config_default_path();
```

Config loading is intentionally small and line-oriented. CLI options in `main.cpp` override loaded config.

## Input And UI Helper APIs

```cpp
enum class SortMode { Pid, Cpu, Mem, Time, Name };
int default_sort_direction(SortMode mode);
SortMode cycle_sort(SortMode mode);
SortState apply_header_sort_click(SortState current, SortMode clicked_mode);
```

```cpp
enum class ViewMode { Overview, SystemIo, GpuActive };
const char* view_mode_label(ViewMode mode);
ViewMode cycle_view_mode(ViewMode mode, int delta);
const char* metric_availability_label(MetricAvailability availability);
std::string metric_status_label(const MetricStatus& status);
std::string format_throughput_rate(bool available, std::uint64_t bytes_per_sec);
```

These helpers are kept outside curses where practical so they can be tested without rendering the TUI.

## macOS Sampling Semantics

- CPU: Mach `host_processor_info(PROCESSOR_CPU_LOAD_INFO)`.
- Memory and paging: Mach `host_statistics64(HOST_VM_INFO64)`.
- Swap total/used: `sysctlbyname("vm.swapusage")`.
- Disk: IOKit `IOBlockStorageDriver` `Bytes (Read)` / `Bytes (Write)`.
- Network: `getifaddrs` interface byte counters.
- Process table: `libproc`.
- Root enhancements: `powermetrics` plist and task text output.
- GPU: best-effort Apple GPU platform counters.

Unavailable metrics should be represented with an explicit `MetricStatus` reason rather than as a misleading zero.
