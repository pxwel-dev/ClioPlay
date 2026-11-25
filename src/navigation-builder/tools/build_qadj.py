#!/usr/bin/env python3
"""Build optional QAD7 unified-packet sidecar from CPR2 v5 route.cpr."""
import argparse
from pathlib import Path
from native_helper import run_compiled
HERE=Path(__file__).resolve().parent

def main():
    ap=argparse.ArgumentParser(description='Build optional QAD7 accelerator sidecar for CPR2 v5')
    ap.add_argument('route_cpr',type=Path); ap.add_argument('route_qadj',type=Path)
    a=ap.parse_args(); a.route_qadj.parent.mkdir(parents=True,exist_ok=True)
    src=HERE/'build_qadj.cpp'
    # Atlas builds run this once per region; reuse a binary cached in a private per-user dir.
    run_compiled('build_qadj',src,['-O3','-std=c++17','-Wall','-Wextra'],
                 [a.route_cpr,a.route_qadj],fallback_dir=a.route_qadj.parent)
if __name__=='__main__': main()
