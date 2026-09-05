"""Validate the actual desktop mix trace after the complete ending fixture."""
import csv
import json
import sys
from pathlib import Path

if len(sys.argv) != 4:
    raise SystemExit("Usage: check-runtime-ending-audio.py AUDIO.csv original|ex|msu OUTPUT.json")
path, variant, output = Path(sys.argv[1]), sys.argv[2], Path(sys.argv[3])
if variant not in ("original", "ex", "msu"):
    raise SystemExit("Unknown variant")
rows = list(csv.DictReader(path.open(newline="", encoding="utf-8")))
assert rows and len(rows) >= 30000, "Incomplete 1,500-second desktop audio trace"
assert all(row["mixed_peak"] is not None for row in rows), "Incomplete trace row"
assert all(row["msu_enabled"] == ("1" if variant == "msu" else "0") for row in rows)

quiet_since = None
gaps = []
for i, row in enumerate(rows):
    seconds = float(row["seconds"])
    if int(row["music_peak"]) <= 8:
        if quiet_since is None:
            quiet_since = seconds
    else:
        if quiet_since is not None and seconds - quiet_since > 100:
            gaps.append((quiet_since, seconds, i))
        quiet_since = None
assert len(gaps) == 1, f"Expected exactly one delayed music reprise: {gaps}"
start, reprise, index = gaps[0]
expected_silence = 570.2 if variant == "msu" else 591.4
assert abs(reprise - start - expected_silence) < 1, "Wrong credits silence interval"
assert any(int(row["music_peak"]) > 100 and int(row["mixed_peak"]) > 100
           for row in rows[index:index + 1000]), "Jingle did not reach the actual mixed output"
last = max(float(row["seconds"]) for row in rows if int(row["music_peak"]) > 8)
assert 40 <= last - reprise <= 80, "Jingle missing or truncated"
if variant == "msu":
    assert all(row["native_tail"] == "1" for row in rows[index:]), "MSU lost the native tail"
    assert all(row["msu_track"] == "49" for row in rows[index:]), "Wrong MSU staff-roll track"

summary = dict(variant=variant, audio_batches=len(rows), seconds=float(rows[-1]["seconds"]),
               silence_start=start, jingle_start=reprise, jingle_end=last,
               silence_seconds=round(reprise-start, 2), mixed_output_verified=True,
               msu_native_tail=variant == "msu")
output.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
print(json.dumps(summary))
