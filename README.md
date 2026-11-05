# CS 425 — Project 1: Shared-Memory Multiprocessing

This is a starting implementation for the assignment in `ShM-MP.pdf`.
It intentionally uses **no threads, mutexes, semaphores, condition variables,
or atomic read-modify-write synchronization**. Duplicate, lost, or out-of-order
values are possible and are relevant to the report.

## Files
- `main.c` — arguments; POSIX shared memory; fork/exec; waiting; timings.
- `producer.c` — producer program (one executable per process).
- `consumer.c` — consumer program (one executable per process).
- `shared.h` — flexible shared-memory layout.
- `Makefile` — builds all three executables.
- `experiments.py` — runs repeated experiments, saves CSV, optional plots.
- `REPORT_NOTES.md` — outline for the required report.

## Build and run (macOS or Linux)

From a terminal **inside this directory**:

```sh
make
./a.out 8 2 2 30
```

The four arguments are buffer size, producer count, consumer count, and
largest integer (inclusive). Consumer output goes to **stdout** (one integer
per line); timing metrics go to **stderr**.

```sh
./a.out 8 2 2 30 > consumed.txt 2> timing.txt
cat timing.txt
```

Run invalid-argument tests:

```sh
./a.out
./a.out 0 2 2 30
./a.out 8 -2 2 30
```

## Design notes

Shared: `count` (believed occupancy), `head`, `tail`, `next_value`,
`production_done`, and `buffer[]`.

Local: `capacity`, `upper_limit`, child loop variables, cached count,
positions, values, process IDs, timing variables and resource totals.
The main process sends size and limit to children via `execlp()` arguments.

**Producer termination:** read the shared `next_value`; stop when greater than
`upper_limit`. There is no guarantee that each value is produced once, because
checking and incrementing `next_value` is unsynchronized.

**Consumer termination:** when `count == 0`, exit only if the parent has
marked `production_done` after reaping every producer. This prevents consumers
from treating an empty-but-not-yet-finished buffer as final. A corrupt count
can still cause early exit, omitted data or hangs: neither the occupancy
counter nor head/tail operations are atomic.

`volatile` does **not** prevent races or add interprocess synchronization.
`O0` is used for consistency of the teaching experiment, but it does not
make the shared-memory accesses safe. `sched_yield()` is a CPU scheduling
hint on full/empty retries, not a synchronization primitive.

## Collect measurements

Use the same build and output policy every time:

```sh
python3 experiments.py
```

Produces `results.csv`, `summary.csv`, and, if `matplotlib` is installed,
six plots (elapsed time and CPU utilization for each varied parameter). The script records the actual
OS and architecture. The program reports the logical CPU core count (on
Linux, based on CPU affinity when available; otherwise online cores).

To get plotting support (optional):

```sh
python3 -m pip install matplotlib
```

## Academic/report considerations

- Review the program yourself and understand all shared variables and races.
- Do **not** claim results from someone else's hardware as your measurements.
- Test on the machine you will describe in the report (especially if using
  macOS or a course Linux environment).
- The experiment deliberately cannot ensure exactly-once FIFO consumption.
- For timing interpretation, compare output counts, duplicates, and missing
  values alongside time and CPU utilization.
