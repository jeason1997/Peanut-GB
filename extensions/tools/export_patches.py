#!/usr/bin/env python3
"""Export only reviewed extension paths; never collect workspace assets."""
import argparse
import difflib
import hashlib
import json
import subprocess
from pathlib import Path
BASE = "d0bcca771c83a2638c93a9ae61f3d51f226dc905"
ROOT = Path(__file__).resolve().parents[2]
def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, check=True).stdout

def normalized(data):
    return data.replace(b"\r\n", b"\n")

def patch(paths):
    chunks = []
    for path in paths:
        old = subprocess.run(["git", "show", f"{BASE}:{path}"], cwd=ROOT, capture_output=True)
        before = normalized(old.stdout) if old.returncode == 0 else b""
        after = normalized((ROOT / path).read_bytes())
        if before == after:
            continue
        chunks.append(f"diff --git a/{path} b/{path}\n")
        if old.returncode:
            chunks.append("new file mode 100644\n")
        chunks.extend(difflib.unified_diff(before.decode().splitlines(True), after.decode().splitlines(True),
            fromfile=f"a/{path}" if old.returncode == 0 else "/dev/null", tofile=f"b/{path}"))
    return "".join(chunks).encode()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    # Descendant commits contain the independent module. The integration base
    # remains pinned; exporting must keep working after committing this module.
    git("merge-base", "--is-ancestor", BASE, "HEAD")
    for path in ["peanut_gb.h", "examples/sdl2/peanut_sdl.c", "test/test.c"]:
        if normalized((ROOT / path).read_bytes()) != normalized(git("show", f"{BASE}:{path}")):
            raise SystemExit("Export from the unpatched upstream layout; changed path: " + path)
    out = args.output.resolve()
    if out.exists() and any(out.iterdir()):
        raise SystemExit("Output directory must be empty (existing artifacts are never overwritten).")
    out.mkdir(parents=True, exist_ok=True)
    module = sorted(p.relative_to(ROOT).as_posix() for p in (ROOT / "extensions").rglob("*")
        if p.is_file() and p.suffix in {".h", ".c", ".py", ".md", ".sh", ".patch", ".json"} and "__pycache__" not in p.parts)
    # The upstream checkout stays untouched. Optional integration/test diffs
    # are reviewed frozen inputs, not regenerated from modified live sources.
    groups = [("0001-extension-module.patch", patch(module))]
    for name in ["0002-peanut-integration.patch", "0003-regression-tests.patch"]:
        groups.append((name, (ROOT / "extensions/patches" / name).read_bytes()))
    manifest = {"base_commit": BASE, "source_hash_format": "SHA256 after CRLF->LF normalization",
        "patches": {}, "files": json.loads((ROOT / "extensions/patches/integration-targets.json").read_text())}
    for path in module:
        manifest["files"][path] = hashlib.sha256(normalized((ROOT / path).read_bytes())).hexdigest()
    for name, data in groups:
        (out / name).write_bytes(data)
        manifest["patches"][name] = hashlib.sha256(data).hexdigest()
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (out / "APPLY.md").write_text(f"""# Apply on a clean checkout

Base commit: `{BASE}`. This package contains no ROM/save/binaries.
Source hashes in manifest normalize CRLF to LF; patch hashes hash exact bytes.
Do not apply to the already modified development workspace.

```sh
git checkout {BASE}
git apply --check 0001-extension-module.patch
git apply 0001-extension-module.patch
git apply --check 0002-peanut-integration.patch
git apply 0002-peanut-integration.patch
git apply --check 0003-regression-tests.patch
git apply 0003-regression-tests.patch
sh extensions/tests/run.sh
make -C examples/sdl2 OS=Linux -j2
make -C test test test_so test_silver_jp
./test/test
./test/test_so
```

For a CRLF checkout, first inspect `git apply --check --ignore-space-change PATCH`;
use the same option for application only if that check succeeds. Context changes
on a different fork need a manual port and regression checks, not forced application.
Module 0001 can be used without 0002/0003 by another emulator through the neutral API.
With a matching external ROM/save, run `sh extensions/tests/run.sh ROM SAVE`.
Reverse in descending order using `git apply -R PATCH`; first --check the reverse.
Read extensions/PORTING.md before porting. NES is an interface design/test target,
not an implemented game backend. MCU integration and SD ROM access remain future work.
""", encoding="utf-8")
    print(out)
    for name, _ in groups:
        print(name, (out / name).stat().st_size, manifest["patches"][name])
if __name__ == "__main__":
    main()
