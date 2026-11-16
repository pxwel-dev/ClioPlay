#!/usr/bin/env python3
"""One-command regeneration for all ClioPlay demo GIFs.

Without map assets (quick preview of changed UI):
    python3 tools/regenerate_gifs.py

With your existing Newcastle/UK map bundle (all published GIFs, including
Trips, navigation demos, map transitions and the phone/browser composite):
    python3 tools/regenerate_gifs.py --maps /path/to/clio843/maps

You can optionally specify --phone-capture /path/to/browser/captures for a fresh
ClioPlay Send browser panel; otherwise the existing published panel is reused.

Requires Pillow, a C compiler (cc), and, for full map-backed generation, the
project's native text-engine dependencies plus the existing maps/ bundle.
All generated previews are written to src/docs/demo.
"""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--maps', type=Path, help='Existing map bundle, containing build.json, fonts.cpf, etc.')
    parser.add_argument('--phone-capture', type=Path, help='Optional fresh browser capture for ClioPlay Send')
    args = parser.parse_args()
    if not args.maps:
        if args.phone_capture:
            parser.error('--phone-capture needs --maps')
        print('No maps supplied: regenerating seven live C-rendered Trips/2.1.6 GIFs.')
        command = [sys.executable, str(ROOT/'tools/render_mapless_gifs.py'), '--output', str(ROOT/'docs/demo')]
    else:
        path = args.maps.expanduser().resolve()
        if not (path/'build.json').is_file():
            parser.error(f'{path}/build.json not found; specify the built clio843/maps directory')
        print('Map bundle found: regenerating all demo GIFs, including map-backed Trips previews.')
        command = [sys.executable, str(ROOT/'tools/render_previews.py'), '--maps', str(path)]
        if args.phone_capture:
            command += ['--phone-capture', str(args.phone_capture.expanduser().resolve())]
    subprocess.run(command, check=True)


if __name__ == '__main__':
    main()
