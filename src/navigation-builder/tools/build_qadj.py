#!/usr/bin/env python3
"""Build optional QAD7 unified-packet sidecar from CPR2 v4/v5 route.cpr."""
import argparse, subprocess, tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent

def main():
    ap=argparse.ArgumentParser(description='Build optional QAD7 accelerator sidecar for CPR2 v4/v5')
    ap.add_argument('route_cpr',type=Path); ap.add_argument('route_qadj',type=Path)
    a=ap.parse_args(); a.route_qadj.parent.mkdir(parents=True,exist_ok=True)
    src=HERE/'build_qadj.cpp'
    with tempfile.TemporaryDirectory() as td:
        exe=Path(td)/'build_qadj'
        subprocess.run(['c++','-O3','-std=c++17','-Wall','-Wextra','-o',str(exe),str(src)],check=True)
        subprocess.run([str(exe),str(a.route_cpr),str(a.route_qadj)],check=True)
if __name__=='__main__': main()
