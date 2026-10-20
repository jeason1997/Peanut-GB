#!/usr/bin/env python3
"""Build and verify optional GBC support in WSL; ROM is always copied."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build-extensions/gbc-verification"
def run(args, **kw):
    return subprocess.run([str(x) for x in args], cwd=ROOT, check=True, **kw)
def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    if sys.platform != "linux":
        raise SystemExit("Run in WSL/Linux")
    OUT.mkdir(parents=True, exist_ok=True)
    run([sys.executable, "extensions/tools/build_peanut.py", "--gbc"])
    flags = subprocess.check_output(["sdl2-config", "--cflags", "--libs"], text=True).split()
    run(["cc", "-std=c99", "-O1", "-g", "-fsanitize=address,undefined", "extensions/gbc/test_core.c", "-o", OUT / "test-core", *flags])
    run([OUT / "test-core"])
    # Run the repository's instruction/timing/DMG LCD fixtures against this
    # optional core too, without editing their original sources.
    upstream = OUT / "upstream"
    shutil.copytree(ROOT / "test", upstream / "test", dirs_exist_ok=True)
    shutil.copyfile(ROOT / "extensions/gbc/peanut_gbc.h", upstream / "core.h")
    (upstream / "peanut_gb.h").write_text(
        '#define PEANUT_FULL_GBC_SUPPORT 1\n'
        '#define __not_in_flash_func(x) x\n'
        '#define RGB555_TO_RGB444(c) ((((c)>>3)&0xf00)|(((c)>>2)&0xf0)|(((c)>>1)&0xf))\n'
        '#include "core.h"\n')
    run(["cc", "-std=c99", "-O2", upstream / "test/test.c", "-o", OUT / "test-dmg-fixtures"])
    run([OUT / "test-dmg-fixtures"])
    if len(sys.argv) != 2:
        print("Pass ROM path for real-game tests.")
        return
    original = Path(sys.argv[1]).resolve()
    before = digest(original)
    copy = OUT / "test.gbc"
    shutil.copyfile(original, copy)
    binary = ROOT / "build-extensions/bin/peanut-sdl-gbc"
    env = dict(os.environ, SDL_VIDEODRIVER="dummy")
    def emulate(*args):
        run([binary, copy, "--hidden", "--fast", *args], env=env)
    inputs = "".join(f"{i} 1\n{i+3} 0\n" for i in range(120, 12000, 120))
    for name, end, tail in [("room", 12000, ""), ("move", 12300, "12000 80\n12045 0\n12100 10\n12145 0\n"), ("menu", 12300, "12000 8\n12003 0\n")]:
        script = OUT / (name + ".txt")
        script.write_text(inputs + tail)
        emulate("--frames", end, "--input", script, "--snapshot", OUT / (name + ".ppm"))
    assert len({digest(OUT / (n + ".ppm")) for n in ["room", "move", "menu"]}) == 3
    # File persistence roundtrip, deliberately distinct from in-game Continue.
    save = OUT / "roundtrip.sav"
    save.write_bytes(bytes(range(256)) * 128)
    saved = digest(save)
    emulate("--frames", 1, "--save", save, "--write-save")
    assert digest(save) == saved
    emulate("--frames", 1, "--save", save)
    assert digest(save) == saved
    assert digest(original) == before == digest(copy)
    print("ROM SHA256:", before)
    print("Input/frame smoke and save file roundtrip PASS; inspect PPMs:", OUT)
if __name__ == "__main__":
    main()
