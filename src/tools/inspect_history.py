#!/usr/bin/env python3
"""Inspect checksummed ClioPlay GPS logs locally; never print coordinates."""
import argparse
import json
from pathlib import Path
import struct

RECORD = struct.Struct("<IIIiiHHI")
BEGIN, SAMPLE, END = 0x3142474D, 0x3153474D, 0x3145474D


def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & 0xffffffff
    return h


def inspect(path):
    report = dict(file=path.name, records=0, complete_sessions=0,
                  incomplete_sessions=0, invalid_records=0, samples=0,
                  recorded_seconds=0)
    active = None
    with path.open("rb") as stream:
        while raw := stream.read(RECORD.size):
            if len(raw) != RECORD.size:
                report["invalid_records"] += 1
                break
            kind, epoch, ms, lat, lon, speed, heading, check = RECORD.unpack(raw)
            report["records"] += 1
            if fnv(raw[:-4]) != check or kind not in (BEGIN, SAMPLE, END):
                report["invalid_records"] += 1
                if active is not None:
                    report["incomplete_sessions"] += 1
                    active = None
                continue
            if kind == BEGIN:
                if active is not None:
                    report["incomplete_sessions"] += 1
                active = [ms, 0]
            elif active is not None:
                if ms < active[0]:
                    report["incomplete_sessions"] += 1
                    active = None
                    continue
                active[0] = ms
                if kind == SAMPLE:
                    active[1] += 1
                elif kind == END:
                    if active[1]:
                        report["complete_sessions"] += 1
                        report["samples"] += active[1]
                        report["recorded_seconds"] += ms // 1000
                    else:
                        report["incomplete_sessions"] += 1
                    active = None
    if active is not None:
        report["incomplete_sessions"] += 1
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path)
    args = parser.parse_args()
    reports = [inspect(p) for p in args.paths if p.is_file()]
    if not reports:
        parser.error("No history files found")
    print(json.dumps(reports, indent=2))


if __name__ == "__main__":
    main()
