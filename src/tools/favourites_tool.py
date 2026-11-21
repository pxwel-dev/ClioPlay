#!/usr/bin/env python3
"""Inspect, verify, export and import ClioPlay CLP2 favourites/recents files.

Supports CSV, JSON and GPX 1.1 using only the Python 3.11+ standard library.
CLP2 binary layout (matching clio843/src/clio040.c):
  Header: magic = 0x32504c43 ("CLP2"), uint32 count (0..25)
  Record: int32 lat (1e-5 deg), int32 lon (1e-5 deg), char name[161], 3 pad bytes (172 B)
"""
from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
import json
import math
import os
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unicodedata
import xml.etree.ElementTree as ET

CLP2_MAGIC = 0x32504C43
HEADER_STRUCT = struct.Struct("<II")
RECORD_STRUCT = struct.Struct("<ii161s3x")
MAX_PLACES = 25
NAME_MAX_CHARS = 40
NAME_MAX_BYTES = 160
SAME_PLACE_E5 = 8
GPX_NS = "http://www.topografix.com/GPX/1/1"


@dataclass(frozen=True)
class Place:
    lat_e5: int
    lon_e5: int
    name: str

    @property
    def lat(self) -> float:
        return self.lat_e5 / 1e5

    @property
    def lon(self) -> float:
        return self.lon_e5 / 1e5


def sanitize_name(raw: str | None) -> str:
    """Normalize and clamp a place name to ClioPlay's 40-char / 160-byte UTF-8 limit."""
    text = unicodedata.normalize("NFC", "" if raw is None else str(raw))
    cleaned_chars: list[str] = []
    for ch in text:
        cat = unicodedata.category(ch)
        if cat in ("Cc", "Cf", "Cs", "Co", "Cn"):
            cleaned_chars.append(" " if ch in ("\t", "\r", "\n") else "")
        else:
            cleaned_chars.append(ch)
    collapsed = " ".join("".join(cleaned_chars).split())
    if not collapsed:
        return "MAP POINT"
    out: list[str] = []
    used_bytes = 0
    for ch in collapsed:
        if len(out) >= NAME_MAX_CHARS:
            break
        enc = ch.encode("utf-8")
        if used_bytes + len(enc) > NAME_MAX_BYTES:
            break
        out.append(ch)
        used_bytes += len(enc)
    return "".join(out) or "MAP POINT"


def parse_coord_e5(value: object, limit: int, label: str) -> int:
    try:
        num = float(str(value).strip())
    except (TypeError, ValueError) as exc:
        raise ValueError(f"Invalid {label}: {value!r}") from exc
    if not math.isfinite(num) or num < -limit or num > limit:
        raise ValueError(f"{label} out of range [-{limit}, {limit}]: {value!r}")
    e5 = int(math.floor(num * 1e5 + 0.5)) if num >= 0 else -int(math.floor(-num * 1e5 + 0.5))
    max_e5 = limit * 100000
    if e5 < -max_e5 or e5 > max_e5:
        raise ValueError(f"{label} out of range [-{limit}, {limit}]: {value!r}")
    return e5


def same_place(a: Place, b: Place) -> bool:
    return abs(a.lat_e5 - b.lat_e5) <= SAME_PLACE_E5 and abs(a.lon_e5 - b.lon_e5) <= SAME_PLACE_E5


def read_clp2(path: Path, max_places: int = MAX_PLACES) -> list[Place]:
    data = path.read_bytes()
    if len(data) < HEADER_STRUCT.size:
        raise ValueError(f"{path}: truncated CLP2 header ({len(data)} bytes)")
    magic, count = HEADER_STRUCT.unpack_from(data, 0)
    if magic != CLP2_MAGIC:
        raise ValueError(f"{path}: bad CLP2 magic 0x{magic:08x} (expected 0x{CLP2_MAGIC:08x})")
    if count > max_places:
        raise ValueError(f"{path}: record count {count} exceeds maximum {max_places}")
    expected = HEADER_STRUCT.size + count * RECORD_STRUCT.size
    if len(data) != expected:
        raise ValueError(f"{path}: expected {expected} bytes for {count} places, found {len(data)}")
    places: list[Place] = []
    offset = HEADER_STRUCT.size
    for index in range(count):
        lat_e5, lon_e5, raw_name = RECORD_STRUCT.unpack_from(data, offset)
        offset += RECORD_STRUCT.size
        if not (-9000000 <= lat_e5 <= 9000000) or not (-18000000 <= lon_e5 <= 18000000):
            raise ValueError(f"{path}: record {index} has out-of-range coordinates ({lat_e5}, {lon_e5})")
        nul = raw_name.find(b"\x00")
        name_bytes = raw_name if nul < 0 else raw_name[:nul]
        try:
            name = name_bytes.decode("utf-8")
        except UnicodeDecodeError as exc:
            raise ValueError(f"{path}: record {index} contains invalid UTF-8") from exc
        places.append(Place(lat_e5=lat_e5, lon_e5=lon_e5, name=sanitize_name(name)))
    return places


def encode_clp2(places: list[Place], max_places: int = MAX_PLACES) -> bytes:
    if len(places) > max_places:
        raise ValueError(f"Too many places ({len(places)} > {max_places})")
    chunks = [HEADER_STRUCT.pack(CLP2_MAGIC, len(places))]
    for place in places:
        if not (-9000000 <= place.lat_e5 <= 9000000) or not (-18000000 <= place.lon_e5 <= 18000000):
            raise ValueError(f"Coordinates out of range: {place}")
        clean = sanitize_name(place.name)
        encoded = clean.encode("utf-8")
        if len(encoded) > NAME_MAX_BYTES:
            raise ValueError(f"Encoded name exceeds {NAME_MAX_BYTES} bytes: {clean!r}")
        padded = encoded + b"\x00" * (161 - len(encoded))
        chunks.append(RECORD_STRUCT.pack(place.lat_e5, place.lon_e5, padded))
    return b"".join(chunks)


def write_clp2_atomic(path: Path, places: list[Place], backup: bool = False) -> None:
    payload = encode_clp2(places)
    path.parent.mkdir(parents=True, exist_ok=True)
    if backup and path.exists():
        shutil.copy2(path, path.with_name(path.name + ".bak"))
    fd, tmp_name = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=str(path.parent))
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(tmp_name, path)
    except Exception:
        try:
            os.unlink(tmp_name)
        except OSError:
            pass
        raise


def detect_format(path: Path, explicit: str | None) -> str:
    if explicit:
        return explicit.lower()
    suffix = path.suffix.lower().lstrip(".")
    if suffix in ("csv", "json", "gpx"):
        return suffix
    raise ValueError(f"Cannot infer format from {path.name!r}; pass --format csv|json|gpx")


def export_places(places: list[Place], output: Path, fmt: str) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    if fmt == "csv":
        with output.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.writer(handle)
            writer.writerow(["name", "lat", "lon"])
            for p in places:
                writer.writerow([p.name, f"{p.lat:.5f}", f"{p.lon:.5f}"])
    elif fmt == "json":
        payload = {
            "format": "clioplay-places-v1",
            "places": [{"name": p.name, "lat": round(p.lat, 5), "lon": round(p.lon, 5)} for p in places],
        }
        output.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    elif fmt == "gpx":
        ET.register_namespace("", GPX_NS)
        root = ET.Element(f"{{{GPX_NS}}}gpx", {"version": "1.1", "creator": "ClioPlay"})
        for p in places:
            wpt = ET.SubElement(root, f"{{{GPX_NS}}}wpt", {"lat": f"{p.lat:.5f}", "lon": f"{p.lon:.5f}"})
            name_el = ET.SubElement(wpt, f"{{{GPX_NS}}}name")
            name_el.text = p.name
        tree = ET.ElementTree(root)
        ET.indent(tree, space="  ")
        tree.write(output, encoding="utf-8", xml_declaration=True)
    else:
        raise ValueError(f"Unsupported export format: {fmt}")


def load_places_file(source: Path, fmt: str) -> list[Place]:
    places: list[Place] = []
    if fmt == "csv":
        with source.open("r", encoding="utf-8-sig", newline="") as handle:
            reader = csv.DictReader(handle)
            if not reader.fieldnames:
                return []
            lower = {f.strip().lower(): f for f in reader.fieldnames if f}
            lat_col = lower.get("lat") or lower.get("latitude")
            lon_col = lower.get("lon") or lower.get("lng") or lower.get("longitude")
            name_col = lower.get("name") or lower.get("title") or lower.get("label")
            if not lat_col or not lon_col:
                raise ValueError("CSV must contain 'lat' and 'lon' (or 'latitude'/'longitude') columns")
            for row in reader:
                if not any((v or "").strip() for v in row.values()):
                    continue
                lat_e5 = parse_coord_e5(row[lat_col], 90, "latitude")
                lon_e5 = parse_coord_e5(row[lon_col], 180, "longitude")
                places.append(Place(lat_e5=lat_e5, lon_e5=lon_e5, name=sanitize_name(row.get(name_col, "") if name_col else "")))
    elif fmt == "json":
        data = json.loads(source.read_text(encoding="utf-8"))
        rows = data["places"] if isinstance(data, dict) and "places" in data else data
        if not isinstance(rows, list):
            raise ValueError("JSON must be a list of places or an object with a 'places' list")
        for item in rows:
            if not isinstance(item, dict):
                raise ValueError("Each JSON place entry must be an object")
            lat_val = item.get("lat", item.get("latitude"))
            lon_val = item.get("lon", item.get("lng", item.get("longitude")))
            lat_e5 = parse_coord_e5(lat_val, 90, "latitude")
            lon_e5 = parse_coord_e5(lon_val, 180, "longitude")
            places.append(Place(lat_e5=lat_e5, lon_e5=lon_e5, name=sanitize_name(item.get("name", item.get("title", "")))))
    elif fmt == "gpx":
        tree = ET.parse(source)
        root = tree.getroot()
        for elem in root.iter():
            tag = elem.tag.rsplit("}", 1)[-1]
            if tag != "wpt":
                continue
            lat_e5 = parse_coord_e5(elem.attrib.get("lat"), 90, "latitude")
            lon_e5 = parse_coord_e5(elem.attrib.get("lon"), 180, "longitude")
            name = ""
            for child in elem:
                if child.tag.rsplit("}", 1)[-1] == "name" and child.text:
                    name = child.text
                    break
            places.append(Place(lat_e5=lat_e5, lon_e5=lon_e5, name=sanitize_name(name)))
    else:
        raise ValueError(f"Unsupported import format: {fmt}")
    return places


def merge_places(existing: list[Place], incoming: list[Place], max_places: int = MAX_PLACES) -> tuple[list[Place], int, int]:
    """Combine existing and incoming places, deduplicating within ClioPlay's same_place tolerance.

    Returns (combined, duplicates_skipped, overflow_skipped).
    """
    result = list(existing)
    duplicates = 0
    overflow = 0
    for place in incoming:
        if any(same_place(curr, place) for curr in result):
            duplicates += 1
            continue
        if len(result) >= max_places:
            overflow += 1
            continue
        result.append(place)
    return result, duplicates, overflow


def cmd_list(args: argparse.Namespace) -> int:
    places = read_clp2(args.file)
    if args.json:
        print(json.dumps([{"name": p.name, "lat": round(p.lat, 5), "lon": round(p.lon, 5)} for p in places], ensure_ascii=False, indent=2))
        return 0
    if not places:
        print(f"{args.file}: 0 places")
        return 0
    print(f"{args.file}: {len(places)}/{MAX_PLACES} places")
    for idx, p in enumerate(places, 1):
        print(f"  {idx:2d}. {p.name:<40s}  {p.lat:9.5f}, {p.lon:10.5f}")
    return 0


def cmd_verify(args: argparse.Namespace) -> int:
    places = read_clp2(args.file)
    print(f"OK: {args.file} is valid CLP2 ({len(places)}/{MAX_PLACES} places, {HEADER_STRUCT.size + len(places) * RECORD_STRUCT.size} bytes)")
    return 0


def cmd_export(args: argparse.Namespace) -> int:
    places = read_clp2(args.file)
    fmt = detect_format(args.output, args.format)
    export_places(places, args.output, fmt)
    print(f"Exported {len(places)} place(s) from {args.file} -> {args.output} ({fmt.upper()})")
    return 0


def cmd_import(args: argparse.Namespace) -> int:
    fmt = detect_format(args.source, args.format)
    incoming = load_places_file(args.source, fmt)
    existing: list[Place] = []
    if args.merge and args.target.exists():
        existing = read_clp2(args.target)
    combined, duplicates, overflow = merge_places(existing, incoming, MAX_PLACES)
    if args.strict and overflow > 0:
        raise ValueError(f"Import exceeds ClioPlay limit of {MAX_PLACES} places ({overflow} would be dropped)")
    write_clp2_atomic(args.target, combined, backup=args.backup)
    extra = []
    if duplicates:
        extra.append(f"{duplicates} duplicate(s) skipped")
    if overflow:
        extra.append(f"{overflow} over {MAX_PLACES}-place cap skipped")
    suffix = f" ({', '.join(extra)})" if extra else ""
    print(f"Wrote {len(combined)} place(s) to {args.target}{suffix}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Inspect, export and import ClioPlay CLP2 favourites/recents files.")
    sub = parser.add_subparsers(dest="command", required=True)

    p_list = sub.add_parser("list", help="List places stored in a CLP2 file (faves.dat or recent.dat)")
    p_list.add_argument("file", type=Path)
    p_list.add_argument("--json", action="store_true", help="Print places as JSON")
    p_list.set_defaults(func=cmd_list)

    p_verify = sub.add_parser("verify", help="Validate a CLP2 file's magic, size, coordinates and UTF-8 names")
    p_verify.add_argument("file", type=Path)
    p_verify.set_defaults(func=cmd_verify)

    p_export = sub.add_parser("export", help="Export CLP2 places to CSV, JSON or GPX")
    p_export.add_argument("file", type=Path, help="Input faves.dat or recent.dat")
    p_export.add_argument("output", type=Path, help="Output .csv, .json or .gpx path")
    p_export.add_argument("--format", choices=("csv", "json", "gpx"), help="Override output format")
    p_export.set_defaults(func=cmd_export)

    p_import = sub.add_parser("import", help="Import CSV, JSON or GPX places into a CLP2 file")
    p_import.add_argument("source", type=Path, help="Input .csv, .json or .gpx path")
    p_import.add_argument("target", type=Path, help="Target faves.dat or recent.dat")
    p_import.add_argument("--format", choices=("csv", "json", "gpx"), help="Override input format")
    p_import.add_argument("--merge", action="store_true", help="Keep existing places and append non-duplicate imports")
    p_import.add_argument("--backup", action="store_true", help="Create <target>.bak before overwriting")
    p_import.add_argument("--strict", action="store_true", help="Fail instead of dropping entries past the 25-place cap")
    p_import.set_defaults(func=cmd_import)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
