import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand, explicit_spec
from entry_stats_creature_recipes import variants


class EntryStatsCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}

    def declaration(self, name='Kinsbaile Skirmisher', selected=None):
        effect, power, toughness, amount = ('team_pump', 1, 1, 0) if name == 'Leonin Armorguard' else ('counter', 0, 0, 1) if name == 'Bond Beetle' else ('target_pump', 1, 1, 0)
        return dict(family='entry_stats_creature', version=2, params=dict(effect=effect,power=power,toughness=toughness,amount=amount,keywords=[]),oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['cast_0'])

    def cases(self, name='Kinsbaile Skirmisher', selected=None, d=None, oracle=None):
        return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],self.rows,oracle_context=oracle or self.oracle)

    def test_pin_and_full_text_reject_unreviewed_effects(self):
        d=self.declaration()
        for patch in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],power=2)),dict(params=dict(d['params'],amount=1)),dict(params=dict(d['params'],power=True)),dict(params=dict(d['params'],keywords=['Flying']))):
            with self.assertRaises(ValueError):self.cases(d=dict(d,**patch))
        for extra in ('\nWhen this creature dies, draw a card.','\n{T}: Add {G}.'):
            r=dict(self.oracle['Kinsbaile Skirmisher'],oracle_text=d['oracle_text']+extra)
            with self.assertRaises(ValueError):explicit_spec(dict(d,oracle_text=r['oracle_text']),r)

    def test_restricted_and_optional_targets_are_not_silently_enrolled(self):
        d=self.declaration()
        for clause in ('target creature you control gets','another target creature gets','you may have target creature get'):
            text=d['oracle_text'].replace('target creature gets',clause);r=dict(self.oracle['Kinsbaile Skirmisher'],oracle_text=text)
            with self.assertRaises(ValueError):explicit_spec(dict(d,oracle_text=text),r)

    def test_team_bonus_includes_source_and_owned_witnesses_only(self):
        c=self.cases('Leonin Armorguard')[0]
        for ref,power in [('source',4),('recipient',3),('friendly',4),('enemy',2),('enemy_other',3)]:
            self.assertIn(dict(op='assert_card',ref=ref,property='power',value=power),c['steps'])
        self.assertIn(dict(op='assert_card',ref='artifact',property='in_play',value=1),c['steps'])

    def test_target_bonus_does_not_touch_other_creatures(self):
        c=self.cases()[0]
        self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=3),c['steps'])
        self.assertIn(dict(op='assert_card',ref='friendly',property='power',value=3),c['steps'])
        self.assertIn(dict(op='choose_target',player=0,ref='recipient'),c['steps'])

    def test_counters_survive_cleanup_and_temporary_bonuses_do_not(self):
        pump=self.cases(selected=['cleanup_0'])[0];counter=self.cases('Bond Beetle',['cleanup_0'])[0]
        self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=2),pump['steps'])
        self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=3),counter['steps'])
        self.assertIn(dict(op='assert_card',ref='recipient',property='plus1_counters',value=1),counter['steps'])

    def test_blink_and_recast_retrigger_but_source_removal_preserves_receipt(self):
        for v,power in [('blink_0',4),('bounce_recast_0',4),('bounce_only_0',3),('destroy_source_0',3)]:
            c=self.cases(selected=[v])[0]
            self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=power),c['steps'])

    def test_blinked_team_source_loses_old_bonus_and_gets_new_one(self):
        c=self.cases('Leonin Armorguard',['blink_0'])[0]
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=4),c['steps'])
        self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=4),c['steps'])

    def test_later_arrival_does_not_join_frozen_team_cohort(self):
        c=self.cases('Leonin Armorguard',['late_entry_0'])[0]
        self.assertIn(dict(op='assert_card',ref='late',property='power',value=2),c['steps'])

    def test_effect_is_absent_before_resolution(self):
        s=self.cases()[0]['steps'];cut=next(i for i,x in enumerate(s) if x['op']=='resolve')
        self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=2),s[:cut])

    def test_both_controllers_and_exact_payment(self):
        a,b=self.cases(selected=['payment_0','payment_1'])
        for c,owner in [(a,0),(b,1)]:
            self.assertIn(dict(op='assert_mana_total',player=owner,value=0),c['steps'])
            self.assertIn(dict(op='choose_target',player=owner,ref='recipient'),c['steps'])

    def test_snapshot_replays_assertions_and_ai_roundtrip(self):
        c=self.cases(selected=['pending_snapshot_0'])[0]
        for op in ('capture','record_state','ai_roundtrip','restore','assert_recorded_state'):
            self.assertTrue(any(s['op']==op for s in c['steps']))
        self.assertEqual(sum(s==dict(op='assert_card',ref='recipient',property='power',value=3) for s in c['steps']),2)

    def test_witness_pins_and_variants_fail_closed(self):
        o=copy.deepcopy(self.oracle);o['Grizzly Bears']['power']='3'
        with self.assertRaises(ValueError):self.cases(oracle=o)
        for vs in ([],['unknown_0'],['cast_0','cast_0']):
            d=self.declaration();d['variants']=vs
            with self.assertRaises(ValueError):self.cases(d=d)
        v=variants(explicit_spec(self.declaration(),self.oracle['Kinsbaile Skirmisher']));self.assertEqual(len(v),len(set(v)))


if __name__ == '__main__':unittest.main()
