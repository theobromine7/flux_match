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

## Next: Phase 2.6 and 2.7 — measure before changing

1. Record a baseline for the fixed workloads **before** touching data structures.
2. Count allocations per operation.
3. Only then try one candidate change at a time (2.8), rerunning the same tests
   and benchmarks.

The candidate changes, in order of expected value:

- price-level lookup,
- order lookup and storage,
- per-level queue layout,
- pool allocator, only if allocation counting shows it matters.
