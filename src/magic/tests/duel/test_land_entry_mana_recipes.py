import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry, validate_library_observation
from recipes import expand, explicit_spec
from land_entry_mana_recipes import variants


class LandEntryManaRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}

    def declaration(self, name='Blood Crypt', selected=None):
        mode, outputs, production, life = {
            'Blood Crypt': ('shock',['{B}','{R}'],'choice',2),
            'Azorius Chancery': ('bounce',['{W}{U}'],'fixed',0),
            "Dromar's Cavern": ('lair',['{W}','{U}','{B}'],'choice',0),
            'Temple of Abandon': ('scry',['{R}','{G}'],'choice',0),
            'Radiant Fountain': ('life_untapped',['{C}'],'fixed',2),
            'Zhalfirin Void': ('scry_untapped',['{C}'],'fixed',0),
            'Desert of the Fervent': ('tapped',['{R}'],'fixed',0),
        }[name]
        return dict(family='land_entry_mana',version=2,params=dict(entry=mode,outputs=outputs,production=production,life=life),oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['entry_0'])

    def cases(self,name='Blood Crypt',selected=None,d=None,oracle=None):
        return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],self.rows,oracle_context=oracle or self.oracle)

    def test_complete_pin_and_parameters_reject_unreviewed_effects(self):
        d=self.declaration()
        for patch in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],life=True)),dict(params=dict(d['params'],life=3)),dict(params=dict(d['params'],outputs=[[]])),dict(params=dict(d['params'],outputs=['{B}','{B}']))):
            with self.assertRaises(ValueError):self.cases(d=dict(d,**patch))
        r=dict(self.oracle['Blood Crypt'],oracle_text=d['oracle_text']+'\nWhen this land enters, draw a card.')
        with self.assertRaises(ValueError):explicit_spec(dict(d,oracle_text=r['oracle_text']),r)

    def test_real_atlas_hand_selection_without_casting_a_land(self):
        c=self.cases()[0];s=c['steps']
        self.assertIn(dict(op='activate',ref='atlas',value=1),s)
        self.assertIn(dict(op='choose_zone_card',player=0,value=self.rows['Blood Crypt']['csvid']),s)
        self.assertFalse(any(x['op']=='cast' and x['ref']=='source' for x in s))

    def test_shock_pay_decline_and_insufficient_life(self):
        for variant,choice,life,tapped in [('entry_0',1,18,0),('decline_life_0',0,20,1),('insufficient_life_0',0,1,1)]:
            s=self.cases(selected=[variant])[0]['steps']
            for x in (dict(op='choose_dialog',player=0,value=choice),dict(op='assert_life',player=0,value=life),dict(op='assert_card',ref='source',property='tapped',value=tapped)):self.assertIn(x,s)

    def test_bounce_targets_owned_tapped_untapped_and_itself(self):
        for v,target in [('entry_1','own_land'),('return_alternate_0','own_other'),('return_untapped_1','own_land'),('no_other_lands_0','source'),('return_self_1','source')]:
            s=self.cases('Azorius Chancery',[v])[0]['steps'];owner=int(v[-1])
            self.assertIn(dict(op='choose_target',player=owner,ref=target),s)
            self.assertIn(dict(op='assert_card',ref=target,property='in_play',value=0),s)
            self.assertIn(dict(op='assert_card',ref=target,property='in_hand',value=1),s)

    def test_lair_sacrifices_when_no_eligible_land_or_declined(self):
        for v in ('no_other_lands_0','decline_return_1'):
            c=self.cases("Dromar's Cavern",[v])[0]
            self.assertIn(dict(op='choose_cancel_target',player=int(v[-1])),c['steps'])
            self.assertIn(dict(op='require_card',ref='source',property='in_play',value=0),c['steps'])
            self.assertIn(dict(op='assert_zone_multiplicity',player=int(v[-1]),zone='graveyard',index=self.rows["Dromar's Cavern"]['csvid'],value=1),c['steps'])

    def test_human_scry_positive_bottom_and_ai_legal_result(self):
        for owner in (0,1):
            s=self.cases('Temple of Abandon',[f'bottom_{owner}'])[0]['steps']
            self.assertIn(dict(op='record_library',player=owner),s)
            self.assertIn(dict(op='assert_scry_result',player=owner,value=1),s)
            self.assertEqual(any(x['op']=='choose_zone' for x in s),owner==0)
            self.assertEqual(any(x['op']=='choose_optional_zone' for x in s),owner==1)
            self.assertIn(dict(op='assert_forced_zone_choices',value=0),s)
            if owner==0:self.assertIn(dict(op='assert_zone_card',player=0,zone='library',index=0,value=self.rows['Plains']['csvid']),s)

    def test_returned_lands_are_rebound_to_their_new_hand_slots(self):
        s=self.cases('Azorius Chancery',['entry_1'])[0]['steps']
        rebind=dict(op='bind_hand',player=1,value=self.rows['Forest']['csvid'],ref='own_land')
        self.assertIn(rebind,s)
        self.assertGreater(s.index(rebind),next(i for i,x in enumerate(s) if x['op']=='resolve'))

    def test_empty_short_libraries_and_untapped_scry(self):
        for v,size in [('empty_library_0',0),('short_library_1',1)]:
            c=self.cases('Zhalfirin Void',[v])[0]
            self.assertEqual(c['fixture'],'empty_main')
            self.assertIn(dict(op='assert_zone',player=int(v[-1]),zone='library',value=size),c['steps'])
            self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),c['steps'])

    def test_life_gain_is_repeated_for_each_actual_entry(self):
        for v in ('two_sources_0','reenter_1'):
            s=self.cases('Radiant Fountain',[v])[0]['steps']
            self.assertIn(dict(op='assert_life',player=int(v[-1]),value=24),s)
            self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),s)

    def test_mana_is_immediate_and_remains_exempt_from_field(self):
        s=self.cases('Azorius Chancery',['tax_0_0'])[0]['steps']
        self.assertIn(dict(op='assert_card',ref='taxer',property='in_play',value=1),s)
        self.assertIn(dict(op='assert_mana',player=0,color=2,value=20),s) # Twiddle spent one blue first.
        self.assertIn(dict(op='assert_mana',player=0,color=5,value=20),s) # Field spent one white first.
        self.assertIn(dict(op='assert_stack',value=0),s)

    def test_sorcery_speed_tax_witness_is_cast_by_the_active_player(self):
        c=self.cases('Azorius Chancery',['tax_0_1'])[0]
        self.assertIn(dict(name='Suppression Field',ref='taxer',player=1,zone='hand'),c['setup'])
        self.assertIn(dict(op='assert_mana',player=1,color=0,value=19),c['steps'])

    def test_colored_and_generic_receipts_spend_the_generated_mana(self):
        for v in ('spend_colored_0_1','spend_generic_0_0'):
            s=self.cases('Azorius Chancery',[v])[0]['steps'];owner=int(v[-1])
            self.assertTrue(any(x['op']=='cast' and x['ref'].startswith('spender_') for x in s))
            for color in range(6):self.assertIn(dict(op='assert_mana',player=owner,color=color,value=0),s)

    def test_snapshots_replay_decisions_and_effect_receipts(self):
        for v in ('ready_snapshot_0','pending_snapshot_1','mana_snapshot_1_0','resolved_snapshot_1'):
            c=self.cases(selected=[v])[0]
            for op in ('capture','record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertTrue(any(x['op']==op for x in c['steps']))

    def test_cycling_is_a_named_separate_gap_and_legend_rule_is_respected(self):
        d=self.declaration('Desert of the Fervent');p=explicit_spec(d,self.oracle['Desert of the Fervent'])
        self.assertEqual(len(p['untested_rules']),1)
        r=dict(self.oracle['Desert of the Fervent'],type_line='Legendary Land — Desert')
        self.assertNotIn('two_sources_0',variants(explicit_spec(d,r)))

    def test_witness_pin_variants_and_input_immutability(self):
        d=self.declaration();old=copy.deepcopy(d);self.cases(d=d);self.assertEqual(d,old)
        o=copy.deepcopy(self.oracle);o['Walking Atlas']['oracle_text']='{T}: Draw a card.'
        with self.assertRaises(ValueError):self.cases(oracle=o)
        for vs in ([],['unknown_0'],['entry_0','entry_0']):
            with self.assertRaises(ValueError):self.cases(d=dict(d,variants=vs))

    def test_library_observations_require_player_specific_prior_record(self):
        recorded=set()
        with self.assertRaises(ValueError):validate_library_observation(dict(op='assert_scry_result',player=0,value=1),recorded)
        validate_library_observation(dict(op='record_library',player=0),recorded)
        for player,value in [(1,1),(0,-1),(0,21),(0,True),(True,1),(-1,1),(None,1)]:
            with self.assertRaises(ValueError):validate_library_observation(dict(op='assert_scry_result',player=player,value=value),recorded)
        for amount in (0,1,20):validate_library_observation(dict(op='assert_scry_result',player=0,value=amount),recorded)


if __name__=='__main__':unittest.main()
