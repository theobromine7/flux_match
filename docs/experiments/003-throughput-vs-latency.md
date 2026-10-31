# Experiment 003 — throughput vs latency, and the cost of the harness

| Field | Fill in |
| --- | --- |
| Question | What is the engine's own latency, and how much does the benchmark harness add? |
| Hypothesis | End-to-end latency must be greater than or equal to engine-only latency, and the gap is the dispatch cost. |
| Change | Added `--mode throughput|latency|both`. Latency runs twice against fresh engines: timer around the engine call only, then timer around dispatch + engine call. Added an empty-region timer measurement. |
| Correctness | All three passes produce identical trade counts (`81559`) from the same stream hash (`a63351e7e27de08b`), which confirms the engine starts fresh each time. |
| Measurement | `mixed`, 200,000 commands, seed 42, three runs. File: `004-modes.txt`. |
| Conclusion | The measured delta is negative and therefore invalid as a harness estimate. Pass-order warming dominates. Warm-up is required before this comparison means anything. |

## Results

| Run | Throughput | Engine P50 | End-to-end P50 | Delta | Timer floor P50 |
| --- | --- | --- | --- | --- | --- |
| 1 | 3.87 M/s | 566 | 552 | −14 | 32 |
| 2 | 2.29 M/s | 636 | 622 | −14 | 30 |
| 3 | 2.17 M/s | 634 | 574 | −60 | 32 |

## Interpretation

End-to-end does strictly more work inside the timer, so it cannot genuinely be
faster. The only difference between the passes is order: engine-only always runs
first and warms the engine code, caches, allocator, and branch predictor. That
warm-up benefit is tens of cycles, larger than the dispatch cost being measured.

This is a textbook example of a confounder. The benchmark is measuring "first
pass vs second pass", not "with box vs without box".

## What is trustworthy

- **Timer floor ~30–32 cycles.** Stable across runs, and consistent with two
  serialized timestamp reads. Every per-command figure includes this.
- **Trade counts.** Identical in all passes, so fresh-engine semantics hold.
- **Shape of the latency distribution.** P50 is hundreds of cycles; P99 is
  roughly 4–5× P50; max is dominated by interrupts and scheduling.

## What is not trustworthy yet

- The harness delta (sign is wrong).
- Throughput across runs (2.17–3.87 M/s for identical input).
- Any small before/after comparison.

## Next

Phase 1.4 adds a warm-up pass before each measurement. Phase 1.6 repeats runs
and reports variability. After both, the delta should be positive and the
throughput spread should be reported rather than hidden.
