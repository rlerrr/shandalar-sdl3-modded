import json
from pathlib import Path
import unittest

from generate_cases import registry
from morph_creature_recipes import FACE_DOWN
from morph_faceup_activated_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants


class MorphFaceupActivatedTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry()}

    def make(self, name, variant, seat):
        record = self.oracle[name]
        variant = f'{variant}_{seat}'
        declaration = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'], params=classify(record), variants=[variant])
        selected, = expand(name, self.rows[name]['csvid'], declaration, spec(declaration, record), [variant], self.rows, self.oracle)
        return selected

    def actions(self, c, op, **fields):
        return [line for line in c['steps'] if line['op'] == op and all(line.get(field) == value for field,value in fields.items())]

    def values(self, c, op, **fields):
        return [line['value'] for line in self.actions(c,op,**fields)]

    def test_pinned_whole_oracle_and_bilateral_population(self):
        validate_witnesses(self.oracle,self.rows)
        sources = profiles()['sources']
        self.assertEqual({entry['registry']['csvid'] for entry in sources.values()}, {6071,14308,14957,6320,6161,6272})
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name in sources),210)
        for name in sources:
            record = self.oracle[name]
            with self.assertRaises(ValueError):
                classify(dict(record,oracle_text=record['oracle_text']+'\nHaste'))
            with self.assertRaises(ValueError):
                spec(dict(family=FAMILY,version=2.0,params=classify(record),oracle_text=record['oracle_text'],variants=['effect_0']),record)

    def test_morph_receipts_show_identity_and_megamorph_counter(self):
        for seat in (0,1):
            for name in profiles()['sources']:
                c = self.make(name,'effect',seat)
                self.assertEqual(self.values(c,'assert_card',ref='source',property='csvid')[0], FACE_DOWN)
                self.assertEqual(self.values(c,'assert_card',ref='source',property='csvid')[-1], self.rows[name]['csvid'])
                self.assertEqual(self.values(c,'assert_counter',ref='source',counter_kind='plus1'), [int(name=='Marang River Skeleton')])
                self.assertEqual(self.values(c,'cast',ref='source'),[1])
                self.assertTrue(all(s['op']=='set_mana' and s['value']==0 for s in c['steps'][1:13]))

    def test_hidden_faces_do_not_use_printed_ability(self):
        for seat in (0,1):
            for name in ('Whipcorder','Goblin Taskmaster'):
                c = self.make(name,'hidden_ability',seat)
                self.assertEqual(self.values(c,'activate',ref='source'),[1])
                self.assertFalse(self.actions(c,'choose_target'))
                self.assertEqual(self.values(c,'assert_card',ref='source',property='csvid')[-1], self.rows[name]['csvid'])
            c = self.make('Snarling Undorak','hidden_ability',seat)
            self.assertEqual(self.values(c,'activate',ref='source'),[0])
            self.assertEqual(self.values(c,'assert_card',ref='source',property='csvid')[-1], FACE_DOWN)

    def test_actual_paid_regeneration_responds_with_target_choice_and_damage_reset(self):
        for seat in (0,1):
            c = self.make('Boneknitter','foe_zombie',seat)
            priority, = self.actions(c,'choose_regeneration',ref='source')
            target, = self.actions(c,'choose_target',ref='recipient',player=seat)
            self.assertEqual(c['steps'].index(target),c['steps'].index(priority)+1)
            self.assertEqual(self.values(c,'assert_card',ref='recipient',property='tapped')[-1],1)
            self.assertEqual(self.values(c,'assert_card',ref='recipient',property='damage')[-1],0)
            self.assertEqual(self.values(c,'assert_zone_multiplicity',player=1-seat,zone='battlefield',index=self.rows['Scathe Zombies']['csvid'])[-1],1)
            lethal = self.make('Kin-Tree Warden','regen_lethal',seat)
            self.assertTrue(any(row['name']=='Lightning Bolt' for row in lethal['setup']))
            self.assertEqual(self.values(lethal,'assert_card',ref='source',property='damage')[-1],0)
            self.assertEqual(self.values(lethal,'assert_card',ref='source',property='tapped')[-1],1)

    def test_proactive_and_nonregenerable_removal_are_distinct(self):
        for seat in (0,1):
            name = 'Marang River Skeleton'
            c = self.make(name,'proactive',seat)
            self.assertEqual(self.values(c,'activate',ref='source'),[1,1])
            self.assertFalse(self.actions(c,'choose_regeneration'))
            self.assertTrue(self.actions(c,'destroy',ref='source'))
            self.assertEqual(self.values(c,'assert_card',ref='source',property='tapped')[-1],1)
            blocked = self.make(name,'cannot_regenerate',seat)
            self.assertIn(1,self.values(blocked,'assert_unused_choices'))
            self.assertEqual(self.values(blocked,'assert_zone_multiplicity',player=seat,zone='graveyard',index=self.rows[name]['csvid'])[-1],1)

    def test_whipcorder_uses_actual_age_tap_cost_and_later_untap(self):
        for seat in (0,1):
            fresh = self.make('Whipcorder','sick',seat)
            self.assertEqual(self.values(fresh,'activate',ref='source'),[1,0])
            reuse = self.make('Whipcorder','untap_reuse',seat)
            self.assertEqual(self.values(reuse,'activate',ref='source'),[1,1,0,1])
            self.assertEqual(len(self.actions(reuse,'untap_step')),4)
            self.assertEqual(self.values(reuse,'assert_card',ref='recipient',property='tapped')[-1],1)
            self.assertEqual(self.values(reuse,'assert_card',ref='source',property='tapped')[-1],1)

    def test_typed_pump_scope_and_end_turn_expiry(self):
        for seat in (0,1):
            for name in ('Goblin Taskmaster','Snarling Undorak'):
                p = self.make(name,'typed_target',seat)
                self.assertEqual(self.values(p,'probe_activation_target',other='wrong_type'),[0])
                self.assertEqual(self.values(p,'probe_activation_target',other='recipient'),[1])
                h = self.make(name,'target_protection',seat)
                self.assertIsNotNone(next(row for row in h['setup'] if row['name']=='Asceticism'))
                self.assertEqual(self.values(h,'probe_activation_target',other='protected'),[0])
                e = self.make(name,'expiry',seat)
                self.assertEqual(len(self.actions(e,'advance_turn')),1)
                observed = self.values(e,'assert_card',ref='recipient',property='power')
                self.assertEqual(observed[-2],observed[-1]+1)
            q = self.make('Snarling Undorak','combat_power',seat)
            self.assertTrue(self.actions(q,'block',ref='blocker',other='recipient'))
            self.assertEqual(self.values(q,'assert_card',ref='recipient',property='damage')[-1],4)
            self.assertEqual(self.values(q,'assert_card',ref='recipient',property='toughness')[-1],5)

    def test_replay_repeats_all_paid_turn_up_and_ability_steps(self):
        for name in profiles()['sources']:
            for seat in (0,1):
                c = self.make(name,'replay',seat)
                steps = c['steps']
                a = steps.index(dict(op='capture'))
                b = steps.index(dict(op='record_state'))
                self.assertEqual(steps[a+1:b],steps[b+3:-1])
                self.assertEqual(steps[-1]['op'],'assert_recorded_state')

    def test_undorak_counter_response_requires_paid_source_on_stack(self):
        for seat in (0, 1):
            c = self.make('Snarling Undorak', 'countered', seat)
            precondition, = self.actions(c, 'require_card', ref='source', property='in_hand', value=0)
            announce, = self.actions(c, 'announce_spell', ref='source')
            self.assertEqual(c['steps'].index(precondition) + 1, c['steps'].index(announce))
            self.assertEqual(self.values(c, 'cast', ref='source'), [1])
            self.assertEqual(self.values(c, 'cast_response', ref='counter'), [1])
