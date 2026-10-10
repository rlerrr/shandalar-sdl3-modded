"""Oracle-backed evasion expectations and fail-closed recipe authoring."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from evasion_creature_recipes import variants,may_block,characteristics,validate_witnesses

class EvasionCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry={r['name']:r for r in registry()}
    def declaration(self,name='Accursed Spirit',selected=None,keys=None,unblockable=False):
        p=dict(keywords=['Intimidate'] if keys is None else keys)
        if unblockable:p['unblockable']=True
        return dict(family='evasion_creature',version=2,oracle_text=self.oracle[name]['oracle_text'],params=p,variants=selected or ['attack_artifact'])
    def cases(self,name,d,registry=None,oracle=None):
        return expand(name,self.registry[name]['csvid'],d,self.oracle[name],registry or self.registry,oracle_context=oracle or self.oracle)
    def test_pin_scope_and_explicit_params(self):
        d=self.declaration();self.cases('Accursed Spirit',d)
        for change in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(keywords=['Fear'])),dict(params=dict(keywords=['Intimidate'],unblockable=False)),dict(params=dict(keywords=['Intimidate'],extra=True))):
            with self.assertRaises(ValueError):self.cases('Accursed Spirit',dict(d,**change))
        for name in ('Infantry Veteran','Furnace Whelp','Hypnotic Specter'):
            with self.assertRaises(ValueError):explicit_spec(dict(d,oracle_text=self.oracle[name]['oracle_text']),self.oracle[name])
    def test_fear_black_and_artifact_are_independent_exceptions(self):
        d=self.declaration('Severed Legion',keys=['Fear']);p=explicit_spec(d,self.oracle['Severed Legion'])
        for w,legal in [('black',True),('artifact',True),('colored_artifact',True),('white',False),('colorless',False)]:
            self.assertEqual(may_block(p,characteristics(w)),legal,w)
    def test_intimidate_matches_source_color_not_black(self):
        d=self.declaration('Bladetusk Boar');p=explicit_spec(d,self.oracle['Bladetusk Boar'])
        self.assertTrue(may_block(p,characteristics('red')))
        self.assertFalse(may_block(p,characteristics('black')))
        self.assertTrue(may_block(p,characteristics('colored_artifact')))
        self.assertFalse(may_block(p,characteristics('colorless')))
    def test_shadow_is_symmetric_and_combines_with_flying(self):
        d=self.declaration('Dauthi Marauder',keys=['Shadow']);p=explicit_spec(d,self.oracle['Dauthi Marauder'])
        self.assertFalse(may_block(p,characteristics('green')))
        self.assertFalse(may_block(characteristics('green'),p))
        self.assertTrue(may_block(p,characteristics('shadow')))
        flying=dict(p,keywords=['Flying','Shadow'])
        self.assertFalse(may_block(flying,characteristics('shadow')))
        self.assertTrue(may_block(flying,characteristics('shadow_flying')))
    def test_horsemanship_only_restricts_attacking_creature(self):
        d=self.declaration('Shu Cavalry',keys=['Horsemanship']);p=explicit_spec(d,self.oracle['Shu Cavalry'])
        self.assertFalse(may_block(p,characteristics('flying')))
        self.assertTrue(may_block(p,characteristics('horse')))
        self.assertTrue(may_block(characteristics('green'),p))
    def test_unblockable_does_not_prevent_blocking(self):
        d=self.declaration('Phantom Warrior',keys=[],unblockable=True);p=explicit_spec(d,self.oracle['Phantom Warrior'])
        self.assertFalse(may_block(p,characteristics('artifact')))
        self.assertTrue(may_block(characteristics('green'),p))
        self.assertEqual(set(p['keywords']),set())
    def test_landwalk_uses_defenders_battlefield_and_real_removal(self):
        d=self.declaration('River Boa',keys=['Islandwalk'],selected=['land_attacker_only','land_tapped','land_removed','opponent_land_matching'])
        own,tapped,removed,opponent=self.cases('River Boa',d)
        self.assertEqual(own['setup'][-1]['player'],0)
        self.assertIn(dict(op='block',ref='witness',other='source',value=1),own['steps'])
        self.assertEqual(tapped['setup'][-1]['state'],['tapped'])
        self.assertIn(dict(op='block',ref='witness',other='source',value=0),tapped['steps'])
        s=removed['steps'];self.assertLess(s.index(dict(op='cast',ref='response',value=1)),s.index(dict(op='begin_blockers')))
        self.assertIn(dict(op='assert_zone_contains',player=1,zone='hand',value=self.registry['Island']['csvid']),s)
        self.assertIn(dict(op='assert_zone_contains',player=0,zone='graveyard',value=self.registry['Boomerang']['csvid']),s)
        self.assertEqual(opponent['setup'][0]['player'],1);self.assertEqual(opponent['setup'][-1]['player'],0)
    def test_stocked_zones_combat_damage_cleanup_and_snapshot(self):
        d=self.declaration(selected=['evasion_damage','combat_snapshot','cast']);cases=self.cases('Accursed Spirit',d)
        self.assertTrue(all(c['fixture']=='stocked_zones' for c in cases))
        by_variant={c['_recipe']['variant']:c for c in cases}
        damage,snapshot=by_variant['evasion_damage'],by_variant['combat_snapshot']
        self.assertIn(dict(op='combat_damage'),damage['steps'])
        self.assertIn(dict(op='assert_life',player=1,value=17),damage['steps'])
        self.assertIn(dict(op='capture'),snapshot['steps']);self.assertIn(dict(op='ai_roundtrip'),snapshot['steps'])
        self.assertIn(dict(op='assert_recorded_state'),snapshot['steps'])
        for player in (0,1):
            self.assertIn(dict(op='assert_zone',player=player,zone='library',value=7),snapshot['steps'])
            self.assertIn(dict(op='assert_zone',player=player,zone='exile',value=2),snapshot['steps'])
    def test_handlers_do_not_supply_expected_behavior_and_other_modes_are_gaps(self):
        name='Stronghold Overseer';d=self.declaration(name,keys=['Flying','Shadow'],selected=['attack_shadow','cast'])
        altered=copy.deepcopy(self.registry)
        for r in altered.values():r['handler']='card_dummy';r['mana_cost']='{99}'
        self.assertEqual(self.cases(name,d),self.cases(name,d,registry=altered))
        for c in self.cases(name,d):self.assertTrue(c['_recipe']['untested_abilities'])
    def test_witness_changes_and_unsupported_variants_fail_closed(self):
        changed=copy.deepcopy(self.oracle);changed['Ornithopter']['colors']=['R']
        with self.assertRaises(ValueError):validate_witnesses(changed,self.registry)
        with self.assertRaises(ValueError):self.cases('Accursed Spirit',dict(self.declaration(),variants=['land_matching']))
        with self.assertRaises(ValueError):self.cases('Accursed Spirit',dict(self.declaration(),cast_dialogs=[0]))

if __name__=='__main__':unittest.main()
