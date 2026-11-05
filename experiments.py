#!/usr/bin/env python3
"""Controlled repetitions for CS425, no nonstandard deps except optional plots."""
import csv
from collections import Counter, defaultdict
from pathlib import Path
import platform
import re
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
UPPER_LIMIT = 5000
TRIALS = 3
# Keep all unlisted parameters fixed while varying one at a time.
CONFIGS = [
    ("buffer_size", 2, 2, 2, UPPER_LIMIT),
    ("buffer_size", 8, 2, 2, UPPER_LIMIT),
    ("buffer_size", 32, 2, 2, UPPER_LIMIT),
    ("num_producers", 8, 1, 2, UPPER_LIMIT),
    ("num_producers", 8, 2, 2, UPPER_LIMIT),
    ("num_producers", 8, 4, 2, UPPER_LIMIT),
    ("num_consumers", 8, 2, 1, UPPER_LIMIT),
    ("num_consumers", 8, 2, 2, UPPER_LIMIT),
    ("num_consumers", 8, 2, 4, UPPER_LIMIT),
]


def run_once(dimension, capacity, producers, consumers, limit, trial):
    # Consumer output always goes to a captured pipe, in all configurations.
    command = [str(ROOT / "a.out"), str(capacity), str(producers),
               str(consumers), str(limit)]
    completed = subprocess.run(command, cwd=ROOT, capture_output=True,
                               text=True, timeout=30, check=False)
    if completed.returncode != 0:
        raise RuntimeError(f"Failed: {command}\n{completed.stderr}")
    match = re.search(r"^METRICS (.+)$", completed.stderr, re.MULTILINE)
    if not match:
        raise RuntimeError(f"No METRICS line: {command}\n{completed.stderr}")
    metrics = dict(piece.split("=", 1) for piece in match.group(1).split())

    try:
        observed = [int(line) for line in completed.stdout.splitlines()]
    except ValueError as exc:
        raise RuntimeError(f"Non-integer consumer output: {exc}") from exc
    counts = Counter(observed)
    expected = set(range(limit + 1))
    actual = set(counts)
    return {
        "dimension": dimension, "buffer_size": capacity,
        "num_producers": producers, "num_consumers": consumers,
        "upper_limit": limit, "trial": trial,
        "elapsed_seconds": float(metrics["elapsed_seconds"]),
        "cpu_percent": float(metrics["cpu_percent"]),
        "logical_cores": int(metrics["cores"]),
        "total_cpu_seconds": float(metrics["total_cpu_seconds"]),
        "final_count": int(metrics["final_count"]),
        "printed_count": len(observed),
        "unique_count": len(actual),
        "duplicate_count": sum(n - 1 for n in counts.values() if n > 1),
        "missing_count": len(expected - actual),
        "out_of_range_count": len(actual - expected),
        "operating_system": platform.platform(),
        "architecture": platform.machine(),
    }


def csv_write(path, rows):
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def summarize(rows):
    groups = defaultdict(list)
    for row in rows:
        key = (row["dimension"], row["buffer_size"],
               row["num_producers"], row["num_consumers"], row["upper_limit"])
        groups[key].append(row)
    summary = []
    for (dim, cap, prod, cons, limit), trials in groups.items():
        seconds = [row["elapsed_seconds"] for row in trials]
        cpus = [row["cpu_percent"] for row in trials]
        summary.append({
            "dimension": dim, "buffer_size": cap, "num_producers": prod,
            "num_consumers": cons, "upper_limit": limit,
            "trials": len(trials),
            "trial_seconds": " | ".join(f"{x:.6f}" for x in seconds),
            "mean_elapsed_seconds": statistics.mean(seconds),
            "sd_elapsed_seconds": statistics.stdev(seconds) if len(seconds) > 1 else 0,
            "trial_cpu_percent": " | ".join(f"{x:.4f}" for x in cpus),
            "mean_cpu_percent": statistics.mean(cpus),
            "sd_cpu_percent": statistics.stdev(cpus) if len(cpus) > 1 else 0,
            "mean_printed_count": statistics.mean(r["printed_count"] for r in trials),
            "mean_duplicate_count": statistics.mean(r["duplicate_count"] for r in trials),
            "mean_missing_count": statistics.mean(r["missing_count"] for r in trials),
            "logical_cores": trials[0]["logical_cores"],
        })
    return summary


def make_plots(summary):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("Plots skipped. For plots: python3 -m pip install matplotlib")
        return

    dimensions = [
        ("buffer_size", "Buffer size"),
        ("num_producers", "Number of producers"),
        ("num_consumers", "Number of consumers"),
    ]
    measures = [
        ("mean_elapsed_seconds", "sd_elapsed_seconds", "Elapsed time (seconds)", "elapsed"),
        ("mean_cpu_percent", "sd_cpu_percent", "CPU utilization (%)", "cpu"),
    ]
    for dimension, x_label in dimensions:
        selected = sorted((r for r in summary if r["dimension"] == dimension),
                          key=lambda r: r[dimension])
        for mean_key, sd_key, y_label, short_name in measures:
            fig, ax = plt.subplots(figsize=(7, 4.5))
            x = [r[dimension] for r in selected]
            y = [r[mean_key] for r in selected]
            err = [r[sd_key] for r in selected]
            ax.errorbar(x, y, yerr=err, marker="o", capsize=4)
            ax.set_xticks(x)
            ax.set_xlabel(x_label)
            ax.set_ylabel(y_label)
            ax.set_title(f"{y_label} vs. {x_label.lower()} (mean ± 1 SD, n=3)")
            ax.grid(alpha=0.3)
            fig.tight_layout()
            filename = f"{short_name}_by_{dimension}.png"
            fig.savefig(ROOT / filename, dpi=150)
            plt.close(fig)
            print("Wrote", filename)


def main():
    print("OS:", platform.platform())
    print("Arch:", platform.machine())
    print("Rebuild before running: make clean && make")
    if not (ROOT / "a.out").exists():
        print("Missing a.out: run 'make' first.", file=sys.stderr)
        return 1
    rows = []
    for dim, cap, prod, cons, limit in CONFIGS:
        for trial in range(1, TRIALS + 1):
            row = run_once(dim, cap, prod, cons, limit, trial)
            rows.append(row)
            print(f"{dim}: buffer={cap} P={prod} C={cons} trial={trial} "
                  f"elapsed={row['elapsed_seconds']:.6f}s "
                  f"CPU={row['cpu_percent']:.3f}% "
                  f"printed={row['printed_count']} "
                  f"duplicate={row['duplicate_count']} "
                  f"missing={row['missing_count']}")
    summary = summarize(rows)
    csv_write(ROOT / "results.csv", rows)
    csv_write(ROOT / "summary.csv", summary)
    print("Wrote results.csv and summary.csv")
    make_plots(summary)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
