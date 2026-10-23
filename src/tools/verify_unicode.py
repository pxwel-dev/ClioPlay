#!/usr/bin/env python3
"""Execute the actual native text engine and UI under UBSan; save framebuffer data."""
from pathlib import Path
import json,shutil,subprocess,tempfile
from text_engine import build
ROOT=Path(__file__).resolve().parents[1]
def main():
 lib=build('host-pool');out=ROOT/'docs/native-text';out.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='clioplay-unicode-') as td:
  td=Path(td);(td/'clio843').mkdir();shutil.copyfile(ROOT/'clio843/font.bin',td/'clio843/font.bin');obj=td/'test.o';exe=td/'test'
  flags=['-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all']
  subprocess.run(['cc',*flags,'-DCLIO_HOST_ROOT="'+str(td)+'/"','-c',str(ROOT/'tests/unicode.c'),'-o',str(obj)],check=True)
  subprocess.run(['c++',*flags,str(obj),str(lib),'-o',str(exe)],check=True)
  run=subprocess.run([str(exe),str(ROOT/'navigation-builder/data/global-fonts.cpf'),str(out)],text=True,capture_output=True);print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
 (out/'verification.json').write_text(json.dumps({'result':run.stdout.strip(),'engine':'Native HarfBuzz 10.4.0 + SheenBidi 3.0.0; actual production UI, host UBSan','head_unit':'Not tested on hardware'},indent=2)+'\n')
if __name__=='__main__':main()
