"""Re-run the pinned isolated GSU audits and compare with Snes9x numeric results.

Both engines use the same Snes9x boot SRAM; only GSU execution is independent.
Raw outputs stay in the requested directory. No cartridge/core is redistributed.
"""
import argparse
import collections
import csv
import hashlib
import json
from pathlib import Path
import statistics
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def rows(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-directory", type=Path, default=ROOT / "build/current")
    parser.add_argument("--core", type=Path,
                        default=ROOT / "tmp/reference-snes9x-clean/libretro/snes9x_libretro.dll")
    parser.add_argument("--ares-source", type=Path, default=ROOT / "tmp/ares-timing")
    parser.add_argument("--output-directory", type=Path, default=ROOT / "tmp/ares-audit")
    args = parser.parse_args()
    for name in ("build_directory", "core", "ares_source", "output_directory"):
        setattr(args, name, getattr(args, name).resolve())
    cache = (args.build_directory / "CMakeCache.txt").read_text()
    configured = next((line.split("=", 1)[1] for line in cache.splitlines()
                       if line.startswith("STARFOX_REFERENCE_ARES_DIR:PATH=")), None)
    if not configured or Path(configured).resolve() != args.ares_source:
        raise RuntimeError("Build uses a different Ares source directory")
    revision = subprocess.check_output(["git", "-C", str(args.ares_source), "rev-parse", "HEAD"], text=True).strip()
    if revision != "0aafd85789215e84e1e43415c07d4c88461b7899":
        raise RuntimeError("Ares revision differs from the audited pin")
    if subprocess.check_output(["git", "-C", str(args.ares_source), "status", "--porcelain", "--untracked-files=all"], text=True).strip():
        raise RuntimeError("Ares checkout has local edits")
    args.output_directory.mkdir(parents=True, exist_ok=True)
    exe = args.build_directory / "starfox_reference_ares_render.exe"
    subprocess.run([str(args.build_directory / "starfox_reference_ares_tests.exe")], check=True)
    report = {"ares_revision": revision, "tool_sha256": digest(exe),
              "bootstrap_core_sha256": digest(args.core), "cold_cache": True,
              "clsr": 0, "cfgr": 0, "cpu_bus_contention": False, "video_dma": False,
              "cases": [], "compared_rows": 0, "isolated_calls": 0}
    for game, rom, symbols in (
        ("original", ROOT / "upstream-ultrastarfox/SF.SFC", ROOT / "upstream-ultrastarfox/SYMBOLS.TXT"),
        ("ex", ROOT / "tmp/runtime-inputs/starfox-ex/SFES.SFC", ROOT / "assets/symbols/starfox-ex.txt"),
    ):
        for mode in ("render", "sprites", "matrices", "points", "dust", "grid"):
            golden = (ROOT / f"docs/validation/reference-render-{game}-expanded.csv" if mode == "render"
                      else ROOT / f"docs/validation/reference-sprites-{game}.csv" if mode == "sprites"
                      else ROOT / f"tests/data/reference-{mode}-{game}.csv")
            expected = rows(golden)
            command = "all-frames" if mode == "render" else mode
            if mode == "sprites":
                command = "sprites:" + ",".join(dict.fromkeys(row["shape"] for row in expected))
            output = args.output_directory / f"{mode}-{game}.csv"
            timing = args.output_directory / f"{mode}-{game}-clocks.csv"
            subprocess.run([str(exe), str(args.core), str(rom), str(symbols), command,
                            str(output), str(timing)], check=True, cwd=ROOT)
            actual = rows(output)
            if len(actual) != len(expected):
                raise RuntimeError(f"{game}/{mode}: row count differs")
            # Census records include known port differences. Compare native
            # results, never discard those rows to make the oracle agree.
            if mode in ("render", "sprites"):
                fields = [name for name in expected[0] if not name.startswith("port_")
                          and name not in ("different_pixels", "mask_difference")]
            else:
                fields = list(expected[0])
            for index, (want, got) in enumerate(zip(expected, actual)):
                changed = [name for name in fields if want[name] != got[name]]
                if changed:
                    raise RuntimeError(f"{game}/{mode} row {index}: {changed} differ")
            calls = rows(timing)
            by_address = collections.defaultdict(list)
            for index, call in enumerate(calls):
                if int(call["call"]) != index or int(call["master_clocks"]) <= 0:
                    raise RuntimeError("Missing or invalid cycle trace row")
                by_address[int(call["address"])].append(int(call["master_clocks"]))
            entry = {"game": game, "mode": mode, "rows": len(actual), "calls": len(calls),
                     "oracle_differences": 0, "rom_sha256": digest(rom),
                     "symbols_sha256": digest(symbols), "golden_sha256": digest(golden),
                     "output_sha256": digest(output), "timing_sha256": digest(timing),
                     "port_different_rows": sum(int(row.get("different_pixels", "0")) != 0 for row in actual),
                     "routine_clocks": [{"address": address, "calls": len(values),
                                         "min": min(values), "max": max(values),
                                         "median": statistics.median(values)}
                                        for address, values in sorted(by_address.items())]}
            report["cases"].append(entry)
            report["compared_rows"] += len(actual)
            report["isolated_calls"] += len(calls)
            print(f"{game}/{mode}: {len(actual)} rows agree, {len(calls)} cycle measurements", flush=True)
    (args.output_directory / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"All {report['compared_rows']} rows agree; {report['isolated_calls']} isolated calls measured.")


if __name__ == "__main__":
    main()
