"""Review boundaries and independent costs/zones for the cycling component."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import explicit_spec,expand
from cycling_recipes import variants,validate_witnesses

class CyclingRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text())['cards']
        cls.rows={r['name']:r for r in registry()}
    def declaration(self,name='Drifting Meadow',cost='{2}',selected=None):
        return dict(family='cycling',version=2,params=dict(cycling_cost=cost),oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['cycle_0'])
    def cases(self,name='Drifting Meadow',cost='{2}',selected=None):
        return expand(name,self.rows[name]['csvid'],self.declaration(name,cost,selected),self.oracle[name],self.rows,oracle_context=self.oracle)
    def test_explicit_cost_and_text_required(self):
        d=self.declaration()
        for change in (dict(params={}),dict(params=dict(cycling_cost='{3}')),dict(oracle_text='changed'),dict(params=dict(cycling_cost='{2}',extra=True))):
            with self.assertRaises(ValueError):explicit_spec(dict(d,**change),self.oracle['Drifting Meadow'])
    def test_bad_costs_rejected(self):
        for cost in (None,'{0}','{X}','{W/U}','{2/W}','{P}','{10}','{8}{W}'):
            with self.assertRaises(ValueError):explicit_spec(self.declaration(cost=cost),self.oracle['Drifting Meadow'])
    def test_structured_cycling_and_single_face_required(self):
        for change in (dict(keywords=[]),dict(layout='transform'),dict(card_faces=[{}])):
            with self.assertRaises(ValueError):explicit_spec(self.declaration(),dict(self.oracle['Drifting Meadow'],**change))
    def test_cycling_triggers_require_dedicated_cases(self):
        for name,cost in (('Choking Tethers','{1}{U}'),('Renewed Faith','{1}{W}'),('Ichor Slick','{2}'),('Undead Gladiator','{1}{B}')):
            with self.assertRaises(ValueError):self.cases(name,cost)
    def test_land_and_spell_components_do_not_require_castable_cost(self):
        for name,cost in (('Drifting Meadow','{2}'),('Starstorm','{3}')):
            p=explicit_spec(self.declaration(name,cost),self.oracle[name]);self.assertEqual(p['activation_mana'][0],int(cost[1]))
    def test_remaining_rules_are_visible_gaps(self):
        c=self.cases('Cloud of Faeries')[0]
        self.assertTrue(c['_recipe']['untested_rules']);self.assertIn('Flying',c['_recipe']['untested_keywords'])
        self.assertFalse(any(s['op']=='cast' for s in c['steps']))
    def test_unknown_variant_rejected(self):
        with self.assertRaises(ValueError):self.cases(selected=['unreviewed'])
    def test_witnesses_pinned(self):
        for name in ('Fluctuator','Suppression Field','Reclaim',"Tormod's Crypt"):
            o=copy.deepcopy(self.oracle);o[name]['oracle_text']='changed'
            with self.assertRaises(ValueError):validate_witnesses(o,self.rows)
    def test_activation_uses_proxy_and_scripted_menu(self):
        s=self.cases()[0]['steps'];ops=[a['op'] for a in s]
        self.assertIn(dict(op='choose_dialog',player=0,value=1),s)
        self.assertLess(ops.index('require_can_activate_hand'),ops.index('choose_dialog'))
        self.assertIn('activate_hand',ops);self.assertNotIn('cast',ops)
    def test_payment_and_discard_precede_draw(self):
        s=self.cases(selected=['payment_0'])[0]['steps'];ops=[a['op'] for a in s]
        resolve=ops.index('resolve')
        self.assertIn(dict(op='assert_mana_total',player=0,value=0),s[:resolve])
        self.assertIn(dict(op='assert_zone',player=0,zone='graveyard',value=5),s[:resolve])
        self.assertIn(dict(op='assert_zone',player=0,zone='library',value=7),s[:resolve])
        self.assertIn(dict(op='assert_zone',player=0,zone='library',value=6),s[resolve:])
    def test_both_players_are_scripted(self):
        s=self.cases(selected=['cycle_1'])[0]['steps']
        self.assertIn(dict(op='choose_dialog',player=1,value=1),s)
        self.assertIn(dict(op='assert_zone',player=1,zone='graveyard',value=5),s)
    def test_opposing_turn_is_legal(self):
        s=self.cases(selected=['enemy_turn_0'])[0]['steps']
        self.assertIn(dict(op='turn',player=1,value=0),s);self.assertIn(dict(op='require_can_activate_hand',ref='source',value=1),s)
    def test_combat_uses_full_attacker_declaration(self):
        ops=[s['op'] for s in self.cases(selected=['combat_0'])[0]['steps']]
        self.assertLess(ops.index('finish_attackers'),ops.index('activate_hand'))
    def test_wrong_color_does_not_activate_or_discard(self):
        s=self.cases('Barren Moor','{B}',['wrong_color_0'])[0]['steps']
        self.assertIn(dict(op='can_activate_hand',ref='source',value=0),s);self.assertFalse(any(a['op']=='activate_hand' for a in s))
    def test_colorless_reduction_does_not_replace_colored_cost(self):
        s=self.cases('Barren Moor','{B}',['reducer_missing_color_0'])[0]['steps']
        self.assertIn(dict(op='can_activate_hand',ref='source',value=0),s)
    def test_suppression_field_taxes_both_controllers(self):
        for c in self.cases(selected=['tax_0','opposing_tax_0']):
            self.assertIn(dict(op='set_mana',player=0,color=0,value=4),c['steps'])
    def test_reducer_only_applies_to_its_controller(self):
        own,other=self.cases(selected=['reducer_0','opposing_reducer_0'])
        self.assertIn(dict(op='set_mana',player=0,color=0,value=0),own['steps']);self.assertIn(dict(op='set_mana',player=0,color=0,value=2),other['steps'])
    def test_reducer_and_tax_are_combined_before_flooring(self):
        c=self.cases('Barren Moor','{B}',['reducer_tax_0'])[0]
        self.assertIn(dict(op='set_mana',player=0,color=0,value=0),c['steps'])
    def test_cancel_does_not_pay_or_draw(self):
        s=self.cases(selected=['cancel_0'])[0]['steps']
        self.assertIn(dict(op='choose_dialog',player=0,value=0),s);self.assertNotIn('resolve',[a['op'] for a in s])
    def test_two_copies_draw_two_ordered_cards(self):
        s=self.cases(selected=['two_copies_0'])[0]['steps']
        self.assertEqual(sum(a['op']=='activate_hand' for a in s),2)
        self.assertIn(dict(op='assert_zone',player=0,zone='library',value=5),s)
    def test_pending_snapshot_captures_paid_discard_and_stack(self):
        ops=[s['op'] for s in self.cases(selected=['pending_snapshot_0'])[0]['steps']]
        self.assertLess(ops.index('activate_hand'),ops.index('capture'));self.assertLess(ops.index('capture'),ops.index('resolve'))
        self.assertIn('ai_roundtrip',ops);self.assertEqual(ops[-1],'assert_recorded_state')
    def test_duplicate_and_last_draw_are_explicit(self):
        c=self.cases(selected=['duplicate_draw_0','one_card_library_0'])
        self.assertEqual([s['name'] for s in c[0]['setup'] if s['zone']=='library' and s['player']==0][:2],['Forest','Forest'])
        self.assertIn(dict(op='assert_zone',player=0,zone='library',value=0),c[1]['steps'])
    def test_reclaimed_or_exiled_source_does_not_cancel_pending_draw(self):
        for c in self.cases(selected=['reclaim_pending_0','crypt_pending_0']):
            self.assertIn(dict(op='assert_stack',value=2),c['steps']);self.assertIn('resolve',[s['op'] for s in c['steps']])

    def test_crypt_uses_human_controller_for_either_graveyard(self):
        for owner in (0,1):
            c=self.cases(selected=['crypt_pending_'+str(owner)])[0]
            self.assertEqual(next(a['player'] for a in c['setup'] if a['ref']=='crypt'),0)
            self.assertIn(dict(op='choose_player',player=0,value=owner),c['steps'])
            self.assertIn(dict(op='assert_zone_contains',player=owner,zone='exile',value=self.rows['Drifting Meadow']['csvid']),c['steps'])

if __name__=='__main__':unittest.main()
