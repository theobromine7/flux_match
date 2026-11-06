# Phase 2 — correctness before optimization

Phase 2 makes the engine's behavior explicit and gives us a way to prove that
later performance changes preserve it. No optimization happens until the
correctness layer is in place.

## Phase 2.1 — invariants (done)

`docs/INVARIANTS.md` defines the contract:

- five order states and the allowed transitions,
- submission and rejection rules,
- price-time priority and the resting-price trade rule,
- cancel semantics for live and terminal orders,
- market-order expiry,
- book structure invariants,
- determinism requirements,
- explicit limitations.

## Phase 2.2 — edge-case tests (done)

`tests/EdgeCaseTests.cpp` covers:

- empty book behavior,
- partial fill followed by cancel,
- cancelling twice and cancelling a filled order,
- zero and negative quantity, zero and negative limit price,
- market price being ignored,
- price-time priority at equal prices,
- market orders sweeping multiple levels and expiring,
- non-crossing limit orders resting,
- extreme valid prices.

Behavior change: invalid orders are now rejected with ID `0` and do not touch
the book. The existing regression test generated negative limit prices, so it
was updated to generate valid prices.

## Phase 2.3 — reference model (done)

`tests/reference_model.hpp` is a deliberately simple implementation:

- stores orders in a plain vector,
- scans every order to find the best opposite price,
- scans for the best bid and ask,
- ignores performance entirely,
- has its own invariant check so a bug in the model is not silently trusted.

It is an **oracle**: a second, independent answer to the same question.

## Phase 2.4 — randomized differential tests (done)

`tests/DifferentialTests.cpp` runs the same seeded command stream through both
implementations and compares:

- order IDs,
- every order's state, price, original quantity, and remaining quantity,
- best bid and best ask,
- every trade's buyer, seller, price, and quantity,
- engine invariants and reference invariants after every operation.

Current coverage: **40 seeds × 750 operations = 30,000 operations**.

A failure is reproducible from its seed and step number.

## Phase 2.5 — invariant checker (done)

`OrderBook::checkInvariants()` walks the book and verifies level ordering, linked
list consistency, order lifecycle state, quantity rules, non-crossing, and ID
storage position. It writes a human-readable reason on failure.

It is called after every operation in the differential test, not inside the hot
path. This keeps the timing measurements representative.

## Results

```text
Edge-case checks passed
Differential checks passed (40 seeds x 750 operations)
Model checks passed

100% tests passed, 0 tests failed out of 3
```

The differential test caught one real issue immediately: the old regression
test used negative limit prices, which are now invalid. No engine-versus-model
mismatch was found in 30,000 randomized operations.

## What this buys us

Every later optimization must keep these tests green. That turns "it feels
faster" into "it is faster and still correct".

## Phase 2.6 — baseline before changing anything (done)

Five runs per workload, 200,000 commands, seed 42, throughput mode, 1.0 s
minimum warm-up:

| Workload | Median (commands/s) | Range |
| --- | --- | --- |
| resting | 7,343,110 | 6.67–7.51 M/s |
| one_level_fill | 10,853,000 | 9.38–11.48 M/s |
| multi_level_sweep | 9,397,170 | 9.00–9.58 M/s |
| cancel | 6,011,980 | 5.55–6.27 M/s |
| mixed | 4,992,420 | 4.39–6.12 M/s |

Run-to-run spread is 6–39%, so a change must move the median, not one run.
Raw data: `docs/experiments/008-baseline-throughput.txt`.

## Phase 2.7 — allocation counting (done)

`fluxmatch_alloc_bench` replaces global `operator new` in a separate executable.
It resets counters around each command and reports allocations, bytes, and a
histogram.

| Workload | Allocations/command | Zero-allocation commands |
| --- | --- | --- |
| resting | 1.0003 | 0% |
| one_level_fill | 1.0001 | 0% |
| multi_level_sweep | 1.0001 | 0% |
| cancel | 0.7503 | 25% |
| mixed | 0.7996 | 20.1% |

**Every submit allocates once; every cancel allocates zero times.** On this
MSVC build the allocation comes from the `std::deque<OrderEntry>` order store,
not from matching or cancelling. Price-level storage accounts for only the rare
2–3 allocation commands.

This changes the priority for Phase 2.8: order storage is the first candidate,
not price-level lookup.

Raw data: `docs/experiments/007-allocations.txt`.

## Next: Phase 2.8 — one change at a time

First candidate: **order storage**. Keep the same public behavior and tests, but
replace the per-order allocation with a pool or chunked storage. Then:

1. Re-run all correctness tests (edge cases, differential, invariants).
2. Re-run the 5-run baseline.
3. Report allocations per command and throughput medians.
4. Keep the change only if it improves the median beyond the run-to-run spread.

Only after that should we try price-level lookup, queue layout, or the trade
log.

