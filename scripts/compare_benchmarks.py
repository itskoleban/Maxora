"""Compare unmodified Maxora 2.0 and omp-mmdb core benchmarks on one host."""
import argparse
import json
import pathlib
import statistics
import subprocess


PAIRS = (
    ("clock_pair", "clock_pair"),
    ("ipv4_lookup_release", "ipv4_direct"),
    ("ipv6_lookup_release", "ipv6_direct"),
    ("missing_record", "missing_record"),
    ("nested_ephemeral_three_fields", "nested_direct_three_fields"),
    ("nested_reusable_three_fields", "nested_direct_three_fields"),
    ("nested_ephemeral_three_fields", "nested_single_lookup_three_fields"),
    ("four_databases", "four_databases"),
    ("load_reload_unload", "load_reload_unload"),
)
MEASURES = ("mean_ns", "p50_ns", "p95_ns", "p99_ns", "resident_after_bytes", "peak_resident_bytes")


def run(binary, workload, iterations, fixtures, log):
    command = [str(binary), "--iterations", str(iterations), "--workload", workload]
    if fixtures is not None:
        command.extend(["--fixtures", str(fixtures)])
    result = subprocess.run(command, capture_output=True, encoding="utf-8", timeout=300, check=False)
    log.write_text(result.stdout, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"{binary.name} {workload} failed: {result.stderr.strip()}")
    records = {item["kind"]: item for item in map(json.loads, result.stdout.splitlines())}
    measured = records["workload"]
    summary = records["summary"]
    if measured["name"] != workload or measured["operations"] != iterations or summary["operations"] != iterations:
        raise RuntimeError(f"unexpected workload or iteration count in {log}")
    return records["environment"], summary["checksum"], {**measured, **summary}


def compare_pair(args, baseline, candidate):
    series = {"baseline": [], "candidate": []}
    binaries = {"baseline": (args.baseline, baseline), "candidate": (args.candidate, candidate)}
    for trial in range(1, args.trials + 1):
        order = ("baseline", "candidate") if trial % 2 else ("candidate", "baseline")
        results = {}
        for side in order:
            binary, workload = binaries[side]
            log = args.output_dir / f"{baseline}--{candidate}--{side}-{trial}.jsonl"
            results[side] = run(binary, workload, args.iterations, args.fixtures, log)
        before, before_sum, before_metrics = results["baseline"]
        after, after_sum, after_metrics = results["candidate"]
        for key in ("reader", "pointer_bits", "warmup_operations_per_workload"):
            if before[key] != after[key]:
                raise RuntimeError(f"baseline and candidate differ in {key}")
        if before_sum != after_sum:
            raise RuntimeError(f"workload checksum mismatch: {baseline} vs {candidate}")
        series["baseline"].append(before_metrics)
        series["candidate"].append(after_metrics)
    medians = {
        side: {key: statistics.median(row[key] for row in rows) for key in MEASURES}
        for side, rows in series.items()
    }
    old_mean = medians["baseline"]["mean_ns"]
    return {
        "baseline_workload": baseline,
        "candidate_workload": candidate,
        "trials": args.trials,
        "operations_per_trial": args.iterations,
        "median_by_metric": medians,
        "candidate_over_baseline_mean": medians["candidate"]["mean_ns"] / old_mean if old_mean else None,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("baseline", "candidate", "fixtures", "output-dir"):
        parser.add_argument(f"--{name}", type=lambda path: pathlib.Path(path).resolve(), required=name != "fixtures")
    parser.add_argument("--iterations", type=int, default=10000)
    parser.add_argument("--trials", type=int, default=5)
    args = parser.parse_args()
    if args.trials < 1 or args.iterations < 1:
        parser.error("iterations and trials must be positive")
    if not args.baseline.is_file() or not args.candidate.is_file():
        parser.error("baseline and candidate must be executable files")
    args.output_dir.mkdir(parents=True)
    results = [compare_pair(args, baseline, candidate) for baseline, candidate in PAIRS]
    report = json.dumps(results, indent=2, allow_nan=False) + "\n"
    (args.output_dir / "summary.json").write_text(report, encoding="utf-8")
    print(report, end="")


if __name__ == "__main__":
    main()
