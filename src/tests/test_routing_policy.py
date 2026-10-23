import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'navigation-builder/tools'))
from routing_policy import direction,barrier_blocked,split_at_barriers
import build_route as r
class Access(unittest.TestCase):
 def test_specificity(self):
  self.assertIsNone(direction({'motorcar':'no'}))
  self.assertEqual(direction({'access':'no','motorcar':'yes'}),0)
  self.assertEqual(direction({'motor_vehicle':'no','motorcar':'permissive'}),0)
  self.assertIsNone(direction({'vehicle':'yes','motorcar':'private'}))
  self.assertEqual(direction({'access':'no','vehicle':'yes'}),0)
 def test_direction(self):
  self.assertEqual(direction({'motorcar:forward':'no'}),-1)
  self.assertEqual(direction({'motorcar:backward':'no'}),1)
  self.assertIsNone(direction({'oneway':'yes','motorcar:forward':'no'}))
  self.assertEqual(direction({'oneway':'yes','oneway:motorcar':'no'}),0)
  self.assertEqual(direction({'oneway':'-1'}),-1)
  self.assertEqual(direction({'junction':'roundabout'}),1)
  self.assertEqual(direction({'junction':'roundabout','oneway':'no'}),0)
 def test_unresolved_rules_never_open_a_prohibition(self):
  self.assertIsNone(direction({'motorcar:conditional':'no @ (Mo-Fr 08:00-09:00)'}))
  self.assertEqual(direction({'access:conditional':'no @ (wet)','motorcar':'yes'}),0)
  self.assertIsNone(direction({'access':'private','motorcar:conditional':'yes @ (Mo-Fr)'}))
  self.assertIsNone(direction({'oneway':'reversible'}))
  self.assertIsNone(direction({'oneway:conditional':'yes @ (Mo-Fr)'}))
  self.assertIsNone(direction({'access':'customers'}))
 def test_physical_barrier(self):
  self.assertTrue(barrier_blocked({'barrier':'bollard'}))
  self.assertTrue(barrier_blocked({'motorcar':'no'}))
  self.assertFalse(barrier_blocked({'barrier':'gate','motorcar':'yes','access':'no'}))
  self.assertFalse(barrier_blocked({'barrier':'gate'}))
  obj={'properties':{'@way_nodes':[1,2,3,4,5],'@id':10},'geometry':{'type':'LineString','coordinates':[[i,0] for i in range(5)]}}
  pieces=list(split_at_barriers(obj,{3}));self.assertEqual([p['properties']['@way_nodes'] for p in pieces],[[1,2],[4,5]])
 def test_graph_uses_policy(self):
  obj={'properties':{'@id':1,'@way_nodes':[1,2],'highway':'residential','motorcar':'no'},'geometry':{'type':'LineString','coordinates':[[0,0],[.001,0]]}}
  self.assertIsNone(r.parse_way(obj));obj['properties'].update({'motorcar':'yes','access':'no','motorcar:backward':'no'});self.assertEqual(r.parse_way(obj)[-1],1)
if __name__=='__main__':unittest.main()
