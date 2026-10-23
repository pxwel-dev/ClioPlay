#!/usr/bin/env python3
"""Publish/check the ClioPlay clean-install source package."""
import argparse,hashlib,json,os,shutil,tempfile
from publication import source_files,gitignore,source_gitignore
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT.parent if ROOT.name=='src' and (ROOT.parent/'latest.json').is_file() else ROOT.parent/'CLIOPLAY-LATEST-VERSION'
PROGRAMS=('clio040','cliospl','clioa2dp')

def sha(path):
 h=hashlib.sha256()
 with Path(path).open('rb') as f:
  for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
 return h.hexdigest()

def inputs():
 from build_ui import inspect_elf
 files={};reports={};expected={}
 for name in PROGRAMS:
  report=ROOT/'build'/f'{name}.build.json'
  if not report.exists():report=ROOT/'build-reports'/f'{name}.build.json'
  data=json.loads(report.read_text());binary=ROOT/'clio843'/name
  if inspect_elf(binary)['sha256']!=data['sha256'] or sha(ROOT/'clio843'/name)!=data['sha256']:
   raise ValueError('Compile and install the current program first: '+name)
  for rel,digest in data['inputs'].items():
   if rel in expected and expected[rel]!=digest:raise ValueError('Programs were built from different sources: '+rel)
   expected[rel]=digest
  reports[name]=data
 for rel,digest in expected.items():
  if not (ROOT/rel).is_file() or sha(ROOT/rel)!=digest:raise ValueError('Changed source; compile and install first: '+rel)
 font=ROOT/'navigation-builder/data/global-fonts.cpf';info=json.loads(font.with_suffix('.cpf.json').read_text())
 if sha(font)!=info['sha256']:raise ValueError('Global font checksum mismatch')
 files.update(source_files(ROOT))
 for name in ('LICENSE','THIRD_PARTY_NOTICES.md'):
  files[name]=ROOT/name
 files['SOURCE_STATUS.md']=ROOT/'docs/source-status.md'
 files['Install.command']=ROOT/'tools/Publication-Install.command'
 files['install.py']=ROOT/'tools/publication_install.py'
 for key,p in list(files.items()):
  if key.startswith('src/repository/'):
   name=key.removeprefix('src/repository/')
   if name in files:raise ValueError('Duplicate repository metadata: '+name)
   files[name]=p
 return files,reports

def readme(updated):
 template=(ROOT/'docs/repository-readme.md').read_text()
 film=json.loads((ROOT/'docs/demo/manifest.json').read_text())
 seconds=round(film['duration_seconds']);duration=f'{seconds//60}:{seconds%60:02}'
 date=datetime.fromisoformat(updated).strftime('%d %B %Y')
 return template.replace('@@UPDATED@@',date).replace('@@DURATION@@',duration).encode()


def safe_target(output,name):
 p=Path(name)
 if p.is_absolute() or '..' in p.parts:raise ValueError('Invalid managed path: '+name)
 target=output/p
 for part in [target,*target.parents]:
  if part.is_symlink():raise ValueError('Refusing redirected delivery path: '+str(part))
  if part==output:break
 return target

def refresh(output=DEFAULT,check=False):
 output=Path(output).absolute()
 portable=ROOT.name=='src' and output==ROOT.parent and (output/'latest.json').is_file()
 if not portable and (output==ROOT or output in ROOT.parents or ROOT in output.parents):raise ValueError('Choose a delivery folder beside the Global source, outside it')
 if output.is_symlink():raise ValueError('Delivery folder is a symbolic link')
 previous=json.loads((output/'latest.json').read_text()) if (output/'latest.json').exists() else None
 if portable and previous.get('format')!='clioplay-latest-1':raise ValueError('Not a ClioPlay publication folder')
 files,reports=inputs()
 # Source fingerprints use publication-relative identities, never the local
 # build/ path of a freshly generated report. A Git checkout has only the
 # corresponding build-reports/ file, with the same content.
 source_names={p:name.removeprefix('src/') for name,p in files.items() if name.startswith('src/')}
 for p in files.values():source_names.setdefault(p,str(p.relative_to(ROOT)))
 source_paths={name:p for p,name in source_names.items()}
 source={name:sha(p) for name,p in source_paths.items()}
 if check:
  if not previous:raise ValueError('Latest folder has not been generated')
  if previous['source_sha256']!=source:raise ValueError('Project changed; refresh the latest folder')
  for name,digest in previous['files'].items():
   p=safe_target(output,name)
   if not p.is_file() or sha(p)!=digest:raise ValueError('Latest file missing or changed: '+name)
  print('Latest folder verified:',output);return previous
 tests={}
 updated=datetime.now(ZoneInfo('Europe/London')).isoformat(timespec='seconds')
 generated={'README.md':readme(updated),'.gitignore':gitignore(),'src/.gitignore':source_gitignore(ROOT)}
 output.parent.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='.clioplay-latest-stage-',dir=output.parent) as td:
  stage=Path(td)
  for name,p in files.items():
   if name in generated:continue
   dest=stage/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,dest);dest.chmod(0o755 if name.startswith('src/') and p.stat().st_mode&0o111 or name.endswith('.command') else 0o644)
  for name,raw in generated.items():
   dest=stage/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw)
  # Catch an edit during verification/copying before touching the delivery tree.
  if any(sha(source_paths[name])!=digest for name,digest in source.items()):raise ValueError('Source changed while preparing latest files; retry')
  for name,p in files.items():
   if name not in generated and sha(stage/name)!=source[source_names[p]]:raise ValueError('Staged copy checksum mismatch: '+name)
  from verify_publication import verify_tree
  tests['publication']=verify_tree(stage)
  hashes={str(p.relative_to(stage)):sha(p) for p in sorted(stage.rglob('*')) if p.is_file()}
  checksum=''.join(digest+'  '+name+'\n' for name,digest in hashes.items());(stage/'SHA256SUMS').write_text(checksum);hashes['SHA256SUMS']=sha(stage/'SHA256SUMS')
  manifest={'format':'clioplay-latest-1','updated_at':updated,'edition':'Global','payload':'Clean-install source package, current binaries, legacy bootstrap, CAB-based installer; no vendor firmware, navigation packs or companion website','executables':{name:reports[name]['sha256'] for name in PROGRAMS},'source_sha256':source,'files':hashes,'checks':tests}
  for name in [*hashes,'latest.json',*(previous or {}).get('files',{})]:safe_target(output,name)
  output.mkdir(parents=True,exist_ok=True)
  for name in hashes:
   dest=output/name;dest.parent.mkdir(parents=True,exist_ok=True)
   if dest.is_file() and sha(dest)==hashes[name]:
    mode=(stage/name).stat().st_mode&0o777
    if dest.stat().st_mode&0o777!=mode:dest.chmod(mode)
   else:os.replace(stage/name,dest)
  # Remove only obsolete files listed by a previous export; preserve personal additions.
  for name in set((previous or {}).get('files',{}))-set(hashes):
   obsolete=safe_target(output,name);obsolete.unlink(missing_ok=True)
   parent=obsolete.parent
   while parent!=output:
    try:parent.rmdir()
    except OSError:break
    parent=parent.parent
  temporary=stage/'latest.json';temporary.write_text(json.dumps(manifest,indent=2)+'\n');os.replace(temporary,output/'latest.json')
 print('Updated latest folder:',output)
 return refresh(output,check=True)

if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=DEFAULT);parser.add_argument('--check',action='store_true');args=parser.parse_args();refresh(args.output,args.check)
