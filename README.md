# mtop

[English](./README.md) | [简体中文](./docs/README.zh-CN.md)

`mtop` is a terminal monitor built for Apple Silicon Macs.

It aims to feel familiar to people who love `htop` and `nvtop`, but it is designed around the realities of macOS and M-series SoCs: unified memory, asymmetric cores, GPU activity that matters, and power data that is useful when you are tuning local AI workloads.

## Quickstart

Install directly from the Homebrew tap:

```bash
brew install lxrzlyr/mtop/mtop
```

Or tap once, then use the short name:

```bash
brew tap lxrzlyr/mtop
brew install mtop
```

Run it:

```bash
mtop
```

Preview mode:

```bash
mtop --demo
```

## Screenshot

![mtop demo](./docs/assets/mtop-demo.png)

## Why This Exists

There still is not a truly satisfying, Mac-native terminal monitor for Apple Silicon.

For a long time, watching resource usage on a personal Mac did not feel urgent. That changed quickly in the AI era. More people are running local inference, compiling heavier stacks, profiling models, stress-testing unified memory, and trying to understand where their machines are spending time and power.

`mtop` exists because every Mac user should have a tool that feels precise, readable, and honest about what the machine is doing.

We want to provide something practical for every user:

- a monitor that understands Apple Silicon instead of pretending it is a generic Unix box
- a tool that is useful without `sudo`
- a tool that gets deeper when you do choose to run as root
- a TUI that makes performance, memory pressure, GPU activity, and process behavior easy to read at a glance

## What mtop Is

`mtop` is:

- a macOS-first terminal monitor for Apple Silicon
- optimized for M-series SoCs
- inspired by `htop` for process interaction
- inspired by `nvtop` for GPU charting
- built from scratch in C++20 and ncurses

## What It Shows

Without root:

- SoC model and Apple Silicon core topology
- per-core CPU utilization grouped by core class
- cluster-level CPU utilization summaries for `S / P / E`
- unified memory, swap usage, and derived memory pressure
- process list with sorting, filtering, tree mode, search, selection, nice, and signal actions
- GPU utilization with richer panel summaries
- system-level block storage, VM paging, and network throughput in a secondary `System I/O` view
- GPU-active filtered process view
- battery, uptime, and load average

With root:

- thermal pressure
- ANE activity when the platform exposes it
- estimated SoC subsystem power from `powermetrics`
- GPU power / frequency enrichment
- process core-class mix derived from `powermetrics`
- process IO and energy-impact columns derived from `powermetrics`

Secondary views and detail interactions:

- `System I/O`: host-level block storage, VM paging, and network throughput
- `GPU Active`: focused process view filtered by GPU-active state
- `Process Detail`: popup with full command, pid/ppid, runtime, memory, mix, io, power, and GPU state

## Process Extension Columns

In root mode, the process table can show three extra columns sourced from `powermetrics`:

- `MIX`: estimated core-class mix for the sampled window, such as `S:12% P:88%`
- `IO`: per-process `Bytes Read / Bytes Written` activity during the sample window, such as `3.9K/7.8K`
- `PWR`: per-process Energy Impact from `powermetrics`

These columns are best-effort and sample-window based, so they should be read with a few caveats:

- `0B/0B` or `0` means the metric was available and the sampled value was zero
- `n/a` means root sampling succeeded, but `powermetrics` did not provide a usable value for that process in that sample
- `wait` means root sampling has not delivered its first background sample yet
- `stale` means the latest root sample failed, so mtop is showing the last successful root sample
- `root` means the column requires `sudo ./build/mtop`

## System I/O Semantics

The `System I/O` view separates three host-level signals:

- `Disk`: IOKit block storage bytes read / written across available storage drivers
- `Paging`: VM pageins / pageouts from Mach VM counters; this is memory paging activity, not disk throughput
- `Net`: interface byte counters across active non-loopback network interfaces

If a counter is unavailable, mtop shows `n/a` instead of treating the value as zero.

## A Note About `SOC` Power

The `SOC` value shown in the GPU panel is **not** charger power and **not** whole-machine wall power.

It is an **estimated SoC subsystem power** value assembled from `powermetrics` fields such as CPU, GPU, and ANE energy. On macOS, this is the most practical stable source we currently have for live power telemetry in a terminal tool.

That means:

- it is useful
- it is approximate
- it reflects chip subsystem activity, not total power draw from the adapter

## Current Feature Set

- Apple Silicon-aware CPU panel with core-class grouping
- compact CPU cluster summary line
- `htop`-style main process table
- incremental search and filter
- tree mode with expand / collapse
- sorting from keyboard and mouse
- GPU chart panel inspired by `nvtop`
- unified memory, swap, and pressure visualization
- secondary `System I/O` and `GPU Active` views
- unavailable reasons for best-effort GPU, ANE, thermal, root process, disk, paging, and network metrics
- process detail popup
- non-root default mode
- root-enhanced sampling path
- demo mode for UI iteration
- standalone input diagnostic utility for terminal compatibility debugging
- release packaging with `cpack`

## Build

```bash
cmake -S . -B build
cmake --build build -j4
```

## Run

Default mode:

```bash
./build/mtop
```

Root-enhanced mode:

```bash
sudo ./build/mtop
```

Security note:

- `sudo` mode is optional and should only be used on systems you trust
- do not run `mtop` as root on machines that are already compromised, jailbroken, tampered with, or otherwise in a questionable security state
- root mode runs privileged telemetry collection and therefore inherits the security posture of the host system

UI preview mode:

```bash
./build/mtop --demo
```

Helper preview script:

```bash
./scripts/preview_demo.sh
```

## Controls

Main interaction:

- `Up / Down / PgUp / PgDn / Home / End`: move selection
- `F3` or `/`: incremental search
- `F4` or `\`: incremental filter
- `F5` or `t`: tree mode
- `+ / - / *`: expand / collapse / toggle tree nodes
- `F6` or `>` or `.`: sort menu
- `N / P / M / T / A / I`: PID / CPU / MEM / TIME / NAME / invert
- `F7 / F8`, `Tab / Shift-Tab`, or `{ / }`: previous / next secondary view
- `] / [`: renice
- `F9` or `k`: send signal
- `d`: process detail popup
- `F10` or `q`: quit
- mouse: click process headers to sort, click rows to select, click function bar buttons

## Configuration

Optional config path:

```text
~/.config/mtop/config
```

Example file:

```text
.config.example
```

Supported keys:

```text
theme=apple
refresh_ms=1000
process_limit=12
demo_mode=false
```

CLI overrides:

```bash
./build/mtop --demo
./build/mtop --refresh-ms 500
./build/mtop --theme mono
./build/mtop --config /path/to/config
./build/mtop --debug-input
./build/mtop --debug-input --debug-log /tmp/mtop-input.log
```

## Install And Package

Install locally:

```bash
cmake --install build --prefix /usr/local
```

Build a local release archive:

```bash
./scripts/release.sh
```

If you are installing with Homebrew, use:

```bash
brew install lxrzlyr/mtop/mtop
```

## Acknowledgements

`mtop` learns from excellent existing tools.

Huge thanks to:

- [`htop`](https://github.com/htop-dev/htop) for process interaction ideas, controls, memory presentation, and years of excellent TUI design
- [`nvtop`](https://github.com/Syllo/nvtop) for GPU charting ideas and plot behavior

Their work made this project better.

## License

`mtop` is licensed under the GNU GPL v3.0 or later.

See [LICENSE](./LICENSE).
