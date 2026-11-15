#!/usr/bin/env python3
"""Capture the current native theme-aware splash in both themes outside the project."""
import argparse,hashlib,json,subprocess,tempfile,time
from pathlib import Path
from PIL import Image
from verify import decode
import build_splash_icon
ROOT=Path(__file__).resolve().parents[1]
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--out',type=Path,help='external output folder; defaults to a temporary directory')
 args=parser.parse_args();out=(args.out or Path(tempfile.mkdtemp(prefix='clioplay-boot-renders-'))).resolve()
 if out==ROOT.parent or ROOT.parent in out.parents:raise SystemExit('Keep captures outside the project')
 out.mkdir(parents=True,exist_ok=True);build_splash_icon.main()
 with tempfile.TemporaryDirectory(prefix='clioplay-splash-test-') as folder:
  work=Path(folder);(work/'clio843').mkdir();exe=work/'test'
  subprocess.run(['cc','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-DCLIO_HOST_ROOT="'+str(work)+'/"',str(ROOT/'tests/splash.c'),'-o',str(exe)],check=True)
  start=time.monotonic();result=subprocess.check_output([str(exe),str(out)],text=True);elapsed=time.monotonic()-start;print(result)
 board=Image.new('RGB',(960,272))
 for i,theme in enumerate(('day','night')):
  image=decode((out/f'splash-{theme}.rgb565').read_bytes());image.save(out/f'splash-{theme}.png');board.paste(image,(i*480,0))
 board.save(out/'themes.png')
 sources=('clio843/src/cliospl.c','clio843/src/boot_art.h','clio843/src/boot_handoff.h','clio843/src/splash_icon.h','tools/build_splash_icon.py','tests/splash.c')
 report={'renderer':'Production native RGB565 splash; UBSan','native':result,'host_test_seconds':elapsed,'hardware':'not tested; host timing is not head-unit timing','source_sha256':{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in sources},'frames':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('*.rgb565')}}
 (out/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print('Renders:',out)
if __name__=='__main__':main()
