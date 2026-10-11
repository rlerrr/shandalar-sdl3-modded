"""Fail-closed entry scope and Oracle expectations independent of handlers."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import explicit_spec, expand
from entry_counter_creature_recipes import variants, validate_witnesses


class EntryCounterCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,name='Triskelion',selected=None,**changes):
        p=dict(keywords=[],mechanic='fixed',amount=3);p.update(changes)
        return dict(family='entry_counter_creature',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['cast_0'])

    def cases(self,name,d):return expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,oracle_context=self.oracle)

    def blood(self,selected=None):return self.declaration('Bloodrage Vampire',selected,mechanic='bloodthirst',amount=1)

    def test_parameters_and_pin_required(self):
        d=self.declaration()
        for change in (dict(params={}),dict(oracle_text='changed'),dict(params=dict(d['params'],extra=True)),dict(variants=['unknown'])):
            with self.assertRaises(ValueError):self.cases('Triskelion',dict(d,**change))

    def test_bounds_and_types(self):
        for amount in (True,3.0,0,8,-1):
            with self.assertRaises(ValueError):self.cases('Triskelion',self.declaration(amount=amount))

    def test_mechanic_amount_and_keywords_cannot_be_inferred(self):
        for change in (dict(amount=2),dict(mechanic='bloodthirst'),dict(mechanic='graft'),dict(keywords=['Flying'])):
            with self.assertRaises(ValueError):self.cases('Triskelion',self.declaration(**change))

    def test_normal_face_numeric_stats_and_cost(self):
        for change in (dict(power='*'),dict(layout='transform'),dict(card_faces=[{}]),dict(mana_cost='{X}{G}')):
            with self.assertRaises(ValueError):explicit_spec(self.declaration(),dict(self.oracle['Triskelion'],**change))

    def test_zero_base_toughness_is_supported_by_fixed_counters(self):
        d=self.declaration('Fertilid',amount=2);p=explicit_spec(d,self.oracle['Fertilid'])
        self.assertEqual(p['toughness'],0)
        with self.assertRaises(ValueError):explicit_spec(self.blood(),dict(self.oracle['Bloodrage Vampire'],toughness='0'))

    def test_unknown_reminder_or_passive_clause_rejected(self):
        for suffix in ('\nWhenever this creature attacks, draw a card.','\nThis creature has flying.'):
            r=dict(self.oracle['Triskelion'],oracle_text=self.oracle['Triskelion']['oracle_text']+suffix)
            with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),oracle_text=r['oracle_text']),r)

        text=self.oracle['Triskelion']['oracle_text'].replace('on it.','on it. (Different reminder.)',1)
        with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),oracle_text=text),dict(self.oracle['Triskelion'],oracle_text=text))

    def test_other_entry_mechanics_rejected(self):
        for name in ('Arcbound Worker','Simic Initiate','Phantom Nomad','Clockwork Beetle','Avatar of the Resolute','Aether Figment'):
            with self.assertRaises(ValueError):self.cases(name,self.declaration(name,amount=1))

    def test_structured_bloodthirst_keyword_required(self):
        with self.assertRaises(ValueError):explicit_spec(self.blood(),dict(self.oracle['Bloodrage Vampire'],keywords=[]))

    def test_witness_rules_pinned(self):
        for name,field,value in (('Lightning Bolt','oracle_text','Deal 4 damage.'),('Cloudshift','mana_cost','{U}'),('Savannah Lions','power','3'),('Glorious Anthem','oracle_text','Creatures get +1/+1.')):
            o=copy.deepcopy(self.oracle);o[name][field]=value
            with self.assertRaises(ValueError):validate_witnesses(o,self.rows)

    def test_fixed_counters_are_directly_observed(self):
        s=self.cases('Triskelion',self.declaration())[0]['steps']
        for prop,value in (('plus1_counters',3),('power',4),('toughness',4)):
            self.assertIn(dict(op='assert_card',ref='source',property=prop,value=value),s)

    def test_entry_prerequisite_precedes_dependent_targeting(self):
        s=self.cases('Festercreep',self.declaration('Festercreep',selected=['blink_0'],amount=1))[0]['steps']
        gate=next(i for i,a in enumerate(s) if a['op']=='require_card')
        target=next(i for i,a in enumerate(s) if a['op']=='choose_target')
        self.assertLess(gate,target)
        self.assertEqual(s[gate],dict(op='require_card',ref='source',property='in_play',value=1))

    def test_repeated_expensive_casts_have_enough_colorless_mana(self):
        c=self.cases('Triskelion',self.declaration(selected=['two_copies_0']))[0]
        self.assertIn(dict(op='set_mana',player=0,color=0,value=20),c['steps'])
        self.assertIn(dict(op='assert_mana_total',player=0,value=58),c['steps'])

    def test_activated_and_keyword_components_are_visible_gaps(self):
        c=self.cases('Shinewend',self.declaration('Shinewend',amount=1,keywords=['Flying']))[0]
        self.assertEqual(c['_recipe']['untested_keywords'],['Flying']);self.assertEqual(len(c['_recipe']['untested_abilities']),1)
        self.assertFalse(any(s['op']=='activate' for s in c['steps']))

    def test_bloodthirst_requires_damage_to_opponent(self):
        cases=self.cases('Bloodrage Vampire',self.blood(['opponent_damage_0','self_damage_0','creature_damage_0','life_loss_0']))
        for c,n in zip(cases,(1,0,0,0)):
            self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=n),c['steps'])
        self.assertIn(dict(op='choose_player',player=0,value=1),cases[0]['steps'])
        self.assertIn(dict(op='choose_player',player=0,value=0),cases[1]['steps'])

    def test_both_controllers_use_their_own_opponent(self):
        c=self.cases('Bloodrage Vampire',self.blood(['opponent_damage_1']))[0]
        self.assertIn(dict(op='turn',player=1,value=0),c['steps'])
        self.assertIn(dict(op='choose_player',player=0,value=0),c['steps'])

    def test_damage_amount_does_not_multiply_counters(self):
        s=self.cases('Bloodrage Vampire',self.blood(['multiple_damage_0']))[0]['steps']
        self.assertEqual(sum(a['op']=='cast' and a['ref'] in ('damage','second_damage') for a in s),2)
        self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s)
        self.assertIn(dict(op='assert_life',player=1,value=14),s)

    def test_real_combat_sets_bloodthirst_condition(self):
        s=self.cases('Bloodrage Vampire',self.blood(['combat_damage_0']))[0]['steps'];ops=[a['op'] for a in s]
        self.assertTrue(all(op in ops for op in ('begin_attackers','finish_attackers','begin_blockers','advance_combat','combat_damage','end_combat')))
        self.assertLess(ops.index('combat_damage'),next(i for i,a in enumerate(s) if a['op']=='cast' and a['ref']=='source'))

    def test_damage_while_spell_pending_is_not_cast_time_condition(self):
        s=self.cases('Bloodrage Vampire',self.blood(['late_damage_0']))[0]['steps']
        self.assertIn(dict(op='assert_stack',value=2),s)
        self.assertLess(next(i for i,a in enumerate(s) if a['op']=='cast' and a['ref']=='source'),next(i for i,a in enumerate(s) if a['op']=='cast' and a['ref']=='late_damage'))

    def test_damage_flags_snapshot_after_real_damage(self):
        s=self.cases('Bloodrage Vampire',self.blood(['damage_snapshot_0']))[0]['steps'];ops=[a['op'] for a in s]
        self.assertLess(ops.index('resolve'),ops.index('capture'));self.assertIn('ai_roundtrip',ops);self.assertEqual(ops[-1],'assert_recorded_state')

    def test_prior_turn_damage_does_not_qualify(self):
        s=self.cases('Bloodrage Vampire',self.blood(['previous_damage_0']))[0]['steps'];ops=[a['op'] for a in s]
        self.assertEqual(ops.count('advance_turn'),2);self.assertNotIn('cleanup',ops)
        self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),s)

    def test_life_loss_is_a_resolved_spell_not_a_life_total_edit(self):
        for c in self.cases('Bloodrage Vampire',self.blood(['life_loss_0','life_loss_1'])):
            self.assertIn(dict(op='cast',ref='life_loss',value=1),c['steps'])
            self.assertNotIn('set_life',[s['op'] for s in c['steps']])
            self.assertEqual(next(s['name'] for s in c['setup'] if s['ref']=='life_loss'),'Death of a Thousand Stings')

    def test_blink_and_recast_new_turn_reset_conditional_counters(self):
        for c in self.cases('Bloodrage Vampire',self.blood(['blink_new_turn_0','recast_new_turn_1'])):
            s=c['steps'];self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s)
            self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),s)

    def test_blink_pending_snapshot_rebinds_after_resolution(self):
        s=self.cases('Triskelion',self.declaration(selected=['blink_snapshot_0']))[0]['steps'];ops=[a['op'] for a in s]
        cut=ops.index('capture');self.assertEqual(ops[cut+1],'resolve');self.assertEqual(ops.count('bind_permanent'),2)

    def test_anthem_and_pump_do_not_create_counters(self):
        for c in self.cases('Triskelion',self.declaration(selected=['anthem_0','pump_0'])):
            s=c['steps'];self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=3),s)
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=5),self.cases('Triskelion',self.declaration(selected=['anthem_0']))[0]['steps'])

    def test_legendary_owned_duplicates_excluded(self):
        p=explicit_spec(self.declaration('Ghave, Guru of Spores',amount=5),self.oracle['Ghave, Guru of Spores'])
        self.assertNotIn('two_copies_0',variants(p))

    def test_full_and_empty_zones_deliberate(self):
        for c,total in zip(self.cases('Triskelion',self.declaration(selected=['cast_0','empty_zones_0'])),(7,0)):
            for player in (0,1):self.assertIn(dict(op='assert_zone',player=player,zone='library',value=total),c['steps'])

    def test_exact_payment_and_negative_casts(self):
        for c in self.cases('Triskelion',self.declaration(selected=['payment','insufficient_mana'])):
            self.assertIn(dict(op='assert_mana_total',player=0,value=0),c['steps'])

    def test_countered_cast_creates_no_permanent(self):
        s=self.cases('Triskelion',self.declaration(selected=['countered']))[0]['steps']
        self.assertIn(dict(op='cast',ref='source',value=0),s)
        self.assertIn(dict(op='assert_zone',player=0,zone='battlefield',value=0),s)


if __name__=='__main__':unittest.main()
