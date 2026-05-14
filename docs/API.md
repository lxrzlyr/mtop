# mtop API And Architecture

This document describes the repository layout, core data model, and internal C++ APIs used by mtop.

Current stable release: `v2.0.0`.

`v2.0.0` keeps the non-curses JSON snapshot path and adds JSON v2 workload/risk fields plus JSONL sessions. Alpha and beta profiles share one sampler, one `SystemSnapshot`, and one output path.

## Repository Layout

- `src/main.cpp`: ncurses application shell, CLI parsing, layout, view rendering, input dispatch, and demo snapshot shaping.
- `src/platform/darwin_sampler.cpp`: Darwin/macOS system sampler implementation.
- `src/platform/apple_gpu.mm`: Objective-C++ Apple GPU best-effort sampler.
- `src/root_metrics_parser.cpp`: parser for `powermetrics` task text output.
- `src/metrics.cpp`: derived metric helpers such as memory pressure.
- `src/snapshot_json.cpp`: dependency-light JSON serializer for `SystemSnapshot`.
- `src/process_sort.cpp`: pure process sort comparator for UI and tests.
- `src/ui_support.cpp`: UI-independent formatting and view helper functions.
- `src/input_logic.cpp`: pure sort/input state transitions.
- `src/input_parser.cpp`: terminal input normalization around curses.
- `include/monitor/*.hpp`: public internal module interfaces used by the executable and tests.
- `tests/*.cpp`: small executable tests registered by CTest.
- `tools/input_diag.cpp`: standalone terminal input diagnostic utility.

## Runtime Flow

1. `main()` loads config, applies CLI overrides, creates a `Sampler`, and either enters snapshot mode or starts the curses UI.
2. `monitor::create_darwin_sampler()` returns a Darwin sampler instance for normal mode.
3. `monitor::create_demo_sampler()` returns a synthetic sampler for `--demo` and does not call Darwin sampling APIs.
4. Each UI tick calls `Sampler::sample()` and receives a `SystemSnapshot`.
5. The UI renders CPU, GPU, memory, process, System I/O, and detail views from the snapshot.
6. Snapshot mode calls `Sampler::sample()`, serializes the returned `SystemSnapshot`, prints JSON, and exits or repeats according to `--count` / `--loop`.
7. In root mode, the Darwin sampler runs `powermetrics` on a background thread and merges the latest root-enhanced sample into the main snapshot.

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

`SystemSnapshot` also carries:

- `timestamp_unix_ms`: wall-clock timestamp for structured output.
- `sample_interval_ms`: elapsed sample interval or requested snapshot interval.
- `macos_version`: host OS version string for JSON consumers.

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

Sampler* create_darwin_sampler(int root_sample_ms = 1000);
Sampler* create_demo_sampler();

}  // namespace monitor
```

`sample()` must not block on slow root telemetry. Root-only data is collected asynchronously and copied into the returned snapshot when available.

`create_demo_sampler()` is the screenshot and preview data source. It returns fixed demo hardware and process data, including `Apple M3 Pro`, 12 CPU cores, 18 GPU cores, 36 GB unified memory, `demo` users, and `/demo/...` paths.

## Snapshot JSON API

```cpp
struct SnapshotJsonOptions {
  ViewProfile view_profile = ViewProfile::Alpha;
  bool include_v2_models = true;
  std::vector<WorkloadSnapshot> workloads;
  MemoryRiskSnapshot memory_risk;
};

std::string json_escape(const std::string& value);
std::string snapshot_to_json(const SystemSnapshot& snapshot);
std::string snapshot_to_json(const SystemSnapshot& snapshot, const SnapshotJsonOptions& options);
```

The serializer is intentionally small and internal. It emits one complete JSON object per snapshot. Schema v2 preserves the 1.5 fields and adds `view_profile`, `workloads`, and `memory_risk`; `include_v2_models=false` is reserved for compatibility tests.

Top-level fields:

- `schema_version`
- `view_profile`
- `timestamp_unix_ms`
- `sample_interval_ms`
- `host`
- `capabilities`
- `cpu`
- `memory`
- `gpu`
- `ane`
- `io`
- `processes`
- `workloads`
- `memory_risk`

Best-effort metrics include status objects with `availability`, `reason`, and `age_ms` so unavailable values are explainable to scripts.

## Snapshot Session API

```cpp
struct SnapshotSessionInfo {
  std::string session_id;
  std::string label;
  std::uint64_t started_unix_ms = 0;
};

std::string make_session_id(std::uint64_t started_unix_ms);
std::string session_start_json(const SnapshotSessionInfo& session);
std::string session_snapshot_json(const std::string& snapshot_json);
std::string session_end_json(const SnapshotSessionInfo& session, std::uint64_t ended_unix_ms);
```

Session mode is JSONL: one `session_start`, one `snapshot` wrapper per sample, and one `session_end`. Snapshot mode never initializes curses; output-file failures are reported before sampling starts.

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
  int root_sample_ms = 1000;
  int snapshot_interval_ms = 1000;
  bool show_cached_memory = false;
  ViewMode default_view = ViewMode::Overview;
  SortMode sort = SortMode::Cpu;
  int sort_direction = -1;
  ViewProfile view_profile = ViewProfile::Alpha;
};

AppConfig load_config(const std::string& explicit_path = "");
std::string config_default_path();
bool persist_view_profile(const std::string& explicit_path, ViewProfile profile, std::string* error);
```

Config loading is intentionally small and line-oriented. CLI options in `main.cpp` override loaded config. Invalid values clamp or fall back. `view_profile` defaults to alpha, accepts alpha/beta, and is persisted by `-view` / `--view` before curses starts.

## View Profile API

```cpp
enum class ViewProfile {
  Alpha,
  Beta,
};

const char* view_profile_name(ViewProfile profile);
const char* view_profile_label(ViewProfile profile);
std::optional<ViewProfile> parse_view_profile(std::string_view value);
```

The profile model is the first 2.0 boundary between shared sampling/core data and UI product shape. Alpha and beta share the same sampler and `SystemSnapshot`.

## Input And UI Helper APIs

```cpp
enum class SortMode { Pid, Cpu, Mem, Time, Name, GpuActive, Io, Power };
int default_sort_direction(SortMode mode);
SortMode cycle_sort(SortMode mode);
SortState apply_header_sort_click(SortState current, SortMode clicked_mode);
```

```cpp
bool process_sort_less(const ProcessSnapshot& lhs,
                       const ProcessSnapshot& rhs,
                       SortMode mode,
                       int direction);
```

`GpuActive`, `Io`, and `Power` are root-enhanced quality-of-life sort modes. Missing root values sort after available numeric values in the default descending direction.

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
