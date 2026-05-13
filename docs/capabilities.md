# Capabilities

mtop separates stable macOS counters from root-enhanced and best-effort signals. Unavailable best-effort metrics should carry a short reason in the UI instead of being silently treated as zero.

## Stable Non-root

- SoC identification through `sysctl`
- CPU topology through `hw.perflevel*`
- per-core CPU utilization through Mach host CPU tick counters
- process CPU, memory, state, parent PID, command, and user through `libproc`
- unified memory, compressed memory, purgeable memory, VM page activity, and swap totals through Mach VM counters and `vm.swapusage`
- derived memory pressure from compression, reclaimable memory, and swap usage
- block storage throughput through IOKit `IOBlockStorageDriver` statistics when exposed by the platform
- network throughput through active non-loopback interface byte counters
- uptime, load average, and battery state

## Root-enhanced

- thermal pressure from `powermetrics`
- ANE activity when `powermetrics` exposes ANE energy
- estimated CPU / GPU / ANE / SoC subsystem power from `powermetrics`
- GPU frequency from `powermetrics`
- process core-class mix, process IO, and Energy Impact from `powermetrics`

Root-enhanced samples run in a background thread. A failed sample keeps the previous successful sample where possible and marks it `stale`.

## Best-effort / Hardware-dependent

- total GPU utilization and memory through Apple GPU user-mode / IORegistry counters
- GPU active PID list
- ANE fields, depending on hardware and macOS powermetrics output
- block storage counters on systems that do not expose `IOBlockStorageDriver` statistics

Common labels:

- `root`: metric requires root
- `wait`: first async sample has not arrived
- `stale`: last successful sample is being reused after a failure
- `parse`: sampler output could not be parsed
- `denied`: permission failure
- `n/a`: metric is unavailable or unsupported in the current environment
