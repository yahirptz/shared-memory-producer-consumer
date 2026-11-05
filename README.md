# Shared-Memory Producer–Consumer System

**CS 425: Operating Systems I — Project 1**  
**Yahir Perez and Clement Anzuruni**  
Western Kentucky University | Fall 2026

## Overview

This project examines how multiple processes communicate through POSIX shared memory using a bounded producer–consumer buffer. The main program creates a shared-memory region and starts separate producer and consumer processes. Producers attempt to generate integers from 0 through a specified upper limit, while consumers remove values from the buffer and print them.

The implementation deliberately **does not use mutexes, semaphores, threads, or other synchronization primitives**, as required by the assignment. The goal is to observe what happens when processes read and modify shared data without atomic coordination, rather than to guarantee that every value is consumed exactly once.

## Implementation

The project is written in C and consists of three executable programs:

- `main.c` validates command-line arguments, creates and initializes the POSIX shared-memory region, starts child processes using `fork()` and `execlp()`, waits for them, and reports performance measurements.
- `producer.c` checks the buffer count, writes a value into the buffer when space appears available, and advances the shared producer value.
- `consumer.c` checks the buffer count, reads and prints a value when an item appears available, and updates the buffer position.

The shared-memory structure is declared in `shared.h`. It contains the buffer, its current count, the head and tail positions, the next value to produce, and a flag indicating when producer processes have finished. Configuration values and temporary variables are kept local to each process where possible.

The parent sets `production_done` after waiting for all producer processes. Consumers use this flag and the buffer count to decide when to exit. Because the count and buffer updates are unsynchronized, the termination strategy is not guaranteed to succeed in every possible execution.

## Building and Running

The program was developed and tested on macOS using Apple Clang. It uses POSIX interfaces and can also be compiled in a compatible Linux environment.

From the project directory, run:

```bash
make clean && make
./a.out 8 2 2 30
```

The command-line format is:

```text
./a.out <buffer_size> <num_producers> <num_consumers> <upper_limit>
```

The four parameters specify the bounded-buffer capacity, the number of producer processes, the number of consumer processes, and the maximum integer to generate (inclusive). All parameters except `upper_limit` must be positive; `upper_limit` may be zero.

Consumers print the values they remove to standard output. The main process prints a `METRICS` line to standard error containing elapsed time, CPU utilization, logical core count, total CPU time, final buffer count, and child-process failure status.

## Experiments

All reported measurements were collected on the same machine with the following configuration:

| Component | Configuration |
| --- | --- |
| Computer | Apple MacBook Pro |
| Processor | Apple M4 Pro (ARM64) |
| Memory | 24 GB unified memory |
| Operating system | macOS Tahoe 26.6.2 |
| Logical CPU cores | 14 |
| Compiler | Apple Clang 21.0.0 |
| Build flags | `-std=c11 -O0 -Wall -Wextra -Wpedantic` |

The baseline configuration was a buffer size of 8, two producers, two consumers, and an upper limit of 5,000. We changed one variable at a time:

- **Buffer size:** 2, 8, and 32, with two producers and two consumers.
- **Producer count:** 1, 2, and 4, with a buffer size of 8 and two consumers.
- **Consumer count:** 1, 2, and 4, with a buffer size of 8 and two producers.

Each configuration was run three times, for a total of **27 trials**. Elapsed time was measured with `clock_gettime(CLOCK_MONOTONIC)`. CPU utilization was calculated from the parent and child CPU times, using `wait4()` resource-usage data and normalizing by the 14 logical cores.

To repeat the measurements:

```bash
python3 experiments.py
```

The script writes individual results to `results.csv`, aggregates them in `summary.csv`, and generates six plots if Matplotlib is available. The CSV files also record how many output values were duplicated or missing.

## Results and Observations

The experiments showed that increasing buffer capacity from 2 to 32 reduced mean elapsed time from approximately **7.15 ms to 5.70 ms**. Increasing the number of producers or consumers generally raised CPU utilization, but did not consistently reduce elapsed time.

Duplicate and missing values occurred in the recorded trials. For example, with a buffer size of 2 and two producers and consumers, the three trials averaged about **842 duplicate values** and **360 missing values**. These observations are consistent with the unsynchronized reads and writes to the shared buffer, counters, and indices.

These measurements should not be interpreted as a strict comparison of processing efficiency. Some runs print more values than others, so faster execution does not necessarily mean that more useful or correct work was completed. Short execution times also make scheduling and process-startup overhead relevant to the measurements.

## Project Files

| File | Purpose |
| --- | --- |
| `main.c` | Main process, shared-memory setup, child management, and metrics |
| `producer.c` | Producer process |
| `consumer.c` | Consumer process |
| `shared.h` | Shared-memory data structure |
| `Makefile` | Build commands |
| `experiments.py` | Repeated trials, CSV summaries, and plots |
| `results.csv` | Individual experimental results |
| `summary.csv` | Mean results and standard deviations |
| `elapsed_by_*.png`, `cpu_by_*.png` | Performance figures |

## Limitations

Because there is no synchronization, the buffer count and head/tail positions can become inconsistent. This can lead to overwritten items, duplicate output, missing values, or an execution that does not terminate normally. The `volatile` qualifiers in `shared.h` do not make compound operations atomic, and `sched_yield()` only asks the scheduler to give another process a chance to run.

These limitations are intentional for this assignment and are discussed in the accompanying project report.
