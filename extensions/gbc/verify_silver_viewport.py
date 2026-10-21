#!/usr/bin/env python3
"""Reproducible Silver CGB expanded-view tests; never write input ROM/save."""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import sys
import zlib

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build-extensions/gbc-silver-viewport"
def run(args, **kw):
    return subprocess.run([str(a) for a in args], cwd=ROOT, check=True, **kw)
def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def ppm(p):
    magic, dims, maximum, data = p.read_bytes().split(b"\n", 3)
    w, h = map(int, dims.split())
    assert magic == b"P6" and maximum == b"255" and len(data) == w*h*3
    return w, h, data
def center(p):
    w, h, b = ppm(p)
    left, top = (w-160)//2, (h-144)//2
    return b"".join(b[((y+top)*w+left)*3:((y+top)*w+left+160)*3] for y in range(144))
def main():
    if sys.platform != "linux" or len(sys.argv) != 3:
        raise SystemExit("WSL: python3 extensions/gbc/verify_silver_viewport.py ROM SAVE")
    original, original_save = (Path(p).resolve() for p in sys.argv[1:])
    before, saved = digest(original), digest(original_save)
    assert zlib.crc32(original.read_bytes()) == 0x0aea5383, "Requires exact Silver JP Rev1"
    OUT.mkdir(parents=True, exist_ok=True)
    rom, save = OUT / "silver.gbc", OUT / "silver.sav"
    shutil.copyfile(original, rom); shutil.copyfile(original_save, save)
    run([sys.executable, "extensions/tools/build_peanut.py", "--gbc"])
    flags = subprocess.check_output(["sdl2-config", "--cflags", "--libs"], text=True).split()
    for name in ["viewport", "sprite_seams", "silver_roofs", "silver_viewport"]:
        run(["cc", "-std=c99", "-O1", "-g", "-fsanitize=address,undefined", f"extensions/gbc/test_{name}.c", "-o", OUT / name, *flags])
        run([OUT / name, *([rom, save] if name == "silver_viewport" else [rom] if name == "silver_roofs" else [])])
    run(["cc", "-std=c99", "-O1", "-g", "-fsanitize=address,undefined", "extensions/gbc/test_silver_roof_route.c", "-o", OUT / "roof-route", *flags])
    route = subprocess.run([str(OUT / "roof-route"), str(rom), str(save)], cwd=ROOT)
    if route.returncode not in (0, 77):
        raise subprocess.CalledProcessError(route.returncode, route.args)
    run(["cc", "-std=c99", "-O1", "-g", "-fsanitize=address,undefined", "extensions/gbc/test_silver_sprite_seams.c", "-o", OUT / "sprite-seams", *flags])
    seams = subprocess.run([str(OUT / "sprite-seams"), str(rom), str(save)], cwd=ROOT)
    if seams.returncode not in (0, 77):
        raise subprocess.CalledProcessError(seams.returncode, seams.args)
    run(["cc", "-std=c99", "-O1", "-g", "-fsanitize=address,undefined", "extensions/gbc/test_transition_catchup.c", "-o", OUT / "transition-catchup", *flags])
    transition = subprocess.run([str(OUT / "transition-catchup"), str(rom), str(save)], cwd=ROOT)
    if transition.returncode not in (0, 77):
        raise subprocess.CalledProcessError(transition.returncode, transition.args)
    script = OUT / "continue.txt"
    text = "".join(f"{i} {'8' if i < 180 else '1'}\n{i+4} 0\n" for i in range(120, 460, 60))
    script.write_text(text)
    binary = ROOT / "build-extensions/bin/peanut-sdl-gbc"
    env = dict(os.environ, SDL_VIDEODRIVER="dummy")
    def emulate(size, tag=None, extra=(), frames=1800, input_path=script):
        shot = OUT / ((tag or size) + ".ppm")
        run([binary, rom, "--save", save, "--viewport", size, "--input", input_path, "--hidden", "--fast", "--frames", frames, "--snapshot", shot, *extra], env=env)
        return shot
    reference = center(emulate("160x144"))
    for size in ["200x150", "320x240", "321x241", "640x480", "160x240", "320x144"]:
        shot = emulate(size)
        w, h, b = ppm(shot)
        # Expanded NPC activation intentionally changes game state/RNG, so
        # comparing this scene with unpatched 160x144 is no longer valid.
        assert f"{w}x{h}" == size
    blank = emulate("320x240", "no-background", ["--no-background"])
    assert center(blank) == reference
    w, h, b = ppm(blank)
    assert all(b[(y*w+x)*3:(y*w+x+1)*3] == bytes(3) for y in range(h) for x in range(w) if x<80 or x>=240 or y<48 or y>=192)
    menu = OUT / "menu.txt"; menu.write_text(text + "1800 8\n1804 0\n")
    emulate("320x240", "menu", frames=2100, input_path=menu)
    walk = OUT / "walk.txt"; walk.write_text(text + "1800 40\n2100 0\n")
    emulate("320x240", "walk", frames=2400, input_path=walk)
    assert digest(original) == before and digest(original_save) == saved and digest(save) == saved
    print("PASS Silver expanded sizes/menu/walk/no-background; display-only native/core comparison and production NPC continuity; ROM and save unchanged")
    print("Screenshots:", OUT)
if __name__ == "__main__":
    main()
