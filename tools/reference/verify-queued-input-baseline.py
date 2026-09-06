"""Reject the old merging latch with current primary/multiplayer roll fixtures."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
build = root / "build/current"
output = root / "tmp/queued-input-baseline"
header = output / "include/starfox/input/input_latch.hpp"
header.parent.mkdir(parents=True, exist_ok=True)
header.write_bytes(subprocess.check_output([
    "git", "-C", str(root), "show", "1b4a06e:include/starfox/input/input_latch.hpp"]))
compiler = "C:/Strawberry/c/bin/g++.exe"
executables = {}
for fixture in ("timing_parity", "multiplayer_input"):
    exe = output / (fixture + ".exe")
    subprocess.run([compiler, "-std=gnu++20", "-O3", "-DNDEBUG",
        "-D__FUNCSIG__=__PRETTY_FUNCTION__", "-I" + str(output / "include"),
        "-I" + str(root / "include"), str(root / "tests" / (fixture + "_tests.cpp")),
        *(str(build / lib) for lib in ("libstarfox_core.a", "libretro_cpu_65816.a",
            "libretro_cpu_core.a", "libsnes_spc_core.a")), "-o", str(exe)], check=True)
    executables[fixture] = exe
lines = []
for name, fixture, rom, symbols, extra, expected in (
    ("Original primary", "timing_parity", "upstream-ultrastarfox/SF.SFC",
     "upstream-ultrastarfox/SYMBOLS.TXT", ["--short-taps-only"], "presses=1"),
    ("EX primary", "timing_parity", "tmp/runtime-inputs/starfox-ex/SFES.SFC",
     "assets/symbols/starfox-ex.txt", ["--short-taps-only"], "presses=1"),
    ("EX secondary", "multiplayer_input", "tmp/runtime-inputs/starfox-ex/SFES.SFC",
     "assets/symbols/starfox-ex.txt", [], "Multiplayer roll differs")):
    result = subprocess.run([str(executables[fixture]), str(root / rom), str(root / symbols),
                             *extra], capture_output=True, text=True, timeout=180)
    if result.returncode != 1 or expected not in result.stderr or "rolled=0" not in result.stderr:
        raise RuntimeError(f"Unexpected baseline result: {name}: {result.returncode}: {result.stdout} {result.stderr}")
    lines.append(f"{name}: {result.stderr.strip()}")
    print(lines[-1], flush=True)
(output / "result.txt").write_text("\n".join(lines) + "\n")
