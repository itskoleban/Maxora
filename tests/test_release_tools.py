"""Release gate regression tests: incomplete evidence must never permit stable."""
import copy
import importlib.util
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("release_gate", ROOT / "scripts/release_gate.py")
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)


class ReleaseGateTests(unittest.TestCase):
    def setUp(self):
        self.commit = "a" * 40
        names = ("windows_x86_build", "linux_x86_build", "windows_x86_runtime", "linux_x86_runtime", "unit_native_tests", "asan_ubsan", "fuzz", "stress", "benchmark_review", "documentation_examples", "package_verification", "release_candidate_artifacts", "security_review")
        self.report = {"source_commit": self.commit, "gates": {name: {"status": "passed", "evidence": "https://example.test/run", "source_commit": self.commit} for name in names}, "soak": {"status": "passed", "source_commit": self.commit, "duration_seconds": 86400, "environment": "test-host", "workload": "documented", "evidence": "https://example.test/soak"}, "unresolved_critical_high": 0, "reviewer": "maintainer", "reviewed_at": "2026-11-02T00:00:00Z"}
        self.report["candidate_tag"] = "v2.0.0-rc.1"
        self.report["package_sha256"] = {"windows-x86": "a" * 64, "linux-x86": "b" * 64}

    def test_complete_reviewed_report(self):
        self.assertEqual(GATE.check(self.report, self.commit), [])

    def test_empty_report_fails(self):
        self.assertTrue(GATE.check({}, self.commit))

    def test_each_mandatory_gate_is_required(self):
        for name in self.report["gates"]:
            report = copy.deepcopy(self.report)
            del report["gates"][name]
            self.assertTrue(GATE.check(report, self.commit), name)

    def test_soak_duration_must_be_continuous_24_hours(self):
        self.report["soak"]["duration_seconds"] = 86399
        self.assertTrue(GATE.check(self.report, self.commit))

    def test_wrong_commit_is_rejected(self):
        self.assertTrue(GATE.check(self.report, "b" * 40))

    def test_nonfinite_soak_duration_is_rejected(self):
        for duration in (float("nan"), float("inf"), -1, "86400", True):
            self.report["soak"]["duration_seconds"] = duration
            self.assertTrue(GATE.check(self.report, self.commit))

    def test_unreviewed_package_digest_is_rejected(self):
        del self.report["package_sha256"]["linux-x86"]
        self.assertTrue(GATE.check(self.report, self.commit))

    def test_unresolved_high_is_rejected(self):
        self.report["unresolved_critical_high"] = 1
        self.assertTrue(GATE.check(self.report, self.commit))

    def test_passed_gate_without_evidence_is_rejected(self):
        self.report["gates"]["security_review"]["evidence"] = ""
        self.assertTrue(GATE.check(self.report, self.commit))


class PackageVerificationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.archive = pathlib.Path(self.directory.name) / "package.zip"
        self.commit = "a" * 40

    def package(self, files=None, version="2.0.0", bad_hash=False, platform="linux-x86"):
        if files is None:
            paths = ("components/Maxora.so", "include/maxora.inc", "LICENSE", "README.md", "CHANGELOG.md", "docs/production-readiness.md", "docs/api.md", "examples/geolocation.pwn", "licenses/libmaxminddb/LICENSE", "licenses/libmaxminddb/NOTICE", "licenses/open.mp-sdk/LICENSE.md")
            files = {path: b"synthetic-test-content" for path in paths}
        manifest = {"version": version, "commit": self.commit, "platform": platform, "files": {key: hashlib.sha256(value).hexdigest() for key, value in files.items()}}
        if bad_hash:
            manifest["files"][next(iter(files))] = "0" * 64
        with zipfile.ZipFile(self.archive, "w") as archive:
            for key, value in files.items():
                archive.writestr(key, value)
            archive.writestr("manifest.json", json.dumps(manifest))
        checksum = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        self.archive.with_suffix(".zip.sha256").write_text(f"{checksum}  {self.archive.name}\n", encoding="ascii")
        return checksum

    def verify(self, *arguments):
        return subprocess.run([sys.executable, str(ROOT / "scripts/verify_package.py"), str(self.archive), "--commit", self.commit, *arguments], capture_output=True, text=True)

    def test_valid_package(self):
        checksum = self.package()
        self.assertEqual(self.verify("--sha", checksum).returncode, 0)

    def test_tampered_outer_archive_is_rejected(self):
        self.package()
        with self.archive.open("ab") as stream:
            stream.write(b"tamper")
        self.assertNotEqual(self.verify().returncode, 0)

    def test_tampered_manifest_hash_is_rejected(self):
        self.package(bad_hash=True)
        self.assertNotEqual(self.verify().returncode, 0)

    def test_path_traversal_is_rejected(self):
        self.package({"../escape": b"bad"})
        self.assertNotEqual(self.verify().returncode, 0)

    def test_wrong_version_is_rejected(self):
        self.package(version="1.1.1")
        self.assertNotEqual(self.verify().returncode, 0)

    def test_missing_required_files_is_rejected(self):
        self.package(files={"components/Maxora.so": b"binary"})
        self.assertNotEqual(self.verify().returncode, 0)

    def test_wrong_platform_is_rejected(self):
        self.package(platform="windows-x86")
        self.assertNotEqual(self.verify().returncode, 0)

    def test_wrong_commit_is_rejected(self):
        self.package()
        self.assertNotEqual(self.verify("--commit", "b" * 40).returncode, 0)

    def test_independent_review_digest_is_required_to_match(self):
        self.package()
        self.assertNotEqual(self.verify("--sha", "0" * 64).returncode, 0)


if __name__ == "__main__":
    unittest.main()
