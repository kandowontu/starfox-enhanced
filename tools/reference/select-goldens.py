"""Retain independently matching cases; preserve all failures in audit CSVs."""
import csv
from pathlib import Path

root = Path(__file__).resolve().parents[2]
for variant in ("original", "ex"):
    selected = {}
    for kind in ("census", "scenarios", "followup", "expanded"):
        source = root / "docs/validation" / f"reference-render-{variant}-{kind}.csv"
        if not source.exists():
            continue
        with source.open(newline="", encoding="utf-8") as handle:
            reader = csv.DictReader(handle)
            columns = reader.fieldnames
            for row in reader:
                if (row["different_pixels"] == "0"
                        and row["native_hash"] == row["port_hash"]):
                    key = tuple(row[name] for name in ("address", "frame", "yaw", "depth"))
                    selected[key] = row
    destination = root / "tests/data" / f"reference-render-{variant}.csv"
    with destination.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, columns, lineterminator="\n")
        writer.writeheader()
        writer.writerows(sorted(selected.values(), key=lambda row: (
            row["shape"], int(row["frame"]), int(row["yaw"]), int(row["depth"]))))
    visible = sum(int(row["native_pixels"]) > 0 for row in selected.values())
    print(f"{variant}: {len(selected)} reference cases ({visible} visible); failures remain in docs/validation")
