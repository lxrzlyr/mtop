# Darwin Sampling

This document records the source and semantics of the main macOS counters used by mtop.

## CPU

- Per-core utilization uses Mach `host_processor_info(PROCESSOR_CPU_LOAD_INFO)` deltas.
- Core topology uses `sysctl` keys such as `hw.logicalcpu` and `hw.perflevel*`.
- Cluster summaries are derived from per-core utilization grouped by perf level.

## Memory And Paging

- Memory totals and VM counters use `host_statistics64(HOST_VM_INFO64)`.
- Swap total and used bytes use `sysctlbyname("vm.swapusage")`.
- Memory pressure is derived locally from active+wired memory, reclaimable memory, compressed memory, and swap usage.
- Paging throughput is VM pageins/pageouts from Mach VM counters. It is not block device throughput.
- Swap in/out rate is sampled from VM `swapins` / `swapouts` deltas when available.

## Disk And Network

- Disk throughput uses IOKit `IOBlockStorageDriver` `Statistics` counters:
  - `Bytes (Read)`
  - `Bytes (Write)`
- If block storage counters are missing, Disk is unavailable instead of reported as zero.
- Network throughput uses `getifaddrs` interface byte counters across active non-loopback interfaces.

## GPU

- Total GPU utilization, GPU memory, and active PID data are best-effort Apple GPU counters from the platform GPU sampler.
- These counters are hardware and macOS-version dependent. Missing fields should be represented as unavailable with a reason.

## Root-enhanced Powermetrics

Root mode starts a background sampler for `powermetrics`.

- Thermal, CPU/GPU/ANE power, and GPU frequency are collected from plist powermetrics output.
- Process core mix, process IO, and Energy Impact are collected from text powermetrics task output.
- Each powermetrics call has a timeout.
- If a sample fails after a previous success, mtop keeps the last successful sample and marks it `stale`.
- If no sample has succeeded yet, root-only metrics show `wait`, `denied`, `parse`, or `n/a` depending on the failure.

The root sampler is best-effort and must not block the curses UI loop.
