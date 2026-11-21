#!/usr/bin/env python3
"""Install ClioPlay Global, or update the software on an existing card."""
from pathlib import Path
import argparse,os,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parent
BUILDER=ROOT/'navigation-builder'
def main(argv=None):
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--update',type=Path,metavar='SD_ROOT',help='update existing software without rebuilding maps or downloading firmware')
 parser.add_argument('--dry-run',action='store_true',help='validate and list an update without changing the card')
 parser.add_argument('--pbf',type=Path,action='append',default=[],metavar='PATH',help='OSM extract for a fresh installation, repeat for more regions')
 parser.add_argument('--speed-import',type=Path,help='optional source-attributed missing speed-limit corrections')
 args=parser.parse_args(argv)
 if args.update is not None:
  if args.speed_import:parser.error("--speed-import requires a fresh navigation build, not --update")
  if args.pbf:parser.error("--pbf requires a fresh navigation build, not --update")
  sys.path.insert(0,str(ROOT/'tools'))
  from update_card import run_update
  try:run_update(args.update,args.dry_run)
  except (OSError,ValueError,RuntimeError) as error:parser.exit(1,f"{error}\n")
  return
 if args.dry_run:parser.error("--dry-run requires --update")
 if args.speed_import and not args.speed_import.expanduser().is_file():parser.error('Speed import not found')
 if not sys.stdin.isatty():raise SystemExit('Run interactively in a terminal; build_navigation.py supports unattended builds.')
 print('ClioPlay Global · clean installation')
 try:
  command=[sys.executable,str(BUILDER/'build_navigation.py'),
           *[str(path.expanduser().resolve()) for path in args.pbf],
           '--output',str(ROOT/'navigation-output')]
  if args.speed_import:command+=['--speed-import',str(args.speed_import.expanduser().resolve())]
  subprocess.run(command,check=True)
 except subprocess.CalledProcessError:
  raise SystemExit('Navigation build did not complete. The target has not been changed. Run the installer again to resume completed atlas regions.') from None
 print('The complete navigation bundle has been built and checked.')
 path=input('SD-card root or local folder, or Enter to keep navigation files only: ').strip()
 if not path:return
 import shlex
 parts=shlex.split(path);sd=Path(parts[0] if len(parts)==1 else path).expanduser().resolve()
 if not sd.is_dir():raise SystemExit('Create the target folder or mount the SD card first.')
 sys.path.insert(0,str(ROOT/'tools'))
 from prepare_card import prepare
 with tempfile.TemporaryDirectory(prefix='clioplay-clean-runtime-') as temp:
  runtime=Path(temp)
  try:
   prepare(runtime)
  except (OSError,ValueError,subprocess.CalledProcessError) as error:
   raise SystemExit(f'Runtime preparation failed: {error} The target has not been changed.') from None
  print(f'Install the complete Global runtime, Navcore compatibility files and navigation onto {sd}. Replaced files are backed up; existing preferences and paired phones are retained.')
  if input('Install onto this target? [y/N] ').strip().lower() not in ('y','yes'):return
  python=BUILDER/'.venv'/('Scripts/python.exe' if os.name=='nt' else 'bin/python')
  subprocess.run([str(python if python.exists() else sys.executable),str(BUILDER/'tools/install_nav.py'),str(ROOT/'navigation-output'),str(sd),'--runtime',str(runtime)],check=True)
 print('Installation complete. Safely eject the card before starting the car.')
if __name__=='__main__':main()
