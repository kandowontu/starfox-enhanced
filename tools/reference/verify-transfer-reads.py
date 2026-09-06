"""Check both EX LEVEL7_2 live transfer-word readers in a native trace.

This is reference evidence only: it does not certify or modify host gameplay.
Run full_reference with LEVEL7_2, 5000 video frames, source policy and no
GAMEPLAY_UPDATES argument, then pass its output prefix here.
"""
import argparse
from collections import Counter
import csv
import hashlib
import json
from pathlib import Path


def inspect(trace):
    result = {}
    for strategy in ("SCORPION1_STRAT", "SCORPION4_STRAT"):
        selected = [row for row in trace if row["strategy"] == strategy]
        if not selected:
            raise ValueError(f"Missing live reader: {strategy}")
        if any(int(row["direct"]) != 0 or int(row["status"]) & 0x20 for row in selected):
            raise ValueError(f"Reader did not load the direct-page transfer word: {strategy}")
        counts = Counter(row["target"] for row in selected)
        if set(counts) != {"2", "4"}:
            raise ValueError(f"Expected both observed transfer phases: {strategy}: {counts}")
        result[strategy] = {"reads": len(selected), "transfer_words": dict(sorted(counts.items())),
                            "objects": sorted({int(row["object"]) for row in selected}),
                            "instruction_addresses": sorted({int(row["pc"]) for row in selected})}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prefix", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    path = Path(str(args.prefix) + "-transfer-reads.csv")
    with path.open(newline="") as stream:
        trace = list(csv.DictReader(stream))
    report = inspect(trace)
    # Negative controls ensure missing-reader and constant-phase traces cannot
    # be mistaken for evidence covering both timing-dependent strategies.
    for altered in ([r for r in trace if r["strategy"] != "SCORPION1_STRAT"],
                    [dict(r, target="2") for r in trace]):
        try:
            inspect(altered)
        except ValueError:
            continue
        raise RuntimeError("Transfer-read negative control was accepted")
    summary = {"readers": report, "negative_controls_rejected": 2,
               "trace_sha256": hashlib.sha256(path.read_bytes()).hexdigest().upper(),
               "scope": "Native-only changing transfer-word reads; no passing host gameplay claim."}
    args.output.write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
