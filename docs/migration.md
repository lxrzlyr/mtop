# Project Evolution Notes

This document records the current implementation direction of `mtop`.

The project has settled on a few core principles:

- macOS-first
- Apple Silicon-aware
- useful without `sudo`
- richer telemetry when root is available
- `htop`-style process interaction
- `nvtop`-style GPU charting

The repository now centers on the current C++20 / Objective-C++ / ncurses implementation.

At this stage, the work is no longer about project renaming or compatibility with an earlier prototype. The focus is on:

- improving accuracy
- refining the terminal UI
- expanding macOS-specific telemetry
- keeping release packaging and documentation clean
