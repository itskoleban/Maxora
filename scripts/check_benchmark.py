"""Check benchmark output integrity without claiming stable hosted-runner timing."""
import argparse
import json
import math
import pathlib


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("result", type=pathlib.Path)
    parser.add_argument("--minimum-seconds", type=float, default=0)
    args = parser.parse_args()
    records = [json.loads(line) for line in args.result.read_text(encoding="utf-8").splitlines() if line.strip()]
    workloads = [x for x in records if x.get("kind") == "workload"]
    expected = {
        "clock_pair", "ipv4_direct", "ipv6_direct", "missing_record", "nested_direct_three_fields",
        "nested_single_lookup_three_fields", "four_databases", "load_reload_unload", "concurrent_reload_query",
    }
    if {x.get("name") for x in workloads} != expected or len(workloads) != len(expected):
        raise SystemExit("Missing or duplicate representative workload")
    for item in workloads:
        if item.get("operations", 0) <= 0 or item.get("samples", 0) <= 0 or item.get("mean_ns", -1) < 0 or item.get("timed_throughput_ops_s", 0) <= 0:
            raise SystemExit("Invalid workload measurement")
        if not 0 <= item.get("p50_ns", -1) <= item.get("p95_ns", -1) <= item.get("p99_ns", -1) <= item.get("p999_ns", -1):
            raise SystemExit("Invalid workload latency percentiles")
    summary = next((x for x in records if x.get("kind") == "summary"), {})
    if (summary.get("wall_seconds", 0) < args.minimum_seconds or summary.get("operations", 0) <= 0
            or summary.get("live_resources") != 8 or summary.get("concurrent_reloads_completed", 0) <= 0):
        raise SystemExit("Benchmark duration, operations or resource invariant failed")
    if summary["operations"] != sum(x["operations"] for x in workloads):
        raise SystemExit("Summary/workload operation counts differ")
    for item in records:
        for key, value in item.items():
            if isinstance(value, (int, float)) and not math.isfinite(value):
                raise SystemExit(f"Nonfinite benchmark value: {key}")
    if len(records) < 3:
        raise SystemExit("Missing workload measurements")
    print(f"Validated measurement structure: {summary['operations']} operations over {summary['wall_seconds']} seconds")


if __name__ == "__main__":
    main()
