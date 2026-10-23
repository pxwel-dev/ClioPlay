"""Build the native Unicode engine for the desktop verifier or ARM7TDMI.

Cross headers are build tools, fetched automatically from Debian. Runtime
arithmetic is compiled from the bundled LLVM source for ARMv4T, not linked
against a newer-CPU libgcc or a desktop libc.
"""
from pathlib import Path
import hashlib,io,json,os,subprocess,tarfile,urllib.request
ROOT=Path(__file__).resolve().parents[1];SRC=ROOT/'clio843/src';V=SRC/'vendor'
BASE=['-Os','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-pic','-fno-pie','-ffunction-sections','-fdata-sections','-fno-unwind-tables','-fno-asynchronous-unwind-tables']
ARM=['--target=arm-linux-gnueabi','-mcpu=arm7tdmi','-marm','-mfloat-abi=soft']
PACKAGE_SHA={'libc6-dev-armel-cross_2.36-8cross1_all.deb':'c0f6385b372dcdcbd6c4b1628cc3c41c4d114d591665b763f62e33947d032117','libstdc++-12-dev-armel-cross_12.2.0-14cross1_all.deb':'f794867fb924f675c994776af0e8d4b87e7d70f2603d4f4c18f3a27ba38bafb6'}
CPPPACKAGES={
 'libc6-dev-armel-cross_2.36-8cross1_all.deb':'https://deb.debian.org/debian/pool/main/c/cross-toolchain-base/',
 'libstdc++-12-dev-armel-cross_12.2.0-14cross1_all.deb':'https://deb.debian.org/debian/pool/main/g/gcc-12-cross/'}
def headers():
 root=ROOT/'build/cross-sysroot';marker=root/'.ready'
 if marker.exists():return root/'usr/arm-linux-gnueabi'
 root.mkdir(parents=True,exist_ok=True)
 for name,url in CPPPACKAGES.items():
  cache=ROOT/'build'/name
  if not cache.exists():
   print('Downloading cross headers: '+name,flush=True)
   with urllib.request.urlopen(url+name,timeout=60) as source:cache.write_bytes(source.read())
  data=cache.read_bytes()
  if hashlib.sha256(data).hexdigest()!=PACKAGE_SHA[name]:raise ValueError('Cross-header checksum mismatch: '+name)
  assert data[:8]==b'!<arch>\n';off=8
  while off+60<=len(data):
   name=data[off:off+16].decode().strip().rstrip('/');size=int(data[off+48:off+58]);blob=data[off+60:off+60+size];off+=60+size+size%2
   if name.startswith('data.tar'):
    with tarfile.open(fileobj=io.BytesIO(blob)) as tar:tar.extractall(root,filter='data')
 marker.write_text('Debian cross-compilation headers only; no target library is linked.\n')
 return root/'usr/arm-linux-gnueabi'

def build(target='arm',clang='clang',cxx='clang++'):
 # Several independent native verifiers share this cache. Keep object writes,
 # archive publication and its input manifest in the same critical section.
 out=ROOT/'build'/('text-'+target);out.mkdir(parents=True,exist_ok=True)
 with (out/'.build.lock').open('a+b') as lock:
  if os.name=='nt':
   import msvcrt,time
   if not lock.tell():lock.write(b'0');lock.flush()
   while True:
    try:lock.seek(0);msvcrt.locking(lock.fileno(),msvcrt.LK_NBLCK,1);break
    except OSError:time.sleep(.1)
  else:
   import fcntl
   fcntl.flock(lock.fileno(),fcntl.LOCK_EX)
  try:return _build(target,clang,cxx,out)
  finally:
   if os.name=='nt':lock.seek(0);msvcrt.locking(lock.fileno(),msvcrt.LK_UNLCK,1)
   else:fcntl.flock(lock.fileno(),fcntl.LOCK_UN)

def _build(target,clang,cxx,out):
 hb=V/'harfbuzz/src';sb=V/'sheenbidi';rt=V/'compiler-rt/builtins'
 sources=[SRC/'global_text.cc',SRC/'global_text.h',SRC/'unicode_support.c']+sorted(p for p in V.rglob('*') if p.is_file())
 signature=hashlib.sha256(('v7'+target).encode()+Path(__file__).read_bytes()+b''.join(str(p.relative_to(SRC)).encode()+p.read_bytes() for p in sources)).hexdigest()
 manifest=out/'build.json';lib=out/'libtext.a'
 if manifest.exists() and json.loads(manifest.read_text()).get('inputs')==signature and lib.exists():return lib
 common=BASE.copy();sysflags=[]
 if target=='arm':
  root=headers();common+=ARM+['--sysroot='+str(root)];sysflags=['-isystem',str(root/'include/c++/12'),'-isystem',str(root/'include/c++/12/arm-linux-gnueabi')]
 else:common+=['-DHOST_TEST=1']+(['-DCLIO_TEST_POOL=1'] if target=='host-pool' else [])
 objects=[]
 def compile(source,flags=(),cpp=False):
  obj=out/(str(source.relative_to(SRC)).replace('/','_').replace('.','_')+'.o')
  subprocess.run([cxx if cpp else clang,*common,*(sysflags if cpp else []),*flags,'-c',str(source),'-o',str(obj)],check=True)
  objects.append(obj)
 compile(hb/'harfbuzz.cc',['-std=c++11','-DHB_TINY','-DHB_CUSTOM_MALLOC','-fno-exceptions','-fno-rtti','-fno-threadsafe-statics'],True)
 compile(SRC/'global_text.cc',['-std=c++11','-I'+str(sb/'Headers'),'-DHB_NO_MT','-fno-exceptions','-fno-rtti','-fno-threadsafe-statics'],True)
 compile(sb/'Source/SheenBidi.c',['-std=c99','-DNDEBUG','-I'+str(sb/'Headers'),'-I'+str(sb/'Source'),'-DSB_CONFIG_UNITY','-DSB_CONFIG_ALLOW_NON_ATOMIC_FALLBACK','-DSB_CONFIG_DISABLE_SCRATCH_MEMORY','-DCLIO_SINGLE_THREAD','-Dmalloc=hb_malloc_impl','-Dcalloc=hb_calloc_impl','-Drealloc=hb_realloc_impl','-Dfree=hb_free_impl'])
 compile(SRC/'unicode_support.c',['-std=c99'])
 if target=='arm':
  names='fp_mode adddf3 addsf3 subdf3 subsf3 muldf3 mulsf3 divdf3 divsf3 comparedf2 comparesf2 extendsfdf2 truncdfsf2 fixdfsi fixunsdfsi fixsfsi fixunssfsi fixsfdi fixunssfdi fixdfdi fixunsdfdi floatsidf floatsisf floatdisf floatdidf floatunsidf floatunsisf floatundidf floatundisf divdi3 moddi3 divmoddi4 udivdi3 umoddi3 udivmoddi4 muldi3 ashldi3 ashrdi3 lshrdi3 clzsi2 clzdi2 negdi2'.split()
  for name in names:compile(rt/(name+'.c'),['-I'+str(rt)])
  for name in ('aeabi_fcmp','aeabi_dcmp','aeabi_ldivmod','aeabi_uldivmod'):compile(rt/'arm'/(name+'.S'),['-I'+str(rt)])
 archive='/opt/homebrew/opt/llvm/bin/llvm-ar' if target=='arm' and Path('/opt/homebrew/opt/llvm/bin/llvm-ar').exists() else 'ar'
 temporary=lib.with_name('libtext.new.a');temporary.unlink(missing_ok=True)
 subprocess.run([archive,'rcs',str(temporary),*map(str,objects)],check=True)
 os.replace(temporary,lib)
 manifest.write_text(json.dumps({'inputs':signature,'target':target,'sha256':hashlib.sha256(lib.read_bytes()).hexdigest()},indent=2)+'\n');return lib
if __name__=='__main__':
 import argparse
 p=argparse.ArgumentParser();p.add_argument('--target',choices=['arm','host'],default='arm');a=p.parse_args();print(build(a.target))
