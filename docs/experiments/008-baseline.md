# Experiment 008 — pre-optimization baseline

| Field | Fill in |
| --- | --- |
| Question | What is the current throughput of each fixed workload before any Phase 2 optimization? |
| Hypothesis | The workloads have different costs, and run-to-run spread is large enough that a change must move the median, not just one run. |
| Change | None. This is the reference point for Phase 2.8. |
| Correctness | Stream hashes match the Phase 1 workloads; trade counts match the allocation run. |
| Measurement | 5 runs per workload, 200,000 commands, seed 42, throughput mode, 1.0 s minimum warm-up. Raw output: `008-baseline-throughput.txt`. |
| Conclusion | Medians range from 4.99 M/s (mixed) to 10.85 M/s (one-level fill). Mixed is the noisiest workload, so Phase 2.8 should judge changes with repeats, not a single run. |

## Results

| Workload | Min (commands/s) | Median | Max | Max / Min |
| --- | --- | --- | --- | --- |
| resting | 6,667,960 | 7,343,110 | 7,513,060 | 1.13× |
| one_level_fill | 9,384,030 | 10,853,000 | 11,475,600 | 1.22× |
| multi_level_sweep | 9,001,950 | 9,397,170 | 9,584,470 | 1.06× |
| cancel | 5,548,020 | 6,011,980 | 6,267,080 | 1.13× |
| mixed | 4,385,440 | 4,992,420 | 6,117,310 | 1.39× |

## Interpretation

- **Workload shape matters more than a single "engine speed".** A resting book,
  a fill-heavy stream, a sweep, and a cancel-heavy stream are different
  problems. Reporting one number for all of them would hide that.
- **Noise is 6–39% between runs.** A one-off improvement smaller than the spread
  is not evidence. Phase 2.8 compares medians over repeated runs.
- **`mixed` is the hardest to measure.** It combines resting limits, cancels,
  and market orders, so its cost changes as the book grows.

## How this will be used

For each candidate in Phase 2.8:

1. Change one thing.
2. Re-run all correctness tests.
3. Re-run this same 5-run baseline.
4. Compare medians and report the whole range.
5. Keep the change only if the improvement is larger than the noise and
   correctness is unchanged.
