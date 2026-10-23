#!/usr/bin/env python3
"""Check the real CPC exporter and native alerts across reviewed/pending regions."""
import csv,hashlib,json,shutil,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'navigation-builder/tools'))
from global_regions import Regions,POLICY,CAMERA_ALLOWED,PROFILES
from build_cameras import build,HDR,REC

def main():
 points={'AT':(48.2082,16.3738),'BE':(50.8503,4.3517),'CZ':(50.0755,14.4378),'DK':(57.0488,9.9217),'ES':(40.4168,-3.7038),'FI':(60.1699,24.9384),'GB':(51.5074,-.1278),'HR':(45.8150,15.9819),'HU':(47.4979,19.0402),'IT':(41.9028,12.4964),'LU':(49.765,5.89),'NL':(52.3676,4.9041),'NO':(59.9139,10.7522),'PL':(52.2297,21.0122),'PT':(38.7223,-9.1393),'RS':(44.7866,20.4489),'RU':(55.7558,37.6173),'SE':(59.3293,18.0686),'SI':(46.0569,14.5058),
 'FR':(48.8566,2.3522),'DE':(52.52,13.405),'CH':(47.3769,8.5417),'CY':(34.9,33.0),'TR':(39.9334,32.8597),'GR':(37.9838,23.7275),'UA':(50.4501,30.5234),'SA':(24.7136,46.6753),'AE':(24.4539,54.3773),'EG':(30.0444,31.2357),'CN':(39.9042,116.4074),'JP':(35.6762,139.6503),'KR':(37.5665,126.9780),'IN':(28.6139,77.2090),'IL':(32.0853,34.7818),'US':(38.9072,-77.0369),'CA':(45.4215,-75.6972),'AU':(-35.2809,149.13),'NZ':(-41.2866,174.7756),'IE':(53.3498,-6.2603),'BR':(-15.7975,-47.8919)}
 assert set(CAMERA_ALLOWED)<=set(points)
 assert set(POLICY['policy_states'])==set(PROFILES)
 languages=json.loads((ROOT/'locales/ui.json').read_text())['languages']
 assert set(POLICY['language_country_examples'])==set(languages)
 assert all(code in PROFILES for examples in POLICY['language_country_examples'].values() for code in examples)
 assert all(POLICY['review_sources'].get(code) for code in CAMERA_ALLOWED)
 regions=Regions()
 with tempfile.TemporaryDirectory(prefix='clioplay-policy-') as td:
  td=Path(td);maps=td/'clio843/maps';maps.mkdir(parents=True);regions.write(maps/'regions.cpg')
  rows=[];xml=[]
  for i,(iso,(lat,lon)) in enumerate(points.items(),1):
   assert regions.country(lat,lon)[0]==iso,(iso,regions.country(lat,lon))
   assert regions.camera_allowed(lat,lon)==(iso in CAMERA_ALLOWED),iso
   rows.append({'id':iso,'latitude':lat+.0015,'longitude':lon,'type':'fixed','heading':0,'maxspeed':50})
   xml.append(f'<node id="{i}" lat="{lat+.0015}" lon="{lon}"><tag k="highway" v="speed_camera"/><tag k="maxspeed" v="50"/></node>')
  source=td/'fixture.osm';source.write_text('<osm version="0.6">'+''.join(xml)+'</osm>')
  imports=td/'fixture.csv'
  with imports.open('w') as f:
   writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
  reports=[]
  for pbf,csvs in [(source,[]),(None,[imports])]:
   report=build(pbf,csvs,maps/'cameras.cpc',regions);raw=(maps/'cameras.cpc').read_bytes();header=HDR.unpack_from(raw)
   exported={REC.unpack_from(raw,header[4]+i*REC.size)[9].decode() for i in range(header[3])}
   assert exported==CAMERA_ALLOWED,(exported,CAMERA_ALLOWED);assert report['policy_excluded']==len(points)-len(CAMERA_ALLOWED);reports.append(report)
  # Imported country labels cannot authorize a location; average sections require
  # both ends to pass policy, and the conservative restricted-border buffer stays.
  crossing=td/'crossing.csv';crossing.write_text('id,latitude,longitude,type,end_latitude,end_longitude\ncross,40.4168,-3.7038,average,48.8566,2.3522\n')
  report=build(None,[crossing],td/'blocked.cpc',regions);assert report['records']==0 and report['policy_excluded']==1
  assert not regions.camera_allowed(43.353,-1.79) # Spanish side near France
  cases='struct PolicyCase {const char*iso;int lat,lon,enabled;};\nstatic const struct PolicyCase camera_policy_cases[]={\n'+''.join('{"'+iso+'",'+str(round(lat*1e5))+','+str(round(lon*1e5))+','+str(int(iso in CAMERA_ALLOWED))+'},\n' for iso,(lat,lon) in points.items())+'};\n'
  (td/'camera_policy_cases.h').write_text(cases);exe=td/'test'
  subprocess.run(['cc','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(td),'-DCLIO_HOST_ROOT="'+str(td)+'/"',str(ROOT/'tests/camera-policy.c'),'-o',str(exe)],check=True)
  result=subprocess.check_output([str(exe)],text=True);print(result)
 report={'policy_sha256':hashlib.sha256((ROOT/'navigation-builder/data/camera-policy.json').read_bytes()).hexdigest(),'enabled':sorted(CAMERA_ALLOWED),'blocked':POLICY['blocked_exact_locations'],'explicit_territories':len(PROFILES),'sample_countries':len(points),'osm_and_csv_exports':reports,'average_crossing_and_border_buffer':'passed','native':result,'hardware':'not tested'}
 output=ROOT/'docs/next-release/camera-policy-verification.json';output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
