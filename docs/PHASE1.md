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

## Next: Phase 1.2 — workload generators

Design five fixed-seed workloads:

- **resting:** insert orders that never cross,
- **one-level fill:** each incoming order fills exactly one resting order,
- **multi-level sweep:** one incoming order consumes several price levels,
- **cancel:** insert then cancel in a deterministic pattern,
- **mixed:** a weighted mix of the above.

Requirements: same seed produces the same stream, each workload is parameterised
by size, and the benchmark reports which workload it ran. The current
1,000,000-sell setup becomes one of these workloads rather than being hardcoded.
