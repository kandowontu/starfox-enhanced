"""Compare measured CPU work in selected strategy traces, without certifying parity."""
import argparse
from collections import Counter, defaultdict
import csv
from difflib import SequenceMatcher
import hashlib
import json
from pathlib import Path


def compare(path):
    groups = defaultdict(list)
    with path.open(newline="") as stream:
        for row in csv.DictReader(stream):
            sequence = groups[row["engine"]]
            item = {key: int(value) for key, value in row.items() if key != "engine"}
            # Pausing and resuming at a boundary can report it twice.
            if sequence and (item["pc"], item["work_clocks"]) == (
                    sequence[-1]["pc"], sequence[-1]["work_clocks"]):
                continue
            sequence.append(item)
    if set(groups) != {"native-instruction", "host-boundary"}:
        raise ValueError("Trace must contain both instruction engines")
    native, host = [groups[key] for key in ("native-instruction", "host-boundary")]
    for sequence in (native, host):
        if len(sequence) < 2 or sequence[-1]["endpoint"] != 1:
            raise ValueError("Trace must finish at GETVIEW_L")
        if any(row["endpoint"] for row in sequence[:-1]):
            raise ValueError("Trace contains multiple execution intervals")
        if any(b["work_clocks"] < a["work_clocks"] for a, b in zip(sequence, sequence[1:])):
            raise ValueError("CPU work clocks regressed")
    differences = Counter()
    unmatched = []
    matcher = SequenceMatcher(None, [row["pc"] for row in native],
                              [row["pc"] for row in host], autojunk=False)
    for tag, a, b, c, d in matcher.get_opcodes():
        if tag != "equal":
            unmatched.append({"native": [hex(row["pc"]) for row in native[a:b]],
                              "host": [hex(row["pc"]) for row in host[c:d]]})
            continue
        for i, j in zip(range(a, b), range(c, d)):
            if i + 1 == len(native) or j + 1 == len(host):
                continue
            nc = native[i + 1]["work_clocks"] - native[i]["work_clocks"]
            hc = host[j + 1]["work_clocks"] - host[j]["work_clocks"]
            if nc != hc:
                differences[(native[i]["pc"], nc, hc)] += 1
    return {
        "trace": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest().upper(),
        "native_clocks": native[-1]["work_clocks"] - native[0]["work_clocks"],
        "host_clocks": host[-1]["work_clocks"] - host[0]["work_clocks"],
        "matched_timing_differences": [
            {"pc": hex(pc), "native": nc, "host": hc, "count": count}
            for (pc, nc, hc), count in differences.items()],
        "unmatched": unmatched,
        "scope": "PC sequence alignment and scoped CPU work only. Excludes DMA and refresh. "
                 "Unmatched instructions and patched code require separate interpretation; "
                 "this report does not assert gameplay, scheduler or physical timing parity.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path, nargs="+")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = json.dumps([compare(path) for path in args.trace], indent=2) + "\n"
    if args.output:
        args.output.write_text(result)
    print(result, end="")
