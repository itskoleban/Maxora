"""Verify outer checksum and every manifest member before artifact integration."""
import argparse
import hashlib
import json
import pathlib
import zipfile


def require(condition, message):
	if not condition:
		raise SystemExit(message)


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("archive", type=pathlib.Path)
	parser.add_argument("--commit", required=True)
	parser.add_argument("--sha", help="Independently reviewed outer SHA-256")
	args = parser.parse_args()
	expected, filename = args.archive.with_suffix(args.archive.suffix + ".sha256").read_text(encoding="ascii").split()
	require(not args.sha or expected == args.sha, "Package differs from independently reviewed SHA-256")
	require(filename == args.archive.name, "Unexpected checksum filename")
	require(hashlib.sha256(args.archive.read_bytes()).hexdigest() == expected, "Archive checksum mismatch")
	with zipfile.ZipFile(args.archive) as archive:
		names = archive.namelist()
		require(len(set(names)) == len(names), "Duplicate archive member")
		for name in names:
			require(not name.startswith("/") and ".." not in pathlib.PurePosixPath(name).parts and "\\" not in name and ":" not in name, "Unsafe archive member")
		manifest = json.loads(archive.read("manifest.json"))
		require(manifest.get("project") == "omp-mmdb" and manifest["version"] == "2.0.0" and manifest["commit"] == args.commit, "Wrong version/commit")
		platform = manifest.get("platform")
		require(platform in {"windows-x86", "linux-x86"}, "Unsupported package platform")
		require(args.archive.name == f"{manifest['project']}-{manifest['version']}-{platform}.zip", "Unexpected archive filename")
		binary = "MMDB.dll" if platform == "windows-x86" else "MMDB.so"
		required = {f"components/{binary}", "include/omp_mmdb.inc", "LICENSE", "README.md", "CHANGELOG.md", "docs/production-readiness.md", "docs/api.md", "examples/geolocation.pwn", "licenses/libmaxminddb/LICENSE", "licenses/libmaxminddb/NOTICE", "licenses/open.mp-sdk/LICENSE.md"}
		require(required.issubset(set(names)), "Required package binary, API, example, documentation or license missing")
		require(set(names) == set(manifest["files"]) | {"manifest.json"}, "Manifest member set mismatch")
		for name, checksum in manifest["files"].items():
			require(hashlib.sha256(archive.read(name)).hexdigest() == checksum, f"Member mismatch: {name}")
	print(f"Verified {args.archive.name} ({args.commit})")


if __name__ == "__main__":
	main()
