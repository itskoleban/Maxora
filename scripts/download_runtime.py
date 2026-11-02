"""Download only the pinned official open.mp runtime and verify its SHA-256."""
import argparse
import hashlib
import pathlib
import urllib.request

RUNTIMES = {
    "windows-x86": ("open.mp-win-x86.zip", "db44b6cedc78e79fa4c7470ec81186ad9e880ad34529ae78fddfea70ec603223"),
    "linux-x86": ("open.mp-linux-x86.tar.gz", "0f7bfe70bb68ea780fe24a910b69412046e53d10feb65a9d4a9de8ff1f7f1329"),
}
VERSION = "v1.6.7.3218"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", choices=RUNTIMES, required=True)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    args = parser.parse_args()
    name, expected = RUNTIMES[args.platform]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    url = f"https://github.com/openmultiplayer/open.mp/releases/download/{VERSION}/{name}"
    with urllib.request.urlopen(url, timeout=120) as response:
        payload = response.read(128 * 1024 * 1024 + 1)
    if len(payload) > 128 * 1024 * 1024 or hashlib.sha256(payload).hexdigest() != expected:
        raise SystemExit("Runtime download digest/size validation failed")
    args.out.write_bytes(payload)
    print(f"Verified {VERSION} {name} {expected}")


if __name__ == "__main__":
    main()
