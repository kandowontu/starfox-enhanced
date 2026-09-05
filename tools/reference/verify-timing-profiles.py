"""Check diagnostic GSU policies and safe stage entry in the full SNES reference.

Inputs and the optional Ares executable remain local development dependencies.
These bounded, neutral-input runs do not establish physical cartridge timing.
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
    parser.add_argument("--output", type=Path, default=root / "tmp/timing-profile-audit")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    inputs = {
        "original": (args.original_rom.resolve(), args.original_symbols.resolve()),
        "ex": (args.ex_rom.resolve(), args.ex_symbols.resolve()),
    }
    cases = [(variant, "LEVEL2_1", policy) for variant in inputs
             for policy in ("source", "divided-standard", "divided-fast")]
    cases += [(variant, "LEVEL3_1", "source") for variant in inputs]

    def run(case):
        variant, stage, policy = case
        rom, symbols = inputs[variant]
        prefix = args.output / f"{variant}-{stage}-{policy}"
        command = [str(args.executable.resolve()), str(rom), str(symbols), stage,
                   "1200", str(prefix.resolve()), policy]
        result = subprocess.run(command, capture_output=True, text=True, timeout=600)
        Path(str(prefix) + ".log").write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f"{case}: exit {result.returncode}; see {prefix}.log")
        paths = {kind: Path(str(prefix) + f"-{kind}.csv")
                 for kind in ("entry", "registers", "frames", "camera", "gsu")}
        entries, writes, frames, camera, gsu = (rows(paths[k]) for k in paths)
        assert len(entries) == 1 and entries[0]["map"] == stage, case
        assert entries[0]["policy"] == policy, case
        assert int(entries[0]["gsu_running"]) == int(entries[0]["pending_ram_clocks"]) == 0, case
        assert len(frames) >= 100 and len(gsu) >= 500 and len(camera) >= 100 * 17, case
        assert all(row["host"] == row["native"] for row in camera), case
        assert writes, case
        for write in writes:
            location, requested, effective = (int(write[k]) for k in ("register", "requested", "effective"))
            assert location in (0x3037, 0x3039), case
            expected = requested
            if policy != "source":
                if location == 0x3039:
                    expected &= ~1
                elif policy == "divided-standard":
                    expected &= ~0x20
                else:
                    expected |= 0x20
            assert effective == expected, (case, write)
        for launch in gsu:
            if policy != "source":
                assert int(launch["clsr"]) == 0, case
                assert bool(int(launch["cfgr"]) & 0x20) == (policy == "divided-fast"), case
        # Compare the same opening source updates; exclude loading and the
        # initial GAMEFRAME value inherited from the interrupted frontend.
        start = next(i for i, row in enumerate(frames) if int(row["game_frame"]) == 0)
        opening = frames[start:start + 41]
        assert [int(row["game_frame"]) for row in opening] == list(range(41)), case
        opening_clocks = sum(int(row["master_clocks"]) for row in opening[1:])
        state_fields = ("game_frame", "active", "view_z", "map", "player_x", "player_y", "player_z", "draw")
        opening_state = [[row[field] for field in state_fields] for row in opening]
        report = {
            "variant": variant, "map": stage, "policy": policy,
            "exit_code": result.returncode, "stage_entry_video_frame": int(entries[0]["video_frame"]),
            "camera_calls": len(camera) // 17, "camera_differences": 0,
            "frame_boundaries": len(frames), "gsu_intervals": len(gsu),
            "register_writes": len(writes),
            "overridden_register_writes": sum(row["requested"] != row["effective"] for row in writes),
            "opening_updates_1_to_40_master_clocks": opening_clocks,
            "opening_state_sha256": hashlib.sha256(json.dumps(opening_state).encode()).hexdigest().upper(),
            "trace_sha256": {kind: sha256(path) for kind, path in paths.items()},
        }
        print(f"{variant} {stage} {policy}: {report['camera_calls']} camera calls, safe entry, policies verified", flush=True)
        return report

    with ThreadPoolExecutor(max_workers=2) as pool:
        reports = list(pool.map(run, cases))
    ex = {r["policy"]: r for r in reports if r["variant"] == "ex" and r["map"] == "LEVEL2_1"}
    for kind in ("registers", "frames", "camera", "gsu"):
        assert ex["source"]["trace_sha256"][kind] == ex["divided-fast"]["trace_sha256"][kind], kind
    for variant in inputs:
        policies = {r["policy"]: r for r in reports if r["variant"] == variant and r["map"] == "LEVEL2_1"}
        assert len({r["opening_state_sha256"] for r in policies.values()}) == 1, variant
        assert policies["divided-standard"]["opening_updates_1_to_40_master_clocks"] > policies["source"]["opening_updates_1_to_40_master_clocks"], variant
    summary = {
        "ares_revision": "0aafd85789215e84e1e43415c07d4c88461b7899",
        "executable_sha256": sha256(args.executable),
        "inputs": {v: {"rom_sha256": sha256(r), "symbols_sha256": sha256(s)} for v, (r, s) in inputs.items()},
        "boot_video_frames": 600, "stage_video_frames": 1200,
        "entry": "Next TRANSFER_L instruction boundary with GSU stopped and RAM write buffer empty; then GAMESTART",
        "ex_source_and_divided_fast_traces_identical": True,
        "opening_state_matches_across_policies": True,
        "cases": reports,
        "limits": ["Generic Ares GSU, not a physical MC1 revision", "Reconstructed inputs, not stock retail ROMs",
                   "Neutral input and PSHIPFLAGS3 bit 3; not complete campaigns",
                   "Clock policy changes can change preceding frontend state",
                   "No production timing change or full-scene pixel certification"],
    }
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Verified {len(reports)} reference runs; summary: {args.output / 'summary.json'}")


if __name__ == "__main__":
    main()
