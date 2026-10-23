#!/usr/bin/env python3
"""Launch the self-contained source tree's sequential card installer."""
from pathlib import Path
import subprocess
import sys

if __name__ == '__main__':
    raise SystemExit(subprocess.call([
        sys.executable, str(Path(__file__).resolve().parent / 'src/install.py'), *sys.argv[1:]
    ]))
