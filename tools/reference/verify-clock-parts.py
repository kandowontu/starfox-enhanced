"""Validate disjoint reference clock accounting, without certifying host parity."""
import argparse
import csv
import hashlib
import json
from pathlib import Path


def validate(prefix, work=False):
    prefix = str(prefix)
    paths = [Path(prefix + suffix) for suffix in
             ("-clock-work.csv" if work else "-clock-parts.csv", "-transfer-phases.csv")]
    rows, phases = [list(csv.DictReader(path.open(newline=""))) for path in paths]
    if not rows or len(rows) != len(phases):
        raise ValueError("Clock accounting must cover every reference phase")
    totals = [0, 0, 0]
    last = None
    for row, phase in zip(rows, phases):
        for field in ("video_frame", "game_frame", "phase", "transfer_clocks"):
            if row[field] != phase[field]:
                raise ValueError(f"Clock accounting phase differs: {field}")
        elapsed = int(row["transfer_clocks"])
        parts = [int(row[key]) for key in
                 ("cpu_clocks", "dma_work_clocks" if work else "dma_active_clocks", "refresh_clocks")]
        if elapsed < 0 or min(parts) < 0 or sum(parts) != elapsed:
            raise ValueError("Clock categories do not partition elapsed master clocks")
        if parts[2] % 40:
            raise ValueError("Refresh clocks do not contain complete source refresh sequences")
        if row["phase"] == "settled":
            if elapsed or any(parts):
                raise ValueError("Settled transfer did not reset accounting")
            last = [0, 0, 0]
        else:
            if last is None or any(a < b for a, b in zip(parts, last)):
                raise ValueError("Clock categories regressed within a transfer")
            last = parts
        totals = [max(a, b) for a, b in zip(totals, parts)]
    if not all(totals):
        raise ValueError("Trace did not exercise every clock category")
    return {
        "prefix": prefix, "phase_rows": len(rows), "accounting_passed": True,
        "maximum_clocks_per_category": dict(zip(
            ("cpu", "dma_work" if work else "dma_active", "refresh"), totals)),
        "sha256": {path.name: hashlib.sha256(path.read_bytes()).hexdigest().upper()
                   for path in paths},
        "scope": ("Reference elapsed-clock partition only. "
                  + ("DMA work is scoped to dmaEdge execution, including nested DMA/HDMA and alignment. "
                     if work else "DMA-active includes arbitration and alignment and CPU cycles while DMA is pending. ")
                  + "CPU includes wait-loop instructions. Not host parity or hardware certification."),
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prefix", nargs="+", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--work", action="store_true", help="Validate execution-scoped DMA clocks")
    args = parser.parse_args()
    result = [validate(prefix, args.work) for prefix in args.prefix]
    text = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(text)
    print(text, end="")
