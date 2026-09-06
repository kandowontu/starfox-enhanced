"""Isolate EX chase inputs in a separate counterfactual reference executable.

Never a parity result: this substitutes zero for two native transfer-word loads.
ROM files, the normal reference executable, and production code stay unchanged.
"""
import argparse
from collections import Counter
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--updates", type=int, default=1000)
parser.add_argument("--output", type=Path, default=root / "tmp/zero-transfer-diagnostic")
args = parser.parse_args()
if not 1 <= args.updates <= 10000:
    parser.error("updates must be 1..10000")
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
main = (root / "tools/reference/full-system/main.cpp").read_text()
main = main.replace('#include "gameplay_audit.hpp"',
                    '#include "' + (root / "tools/reference/full-system/gameplay_audit.hpp").as_posix() + '"')
anchor = '    cpu_hook = [&](unsigned pc, unsigned clocks) {'
assert main.count(anchor) == 1
main = main.replace(anchor, """    std::map<std::string, unsigned> zeroed_reads;
    std::string diagnostic_error;
""" + anchor)
anchor = '        if (platform.pending_jump) return;'
assert main.count(anchor) == 1
main = main.replace(anchor, anchor + """
        if (const auto reader = scorpion_transfer_reads.find(pc - 2U);
                reader != scorpion_transfer_reads.end()) {
            auto& r = sfc::cpu.r;
            if (unsigned(r.d.w) != 0U || (unsigned(r.p) & 0x20U) != 0U) {
                diagnostic_error = "Counterfactual reader has unexpected D/M state";
            } else {
                r.a.w = 0;
                r.p.n = 0;
                r.p.z = 1;
                ++zeroed_reads[reader->second];
            }
        }
""")
main = main.replace('        if (!camera_error.empty()) break;',
                    '        if (!camera_error.empty() || !diagnostic_error.empty()) break;')
anchor = '    if (gameplay) gameplay->finish();'
assert main.count(anchor) == 1
main = main.replace(anchor, r"""    std::cout << "COUNTERFACTUAL: native transfer loads replaced with zero; NOT PARITY\n";
    for (const auto& [name, count] : zeroed_reads)
        std::cout << "ZEROED " << name << ' ' << count << '\n';
    if (!diagnostic_error.empty()) throw std::runtime_error(diagnostic_error);
""" + anchor)
source = output / "zero_transfer.cpp"
source.write_text(main)
ares = root / "tmp/ares-timing"
reference = root / "tmp/full-reference-build"
build = root / "build/current"
exe = output / "zero_transfer.exe"
command = ["C:/Strawberry/c/bin/g++.exe", "-std=gnu++20", "-O3", "-DNDEBUG",
           "-DBUILD_RELEASE", "-DPROFILE_ACCURACY", "-DSLJIT_HAVE_CONFIG_POST=1",
           "-DSLJIT_HAVE_CONFIG_PRE=1", '-DSTARFOX_REFERENCE_ARES_ROOT="' + ares.as_posix() + '"',
           "-I" + str(root / "include")]
for directory in (reference / "generated", ares / "ares", ares / "nall", ares, ares / "thirdparty"):
    command += ["-isystem", str(directory)]
command += [str(source), str(reference / "libares_full.a")]
command += [str(build / lib) for lib in ("libstarfox_core.a", "libretro_cpu_65816.a",
                                        "libretro_cpu_core.a", "libsnes_spc_core.a")]
command += ["-lws2_32", "-lole32", "-lshell32", "-lshlwapi", "-static", "-Wl,--stack,16777216", "-o", str(exe)]
subprocess.run(command, check=True)
rom = root / "tmp/runtime-inputs/starfox-ex/SFES.SFC"
symbols = root / "assets/symbols/starfox-ex.txt"
prefix = output / "ex-LEVEL7_2"
run = subprocess.run([str(exe), str(rom), str(symbols), "LEVEL7_2", "10000", str(prefix),
                      "source", str(args.updates), "first-transfer"], capture_output=True, text=True, timeout=600)
log = run.stdout + run.stderr
(output / "run.log").write_text(log)
if run.returncode not in (0, 1) or "COUNTERFACTUAL:" not in log:
    raise RuntimeError(f"Diagnostic failed to run: {run.returncode}: {log}")
if run.returncode and "Gameplay state differs from native execution" not in log:
    raise RuntimeError(f"Diagnostic failed before a valid comparison: {log}")
def rows(kind):
    with Path(str(prefix) + '-' + kind + '.csv').open(newline='') as stream:
        return list(csv.DictReader(stream))
frames = rows("gameplay")
differences = rows("gameplay-differences")
if run.returncode == 0 and (len(frames) != args.updates or differences):
    raise RuntimeError("Counterfactual completion lacks the requested matching updates")
counts = {name: int(count) for name, count in re.findall(r"ZEROED (\S+) (\d+)", log)}
if not counts:
    raise RuntimeError("Counterfactual did not exercise either native read")
observed = Counter(row["strategy"] for row in rows("transfer-reads"))
if counts != dict(observed):
    raise RuntimeError(f"Substitutions do not match observed load executions: {counts}: {observed}")
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest().upper()
report = {"counterfactual_only": True, "host_parity_passed": False,
          "requested_updates": args.updates, "observed_updates": len(frames),
          "counterfactual_completed_without_difference": run.returncode == 0,
          "comparisons": sum(int(r["comparisons"]) for r in frames),
          "substituted_native_reads": counts, "differences": differences,
          "executable_sha256": sha(exe), "source_sha256": sha(source),
          "rom_sha256": sha(rom), "symbols_sha256": sha(symbols),
          "scope": "Replace loaded A with zero and set N=0/Z=1 after the two word loads, without changing the load's instruction timing, and with one initial RAM seed. Not production behavior or a parity fix."}
(output / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
