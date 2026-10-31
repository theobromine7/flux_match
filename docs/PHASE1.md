# Phase 1 — Benchmarking fundamentals

Phase 1.1 is done. The benchmark now has its own executable and the demo is
demo-only. The rest of Phase 1 (workloads, statistics, metadata, repeat runs,
JSON output, timer experiment) is still ahead.

## What changed

| File | Change |
| --- | --- |
| `benchmarks/bench_main.cpp` | The benchmark, moved verbatim from `src/main.cpp` |
| `src/main.cpp` | New small deterministic demo |
| `CMakeLists.txt` | Added `fluxmatch_bench`, linking `fluxmatch_core` |
| `docs/BUILD.md` | Documents both executables |
| `docs/experiments/001-*` | Baseline, post-split outputs, repeat runs, experiment record |

No matching or order-book code changed. That was deliberate.

## Why a pure move

The old benchmark has real flaws: no warm-up, each sample includes
`latencies.push_back`, TSC counts cycles rather than time, and the workload is
dominated by price-level erase/insert. If those were fixed during the move, a
later comparison could not distinguish "the refactor changed something" from
"the measurement changed." Move first, keep behavior fixed, then change one
thing per experiment.

## The demo

`src/main.cpp` now does this:

1. rests a sell at 100 (qty 2) and a sell at 101 (qty 3);
2. submits a buy at 101 (qty 4), which fills 2 at 100 and 2 at 101;
3. leaves 1 unit of the 101 sell resting;
4. submits a bid at 99 and shows it becomes the best buy.

Deterministic output:

```text
FluxMatch demo
Trades:
  buy=3 sell=1 price=100 qty=2
  buy=3 sell=2 price=101 qty=2
Best sell: 2
Best buy: 4 (expected 4)
```

This is also a readable specification of price/time priority: the 100 level
trades before the 101 level, and the resting order's price is the trade price.

## The benchmark, before and after

| Run | Orders/sec | P50 | P99 | Max |
| --- | --- | --- | --- | --- |
| Baseline (before split) | 2.22 M | 590 | 4,136 | 99,373,370 |
| After split, first run | 4.19 M | 352 | 2,826 | 14,282,963 |
| After split, five repeats | 4.12–5.26 M | 272–344 | 2,394–4,000 | 8.6–10.6 M |

**Do not conclude the refactor made it faster.** The moved code is identical,
and the same binary varies by ~28% between runs on this laptop. The baseline run
was probably disturbed by a cold executable scan, power state, or background
load. The experiment record (`docs/experiments/001-bench-split.md`) states this
as a limitation, and it motivates the rest of Phase 1.

This is the central Phase 1 lesson: a single number is not a measurement.

## Answers to the three questions

1. **Why keep the flaws during the move?** So correctness and performance
   changes stay separable. A refactor should be a no-op you can prove, not a
   bundle of improvements.
2. **Why link `fluxmatch_core` instead of recompiling `src/*.cpp`?** One
   definition of the engine, one set of compile flags, faster builds, and the
   test, demo, and benchmark exercise the same object code. Recompiling per
   executable is how "works in the test but not in the product" happens.
3. **A 20% p50 difference after the refactor — evidence of a regression?** No.
   First check run-to-run spread, warm-up, frequency/power state, background
   load, and whether the binary was just written to disk. Only after the noise
   floor is known can a 20% change mean anything.

## How to run

```powershell
cmake --build --preset windows-release
.\build\windows-release\Release\fluxmatch_demo.exe
.\build\windows-release\Release\fluxmatch_bench.exe
```

On Linux, the CI job builds both; run the binaries from `build/` after the same
commands.

## Phase 1.2 — workload generators (done)

The benchmark now takes `--workload`, `--commands`, and `--seed`:

```powershell
.\build\windows-release\Release\fluxmatch_bench.exe --workload mixed --commands 200000 --seed 42
```

Five generators live in `benchmarks/workloads.{hpp,cpp}`:

| Workload | Shape |
| --- | --- |
| `resting` | Non-crossing limits; bids below mid, asks above |
| `one_level_fill` | Resting sell + matching buy, quantity 1 |
| `multi_level_sweep` | 10 sells on consecutive prices, then a buy of 10 |
| `cancel` | Resting submits with periodic cancels of active orders |
| `mixed` | 50% resting, 20% cancels, 30% market orders |

`std::mt19937_64` plus integer arithmetic (no `uniform_int_distribution`, whose
implementation is not portable) makes the stream deterministic. The benchmark
prints a `Stream hash`; the same seed gave `7193c5f7311ff72f` twice across
separate processes.

### Why functions plus a dispatch table

Three options were considered:

- **Single switch:** one function grows with every workload and generators
  cannot be tested in isolation.
- **Polymorphic `IWorkload`:** justified when each workload needs runtime state
  or its own parameters; unnecessary virtual dispatch for five stateless
  generators.
- **Free functions + dispatch table (chosen):** each generator is independently
  testable, adding one is a function plus a table entry, and the dispatch is
  trivial.

It should be revisited when workloads gain parameters beyond count and seed.
That is the point where the common signature stops fitting and an interface or
a config struct becomes justified.

### Bug found

`Side.hpp` and `OrderType.hpp` had no `#pragma once`. They worked only because
`Order.hpp` included each once; including them directly from `workloads.hpp`
produced enum redefinitions. Fixed.

### Results (200,000 commands, seed 42, single run)

| Workload | Commands/sec | P50 | P99 | Trades |
| --- | --- | --- | --- | --- |
| resting | 5.10 M | 364 | 970 | 0 |
| one_level_fill | 7.97 M | 218 | 696 | 100,000 |
| multi_level_sweep | 4.58 M | 226 | 2,524 | 181,810 |
| cancel | 4.35 M | 386 | 2,102 | 0 |
| mixed | 3.37 M | 480 | 2,278 | 81,559 |

Treat these as shape, not truth: still one run each, no warm-up.

## Phase 1.3 — throughput vs latency (done)

The benchmark now has three modes:

```powershell
.\build\windows-release\Release\fluxmatch_bench.exe --workload mixed --commands 200000 --seed 42 --mode both
```

- `--mode throughput` times the whole stream once. Almost no measurement overhead.
- `--mode latency` times each command and reports percentiles.
- `--mode both` runs both, each against a **fresh engine**.

Latency is reported twice:

| Scope | Timer starts | Timer ends |
| --- | --- | --- |
| `engine only` | after the command is decoded and the cancel target resolved | immediately after the engine call |
| `end to end` | before the command branch | after the engine call |

The first is "the item". The second is "the item plus the box".

### Finding: the comparison is currently invalid

Three runs of `mixed`, 200,000 commands:

| Run | Throughput | Engine P50 | End-to-end P50 | Delta | Timer floor P50 |
| --- | --- | --- | --- | --- | --- |
| 1 | 3.87 M/s | 566 | 552 | −14 | 32 |
| 2 | 2.29 M/s | 636 | 622 | −14 | 30 |
| 3 | 2.17 M/s | 634 | 574 | −60 | 32 |

The end-to-end number is **lower** than engine-only, which should be impossible:
it does strictly more work inside the timer. The cause is pass order. The
engine-only pass always runs first and warms the code, caches, allocator, and
branch predictor; the end-to-end pass then benefits. The warming effect is
larger than the dispatch cost we are trying to measure.

This is why the benchmark prints a warning next to the delta. Phase 1.4 adds a
warm-up phase before each measured pass, and Phase 1.6 repeats runs. Only then
will a positive delta mean anything.

### Two other observations

- **Timer floor ≈ 30–32 cycles.** Two back-to-back `RDTSCP` readings cost that
  much. Every per-command number already contains this floor. For an operation
  around 600 cycles, the timer is roughly 5% of the reported value.
- **Throughput fell from 3.87 M/s to 2.17 M/s across runs** with identical input.
  Frequency scaling, thermals, or background load. A single throughput number is
  not a measurement; this is Phase 1.6's job.

Files: `docs/experiments/004-modes.txt`, record `003-throughput-vs-latency.md`.

## Next: Phase 1.4 — warm-up and honest statistics

1. Add a warm-up phase before every measured pass, using the same workload but a
   throwaway engine.
2. Keep bookkeeping (`latencies.push_back`, id assignment) outside the timed
   region for the engine-only case.
3. Re-measure: the end-to-end delta should become positive and stable.
4. Decide how much warm-up is enough: hours, commands, or until the median
   stops drifting.


