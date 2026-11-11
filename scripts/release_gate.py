"""Fail closed unless reviewed, commit-bound production evidence is complete."""
import argparse
import json
import math
import pathlib
import re


def check(report, commit):
	failures = []
	if not re.fullmatch(r"[0-9a-f]{40}", commit) or report.get("source_commit") != commit:
		failures.append("evidence must name the exact 40-character release candidate commit")
	if not re.fullmatch(r"v2\.0\.0-rc\.(?:[2-9]|[1-9][0-9]+)", str(report.get("candidate_tag", ""))):
		failures.append("reviewed candidate_tag must be v2.0.0-rc.N with candidate number >= 2, without leading zeros")
	for platform in ("windows-x86", "linux-x86"):
		if not re.fullmatch(r"[0-9a-f]{64}", str(report.get("package_sha256", {}).get(platform, ""))):
			failures.append(f"{platform}: independently reviewed package SHA-256 is required")
	for key in ("windows_x86_build", "linux_x86_build", "windows_x86_runtime", "linux_x86_runtime", "unit_native_tests", "asan_ubsan", "tsan", "fuzz", "stress", "benchmark_review", "documentation_examples", "package_verification", "release_candidate_artifacts", "security_review"):
		item = report.get("gates", {}).get(key, {})
		if item.get("status") != "passed" or not item.get("evidence") or item.get("source_commit") != commit:
			failures.append(f"{key}: missing passed evidence bound to release commit")
	soak = report.get("soak", {})
	duration = soak.get("duration_seconds", 0)
	duration_valid = isinstance(duration, (int, float)) and not isinstance(duration, bool) and math.isfinite(duration) and duration >= 86400
	if soak.get("status") != "passed" or soak.get("source_commit") != commit or not duration_valid or not all(soak.get(key) for key in ("environment", "workload", "evidence")):
		failures.append("24-hour continuous soak has not passed with environment, workload and evidence")
	if report.get("unresolved_critical_high") != 0:
		failures.append("security/correctness critical/high review incomplete or unresolved")
	if not report.get("reviewer") or not report.get("reviewed_at"):
		failures.append("final engineering review is missing")
	return failures


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("--evidence", type=pathlib.Path, required=True)
	parser.add_argument("--commit", required=True)
	parser.add_argument("--candidate-tag")
	parser.add_argument("--candidate-runs", type=pathlib.Path)
	args = parser.parse_args()
	report = json.loads(args.evidence.read_text(encoding="utf-8"))
	failures = check(report, args.commit)
	if args.candidate_tag and report.get("candidate_tag") != args.candidate_tag:
		failures.append("reviewed candidate tag differs from requested promotion")
	if args.candidate_runs:
		runs = json.loads(args.candidate_runs.read_text(encoding="utf-8")).get("workflow_runs", [])
		if not any(run.get("head_sha") == args.commit and run.get("head_branch") == args.candidate_tag and run.get("event") == "workflow_dispatch" and run.get("conclusion") == "success" for run in runs):
			failures.append("successful candidate workflow including both published artifact tests is missing")
	if failures:
		raise SystemExit("Stable release BLOCKED:\n- " + "\n- ".join(failures))
	print(f"All stable gates passed for {args.commit}")


if __name__ == "__main__":
	main()
