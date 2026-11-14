"""Independent complete-Aura predicates, payment, lifecycle and queue guards."""
import copy
import json
import unittest
from pathlib import Path

from generate_cases import OPS,PROPERTIES,TOKEN_PROPERTIES,registry
import entry_aura_freeze_recipes as model


class EntryAuraFreezeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,n):
        return dict(family=model.FAMILY,version=2,oracle_text=self.oracle[n]['oracle_text'],variants=['tap_0'],params=dict(schema=1,mechanism=model.mechanism(self.oracle[n])))

    def case(self,n,mode='tap',owner=0):
        d=self.declaration(n);d['variants']=[f'{mode}_{owner}'];p=model.spec(d,self.oracle[n])
        return model.expand(n,self.rows[n]['csvid'],d,p,d['variants'],self.rows,self.oracle)[0]

    def test_nine_complete_oracle_auras_expand702_valid_both_controller_cases(self):
        count=0
        for n in model.pins()['sources']:
            d=self.declaration(n);p=model.spec(d,self.oracle[n]);d['variants']=model.variants(p);cases=model.expand(n,p['csvid'],d,p,d['variants'],self.rows,self.oracle);count+=len(cases)
            self.assertEqual({int(c['_recipe']['variant'][-1]) for c in cases},{0,1})
            for c in cases:
                self.assertEqual(c['_recipe']['covered_clauses'],self.oracle[n]['oracle_text'].splitlines())
                self.assertEqual(len(c['setup']),len({x['ref'] for x in c['setup']}))
                for s in c['steps']:
                    self.assertIn(s['op'],OPS)
                    if 'property' in s:self.assertIn(s['property'],PROPERTIES | (TOKEN_PROPERTIES if s['op']=='assert_matching_permanents' else set()))
        self.assertEqual(count,702)

    def test_whole_oracle_pins_and_unknown_clauses_fail_closed(self):
        for n in model.pins()['sources']:
            d=self.declaration(n)
            for key,value in [('mana_cost','{0}'),('type_line','Enchantment'),('layout','transform'),('oracle_text',self.oracle[n]['oracle_text']+'\nDraw a card.')]:
                r=dict(self.oracle[n],**{key:value})
                with self.assertRaises(ValueError):model.spec(dict(d,oracle_text=r['oracle_text']),r)
        d=self.declaration("Runner's Bane");d['params']['mechanism']['predicate']['power_max']=True
        with self.assertRaises(ValueError):model.spec(d,self.oracle["Runner's Bane"])

    def test_persistent_duration_and_enchanted_host_wording_are_not_rewritten(self):
        for n in model.pins()['sources']:
            r=dict(self.oracle[n],oracle_text=self.oracle[n]['oracle_text'].replace("doesn't untap during its controller's untap step.","doesn't untap during its controller's next untap step."))
            with self.assertRaises(ValueError):model.mechanism(r)
            r=dict(self.oracle[n],oracle_text=self.oracle[n]['oracle_text'].replace('tap enchanted','tap target'))
            with self.assertRaises(ValueError):model.mechanism(r)

    def test_actual_costs_types_and_flash_are_independent(self):
        expected={'Sleep Paralysis':[3,0,1,0,0,0],'Mystic Restraints':[2,0,2,0,0,0],"Runner's Bane":[1,0,1,0,0,0],
                  'Encase in Ice':[1,0,1,0,0,0],'Roots':[3,0,0,1,0,0],'Malfunction':[3,0,1,0,0,0]}
        for n,cost in expected.items():
            p=model.spec(self.declaration(n),self.oracle[n]);self.assertEqual(p['mana'],cost);self.assertEqual(p['flash'],n in ('Mystic Restraints','Encase in Ice'))
            self.assertEqual(self.oracle[n]['type_line'],'Enchantment — Aura')
            s=self.case(n,'payment')['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
            for color,value in enumerate(cost):self.assertIn(dict(op='set_mana',player=0,color=color,value=value),s[:cast])

    def test_three_exact_followers_have_actual_costs_flash_and_unchanged_predicate_grammar(self):
        expected={"Castaway's Despair":([3,0,1,0,0,0],False,76),'Waterknot':([1,0,2,0,0,0],False,76),'Capture Sphere':([3,0,1,0,0,0],True,78)}
        for n,(cost,flash,count) in expected.items():
            p=model.spec(self.declaration(n),self.oracle[n]);self.assertEqual(p['mana'],cost);self.assertEqual(p['flash'],flash);self.assertEqual(len(model.variants(p)),count)
            self.assertEqual(p['predicate'],dict(types=['Creature'],power_max=None,colors=[],without_flying=False))
            self.assertEqual(p['clauses'],self.oracle[n]['oracle_text'].splitlines());self.assertEqual(self.oracle[n]['type_line'],'Enchantment — Aura')
        for owner in (0,1):
            self.assertEqual(self.case('Capture Sphere','offturn',owner)['steps'][0],dict(op='turn',player=1-owner,value=0))
            s=self.case('Waterknot','payment',owner)['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
            self.assertIn(dict(op='set_mana',player=owner,color=0,value=1),s[:cast]);self.assertIn(dict(op='set_mana',player=owner,color=2,value=2),s[:cast])

    def test_independent_type_color_power_and_flying_predicates(self):
        p={n:model.spec(self.declaration(n),self.oracle[n]) for n in model.pins()['sources']}
        for n in p:
            self.assertTrue(model.eligible(self.oracle['Elvish Mystic'],p[n]));self.assertFalse(model.eligible(self.oracle['Seal of Cleansing'],p[n]));self.assertFalse(model.eligible(self.oracle['Forest'],p[n]))
        for n in ('Ornithopter','Darksteel Relic','Darksteel Citadel'):self.assertTrue(model.eligible(self.oracle[n],p['Malfunction']))
        self.assertFalse(model.eligible(self.oracle['Darksteel Citadel'],p['Sleep Paralysis']));self.assertTrue(model.eligible(self.oracle['Dryad Arbor'],p['Sleep Paralysis']))
        self.assertTrue(model.eligible(self.oracle['Hill Giant'],p["Runner's Bane"]));self.assertFalse(model.eligible(self.oracle['Craw Wurm'],p["Runner's Bane"]))
        self.assertTrue(model.eligible(self.oracle['Goblin Piker'],p['Encase in Ice']));self.assertFalse(model.eligible(self.oracle['Merfolk of the Pearl Trident'],p['Encase in Ice']))
        self.assertTrue(model.eligible(self.oracle['Giant Spider'],p['Roots']));self.assertFalse(model.eligible(self.oracle['Ornithopter'],p['Roots']))

    def test_illegal_host_palette_uses_real_cast_denial_without_unconsumed_target_probe(self):
        for n in model.pins()['sources']:
            p=model.spec(self.declaration(n),self.oracle[n])
            for i,witness in enumerate(model.PALETTE):
                if model.eligible(self.oracle[witness],p):continue
                c=self.case(n,f'host_{i}');s=c['steps'];self.assertIn(dict(op='cast',ref='source',value=0),s)
                self.assertFalse(any(x['op'].startswith('choose_') for x in s))
                self.assertFalse(any('Creature' in self.oracle[x['name']]['type_line'] for x in c['setup'] if x['zone']=='battlefield' and x['ref']!='host'))

    def test_cast_time_target_is_single_and_precedes_cast_no_entry_target(self):
        for n in model.pins()['sources']:
            for owner in (0,1):
                s=self.case(n,owner=owner)['steps'];picks=[i for i,x in enumerate(s) if x['op']=='choose_target'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
                self.assertEqual(len(picks),1);self.assertEqual(s[picks[0]],dict(op='choose_target',player=owner,ref='host'));self.assertLess(picks[0],cast)
                self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=1),s[cast:])

    def test_countered_source_keeps_consumed_cast_target_and_no_future_entry_choices(self):
        for n in model.pins()['sources']:
            s=self.case(n,'countered')['steps'];announce=next(i for i,x in enumerate(s) if x['op']=='announce_spell');response=next(i for i,x in enumerate(s) if x['op']=='cast_response')
            self.assertLess(announce,response);self.assertEqual(sum(x['op']=='choose_target' for x in s),1)
            self.assertFalse(any(x['op'].startswith('choose_') for x in s[announce:]))
            self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=0),s[response:])
            self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows[n]['csvid'],value=1),s[response:])

    def test_pending_host_departure_response_target_order_is_actual_callback_order(self):
        for n in model.pins()['sources']:
            s=self.case(n,'host_bounced_pending')['steps'];parent=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source');announce=next(i for i,x in enumerate(s) if x['op']=='announce_spell');response=next(i for i,x in enumerate(s) if x['op']=='cast_response')
            self.assertLess(parent,announce);self.assertLess(announce,response);self.assertEqual(s[response-1],dict(op='choose_target',player=0,ref='host'))
            self.assertIn(dict(op='resolve_response'),s[response:]);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows[n]['csvid'],value=1),s[response:])

    def test_payment_and_no_legal_host_do_not_queue_choices_or_use_owned_producers(self):
        for n in model.pins()['sources']:
            for mode in ('insufficient_mana','wrong_color','no_legal_host','shroud_host'):
                for owner in (0,1):
                    c=self.case(n,mode,owner);s=c['steps'];self.assertIn(dict(op='require_can_cast',ref='source',value=int(mode in ('insufficient_mana','wrong_color'))),s)
                    self.assertIn(dict(op='cast',ref='source',value=0),s)
                    self.assertFalse(any(x['op'].startswith('choose_') for x in s))
                    self.assertFalse(any(x['player']==owner and x['zone']=='battlefield' and 'Land' in self.oracle[x['name']]['type_line'] for x in c['setup']))

    def test_real_first_and_second_host_untaps_keep_persistent_lock(self):
        for n in model.pins()['sources']:
            for owner in (0,1):
                s=self.case(n,'first_second_untap',owner)['steps'];steps=[i for i,x in enumerate(s) if x['op']=='untap_step' and x['player']==1-owner];self.assertEqual(len(steps),2)
                for i in steps:
                    self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=1),s[i:])
                    self.assertIn(dict(op='assert_card',ref='unselected',property='tapped',value=0),s[i:])
                for i,x in enumerate(s):
                    if x['op']=='untap_step':self.assertEqual(s[i-1]['op'],'advance_turn')

    def test_source_departure_does_not_immediately_untap_but_releases_next_own_step(self):
        for n in model.pins()['sources']:
            for owner in (0,1):
                for mode,leaveop in [('aura_destroyed','destroy'),('aura_bounced','cast')]:
                    s=self.case(n,mode,owner)['steps'];untap=next(i for i,x in enumerate(s) if x['op']=='untap_step' and x['player']==1-owner)
                    self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=1),s[:untap])
                    self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=0),s[untap:])
                s=self.case(n,'cleanup',owner)['steps'];cleanup=next(i for i,x in enumerate(s) if x['op']=='cleanup')
                self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=1),s[cleanup:])

    def test_real_manual_untap_retap_dialog_order_and_lock_persistence(self):
        for n in model.pins()['sources']:
            for owner in (0,1):
                s=self.case(n,'manual_untap',owner)['steps']
                self.assertIn(dict(op='present_ai_dialogs',value=1),s)
                for ref,value in [('untapper',1),('retapper',0)]:
                    cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']==ref)
                    self.assertEqual(s[cast-1],dict(op='choose_target',player=owner,ref='host'));self.assertIn(dict(op='choose_dialog',player=owner,value=value),s[cast:cast+5])
                untap=next(i for i,x in enumerate(s) if x['op']=='untap_step' and x['player']==1-owner)
                self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=1),s[untap:])

    def test_no_untap_is_not_a_blanket_activation_or_attack_prohibition(self):
        for n in model.pins()['sources']:
            for owner in (0,1):
                s=self.case(n,'mana_function',owner)['steps'];act=next(i for i,x in enumerate(s) if x['op']=='activate')
                self.assertEqual(s[act]['ref'],'host');self.assertIn(dict(op='require_can_activate',ref='host',value=1),s[:act])
                self.assertIn(dict(op='assert_mana',player=1-owner,color=3,value=1),s[act:])
                s=self.case(n,'attack_after_untap',owner)['steps'];attack=next(i for i,x in enumerate(s) if x['op']=='attack')
                self.assertEqual(s[attack],dict(op='attack',ref='host',value=1));self.assertTrue(any(x['op']=='untap_step' for x in s[:attack]))
                advance=next(i for i,x in enumerate(s) if x['op']=='advance_combat');damage=next(i for i,x in enumerate(s) if x['op']=='combat_damage');end=next(i for i,x in enumerate(s) if x['op']=='end_combat')
                self.assertLess(advance,damage);self.assertLess(damage,end)

    def test_produced_green_is_receipted_then_paid_normally_on_real_fog_before_untap(self):
        for n in model.pins()['sources']:
            for owner in (0,1):
                c=self.case(n,'mana_function',owner);s=c['steps'];spender=next(x for x in c['setup'] if x['ref']=='mana_spender')
                self.assertEqual(spender['name'],'Fog');self.assertEqual(spender['player'],1-owner);self.assertEqual(spender['zone'],'hand')
                produced=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='host')
                paid=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='mana_spender');advance=next(i for i,x in enumerate(s) if x['op']=='advance_turn')
                self.assertLess(produced,paid);self.assertLess(paid,advance)
                self.assertIn(dict(op='assert_mana',player=1-owner,color=3,value=1),s[produced:paid])
                self.assertIn(dict(op='assert_mana',player=1-owner,color=3,value=0),s[paid:advance])
                self.assertFalse(any(x['op']=='set_mana' for x in s[produced:advance]))
                self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows['Fog']['csvid'],value=1),s[paid:advance])

    def test_pending_color_loss_uses_pinned_creature_only_frog_with_complete_receipts(self):
        for owner in (0,1):
            c=self.case('Encase in Ice','pending_illegal',owner);s=c['steps'];self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='modifier'),'Turn to Frog')
            cast=next(i for i,x in enumerate(s) if x['op']=='cast_response');announce=next(i for i,x in enumerate(s) if x['op']=='announce_spell')
            self.assertLess(announce,cast);self.assertEqual(s[cast-1],dict(op='choose_target',player=owner,ref='host'))
            self.assertIn(dict(op='set_mana',player=owner,color=0,value=1),s[announce:cast]);self.assertIn(dict(op='set_mana',player=owner,color=2,value=1),s[announce:cast])
            for prop,value in [('color',4),('subtype_Frog',1),('subtype_Elf',0)]:
                self.assertIn(dict(op='assert_matching_permanents',player=1-owner,index=self.rows['Elvish Mystic']['csvid'],property=prop,value=value),s[cast:])
            self.assertIn(dict(op='require_can_activate',ref='host',value=0),s[cast:])
            self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=0),s[cast:])
            for prop in ('power','toughness'):self.assertIn(dict(op='assert_card',ref='host',property=prop,value=1),s[cast:])
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=14910,value=1),s[cast:])
            # The successful normal color-loss control remains the original
            # permanent-only Purelace path, with no pending spell to steal it.
            self.assertEqual(next(x['name'] for x in self.case('Encase in Ice','color_changes',owner)['setup'] if x['ref']=='modifier'),'Purelace')

    def test_continuous_enchant_legality_uses_real_effects_and_state_based_aura_departure(self):
        for n,mode,spell in [("Runner's Bane",'power_grows','Giant Growth'),('Roots','gains_flying','Jump'),('Encase in Ice','color_changes','Purelace')]:
            for owner in (0,1):
                c=self.case(n,mode,owner);s=c['steps'];self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='modifier'),spell)
                mod=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='modifier')
                self.assertFalse(any(x['op']=='destroy' and x['ref']=='source' for x in s))
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[n]['csvid'],value=1),s[mod:])
                untap=next(i for i,x in enumerate(s) if x['op']=='untap_step' and x['player']==1-owner)
                self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=0),s[untap:])
                if n=="Runner's Bane":self.assertIn(dict(op='assert_card',ref='host',property='power',value=4),s[mod:untap])

    def test_foreign_owned_host_return_and_aura_owner_are_distinct(self):
        for n in model.pins()['sources']:
            for owner in (0,1):
                c=self.case(n,'foreign_owned_host',owner);host=next(x for x in c['setup'] if x['ref']=='host')
                self.assertEqual(host['player'],owner);self.assertIn('opponent_owned',host['state'])
                s=c['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='hand',index=self.rows['Elvish Mystic']['csvid'],value=1),s)
                self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[n]['csvid'],value=1),s)

    def test_real_aura_bounce_recast_returns_to_main1_and_binds_same_source(self):
        for n in model.pins()['sources']:
            for owner in (0,1):
                s=self.case(n,'bounce_recast',owner)['steps'];casts=[i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source'];self.assertEqual(len(casts),2)
                bind=next(i for i,x in enumerate(s) if x['op']=='bind_hand_copy');self.assertLess(casts[0],bind);self.assertLess(bind,casts[1])
                self.assertTrue(any(x==dict(op='turn',player=owner,value=0) for x in s[bind:casts[1]]))
                self.assertIn(dict(op='choose_target',player=owner,ref='host'),s[bind:casts[1]])

    def test_flash_is_actual_opposing_turn_and_regular_entries_stay_own_turn(self):
        for n in ('Mystic Restraints','Encase in Ice'):
            for owner in (0,1):self.assertEqual(self.case(n,'offturn',owner)['steps'][0],dict(op='turn',player=1-owner,value=0))
        for n in ('Sleep Paralysis',"Runner's Bane",'Roots','Malfunction'):
            p=model.spec(self.declaration(n),self.oracle[n]);self.assertFalse(any(x.startswith('offturn_') for x in model.variants(p)))

    def test_nonmatching_entry_happens_after_actual_first_attachment(self):
        for n in model.pins()['sources']:
            s=self.case(n,'nonmatching_entry')['steps'];first=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source');other=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='nonmatching')
            self.assertLess(first,other);self.assertEqual(sum(x['op']=='choose_target' for x in s),1)
            self.assertIn(dict(op='assert_card',ref='host',property='tapped',value=1),s[other:])

    def test_replay_tails_and_unobservable_priority_attachment_gaps_remain_explicit(self):
        for n in model.pins()['sources']:
            for mode in ('ready_snapshot','pending_snapshot','attached_snapshot'):
                c=self.case(n,mode);s=c['steps'];capture=next(i for i,x in enumerate(s) if x['op']=='capture');record=next(i for i,x in enumerate(s) if x['op']=='record_state');restore=next(i for i,x in enumerate(s) if x['op']=='restore')
                self.assertEqual(s[capture+1:record],s[restore+1:-1]);self.assertEqual(s[-1]['op'],'assert_recorded_state')
                self.assertTrue(any('no exposed priority' in g for g in c['_recipe']['untested_components']))
                self.assertTrue(any('no generic attachment' in g for g in c['_recipe']['untested_components']))


if __name__=='__main__':unittest.main()
