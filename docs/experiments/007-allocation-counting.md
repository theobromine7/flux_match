# Experiment 007 — allocation counting

| Field | Fill in |
| --- | --- |
| Question | Where does the engine allocate, and how often? |
| Hypothesis | If every order allocates, order storage is a better first optimization target than price-level lookup. |
| Change | Added `fluxmatch_alloc_bench`, which replaces global `operator new` in a separate executable and snapshots the counters around every command. Engine and workload setup happen before the counters are reset. |
| Correctness | The allocation benchmark does not replace the engine tests. It runs the same deterministic workloads and reports the same stream hashes and trade counts. |
| Measurement | 200,000 commands, seed 42, five workloads. Raw output: `007-allocations.txt`. |
| Conclusion | Every submit allocates exactly once on this MSVC build; cancels never allocate. Order storage is the first candidate in 2.8. |

## Results

| Workload | Allocations | Allocations/command | Bytes/command | Zero-allocation commands | Max in one command |
| --- | --- | --- | --- | --- | --- |
| resting | 200,052 | 1.0003 | 93.7 | 0% | 3 |
| one_level_fill | 200,018 | 1.0001 | 93.0 | 0% | 3 |
| multi_level_sweep | 200,024 | 1.0001 | 93.0 | 0% | 3 |
| cancel | 150,052 | 0.7503 | 75.7 | 25% | 3 |
| mixed | 159,912 | 0.7996 | 79.3 | 20.1% | 3 |

## Interpretation

- **Submit costs one allocation every time.** The count is almost exactly the
  number of submissions. The rare extra allocations (2 or 3 in a single command)
  come from price-level storage growth.
- **Cancel is allocation-free.** Unlinking a resting order only rewrites
  pointers; it never grows a container.
- **The number of allocations scales with orders submitted, not with trades.**
  `one_level_fill` trades 100,000 times but still allocates once per submit.

The likely cause is the order store: `std::deque<OrderEntry>` with an
`OrderEntry` around 80 bytes. On this MSVC standard library, that appears to
allocate a block for each pushed entry. The exact block policy is a library
implementation detail, so the Linux count may differ.

That is the point of measuring instead of assuming: **before optimizing price
levels, the evidence says order storage allocates far more often.**

## Limitations

- The tracker replaces global `operator new` for the whole executable. Between
  reset and snapshot only engine code runs, but the loop and any hidden library
  allocation would also be counted.
- Construction costs (engine, trade reserve, command stream, ID array) are
  excluded by resetting after setup.
- Counts are specific to the compiler's standard library. The Linux CI run
  reports its own numbers.
- The tracker is Windows-verified; Linux compilation is covered by CI.
