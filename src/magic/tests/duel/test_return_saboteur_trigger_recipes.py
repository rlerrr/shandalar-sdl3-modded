"""Focused Oracle, lifecycle and destination guards for return saboteurs."""
import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry, OPS
import return_saboteur_trigger_recipes as model


class ReturnSaboteurTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']:r for r in registry()}

    def declaration(self, name):
        return dict(family=model.FAMILY, version=2, params=model.parameters(self.oracle[name]),
                    oracle_text=self.oracle[name]['oracle_text'], variants=['unblocked_0'])

    def case(self, name, variant):
        d = self.declaration(name); d['variants'] = [variant]; p = model.spec(d, self.oracle[name])
        return model.expand(name, self.rows[name]['csvid'], d, p, [variant], self.rows, self.oracle)[0]

    def has(self, c, op, **kw): self.assertIn(dict(op=op, **kw), c['steps'])

    def test_complete_oracle_derives_destinations_and_actual_costs(self):
        expected = {'Cephalid Constable':('bounce_damage','permanent',None),
                    'Blizzard Specter':('modal_bounce_discard','permanent',None),
                    'Dromar, the Banisher':('color_bounce','creature','{2}{U}'),
                    'Charnelhoard Wurm':('grave_hand','card',None),
                    'The Unspeakable':('grave_hand','arcane',None),
                    'Scion of Darkness':('grave_play','damaged_grave',None),
                    'Teneb, the Harvester':('grave_play','any_grave','{2}{B}'),
                    'Ink-Eyes, Servant of Oni':('grave_play','damaged_grave',None)}
        for n, values in expected.items():
            p=model.parameters(self.oracle[n]); self.assertEqual((p['effect'],p['selector'],p['trigger_cost']),values)
        self.assertEqual(model.parameters(self.oracle['Scion of Darkness'])['cycling'],'{3}')
        p=model.parameters(self.oracle['Ink-Eyes, Servant of Oni'])
        self.assertEqual((p['ninjutsu'],p['regeneration']),('{3}{B}{B}','{1}{B}'))

    def test_strict_version_typed_params_and_unique_variants(self):
        d=self.declaration('Cephalid Constable')
        for values in (dict(version=2.0),dict(version=True),dict(family='combat_value_trigger'),dict(variants=[]),
                       dict(variants=['unblocked_0','unblocked_0']),dict(variants=['unblocked_2']),dict(oracle_text=d['oracle_text']+' ')):
            with self.assertRaises(ValueError):model.spec(dict(d,**values),self.oracle['Cephalid Constable'])
        bad=copy.deepcopy(d);bad['params']['optional']=1
        with self.assertRaises(ValueError):model.spec(bad,self.oracle['Cephalid Constable'])

    def test_unknown_clauses_and_changed_payment_fail_closed(self):
        for name, old, new in [('Scion of Darkness','Cycling {3}','Cycling {2}'),
                              ('Dromar, the Banisher','{2}{U}','{3}{U}'),
                              ('Cephalid Constable','target permanents','target creatures')]:
            r=copy.deepcopy(self.oracle[name]);r['oracle_text']=r['oracle_text'].replace(old,new)
            with self.assertRaises(ValueError):model.spec(self.declaration(name),r)
        r=copy.deepcopy(self.oracle['Charnelhoard Wurm']);r['oracle_text']+='\nHaste'
        with self.assertRaises(ValueError):model.parameters(r)

    def test_full_source_and_witness_implementation_pins(self):
        model.validate_witnesses(self.oracle,self.rows)
        for name, field, value in [('Elvish Visionary','oracle_text','When this creature enters, draw two cards.'),
                                  ('Wall of Denial','toughness','7'),('Murder','mana_cost','{B}{B}')]:
            o=copy.deepcopy(self.oracle);o[name][field]=value
            with self.assertRaises(ValueError):model.validate_witnesses(o,self.rows)
        rows=copy.deepcopy(self.rows);rows['Teneb, the Harvester']['handler']='card_dummy'
        with self.assertRaises(ValueError):model.validate_witnesses(self.oracle,rows)

    def test_all_declared_variants_generate_registered_real_operations(self):
        count=0
        for name in model.profiles()['sources']:
            d=self.declaration(name);p=model.spec(d,self.oracle[name]);chosen=model.variants(p)
            cases=model.expand(name,self.rows[name]['csvid'],d,p,chosen,self.rows,self.oracle)
            self.assertEqual(len(cases),len(set(c['name'] for c in cases)))
            for c in cases:
                self.assertEqual(c['_recipe']['oracle_text'],self.oracle[name]['oracle_text'])
                self.assertLessEqual({s['op'] for s in c['steps']},OPS)
                self.assertFalse(any(s['op'] in ('set_life','destroy','remove_from_combat','reset_death_history') for s in c['steps']))
                self.assertFalse(any('attacking' in s.get('state',[]) or 'opponent_owned' in s.get('state',[]) for s in c['setup']))
            count+=len(cases)
        self.assertEqual(count,393)

    def test_damage_limit_up_to_zero_partial_and_land_artifact_destinations(self):
        c=self.case('Cephalid Constable','partial_returns_0')
        self.has(c,'cast',ref='helper',value=1);self.has(c,'choose_target',player=0,ref='victim')
        self.has(c,'choose_optional_target',player=0,ref='land')
        self.has(c,'assert_life',player=1,value=16)
        self.has(c,'assert_zone_multiplicity',player=1,zone='hand',index=106,value=1)
        z=self.case('Cephalid Constable','zero_returns_1')
        self.has(z,'choose_optional_target',player=1,ref='victim');self.has(z,'assert_forced_target_choices',value=0)
        for variant,ref,csvid in [('land_return_0','land',91),('artifact_return_0','artifact',514)]:
            c=self.case('Cephalid Constable',variant);self.has(c,'choose_target',player=0,ref=ref)
            self.has(c,'assert_zone_multiplicity',player=1,zone='hand',index=csvid,value=2 if csvid==91 else 1)

    def test_each_remaining_damage_slot_can_be_declined_without_extra_returns(self):
        # Actual Cephalid UI has four one-slot prompts after Growth: choose
        # one victim and decline three separate remaining slots. The Oracle
        # effect remains return ONE permanent after FOUR damage.
        for owner in (0,1):
            for mode,expected in [('partial_returns',3),('zero_returns',1),('one_return',0),('pump',0),('unblocked',0),('land_return',0),('artifact_return',0)]:
                c=self.case('Cephalid Constable',f'{mode}_{owner}')
                stops=[s for s in c['steps'] if s['op']=='choose_optional_target']
                self.assertEqual(len(stops),expected,(mode,owner))
                self.assertTrue(all(s['player']==owner for s in stops))
                if mode=='partial_returns':
                    self.has(c,'assert_life',player=1-owner,value=16)
                    self.has(c,'assert_zone_multiplicity',player=1-owner,zone='hand',index=106,value=1)
                    self.has(c,'assert_card',ref='land',property='in_play',value=1)
                    self.has(c,'assert_card',ref='artifact',property='in_play',value=1)
                    self.has(c,'assert_forced_target_choices',value=0)

    def test_modal_bounce_is_non_targeted_and_victim_chooses(self):
        c=self.case('Blizzard Specter','shroud_return_0')
        self.has(c,'choose_dialog',player=0,value=1);self.has(c,'choose_target',player=1,ref='wall')
        self.has(c,'assert_zone_multiplicity',player=1,zone='hand',index=10374,value=1)
        self.assertEqual(set(self.oracle['Wall of Denial']['keywords']),{'Defender','Flying','Shroud'})

    def test_modal_discard_keeps_mixed_hand_and_exact_identity_conservation(self):
        c=self.case('Blizzard Specter','discard_mode_0')
        names=[s['name'] for s in c['setup'] if s['zone']=='hand' and s['player']==1]
        self.assertEqual(names,['Forest','Plains'])
        self.has(c,'choose_dialog',player=0,value=0)
        self.has(c,'assert_hand_graveyard_total',player=1,index=91,value=2)
        self.has(c,'assert_hand_graveyard_total',player=1,index=188,value=1)
        self.has(c,'assert_zone',player=1,zone='hand',value=1);self.has(c,'assert_zone',player=1,zone='graveyard',value=2)

    def test_color_bounce_real_payment_both_sides_and_multicolor_membership(self):
        c=self.case('Dromar, the Banisher','trigger_payment_0')
        self.has(c,'choose_dialog',player=0,value=0);self.has(c,'choose_color',player=0,value=3)
        self.has(c,'assert_mana_total',player=0,value=0);self.has(c,'assert_mana',player=0,color=2,value=0)
        self.has(c,'assert_zone',player=0,zone='hand',value=4);self.has(c,'assert_zone',player=1,zone='hand',value=3)
        for variant,color in [('self_color_0',2),('multicolor_0',1)]:
            c=self.case('Dromar, the Banisher',variant);self.has(c,'choose_color',player=0,value=color)
            self.has(c,'assert_zone_multiplicity',player=0,zone='hand',index=4988,value=1)
        c=self.case('Dromar, the Banisher','trigger_wrong_color_0')
        self.assertFalse(any(s['op']=='choose_color' for s in c['steps']));self.has(c,'assert_mana_total',player=0,value=60)

    def test_any_damage_charnelhoard_keeps_correct_noncombat_effect(self):
        c=self.case('Charnelhoard Wurm','noncombat_player_0')
        self.has(c,'cast',ref='grant',value=1);self.has(c,'activate',ref='grant',value=1)
        self.has(c,'choose_zone_card',player=0,value=7353)
        self.has(c,'assert_zone_multiplicity',player=0,zone='hand',index=7353,value=1)
        self.has(c,'assert_card',ref='source',property='tapped',value=1)
        self.has(c,'assert_card',ref='grant',property='tapped',value=0)
        self.has(c,'assert_life',player=1,value=19)
        self.assertFalse(any(s['op']=='combat_damage' for s in c['steps']))
        c=self.case('Charnelhoard Wurm','noncombat_self_1')
        self.assertFalse(any(s['op']=='choose_zone_card' for s in c['steps']))

    def test_missing_noncombat_trigger_choices_are_observed_then_drained(self):
        for owner in (0,1):
            c=self.case('Charnelhoard Wurm',f'noncombat_player_{owner}');s=c['steps']
            activation=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='grant')
            after=s[activation:]
            observed=next(i for i,x in enumerate(after) if x['op']=='assert_unused_choices')
            strict=next(i for i,x in enumerate(after) if x['op']=='assert_choices_consumed')
            self.assertLess(observed,strict);self.assertEqual(after[observed],dict(op='assert_unused_choices',value=0))
            self.has(c,'choose_zone_card',player=owner,value=7353)
            self.has(c,'assert_zone_multiplicity',player=owner,zone='hand',index=7353,value=1)
            self.has(c,'assert_zone_multiplicity',player=owner,zone='graveyard',index=7353,value=0)
            self.assertFalse(any(x['op']=='choose_priority' for x in s))

    def test_arcane_filter_empty_other_grave_and_land_recovery(self):
        c=self.case('The Unspeakable','unblocked_0')
        self.has(c,'choose_zone_card',player=0,value=7353)
        self.has(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=91,value=1)
        c=self.case('The Unspeakable','wrong_type_0');self.has(c,'choose_optional_zone',player=0,value=7353)
        for variant in ('other_grave_0','empty_grave_0'):
            c=self.case('Charnelhoard Wurm',variant)
            self.assertFalse(any(s['op']=='choose_zone_card' for s in c['steps']))
        c=self.case('Charnelhoard Wurm','land_return_0')
        self.has(c,'choose_zone_card',player=0,value=91)
        self.has(c,'assert_zone_multiplicity',player=0,zone='hand',index=91,value=2)

    def test_reanimation_real_entry_draw_and_original_owner_destination(self):
        c=self.case('Scion of Darkness','reanimated_bounce_0')
        self.has(c,'choose_zone_card',player=0,value=1363)
        self.has(c,'bind_permanent_copy',player=0,value=1363,index=0,ref='returned_0')
        self.has(c,'assert_card',ref='returned_0',property='sick',value=1)
        self.has(c,'assert_zone',player=0,zone='library',value=6)
        self.has(c,'choose_target',player=0,ref='returned_0');self.has(c,'cast',ref='helper',value=1)
        self.has(c,'assert_zone_multiplicity',player=1,zone='hand',index=1363,value=1)
        self.has(c,'assert_zone_multiplicity',player=0,zone='hand',index=1363,value=0)

    def test_any_grave_paid_reanimation_and_decline(self):
        c=self.case('Teneb, the Harvester','both_graves_0')
        self.has(c,'choose_player',player=0,value=0);self.has(c,'choose_zone_card',player=0,value=106)
        self.has(c,'bind_permanent_copy',player=0,value=106,index=1,ref='returned_0')
        self.has(c,'assert_mana_total',player=0,value=57)
        c=self.case('Teneb, the Harvester','declined_0');self.has(c,'choose_cancel_mana',player=0)
        self.assertFalse(any(s['op']=='choose_zone_card' for s in c['steps']))

    def test_paid_human_decline_remains_affordable_without_floating_autopayment(self):
        c=self.case('Teneb, the Harvester','declined_0')
        lands=[s for s in c['setup'] if s['ref'].startswith('payment_land_')]
        self.assertEqual([s['name'] for s in lands],['Forest','Forest','Swamp'])
        self.assertTrue(all(s['zone']=='battlefield' and s['player']==0 and not s.get('state') for s in lands))
        for color in range(6): self.has(c,'set_mana',player=0,color=color,value=0)
        self.has(c,'choose_cancel_mana',player=0);self.has(c,'assert_mana_total',player=0,value=0)
        for s in lands:self.has(c,'assert_card',ref=s['ref'],property='tapped',value=0)
        self.has(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=1363,value=1)
        self.assertFalse(any(s['op']=='bind_permanent_copy' for s in c['steps']))

    def test_ai_payment_decline_is_exactly_one_pending_control_not_shortage(self):
        excluded=[]
        for name in model.profiles()['sources']:
            d=self.declaration(name);p=model.spec(d,self.oracle[name])
            if 'declined_0' in model.variants(p) and 'declined_1' not in model.variants(p):excluded.append(name)
        self.assertEqual(excluded,['Teneb, the Harvester'])
        d=self.declaration('Teneb, the Harvester');d['variants']=['declined_1']
        with self.assertRaises(ValueError):model.spec(d,self.oracle['Teneb, the Harvester'])

    def test_real_cycling_cost_discard_precedes_actual_draw(self):
        c=self.case('Scion of Darkness','cycling_payment_0');s=c['steps'];ops=[x['op'] for x in s]
        self.assertLess(ops.index('activate_hand'),ops.index('resolve'))
        self.has(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=6432,value=1)
        self.has(c,'assert_mana_total',player=0,value=0);self.has(c,'assert_zone',player=0,zone='library',value=6)
        c=self.case('Scion of Darkness','cycling_insufficient_1')
        self.has(c,'can_activate_hand',ref='source',value=0)
        self.assertFalse(any(s['op']=='activate_hand' for s in c['steps']))

    def test_ninjutsu_uses_real_postblockers_window_and_return_cost(self):
        c=self.case('Ink-Eyes, Servant of Oni','ninjutsu_payment_1');s=c['steps'];ops=[x['op'] for x in s]
        self.assertLess(ops.index('advance_combat'),ops.index('activate_hand'))
        at=ops.index('activate_hand');self.assertEqual(s[at-2:at],[dict(op='choose_dialog',player=1,value=1),dict(op='choose_target',player=1,ref='idle')])
        for prop in ('tapped','attacking','sick'):self.has(c,'assert_card',ref='source',property=prop,value=1)
        self.has(c,'assert_zone_multiplicity',player=1,zone='hand',index=106,value=1)
        self.has(c,'assert_mana_total',player=1,value=0)
        self.has(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=1363,value=0)
        for variant in ('ninjutsu_insufficient_0','ninjutsu_blocked_0'):
            c=self.case('Ink-Eyes, Servant of Oni',variant)
            self.assertFalse(any(s['op']=='activate_hand' for s in c['steps']))

    def test_regeneration_uses_legal_destroy_spell_and_actual_cost(self):
        c=self.case('Ink-Eyes, Servant of Oni','regenerate_0')
        self.assertIn('Murder',[s['name'] for s in c['setup']]);self.has(c,'choose_regeneration',ref='source')
        self.has(c,'assert_card',ref='source',property='in_play',value=1)
        self.has(c,'assert_card',ref='source',property='tapped',value=1)
        self.has(c,'assert_mana_total',player=0,value=55)
        for variant in ('regenerate_insufficient_0','regenerate_cannot_0'):
            c=self.case('Ink-Eyes, Servant of Oni',variant)
            self.assertFalse(any(s['op']=='choose_regeneration' for s in c['steps']))
            self.has(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=7493,value=1)

    def test_ninjutsu_then_regeneration_has_two_actual_payments_and_destruction(self):
        c=self.case('Ink-Eyes, Servant of Oni','ninjutsu_regenerate_0');ops=[s['op'] for s in c['steps']]
        self.assertLess(ops.index('activate_hand'),ops.index('combat_damage'))
        self.assertLess(ops.index('end_combat'),ops.index('cast'))
        self.has(c,'choose_regeneration',ref='source');self.has(c,'assert_mana_total',player=0,value=50)
        self.has(c,'assert_card',ref='source',property='in_play',value=1)

    def test_two_hits_advance_actual_turns_and_select_same_owner_main_phase(self):
        c=self.case('Scion of Darkness','two_hits_1');s=c['steps']
        a=[i for i,x in enumerate(s) if x['op']=='advance_turn'];self.assertEqual(len(a),2)
        self.assertEqual(s[a[0]:a[1]+3],[dict(op='advance_turn'),dict(op='untap_step',player=0),dict(op='advance_turn'),dict(op='untap_step',player=1),dict(op='turn',player=1,value=0)])
        self.assertEqual(sum(x['op']=='combat_damage' for x in s),2)

    def test_snapshots_replay_real_decisions_and_pending_hand_abilities(self):
        for name,variant in [('Scion of Darkness','cycling_pending_snapshot_0'),('Ink-Eyes, Servant of Oni','ninjutsu_pending_snapshot_1'),('Cephalid Constable','combat_snapshot_0')]:
            c=self.case(name,variant);ops=[s['op'] for s in c['steps']]
            self.assertEqual(ops.count('capture'),1);self.assertEqual(ops.count('restore'),1)
            self.assertEqual(ops[-1],'assert_recorded_state')
            self.assertLess(ops.index('capture'),ops.index('record_state'))
            self.assertLess(ops.index('record_state'),ops.index('restore'))


if __name__ == '__main__': unittest.main()
