# Capabilities

## Non-root

- SoC identification
- CPU topology
- per-core CPU utilization
- process CPU utilization
- process memory utilization
- unified memory
- swap
- uptime
- battery
- system load
- total GPU utilization through Apple user-mode GPU statistics when available

## Root-enhanced

- thermal pressure
- ANE metrics
- GPU summary with extra counters
- process core-class mix from `powermetrics`

## Unsupported / best-effort

- per-process GPU utilization may remain unavailable on Apple Silicon even though total GPU utilization is available
- unsupported metrics are displayed as `N/A`
