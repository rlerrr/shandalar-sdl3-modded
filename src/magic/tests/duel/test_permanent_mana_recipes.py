"""Scope, independent payments and real setup for mana lands/artifacts."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import explicit_spec,expand
from permanent_mana_recipes import variants,validate_witnesses

class PermanentManaRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text())['cards']
        cls.rows={r['name']:r for r in registry()}
    def declaration(self,name='Azorius Signet',selected=None,**change):
        p=dict(activation_cost='{1}, {T}',mana_outputs=['{W}{U}'],production='fixed',enters_tapped=False,entry_life=0,indestructible=False)
        p.update(change)
        return dict(family='permanent_mana',version=2,oracle_text=self.oracle[name]['oracle_text'],params=p,variants=selected or ['production_0_0'])
    def cases(self,name='Azorius Signet',selected=None,**change):
        return expand(name,self.rows[name]['csvid'],self.declaration(name,selected,**change),self.oracle[name],self.rows,oracle_context=self.oracle)
    def test_explicit_parameters_required(self):
        d=self.declaration()
        for p in ({},dict(d['params'],extra=True),dict(d['params'],activation_cost='{2}, {T}')):
            with self.assertRaises(ValueError):explicit_spec(dict(d,params=p),self.oracle['Azorius Signet'])
    def test_complete_text_pin_required(self):
        with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),oracle_text='changed'),self.oracle['Azorius Signet'])
    def test_creatures_and_alternate_faces_rejected(self):
        for change in (dict(type_line='Artifact Creature'),dict(layout='transform'),dict(card_faces=[{}])):
            with self.assertRaises(ValueError):explicit_spec(self.declaration(),dict(self.oracle['Azorius Signet'],**change))
    def test_typed_entry_parameters(self):
        for change in (dict(entry_life=True),dict(enters_tapped=1),dict(indestructible=0),dict(entry_life=2)):
            with self.assertRaises(ValueError):self.cases(**change)
    def test_signets_have_input_and_two_independent_outputs(self):
        p=explicit_spec(self.declaration(),self.oracle['Azorius Signet'])
        self.assertEqual(p['activation_mana'],[1,0,0,0,0,0]);self.assertEqual(p['output_pools'],[[0,0,1,0,0,1]])
    def test_filtering_cost_boundaries(self):
        for cost in ('{0}, {T}','{X}, {T}','{W/U}, {T}','{W}, {T}','{4}, {T}','{T}, Sacrifice this artifact'):
            with self.assertRaises(ValueError):self.cases(activation_cost=cost)
    def test_output_bounds_and_uniqueness(self):
        for outputs in ([],['{X}'],['{C}']*2,['{C}']*4,['{C}{C}{C}{C}']):
            with self.assertRaises(ValueError):self.cases(mana_outputs=outputs)
    def test_additional_activation_or_trigger_rejected(self):
        for name in ('Mind Stone','City of Brass','Azorius Chancery','Cavern of Souls','Adarkar Wastes'):
            with self.assertRaises(ValueError):self.cases(name,activation_cost='{T}',mana_outputs=['{C}'])
    def test_three_choices_pin_punctuation_and_order(self):
        self.cases('Arcane Sanctum',activation_cost='{T}',mana_outputs=['{W}','{U}','{B}'],production='choice',enters_tapped=True)
        with self.assertRaises(ValueError):self.cases('Arcane Sanctum',activation_cost='{T}',mana_outputs=['{B}','{U}','{W}'],production='choice',enters_tapped=True)
    def test_any_color_choices_explicit(self):
        self.cases('Manalith',activation_cost='{T}',mana_outputs=['{B}','{U}','{G}','{R}','{W}'],production='any_color')
    def test_three_of_any_one_color_explicit(self):
        self.cases('Gilded Lotus',activation_cost='{T}',mana_outputs=[s*3 for s in ['{B}','{U}','{G}','{R}','{W}']],production='any_one_color')
    def test_zero_cost_artifacts_can_be_cast(self):
        c=self.cases('Mox Pearl',['fresh_cast_0'],activation_cost='{T}',mana_outputs=['{W}'])[0]
        self.assertIn(dict(op='cast',ref='source',value=1),c['steps'])
    def test_basic_land_reminder_is_not_a_trigger(self):
        self.cases('Forest',activation_cost='{T}',mana_outputs=['{G}'])
    def test_cycling_is_an_explicit_remaining_gap(self):
        c=self.cases('Barren Moor',activation_cost='{T}',mana_outputs=['{B}'],enters_tapped=True)[0]
        self.assertTrue(c['_recipe']['untested_rules']);self.assertNotIn('activate_hand',[s['op'] for s in c['steps']])
    def test_witness_text_pins(self):
        for name in ('Twiddle','Boomerang','Negate','Suppression Field','Phyrexian Hulk'):
            o=copy.deepcopy(self.oracle);o[name]['oracle_text']='changed'
            with self.assertRaises(ValueError):validate_witnesses(o,self.rows)
    def test_missing_eligibility_stops_before_choices(self):
        c=self.cases('Manalith',activation_cost='{T}',mana_outputs=['{B}','{U}','{G}','{R}','{W}'],production='any_color')[0]
        ops=[s['op'] for s in c['steps']]
        self.assertLess(ops.index('require_can_activate'),ops.index('choose_color'))
    def test_exact_signets_payment_observed(self):
        s=self.cases(selected=['payment_0_0'])[0]['steps']
        self.assertIn(dict(op='set_mana',player=0,color=0,value=1),s)
        self.assertIn(dict(op='assert_mana',player=0,color=0,value=0),s)
        self.assertIn(dict(op='assert_mana',player=0,color=2,value=1),s)
        self.assertIn(dict(op='assert_mana',player=0,color=5,value=1),s)
    def test_tapped_entry_observed_before_untap(self):
        c=self.cases('Worn Powerstone',['entry_0'],activation_cost='{T}',mana_outputs=['{C}{C}'],enters_tapped=True)[0]
        self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),c['steps']);self.assertFalse(any(s['op']=='cast' for s in c['steps']))
    def test_tapped_sources_legally_untapped(self):
        c=self.cases('Worn Powerstone',activation_cost='{T}',mana_outputs=['{C}{C}'],enters_tapped=True)[0]
        self.assertIn(dict(op='cast',ref='initial_untap',value=1),c['steps']);self.assertIn(dict(op='choose_dialog',player=0,value=1),c['steps'])
    def test_life_entry_asserted(self):
        c=self.cases('Akoum Refuge',['entry_1'],activation_cost='{T}',mana_outputs=['{B}','{R}'],production='choice',enters_tapped=True,entry_life=1)[0]
        self.assertIn(dict(op='assert_life',player=1,value=21),c['steps'])
    def test_mana_tax_constructed_through_real_cast(self):
        c=self.cases(selected=['tax_0'])[0]
        self.assertEqual(next(s['zone'] for s in c['setup'] if s['ref']=='field'),'hand')
        self.assertIn(dict(op='cast',ref='field',value=1),c['steps'])
    def test_mana_abilities_do_not_use_stack(self):
        s=self.cases()[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='activate')
        self.assertEqual(s[i+1],dict(op='assert_stack',value=0))
    def test_two_sources_separate_taps(self):
        c=self.cases(selected=['two_sources_0'])[0]
        self.assertEqual([s['ref'] for s in c['steps'] if s['op']=='activate'],['source','second'])
    def test_colored_spending_consumes_all_produced_colors(self):
        c=self.cases(selected=['spend_colored_0_0'])[0]
        self.assertEqual([s['name'] for s in c['setup'] if s['ref'].startswith('spend_')],['Merfolk of the Pearl Trident','Savannah Lions'])
        for color in range(6):self.assertIn(dict(op='assert_mana',player=0,color=color,value=0),c['steps'])
    def test_generic_spending_supplies_only_shortfall(self):
        c=self.cases(selected=['spend_generic_0_0'])[0]
        self.assertIn(dict(op='set_mana',player=0,color=0,value=5),c['steps'])
    def test_snapshots_repeat_activation_after_ai_backups(self):
        c=self.cases(selected=['ready_snapshot_0_0'])[0];ops=[s['op'] for s in c['steps']]
        self.assertLess(ops.index('capture'),ops.index('activate'));self.assertEqual(ops.count('activate'),2);self.assertEqual(ops[-1],'assert_recorded_state')
    def test_pending_cast_snapshot_precedes_resolution(self):
        ops=[s['op'] for s in self.cases(selected=['pending_cast_snapshot_1'])[0]['steps']]
        self.assertLess(ops.index('cast'),ops.index('capture'));self.assertLess(ops.index('capture'),ops.index('resolve'))
    def test_land_play_not_emulated_by_spell_casting(self):
        p=explicit_spec(self.declaration('Forest',activation_cost='{T}',mana_outputs=['{G}']),self.oracle['Forest'])
        self.assertNotIn('fresh_cast_0',variants(p))
    def test_empty_zones_deliberate(self):
        c=self.cases(selected=['bare_zones_0'])[0]
        self.assertEqual(c['fixture'],'empty_main')
        self.assertIn(dict(op='assert_zone',player=1,zone='library',value=0),c['steps'])
    def test_indestructibility_preserves_destroyed_source(self):
        c=self.cases('Darksteel Citadel',['destroy_1'],activation_cost='{T}',mana_outputs=['{C}'],indestructible=True)[0]
        self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),c['steps'])
    def test_countering_uses_real_priority_window(self):
        s=self.cases(selected=['countered_0'])[0]['steps']
        self.assertIn(dict(op='choose_priority',ref='counter'),s)
        self.assertIn(dict(op='cast',ref='source',value=0),s)
        self.assertFalse(any(a['op']=='cast' and a['ref']=='counter' for a in s))
    def test_fresh_cast_refreshes_before_mana_activation(self):
        ops=[s['op'] for s in self.cases(selected=['fresh_cast_0'])[0]['steps']]
        self.assertLess(ops.index('resolve'),ops.index('refresh'))
        self.assertLess(ops.index('refresh'),ops.index('activate'))
    def test_generic_payment_scripts_colored_pool_clicks(self):
        s=self.cases(selected=['spend_generic_0_0'])[0]['steps']
        self.assertIn(dict(op='choose_mana',player=0,color=2),s)
        self.assertIn(dict(op='choose_mana',player=0,color=5),s)
    def test_ai_generic_payment_does_not_queue_manual_pool_clicks(self):
        s=self.cases(selected=['spend_generic_0_1'])[0]['steps']
        self.assertFalse(any(a['op']=='choose_mana' for a in s))
        self.assertIn(dict(op='assert_mana',player=1,color=2,value=0),s)
        self.assertIn(dict(op='assert_mana',player=1,color=5,value=0),s)
    def test_empty_input_pool_refreshes_before_fatal_eligibility_check(self):
        s=self.cases(selected=['insufficient_input_0'])[0]['steps']
        ops=[a['op'] for a in s]
        self.assertLess(max(i for i,a in enumerate(s) if a['op']=='set_mana'),ops.index('refresh'))
        self.assertLess(ops.index('refresh'),ops.index('require_can_activate'))
        self.assertLess(ops.index('require_can_activate'),ops.index('activate'))
        self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
    def test_unknown_variant_rejected(self):
        with self.assertRaises(ValueError):self.cases(selected=['made_up'])

if __name__=='__main__':unittest.main()
