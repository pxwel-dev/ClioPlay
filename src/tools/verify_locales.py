#!/usr/bin/env python3
"""Render every localized C interface; verify glyphs, controls and persistence."""
import json,subprocess,tempfile,shutil,hashlib
from pathlib import Path
from text_engine import build
import build_translations
ROOT=Path(__file__).resolve().parents[1]
def main():
 build_translations.main();data=build_translations.load();out=ROOT/'docs/next-release/localized-framebuffer';out.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='clioplay-locales-') as t:
  t=Path(t);(t/'clio843').mkdir();shutil.copyfile(ROOT/'clio843/font.bin',t/'clio843/font.bin');header=f'static const char*const locale_cases[][{len(data["languages"])}]={{\n'
  for key,values in data['strings'].items():header+='{'+','.join(json.dumps(x,ensure_ascii=False) for x in [key,*values])+'},\n'
  header+='};\n';(t/'locale_cases.h').write_text(header);flags=['-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(t),'-DCLIO_HOST_ROOT="'+str(t)+'/"'];obj=t/'test.o';exe=t/'test';subprocess.run(['cc',*flags,'-c',str(ROOT/'tests/locales.c'),'-o',str(obj)],check=True);subprocess.run(['c++',*flags,str(obj),str(build('host-pool')),'-o',str(exe)],check=True);result=subprocess.check_output([str(exe),str(ROOT/'navigation-builder/data/global-fonts.cpf'),str(out)],text=True);print(result)
 (ROOT/'docs/next-release/locales-verification.json').write_text(json.dumps({'languages':data['languages'],'language_names':data['language_names'],'entries':len(data['strings']),'frames':len(data['languages'])*20,'frame_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(out.glob('*.rgb565'))},'native':result,'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'clio843/src/clio040.c',ROOT/'clio843/src/ui_translations.h',ROOT/'tests/locales.c',*sorted((ROOT/'locales').glob('*.json'))]},'hardware':'not tested'},ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
