"""Whole-Oracle stage, subtype, ownership, payment and replay guards."""
import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry,OPS
import block_damage_destruction_recipes as model


class BlockDamageDestructionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,name):
        return dict(family=model.FAMILY,version=2,params=model.parameters(self.oracle[name]),oracle_text=self.oracle[name]['oracle_text'],variants=['attack_blocked_0'])

    def case(self,name,variant):
        d=self.declaration(name);d['variants']=[variant];p=model.spec(d,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],d,p,[variant],self.rows,self.oracle)[0]

    def has(self,c,op,**kw):self.assertIn(dict(op=op,**kw),c['steps'])

    def section(self,c,start,end):
        steps=c['steps'];a=next(i for i,s in enumerate(steps) if s['op']==start)
        b=next(i for i,s in enumerate(steps[a+1:],a+1) if s['op']==end)
        return steps[a+1:b]

    def test_complete_eight_source_clauses_and_companion_parameters(self):
        expected={'Alaborn Zealot':('block','both','immediate'),'Arrogant Bloodlord':('block','self','end_combat'),
                  'Engulfing Slagwurm':('block','other','immediate'),'Infernal Medusa':('block','other','end_combat'),
                  'Lowland Basilisk':('damage','other','end_combat'),'Mephitic Ooze':('damage','other','immediate'),
                  'Sosuke, Son of Seshiro':('damage','other','end_combat'),'Toxin Sliver':('damage','other','immediate'),
                  'Loyal Sentry':('block','both','immediate'),'Abomination':('block','other','end_combat'),
                  'Dread Specter':('block','other','end_combat'),'Rock Basilisk':('block','other','end_combat'),
                  'Ohran Viper':('damage','other','end_combat'),'Voracious Cobra':('damage','other','immediate')}
        self.assertEqual(set(model.profiles()['sources']),set(expected))
        for name,want in expected.items():
            p=model.parameters(self.oracle[name]);self.assertEqual((p['trigger'],p['victims'],p['timing']),want)
        self.assertTrue(model.parameters(self.oracle['Mephitic Ooze'])['artifact_power'])
        self.assertTrue(model.parameters(self.oracle['Sosuke, Son of Seshiro'])['snake_bonus'])
        self.assertTrue(model.parameters(self.oracle['Engulfing Slagwurm'])['toughness_life'])

    def test_strict_versions_full_oracle_and_boolean_parameter_identity(self):
        name='Toxin Sliver';d=self.declaration(name)
        for extra in (dict(version=2.0),dict(version=True),dict(variants=[]),dict(variants=['attack_blocked_0']*2),dict(oracle_text=d['oracle_text']+' ')):
            with self.assertRaises(ValueError):model.spec(dict(d,**extra),self.oracle[name])
        bad=copy.deepcopy(d);bad['params']['cannot_regenerate']=1
        with self.assertRaises(ValueError):model.spec(bad,self.oracle[name])
        for name in model.profiles()['sources']:
            r=copy.deepcopy(self.oracle[name]);r['oracle_text']+='\nDraw a card.'
            with self.assertRaises(ValueError):model.parameters(r)

    def test_independent_source_and_effect_witness_pins(self):
        model.validate_witnesses(self.oracle,self.rows)
        for name in ('Holy Strength','Reflexes','Mistform Ultimus','Wall of Brambles','Withstand Death','Lagonna-Band Trailblazer','Boomerang'):
            o=copy.deepcopy(self.oracle);o[name]['oracle_text']+=' '
            with self.assertRaises(ValueError):model.validate_witnesses(o,self.rows)
        rows=copy.deepcopy(self.rows);rows['Toxin Sliver']['handler']='card_dummy'
        with self.assertRaises(ValueError):model.validate_witnesses(self.oracle,rows)
        pin=model.profiles()['sources']['Arrogant Bloodlord']['compiled_source']
        self.assertEqual(pin['path'],'src/manalinkeh/cards/rise_of_the_eldrazi.c')

    def test_every_variant_uses_real_operations_and_retains_complete_record(self):
        count=0
        for name in model.profiles()['sources']:
            d=self.declaration(name);p=model.spec(d,self.oracle[name]);vs=model.variants(p)
            cases=model.expand(name,self.rows[name]['csvid'],d,p,vs,self.rows,self.oracle);count+=len(cases)
            self.assertEqual(len({c['name'] for c in cases}),len(vs))
            for c in cases:
                self.assertLessEqual({s['op'] for s in c['steps']},OPS)
                self.assertFalse(any(s['op'] in ('destroy','choose_priority','reset_death_history','remove_from_combat','set_life') for s in c['steps']))
                self.assertFalse(any('attacking' in s.get('state',[]) for s in c['setup']))
                self.assertEqual(c['_recipe']['oracle_text'],self.oracle[name]['oracle_text'])
        self.assertEqual(count,573)

    def test_alaborn_defends_and_both_die_before_combat_damage(self):
        for owner in (0,1):
            c=self.case('Alaborn Zealot','defend_'+str(owner))
            self.has(c,'turn',player=1-owner,value=0);self.has(c,'attack',ref='victim',value=1)
            self.has(c,'block',ref='source',other='victim',value=1)
            between=self.section(c,'advance_combat','combat_damage')
            for ref in ('source','victim'):self.assertIn(dict(op='assert_card',ref=ref,property='in_play',value=0),between)
        c=self.case('Alaborn Zealot','attack_blocked_0')
        self.assertIn(dict(op='assert_card',ref='victim',property='in_play',value=1),self.section(c,'advance_combat','combat_damage'))

    def test_delayed_effect_is_alive_after_damage_then_dies_at_real_end(self):
        for name in ('Infernal Medusa','Lowland Basilisk','Sosuke, Son of Seshiro'):
            c=self.case(name,'attack_blocked_0');middle=self.section(c,'combat_damage','end_combat')
            self.assertIn(dict(op='assert_card',ref='victim',property='in_play',value=1),middle)
            end=next(i for i,s in enumerate(c['steps']) if s['op']=='end_combat')
            self.assertIn(dict(op='assert_card',ref='victim',property='in_play',value=0),c['steps'][end:])

    def test_arrogant_power_boundary_and_real_indestructible_are_not_sacrifice(self):
        c=self.case('Arrogant Bloodlord','one_power_0');self.has(c,'assert_card',ref='source',property='in_play',value=0)
        c=self.case('Arrogant Bloodlord','two_power_0');self.has(c,'assert_card',ref='source',property='in_play',value=1)
        c=self.case('Arrogant Bloodlord','source_indestructible_1');self.has(c,'cast',ref='helper',value=1)
        self.has(c,'assert_card',ref='source',property='in_play',value=1);self.has(c,'assert_mana',player=1,color=3,value=9)

    def test_engulfing_life_uses_modified_toughness_and_is_not_conditional_on_death(self):
        c=self.case('Engulfing Slagwurm','toughness_gain_0')
        self.has(c,'choose_target',player=0,ref='victim');self.has(c,'cast',ref='victim_spell',value=1)
        self.has(c,'assert_life',player=0,value=25)
        c=self.case('Engulfing Slagwurm','indestructible_victim_0')
        self.has(c,'assert_life',player=0,value=21);self.has(c,'assert_card',ref='victim',property='in_play',value=1)

    def test_medusa_wall_attack_is_contrary_but_wall_defense_is_positive(self):
        c=self.case('Infernal Medusa','wall_attack_0');self.has(c,'assert_card',ref='victim',property='in_play',value=1)
        c=self.case('Infernal Medusa','wall_defend_0')
        self.assertEqual(next(s['name'] for s in c['setup'] if s['ref']=='victim'),'Mistform Ultimus')
        self.has(c,'attack',ref='victim',value=1);self.has(c,'assert_card',ref='victim',property='in_play',value=0)

    def test_actual_noncombat_source_and_next_real_end_combat_for_lowland(self):
        c=self.case('Lowland Basilisk','noncombat_creature_0')
        self.has(c,'cast',ref='grant',value=1);self.has(c,'activate',ref='grant',value=1)
        self.has(c,'assert_card',ref='source',property='tapped',value=1);self.has(c,'assert_card',ref='grant',property='tapped',value=0)
        self.has(c,'end_combat');self.has(c,'assert_card',ref='victim',property='in_play',value=0)
        c=self.case('Mephitic Ooze','noncombat_creature_0');self.has(c,'assert_card',ref='victim',property='in_play',value=1)

    def test_ooze_zero_power_and_own_artifact_scope_are_independent(self):
        for mode,power,dead in [('no_artifacts',0,False),('opponent_artifacts',0,False),('creature_artifact',1,True),('two_artifacts',2,True),('artifact_bounce',0,False)]:
            c=self.case('Mephitic Ooze',mode+'_0')
            self.has(c,'assert_card',ref='source',property='power',value=power)
            self.has(c,'assert_card',ref='victim',property='in_play',value=int(not dead))
        c=self.case('Mephitic Ooze','artifact_bounce_0');self.has(c,'cast',ref='helper',value=1)

    def test_artifact_return_uses_real_permanent_spell_not_creature_only_unsummon(self):
        for owner in (0,1):
            c=self.case('Mephitic Ooze','artifact_bounce_'+str(owner))
            self.assertEqual(next(s['name'] for s in c['setup'] if s['ref']=='helper'),'Boomerang')
            self.has(c,'choose_target',player=owner,ref='artifact')
            self.has(c,'cast',ref='helper',value=1)
            self.has(c,'assert_mana',player=owner,color=2,value=8)
            self.has(c,'assert_mana',player=owner,color=0,value=10)
            self.has(c,'assert_mana_total',player=owner,value=58)
            self.has(c,'assert_zone_multiplicity',player=owner,zone='hand',index=230,value=1)
            self.has(c,'assert_zone_multiplicity',player=owner,zone='graveyard',index=585,value=1)
            self.has(c,'assert_card',ref='source',property='power',value=0)
            self.has(c,'assert_card',ref='victim',property='in_play',value=1)

    def test_sosuke_own_warrior_damage_and_other_own_snake_bonus(self):
        c=self.case('Sosuke, Son of Seshiro','ally_hit_0')
        self.has(c,'assert_card',ref='ally',property='power',value=4)
        self.has(c,'assert_card',ref='team_victim',property='in_play',value=0)
        for mode in ('enemy_hit','wrong_type_hit'):
            c=self.case('Sosuke, Son of Seshiro',mode+'_0');self.has(c,'assert_card',ref='team_victim',property='in_play',value=1)
        c=self.case('Sosuke, Son of Seshiro','opponent_snake_0');self.has(c,'assert_card',ref='ally',property='power',value=3)
        self.has(c,'assert_card',ref='source',property='power',value=3)

    def test_toxin_applies_to_other_player_sliver_not_arbitrary_creature(self):
        c=self.case('Toxin Sliver','enemy_hit_0');self.has(c,'turn',player=1,value=0)
        self.has(c,'assert_card',ref='team_victim',property='in_play',value=0)
        c=self.case('Toxin Sliver','wrong_type_hit_0');self.has(c,'assert_card',ref='team_victim',property='in_play',value=1)

    def test_granted_ally_damage_never_targets_the_shroud_combat_witness(self):
        for name in ('Sosuke, Son of Seshiro','Toxin Sliver'):
            c=self.case(name,'ally_noncombat_0')
            target=next(s['name'] for s in c['setup'] if s['ref']=='team_victim')
            self.assertNotIn('Shroud',self.oracle[target]['keywords'])
            self.has(c,'choose_target',player=0,ref='team_victim')
            self.has(c,'assert_card',ref='team_victim',property='in_play',value=1)

    def test_regeneration_controls_pay_real_aura_on_own_prior_turn(self):
        c=self.case('Infernal Medusa','regenerate_victim_0');ops=[s['op'] for s in c['steps']]
        self.has(c,'turn',player=1,value=0);self.has(c,'choose_target',player=1,ref='victim')
        self.has(c,'cast',ref='victim_spell',value=1);self.has(c,'assert_mana_total',player=1,value=59)
        self.assertLess(ops.index('cast'),ops.index('advance_turn'));self.assertLess(ops.index('untap_step'),ops.index('begin_attackers'))
        self.has(c,'choose_regeneration',ref='victim');self.has(c,'assert_mana',player=1,color=3,value=9)
        self.has(c,'assert_card',ref='victim',property='in_play',value=1);self.has(c,'assert_card',ref='victim',property='tapped',value=1)

    def test_cannot_regenerate_keeps_actual_attempt_observable_and_correct_zones(self):
        for name in ('Mephitic Ooze','Toxin Sliver'):
            c=self.case(name,'regenerate_victim_0')
            self.has(c,'choose_regeneration',ref='victim');self.has(c,'assert_unused_choices',value=1)
            self.has(c,'assert_card',ref='victim',property='in_play',value=0)
            self.has(c,'assert_mana',player=1,color=3,value=10)
            self.has(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=112,value=1)

    def test_actual_first_strike_aura_cost_and_return_damage_timing(self):
        c=self.case('Mephitic Ooze','first_strike_1')
        self.has(c,'cast',ref='strike_aura',value=1);self.has(c,'assert_mana',player=1,color=4,value=9)
        self.has(c,'assert_card',ref='source',property='damage',value=0)
        self.has(c,'assert_card',ref='victim',property='in_play',value=0)

    def test_printed_source_costs_and_failures_for_both_controllers(self):
        for name in model.profiles()['sources']:
            for owner in (0,1):
                c=self.case(name,'payment_'+str(owner));self.has(c,'cast',ref='source',value=1)
                self.has(c,'assert_mana_total',player=owner,value=0)
                c=self.case(name,'wrong_color_'+str(owner));self.has(c,'cast',ref='source',value=0)
                self.has(c,'assert_card',ref='source',property='in_hand',value=1)

    def test_delayed_source_removal_uses_real_spell_not_synthetic_window(self):
        c=self.case('Infernal Medusa','source_removed_0')
        ops=[s['op'] for s in c['steps']]
        self.assertLess(ops.index('combat_damage'),ops.index('cast'));self.assertLess(ops.index('cast'),ops.index('end_combat'))
        self.has(c,'choose_target',player=0,ref='source');self.has(c,'cast',ref='helper',value=1)
        self.has(c,'assert_card',ref='source',property='in_play',value=0);self.has(c,'assert_card',ref='victim',property='in_play',value=0)

    def test_snapshot_replay_retains_real_stage_boundaries(self):
        for mode in ('ready_snapshot','declared_snapshot','damage_snapshot'):
            c=self.case('Lowland Basilisk',mode+'_0');ops=[s['op'] for s in c['steps']]
            self.assertEqual(ops.count('capture'),1);self.assertEqual(ops.count('restore'),1)
            self.assertEqual(ops.count('assert_recorded_state'),1)
            if mode=='damage_snapshot':self.assertLess(ops.index('combat_damage'),ops.index('capture'))

    def test_follow_on_clause_compositions_are_whole_record_and_fail_closed_elsewhere(self):
        p=model.parameters(self.oracle['Ohran Viper']);self.assertTrue(p['optional_player_draw']);self.assertEqual(p['scope'],'combat')
        p=model.parameters(self.oracle['Voracious Cobra']);self.assertEqual(p['keywords'],['First strike'])
        for name in ('Creepy Doll','Phage the Untouchable','Stone-Tongue Basilisk','Simic Basilisk','Stinkweed Imp'):
            with self.assertRaises(ValueError):model.parameters(self.oracle[name])

    def test_loyal_source_uses_independent_record_and_correct_shared_handler(self):
        self.assertEqual(self.rows['Loyal Sentry']['handler'],self.rows['Alaborn Zealot']['handler'])
        c=self.case('Loyal Sentry','defend_1')
        self.assertEqual(c['_recipe']['oracle_text'],self.oracle['Loyal Sentry']['oracle_text'])
        self.assertNotEqual(c['_recipe']['oracle_text'],self.oracle['Alaborn Zealot']['oracle_text'])
        self.assertEqual(c['card'],'Loyal Sentry')
        self.has(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=4364,value=1)

    def test_typed_green_white_and_nonblack_conditions_are_not_same_predicate(self):
        for mode,abomination_dead,dread_dead in [('green_creature',True,True),('white_creature',True,True),
            ('black_creature',False,False),('colorless_creature',False,True),('multicolor_black',True,False),('multicolor_white',True,True)]:
            for name,dead in [('Abomination',abomination_dead),('Dread Specter',dread_dead)]:
                c=self.case(name,mode+'_0')
                end=next(i for i,s in enumerate(c['steps']) if s['op']=='end_combat')
                self.assertIn(dict(op='assert_card',ref='victim',property='in_play',value=int(not dead)),c['steps'][end:])
                self.assertFalse(any(s['op']=='choose_target' for s in c['steps']))

    def test_rock_nonwall_positive_survives_damage_and_wall_contraries_are_honest(self):
        c=self.case('Rock Basilisk','attack_blocked_0')
        self.assertEqual(next(s['name'] for s in c['setup'] if s['ref']=='victim'),'Mephitic Ooze')
        middle=self.section(c,'combat_damage','end_combat')
        self.assertIn(dict(op='assert_card',ref='victim',property='in_play',value=1),middle)
        self.assertIn(dict(op='assert_card',ref='victim',property='damage',value=4),middle)
        for mode in ('wall_attack','wall_defend'):
            c=self.case('Rock Basilisk',mode+'_0');self.has(c,'assert_card',ref='victim',property='in_play',value=1)
        c=self.case('Rock Basilisk','wall_defend_0');self.has(c,'cast',ref='victim_spell',value=1)
        self.has(c,'choose_target',player=1,ref='victim')
        self.has(c,'assert_card',ref='victim',property='toughness',value=5)
        c=self.case('Rock Basilisk','regenerate_victim_0')
        self.assertEqual(next(s['name'] for s in c['setup'] if s['ref']=='victim'),'Cudgel Troll')
        self.has(c,'choose_regeneration',ref='victim');self.has(c,'assert_card',ref='victim',property='in_play',value=1)

    def test_ohran_actual_player_draw_acceptance_and_human_decline(self):
        for owner in (0,1):
            c=self.case('Ohran Viper','unblocked_'+str(owner))
            self.assertFalse(any(s['op']=='block' for s in c['steps']))
            self.has(c,'assert_life',player=1-owner,value=19)
            self.has(c,'assert_zone',player=owner,zone='library',value=6)
            self.has(c,'assert_zone_multiplicity',player=owner,zone='hand',index=91,value=2)
            self.assertEqual(sum(s['op']=='choose_trigger_source' for s in c['steps']),int(owner==0))
        c=self.case('Ohran Viper','draw_declined_0')
        self.assertFalse(any(s['op'] in ('choose_trigger_source','block') for s in c['steps']))
        self.has(c,'assert_life',player=1,value=19);self.has(c,'assert_zone',player=0,zone='library',value=7)
        self.has(c,'assert_zone_multiplicity',player=0,zone='hand',index=91,value=1)
        self.assertNotIn('draw_declined_1',model.variants(model.spec(self.declaration('Ohran Viper'),self.oracle['Ohran Viper'])))

    def test_ohran_noncombat_neither_destroys_nor_draws(self):
        c=self.case('Ohran Viper','noncombat_creature_0')
        self.has(c,'activate',ref='grant',value=1);self.has(c,'assert_card',ref='victim',property='in_play',value=1)
        self.has(c,'assert_card',ref='victim',property='damage',value=1)
        self.has(c,'assert_zone',player=0,zone='library',value=7)

    def test_cobra_intrinsic_first_strike_needs_no_aura_and_stops_return_damage(self):
        for owner in (0,1):
            c=self.case('Voracious Cobra','intrinsic_first_strike_'+str(owner))
            self.assertFalse(any(s['name']=='Reflexes' for s in c['setup']))
            self.assertFalse(any(s['op']=='cast' for s in c['steps']))
            self.has(c,'assert_card',ref='source',property='in_play',value=1)
            self.has(c,'assert_card',ref='source',property='damage',value=0)
            self.has(c,'assert_card',ref='victim',property='in_play',value=0)
        c=self.case('Voracious Cobra','regenerate_victim_0')
        self.has(c,'choose_regeneration',ref='victim');self.has(c,'assert_card',ref='victim',property='in_play',value=1)
        self.has(c,'assert_card',ref='source',property='damage',value=0)
        c=self.case('Voracious Cobra','noncombat_creature_0')
        self.has(c,'assert_card',ref='victim',property='in_play',value=1)


if __name__=='__main__':unittest.main()
