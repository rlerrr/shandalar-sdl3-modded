"""Whole printed clauses and actual cost/lifetime compositions; no engine runs."""
import copy
import hashlib
import json
import unittest
from pathlib import Path
from unittest.mock import patch

from generate_cases import OPS, PROPERTIES, registry
import unblockability_composition_recipes as model

HARBOR = 'Harbor Bandit'; SOUL = 'Trespassing Souleater'; GEAR = 'Gearseeker Serpent'; MARCH = "Marchesa's Smuggler"


class UnblockabilityCompositionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']:r for r in registry()}; cls.cases = {}
        for name,pin in model.profiles()['sources'].items():
            d = dict(family=model.FAMILY,version=2,params=pin['params'],oracle_text=pin['oracle']['oracle_text'],variants=model.variants(pin['params']))
            p = model.spec(d,cls.oracle[name])
            cls.cases[name] = {c['_recipe']['variant']:c for c in model.expand(name,pin['csvid'],d,p,d['variants'],cls.rows,cls.oracle)}

    def case(self,name,variant): return self.cases[name][variant]
    def steps(self,name,variant): return self.case(name,variant)['steps']
    def has(self,name,variant,op,**kw): self.assertIn(dict(op=op,**kw),self.steps(name,variant))
    def last(self,name,variant,op,**kw):
        return next(s['value'] for s in reversed(self.steps(name,variant)) if s['op']==op and all(s.get(k)==v for k,v in kw.items()))
    def values(self,name,variant,op,**kw):
        return [s['value'] for s in self.steps(name,variant) if s['op']==op and all(s.get(k)==v for k,v in kw.items())]

    def test_four_independent_complete_original_records_and_exact_population(self):
        self.assertEqual({p['csvid'] for p in model.profiles()['sources'].values()},{12577,11701,16389,14046})
        self.assertEqual({n:len(c) for n,c in self.cases.items()},{HARBOR:62,SOUL:56,GEAR:74,MARCH:82})
        self.assertEqual(sum(map(len,self.cases.values())),274)
        self.assertEqual(hashlib.sha256((Path(__file__).parent/'data/oracle.json').read_bytes()).hexdigest(),'c11dcfedde2ed4ce99ff355f5cf30c187c2b3745c87a03319e715bbfec17cf50')
        for name,pin in model.profiles()['sources'].items():
            self.assertEqual(pin['oracle'],self.oracle[name]); self.assertEqual(pin['params'],model.parameters(self.oracle[name]))

    def test_full_new_clauses_and_costs_retained(self):
        expected = {HARBOR:('{1}{U}',True,None,False,False,False),SOUL:('{U}',False,'U',False,False,False),
            GEAR:('{5}{U}',False,None,True,False,False),MARCH:('{1}{U}{R}',False,None,False,True,True)}
        for name,want in expected.items():
            p = model.parameters(self.oracle[name])
            self.assertEqual((p['paid']['activation_cost'],*(p[k] for k in ('island_bonus','phyrexian_color','affinity_artifacts','dethrone','grants_haste'))),want)
        self.assertEqual(model.parameters(self.oracle[MARCH])['paid']['scope'],'own')

    def test_unknown_omitted_or_extra_clauses_fail_closed(self):
        for name in self.cases:
            for change in (dict(layout='transform'),dict(card_faces=[{}]),dict(power='*'),dict(keywords=['Flying']),
                    dict(oracle_text=self.oracle[name]['oracle_text']+'\nDraw a card.')):
                with self.assertRaises(ValueError): model.parameters(dict(self.oracle[name],**change))
            if '\n' in self.oracle[name]['oracle_text']:
                with self.assertRaises(ValueError): model.parameters(dict(self.oracle[name],oracle_text=self.oracle[name]['oracle_text'].split('\n')[-1]))
        with self.assertRaises(ValueError): model.parameters(dict(self.oracle[SOUL],oracle_text="{U/P}: This creature can't be blocked this turn."))

    def test_strict_version_parameter_types_and_variants(self):
        pin = model.profiles()['sources'][HARBOR]
        d = dict(family=model.FAMILY,version=2,params=pin['params'],oracle_text=pin['oracle']['oracle_text'],variants=['activated_0'])
        for version in (True,2.0,'2',1):
            with self.assertRaises(ValueError): model.spec(dict(d,version=version),self.oracle[HARBOR])
        for variants in ([],['activated_0']*2,['nonsense_0']):
            with self.assertRaises(ValueError): model.spec(dict(d,variants=variants),self.oracle[HARBOR])
        for field,value in [('island_bonus',1),('phyrexian_color',False)]:
            bad = copy.deepcopy(d); bad['params'][field] = value
            with self.assertRaises(ValueError): model.spec(bad,self.oracle[HARBOR])
        bad = copy.deepcopy(d); bad['params']['paid']['tap'] = 0
        with self.assertRaises(ValueError): model.spec(bad,self.oracle[HARBOR])

    def test_independent_compiled_implemented_normal_source_and_witness_gates(self):
        model.validate_witnesses(self.oracle,self.rows)
        for name in (HARBOR,SOUL,GEAR,MARCH,'Underground Sea','Stone Rain','Elvish Mystic','Counterspell','Shatter','Glasses of Urza'):
            bad = copy.deepcopy(self.oracle); bad[name]['oracle_text'] += ' '
            with self.assertRaises(ValueError): model.validate_witnesses(bad,self.rows)
        for field,value in [('handler','card_dummy'),('kind','token'),('csvid',-1)]:
            bad = copy.deepcopy(self.rows); bad[GEAR][field] = value
            with self.assertRaises(ValueError): model.validate_witnesses(self.oracle,bad)
        data = model.profiles(); data['sources'][GEAR]['compiled_source']['sha256'] = '0'*64
        with patch.object(model,'profiles',return_value=data):
            with self.assertRaises(ValueError): model.validate_witnesses(self.oracle,self.rows)

    def test_native_only_witnesses_never_cast_activated_or_used_for_costs(self):
        native = set(model.profiles()['native_characteristics_only_witnesses'])
        self.assertEqual(native,{'Grizzly Bears','Wall of Denial','Ornithopter','Centaur Courser'})
        for cases in self.cases.values():
            for case in cases.values():
                refs = {c['ref'] for c in case['setup'] if c['name'] in native}
                self.assertFalse(any(s['op'] in ('cast','activate','can_activate') and s.get('ref') in refs for s in case['steps']))

    def test_actual_operations_no_effect_counter_damage_or_priority_injection(self):
        for cases in self.cases.values():
            for case in cases.values():
                self.assertLessEqual({s['op'] for s in case['steps']},OPS)
                self.assertFalse(any(s['op'] in ('destroy','damage','put_counters','choose_priority','reset_death_history') for s in case['steps']))
                self.assertTrue(all(s['property'] in PROPERTIES for s in case['steps'] if s['op']=='assert_card'))
                self.assertIn(dict(op='assert_unused_choices',value=0),case['steps'])
                self.assertIn(dict(op='assert_forced_target_choices',value=0),case['steps'])
                # Initial life qualification precedes every actual spell/ability/combat action.
                for i,s in enumerate(case['steps']):
                    if s['op']=='set_life': self.assertFalse(any(x['op'] in ('cast','activate','begin_attackers') for x in case['steps'][:i]))

    def test_island_once_not_per_island_controller_tapped_and_nonbasic(self):
        for variant,power in [('activated',2),('island_own',3),('island_opponent',2),('island_two',3),('island_tapped',3),('island_dual',3)]:
            self.assertEqual(self.last(HARBOR,variant+'_0','assert_card',ref='source',property='power'),power)
            self.assertEqual(self.last(HARBOR,variant+'_0','assert_life',player=1),20-power)
        self.assertEqual(self.oracle['Underground Sea']['type_line'],'Land — Island Swamp')

    def test_island_real_removal_payment_and_bonus_not_activation_duration(self):
        self.has(HARBOR,'island_removed_0','choose_target',player=0,ref='island_0')
        self.has(HARBOR,'island_removed_0','cast',ref='land_remove',value=1)
        self.has(HARBOR,'island_removed_0','assert_mana_total',player=0,value=57)
        self.assertEqual(self.last(HARBOR,'island_removed_0','assert_card',ref='source',property='power'),2)
        self.assertEqual(self.last(HARBOR,'island_removed_0','assert_card',ref='island_0',property='in_play'),0)
        self.assertEqual(self.last(HARBOR,'island_expiry_0','assert_card',ref='source',property='power'),3)
        self.has(HARBOR,'island_expiry_0','block',ref='blocker',other='source',value=1)

    def test_phyrexian_blue_and_life_paid_exactly_with_real_number_for_both_owners(self):
        for owner in (0,1):
            for mode,choice,life in [('pay_blue',0,20),('pay_life',1,18)]:
                variant=f'{mode}_{owner}'
                self.has(SOUL,variant,'choose_number',player=owner,value=choice)
                self.has(SOUL,variant,'activate',ref='source',value=1)
                self.assertEqual(self.last(SOUL,variant,'assert_life',player=owner),life)
                self.assertEqual(self.last(SOUL,variant,'assert_mana_total',player=owner),0)
                self.has(SOUL,variant,'block',ref='blocker',other='source',value=0)

    def test_phyrexian_cancel_no_cost_or_grant_and_real_block(self):
        for owner in (0,1):
            v=f'pay_cancel_{owner}'
            self.has(SOUL,v,'choose_number',player=owner,value=-1)
            self.has(SOUL,v,'activate',ref='source',value=0)
            self.assertEqual(self.last(SOUL,v,'assert_life',player=owner),20)
            self.assertEqual(self.last(SOUL,v,'assert_mana_total',player=owner),1)
            self.has(SOUL,v,'block',ref='blocker',other='source',value=1)

    def test_phyrexian_low_life_blue_only_and_human_life3(self):
        for owner in (0,1):
            v=f'blue_low_life_{owner}'
            self.assertFalse(any(s['op']=='choose_number' for s in self.steps(SOUL,v)))
            self.has(SOUL,v,'activate',ref='source',value=1)
            self.assertEqual(self.last(SOUL,v,'assert_life',player=owner),1)
            v=f'insufficient_life_{owner}'
            self.has(SOUL,v,'can_activate',ref='source',value=0)
            self.has(SOUL,v,'activate',ref='source',value=0)
        self.has(SOUL,'life_low_0','choose_number',player=0,value=1)
        self.assertEqual(self.last(SOUL,'life_low_0','assert_life',player=0),1)

    def test_phyrexian_life2_query_only_preserves_boundary_and_honest_ai_gap(self):
        self.has(SOUL,'life_boundary_query_0','can_activate',ref='source',value=1)
        self.assertFalse(any(s['op'] in ('activate','choose_number') for s in self.steps(SOUL,'life_boundary_query_0')))
        self.assertNotIn('life_low_1',self.cases[SOUL]); self.assertNotIn('life_boundary_query_1',self.cases[SOUL])
        self.assertIn('AI low-life safety suppression',self.case(SOUL,'life_boundary_query_0')['_recipe']['untested_components'][1])

    def test_phyrexian_repeat_and_pending_replay_retain_actual_paid_life(self):
        self.assertEqual(self.values(SOUL,'life_repeat_0','choose_number',player=0),[1,1])
        self.assertEqual(self.last(SOUL,'life_repeat_0','assert_life',player=0),16)
        steps=self.steps(SOUL,'life_pending_snapshot_0'); cut=next(i for i,s in enumerate(steps) if s['op']=='capture')
        self.assertEqual(sum(s['op']=='activate' for s in steps[:cut]),1)
        self.assertTrue(any(s==dict(op='assert_life',player=0,value=18) for s in steps[:cut]))
        self.assertFalse(any(s['op']=='activate' for s in steps[cut:]))
        self.assertEqual(self.last(SOUL,'life_pending_snapshot_0','assert_life',player=0),18)

    def test_affinity_actual_costs_floor_blue_symbols_and_zone_controller_counts(self):
        generic = {'affinity_one':4,'affinity_three':2,'affinity_floor':0,'affinity_overflow':0,
            'affinity_opponent':5,'affinity_hand':5,'affinity_tapped':2,'affinity_creature':4}
        for mode,n in generic.items():
            v=mode+'_0'; steps=self.steps(GEAR,v); cast=next(i for i,s in enumerate(steps) if s['op']=='cast')
            for color,value in enumerate([n,0,2,0,0,0]):
                self.assertIn(dict(op='set_mana',player=0,color=color,value=value),steps[:cast])
            self.has(GEAR,v,'cast',ref='source',value=1)
            self.assertEqual(self.last(GEAR,v,'assert_mana_total',player=0),0)
            self.assertEqual(self.last(GEAR,v,'assert_card',ref='source',property='sick'),1)

    def test_affinity_cannot_pay_blue_floor_with_generic_or_one_blue(self):
        for mode in ('affinity_blue_short','affinity_wrong_color'):
            self.has(GEAR,mode+'_0','cast',ref='source',value=0)
            self.has(GEAR,mode+'_0','assert_card',ref='source',property='in_hand',value=1)
        self.assertEqual(self.last(GEAR,'affinity_blue_short_0','assert_mana_total',player=0),1)
        self.assertEqual(self.last(GEAR,'affinity_wrong_color_0','assert_mana_total',player=0),10)

    def test_affinity_real_artifact_removal_increases_spell_cost_and_not_ability_discount(self):
        self.has(GEAR,'affinity_removed_0','choose_target',player=0,ref='artifact_0')
        self.has(GEAR,'affinity_removed_0','cast',ref='artifact_remove',value=1)
        self.has(GEAR,'affinity_removed_0','assert_mana_total',player=0,value=5)
        self.assertEqual(self.last(GEAR,'affinity_removed_0','assert_mana_total',player=0),0)
        self.assertEqual(self.last(GEAR,'affinity_removed_0','assert_card',ref='artifact_0',property='in_play'),0)
        self.has(GEAR,'affinity_activation_0','set_mana',player=0,color=0,value=5)
        self.has(GEAR,'affinity_activation_0','set_mana',player=0,color=2,value=1)
        self.assertEqual(self.last(GEAR,'affinity_activation_0','assert_mana_total',player=0),0)
        self.has(GEAR,'affinity_activation_0','block',ref='blocker',other='source',value=0)

    def test_real_counterspell_parent_costs_and_zones_no_fabricated_target(self):
        for name in self.cases:
            for owner in (0,1):
                v=f'countered_cast_{owner}'
                self.has(name,v,'cast_response',ref='counter',value=1)
                self.has(name,v,'resolve_response')
                self.assertFalse(any(s['op']=='choose_target' for s in self.steps(name,v)))
                self.assertEqual(self.last(name,v,'assert_card',ref='source',property='in_play'),0)
                self.assertEqual(self.last(name,v,'assert_mana_total',player=1-owner),58)
        self.assertEqual(self.last(GEAR,'affinity_countered_1','assert_mana_total',player=1),0)
        self.assertEqual(self.last(GEAR,'affinity_countered_1','assert_mana_total',player=0),0)

    def test_dethrone_real_declaration_conditional_counter_and_damage(self):
        for mode,counters,opponent in [('dethrone_tied',1,18),('dethrone_behind',1,18),('dethrone_ahead',0,19),('dethrone_unactivated',1,20),('dethrone_other',0,20)]:
            v=mode+'_0'
            self.assertEqual(self.last(MARCH,v,'assert_card',ref='source',property='plus1_counters'),counters)
            self.assertEqual(self.last(MARCH,v,'assert_life',player=1),opponent)
            steps=self.steps(MARCH,v); declared=next(i for i,s in enumerate(steps) if s['op']=='finish_attackers')
            self.assertFalse(any(s.get('property')=='plus1_counters' and s.get('value')==1 for s in steps[:declared]))

    def test_dethrone_repeat_real_turns_retains_counters_and_retests_life(self):
        for owner in (0,1):
            v=f'dethrone_repeat_{owner}'; steps=self.steps(MARCH,v)
            self.assertEqual(sum(s['op']=='advance_turn' for s in steps),2)
            self.assertEqual(sum(s['op']=='untap_step' for s in steps),2)
            self.has(MARCH,v,'assert_card',ref='source',property='plus1_counters',value=1)
            self.assertEqual(self.last(MARCH,v,'assert_card',ref='source',property='plus1_counters'),2)
            self.assertEqual(self.last(MARCH,v,'assert_life',player=1-owner),15)

    def test_fresh_creature_haste_is_real_cast_then_own_target_and_legal_attack(self):
        for mode,ref in [('haste_fresh_ally','ally'),('haste_fresh_self','source')]:
            for owner in (0,1):
                v=f'{mode}_{owner}'; steps=self.steps(MARCH,v)
                self.has(MARCH,v,'cast',ref=ref,value=1)
                self.has(MARCH,v,'assert_card',ref=ref,property='sick',value=1)
                self.has(MARCH,v,'choose_target',player=owner,ref=ref)
                self.has(MARCH,v,'assert_card',ref=ref,property='sick',value=0)
                self.has(MARCH,v,'attack',ref=ref,value=1)
                self.has(MARCH,v,'block',ref='blocker',other=ref,value=0)
                if ref=='ally':
                    self.assertEqual(next(c['name'] for c in self.case(MARCH,v)['setup'] if c['ref']==ref),'Elvish Mystic')

    def test_haste_expiry_actual_untap_cleanup_and_summoning_sick_tap_denial(self):
        for owner in (0,1):
            v=f'haste_expiry_{owner}'; steps=self.steps(MARCH,v)
            self.has(MARCH,v,'cast',ref='expiry_untap',value=1)
            self.has(MARCH,v,'choose_dialog',player=owner,value=1)
            cleanup=next(i for i,s in enumerate(steps) if s['op']=='cleanup')
            self.assertTrue(any(s==dict(op='assert_card',ref='ally',property='tapped',value=0) for s in steps[:cleanup]))
            self.assertEqual(self.last(MARCH,v,'assert_card',ref='ally',property='sick'),1)
            self.has(MARCH,v,'can_activate',ref='ally',value=0)
            self.assertFalse(any(s['op']=='advance_turn' for s in steps))

    def test_honest_own_target_legality_and_real_shroud(self):
        for owner in (0,1):
            self.has(MARCH,f'probe_foreign_{owner}','probe_activation_target',ref='source',other='foreign',index=-1,value=0)
            self.has(MARCH,f'probe_self_{owner}','probe_activation_target',ref='source',other='source',index=-1,value=1)
            self.has(MARCH,f'probe_shroud_{owner}','probe_activation_target',ref='source',other='ally',index=-1,value=0)
            self.assertFalse(any(s['op']=='activate' for s in self.steps(MARCH,f'probe_foreign_{owner}')))

    def test_both_controllers_real_snapshots_and_no_physical_slot_hand_error(self):
        for name,cases in self.cases.items():
            for mode in ('ready_snapshot','pending_snapshot','resolved_snapshot','declared_snapshot'):
                for owner in (0,1):
                    ops=[s['op'] for s in self.steps(name,f'{mode}_{owner}')]
                    self.assertEqual(ops.count('capture'),1); self.assertEqual(ops.count('restore'),1)
                    self.assertIn('ai_roundtrip',ops); self.assertIn('assert_recorded_state',ops)
            self.has(name,'cast_0','assert_card',ref='source',property='in_hand',value=0)

    def test_composition_snapshots_cut_after_real_cost_or_trigger_once(self):
        for owner in (0,1):
            for name,mode in [(HARBOR,'island_pending_snapshot'),(GEAR,'affinity_cast_snapshot'),
                    (MARCH,'haste_pending_snapshot'),(MARCH,'dethrone_declared_snapshot')]:
                steps=self.steps(name,f'{mode}_{owner}'); cut=next(i for i,s in enumerate(steps) if s['op']=='capture')
                self.assertFalse(any(s['op'] in ('cast','activate') for s in steps[cut:]))
                self.assertEqual(sum(s['op']=='capture' for s in steps),1)
                self.assertEqual(sum(s['op']=='restore' for s in steps),1)
                if name==GEAR:
                    self.assertIn(dict(op='assert_mana_total',player=owner,value=0),steps[:cut])
                    self.assertEqual(sum(s['op']=='cast' and s['ref']=='source' for s in steps),1)
                if mode=='haste_pending_snapshot':
                    self.assertEqual(sum(s['op']=='cast' and s['ref']=='ally' for s in steps),1)
                    self.assertEqual(self.last(name,f'{mode}_{owner}','assert_card',ref='ally',property='sick'),0)
                if mode=='dethrone_declared_snapshot':
                    self.assertEqual(sum(s['op']=='finish_attackers' for s in steps),1)
                    self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),steps[:cut])

    def test_immutable_existing_helpers_match_author_entry_pins(self):
        root=Path(__file__).resolve().parents[4]
        pins=json.loads((root/'out/wave8/next949/combat/helper-pins-before.json').read_text(encoding='utf8'))
        self.assertEqual(len(pins),10)
        for path,want in pins.items(): self.assertEqual(hashlib.sha256((root/path).read_bytes()).hexdigest(),want,path)


if __name__=='__main__': unittest.main()
