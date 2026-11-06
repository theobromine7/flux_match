# Experiment 009 — chunked order pool

| Field | Fill in |
| --- | --- |
| Question | Is per-order allocation from the order store the bottleneck the allocation count suggested? |
| Hypothesis | Replacing `std::deque<OrderEntry>` with chunked storage will remove almost all submit allocations and raise throughput, especially on submit-heavy workloads. |
| Change | One change only: order storage moved into a chunked `OrderPool` (4,096 entries per chunk, placement-new, pointers stay stable). Matching, price levels, trade log, and public behavior are unchanged. |
| Correctness | `ctest --preset windows-release`: 3/3 pass. Stream hashes and trade counts are unchanged. Differential tests still compare 40 seeds × 750 operations. |
| Measurement | Same 5-run baseline as experiment 008: 200,000 commands, seed 42, throughput mode, 1.0 s warm-up. Raw output: `009-pool-throughput.txt`, `009-pool-allocations.txt`. |
| Conclusion | Keep the change. Allocations fell by more than 99.9% and every workload's median rose well beyond its run-to-run spread. |

## Allocations before and after

| Workload | Before | After | Reduction |
| --- | --- | --- | --- |
| resting | 200,052 | 96 | 99.95% |
| one_level_fill | 200,018 | 62 | 99.97% |
| multi_level_sweep | 200,024 | 68 | 99.97% |
| cancel | 150,052 | 83 | 99.94% |
| mixed | 159,912 | 86 | 99.95% |

The remaining allocations are almost all price-level vector growth. Submit and
cancel are now effectively allocation-free at steady state.

## Throughput before and after

| Workload | Before median | After median | Change |
| --- | --- | --- | --- |
| resting | 7,343,110 | 11,091,100 | +51% |
| one_level_fill | 10,853,000 | 23,275,800 | +115% |
| multi_level_sweep | 9,397,170 | 14,898,200 | +59% |
| cancel | 6,011,980 | 8,444,380 | +40% |
| mixed | 4,992,420 | 7,980,970 | +60% |

The change is larger than the 6–39% run-to-run spread measured in experiment 008,
so it is real.

## Why the gain is so large

`std::deque<OrderEntry>` on this MSVC build allocated a block for every pushed
entry. Every submit paid a heap allocation; `one_level_fill` submits 200,000
orders, so it paid 200,000 allocations while doing 100,000 trades. Chunking turns
that into roughly 50 allocations for the whole run.

`cancel` also improved 40% even though cancels themselves never allocated,
because three out of four cancel-workload commands are submissions.

## Why pointers stay valid

Orders are addressed by sequential ID, and matching holds `Order*` briefly. A
chunk is allocated once and never moves. The `chunks` vector may reallocate, but
it moves only the chunk descriptors (`unique_ptr`), not the entry storage, so
`Order*` remains valid.

## Limitations

- Chunk storage is retained until the engine is destroyed; memory is not
  returned to the OS when orders are cancelled. That is acceptable for a
  matching engine with a bounded session, but it is not a general-purpose
  allocator.
- `std::make_unique<std::byte[]>` zero-initializes each chunk. That is wasted
  work; removing it is a separate, smaller experiment.
- 4,096 entries per chunk is a guess. Tuning it (1,024 vs 4,096 vs 16,384) is a
  separate experiment.
- Measured on Windows/MSVC only. Linux allocation behavior is reported by CI.
- This changes storage, not matching complexity: price-level lookup and
  insertion are unchanged.
