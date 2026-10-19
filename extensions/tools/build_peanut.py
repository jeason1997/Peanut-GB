#!/usr/bin/env python3
"""Build optional patched Peanut-SDL without modifying upstream source files."""
import argparse
import io
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
BASE = "d0bcca771c83a2638c93a9ae61f3d51f226dc905"
ROOT = Path(__file__).resolve().parents[2]
def run(args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--with-tests", action="store_true", help="also build the optional ROM regression executable")
    args = parser.parse_args()
    if sys.platform != "linux":
        raise SystemExit("Build in Linux/WSL: python3 extensions/tools/build_peanut.py")
    for tool in ["git", "make", "cc", "sdl2-config"]:
        if not shutil.which(tool):
            raise SystemExit("Missing build dependency: " + tool)
    target = ROOT / "build-extensions"
    output = target / "bin"
    output.mkdir(parents=True, exist_ok=True)
    archive = subprocess.check_output(["git", "archive", BASE], cwd=ROOT)
    with tempfile.TemporaryDirectory(prefix="source-", dir=target) as temporary:
        source = Path(temporary)
        with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
            for member in tar.getmembers():
                path = (source / member.name).resolve()
                if source.resolve() not in path.parents:
                    raise SystemExit("Invalid archived source path")
            # Trusted, pinned local Git tree; works on Python versions before 3.12.
            tar.extractall(source)
        # Bound git apply to the isolated tree, even though this directory is
        # nested inside the user's repository. Otherwise Git can discover the
        # outer worktree and silently skip paths outside the current prefix.
        run(["git", "init", "-q"], cwd=source)
        shutil.copytree(ROOT / "extensions", source / "extensions")
        patches = ["0002-peanut-integration.patch"]
        if args.with_tests:
            patches.append("0003-regression-tests.patch")
        for name in patches:
            patch = ROOT / "extensions/patches" / name
            for flags in [["--check"], []]:
                run(["git", "apply"] + flags + [str(patch)], cwd=source)
        run(["make", "-C", "examples/sdl2", "OS=Linux", "-j" + str(min(os.cpu_count() or 1, 4))], cwd=source)
        shutil.copy2(source / "examples/sdl2/peanut-sdl", output / "peanut-sdl")
        if args.with_tests:
            run(["make", "-C", "test", "test_silver_jp"], cwd=source)
            shutil.copy2(source / "test/test_silver_jp", output / "test_silver_jp")
    print("\nExtension executable:", output / "peanut-sdl")
    print('Run from the repository root:\n./build-extensions/bin/peanut-sdl --viewport 200x150 "ROM" "SAVE_COPY"')
    print("Window starts at 1:1. Original examples/sdl2/peanut-sdl is the upstream build.")
if __name__ == "__main__":
    main()
