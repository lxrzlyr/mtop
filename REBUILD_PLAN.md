# mtop Rebuild Plan

## Goal

Build `mtop`, a new macOS-first terminal system monitor for Apple Silicon, that:

- runs without `sudo` by default
- provides a stable `htop`-style CPU/process experience
- provides a stable `nvtop`-style GPU overview
- uses root only for optional extended hardware telemetry
- uses a cleaner architecture centered on the current C++ / Objective-C++ implementation

## Status

The repository now contains a working baseline through the original Phase 8 scope:

- new `C++20 + Objective-C++ + ncurses` codebase
- non-root Darwin CPU/process/memory/battery path
- non-root Apple GPU total-utilization path using `Metal + IORegistry`
- root-only enhancement path for thermal / ANE / process core mix
- installable `mtop` binary via CMake

The remaining work is iterative refinement rather than foundational migration.

## Confirmed Decisions

- License direction: `GPLv3` is acceptable
- Reimplementation direction: `C++ / Objective-C++`
- Primary mode: non-root
- Root mode: optional enhancement layer
- If per-process GPU usage is unavailable from public APIs, show total GPU only and mark unavailable fields as `N/A`

## Constraints

1. Information should appear once. No duplicate CPU/GPU/memory/config displays in multiple places.
2. CPU visualization should follow `htop` conventions:
   one row per core, compact text meter style.
3. GPU visualization should follow `nvtop` conventions:
   total GPU charts first, process GPU only where supported.
4. The project should keep a clean architecture without legacy layout constraints.

## Source Strategy

We will use `htop` and `nvtop` as implementation references, not just visual references.

- `htop`: Darwin CPU/process collection patterns, TUI interaction patterns
- `nvtop`: GPU-focused TUI layout, total GPU chart design, optional per-process GPU columns

Because direct code migration from GPL projects is allowed under the accepted `GPLv3` direction, we can reuse implementation ideas aggressively, but we should still isolate platform-specific adaptation cleanly.

## Runtime Modes

### Non-root mode

Default mode. Must always work.

Expected metrics:

- SoC name
- CPU core topology
- per-core CPU utilization
- process CPU utilization
- process memory usage
- unified memory usage
- swap usage
- uptime
- battery
- system load
- total GPU utilization if public user-mode APIs allow it

Unavailable metrics must show `N/A`, not crash and not silently disappear.

### Root mode

Optional enhancement mode.

Adds, where available:

- GPU frequency
- GPU power
- ANE metrics
- thermal pressure
- extra process/core-cluster attribution
- any reliable root-only counters from `powermetrics`

## Target Architecture

## Layer 1: Core data model

Common structs used by all backends:

- `SystemSnapshot`
- `CpuTopology`
- `CpuCoreSnapshot`
- `GpuSnapshot`
- `MemorySnapshot`
- `PowerSnapshot`
- `ProcessSnapshot`
- `CapabilitiesSnapshot`

This layer must not know whether data came from non-root APIs or root-only tools.

## Layer 2: Sampler backends

Separate samplers behind common interfaces.

- `DarwinCpuSampler`
- `DarwinProcessSampler`
- `DarwinMemorySampler`
- `DarwinBatterySampler`
- `DarwinGpuSampler`
- `PowermetricsExtendedSampler`

All samplers publish into the same snapshot model.

## Layer 3: Aggregation

Responsible for:

- merging multiple sampler outputs
- filling missing fields with `N/A`
- rate calculations
- process sorting
- filter and paging state
- CPU cluster attribution formatting

## Layer 4: TUI

`ncursesw`-based UI, split into view modules:

- summary bar
- CPU meters panel
- GPU panel
- process table
- footer/help line

## Language and Build System

- `C++20`
- `Objective-C++` for Apple API bridges
- `ncursesw`
- `CMake`

Reason:

- native TUI performance
- cleaner Darwin and Metal integration
- easier long-term maintainability than continuing the Python prototype

## Proposed Repository Layout

```text
.
├── CMakeLists.txt
├── README.md
├── REBUILD_PLAN.md
├── LICENSE
├── docs/
│   ├── api-notes.md
│   ├── darwin-sampling.md
│   └── ui-guidelines.md
├── include/
│   ├── monitor/
│   │   ├── snapshot.hpp
│   │   ├── capabilities.hpp
│   │   ├── sampler.hpp
│   │   └── ui_state.hpp
├── src/
│   ├── main.cpp
│   ├── app/
│   ├── model/
│   ├── sampler/
│   ├── tui/
│   └── platform/darwin/
├── tests/
└── old/
```

## UI Design Direction

### Summary area

A small compact panel or a single compressed strip.

Keep only:

- SoC
- CPU core count / layout
- GPU core count
- uptime
- battery
- ANE
- thermal

Do not repeat memory or sorting information elsewhere if already shown.

### CPU area

Primary focus area.

- `S0..`
- `P0..`
- `E0..`

Each row:

- core label
- utilization meter
- optional frequency

The visual style should match `htop` meter density rather than large boxed widgets.

### GPU area

Only total GPU first.

- utilization chart
- frequency
- power
- optional process GPU summary if supported

The visual style should follow `nvtop`: chart-first, not per-core bars.

### Process area

Columns:

- PID
- command
- CPU%
- core-class mix
- MEM%
- GPU% or `N/A`
- IO
- power/energy if available

Interaction:

- sort
- filter
- paging
- selection

## Phase Plan

## Phase 0: Repository reset

Scope:

- freeze old code in `old/`
- create new top-level docs and layout
- adopt new license

Deliverables:

- new root README
- rebuild plan
- new license file
- new project skeleton

Acceptance:

- root directory no longer depends on a legacy package layout

## Phase 1: Core model and build skeleton

Scope:

- set up `CMake`
- create snapshot model
- create sampler interfaces
- add empty TUI shell

Deliverables:

- compilable app that starts and exits
- basic logging/debug snapshot dump mode

Acceptance:

- project builds cleanly with documented toolchain steps

## Phase 2: Non-root CPU/process path

Scope:

- implement Darwin CPU utilization sampler
- implement process enumeration sampler
- implement unified memory / swap / uptime / battery samplers
- remove dependency on `powermetrics` for default CPU and process operation

Deliverables:

- non-root app with live CPU meters and process list

Acceptance:

- no `sudo` required
- per-core CPU meters update correctly
- process table updates correctly

## Phase 3: CPU topology and cluster labeling

Scope:

- robust `S/P/E` topology mapping
- stable core numbering
- process core-class usage formatting

Deliverables:

- correct `S0-Sn`, `P0-Pn`, `E0-En`
- cluster-aware process column

Acceptance:

- no obviously incorrect “always 98%” cores caused by math bugs
- cluster labeling matches machine topology

## Phase 4: Non-root GPU path

Scope:

- inspect `nvtop` Apple path
- implement user-mode GPU total utilization if possible via Apple public APIs
- display `N/A` where public APIs are insufficient

Deliverables:

- GPU panel that runs without root

Acceptance:

- app still works without root even if only partial GPU data is available

## Phase 5: Root-only enhancement sampler

Scope:

- isolate `powermetrics` into a plugin-style extended sampler
- add thermal / ANE / GPU power / extra counters

Deliverables:

- same UI, richer fields when root is available

Acceptance:

- switching between root and non-root changes capabilities, not architecture

## Phase 6: TUI redesign

Scope:

- replace box-heavy Python prototype style with compact `htop/nvtop` layout
- implement selection, paging, filtering, sort states
- compress duplicated information

Deliverables:

- usable ncurses UI

Acceptance:

- no duplicated CPU/GPU/memory status blocks
- CPU panel and process panel readable on standard terminal sizes

## Phase 7: Performance and cleanup

Scope:

- profile sampler cost
- reduce refresh overhead
- remove any remaining prototype behavior

Deliverables:

- stable release candidate

Acceptance:

- acceptable idle overhead
- no dependence on old Python code

## Phase 8: Packaging and migration

Scope:

- release build flow
- install instructions
- project evolution notes

Deliverables:

- release artifacts
- user documentation

Acceptance:

- clean install and run path for new users

## Immediate Next Step

Start with **Phase 0 + Phase 1** only:

- add new `GPLv3` license
- add `CMakeLists.txt`
- create `include/`, `src/`, `docs/`, `tests/`
- stub new executable

This is the correct cut point because it finalizes the new project boundary before more platform code is written.
