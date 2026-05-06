# mtop

[English](./README.md) | [简体中文](./README.zh-CN.md)

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
- unified memory and swap usage
- process list with sorting, filtering, tree mode, search, selection, nice, and signal actions
- GPU utilization
- battery, uptime, and load average

With root:

- thermal pressure
- ANE activity when the platform exposes it
- estimated SoC subsystem power from `powermetrics`
- GPU power / frequency enrichment
- process core-class mix derived from `powermetrics`

## A Note About `SOC` Power

The `SOC` value shown in the GPU panel is **not** charger power and **not** whole-machine wall power.

It is an **estimated SoC subsystem power** value assembled from `powermetrics` fields such as CPU, GPU, and ANE energy. On macOS, this is the most practical stable source we currently have for live power telemetry in a terminal tool.

That means:

- it is useful
- it is approximate
- it reflects chip subsystem activity, not total power draw from the adapter

## Current Feature Set

- Apple Silicon-aware CPU panel with core-class grouping
- `htop`-style main process table
- incremental search and filter
- tree mode with expand / collapse
- sorting from keyboard and mouse
- GPU chart panel inspired by `nvtop`
- unified memory and swap visualization
- non-root default mode
- root-enhanced sampling path
- demo mode for UI iteration
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
- `F7 / F8` or `] / [`: renice
- `F9` or `k`: send signal
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
```

## Package And Install

Install locally:

```bash
cmake --install build --prefix /usr/local
```

Build a release package:

```bash
./scripts/release.sh
```

This will:

- configure the project
- build it
- run tests
- install into `dist/install`
- produce a `.tar.gz` package through `cpack`

There is also a Homebrew formula stub at:

```text
packaging/homebrew/mtop.rb
```

## GitHub Actions

The repository includes a GitHub Actions workflow that:

- builds on macOS
- runs tests
- installs into a staging directory
- generates a release archive with `cpack`
- uploads artifacts
- attaches packaged archives to GitHub Releases for version tags

Current workflow file:

```text
.github/workflows/build-release.yml
```

Recommended release flow:

1. Push normal commits and use the workflow artifacts to verify packaging.
2. Create a version tag such as `v1.0.1`.
3. Push the tag.
4. GitHub Actions will build and attach the package to the release.
5. The same release workflow will update the Homebrew tap and verify the formula matches the new version.

## Homebrew Tap Relationship

`mtop` and `homebrew-mtop` should remain separate repositories.

- `mtop` is the source repository
- `homebrew-mtop` is the Homebrew tap repository

That separation is intentional:

- some users want to build from source manually
- some users want to install with Homebrew
- the tap should only carry the formula, not the whole source history

This repository includes a formula generator and a tap sync workflow:

- [scripts/generate_homebrew_formula.sh](./scripts/generate_homebrew_formula.sh)
- [.github/workflows/update-homebrew-tap.yml](./.github/workflows/update-homebrew-tap.yml)

The tag-based release workflow also performs the tap update and verification step, so the release job fails if the tap does not match the published version.

To enable automatic tap updates, add this secret to the `mtop` GitHub repository:

- `HOMEBREW_TAP_SSH_KEY`

That key should be a deploy key or SSH key with write access to `lxrzlyr/homebrew-mtop`.

## Project Layout

```text
include/     public headers
src/         C++ / Objective-C++ implementation
docs/        design and platform notes
tests/       test planning and future test code
packaging/   package metadata
scripts/     helper scripts
```

## Roadmap

Likely next features:

- richer setup panel
- better process tree visuals and tagging
- improved root telemetry presentation
- clearer power breakdowns
- optional compact header / dense mode
- export / snapshot support
- better per-process GPU attribution where macOS exposes enough data

Suggestions are welcome. If you use this on real workloads, the most useful feedback is usually:

- what you were doing
- what information you expected to see
- what felt noisy, misleading, or missing

## Acknowledgements

`mtop` learns from excellent existing tools.

Huge thanks to:

- [`htop`](https://github.com/htop-dev/htop) for process interaction ideas, controls, memory presentation, and years of excellent TUI design
- [`nvtop`](https://github.com/Syllo/nvtop) for GPU charting ideas and plot behavior

Their work made this project better.

## License

`mtop` is licensed under the GNU GPL v3.0 or later.

See [LICENSE](./LICENSE).
