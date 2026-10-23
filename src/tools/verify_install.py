#!/usr/bin/env python3
"""Clean-card install, atlas/flat switching and a late injected rollback failure."""
import argparse,json,shutil,sys,tempfile
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'navigation-builder/tools'))
import install_nav
from install_nav import install,validate_bundle,sha,bundle_names

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--prepared-runtime',type=Path,required=True);args=ap.parse_args()
 flat=ROOT/'build/global-fixtures/san-francisco/bundle';source=ROOT/'build/atlas-fixture/bundle'
 with tempfile.TemporaryDirectory(prefix='clioplay-install-test-') as t:
  t=Path(t);atlas=t/'atlas';atlas.mkdir();names=['atlas.cpa','regions.cpg','timezones.cpt','fonts.cpf','cameras.cpc']+[r+'/'+n for r in ('west','east') for n in ('map.cpm','route.cpr','route.qadj')]
  for n in names:
   p=atlas/n;p.parent.mkdir(parents=True,exist_ok=True);p.symlink_to(source/n)
  manifest={'builder':'global-atlas-2','regions':[{'path':r} for r in ('west','east')],'files':{n:{'bytes':(atlas/n).stat().st_size,'sha256':sha(atlas/n)} for n in names}}
  (atlas/'build.json').write_text(json.dumps(manifest));validate_bundle(atlas)
  card=t/'card';card.mkdir();backup=install(flat,card,args.prepared_runtime);assert (card/'ttn').is_file() and (card/'ttsystem').is_file() and (card/'vehicledaemon').is_file() and not (card/'PNDNavigator').exists();assert (card/'clio843/hidd').is_file();validate_bundle(card/'clio843/maps')
  (card/'clio843/prefs.ini').write_text('language=2\nunits=0\n');(card/'clio843/bond.dat').write_bytes(b'pairings-preserved')
  # Incomplete runtime and corrupt navigation must fail before target mutation.
  before={str(p.relative_to(card)):sha(p) for p in card.rglob('*') if p.is_file()}
  incomplete=t/'incomplete-runtime';shutil.copytree(args.prepared_runtime,incomplete)
  (incomplete/'vehicledaemon').write_bytes(b'corrupt-vendor-fixture')
  try:install(flat,card,incomplete);raise AssertionError('corrupt runtime accepted')
  except ValueError:pass
  bad=t/'bad-bundle';shutil.copytree(flat,bad)
  with (bad/'cameras.cpc').open('ab') as f:f.write(b'corrupt')
  try:install(bad,card,args.prepared_runtime);raise AssertionError('corrupt bundle accepted')
  except ValueError:pass
  assert before=={str(p.relative_to(card)):sha(p) for p in card.rglob('*') if p.is_file()}
  # Legacy eight-file cards are still readable; upgrades back up unused indexes.
  def legacy_bundle(current,kind):
   target=t/('legacy-'+kind);shutil.copytree(current,target,symlinks=False)
   m=json.loads((target/'build.json').read_text());m['builder']='global-'+kind+'-1'
   packs=[target/r['path'] for r in m['regions']] if kind=='atlas' else [target]
   for pack in packs:
    path=pack/'search.cps';path.write_bytes(b'CPS1-legacy-fixture')
    m['files'][str(path.relative_to(target))]={'bytes':path.stat().st_size,'sha256':sha(path)}
   (target/'build.json').write_text(json.dumps(m));validate_bundle(target);return target
  oldflat=legacy_bundle(flat,'navigation');oldatlas=legacy_bundle(atlas,'atlas')
  install(oldflat,card,args.prepared_runtime);backup=install(flat,card,args.prepared_runtime)
  assert not (card/'clio843/maps/search.cps').exists()
  assert (backup/'clio843/maps/search.cps').read_bytes()==b'CPS1-legacy-fixture'
  install(oldatlas,card,args.prepared_runtime);backup=install(atlas,card,args.prepared_runtime)
  for region in ('west','east'):
   assert not (card/'clio843/maps'/region/'search.cps').exists()
   assert (backup/'clio843/maps'/region/'search.cps').read_bytes()==b'CPS1-legacy-fixture'
  install(flat,card,args.prepared_runtime)
  backup=install(atlas,card,args.prepared_runtime);assert (card/'clio843/maps/atlas.cpa').is_file();assert not (card/'clio843/maps/map.cpm').exists();assert (backup/'clio843/maps/map.cpm').is_file();validate_bundle(card/'clio843/maps')
  backup=install(flat,card,args.prepared_runtime);assert not (card/'clio843/maps/atlas.cpa').exists();assert (backup/'clio843/maps/atlas.cpa').is_file();validate_bundle(card/'clio843/maps')
  install(oldatlas,card,args.prepared_runtime)
  before={str(p.relative_to(card)):sha(p) for p in (card/'clio843').rglob('*') if p.is_file()};replace=install_nav.os.replace;failed=[False]
  def fail(src,dst):
   if not failed[0] and '.clioplay-global-stage-' in str(src) and str(dst).endswith('/west/route.cpr'):failed[0]=True;raise OSError('injected late atlas commit failure')
   return replace(src,dst)
  with patch.object(install_nav.os,'replace',fail):
   try:install(atlas,card,args.prepared_runtime);raise AssertionError('injection not reached')
   except OSError:pass
  assert failed[0];assert before=={str(p.relative_to(card)):sha(p) for p in (card/'clio843').rglob('*') if p.is_file()};validate_bundle(card/'clio843/maps');assert (card/'clio843/prefs.ini').read_text()=='language=2\nunits=0\n';assert (card/'clio843/bond.dat').read_bytes()==b'pairings-preserved'
  # A failure near the end of a blank-card runtime commit removes every partial file.
  blank=t/'blank-failed';blank.mkdir();failed=[False]
  def fail_runtime(src,dst):
   if not failed[0] and '.clioplay-global-stage-' in str(src) and str(dst).endswith('/vehicledaemon'):
    failed[0]=True;raise OSError('injected final runtime commit failure')
   return replace(src,dst)
  with patch.object(install_nav.os,'replace',fail_runtime):
   try:install(flat,blank,args.prepared_runtime);raise AssertionError('runtime injection not reached')
   except OSError:pass
  assert failed[0]
  installed_names=set(json.loads((args.prepared_runtime/'card-runtime.json').read_text())['files'])
  installed_names.update('clio843/maps/'+n for n in bundle_names(json.loads((flat/'build.json').read_text())))
  installed_names.update(('clio843/maps/build.json','clio843/card-runtime.json'))
  assert all(not (blank/n).exists() for n in installed_names)
  # Redirected card directories must fail before mutation.
  redirected=t/'redirected';redirected.mkdir();(redirected/'clio843').symlink_to(card/'clio843',target_is_directory=True);(redirected/'ttn').write_text('existing boot')
  try:install(flat,redirected,args.prepared_runtime);raise AssertionError('symlink accepted')
  except ValueError:pass
 report={'clean_card':'verified supported firmware + complete runtime staged and installed into a temporary blank card directory','atlas_transitions':'flat → atlas → flat; obsolete active selector and packs backed up','rollback':'late atlas failure restores existing files; final runtime commit failure removes all partial blank-card files','legacy_upgrade':'v1 flat and atlas indexes backed up and removed on v2 upgrade','persistence':'preferences and pairing fixture unchanged','symlinks':'redirected card directory rejected before staging','corruption':'bad complete-runtime and navigation checksums rejected before target changes','hardware':'not connected; no vehicle claim'}
 (ROOT/'build/installer-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
