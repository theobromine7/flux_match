# Experiment 004 — warm-up

| Field | Fill in |
| --- | --- |
| Question | Does a time-based, stability-checked warm-up remove the cold-start bias we saw in 1.3? |
| Hypothesis | The first measured pass will stop being the slowest, and the engine-only vs end-to-end comparison will stop being dominated by pass order. |
| Change | Added `--warmup <seconds>` (default 1.0). Warm-up runs the full stream on fresh, discarded engines until the time floor is met and the last three pass throughputs are within 5%. |
| Correctness | Trade counts unchanged (`81559`) and stream hash unchanged (`a63351e7e27de08b`) in every pass. |
| Measurement | `mixed`, 200,000 commands, seed 42, three runs. File: `005-warmup.txt`. |
| Conclusion | Warm-up removes the large cold-start effect. The harness delta is still within the timer's resolution, so it needs repeat runs rather than more warm-up. |

## Results

| Run | Passes | Warm-up time | First pass | Last pass | Delta (end−engine) | Timer floor |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 16 | 1.14 s | 2.18 M/s | 3.78 M/s | −34 | 30 |
| 2 | 32 | 2.76 s | 3.93 M/s | 3.16 M/s | +14 | 30 |
| 3 | 25 | 1.79 s | 3.98 M/s | 3.35 M/s | +16 | 30 |

## Interpretation

**Warm-up works for its intended job.** In run 1 the first pass was 42% slower
than the last. By the time measurement began, throughput was near its steady
value and the last three passes were within 5%.

**It also revealed a second effect.** Runs 2 and 3 ended slower than they began.
On a two-core laptop, sustained load heats the CPU and reduces frequency. A warm
engine is stable, not necessarily fastest. This is a property of the machine,
and it belongs in the environment notes.

**The harness delta is still unresolved.** Two runs are positive (+14, +16), one
is negative (−34), while the clock floor is 30 cycles. The difference between
"engine only" and "end to end" is at or below our measurement resolution. More
warm-up will not fix that; repeated runs and a reported spread will (1.6).

## Why not warm up on the measured engine

The engine is stateful. A one-second warm-up at ~4 M commands/sec would leave
roughly four million orders in the book before measurement started. The measured
workload would then be "a full book", not the stream we generated. Discarding
the warm-up engine keeps state identical across runs.

## Limitations

- A single run per configuration.
- The two latency scopes still run in a fixed order; residual order bias is
  possible, but it is now smaller than the timer floor.
- Warm-up warms the throughput path. The `RDTSCP` code path itself is only a few
  instructions and is not separately warmed.
