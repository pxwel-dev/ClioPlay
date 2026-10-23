#!/usr/bin/env python3
import subprocess,json,sys,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
from text_engine import build
work=ROOT/'build/real-border';bundle=work/'bundle';host=work/'host';(host/'clio843').mkdir(parents=True,exist_ok=True)
if not (host/'clio843/maps').exists():(host/'clio843/maps').symlink_to(bundle,target_is_directory=True)
shutil.copyfile(ROOT/'clio843/font.bin',host/'clio843/font.bin');obj=work/'border.o';exe=work/'border-test';flags=['-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-DCLIO_UNICODE=1','-DCLIO_HOST_ROOT="'+str(host)+'/"'];subprocess.run(['cc',*flags,'-c',str(ROOT/'tests/border.c'),'-o',str(obj)],check=True);subprocess.run(['c++',*flags,str(obj),str(build('host-pool')),'-o',str(exe)],check=True)
out=ROOT/'docs/next-release/framebuffer';out.mkdir(parents=True,exist_ok=True);r=subprocess.check_output([str(exe),str(out)],text=True);print(r);(ROOT/'docs/next-release/border-verification.json').write_text(json.dumps({'source':json.loads((bundle/'build.json').read_text())['source'],'native':r},indent=2)+'\n')
