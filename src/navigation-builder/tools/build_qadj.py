#!/usr/bin/env python3
"""Build optional QAD7 unified-packet sidecar from CPR2 v5 route.cpr."""
import argparse, hashlib, os, subprocess, tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent

def main():
    ap=argparse.ArgumentParser(description='Build optional QAD7 accelerator sidecar for CPR2 v5')
    ap.add_argument('route_cpr',type=Path); ap.add_argument('route_qadj',type=Path)
    a=ap.parse_args(); a.route_qadj.parent.mkdir(parents=True,exist_ok=True)
    src=HERE/'build_qadj.cpp'
    # Atlas builds run this once per region; reuse a binary cached by source content.
    exe=Path(tempfile.gettempdir())/('clioplay-build_qadj-'+hashlib.sha256(src.read_bytes()).hexdigest()[:16])
    if not exe.exists() or exe.stat().st_size==0:
        handle,temporary=tempfile.mkstemp(prefix=exe.name+'.',suffix='.tmp');os.close(handle);tmp=Path(temporary)
        try:
            subprocess.run(['c++','-O3','-std=c++17','-Wall','-Wextra','-o',str(tmp),str(src)],check=True)
            tmp.chmod(0o755);os.replace(tmp,exe)
        finally:tmp.unlink(missing_ok=True)
    subprocess.run([str(exe),str(a.route_cpr),str(a.route_qadj)],check=True)
if __name__=='__main__': main()
