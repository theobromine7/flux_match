# Experiment 002 — workload generators

| Field | Fill in |
| --- | --- |
| Question | Can the benchmark produce repeatable, meaningfully different workloads from a fixed seed? |
| Hypothesis | Five generators with `std::mt19937_64` and integer arithmetic will be deterministic across runs and processes, and different seeds will differ for the random workloads. |
| Change | Added `benchmarks/workloads.{hpp,cpp}`, a `fluxmatch_workloads` library, CLI selection in `fluxmatch_bench`, and `fluxmatch_workload_tests`. |
| Correctness | Unit tests: same seed ⇒ identical stream and hash; different seed ⇒ different mixed stream; cancel targets point to earlier submits; sweep shape is 10 sells + 1 buy; zero and one command work; workload names round-trip. |
| Measurement | 200,000 commands per workload, seed 42, one run each. File: `003-workloads.txt`. |
| Conclusion | Determinism holds. Throughput differs by workload as expected. Numbers are single-run and not yet comparable; warm-up and repeat runs come in 1.4 and 1.6. |

## Workload definitions

| Workload | Shape | Expected fills |
| --- | --- | --- |
| `resting` | Random non-crossing limit orders; bids below mid, asks above | 0 |
| `one_level_fill` | Alternating resting sell and buy at the same price, quantity 1 | 1 per buy |
| `multi_level_sweep` | 10 sells on consecutive prices, then a buy of quantity 10 | 10 levels per sweep |
| `cancel` | 3 resting submits, then cancel a random still-active order | 0 |
| `mixed` | 50% resting limits, 20% cancels, 30% market orders | variable |

## Observed hashes and counts (200,000 commands, seed 42)

| Workload | Stream hash | Trades |
| --- | --- | --- |
| resting | `2dcc3b839b8cf130` | 0 |
| one_level_fill | `5bae424bad664543` | 100,000 |
| multi_level_sweep | `8337df997cb94a60` | 181,810 |
| cancel | `38abd3c4b7abdd6f` | 0 |
| mixed | `a63351e7e27de08b` | 81,559 |

Cross-process determinism check: `mixed`, 50,000 commands, seed 7, run twice →
both `7193c5f7311ff72f`; seed 8 → `38c99cb0afa33b57`.

## Bug found while doing this

`include/fluxmatch/Side.hpp` and `include/fluxmatch/OrderType.hpp` had no
`#pragma once`. They compiled only because `Order.hpp` included them once; as
soon as `workloads.hpp` included them directly, MSVC reported enum
redefinitions. Both headers now have include guards. This is a real latent
packaging bug, not a benchmark issue.

## Why not the other designs

- **Single switch function:** fine at five workloads, becomes one large
  function; generators cannot be tested in isolation.
- **Polymorphic `IWorkload`:** useful when each workload needs runtime state or
  per-workload parameters, but adds virtual dispatch and boilerplate for five
  stateless generators.
- **Chosen: free functions + a dispatch table:** each generator is testable on
  its own, adding one is a function plus a table entry, and the dispatch stays
  small. It will need revisiting if workloads gain parameters beyond count and
  seed.
