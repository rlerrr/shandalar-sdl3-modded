"""The growth family refuses silently changed spell or permanent witnesses."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from growth_counter_spell_recipes import validate_witnesses

class GrowthCounterSpellWitnessesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def test_pinned_witnesses(self):validate_witnesses(self.oracle,self.rows)
 def test_changed_cost_text_stats_or_type_rejected(self):
  for name,field,value in (('Burst of Strength','oracle_text','Put two counters on target creature.'),('Counterspell','mana_cost','{U}'),('Grizzly Bears','toughness','3'),('Sol Ring','type_line','Creature'),('Savannah Lions','layout','transform')):
   oracle=copy.deepcopy(self.oracle);oracle[name][field]=value
   with self.assertRaises(ValueError):validate_witnesses(oracle,self.rows)
 def test_missing_witness_identity_rejected(self):
  rows=dict(self.rows);del rows['Scaled Wurm']
  with self.assertRaises(ValueError):validate_witnesses(self.oracle,rows)
 def test_extension_cost_text_and_face_pins_are_independent(self):
  for name,field,value in (('Murder','mana_cost','{B}'),('Shock','oracle_text','Shock deals3damage to any target.'),('Hill Giant','power','4'),('Murder','card_faces',[{}])):
   oracle=copy.deepcopy(self.oracle);oracle[name][field]=value
   with self.assertRaises(ValueError):validate_witnesses(oracle,self.rows)
 def test_linked_witnesses_are_full_pins_for_sacrifice_flying_and_basic_land(self):
  for name,field,value in (('Greed','oracle_text','Draw a card.'),('Suntail Hawk','keywords',[]),('Wasteland','type_line','Basic Land — Forest')):
   oracle=copy.deepcopy(self.oracle);oracle[name][field]=value
   with self.assertRaises(ValueError):validate_witnesses(oracle,self.rows)
 def test_manifest_observation_identity_is_pinned_without_enrolling_internal_card(self):
  rows=copy.deepcopy(self.rows);rows['Face-Down Creature']['csvid']=13253
  with self.assertRaises(ValueError):validate_witnesses(self.oracle,rows)

if __name__=='__main__':unittest.main()
