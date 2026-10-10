"""Aura lifetime, ownership, real removal, and independent Oracle expectations."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand, explicit_spec


class AuraRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry={r['name']:r for r in registry()}

    def declaration(self,name,p,variants):
        return dict(family='aura_creature',version=2,params=p,variants=variants,oracle_text=self.oracle[name]['oracle_text'])

    def cases(self,name,d,registry=None,oracle=None):
        return expand(name,self.registry[name]['csvid'],d,self.oracle[name],registry or self.registry,oracle_context=oracle or self.oracle)

    def test_type_text_parameters_and_scope_fail_closed(self):
        p=dict(power=3,toughness=3,keywords=[])
        d=self.declaration('Oakenform',p,['friendly_cleanup'])
        self.cases('Oakenform',d)
        for change in (dict(oracle_text='New text'),dict(params=dict(p,power=-1)),dict(params=dict(p,power=11)),dict(params=dict(p,return_aura=False))):
            with self.assertRaises(ValueError):self.cases('Oakenform',dict(d,**change))
        for name in ('Giant Growth','Faith Unbroken'):
            with self.assertRaises(ValueError):explicit_spec(d,self.oracle[name])
        self.cases('Rime Transfusion',self.declaration('Rime Transfusion',dict(power=2,toughness=1,keywords=[]),['payment']))

    def test_registry_handlers_and_costs_do_not_supply_assertions(self):
        d=self.declaration('Oakenform',dict(power=3,toughness=3,keywords=[]),['snapshot'])
        altered=copy.deepcopy(self.registry)
        for r in altered.values():r['handler']='card_dummy';r['mana_cost']='{99}'
        self.assertEqual(self.cases('Oakenform',d),self.cases('Oakenform',d,registry=altered))
        self.assertIn(dict(op='assert_recorded_state'),self.cases('Oakenform',d)[0]['steps'])

    def test_cleanup_preserves_aura_and_opposing_attachment_ownership(self):
        d=self.declaration('Oakenform',dict(power=3,toughness=3,keywords=[]),['friendly_cleanup','opponent_cleanup'])
        own,opposing=self.cases('Oakenform',d)
        for c in (own,opposing):
            steps=c['steps'];tail=steps[steps.index(dict(op='cleanup')):]
            self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=5),tail)
            self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),tail)
        self.assertEqual(next(c['player'] for c in opposing['setup'] if c['ref']=='source'),0)
        self.assertEqual(next(c['player'] for c in opposing['setup'] if c['ref']=='recipient'),1)
        for p in (0,1):self.assertIn(dict(op='assert_zone',player=p,zone='battlefield',value=2),opposing['steps'])

    def test_removed_ability_uses_disenchant_before_cleanup_and_combat(self):
        d=self.declaration('Flight',dict(power=0,toughness=0,keywords=['Flying']),['expired_flying','aura_bounced'])
        removed,bounced=self.cases('Flight',d)
        s=removed['steps']
        self.assertLess(s.index(dict(op='cast',ref='removal',value=1)),s.index(dict(op='cleanup')))
        self.assertLess(s.index(dict(op='cleanup')),s.index(dict(op='attack',ref='recipient',value=1)))
        self.assertIn(dict(op='assert_mana_total',player=0,value=57),s)
        self.assertIn(dict(op='assert_zone_contains',player=0,zone='graveyard',value=87),s)
        self.assertEqual(next(c['name'] for c in bounced['setup'] if c['ref']=='bounce'),'Disperse')
        self.assertIn(dict(op='assert_zone_contains',player=0,zone='hand',value=87),bounced['steps'])

    def test_returning_aura_returns_only_from_battlefield_not_a_fizzled_spell(self):
        d=self.declaration('Rancor',dict(power=2,toughness=0,keywords=['Trample'],return_aura=True),['aura_removed','host_bounced','host_destroyed','lost_target'])
        cases=self.cases('Rancor',d)
        for c in cases[:3]:
            self.assertIn(dict(op='assert_zone_contains',player=0,zone='hand',value=4046),c['steps'])
            self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),c['steps'])
        self.assertIn(dict(op='assert_zone_contains',player=0,zone='graveyard',value=4046),cases[-1]['steps'])
        self.assertIn(dict(op='choose_priority',ref='response'),cases[-1]['steps'])

    def test_entry_draw_is_skipped_for_lost_target_and_lifelink_belongs_to_host(self):
        d=self.declaration('Flight of Fancy',dict(power=0,toughness=0,keywords=['Flying'],draw=2),['friendly_cleanup','lost_target'])
        normal,lost=self.cases('Flight of Fancy',d)
        for c,n in ((normal,5),(lost,7)):self.assertIn(dict(op='assert_zone',player=0,zone='library',value=n),c['steps'])
        d=self.declaration('Mark of the Vampire',dict(power=2,toughness=2,keywords=['Lifelink']),['opponent_unblocked'])
        c=self.cases('Mark of the Vampire',d)[0];s=c['steps']
        self.assertLess(s.index(dict(op='cast',ref='source',value=1)),s.index(dict(op='turn',player=1,value=0)))
        self.assertIn(dict(op='assert_life',player=1,value=24),s)
        self.assertIn(dict(op='assert_life',player=0,value=16),s)

    def test_vigilance_haste_and_shroud_use_normal_game_actions(self):
        d=self.declaration('Vigilance',dict(power=0,toughness=0,keywords=['Vigilance']),['unblocked'])
        s=self.cases('Vigilance',d)[0]['steps']
        self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=0),s)
        self.assertIn(dict(op='advance_combat'),s)
        d=self.declaration('Goblin War Paint',dict(power=2,toughness=2,keywords=['Haste']),['fresh_grant'])
        self.assertIn(dict(op='cast',ref='recipient',value=1),self.cases('Goblin War Paint',d)[0]['steps'])
        d=self.declaration('Robe of Mirrors',dict(power=0,toughness=0,keywords=['Shroud']),['host_bounced'])
        with self.assertRaises(ValueError):self.cases('Robe of Mirrors',d)

    def test_removal_witness_changes_require_review(self):
        d=self.declaration('Flight',dict(power=0,toughness=0,keywords=['Flying']),['aura_removed'])
        o=copy.deepcopy(self.oracle);o['Disenchant']['oracle_text']='Different destruction'
        with self.assertRaises(ValueError):self.cases('Flight',d,oracle=o)

    def test_duplicate_presentation_targets_preserve_one_intended_host(self):
        d=self.declaration('Fearsome Temper',dict(power=2,toughness=2,keywords=[]),['friendly_cleanup'])
        d['cast_target_count']=2
        c=self.cases('Fearsome Temper',d)[0]
        self.assertEqual([s for s in c['steps'] if s['op']=='choose_target'],[dict(op='choose_target',player=0,ref='recipient')]*2)
        self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=4),c['steps'])
        for n in (True,0,3):
            with self.assertRaises(ValueError):self.cases('Fearsome Temper',dict(d,cast_target_count=n))


if __name__=='__main__':unittest.main()
