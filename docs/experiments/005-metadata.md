# Experiment 005 — metadata and JSON output

| Field | Fill in |
| --- | --- |
| Question | Can a result file prove that two runs used the same code, build, setup, and input? |
| Hypothesis | Recording the commit, compiler, build type, machine, configuration, and stream hash is enough to distinguish "reproducible input/code" from "noisy timing". |
| Change | Added a `[metadata]` block and `--json <path>` output. CMake records the git short commit and toolchain facts at configure time. `benchmarks/results/` is gitignored. |
| Correctness | Tests pass (2/2). JSON parses back with the expected schema, commit, stream hash, configuration, CPU, and warm-up status. |
| Measurement | One `mixed` run, 200,000 commands, seed 42, warm-up 1.0 s. File: `006-metadata.txt`. |
| Conclusion | The input and build are now provable. Timing still varies run to run, so the next step is repeated runs and a reported spread. |

## Result

```text
[metadata]
  git commit: 0637d5e
  build: Release
  compiler: MSVC 19.44.35228.0
  c++ standard: 20
  system: Windows / AMD64
  cpu: Intel64 Family 6 Model 140 Stepping 1, GenuineIntel
  logical cores: 4
  host: ALIM
  os: Windows
  time: 2026-10-31T09:33:21Z

Stream hash: a63351e7e27de08b
Trades: 81559
```

The trade count and stream hash match earlier phases exactly. The CPU was in a
different state than the 1.4 runs (2.86 M/s throughput, 540-cycle P50 versus
2.17–3.98 M/s before), which is the point: the metadata identifies the run,
while the spread will describe the machine.

## What each field answers

| Question | Field |
| --- | --- |
| Is this the code I think it is? | `git_commit` |
| Was it built the same way? | `build.config`, compiler, standard |
| Is this the same machine? | CPU, logical cores, OS, host |
| Is this the same experiment? | workload, seed, command count, mode, warm-up |
| Is this the same input? | `workload.stream_hash` |
| When did it run? | `generated_at` (UTC) |

## Why timing cannot match exactly

The command stream is deterministic, so the trade count and hash are identical.
The elapsed time is not: CPU frequency, thermals, background processes, and
cache state differ between runs. Reproducibility therefore means "same input and
same build, with a timing distribution that overlaps", not "same integer".

## Limitations

- Only the short commit is recorded; uncommitted local changes are not flagged.
- Compiler *flags* are not in the JSON yet. Two builds with different flags can
  share the same compiler and configuration name.
- The timestamp is UTC, so compare it with the environment notes when a result
  is produced in another timezone.
