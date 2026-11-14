import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand as public_expand, cost_from_oracle
import death_modular_composition_recipes as family


class DeathModularCompositionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}; cls.data=family.pins()

    def declaration(self,name,variants):
        return dict(family=family.FAMILY,version=2,params=dict(schema=1,**family.oracle_model(self.oracle[name])),
                    oracle_text=self.oracle[name]['oracle_text'],variants=variants)

    def case(self,name='Arcbound Crusher',mode='transfer',player=0):
        d=self.declaration(name,[f'{mode}_{player}'])
        return public_expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,self.oracle)[0]

    def has(self,c,**fields):
        return any(all(s.get(k)==v for k,v in fields.items()) for s in c['steps'])

    def receipt(self,c,ref,prop,value):
        return self.has(c,op='assert_card',ref=ref,property=prop,value=value)

    def zone(self,c,name,p,zone,value):
        return self.has(c,op='assert_zone_multiplicity',index=self.rows[name]['csvid'],player=p,zone=zone,value=value)

    def test_complete_clauses_and_typed_models_fail_closed(self):
        for name in self.data['sources']:
            d=self.declaration(name,['transfer_0']);family.spec(d,self.oracle[name])
            for field,value in [('schema',True),('initial_counters',True),('activation_generic',False),('counter_cost',False)]:
                bad=copy.deepcopy(d);bad['params'][field]=value
                with self.assertRaises(ValueError):family.spec(bad,self.oracle[name])
            for text in (self.oracle[name]['oracle_text']+'\nDraw a card.',self.oracle[name]['oracle_text']+' Draw a card.'):
                with self.assertRaises(ValueError):family.oracle_model(dict(self.oracle[name],oracle_text=text))
            for key,value in [('power','1'),('layout','transform'),('type_line','Creature — Golem')]:
                with self.assertRaises(ValueError):family.oracle_model(dict(self.oracle[name],**{key:value}))
        for name in ('Arcbound Worker','Arcbound Wanderer','Arcbound Ravager'):
            with self.assertRaises(ValueError):family.oracle_model(self.oracle[name])

    def test_whole_pins_implemented_sources_and_static_witness_annotations(self):
        family.validate_witnesses(self.oracle,self.rows)
        self.assertEqual({s['csvid'] for s in self.data['sources'].values()},{6845,6849,6851,6852})
        for name in ('Arcbound Worker','Death Wind','Heat Ray',"Soul's Fire",'Fusion Elemental'):
            changed=copy.deepcopy(self.oracle);changed[name]['oracle_text']+='changed'
            with self.assertRaises(ValueError):family.validate_witnesses(changed,self.rows)
        for name in self.data['sources']:
            rows=copy.deepcopy(self.rows);rows[name]['handler']='NULL'
            with self.assertRaises(ValueError):family.validate_witnesses(self.oracle,rows)
        self.assertIn('Fusion Elemental',self.data['static_base_engine_witnesses'])

    def test_every_reserved_home_has_the_whole_supported_matrix(self):
        total=0
        for name,source in self.data['sources'].items():
            i=source['csvid'];path=Path(__file__).parent/f"data/cases/cards/{i//100:03}/{i:05}-{name.lower().replace(' ','-')}.json"
            home=json.loads(path.read_text(encoding='utf8'));d=home['recipes'][0];m=family.spec(d,self.oracle[name])
            self.assertEqual(d['variants'],family.variants(m));total+=len(d['variants'])
        self.assertEqual(total,296)

    def test_every_variant_public_router_without_seeded_rules_state(self):
        total=0
        for name,source in self.data['sources'].items():
            d=self.declaration(name,['transfer_0']);m=family.spec(d,self.oracle[name]);d['variants']=family.variants(m)
            cases=public_expand(name,source['csvid'],d,self.oracle[name],self.rows,self.oracle);total+=len(cases)
            for c in cases:
                self.assertFalse(any(s['op'] in ('destroy','move_card','set_life','set_counter','dispatch_gfp','set_budget') for s in c['steps']))
                self.assertFalse(any(s['op'].startswith('choose_trigger') for s in c['steps']))
                self.assertTrue(self.has(c,op='assert_unused_choices',value=0))
                self.assertTrue(self.has(c,op='assert_choices_consumed'))
                self.assertEqual(c['_recipe']['scope_gaps'],family.GAPS)
        self.assertEqual(total,296)

    def test_real_source_payments_entry_and_modular_counter_totals(self):
        for name in self.data['sources']:
            m=family.oracle_model(self.oracle[name]);n=m['initial_counters']
            for p in (0,1):
                c=self.case(name,player=p)
                self.assertTrue(self.has(c,op='cast',ref='source',value=1))
                self.assertTrue(self.receipt(c,'source','plus1_counters',n))
                self.assertTrue(self.receipt(c,'recipient','plus1_counters',n))
                self.assertTrue(self.zone(c,name,p,'graveyard',1))
                c=self.case(name,'generic_colored',p)
                self.assertEqual(sum(s['op']=='choose_mana' for s in c['steps']),int(self.oracle[name]['cmc']) if p==0 else 0)

    def test_pending_counterspell_has_real_parent_context_without_unoffered_target(self):
        for name in self.data['sources']:
            for p in (0,1):
                c=self.case(name,'countered',p);ops=[s['op'] for s in c['steps']]
                self.assertLess(ops.index('cast'),ops.index('announce_spell'));self.assertLess(ops.index('announce_spell'),ops.index('cast_response'))
                self.assertFalse(any(s['op']=='choose_target' for s in c['steps']))
                self.assertTrue(self.zone(c,name,p,'graveyard',1));self.assertTrue(self.zone(c,'Counterspell',1-p,'graveyard',1))

    def test_crusher_another_artifact_of_either_controller_and_nonartifact_control(self):
        for p in (0,1):
            for mode,who in [('artifact_own_entry',p),('artifact_enemy_entry',1-p)]:
                c=self.case(mode=mode,player=p)
                self.assertTrue(any(x['ref']=='entry_witness' and x['name']=='Sol Ring' and x['player']==who for x in c['setup']))
                self.assertTrue(self.has(c,op='cast',ref='entry_witness',value=1));self.assertTrue(self.receipt(c,'source','plus1_counters',2))
            c=self.case(mode='nonartifact_entry',player=p)
            self.assertTrue(self.receipt(c,'source','plus1_counters',1));self.assertFalse(self.receipt(c,'source','plus1_counters',2))
            c=self.case(mode='repeat_entry',player=p)
            self.assertTrue(self.receipt(c,'source','plus1_counters',3));self.assertTrue(self.receipt(c,'entry_second','plus1_counters',1))

    def test_crusher_real_trample_overflow_and_prevention_keep_earned_counters(self):
        for p in (0,1):
            c=self.case(mode='trample_overflow',player=p)
            self.assertTrue(self.has(c,op='block',ref='blocker',other='source',value=1))
            self.assertTrue(self.has(c,op='assert_life',player=1-p,value=19))
            self.assertTrue(self.zone(c,'Memnite',1-p,'graveyard',1))
            self.assertTrue(self.receipt(c,'recipient','plus1_counters',2))
            c=self.case(mode='trample_prevented',player=p)
            self.assertTrue(self.has(c,op='cast',ref='prevention',value=1))
            self.assertTrue(self.has(c,op='assert_life',player=1-p,value=20))
            self.assertTrue(self.zone(c,'Memnite',1-p,'graveyard',0))
            self.assertTrue(self.receipt(c,'recipient','plus1_counters',2))

    def test_overseer_real_upkeep_team_includes_source_and_excludes_enemy_nonmodular(self):
        for p in (0,1):
            c=self.case('Arcbound Overseer','own_upkeep_team',p)
            self.assertTrue(self.has(c,op='upkeep_step',player=p))
            self.assertTrue(self.receipt(c,'source','plus1_counters',7))
            self.assertTrue(self.receipt(c,'own_modular','plus1_counters',2))
            self.assertTrue(self.receipt(c,'enemy_modular','plus1_counters',1))
            self.assertFalse(self.receipt(c,'enemy_modular','plus1_counters',2))
            self.assertTrue(self.receipt(c,'recipient','plus1_counters',0))
            self.assertTrue(self.receipt(c,'nonmodular','plus1_counters',0))

    def test_overseer_upkeep_timing_repeat_and_source_absence(self):
        for p in (0,1):
            c=self.case('Arcbound Overseer','opponent_upkeep',p)
            self.assertTrue(self.has(c,op='upkeep_step',player=1-p));self.assertFalse(self.receipt(c,'source','plus1_counters',7))
            c=self.case('Arcbound Overseer','repeat_upkeep',p);self.assertTrue(self.receipt(c,'source','plus1_counters',8))
            c=self.case('Arcbound Overseer','source_enters_after_upkeep',p);s=c['steps']
            self.assertLess(next(i for i,x in enumerate(s) if x['op']=='upkeep_step'),next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source'))
            self.assertTrue(self.receipt(c,'source','plus1_counters',6));self.assertFalse(self.receipt(c,'source','plus1_counters',7))
            c=self.case('Arcbound Overseer','source_removed_before_upkeep',p)
            self.assertTrue(self.receipt(c,'recipient','plus1_counters',6));self.assertFalse(self.receipt(c,'recipient','plus1_counters',7))

    def test_six_counter_numeric_kills_use_real_x_costs_and_complete_combat_upkeep(self):
        for p in (0,1):
            for mode,spell in [('zero_toughness','Death Wind'),('lethal_damage','Heat Ray')]:
                c=self.case('Arcbound Overseer',mode,p)
                self.assertTrue(any(x['ref']=='death_spell' and x['name']==spell for x in c['setup']))
                self.assertTrue(self.has(c,op='choose_x',player=1-p,value=6));self.assertTrue(self.receipt(c,'recipient','plus1_counters',6))
                self.assertTrue(self.has(c,op='set_mana',player=1-p,color=0,value=6))
            c=self.case('Arcbound Overseer','combat_death',p)
            self.assertTrue(self.has(c,op='upkeep_step',player=p))
            self.assertTrue(any(x['ref']=='blocker' and x['name']=='Fusion Elemental' for x in c['setup']))
            self.assertTrue(self.receipt(c,'blocker','damage',7));self.assertTrue(self.receipt(c,'recipient','plus1_counters',7))

    def test_reclaimer_pinned_free_one_counter_cost_is_not_invented_payment(self):
        m=family.oracle_model(self.oracle['Arcbound Reclaimer'])
        self.assertEqual((m['activation_generic'],m['counter_cost'],m['initial_counters']),(0,1,2))
        for p in (0,1):
            c=self.case('Arcbound Reclaimer','activate_fresh',p);s=c['steps'];activate=next(i for i,x in enumerate(s) if x['op']=='activate')
            self.assertFalse(any(x['op']=='advance_turn' for x in s[:activate]))
            self.assertTrue(self.receipt(c,'source','plus1_counters',1));self.assertTrue(self.receipt(c,'recipient','plus1_counters',1))
            c=self.case('Arcbound Reclaimer','activation_no_mana_fee',p)
            self.assertTrue(self.has(c,op='assert_mana',player=p,color=0,value=2))

    def test_reclaimer_own_artifact_top_is_proved_by_real_draw(self):
        for p in (0,1):
            c=self.case('Arcbound Reclaimer','activate_fresh',p)
            self.assertTrue(self.has(c,op='choose_zone_card',player=p,value=self.rows['Sol Ring']['csvid']))
            self.assertTrue(self.has(c,op='cast',ref='draw',value=1))
            self.assertTrue(self.zone(c,'Sol Ring',p,'hand',1));self.assertTrue(self.zone(c,'Sol Ring',p,'graveyard',0))
            c=self.case('Arcbound Reclaimer','artifact_creature_top',p)
            self.assertTrue(self.zone(c,'Memnite',p,'hand',1))

    def test_reclaimer_last_counter_cost_death_preserves_activation_and_zero_transfer(self):
        for p in (0,1):
            c=self.case('Arcbound Reclaimer','last_counter_death',p)
            self.assertEqual(sum(x['op']=='activate' and x['ref']=='source' for x in c['steps']),2)
            self.assertTrue(self.zone(c,'Arcbound Reclaimer',p,'graveyard',1))
            self.assertTrue(self.zone(c,'Memnite',p,'hand',1));self.assertTrue(self.receipt(c,'recipient','plus1_counters',0))
            self.assertTrue(self.has(c,op='choose_target',player=p,ref='recipient'))
            self.assertTrue(self.has(c,op='assert_zone_card',player=p,zone='library',index=0,value=self.rows['Sol Ring']['csvid']))

    def test_reclaimer_counterless_live_body_comes_from_actual_growth_and_two_costs(self):
        for p in (0,1):
            c=self.case('Arcbound Reclaimer','counterless_cannot_activate',p)
            self.assertTrue(self.has(c,op='cast',ref='sustain',value=1))
            self.assertEqual(sum(x['op']=='activate' for x in c['steps']),2)
            self.assertTrue(self.receipt(c,'source','plus1_counters',0));self.assertTrue(self.receipt(c,'source','power',3))
            self.assertTrue(self.has(c,op='require_can_activate',ref='source',value=0))

    def test_reclaimer_negative_zones_and_controller_grave_are_separate_from_owner(self):
        for p in (0,1):
            for mode in ('enemy_grave_only','empty_own_grave'):
                c=self.case('Arcbound Reclaimer',mode,p)
                self.assertTrue(self.has(c,op='require_can_activate',ref='source',value=0))
                self.assertFalse(any(x['op']=='choose_zone_card' for x in c['steps']))
            c=self.case('Arcbound Reclaimer','controller_grave',p)
            self.assertTrue(self.zone(c,'Sol Ring',p,'hand',1))
            self.assertTrue(self.zone(c,'Arcbound Reclaimer',1-p,'graveyard',1))
            self.assertTrue(self.zone(c,'Control Magic',p,'graveyard',1))

    def test_slith_only_actual_combat_player_damage_adds_counter(self):
        for p in (0,1):
            c=self.case('Arcbound Slith','combat_player_hit',p)
            self.assertTrue(self.has(c,op='combat_damage'));self.assertTrue(self.has(c,op='assert_life',player=1-p,value=19))
            self.assertTrue(self.receipt(c,'source','plus1_counters',2));self.assertTrue(self.receipt(c,'recipient','plus1_counters',2))
            for mode in ('blocked_creature_hit','prevented_player_hit'):
                c=self.case('Arcbound Slith',mode,p)
                self.assertTrue(self.receipt(c,'source','plus1_counters',1));self.assertFalse(self.receipt(c,'source','plus1_counters',2))
            c=self.case('Arcbound Slith','repeat_combat_hit',p)
            self.assertTrue(self.receipt(c,'source','plus1_counters',3));self.assertTrue(self.has(c,op='assert_life',player=1-p,value=17))

    def test_slith_noncombat_damage_is_from_real_selected_creature(self):
        for p in (0,1):
            c=self.case('Arcbound Slith','noncombat_source_hit',p)
            self.assertTrue(any(x['name']=="Soul's Fire" for x in c['setup']))
            self.assertTrue(self.has(c,op='choose_player',player=p,value=1-p))
            self.assertFalse(any(x['op']=='combat_damage' for x in c['steps']))
            self.assertTrue(self.has(c,op='assert_life',player=1-p,value=19));self.assertTrue(self.receipt(c,'recipient','plus1_counters',1))

    def test_real_ownership_destinations_bounce_recast_and_optional_cancel(self):
        for name in self.data['sources']:
            c=self.case(name,'source_foreign_owner')
            self.assertTrue(self.has(c,op='cast',ref='control',value=1));self.assertTrue(self.zone(c,name,1,'graveyard',1))
            c=self.case(name,'recast_bounced');self.assertTrue(self.has(c,op='bind_hand',ref='recast_source',value=self.rows[name]['csvid']))
            self.assertTrue(self.has(c,op='cast',ref='recast_source',value=1))
            c=self.case(name,'decline');self.assertTrue(self.has(c,op='choose_optional_target',ref='recipient',player=0))
            self.assertTrue(self.receipt(c,'recipient','plus1_counters',0))

    def test_replays_capture_real_common_and_feature_boundaries(self):
        choices=[('Arcbound Crusher','pending_snapshot'),('Arcbound Overseer','upkeep_snapshot'),
                 ('Arcbound Reclaimer','activation_pending_snapshot'),('Arcbound Slith','combat_snapshot')]
        for name,mode in choices:
            c=self.case(name,mode)
            for op in ('capture','restore','record_state','ai_roundtrip','assert_recorded_state'):
                self.assertTrue(self.has(c,op=op))
            if mode=='activation_pending_snapshot':
                s=c['steps'];at=next(i for i,x in enumerate(s) if x['op']=='capture')
                self.assertEqual(s[at-1],dict(op='assert_stack',value=1));self.assertEqual(s[at+1]['op'],'resolve')


if __name__=='__main__':unittest.main()
