"""Compare consecutive neutral-input updates in selected stages of both games.

This bounded audit seeds WRAM once and supplies observed raster counts; it
does not validate boot state, input handling, host pace or complete campaigns.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import csv
import hashlib
import json
from pathlib import Path
import re
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
    parser.add_argument("--updates", type=int, default=1000)
    parser.add_argument("--video-frames", type=int, default=10000)
    parser.add_argument("--all-stages", action="store_true",
                        help="Check every numbered LEVEL symbol instead of the four route-2/3 openings")
    parser.add_argument("--case", action="append", dest="selected_cases", metavar="VARIANT:LEVEL",
                        help="Repeat to select numbered stages, for example ex:LEVEL7_2; uses first-transfer by default")
    parser.add_argument("--seed-mode", choices=("zero-frame", "first-transfer"),
                        help="Defaults to first-transfer for --all-stages, otherwise zero-frame")
    args = parser.parse_args()
    if not 1 <= args.updates <= 10000:
        parser.error("--updates must be 1..10000")
    if not 120 <= args.video_frames <= 100000:
        parser.error("--video-frames must be 120..100000")
    if args.all_stages and args.selected_cases:
        parser.error("--all-stages and --case are mutually exclusive")
    seed_mode = args.seed_mode or ("first-transfer" if args.all_stages or args.selected_cases else "zero-frame")
    args.output.mkdir(parents=True, exist_ok=True)
    inputs = {"original": (args.original_rom.resolve(), args.original_symbols.resolve()),
              "ex": (args.ex_rom.resolve(), args.ex_symbols.resolve())}

    def run(case):
        variant, stage = case
        rom, symbols = inputs[variant]
        prefix = args.output.resolve() / f"{variant}-{stage}"
        command = [str(args.executable.resolve()), str(rom), str(symbols), stage,
                   str(args.video_frames), str(prefix), "source", str(args.updates), seed_mode]
        result = subprocess.run(command, capture_output=True, text=True, timeout=600)
        Path(str(prefix) + ".log").write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f"{case}: exit {result.returncode}; see {prefix}.log")
        paths = {kind: Path(str(prefix) + f"-{kind}.csv")
                 for kind in ("entry", "gameplay", "gameplay-differences", "camera", "gameplay-seed")}
        entry, gameplay, differences, camera, seed = (rows(paths[k]) for k in paths)
        if (len(entry) != 1 or entry[0]["map"] != stage
                or int(entry[0]["gsu_running"]) or int(entry[0]["pending_ram_clocks"])):
            raise RuntimeError(f"{case}: unsafe or missing stage entry")
        if (len(seed) != 1 or seed[0]["mode"] != seed_mode
                or (seed_mode == "zero-frame" and int(seed[0]["gameframe"]) != 0)):
            raise RuntimeError(f"{case}: missing or incorrect gameplay seed")
        if ([int(row["transfer"]) for row in gameplay] != list(range(1, args.updates + 1))
                or differences or any(int(row["differences"]) for row in gameplay)):
            raise RuntimeError(f"{case}: incomplete or differing gameplay state")
        if (not camera or any(row["host"] != row["native"] for row in camera)
                or any(int(row["comparisons"]) <= 25 for row in gameplay)
                or not sum(int(row["submitted_flags"]) for row in gameplay)):
            raise RuntimeError(f"{case}: missing object/camera comparisons")
        report = {
            "variant": variant, "map": stage, "passed": True, "updates": len(gameplay),
            "seed_mode": seed_mode, "seed_gameframe": int(seed[0]["gameframe"]),
            "comparisons": sum(int(row["comparisons"]) for row in gameplay),
            "differences": 0, "camera_calls": len(camera) // 17,
            "submitted_flags": sum(int(row["submitted_flags"]) for row in gameplay),
            "hitflashes": sum(int(row["hitflashes"]) for row in gameplay),
            "raster_phase_range": [min(int(row["raster_phases"]) for row in gameplay),
                                   max(int(row["raster_phases"]) for row in gameplay)],
            "host_minimum_phase_updates": sum(row["raster_phases"] != row["host_raster_phases"] for row in gameplay),
            "trace_sha256": {kind: sha256(path) for kind, path in paths.items()},
        }
        print(f"{variant} {stage}: {args.updates} updates, {report['comparisons']} comparisons, zero differences", flush=True)
        return report

    cases = []
    for variant, (_, symbols) in inputs.items():
        stages = ("LEVEL2_1", "LEVEL3_1")
        if args.all_stages or args.selected_cases:
            stages = sorted(set(re.findall(r"^(LEVEL[1-7]_[1-9][0-9]*)\s+\$", symbols.read_text(), re.MULTILINE)),
                            key=lambda stage: tuple(map(int, stage[5:].split("_"))))
            if not stages:
                raise RuntimeError(f"No numbered stage symbols in {symbols}")
        cases.extend((variant, stage) for stage in stages)
    if args.selected_cases:
        requested = {tuple(case.split(":")) for case in args.selected_cases}
        unknown = requested - set(cases)
        if unknown:
            parser.error("Unknown --case selection: " + ", ".join(":".join(case) for case in sorted(unknown)))
        cases = [case for case in cases if case in requested]

    def run_recorded(case):
        try:
            return run(case)
        except Exception as error:
            # Keep every failing case visible while collecting the remaining
            # independent stages. A nonzero exit still makes the audit fail.
            print(f"{case[0]} {case[1]}: FAILED: {error}", flush=True)
            return {"variant": case[0], "map": case[1], "passed": False, "error": str(error)}

    with ThreadPoolExecutor(max_workers=2) as pool:
        reports = list(pool.map(run_recorded, cases))
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
        "passed": all(report["passed"] for report in reports),
        "requested_cases": len(cases), "requested_updates_per_case": args.updates,
        "passed_cases": sum(report["passed"] for report in reports),
        "updates": sum(report.get("updates", 0) for report in reports),
        "comparisons": sum(report.get("comparisons", 0) for report in reports),
        "differences": 0 if all(report["passed"] for report in reports) else None,
        "incomplete_run_rejected": True, "cases": reports,
        "scope": "One WRAM seed; observed native raster counts with the host's three-raster minimum still applied; neutral input; selected RAM fields and active object bytes only. No whole-game, rendering, input or physical timing certification.",
    }
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    if not summary["passed"]:
        raise RuntimeError(f"{len(cases) - summary['passed_cases']} of {len(cases)} stage comparisons failed; see summary.json")
    print(f"All {len(cases)} bounded comparisons passed; incomplete-run guard passed.", flush=True)


if __name__ == "__main__":
    main()
