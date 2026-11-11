"""Package an installed, validated build; never rebuild release artifacts."""
import argparse
import hashlib
import json
import pathlib
import subprocess
import time
import zipfile


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("--stage", type=pathlib.Path, required=True)
	parser.add_argument("--platform", choices=["windows-x86", "linux-x86"], required=True)
	parser.add_argument("--out", type=pathlib.Path, required=True)
	args = parser.parse_args()
	root = pathlib.Path(__file__).resolve().parents[1]
	commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
	epoch = int(subprocess.check_output(["git", "show", "-s", "--format=%ct", "HEAD"], cwd=root, text=True))
	binary = "MMDB.dll" if args.platform.startswith("windows") else "MMDB.so"
	required = [f"components/{binary}", "include/omp_mmdb.inc", "LICENSE", "README.md", "CHANGELOG.md", "docs/production-readiness.md", "docs/api.md", "examples/geolocation.pwn", "licenses/libmaxminddb/LICENSE", "licenses/libmaxminddb/NOTICE", "licenses/open.mp-sdk/LICENSE.md"]
	for path in required:
		if not (args.stage / path).is_file():
			raise SystemExit(f"Missing package input: {path}")
	args.out.mkdir(parents=True, exist_ok=True)
	manifest = {"project": "omp-mmdb", "version": "0.1.0", "commit": commit, "platform": args.platform, "files": {}}
	for path in sorted(args.stage.rglob("*")):
		if path.is_file() and path.name != "manifest.json":
			manifest["files"][path.relative_to(args.stage).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
	(args.stage / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
	target = args.out / f"omp-mmdb-0.1.0-{args.platform}.zip"
	stamp = time.gmtime(max(epoch, 315532800))[:6]
	with zipfile.ZipFile(target, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
		for path in sorted(args.stage.rglob("*")):
			if path.is_file():
				info = zipfile.ZipInfo(path.relative_to(args.stage).as_posix(), date_time=stamp)
				info.compress_type = zipfile.ZIP_DEFLATED
				info.external_attr = 0o100644 << 16
				archive.writestr(info, path.read_bytes())
	checksum = hashlib.sha256(target.read_bytes()).hexdigest()
	target.with_suffix(target.suffix + ".sha256").write_text(f"{checksum}  {target.name}\n", encoding="ascii")
	print(json.dumps({"package": str(target), "sha256": checksum, "commit": commit}))


if __name__ == "__main__":
	main()
