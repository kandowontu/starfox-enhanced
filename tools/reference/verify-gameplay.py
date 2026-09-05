"""Compare 300 consecutive neutral-input updates in routes 2/3 of both games.

This bounded audit seeds WRAM once and supplies observed raster counts; it
does not validate boot state, input handling, host pace or complete campaigns.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import csv
import hashlib
import json
from pathlib import Path
import subprocess


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def rows(path):
    with Path(path).open(newline="") as stream:
        return list(csv.DictReader(stream))


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=root / "tmp/full-reference-build/full_reference.exe")
    parser.add_argument("--original-rom", type=Path, default=root / "upstream-ultrastarfox/SF.SFC")
    parser.add_argument("--original-symbols", type=Path, default=root / "upstream-ultrastarfox/SYMBOLS.TXT")
    parser.add_argument("--ex-rom", type=Path, default=root / "tmp/runtime-inputs/starfox-ex/SFES.SFC")
    parser.add_argument("--ex-symbols", type=Path, default=root / "assets/symbols/starfox-ex.txt")
    parser.add_argument("--output", type=Path, default=root / "tmp/gameplay-audit")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    inputs = {"original": (args.original_rom.resolve(), args.original_symbols.resolve()),
              "ex": (args.ex_rom.resolve(), args.ex_symbols.resolve())}

    def run(case):
        variant, stage = case
        rom, symbols = inputs[variant]
        prefix = args.output.resolve() / f"{variant}-{stage}"
        command = [str(args.executable.resolve()), str(rom), str(symbols), stage,
                   "3600", str(prefix), "source", "300"]
        result = subprocess.run(command, capture_output=True, text=True, timeout=600)
        Path(str(prefix) + ".log").write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f"{case}: exit {result.returncode}; see {prefix}.log")
        paths = {kind: Path(str(prefix) + f"-{kind}.csv")
                 for kind in ("entry", "gameplay", "gameplay-differences", "camera")}
        entry, gameplay, differences, camera = (rows(paths[k]) for k in paths)
        if (len(entry) != 1 or entry[0]["map"] != stage
                or int(entry[0]["gsu_running"]) or int(entry[0]["pending_ram_clocks"])):
            raise RuntimeError(f"{case}: unsafe or missing stage entry")
        if ([int(row["transfer"]) for row in gameplay] != list(range(1, 301))
                or differences or any(int(row["differences"]) for row in gameplay)):
            raise RuntimeError(f"{case}: incomplete or differing gameplay state")
        if (not camera or any(row["host"] != row["native"] for row in camera)
                or any(int(row["comparisons"]) <= 25 for row in gameplay)):
            raise RuntimeError(f"{case}: missing object/camera comparisons")
        report = {
            "variant": variant, "map": stage, "updates": len(gameplay),
            "comparisons": sum(int(row["comparisons"]) for row in gameplay),
            "differences": 0, "camera_calls": len(camera) // 17,
            "raster_phase_range": [min(int(row["raster_phases"]) for row in gameplay),
                                   max(int(row["raster_phases"]) for row in gameplay)],
            "host_minimum_phase_updates": sum(row["raster_phases"] != row["host_raster_phases"] for row in gameplay),
            "trace_sha256": {kind: sha256(path) for kind, path in paths.items()},
        }
        print(f"{variant} {stage}: 300 updates, {report['comparisons']} comparisons, zero differences", flush=True)
        return report

    with ThreadPoolExecutor(max_workers=2) as pool:
        reports = list(pool.map(run, [(variant, stage) for variant in inputs
                                     for stage in ("LEVEL2_1", "LEVEL3_1")]))
    # An otherwise successful reference run must fail when too few source
    # updates occur. This catches the original probe's silent partial pass.
    prefix = args.output.resolve() / "incomplete"
    rom, symbols = inputs["original"]
    result = subprocess.run([str(args.executable.resolve()), str(rom), str(symbols),
        "LEVEL2_1", "120", str(prefix), "source", "300"],
        capture_output=True, text=True, timeout=600)
    Path(str(prefix) + ".log").write_text(result.stdout + result.stderr)
    if result.returncode == 0 or "did not reach its required update count" not in result.stderr:
        raise RuntimeError("Incomplete gameplay comparison was not rejected")
    summary = {
        "ares_revision": "0aafd85789215e84e1e43415c07d4c88461b7899",
        "reference_executable_sha256": sha256(args.executable),
        "inputs": {variant: {"rom_sha256": sha256(rom), "symbols_sha256": sha256(symbols)}
                   for variant, (rom, symbols) in inputs.items()},
        "updates": sum(report["updates"] for report in reports),
        "comparisons": sum(report["comparisons"] for report in reports),
        "differences": 0, "incomplete_run_rejected": True, "cases": reports,
        "scope": "One WRAM seed; observed native raster counts with the host's three-raster minimum still applied; neutral input; selected RAM fields and active object bytes only. No whole-game, rendering, input or physical timing certification.",
    }
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("All four bounded comparisons passed; incomplete-run guard passed.", flush=True)


if __name__ == "__main__":
    main()
