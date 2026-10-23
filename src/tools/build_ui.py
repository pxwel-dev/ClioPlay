#!/usr/bin/env python3
"""Build a freestanding ARM7TDMI ClioPlay executable with Clang and LLD."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import text_engine

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT/'clio843/src/clio040.c'
CFLAGS = ['--target=arm-linux-gnueabi', '-mcpu=arm7tdmi', '-marm', '-mfloat-abi=soft',
          '-Os', '-ffreestanding', '-fno-builtin', '-fno-stack-protector', '-fno-pic',
          '-fno-pie', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables']
LDFLAGS = ['-m', 'armelf_linux_eabi', '--static', '--entry=_start', '--image-base=0x10000',
           '-z', 'max-page-size=65536']


def find_tool(explicit, name, candidates=()):
    for candidate in [explicit, shutil.which(name), *candidates]:
        if candidate and Path(candidate).is_file():
            return str(Path(candidate).absolute())
    raise SystemExit(f'{name} not found; supply --{name.replace("ld.lld", "linker")}')


def inspect_elf(path):
    data=path.read_bytes()
    h=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
    if h[0][:7]!=b'\x7fELF\x01\x01\x01' or h[1:4]!=(2,40,1):
        raise ValueError('expected a little-endian ARM ELF32 executable')
    if h[7]!=0x05000200:
        raise ValueError(f'expected soft-float EABI5 flags; got {h[7]:#x}')
    programs=[struct.unpack_from('<8I',data,h[5]+i*h[9]) for i in range(h[10])]
    if any(p[0] in (2,3) for p in programs):
        raise ValueError('executable must have no dynamic loader or dynamic segment')
    if not any(p[0]==1 and p[6]&1 and p[2]<=h[4]<p[2]+p[4] for p in programs):
        raise ValueError('entry point is not in executable file-backed memory')
    raw=[struct.unpack_from('<10I',data,h[6]+i*h[11]) for i in range(h[12])]
    names=raw[h[13]]; strings=data[names[4]:names[4]+names[5]]
    sections={strings[s[0]:].split(b'\0')[0].decode():s for s in raw}
    attrs=sections['.ARM.attributes']
    if b'arm7tdmi\0' not in data[attrs[4]:attrs[4]+attrs[5]]:
        raise ValueError('ARM7TDMI attributes missing')
    for table in raw:
        if table[1]!=2: continue
        st=raw[table[6]]; symbols=data[st[4]:st[4]+st[5]]
        for off in range(table[4],table[4]+table[5],table[9]):
            name,value,size,info,other,section=struct.unpack_from('<IIIBBH',data,off)
            if section==0 and info>>4 in (1,2) and name:
                raise ValueError('undefined symbol: '+symbols[name:].split(b'\0')[0].decode())
    return {'machine':'ARM7TDMI / ARMv4T', 'abi':'ELF32 EABI5 soft-float; Linux OABI syscalls',
            'entry':hex(h[4]), 'bytes':len(data), 'sha256':hashlib.sha256(data).hexdigest(),
            'sections':{n:{'address':hex(sections[n][3]),'bytes':sections[n][5]}
                        for n in ('.text','.rodata','.data','.bss')}}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--install', action='store_true', help='also install the executable into this version’s clio843 folder')
    ap.add_argument('--no-map-antialias',action='store_true',help='build the original map rasterizer as a performance fallback')
    ap.add_argument('--clang')
    ap.add_argument('--linker')
    ap.add_argument('--out',type=Path,default=ROOT/'build/clio040')
    a=ap.parse_args()
    import build_thin_coverage
    build_thin_coverage.main()
    import build_keyboard_data
    build_keyboard_data.main([])
    import build_speed_defaults
    build_speed_defaults.main()
    import build_translations
    build_translations.main()
    import build_splash_icon
    build_splash_icon.main()
    clang=find_tool(a.clang,'clang')
    linker=find_tool(a.linker,'ld.lld',('/opt/homebrew/opt/lld/bin/ld.lld',
                                      '/opt/homebrew/opt/llvm/bin/ld.lld'))
    out=a.out.expanduser().resolve();out.parent.mkdir(parents=True,exist_ok=True)
    # The iCloud conflict copy is unused; only ui_translations.h is included.
    inputs=sorted(p for p in (ROOT/'clio843/src').rglob('*') if p.is_file() and 'recovered' not in p.parts and '__pycache__' not in p.parts and p.name not in ('.DS_Store','ui_translations 2.h'))+[Path(__file__).resolve(),ROOT/'tools/text_engine.py',ROOT/'tools/build_translations.py',ROOT/'tools/build_splash_icon.py',ROOT/'tools/build_keyboard_data.py',ROOT/'tools/build_speed_defaults.py',ROOT/'tools/build_thin_coverage.py',ROOT/'navigation-builder/tools/global_speeds.py',ROOT/'navigation-builder/data/speed-contexts.json']+sorted((ROOT/'locales').rglob('*.json'))
    before=hashlib.sha256(b''.join(str(p.relative_to(ROOT)).encode()+b'\0'+p.read_bytes() for p in inputs)).hexdigest()
    for source,destination in ((SOURCE,out),(ROOT/'clio843/src/cliospl.c',out.with_name('cliospl')),(ROOT/'clio843/src/clioa2dp.c',out.with_name('clioa2dp'))):
        with tempfile.TemporaryDirectory(prefix='clioplay-arm-build-') as tmp:
            obj=Path(tmp)/'native.o';binary=Path(tmp)/destination.name
            compile_flags=CFLAGS+['-ffunction-sections','-fdata-sections']+(['-DCLIO_UNICODE=1'] if source==SOURCE else [])+(['-DMAP_ANTIALIAS=0'] if a.no_map_antialias and source==SOURCE else [])
            subprocess.run([clang,*compile_flags,'-c',str(source),'-o',str(obj)],check=True)
            extras=[str(text_engine.build('arm',clang))] if source==SOURCE else []
            subprocess.run([linker,*LDFLAGS,'--gc-sections',str(obj),*extras,'-o',str(binary)],check=True)
            report=inspect_elf(binary)
            if before!=hashlib.sha256(b''.join(str(p.relative_to(ROOT)).encode()+b'\0'+p.read_bytes() for p in inputs)).hexdigest():
                raise RuntimeError('source changed during build')
            staged=destination.with_name(destination.name+'.new')
            shutil.copyfile(binary,staged);staged.chmod(0o755);os.replace(staged,destination)
        report.update({'source':str(source.relative_to(ROOT)),'source_tree_sha256':before,'inputs':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
                       'compiler':subprocess.check_output([clang,'--version'],text=True).splitlines()[0],
                       'linker':subprocess.check_output([linker,'--version'],text=True).strip(),
                       'compile_flags':compile_flags,'link_flags':LDFLAGS,
                       'validation':'ELF/ABI/entry/undefined-symbol checks; not tested in a vehicle'})
        destination.with_suffix('.build.json').write_text(json.dumps(report,indent=2)+'\n')
        if a.install:
            installed=ROOT/'clio843'/destination.name
            installed.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(destination,installed);installed.chmod(0o755)
        print(f'Built {destination}\nSHA-256 {report["sha256"]}\nBSS {report["sections"][".bss"]["bytes"]:,} bytes')

    delivery=ROOT.parent if ROOT.name=='src' and (ROOT.parent/'latest.json').is_file() else ROOT.parent/'CLIOPLAY-LATEST-VERSION'
    if a.install and delivery.is_dir():
        subprocess.run([sys.executable,str(ROOT/'tools/refresh_latest.py'),'--output',str(delivery)],check=True)


if __name__=='__main__':
    main()
