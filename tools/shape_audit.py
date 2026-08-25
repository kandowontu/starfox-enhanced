#!/usr/bin/env python3
"""Inventory UltraStarFox's canonical assembler model data.

This tool deliberately preserves assembler expressions as strings. It is an
audit/extraction boundary, not a second assembler: the future binary converter
will consume the assembled data so conditional macros and symbol arithmetic
remain byte-exact.
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Iterable


HEADER_OPS = {"shapehdr", "shapehdr_s"}
POINT_BLOCK_OPS = {"pointsb", "pointsw", "pointsxb", "pointsxw"}
POINT_ROW_OPS = {"pb", "pw", "pbd2", "pby2"}
CONTROL_OPS = {
    "datahdr",
    "frames",
    "jumptab",
    "jump",
    "endpoints",
    "faces",
    "fend",
    "fendq",
    "bsp",
    "bspe",
    "bspinit",
    "bspend",
    "bspnull",
    "endshape",
    "viz",
    "vizis",
    "s_sprite",
    "s_spritevis",
}
FACE_RE = re.compile(r"^face(?P<count>\d+)$", re.IGNORECASE)
WIRE_FACE_RE = re.compile(r"^aface(?P<count>[34])$", re.IGNORECASE)
LABEL_RE = re.compile(r"^[A-Za-z_.][A-Za-z0-9_.$@]*:?$")
ASSEMBLER_DIRECTIVES = {
    "else",
    "elseif",
    "endc",
    "endm",
    "endr",
    "ifeq",
    "ifge",
    "ifgt",
    "ifle",
    "iflt",
    "ifnc",
    "ifnd",
    "ifne",
    "mexit",
    "rept",
}


@dataclass
class SourceLocation:
    file: str
    line: int


@dataclass
class ShapeHeader:
    name: str
    points: str
    faces: str
    shift: str | None
    radius: str | None
    bounds: list[str]
    collision: str | None
    shadow: str | None
    lods: list[str]
    source: SourceLocation
    raw_arguments: list[str] = field(repr=False)


@dataclass
class PointBlock:
    label: str
    encoding: str
    declared_count: str
    rows: int
    source: SourceLocation


@dataclass
class FaceGroup:
    label: str
    declared_count: str | None
    faces: int
    expanded_wire_edges: int
    source: SourceLocation


@dataclass
class AuditIssue:
    severity: str
    message: str
    source: SourceLocation


@dataclass
class ShapeAudit:
    source_root: str
    files: int = 0
    headers: list[ShapeHeader] = field(default_factory=list)
    point_blocks: list[PointBlock] = field(default_factory=list)
    face_groups: list[FaceGroup] = field(default_factory=list)
    point_rows: int = 0
    polygon_rows: int = 0
    expanded_wire_edges: int = 0
    visibility_rows: int = 0
    animated_tables: int = 0
    issues: list[AuditIssue] = field(default_factory=list)

    def to_dict(self) -> dict[str, object]:
        result = asdict(self)
        result["summary"] = {
            "files": self.files,
            "shape_headers": len(self.headers),
            "point_blocks": len(self.point_blocks),
            "point_rows": self.point_rows,
            "face_groups": len(self.face_groups),
            "polygon_rows": self.polygon_rows,
            "expanded_wire_edges": self.expanded_wire_edges,
            "visibility_rows": self.visibility_rows,
            "animated_tables": self.animated_tables,
            "errors": sum(issue.severity == "error" for issue in self.issues),
            "warnings": sum(issue.severity == "warning" for issue in self.issues),
        }
        return result


def split_arguments(text: str) -> list[str]:
    """Split an assembler macro argument list without evaluating expressions."""
    arguments: list[str] = []
    current: list[str] = []
    depth = 0
    quote: str | None = None

    for character in text.strip():
        if quote:
            current.append(character)
            if character == quote:
                quote = None
            continue
        if character in {'"', "'"}:
            quote = character
            current.append(character)
        # Parentheses and square brackets group arithmetic. Angle brackets are
        # assembler unary/quoting syntax and, importantly, also appear as the
        # two characters in a left-shift expression (``<<``).
        elif character in "([":
            depth += 1
            current.append(character)
        elif character in ")]":
            depth = max(depth - 1, 0)
            current.append(character)
        elif character == "," and depth == 0:
            arguments.append("".join(current).strip())
            current = []
        else:
            current.append(character)

    if current or text.strip().endswith(","):
        arguments.append("".join(current).strip())
    return arguments


def _strip_comment(line: str) -> str:
    # Shape sources do not use semicolons inside quoted macro arguments.
    return line.split(";", 1)[0].strip()


def _operation(line: str) -> tuple[str | None, str | None, str]:
    """Return (label, operation, argument_text) for model-related source."""
    if not line:
        return None, None, ""

    parts = line.split(None, 2)
    first = parts[0].rstrip(":")
    first_lower = first.lower()
    known_first = (
        first_lower in HEADER_OPS
        or first_lower in POINT_BLOCK_OPS
        or first_lower in POINT_ROW_OPS
        or first_lower in CONTROL_OPS
        or FACE_RE.match(first_lower)
        or WIRE_FACE_RE.match(first_lower)
    )
    if known_first:
        return None, first_lower, line[len(parts[0]) :].strip()

    if len(parts) >= 2 and LABEL_RE.match(parts[0]):
        second = parts[1].lower()
        known_second = (
            second in HEADER_OPS
            or second in POINT_BLOCK_OPS
            or second in POINT_ROW_OPS
            or second in CONTROL_OPS
            or FACE_RE.match(second)
            or WIRE_FACE_RE.match(second)
        )
        if known_second:
            argument_text = parts[2] if len(parts) == 3 else ""
            return first, second, argument_text.strip()

    if (
        len(parts) == 1
        and first_lower not in ASSEMBLER_DIRECTIVES
        and LABEL_RE.match(parts[0])
    ):
        return first, None, ""
    return None, None, ""


def _header_from(
    label: str | None,
    arguments: list[str],
    location: SourceLocation,
) -> ShapeHeader | None:
    if label is None or len(arguments) < 3:
        return None

    # Full ShapeHdr fields:
    # points, unused, faces, unused, sort-z, unused, unused, shift, radius,
    # xmax, ymax, zmax, size, collision, shadow, simple1, simple2, simple3, name
    full = len(arguments) >= 14
    return ShapeHeader(
        name=label,
        points=arguments[0],
        faces=arguments[2],
        shift=arguments[7] if full and len(arguments) > 7 else None,
        radius=arguments[8] if full and len(arguments) > 8 else None,
        bounds=arguments[9:12] if full else [],
        collision=arguments[13] if full and len(arguments) > 13 else None,
        shadow=arguments[14] if full and len(arguments) > 14 else None,
        lods=arguments[15:18] if full else [],
        source=location,
        raw_arguments=arguments,
    )


def audit_shapes(shape_directory: Path) -> ShapeAudit:
    shape_directory = shape_directory.resolve()
    audit = ShapeAudit(source_root=str(shape_directory))
    files = sorted(shape_directory.glob("*.ASM")) + sorted(shape_directory.glob("*.asm"))
    # Avoid duplicates on case-insensitive filesystems.
    files = list(dict.fromkeys(path.resolve() for path in files))
    audit.files = len(files)

    for path in files:
        relative = path.relative_to(shape_directory).as_posix()
        current_label: str | None = None
        current_points: PointBlock | None = None
        current_faces: FaceGroup | None = None

        with path.open("r", encoding="latin-1") as source:
            for line_number, raw_line in enumerate(source, start=1):
                line = _strip_comment(raw_line)
                label, operation, argument_text = _operation(line)
                if label is not None:
                    current_label = label
                if operation is None:
                    continue

                location = SourceLocation(relative, line_number)
                arguments = split_arguments(argument_text)

                if operation in HEADER_OPS:
                    header = _header_from(label or current_label, arguments, location)
                    if header is None:
                        audit.issues.append(AuditIssue(
                            "error", "ShapeHdr is missing a label or required fields", location
                        ))
                    else:
                        audit.headers.append(header)
                    continue

                if operation in POINT_BLOCK_OPS:
                    if current_points is not None:
                        audit.point_blocks.append(current_points)
                    current_points = PointBlock(
                        label=current_label or "<anonymous>",
                        encoding=operation,
                        declared_count=arguments[0] if arguments else "",
                        rows=0,
                        source=location,
                    )
                    continue

                if operation in POINT_ROW_OPS:
                    if len(arguments) != 3:
                        audit.issues.append(AuditIssue(
                            "error", f"{operation} requires three coordinates", location
                        ))
                    if current_points is None:
                        audit.issues.append(AuditIssue(
                            "warning", f"{operation} occurs outside a recognized point block", location
                        ))
                    else:
                        current_points.rows += 1
                    audit.point_rows += 1
                    continue

                if operation == "endpoints":
                    if current_points is not None:
                        audit.point_blocks.append(current_points)
                        current_points = None
                    continue

                if operation == "frames":
                    audit.animated_tables += 1
                    continue

                if operation == "faces":
                    if current_faces is not None:
                        audit.face_groups.append(current_faces)
                    current_faces = FaceGroup(
                        label=current_label or "<anonymous>",
                        declared_count=arguments[0] if arguments else None,
                        faces=0,
                        expanded_wire_edges=0,
                        source=location,
                    )
                    continue

                face_match = FACE_RE.match(operation)
                if face_match:
                    vertex_count = int(face_match.group("count"))
                    expected_arguments = 5 + vertex_count
                    if len(arguments) != expected_arguments:
                        audit.issues.append(AuditIssue(
                            "error",
                            f"{operation} has {len(arguments)} arguments; expected {expected_arguments}",
                            location,
                        ))
                    if current_faces is None:
                        current_faces = FaceGroup(
                            label=current_label or "<implicit>",
                            declared_count=None,
                            faces=0,
                            expanded_wire_edges=0,
                            source=location,
                        )
                    current_faces.faces += 1
                    audit.polygon_rows += 1
                    continue

                wire_match = WIRE_FACE_RE.match(operation)
                if wire_match:
                    vertex_count = int(wire_match.group("count"))
                    expected_arguments = 5 + vertex_count
                    if len(arguments) != expected_arguments:
                        audit.issues.append(AuditIssue(
                            "error",
                            f"{operation} has {len(arguments)} arguments; expected {expected_arguments}",
                            location,
                        ))
                    if current_faces is None:
                        current_faces = FaceGroup(
                            label=current_label or "<implicit>",
                            declared_count=None,
                            faces=0,
                            expanded_wire_edges=0,
                            source=location,
                        )
                    current_faces.faces += 1
                    current_faces.expanded_wire_edges += vertex_count
                    audit.polygon_rows += 1
                    audit.expanded_wire_edges += vertex_count
                    continue

                if operation == "viz":
                    audit.visibility_rows += 1
                    continue

                if operation in {"fend", "fendq", "endshape"}:
                    if current_faces is not None:
                        audit.face_groups.append(current_faces)
                        current_faces = None

        if current_points is not None:
            audit.point_blocks.append(current_points)
        if current_faces is not None:
            audit.face_groups.append(current_faces)

    duplicate_names: dict[str, list[ShapeHeader]] = {}
    for header in audit.headers:
        duplicate_names.setdefault(header.name.lower(), []).append(header)
    for duplicate_group in duplicate_names.values():
        if len(duplicate_group) > 1:
            first = duplicate_group[0]
            audit.issues.append(AuditIssue(
                "warning",
                f"shape name {first.name!r} appears {len(duplicate_group)} times",
                first.source,
            ))

    return audit


def _print_summary(audit: ShapeAudit) -> None:
    summary = audit.to_dict()["summary"]
    assert isinstance(summary, dict)
    print(f"Shape source:       {audit.source_root}")
    print(f"ASM files:          {summary['files']}")
    print(f"Shape headers:      {summary['shape_headers']}")
    print(f"Point blocks/rows:  {summary['point_blocks']} / {summary['point_rows']}")
    print(f"Face groups/rows:   {summary['face_groups']} / {summary['polygon_rows']}")
    print(f"Wire edges emitted: {summary['expanded_wire_edges']}")
    print(f"Visibility rows:    {summary['visibility_rows']}")
    print(f"Animated tables:    {summary['animated_tables']}")
    print(f"Issues:             {summary['errors']} errors, {summary['warnings']} warnings")


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source",
        required=True,
        type=Path,
        help="Path to UltraStarFox/SF/SHAPES",
    )
    parser.add_argument("--output", type=Path, help="Optional JSON manifest path")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Return a failure status when structural errors are found",
    )
    arguments = parser.parse_args(list(argv) if argv is not None else None)

    if not arguments.source.is_dir():
        parser.error(f"shape source directory does not exist: {arguments.source}")

    audit = audit_shapes(arguments.source)
    _print_summary(audit)

    if arguments.output:
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_text(
            json.dumps(audit.to_dict(), indent=2) + "\n", encoding="utf-8"
        )
        print(f"Manifest:           {arguments.output.resolve()}")

    has_errors = any(issue.severity == "error" for issue in audit.issues)
    return 1 if arguments.strict and has_errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
