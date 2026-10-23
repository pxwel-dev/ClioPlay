#!/usr/bin/env python3
"""Two independent verifiers must safely share an initially empty text cache."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

import text_engine


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worker', type=Path)
    args = parser.parse_args()
    if args.worker:
        text_engine.ROOT = args.worker
        lib = text_engine.build('host-pool')
        print(hashlib.sha256(lib.read_bytes()).hexdigest())
        return
    with tempfile.TemporaryDirectory(prefix='clioplay-text-cache-') as tmp:
        root = Path(tmp)
        jobs = [subprocess.Popen([sys.executable, __file__, '--worker', str(root)],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 text=True) for _ in range(2)]
        results = [job.communicate(timeout=180) for job in jobs]
        for job, (stdout, stderr) in zip(jobs, results):
            assert job.returncode == 0, stderr
        assert results[0][0] == results[1][0]
        manifest = json.loads((root/'build/text-host-pool/build.json').read_text())
        assert manifest['sha256'] == results[0][0].strip()
        members = subprocess.check_output(['ar', 't', str(root/'build/text-host-pool/libtext.a')], text=True)
        assert len([name for name in members.splitlines() if name.endswith('.o')]) == 4, members
    print('Concurrent native text builders: same verified archive, four objects, no publication race.')


if __name__ == '__main__':
    main()
