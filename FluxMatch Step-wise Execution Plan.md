# FluxMatch: Step-wise Execution Plan

A phase-by-phase plan for carrying out the FluxMatch systems-engineering project in your repo. File names come from your plan (`main.cpp`, `MatchingEngine.cpp`, `OrderBook.cpp`, `MarketDataClient.cpp`); adjust paths to match what is actually in the repository.

## Overview

| Order | Phase | Focus | Depends on |
| --- | --- | --- | --- |
| 1 | 0 | Build and docs | None |
| 2 | 1 | Benchmarking | 0 |
| 3 | 2 | Data structures and correctness | 1 |
| 4 | 3 | Profiling | 1, 2 |
| 5 | 4 | Concurrency | 2, 3 |
| 6 | 5 | Networking and serialization | 0 (4 helpful) |
| 7 | 6 | Deterministic replay and final report | 2, 5 |
| 8 | 7 | Python analysis and ML | 6 |
| 9 | 8 | Database, API, UI | 6, 7 |
| 10 | 9 | CI/CD and cloud | Grows alongside everything, finalized last |

Phases 0–6 are the core. Phases 7–9 build on top of a dependable replay.

## Phase 0: Build and understand

| # | Step | Where | Done when |
| --- | --- | --- | --- |
| 0.1 | Write build, test, and run instructions for the current Windows setup | `docs/BUILD.md`, `README.md` | A fresh clone builds following only the doc |
| 0.2 | Split targets: `fluxmatch_core` (library), `fluxmatch_demo`, `fluxmatch_marketdata`, `fluxmatch_tests`, `fluxmatch_bench` | `CMakeLists.txt` | Each target builds on its own |
| 0.3 | Add `CMakePresets.json` with debug and release presets for Windows | repo root | `cmake --preset release` works |
| 0.4 | Add a Linux/WSL preset and verify the build there | `CMakePresets.json` | Build and tests pass on both platforms |
| 0.5 | Record compiler, build type, dependencies, known limitations | `docs/ENVIRONMENT.md` | File lists versions and caveats |

**Exit gate:** a fresh checkout builds and tests on Windows and Linux/WSL using only the docs.

## Phase 1: Benchmarking fundamentals

| # | Step | Where | Done when |
| --- | --- | --- | --- |
| 1.1 | Move the benchmark out of `main.cpp` into its own executable | `benchmarks/bench_main.cpp` | `main.cpp` is demo-only |
| 1.2 | Build workload generators (resting, one-level fill, multi-level sweep, cancel, mixed) with fixed seeds | `benchmarks/workloads.*` | Same seed gives the same stream |
| 1.3 | Measure throughput and per-op latency as separate modes | `benchmarks/` | Both reported independently |
| 1.4 | Report p50, p95, p99, and max, with a warm-up phase | `benchmarks/stats.*` | Percentiles printed per workload |
| 1.5 | Capture metadata: workload size, warm-up, compiler, flags, machine, duration | `benchmarks/` | Included in every output |
| 1.6 | Repeat runs and report variability (min, median, stddev across runs) | `benchmarks/` | N-run summary available |
| 1.7 | Write results as JSON or CSV | `benchmarks/results/` (gitignored) | Machine-readable file per run |
| 1.8 | Experiment with cycle counters (`rdtsc`) and document timer overhead, CPU migration, and frequency caveats | `docs/experiments/timers.md` | Written comparison against `steady_clock` |

**Exit gate:** you can explain each benchmark, reproduce it, and tell a real change from noise.

## Phase 2: Data structures, memory, correctness

| # | Step | Where | Done when |
| --- | --- | --- | --- |
| 2.1 | Write order-book invariants and order lifecycle rules (price/time priority, fills, cancels, terminal states) | `docs/INVARIANTS.md` | Reviewed and complete |
| 2.2 | Add edge-case tests (empty book, partial fill, cancel of filled order, duplicate IDs, zero or negative qty) | `tests/` | All pass |
| 2.3 | Write a simple reference matching model | `tests/reference_model.*` | Obviously correct, even if slow |
| 2.4 | Add randomized differential tests, engine versus reference | `tests/` | Thousands of seeded random streams pass |
| 2.5 | Add an invariant checker that can run after each op in debug builds | `src/OrderBook.cpp` | Enabled in tests |
| 2.6 | Profile a baseline workload before changing anything | `docs/experiments/` | Baseline recorded |
| 2.7 | Add allocation counting | `benchmarks/` | Allocs per op reported |
| 2.8 | For each candidate change (price-level lookup, order lookup, per-level queue, pool allocator only if allocation shows up), make one change, rerun tests and the same benchmarks, and write the experiment record | `src/`, `docs/experiments/` | One record per change |

**Exit gate:** you can explain a measured trade-off and show matching behavior is preserved.

## Phase 3: Profiling and CPU fundamentals

| # | Step | Where | Done when |
| --- | --- | --- | --- |
| 3.1 | Choose a stable benchmark workload as the profiling target | `docs/experiments/` | Documented |
| 3.2 | Profile on Windows (WPA/ETW) and Linux/WSL (`perf` or equivalent) | `docs/profiling/` | Baseline profiles saved |
| 3.3 | Review CPU time, allocations, call stacks, and scheduling, plus cache and branch counters where available | `docs/profiling/` | Findings written |
| 3.4 | Inspect generated assembly for 2–3 hot functions (Debug vs Release, `-O2` vs `-O3`) | `docs/profiling/asm.md` | Observations recorded |
| 3.5 | Run controlled layout experiments (hot/cold split, contiguous vs pointer-heavy) | `benchmarks/layout_*` | Results with numbers |
| 3.6 | Log every hypothesis, evidence, and outcome, including failures | `docs/experiments/` | Template used consistently |

**Exit gate:** you can identify a bottleneck from evidence, explain it, and show before/after numbers.

## Phase 4: Concurrency

| # | Step | Where | Done when |
| --- | --- | --- | --- |
| 4.1 | Document ownership of orders, queues, and results | `docs/CONCURRENCY.md` | Diagram and rules written |
| 4.2 | Implement a mutex and condition-variable bounded queue and a producer → matching-thread pipeline | `src/concurrency/` | Works end-to-end |
| 4.3 | Implement a bounded SPSC ring buffer (atomics, acquire/release) | `src/concurrency/spsc_queue.h` | Unit tests pass |
| 4.4 | Define backpressure policy (block, drop, or reject) | `docs/CONCURRENCY.md` | Implemented and tested |
| 4.5 | Add an output/event queue to consumers | `src/` | Consumer lag behavior tested |
| 4.6 | Benchmark both queues: throughput, tail latency, occupancy, and overload | `benchmarks/` | Comparison table |
| 4.7 | Run ThreadSanitizer (Linux/WSL) and other available checks, and add stress tests | `tests/`, CI later | Clean runs |
| 4.8 | Assert that concurrent output equals single-threaded output for the same input | `tests/` | Passes on random streams |
| 4.9 | Evaluate MPSC only if a real use case emerges | `docs/experiments/` | Decision recorded |

**Exit gate:** the concurrent version is deterministic-equivalent to the baseline, and overload behavior is tested.

## Phase 5: Networking and serialization

| # | Step | Where | Done when |
| --- | --- | --- | --- |
| 5.1 | Split `MarketDataClient.cpp` into transport, protocol parsing, normalization, and the core interface | `src/net/`, `src/feed/` | Each layer is independently testable |
| 5.2 | Harden TLS (peer and hostname verification), timeouts, reconnect with backoff, input validation, error reporting | `src/net/` | Failure-injection tests pass |
| 5.3 | Add a capture format and recorder | `src/capture/` | A live session is saved to disk |
| 5.4 | Measure parse, normalize, queue handoff, and I/O time separately | `benchmarks/` | Per-stage numbers |
| 5.5 | Define a text format and a versioned binary event format (magic, version, endianness) and compare them | `schemas/`, `src/capture/` | Size and speed comparison |
| 5.6 | Test disconnects, malformed messages, stale updates, and sequence gaps or out-of-order updates | `tests/` | Faults are surfaced, not swallowed |
| 5.7 | Keep the system in simulation mode: public data only, no real order routing | `README.md` | Stated explicitly |

**Exit gate:** a captured session replays deterministically, and bad or missing data is flagged.

## Phase 6: Deterministic replay and final evaluation

| # | Step | Where | Done when |
| --- | --- | --- | --- |
| 6.1 | Define versioned input and output event schemas | `schemas/` | Schema files committed |
| 6.2 | Build a replay runner executable | `src/replay/`, `fluxmatch_replay` | Feeds a fixed stream into the engine |
| 6.3 | Compare trades, order states, and book state, with a checksum or hash of output | `tests/` | Golden files match |
| 6.4 | Add replay scenarios for normal flow and failure cases | `data/fixtures/` | Small fixtures only |
| 6.5 | Benchmark the same replay before and after changes | `benchmarks/` | Baseline stored |
| 6.6 | Write the final report: correctness, throughput, percentiles, measurement limits, trade-offs | `docs/REPORT.md` | Reviewer can follow it |

**Exit gate:** a reviewer can rerun the same input, reproduce correctness, and compare performance to the baseline.

## Phase 7: Python analysis and ML

| # | Step | Where | Done when |
| --- | --- | --- | --- |
| 7.1 | Create a Python package with a loader for replay data (Parquet) and benchmark output | `python/fluxmatch/` | `pip install -e .` works |
| 7.2 | Build analysis notebooks or scripts: distributions, features, simple rules, transaction costs | `python/analysis/` | Reproducible from fixtures |
| 7.3 | Build a backtester that reuses the C++ event definitions | `python/backtest/` | Event semantics match the schema |
| 7.4 | Model fees, spread, slippage, partial fills, and explicit latency assumptions | `python/backtest/` | Parameters configurable |
| 7.5 | Use chronological or walk-forward splits and prevent look-ahead and leakage | `python/` | Leakage tests pass |
| 7.6 | Add ML only after a credible baseline strategy exists | `python/ml/` | Compared against baseline out-of-sample |
| 7.7 | Track dataset version, parameters, code version, model version, and results per run | `python/tracking/` | Every run is reproducible |
| 7.8 | Add Python tests, formatting, and type checks (pytest, ruff, mypy) | `python/` | Pass locally |

**Exit gate:** results reproduce from data version and config, include costs, and report out-of-sample performance.

## Phase 8: Database, API, UI

| # | Step | Where | Done when |
| --- | --- | --- | --- |
| 8.1 | Design the PostgreSQL schema (instruments, configs, runs, summary metrics) with migrations | `api/migrations/` | Migrations apply cleanly |
| 8.2 | Store large datasets as Parquet outside the database, referenced by path or URI | `data/`, DB | Provenance link per run |
| 8.3 | Build an API to start and inspect replay and analysis runs with async jobs | `api/` | Run lifecycle works |
| 8.4 | Define typed API contracts (OpenAPI) and generate TypeScript types | `api/`, `ui/` | Types in sync |
| 8.5 | Scaffold the React and TypeScript UI | `ui/` | Builds and serves |
| 8.6 | Add pages for benchmark history, build and workload config, replay and correctness, strategy/ML metrics, and service health | `ui/src/pages/` | Each reads real data |
| 8.7 | Add live updates only where useful, and keep it read-only or simulation-only | `ui/`, `api/` | Optional |
| 8.8 | Link each result to its exact dataset, build, and config | API and UI | Saved runs are fully inspectable |

**Exit gate:** you can open a saved run later and see its inputs, configuration, build, and results.

## Phase 9: CI/CD and cloud

Start the CI steps early, right after Phase 0, and extend them as phases land.

| # | Step | Where | Done when |
| --- | --- | --- | --- |
| 9.1 | C++ build and tests on Windows and Linux | `.github/workflows/cpp.yml` | Green on every push |
| 9.2 | Python tests, format, and type checks | `.github/workflows/python.yml` | Green |
| 9.3 | Frontend lint, test, and build | `.github/workflows/ui.yml` | Green |
| 9.4 | Build and smoke-test the container image | `Dockerfile`s, workflow | Starts and passes health check |
| 9.5 | Upload benchmark outputs as artifacts (informational only, no hard thresholds on shared runners) | workflow | Artifacts downloadable |
| 9.6 | Containerize API, worker, and UI separately from the C++ benchmark environment | `docker/` | Each runs standalone |
| 9.7 | Choose one provider, with object storage, managed Postgres, and a small container service | `infra/` | Demo environment up |
| 9.8 | Add secrets management, least-privilege identity, health checks, logs, and basic monitoring | `infra/` | Observable |
| 9.9 | Add automated deployment after CI is stable | workflow | Push-to-deploy works |
| 9.10 | Document costs and a teardown procedure | `docs/CLOUD.md` | Teardown tested |

**Exit gate:** a change is built and tested automatically, and the demo deploys and can be observed without manual source edits.

## Per-experiment template

Copy this into `docs/experiments/NNN-title.md` for each experiment.

| Field | Fill in |
| --- | --- |
| Question | What behavior are you investigating? |
| Hypothesis | What do you expect, and why? |
| Change | Smallest implementation change |
| Correctness | Which tests prove behavior is preserved? |
| Measurement | Workload and metrics |
| Conclusion | What changed, what didn't, limitations |
