import sys,tempfile,unittest,struct
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'navigation-builder/tools'))
from build_cameras import extract,section_chain,build,HDR,REC
class Cameras(unittest.TestCase):
 def test_chain(self):
  ways={1:([1,2],{'oneway':'yes'}),2:([3,2],{'oneway':'-1'})}
  self.assertEqual(section_chain([2,1],ways),([1,2,3],{1},1,2))
  self.assertIsNone(section_chain([1,2],{**ways,2:([3,2],{'oneway':'yes'})}))
  self.assertIsNone(section_chain([1,2,3],{**ways,3:([2,4],{})}))
 def test_osm_sections(self):
  xml='''<osm version="0.6"><node id="1" lat="40.410" lon="-3.700"/><node id="2" lat="40.411" lon="-3.700"><tag k="highway" v="speed_camera"/><tag k="direction" v="forward"/></node><node id="3" lat="40.412" lon="-3.700"/><node id="4" lat="40.413" lon="-3.700"/><way id="10"><nd ref="1"/><nd ref="2"/><nd ref="3"/><tag k="highway" v="primary"/><tag k="oneway" v="yes"/></way><way id="20"><nd ref="3"/><nd ref="4"/><tag k="highway" v="primary"/><tag k="oneway" v="yes"/></way><relation id="100"><member type="way" ref="20" role="section"/><member type="way" ref="10" role="section"/><tag k="type" v="enforcement"/><tag k="enforcement" v="average_speed"/><tag k="maxspeed" v="80"/></relation><relation id="101"><member type="node" ref="1" role="from"/><member type="node" ref="2" role="device"/><member type="node" ref="3" role="device"/><member type="node" ref="4" role="to"/><tag k="type" v="enforcement"/><tag k="enforcement" v="average_speed"/></relation></osm>'''
  with tempfile.TemporaryDirectory() as t:
   p=Path(t)/'fixture.osm';p.write_text(xml);rows,skipped=extract(p)
   self.assertEqual(skipped,0);self.assertEqual(len(rows),2)
   self.assertEqual(rows[0]['latitude'],40.410);self.assertEqual(rows[0]['end_latitude'],40.413);self.assertEqual(rows[0]['heading'],0);self.assertEqual(rows[0]['osm_way_id'],10)
   out=Path(t)/'cameras.cpc';report=build(p,[],out);h=HDR.unpack_from(out.read_bytes());self.assertEqual(h[:3],(b'CPC1',2,40));self.assertEqual(report['matched_road_records'],1)
if __name__=='__main__':unittest.main()
