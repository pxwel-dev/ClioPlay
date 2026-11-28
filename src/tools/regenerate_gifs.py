#!/usr/bin/env python3
"""One-command regeneration for ClioPlay demo GIFs (all or selected).

Without map assets (quick preview of changed UI):
    python3 tools/regenerate_gifs.py
    python3 tools/regenerate_gifs.py --only trips-layout

With your existing Newcastle/UK map bundle (all published GIFs, or selected ones):
    python3 tools/regenerate_gifs.py --maps /path/to/clio843/maps
    python3 tools/regenerate_gifs.py --maps /path/to/clio843/maps --only v223-view-diversion
    python3 tools/regenerate_gifs.py --maps /path/to/clio843/maps --list

You can optionally specify --phone-capture /path/to/browser/captures for a fresh
ClioPlay Link browser panel; otherwise the existing published panel is reused.

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
    parser.add_argument('--phone-capture', type=Path, help='Optional fresh browser capture for ClioPlay Link')
    parser.add_argument('--only', '--gif', dest='only', nargs='+', metavar='GIF',
                        help='Regenerate only the named GIF(s), with or without .gif suffix')
    parser.add_argument('--list', action='store_true', help='List available GIF names and exit')
    args = parser.parse_args()
    if not args.maps:
        if args.phone_capture:
            parser.error('--phone-capture needs --maps')
        command = [sys.executable, str(ROOT/'tools/render_mapless_gifs.py'), '--output', str(ROOT/'docs/demo')]
        if args.list:
            command.append('--list')
        elif args.only:
            command += ['--only', *args.only]
        else:
            print('No maps supplied: regenerating live C-rendered Trips/HUD/coordinate and intelligent-navigation GIFs.')
    else:
        path = args.maps.expanduser().resolve()
        if not args.list and not (path/'build.json').is_file():
            parser.error(f'{path}/build.json not found; specify the built clio843/maps directory')
        command = [sys.executable, str(ROOT/'tools/render_previews.py'), '--maps', str(path)]
        if args.list:
            command.append('--list')
        else:
            if args.only:
                print('Map bundle found: regenerating selected GIF(s):', ', '.join(args.only))
                command += ['--only', *args.only]
            else:
                print('Map bundle found: regenerating all demo GIFs, including map-backed Trips previews.')
            if args.phone_capture:
                command += ['--phone-capture', str(args.phone_capture.expanduser().resolve())]
    subprocess.run(command, check=True)


if __name__ == '__main__':
    main()
