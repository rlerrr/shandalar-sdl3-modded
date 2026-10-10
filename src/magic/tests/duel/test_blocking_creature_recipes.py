"""Guard Oracle scope and independent blocking expectations."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import explicit_spec, expand
from blocking_creature_recipes import may_block, variants, validate_witnesses


class BlockingCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']:r for r in registry()}

    def declaration(self, name='Goblin Raider', restriction='none', limit=0, keys=None, selected=None):
        return dict(family='blocking_creature', version=2, params=dict(keywords=keys or [], restriction=restriction, power_limit=limit),
                    oracle_text=self.oracle[name]['oracle_text'], variants=selected or ['cast'])

    def cases(self, name, d, rows=None):
        return expand(name, self.rows[name]['csvid'], d, self.oracle[name], rows or self.rows, oracle_context=self.oracle)

    def test_pin_parameters_and_scope_fail_closed(self):
        d=self.declaration()
        for change in (dict(oracle_text='changed'), dict(params={}), dict(params=dict(d['params'], power_limit=True)),
                       dict(params=dict(d['params'], power_limit=1)), dict(params=dict(d['params'], restriction='flying_only')),
                       dict(params=dict(d['params'], keywords=['Flying'])), dict(params=dict(d['params'], other=True))):
            with self.assertRaises(ValueError): self.cases('Goblin Raider',dict(d,**change))

    def test_conditional_triggered_and_other_passive_rules_are_rejected(self):
        for name in ('Bloodghast','Felhide Brawler','Fretwork Colony','Goblin Goon','Mardu Skullhunter'):
            with self.assertRaises(ValueError): self.cases(name,self.declaration(name))

    def test_numeric_cost_and_single_face_required(self):
        d=self.declaration();record=copy.deepcopy(self.oracle['Goblin Raider'])
        for change in (dict(power='*'),dict(toughness='0'),dict(mana_cost='{B/R}'),dict(layout='transform'),dict(card_faces=[{}])):
            with self.assertRaises(ValueError): explicit_spec(d,dict(record,**change))

    def test_exact_power_boundaries(self):
        for name,mode,expected in (('Ironclaw Orcs','power_at_least',[True,True,False,False]),('Sunweb','power_at_most',[False,False,False,True])):
            d=self.declaration(name,mode,2,['Defender','Flying'] if name=='Sunweb' else [])
            p=explicit_spec(d,self.oracle[name])
            self.assertEqual([may_block(p,n,False) for n in (0,1,2,3)],expected)
            self.assertEqual(may_block(p,3,True),name=='Sunweb')

    def test_reach_does_not_make_ground_attackers_flying(self):
        p=explicit_spec(self.declaration('Gloomwidow','flying_only',keys=['Reach']),self.oracle['Gloomwidow'])
        self.assertFalse(may_block(p,2,False));self.assertTrue(may_block(p,2,True))

    def test_cannot_block_ignores_power_and_flying(self):
        p=explicit_spec(self.declaration(),self.oracle['Goblin Raider'])
        self.assertTrue(all(not may_block(p,power,flying) for power in range(5) for flying in (False,True)))

    def test_both_players_use_real_combat_and_preserve_zones(self):
        d=self.declaration('Gloomwidow','flying_only',keys=['Reach'],selected=['defend_reach_0','defend_flying_two_1'])
        for c,allowed in zip(self.cases('Gloomwidow',d),(0,1)):
            ops=[s['op'] for s in c['steps']]
            self.assertTrue(all(op in ops for op in ('begin_attackers','finish_attackers','begin_blockers','advance_combat')))
            self.assertIn(dict(op='block',ref='source',other='attacker',value=allowed),c['steps'])
            self.assertIn(dict(op='assert_zone',player=1,zone='exile',value=2),c['steps'])
            self.assertFalse(any(s['op']=='event' for s in c['steps']))

    def test_current_power_changes_use_real_spells_before_blocking(self):
        d=self.declaration('Ironclaw Orcs','power_at_least',2,selected=['growth_before_block','shrink_before_block'])
        for c,allowed,power in zip(self.cases('Ironclaw Orcs',d),(0,1),(4,1)):
            ops=[s['op'] for s in c['steps']]
            self.assertLess(ops.index('cast'),ops.index('begin_blockers'))
            self.assertIn(dict(op='assert_card',ref='attacker',property='power',value=power),c['steps'])
            self.assertIn(dict(op='block',ref='source',other='attacker',value=allowed),c['steps'])

    def test_tapped_and_sick_blocking_are_distinct(self):
        d=self.declaration('Cloud Elemental','flying_only',keys=['Flying'],selected=['tapped_blocker','sick_blocker'])
        for c,state,allowed in zip(self.cases('Cloud Elemental',d),('tapped','sick'),(0,1)):
            self.assertEqual(c['setup'][0]['state'],[state])
            self.assertIn(dict(op='block',ref='source',other='attacker',value=allowed),c['steps'])

    def test_combat_snapshot_captures_declared_attackers_and_replays_ai_backup(self):
        c=self.cases('Goblin Raider',self.declaration(selected=['combat_snapshot']))[0]
        ops=[s['op'] for s in c['steps']]
        self.assertLess(ops.index('finish_attackers'),ops.index('capture'))
        self.assertLess(ops.index('capture'),ops.index('begin_blockers'))
        self.assertEqual(ops.count('begin_blockers'),2)
        for op in ('ai_roundtrip','restore','record_state','assert_recorded_state'):self.assertIn(op,ops)

    def test_denied_blocks_deal_combat_damage_to_defender(self):
        for c,owner in zip(self.cases('Goblin Raider',self.declaration(selected=['damage_0','damage_1'])),(0,1)):
            self.assertIn(dict(op='combat_damage'),c['steps'])
            self.assertIn(dict(op='assert_life',player=owner,value=17),c['steps'])

    def test_empty_zones_are_deliberate(self):
        c=self.cases('Goblin Raider',self.declaration(selected=['empty_zones']))[0]
        self.assertEqual(c['fixture'],'empty_main')
        for p in (0,1):
            for z in ('hand','graveyard','exile','library'):self.assertIn(dict(op='assert_zone',player=p,zone=z,value=0),c['steps'])

    def test_unused_abilities_are_visible_and_not_whole_card_certification(self):
        c=self.cases('Carrion Feeder',self.declaration('Carrion Feeder',selected=['defend_ground_one_0']))[0]
        self.assertEqual(len(c['_recipe']['untested_abilities']),1)
        self.assertFalse(any(s['op']=='activate' for s in c['steps']))

    def test_handlers_do_not_supply_expectations(self):
        d=self.declaration(selected=['defend_ground_two_0']);rows=copy.deepcopy(self.rows);rows['Goblin Raider']['handler']='card_dummy'
        self.assertEqual(self.cases('Goblin Raider',d),self.cases('Goblin Raider',d,rows))

    def test_changed_witness_and_unknown_variant_rejected(self):
        oracle=copy.deepcopy(self.oracle);oracle['Giant Spider']['oracle_text']='Flying'
        with self.assertRaises(ValueError):validate_witnesses(oracle,self.rows)
        with self.assertRaises(ValueError):self.cases('Goblin Raider',self.declaration(selected=['defend_unknown_0']))

    def test_none_and_flying_power_parameters_cannot_be_smuggled(self):
        for mode in ('none','flying_only'):
            with self.assertRaises(ValueError):explicit_spec(self.declaration(restriction=mode,limit=2),self.oracle['Goblin Raider'])


if __name__=='__main__':unittest.main()
