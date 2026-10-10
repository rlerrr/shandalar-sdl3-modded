"""Fail-closed Oracle scope and independent complete-declaration expectations."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import explicit_spec, expand
from combat_keyword_creature_recipes import variants, validate_witnesses


class CombatKeywordCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']:r for r in registry()}

    def declaration(self, name='Devoted Retainer', mode='bushido', n=1, keys=None, selected=None):
        return dict(family='combat_keyword_creature', version=2, params=dict(keywords=keys or [], mechanic=mode, amount=n),
                    oracle_text=self.oracle[name]['oracle_text'], variants=selected or ['cast'])

    def cases(self, name, d, rows=None):
        return expand(name, self.rows[name]['csvid'], d, self.oracle[name], rows or self.rows, oracle_context=self.oracle)

    def test_pinned_parameters_fail_closed(self):
        d=self.declaration()
        for change in (dict(oracle_text='changed'), dict(params={}), dict(params=dict(d['params'],amount=True)),
                       dict(params=dict(d['params'],amount=2)),dict(params=dict(d['params'],mechanic='exalted')),
                       dict(params=dict(d['params'],keywords=['Flying'])),dict(params=dict(d['params'],extra=True))):
            with self.assertRaises(ValueError):self.cases('Devoted Retainer',dict(d,**change))

    def test_bounded_amounts_and_exalted_flanking_fixed_one(self):
        for name,mode in (('Devoted Retainer','bushido'),('Akrasan Squire','exalted'),('Benalish Cavalry','flanking')):
            for n in (0,6,-1,1.0,True):
                with self.assertRaises(ValueError):self.cases(name,self.declaration(name,mode,n))
        with self.assertRaises(ValueError):self.cases('Benalish Cavalry',self.declaration('Benalish Cavalry','flanking',2))

    def test_numeric_stats_ordinary_mana_and_single_face_required(self):
        d=self.declaration();r=self.oracle['Devoted Retainer']
        for change in (dict(power='*'),dict(toughness='0'),dict(mana_cost='{B/R}'),dict(layout='transform'),dict(card_faces=[{}])):
            with self.assertRaises(ValueError):explicit_spec(d,dict(r,**change))

    def test_other_passive_and_triggered_rules_rejected(self):
        for name,mode,n in (('Cavalry Master','flanking',1),('Battlegrace Angel','exalted',1),('Rafiq of the Many','exalted',1),
                            ('Konda, Lord of Eiganjo','bushido',5),('Sublime Archangel','exalted',1)):
            with self.assertRaises(ValueError):self.cases(name,self.declaration(name,mode,n))

    def test_unknown_reminder_rejected(self):
        r=dict(self.oracle['Devoted Retainer'],oracle_text='Bushido 1 (Changed reminder.)')
        with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),oracle_text=r['oracle_text']),r)

    def test_structured_keyword_must_agree(self):
        with self.assertRaises(ValueError):explicit_spec(self.declaration(),dict(self.oracle['Devoted Retainer'],keywords=[]))

    def test_activated_extras_recorded_without_exercising(self):
        c=self.cases('Cursed Ronin',self.declaration('Cursed Ronin',selected=['blocked']))[0]
        self.assertEqual(len(c['_recipe']['untested_abilities']),1)
        self.assertFalse(any(s['op']=='activate' for s in c['steps']))

    def test_real_phases_no_raw_events(self):
        for c in self.cases('Devoted Retainer',self.declaration(selected=['blocked','defend_ground_1'])):
            ops=[s['op'] for s in c['steps']]
            self.assertTrue(all(op in ops for op in ('begin_attackers','finish_attackers','begin_blockers','advance_combat')))
            self.assertNotIn('event',ops)

    def test_bushido_multi_block_once_and_cleanup(self):
        c=self.cases('Samurai Enforcers',self.declaration('Samurai Enforcers',n=2,selected=['two_blockers']))[0]
        self.assertEqual(sum(s['op']=='block' for s in c['steps']),2)
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=6),c['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='toughness',value=6),c['steps'])
        after=c['steps'][next(i for i,s in enumerate(c['steps']) if s['op']=='cleanup')+1:]
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=4),after)

    def test_bushido_blocking_and_summoning_sickness(self):
        c=self.cases('Araba Mothrider',self.declaration('Araba Mothrider',keys=['Flying'],selected=['sick_defend_0']))[0]
        self.assertEqual(c['setup'][0]['state'],['sick'])
        self.assertIn(dict(op='block',ref='source',other='attacker',value=1),c['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=2),c['steps'])

    def test_flanking_affects_each_blocker_and_not_attacker(self):
        c=self.cases('Benalish Cavalry',self.declaration('Benalish Cavalry','flanking',selected=['two_blockers']))[0]
        for ref in ('blocker','blocker_two'):
            self.assertIn(dict(op='assert_card',ref=ref,property='power',value=3),c['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=2),c['steps'])

    def test_flanking_blocker_immune_and_small_blocker_dies(self):
        cases=self.cases('Benalish Cavalry',self.declaration('Benalish Cavalry','flanking',selected=['flanking_blocker','small_blocker']))
        self.assertIn(dict(op='assert_card',ref='blocker',property='power',value=2),cases[0]['steps'])
        self.assertIn(dict(op='assert_card',ref='blocker',property='in_play',value=0),cases[1]['steps'])
        self.assertIn(dict(op='assert_zone_contains',player=1,zone='graveyard',value=self.rows['Muck Rats']['csvid']),cases[1]['steps'])
        self.assertIn(dict(op='combat_damage'),cases[1]['steps'])
        self.assertIn(dict(op='assert_life',player=1,value=20),cases[1]['steps'])

    def test_exalted_complete_attackers_and_multiple_sources(self):
        d=self.declaration('Akrasan Squire','exalted',selected=['attack_pair_0','two_sources_1'])
        pair,copies=self.cases('Akrasan Squire',d)
        self.assertEqual(sum(s['op']=='attack' for s in pair['steps']),2)
        self.assertNotIn(dict(op='assert_card',ref='source',property='power',value=2),pair['steps'])
        self.assertIn(dict(op='assert_card',ref='attacker',property='power',value=6),copies['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=1),copies['steps'])

    def test_human_explicitly_orders_simultaneous_exalted_triggers(self):
        c=self.cases('Noble Hierarch',self.declaration('Noble Hierarch','exalted',selected=['two_sources_0']))[0]
        ops=[s['op'] for s in c['steps']]
        self.assertLess(ops.index('choose_priority'),ops.index('finish_attackers'))
        self.assertIn(dict(op='choose_priority',ref='source'),c['steps'])

    def test_exalted_tapped_sick_and_opposing_attackers(self):
        d=self.declaration('Akrasan Squire','exalted',selected=['tapped_source_1','sick_source_0','defend_ground_0'])
        for c in self.cases('Akrasan Squire',d)[:2]:
            self.assertIn(dict(op='assert_card',ref='attacker',property='power',value=5),c['steps'])
        c=self.cases('Akrasan Squire',d)[2]
        self.assertNotIn(dict(op='assert_card',ref='attacker',property='power',value=4),c['steps'])

    def test_exalted_before_after_source_removal_and_attacker_bounce(self):
        d=self.declaration('Akrasan Squire','exalted',selected=['source_bounced_before','source_bounced_after','multiple_attackers_then_bounce'])
        before,after,pair=self.cases('Akrasan Squire',d)
        for c,bonus in ((before,4),(after,5),(pair,4)):
            self.assertIn(dict(op='assert_card',ref='attacker',property='power',value=bonus),c['steps'])
            self.assertIn(dict(op='cast',ref='response',value=1),c['steps'])
            self.assertTrue(any(s['op']=='bind_hand' for s in c['steps']))
        self.assertIn(dict(op='choose_target',player=1,ref='second'),pair['steps'])

    def test_snapshots_replay_real_triggers(self):
        for name,mode,trigger in (('Devoted Retainer','bushido','advance_combat'),('Benalish Cavalry','flanking','advance_combat'),('Akrasan Squire','exalted','finish_attackers')):
            c=self.cases(name,self.declaration(name,mode,selected=['combat_snapshot_0']))[0]
            ops=[s['op'] for s in c['steps']]
            self.assertLess(ops.index('capture'),ops.index(trigger))
            self.assertEqual(ops.count(trigger),2)
            for op in ('record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertIn(op,ops)

    def test_first_strike_bushido_damage_expectations(self):
        c=self.cases('Kitsune Blademaster',self.declaration('Kitsune Blademaster',keys=['First strike'],selected=['defend_damage_0']))[0]
        self.assertIn(dict(op='combat_damage'),c['steps'])
        self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),c['steps'])
        self.assertIn(dict(op='assert_card',ref='attacker',property='in_play',value=0),c['steps'])

    def test_empty_zones_and_owned_stocked_order(self):
        empty,stocked=self.cases('Devoted Retainer',self.declaration(selected=['empty_zones','blocked']))
        for p in (0,1):
            for z in ('hand','graveyard','exile','library'):self.assertIn(dict(op='assert_zone',player=p,zone=z,value=0),empty['steps'])
            self.assertIn(dict(op='assert_zone_card',player=p,zone='library',index=0,value=self.rows['Forest']['csvid']),stocked['steps'])

    def test_handlers_are_identity_only(self):
        d=self.declaration(selected=['blocked']);rows=copy.deepcopy(self.rows);rows['Devoted Retainer']['handler']='card_dummy'
        self.assertEqual(self.cases('Devoted Retainer',d),self.cases('Devoted Retainer',d,rows))

    def test_witness_drift_unknown_variant_and_defender_eligibility(self):
        o=copy.deepcopy(self.oracle);o['Benalish Cavalry']['oracle_text']='Flanking twice'
        with self.assertRaises(ValueError):validate_witnesses(o,self.rows)
        with self.assertRaises(ValueError):self.cases('Devoted Retainer',self.declaration(selected=['unknown']))
        p=explicit_spec(self.declaration('Guardians of Akrasa','exalted',keys=['Defender']),self.oracle['Guardians of Akrasa'])
        self.assertNotIn('blocked',variants(p));self.assertNotIn('attack_pair_0',variants(p))

    def test_legendary_duplicates_need_a_dedicated_legend_rule_scenario(self):
        p=explicit_spec(self.declaration('Iizuka the Ruthless',n=2),self.oracle['Iizuka the Ruthless'])
        self.assertTrue(p['legendary'])
        self.assertNotIn('two_sources_0',variants(p));self.assertNotIn('two_sources_1',variants(p))


if __name__=='__main__':unittest.main()
