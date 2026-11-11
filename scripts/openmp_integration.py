#!/usr/bin/env python3
"""Exercise the actual component in an isolated official open.mp server runtime.

The artifact under test is copied unchanged. --duration is additional sustained
workload time AFTER lifecycle checks; --duration 86400 is the final 24-hour soak.
"""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import queue
import shutil
import struct
import subprocess
import tarfile
import tempfile
import threading
import time
import zipfile

from download_runtime import RUNTIMES, VERSION

# This observation belongs to the verified archive of this release.
SHUTDOWN_PROBE_VERSION = "v1.6.7.3218"


def rss_bytes(process):
	"""Best-effort resident memory sample; excludes file cache outside this process."""
	if os.name != "nt":
		try:
			for line in Path(f"/proc/{process.pid}/status").read_text().splitlines():
				if line.startswith("VmRSS:"):
					return int(line.split()[1]) * 1024
		except (OSError, ValueError):
			return None
	else:
		import ctypes
		from ctypes import wintypes
		class Counters(ctypes.Structure):
			_fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD)] + [
				(name, ctypes.c_size_t) for name in ("PeakWorkingSetSize", "WorkingSetSize",
				"QuotaPeakPagedPoolUsage", "QuotaPagedPoolUsage", "QuotaPeakNonPagedPoolUsage",
				"QuotaNonPagedPoolUsage", "PagefileUsage", "PeakPagefileUsage")]
		counters = Counters()
		counters.cb = ctypes.sizeof(counters)
		query = ctypes.windll.psapi.GetProcessMemoryInfo
		query.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
		query.restype = wintypes.BOOL
		if query(wintypes.HANDLE(int(process._handle)), ctypes.byref(counters), counters.cb):
			return counters.WorkingSetSize
	return None


def sha256(path):
	with open(path, "rb") as stream:
		return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--server-archive", type=Path)
	parser.add_argument("--server-dir", type=Path)
	parser.add_argument("--component", required=True, type=Path)
	parser.add_argument("--pawncc", type=Path)
	parser.add_argument("--package-dir", type=Path, help="Use the installed include and example from this package tree")
	parser.add_argument("--port", type=int, default=17779)
	parser.add_argument("--dbip-city", type=Path, help="Uncompressed DB-IP City Lite provider file")
	parser.add_argument("--ipinfo-lite", type=Path, help="Uncompressed official IPinfo Lite sample")
	parser.add_argument("--duration", type=float, default=10)
	parser.add_argument("--evidence", required=True, type=Path)
	args = parser.parse_args()
	if args.duration < 1 or bool(args.server_archive) == bool(args.server_dir):
		parser.error("duration must be >=1 and exactly one runtime source is required")
	if not 1 <= args.port <= 65535:
		parser.error("port must be in 1..65535")
	root = Path(__file__).resolve().parents[1]
	package_root = args.package_dir.resolve() if args.package_dir else root
	args.evidence = args.evidence.resolve()
	args.evidence.parent.mkdir(parents=True, exist_ok=True)
	log_path = args.evidence.with_suffix(".log")
	probe_shutdown = bool(args.server_archive) and VERSION == SHUTDOWN_PROBE_VERSION
	exit_callbacks = {"ordinary": [], "shutdown": []}
	result = {"status": "failed", "component_sha256": sha256(args.component),
			  "requested_workload_seconds": args.duration,
			  "runtime": f"open.mp {VERSION}" if args.server_archive else "locally supplied open.mp", "platform": os.name,
			  "exit_callbacks": exit_callbacks, "shutdown_probe_enabled": probe_shutdown,
			  "log": log_path.name, "workload": "Every 100 ms: 100 direct integer queries, missing field, IPv6 prefix, missing IPv4 record. Every 10 ticks: reload, failed reload, unload and reuse a name.",
			  "memory_measurement": "resident process memory sampled once per second; not an allocation count"}
	proc = None
	started = time.monotonic()
	try:
		(root / "work").mkdir(exist_ok=True)
		with tempfile.TemporaryDirectory(prefix="omp-mmdb-openmp-", dir=root / "work") as temporary, contextlib.ExitStack() as shutdown:
			temporary = Path(temporary)
			if args.server_archive:
				archive = args.server_archive.resolve()
				result["runtime_archive_sha256"] = sha256(archive)
				_, expected = RUNTIMES["windows-x86" if os.name == "nt" else "linux-x86"]
				if result["runtime_archive_sha256"] != expected:
					raise RuntimeError(f"runtime archive differs from pinned official {VERSION} asset")
				if zipfile.is_zipfile(archive):
					with zipfile.ZipFile(archive) as z:
						for member in z.infolist():
							target = (temporary / member.filename).resolve()
							if not target.is_relative_to(temporary.resolve()):
								raise RuntimeError("unsafe ZIP member")
						z.extractall(temporary)
				else:
					with tarfile.open(archive) as t:
						t.extractall(temporary, filter="data")
			else:
				shutil.copytree(args.server_dir.resolve(), temporary / "Server")
			candidates = list(temporary.rglob("omp-server.exe" if os.name == "nt" else "omp-server"))
			if len(candidates) != 1:
				raise RuntimeError("archive must contain exactly one server executable")
			server = candidates[0]
			runtime = server.parent
			compiler = args.pawncc.resolve() if args.pawncc else runtime / "qawno" / "pawncc.exe"
			if not compiler.is_file():
				raise RuntimeError("pass --pawncc for the pinned Pawn compiler")
			for directory in ("gamemodes", "filterscripts", "scriptfiles", "components"):
				(runtime / directory).mkdir(exist_ok=True)
			artifact = runtime / "components" / ("MMDB.dll" if os.name == "nt" else "MMDB.so")
			shutil.copy2(args.component.resolve(), artifact)
			assert sha256(artifact) == result["component_sha256"]
			fixture = root / "lib/libmaxminddb/t/maxmind-db/test-data"
			shutil.copy2(fixture / "MaxMind-DB-test-decoder.mmdb", runtime / "scriptfiles/decoder.mmdb")
			shutil.copy2(fixture / "MaxMind-DB-test-ipv4-24.mmdb", runtime / "scriptfiles/ipv4.mmdb")
			shutil.copy2(fixture / "MaxMind-DB-test-ipv6-24.mmdb", runtime / "scriptfiles/ipv6.mmdb")
			shutil.copy2(fixture / "MaxMind-DB-test-nested.mmdb", runtime / "scriptfiles/nested.mmdb")
			(runtime / "scriptfiles/truncated.mmdb").write_bytes((fixture / "MaxMind-DB-test-decoder.mmdb").read_bytes()[:32])
			provider_files = {}
			provider_flags = []
			provider_markers = []
			for name, supplied, define, marker in (
				("dbip-city", args.dbip_city, "MMDB_TEST_DBIP_CITY", "MMDB_DBIP_PASS"),
				("ipinfo-lite", args.ipinfo_lite, "MMDB_TEST_IPINFO_LITE", "MMDB_IPINFO_PASS"),
			):
				if supplied is None:
					continue
				target = runtime / "scriptfiles" / (name + ".mmdb")
				shutil.copy2(supplied.resolve(), target)
				provider_files[name] = {"bytes": target.stat().st_size, "sha256": sha256(target)}
				provider_flags.append(define + "=1")
				provider_markers.append(marker)
			result["provider_files"] = provider_files
			test_flags = ["MMDB_TEST_PINNED_SHUTDOWN=1"] if probe_shutdown else []
			for source, target in ((root / "tests/pawn/integration.pwn", runtime / "gamemodes/mmdb_test.amx"),
								   (root / "tests/pawn/side.pwn", runtime / "filterscripts/mmdb_side.amx"),
								   (package_root / "examples/geolocation.pwn", temporary / "geolocation.amx")):
				compiled = subprocess.run([str(compiler), str(source), "-i" + str(package_root / "include"),
										   "-i" + str(runtime / "qawno/include"), "-t4", "-o" + str(target), "-d3", "-E+",
										   "MMDB_TEST_DURATION_MS=" + str(int(args.duration * 1000)),
										   *provider_flags, *test_flags], cwd=runtime,
										  capture_output=True, text=True, creationflags=0x08000000 if os.name == "nt" else 0)
				if compiled.returncode:
					raise RuntimeError("Pawn compile failed: " + compiled.stdout + compiled.stderr)
				header = target.read_bytes()[:6]
				if len(header) != 6 or struct.unpack_from("<H", header, 4)[0] != 0xF1E0:
					raise RuntimeError("Pawn compiler emitted a non-32-bit AMX; build it with PAWN_CELL_SIZE=32")
			config = {"announce": False, "enable_query": False, "max_players": 5,
					  "name": "omp-mmdb isolated integration", "pawn": {"main_scripts": ["mmdb_test 1"],
					  "side_scripts": ["filterscripts/mmdb_side"], "legacy_plugins": []},
					  "network": {"bind": "127.0.0.1", "port": args.port},
					  "artwork": {"enable": False}, "rcon": {"enable": False},
					  "logging": {"enable": True, "file": "log.txt", "use_timestamp": False}}
			(runtime / "config.json").write_text(json.dumps(config), encoding="utf-8")
			if os.name != "nt":
				server.chmod(server.stat().st_mode | 0o111)
			lines = queue.Queue()
			if os.name == "nt":
				proc = subprocess.Popen([str(server)], cwd=runtime, stdin=subprocess.DEVNULL,
										stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
										text=True, encoding="utf-8", errors="replace", bufsize=1,
										creationflags=0x08000000)
				output = proc.stdout
			else:
				# A PTY makes libc logging line-buffered. A plain CI stdout pipe
				# can hide startup markers until its multi-kilobyte buffer fills.
				import pty
				master, slave = pty.openpty()
				proc = subprocess.Popen([str(server)], cwd=runtime, stdin=subprocess.DEVNULL,
										stdout=slave, stderr=subprocess.STDOUT)
				os.close(slave)
				output = os.fdopen(master, "r", encoding="utf-8", errors="replace")
				shutdown.callback(output.close)
			def stop_process():
				if proc.poll() is None:
					proc.kill()
					proc.wait(timeout=20)
			shutdown.callback(stop_process)
			def reader():
				try:
					for line in output:
						lines.put(line)
				except OSError:
					pass  # Linux PTY master reports EIO after the slave exits.
			log_reader = threading.Thread(target=reader, daemon=True)
			log_reader.start()
			counts = {"MMDB_MAIN_PASS": 0, "MMDB_AFTER_RESET_PASS": 0,
					  "MMDB_SIDE_PASS": 0, "MMDB_SIDE_EXIT": 0, "MMDB_SOAK_TICK": 0,
					  "MMDB_SIDE_CLEANUP_PASS": 0, "MMDB_MAIN_CLEANUP_PASS": 0,
					  "MMDB_WORKLOAD_FINISHED": 0}
			counts.update(dict.fromkeys(provider_markers, 0))
			with log_path.open("w", encoding="utf-8") as log:
				def drain():
					while True:
						try:
							line = lines.get_nowait()
						except queue.Empty:
							return
						log.write(line.replace(str(runtime), "<runtime>"))
						log.flush()
						if any(marker in line for marker in ("MMDB_FAIL", "ERROR: AddressSanitizer",
															 "ERROR: LeakSanitizer", "runtime error:")):
							raise RuntimeError(line.strip())
						_, marker, payload = line.partition("MMDB_EXIT ")
						if marker:
							script, presence_status, loaded, version_status, version = payload.split()
							phase = "shutdown" if counts["MMDB_WORKLOAD_FINISHED"] else "ordinary"
							exit_callbacks[phase].append(
								{"script": script, "presence_status": int(presence_status), "loaded": int(loaded),
								 "version_status": int(version_status), "ip_version": int(version)})
						for marker in counts:
							if marker in line:
								counts[marker] += 1
				def wait_for(marker, count):
					deadline = time.monotonic() + 40
					while counts[marker] < count:
						drain()
						if proc.poll() is not None:
							raise RuntimeError("server exited before " + marker)
						if time.monotonic() >= deadline:
							raise RuntimeError("timeout waiting for " + marker)
						time.sleep(0.05)
				# Pawn timers exercise the real command dispatch, including on
				# Windows where hidden-process wcin may not accept pipe input.
				wait_for("MMDB_SIDE_PASS", 3)
				wait_for("MMDB_AFTER_RESET_PASS", 2)
				for marker in provider_markers:
					wait_for(marker, 2)
				if counts["MMDB_SIDE_CLEANUP_PASS"] < 2 or counts["MMDB_MAIN_CLEANUP_PASS"] < 1:
					raise RuntimeError("script-owned resources were not proven invalid after unload")
				workload_start = time.monotonic()
				initial_ticks = counts["MMDB_SOAK_TICK"]
				samples = []
				next_sample = workload_start
				while time.monotonic() - workload_start < args.duration:
					drain()
					if proc.poll() is not None:
						raise RuntimeError("server exited during workload")
					if time.monotonic() >= next_sample:
						sample = rss_bytes(proc)
						if sample is not None:
							samples.append(sample)
						next_sample = time.monotonic() + 1
					time.sleep(0.05)
				drain()
				elapsed = time.monotonic() - workload_start
				ticks = counts["MMDB_SOAK_TICK"] - initial_ticks
				if ticks < max(1, int(args.duration / 2)):
					raise RuntimeError("insufficient completed Pawn workload markers")
				proc.wait(timeout=20)
				log_reader.join(timeout=5)
				if log_reader.is_alive():
					raise RuntimeError("server log did not close after exit")
				drain()
				if counts["MMDB_WORKLOAD_FINISHED"] != 1:
					raise RuntimeError("Pawn workload did not finish normally")
				if proc.returncode != 0:
					raise RuntimeError("unclean server exit: " + str(proc.returncode))
				filter_exit = {"script": "filterscript", "presence_status": 0, "loaded": 1,
							   "version_status": 0, "ip_version": 4}
				gamemode_exit = dict(filter_exit, script="gamemode", ip_version=6)
				if exit_callbacks["ordinary"] != [filter_exit, filter_exit, gamemode_exit]:
					raise RuntimeError("unexpected MMDB status during reloadfs, unloadfs or GMX")
				# On the pinned runtime both native calls return MMDB_INVALID_ARGUMENT
				# (1) and leave the Pawn output cells at their initial values.
				expected_shutdown = [
					{"script": script, "presence_status": 1, "loaded": 0, "version_status": 1, "ip_version": -1}
					for script in ("filterscript", "gamemode")
				] if probe_shutdown else []
				if sorted(exit_callbacks["shutdown"], key=lambda event: event["script"]) != expected_shutdown:
					raise RuntimeError("unexpected MMDB shutdown observations")
				result.update(status="passed", workload_seconds=elapsed, markers=counts,
							  minimum_completed_direct_queries=max(0, ticks - 1) * 1000,
							  resident_memory_samples=len(samples),
							  first_resident_bytes=samples[0] if samples else None,
							  last_resident_bytes=samples[-1] if samples else None,
							  maximum_sampled_resident_bytes=max(samples) if samples else None,
							  final_24h_soak_passed=elapsed >= 86400, exit_code=proc.returncode,
							  lifecycle_checks=["initial gamemode and filterscript", "filterscript reload/unload/load",
												"same name isolated across scripts", "GMX and post-reset lookup", "graceful shutdown"])
	except Exception as error:
		result["error"] = str(error)
	finally:
		if proc and proc.poll() is None:
			proc.kill()
			proc.wait(timeout=20)
		result["total_elapsed_seconds"] = time.monotonic() - started
		args.evidence.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
	print(json.dumps(result, indent=2))
	return 0 if result["status"] == "passed" else 1


if __name__ == "__main__":
	raise SystemExit(main())
