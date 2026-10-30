# Experiment 001 — split the benchmark out of the demo

| Field | Fill in |
| --- | --- |
| Question | Did moving the benchmark into `benchmarks/bench_main.cpp` change matching behavior or performance? |
| Hypothesis | Behavior must be identical because the moved code is unchanged. Performance should be unchanged, but a single run cannot prove it. |
| Change | Created the `fluxmatch_bench` target. Replaced `src/main.cpp` with a small deterministic demo. No matching code changed. |
| Correctness | Trades and order counts are identical: 1,000,000 orders, 1,000,000 trades, `Trade capacity: 1000000`. `fluxmatch_tests` still passes. |
| Measurement | Same workload and same timer code. Files: `001-baseline-before-phase1.txt`, `002-after-phase1-split.txt`, `002b-repeat-runs.txt`. |
| Conclusion | Semantics unchanged. Throughput numbers are not comparable across single runs. See below. |

## Results

| Run | Orders/sec | P50 cycles | P99 cycles | Max cycles |
| --- | --- | --- | --- | --- |
| Baseline (one run before split) | 2.22 M | 590 | 4,136 | 99,373,370 |
| After split (first run) | 4.19 M | 352 | 2,826 | 14,282,963 |
| After split, runs 1–5 | 4.12–5.26 M | 272–344 | 2,394–4,000 | 8,627,888–10,553,836 |

## What this shows

The refactor cannot plausibly cause a 2× speedup: no matching code changed and
the compiler settings are identical. The same binary, run five times, varies by
about 28% in throughput and 26% in p50. The p99 and max vary much more.

Likely causes, all uncontrolled right now:

- laptop CPU frequency and power state (i3-1115G4, 2 cores / 4 threads),
- background processes and antivirus scanning a freshly linked executable,
- no warm-up phase before measurement,
- TSC cycles are not wall-clock time and the TSC frequency is not calibrated.

## Consequences for Phase 1

1. Every benchmark result needs multiple runs and a reported median/variability
   (1.6).
2. The measurement loop needs a warm-up phase and must exclude bookkeeping such
   as `latencies.push_back` (1.4).
3. Metadata (build type, flags, machine, power state) belongs in every output
   (1.5).
4. Timer choice and overhead need an explicit experiment (1.8).

Until then, treat differences smaller than the observed run-to-run spread as
noise.
