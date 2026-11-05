# CS 425 Project 1 — Report outline (fill with measurements you actually run)

## 1. Environment
- Machine/model: [fill in]
- Operating system and version: [fill in]
- CPU and logical cores available to program: [fill in]
- Compiler/version: [fill in]
- Compile flags: `-std=c11 -O0 -Wall -Wextra -Wpedantic`
- Output policy for all experiments: captured numeric consumer lines.

## 2. Design: shared vs. process-local variables

| Variable | Placement | Why |
|---|---|---|
| `buffer[]` | Shared | Producers write; consumers read the same slots. |
| `count` | Shared | Both roles examine/update believed occupancy. |
| `head`, `tail` | Shared | Every consumer/producer respectively uses a common position. |
| `next_value` | Shared | All producers must use the same integer sequence source. |
| `production_done` | Shared | Parent reports that all producers exited; consumers can detect it. |
| `capacity`, `upper_limit` | Local | Identical process configuration sent via `execlp()`. |
| `value`, `slot`, `observed_count` | Local | Temporaries used only by one process. |
| PIDs, timestamps, resource usage | Local to parent | Only parent creates/waits/measures processes. |

Explain the shared-memory layout and `shm_open`, `ftruncate`, `mmap`, `shm_unlink`.
Explain `fork`, `execlp`, `wait4`, and termination limitations.

## 3. Experimental methods

Baseline: buffer=8, producers=2, consumers=2, upper_limit=5000.
One-parameter variations (three runs each):
- buffer size: 2, 8, 32 (2 producers, 2 consumers)
- producer count: 1, 2, 4 (buffer 8, 2 consumers)
- consumer count: 1, 2, 4 (buffer 8, 2 producers)

Run `python3 experiments.py`. Paste the `summary.csv` results here;
include per-trial times from `results.csv` and both PNG plots.

CPU utilization = (sum of parent + child CPU time) /
                  (elapsed seconds * available logical cores) * 100%.
Each child's CPU time is added exactly once via `wait4` resource usage.

## 4. Performance analysis
Discuss the measured effect of each parameter on BOTH elapsed time
and CPU utilization. Compare work actually done (printed_count), and
avoid attributing lower times to speed if fewer values were delivered.

## 5. Race-condition analysis
Possible example: two producers read `count = 0` and `tail = 0`,
write the same slot, and both store `count = 1` instead of 2. The
first value is overwritten and a generated value may be missing.
Two consumers can similarly read the same head slot or clobber
count updates. Compare printed_count, unique_count, duplicates,
missing values and trial-to-trial variability. The results depend
on scheduling and may be inconsistent, even if a given trial shows
no easily detected anomaly.

## 6. Conclusion / limitations
Discuss unsynchronized read-modify-write operations, potentially
inaccurate state, and termination limitations. Do not claim that
`volatile` or `sched_yield()` prevents races.
