"""Oracle, ordered-prefix, combat timing and real observer controls."""
import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry, OPS
import combat_library_prefix_recipes as model


class CombatLibraryPrefixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,name):
        return dict(family=model.FAMILY,version=2,params=model.parameters(self.oracle[name]),
                    oracle_text=self.oracle[name]['oracle_text'],variants=['unblocked_0'])

    def case(self,name,variant):
        d=self.declaration(name);d['variants']=[variant];p=model.spec(d,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],d,p,[variant],self.rows,self.oracle)[0]

    def has(self,c,op,**kw):self.assertIn(dict(op=op,**kw),c['steps'])

    def test_full_oracle_derives_all_eight_mechanisms_and_other_clauses(self):
        expected={'Augury Adept':('top_life',1),'Dragonlord Ojutai':('one_bottom',3),
                  'Lone Revenant':('one_bottom',4),'Borborygmos Enraged':('land_partition',3),
                  'Mirko Vosk, Mind Drinker':('until_lands',4),'Scalpelexis':('repeat_exile',4),
                  'Windrider Patrol':('scry',2),'Grim Flayer':('surveil',3)}
        for name,want in expected.items():
            p=model.parameters(self.oracle[name]);self.assertEqual((p['kind'],p['window']),want)
        self.assertEqual(model.parameters(self.oracle['Dragonlord Ojutai'])['shield'],'untapped')
        self.assertEqual(model.parameters(self.oracle['Lone Revenant'])['condition'],'alone')
        self.assertTrue(model.parameters(self.oracle['Borborygmos Enraged'])['land_damage'])
        self.assertTrue(model.parameters(self.oracle['Grim Flayer'])['delirium'])

    def test_strict_types_versions_complete_oracle_and_distinct_variants(self):
        name='Grim Flayer';d=self.declaration(name)
        for extra in (dict(version=2.0),dict(version=True),dict(family='combat_value_trigger'),dict(variants=[]),dict(variants=['unblocked_0','unblocked_0']),dict(variants=['unblocked_2']),dict(oracle_text=d['oracle_text']+' ')):
            with self.assertRaises(ValueError):model.spec(dict(d,**extra),self.oracle[name])
        bad=copy.deepcopy(d);bad['params']['delirium']=1
        with self.assertRaises(ValueError):model.spec(bad,self.oracle[name])
        r=copy.deepcopy(self.oracle['Augury Adept']);r['oracle_text']+='\nHaste'
        with self.assertRaises(ValueError):model.parameters(r)

    def test_full_effect_witness_and_primary_implementation_pins(self):
        model.validate_witnesses(self.oracle,self.rows)
        for n,k,v in [('Psychic Corrosion','oracle_text','Whenever you draw a card, each opponent mills a card.'),
                      ('Disinformation Campaign','mana_cost','{2}{U}'),('Flamespeaker Adept','power','3'),('Dryad Arbor','type_line','Creature — Dryad')]:
            o=copy.deepcopy(self.oracle);o[n][k]=v
            with self.assertRaises(ValueError):model.validate_witnesses(o,self.rows)
        rows=copy.deepcopy(self.rows);rows['Grim Flayer']['handler']='card_dummy'
        with self.assertRaises(ValueError):model.validate_witnesses(self.oracle,rows)

    def test_all_variants_expand_only_existing_real_operations(self):
        count=0
        for name in model.profiles()['sources']:
            d=self.declaration(name);p=model.spec(d,self.oracle[name]);vs=model.variants(p)
            cs=model.expand(name,self.rows[name]['csvid'],d,p,vs,self.rows,self.oracle)
            self.assertEqual(len(set(c['name'] for c in cs)),len(vs));count+=len(cs)
            for c in cs:
                self.assertLessEqual({s['op'] for s in c['steps']},OPS)
                self.assertFalse(any(s['op'] in ('set_life','destroy','remove_from_combat','reset_death_history','choose_priority') for s in c['steps']))
                self.assertFalse(any('attacking' in x.get('state',[]) for x in c['setup']))
                self.assertEqual(c['_recipe']['oracle_text'],self.oracle[name]['oracle_text'])
        self.assertEqual(count,404)

    def test_augury_mana_value_zero_two_and_x_are_not_fixed_life(self):
        for mode,life,card in [('mana_value_zero',20,514),('mana_value_two',22,1363),('mana_value_x',21,84)]:
            c=self.case('Augury Adept',mode+'_0');self.has(c,'assert_life',player=0,value=life)
            self.has(c,'assert_zone_multiplicity',player=0,zone='hand',index=card,value=1)
        c=self.case('Augury Adept','empty_library_1');self.has(c,'assert_life',player=1,value=20)

    def test_hybrid_actual_cost_routes_and_deterministic_ai_funding(self):
        for mode,blue,white in [('payment',0,2),('hybrid_other',2,0),('hybrid_mixed',1,1)]:
            c=self.case('Augury Adept',mode+'_0');self.has(c,'set_mana',player=0,color=2,value=blue);self.has(c,'set_mana',player=0,color=5,value=white)
            self.has(c,'assert_mana_total',player=0,value=0)
            self.assertFalse(any(s['op']=='choose_payment' for s in c['steps']))
        c=self.case('Augury Adept','cast_0');self.assertEqual(sum(s['op']=='choose_payment' for s in c['steps']),1)
        c=self.case('Augury Adept','cast_1');self.has(c,'set_mana',player=1,color=2,value=0)
        self.assertFalse(any(s['op']=='choose_payment' for s in c['steps']))

    def test_put_to_hand_has_real_paid_nondraw_observer_and_positive_control(self):
        for name in ('Augury Adept','Dragonlord Ojutai','Lone Revenant'):
            c=self.case(name,'nondraw_0');self.has(c,'cast',ref='draw_observer',value=1)
            self.has(c,'assert_mana_total',player=0,value=57)
            self.has(c,'assert_zone',player=1,zone='library',value=7)
            self.assertFalse(any(x['name']=='Psychic Corrosion' and x['zone']=='battlefield' for x in c['setup']))
            c=self.case(name,'draw_positive_0');self.has(c,'cast',ref='positive_draw',value=1)
            self.has(c,'assert_zone',player=1,zone='library',value=5)
            self.has(c,'assert_zone',player=1,zone='graveyard',value=3)
            self.has(c,'assert_mana_total',player=0,value=56)

    def test_one_card_choice_bottom_order_and_full_suffix(self):
        c=self.case('Dragonlord Ojutai','reverse_bottom_0')
        choices=[s['value'] for s in c['steps'] if s['op']=='choose_zone_card']
        self.assertEqual(choices,[91,145]) # Forest hand, Bolt bottom before Visionary.
        self.has(c,'assert_zone_card',player=0,zone='library',index=0,value=164)
        self.has(c,'assert_zone_card',player=0,zone='library',index=5,value=145)
        self.has(c,'assert_zone_card',player=0,zone='library',index=6,value=1363)

    def test_lone_default_has_no_invisible_friendly_idle_creature(self):
        c=self.case('Lone Revenant','unblocked_0')
        self.assertEqual([s['name'] for s in c['setup'] if s['zone']=='battlefield' and s['player']==0],['Lone Revenant'])
        self.has(c,'choose_zone_card',player=0,value=91)
        c=self.case('Lone Revenant','other_own_creature_1')
        self.assertFalse(any(s['op']=='choose_zone_card' for s in c['steps']))
        for mode in ('opponent_creature','own_noncreature','remove_other'):
            c=self.case('Lone Revenant',mode+'_0');self.has(c,'choose_zone_card',player=0,value=91)
        c=self.case('Lone Revenant','remove_other_0');self.has(c,'cast',ref='remove',value=1)
        self.has(c,'assert_zone_multiplicity',player=0,zone='hand',index=106,value=1)

    def test_real_other_removal_is_proved_before_combat_and_unused_trigger_inputs_observed(self):
        for owner in (0,1):
            c=self.case('Lone Revenant',f'remove_other_{owner}');s=c['steps']
            begin=next(i for i,x in enumerate(s) if x['op']=='begin_attackers')
            self.assertIn(dict(op='assert_card',ref='ally',property='in_play',value=0),s[:begin])
            self.assertIn(dict(op='assert_zone',player=owner,zone='battlefield',value=1),s[:begin])
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=106,value=1),s[:begin])
            damage=next(i for i,x in enumerate(s) if x['op']=='combat_damage');tail=s[damage:]
            observed=next(i for i,x in enumerate(tail) if x['op']=='assert_unused_choices')
            strict=next(i for i,x in enumerate(tail) if x['op']=='assert_choices_consumed')
            self.assertLess(observed,strict);self.assertEqual(tail[observed],dict(op='assert_unused_choices',value=0))
            self.assertEqual([x['value'] for x in s if x['op']=='choose_zone_card'],[91,1363,145])
            self.has(c,'assert_zone_multiplicity',player=owner,zone='hand',index=91,value=2)
            self.has(c,'assert_life',player=1-owner,value=16)
            self.assertFalse(any(x['op'] in ('choose_priority','choose_trigger_source') for x in s))

    def test_conditional_and_always_hexproof_use_real_permission_probes(self):
        c=self.case('Dragonlord Ojutai','hexproof_tapped_0')
        self.has(c,'can_target_creature',ref='enemy_probe',other='source',value=0)
        self.has(c,'can_target_creature',ref='enemy_probe',other='source',value=1)
        c=self.case('Lone Revenant','hexproof_tapped_1')
        self.assertFalse(any(s['op']=='can_target_creature' and s['ref']=='enemy_probe' and s['value']==1 for s in c['steps']))
        c=self.case('Dragonlord Ojutai','hexproof_real_untap_0')
        self.assertEqual(sum(s['op']=='untap_step' for s in c['steps']),2)
        probes=[s for s in c['steps'] if s['op']=='can_target_creature' and s['ref']=='enemy_probe'];self.assertEqual([s['value'] for s in probes],[0,1,0])

    def test_borborygmos_fixed_three_partition_includes_land_creature(self):
        c=self.case('Borborygmos Enraged','land_creature_0')
        self.has(c,'assert_zone_multiplicity',player=0,zone='hand',index=8916,value=1)
        self.has(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=1363,value=1)
        self.has(c,'assert_zone',player=0,zone='library',value=5)
        c=self.case('Borborygmos Enraged','pump_0');self.has(c,'assert_life',player=1,value=10)
        self.has(c,'assert_zone',player=0,zone='library',value=5) # Ten damage still inspects only3.

    def test_borborygmos_actual_discard_land_cost_and_legal_creature_target(self):
        c=self.case('Borborygmos Enraged','activation_creature_0')
        self.has(c,'choose_zone_card',player=0,value=91);self.has(c,'choose_target',player=0,ref='activation_target')
        self.assertFalse(any(s['op']=='choose_target' and s['ref']=='wall' for s in c['steps']))
        self.has(c,'activate',ref='source',value=1)
        self.has(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=91,value=1)
        self.has(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=106,value=1)
        self.has(c,'assert_zone',player=0,zone='library',value=8)
        for mode in ('activation_nonland','activation_empty'):
            c=self.case('Borborygmos Enraged',mode+'_1');self.has(c,'can_activate',ref='source',value=0)
            self.assertFalse(any(s['op']=='activate' for s in c['steps']))

    def test_real_partitioned_land_can_pay_later_untapped_cost_while_source_tapped(self):
        c=self.case('Borborygmos Enraged','partition_activation_1');s=c['steps']
        self.assertFalse(any(x['zone']=='hand' and x['player']==1 and 'Land' in self.oracle[x['name']]['type_line'] for x in c['setup']))
        damage=next(i for i,x in enumerate(s) if x['op']=='combat_damage');activation=next(i for i,x in enumerate(s) if x['op']=='activate')
        self.assertLess(damage,activation)
        self.has(c,'require_can_activate',ref='source',value=1)
        self.has(c,'choose_zone_card',player=1,value=8916)
        self.has(c,'assert_card',ref='source',property='tapped',value=1)
        self.has(c,'assert_life',player=0,value=10)
        self.has(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=8916,value=1)

    def test_fourth_land_is_inclusive_not_four_cards(self):
        c=self.case('Mirko Vosk, Mind Drinker','unblocked_0')
        self.has(c,'assert_zone',player=1,zone='graveyard',value=8) # Original Island +seven through fourth land.
        self.has(c,'assert_zone',player=1,zone='library',value=6)
        c=self.case('Mirko Vosk, Mind Drinker','fourth_land_1');self.has(c,'assert_zone',player=0,zone='library',value=0)
        for mode in ('three_lands','no_lands'):
            c=self.case('Mirko Vosk, Mind Drinker',mode+'_0');self.has(c,'assert_zone',player=1,zone='library',value=0)
        c=self.case('Mirko Vosk, Mind Drinker','unblocked_1')
        self.assertEqual([s['player'] for s in c['steps'] if s['op']=='choose_zone'],[0])

    def test_scalpelexis_repeats_only_for_names_in_one_current_batch(self):
        for mode,left in [('unblocked',5),('repeat_once',5),('cross_batch_name',5),('repeat_twice',5),('short_duplicate',0)]:
            c=self.case('Scalpelexis',mode+'_0');self.has(c,'assert_zone',player=1,zone='library',value=left)
        c=self.case('Scalpelexis','cross_batch_name_0')
        self.has(c,'assert_zone',player=1,zone='exile',value=9) # Initial Mountain +eight, not all13.
        c=self.case('Scalpelexis','repeat_twice_0');self.has(c,'assert_zone',player=1,zone='exile',value=13)

    def test_scry_has_real_event_observer_not_only_unchanged_legal_permutation(self):
        c=self.case('Windrider Patrol','scry_event_0')
        self.has(c,'cast',ref='scry_observer',value=1)
        self.has(c,'record_library',player=0);self.has(c,'assert_scry_result',player=0,value=2)
        self.has(c,'assert_card',ref='scry_observer',property='power',value=4)
        self.has(c,'assert_card',ref='scry_observer',property='toughness',value=3)
        self.has(c,'assert_card',ref='scry_observer',property='power',value=2)
        self.has(c,'assert_mana_total',player=0,value=57)
        self.has(c,'assert_zone_card',player=0,zone='library',index=6,value=145)

    def test_surveillance_observer_is_really_cast_and_entry_ledgers_precede_damage(self):
        c=self.case('Grim Flayer','surveil_event_0');steps=c['steps'];cast=next(i for i,s in enumerate(steps) if s['op']=='cast' and s['ref']=='surveil_observer');damage=next(i for i,s in enumerate(steps) if s['op']=='combat_damage')
        self.assertLess(cast,damage)
        self.assertTrue(any(s['name']=='Disinformation Campaign' and s['zone']=='hand' for s in c['setup']))
        self.has(c,'assert_mana_total',player=0,value=57)
        self.has(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=91,value=1)
        self.has(c,'assert_zone_multiplicity',player=0,zone='hand',index=19340,value=1)
        self.has(c,'assert_card',ref='surveil_observer',property='in_play',value=0)
        self.has(c,'assert_zone_multiplicity',player=0,zone='hand',index=164,value=1)

    def test_delirium_counts_types_not_cards_or_opponent_grave(self):
        for mode,power in [('delirium_four',4),('delirium_multitype',4),('delirium_duplicates',2),('opponent_delirium',2)]:
            c=self.case('Grim Flayer',mode+'_0');first=next(s for s in c['steps'] if s['op']=='assert_card' and s['ref']=='source' and s['property']=='power');self.assertEqual(first['value'],power)
        c=self.case('Grim Flayer','two_hits_0');self.has(c,'assert_life',player=1,value=14) # First2, then4 after adding Creature.
        self.has(c,'assert_card',ref='source',property='power',value=4)

    def test_delirium_reset_is_actual_crypt_sacrifice_exile(self):
        c=self.case('Grim Flayer','delirium_exile_1');self.has(c,'cast',ref='crypt',value=1)
        self.has(c,'choose_player',player=1,value=1);self.has(c,'activate',ref='crypt',value=1)
        self.has(c,'assert_zone',player=1,zone='graveyard',value=0)
        self.has(c,'assert_card',ref='source',property='power',value=2)
        self.has(c,'assert_zone_multiplicity',player=1,zone='exile',index=368,value=1)

    def test_flying_and_trample_have_actual_contrary_and_excess_controls(self):
        c=self.case('Dragonlord Ojutai','ground_block_0');self.has(c,'block',ref='ground_blocker',other='source',value=0)
        c=self.case('Borborygmos Enraged','trample_small_block_0');self.has(c,'block',ref='small_blocker',other='source',value=1)
        self.has(c,'assert_life',player=1,value=12);self.has(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=106,value=1)
        self.has(c,'assert_zone',player=0,zone='library',value=5)

    def test_noncombat_boundary_is_actual_granted_tap_and_no_prefix_effect(self):
        c=self.case('Borborygmos Enraged','noncombat_player_1');self.has(c,'cast',ref='grant',value=1)
        self.has(c,'activate',ref='grant',value=1);self.has(c,'assert_card',ref='source',property='tapped',value=1)
        self.has(c,'assert_card',ref='grant',property='tapped',value=0)
        self.has(c,'assert_life',player=0,value=19);self.has(c,'assert_zone',player=1,zone='library',value=8)
        self.assertFalse(any(s['op']=='choose_zone' for s in c['steps']))

    def test_two_hits_keep_actual_turns_and_snapshots_replay_actual_operations(self):
        c=self.case('Scalpelexis','two_hits_1');s=c['steps'];a=[i for i,x in enumerate(s) if x['op']=='advance_turn'];self.assertEqual(len(a),2)
        self.assertEqual(s[a[0]:a[1]+3],[dict(op='advance_turn'),dict(op='untap_step',player=0),dict(op='advance_turn'),dict(op='untap_step',player=1),dict(op='turn',player=1,value=0)])
        for n,v in [('Grim Flayer','ready_snapshot_0'),('Dragonlord Ojutai','combat_snapshot_1'),('Borborygmos Enraged','activation_snapshot_0')]:
            c=self.case(n,v);ops=[s['op'] for s in c['steps']];self.assertEqual(ops.count('capture'),1);self.assertEqual(ops.count('restore'),1);self.assertEqual(ops[-1],'assert_recorded_state')


if __name__=='__main__':unittest.main()
