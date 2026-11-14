"""Independent countdown timing, whole-clause, payment and lifecycle guards."""
import copy,json,unittest
from pathlib import Path
from generate_cases import OPS,PROPERTIES,registry
import countdown_counter_lifecycle_recipes as m

class CountdownLifecycleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
    def declaration(self,n):return dict(family=m.FAMILY,version=2,oracle_text=self.o[n]['oracle_text'],params=dict(schema=1,mechanism=m.mechanism(self.o[n])))
    def case(self,n,mode='value',owner=0):
        d=self.declaration(n);d['variants']=[f'{mode}_{owner}'];p=m.spec(d,self.o[n]);return m.expand(n,p['csvid'],d,p,d['variants'],self.rows,self.o)[0]
    def test_independent_whole_mechanisms(self):
        expected={'Blastoderm':('fading',3,['Shroud'],False,'none','none',0),'Skyshroud Behemoth':('fading',2,[],True,'none','none',0),'Calciderm':('vanishing',4,['Shroud'],False,'none','none',0),'Aven Riftwatcher':('vanishing',3,['Flying'],False,'life','none',0),'Keldon Marauders':('vanishing',2,[],False,'damage','none',0),'Ancient Hydra':('fading',5,[],False,'none','damage',1),'Jolting Merfolk':('fading',4,[],False,'none','tap',0),'Woodripper':('fading',3,[],False,'none','destroy_artifact',1),'Cloudskate':('fading',3,['Flying'],False,'none','none',0),'Skyshroud Ridgeback':('fading',2,[],False,'none','none',0)}
        self.assertEqual(set(expected),set(m.pins()['sources']))
        for n,v in expected.items():
            p=m.spec(self.declaration(n),self.o[n]);self.assertEqual(tuple(p[k] for k in ('kind','initial','keywords','entry_tapped','edge','activation','generic')),v)
    def test_exact_followers_retain_independent_PT_Flying_entry_and_real_costs(self):
        cloud=m.spec(self.declaration('Cloudskate'),self.o['Cloudskate']);ridge=m.spec(self.declaration('Skyshroud Ridgeback'),self.o['Skyshroud Ridgeback'])
        self.assertEqual((cloud['power'],cloud['toughness'],cloud['keywords'],cloud['entry_tapped'],cloud['mana']),(2,2,['Flying'],False,[1,0,1,0,0,0]))
        self.assertEqual((ridge['power'],ridge['toughness'],ridge['keywords'],ridge['entry_tapped'],ridge['mana']),(2,3,[],False,[0,0,0,1,0,0]))
        self.assertEqual((len(m.variants(cloud)),len(m.variants(ridge))),(50,46));self.assertNotIn('Shroud',cloud['keywords'])
        for owner in (0,1):
            s=self.case('Cloudskate','flying_reach',owner)['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Cloudskate']['csvid'],value=1),s)
            self.assertIn(dict(op='assert_card',ref='blocker',property='damage',value=2),s)
            s=self.case('Skyshroud Ridgeback','value',owner)['steps'];self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),s)

    def test_full_type_cost_text_and_typed_profile_pins(self):
        for n in m.pins()['sources']:
            for key,v in [('mana_cost','{0}'),('type_line','Artifact'),('layout','transform'),('oracle_text',self.o[n]['oracle_text']+'\nDraw a card.')]:
                r=dict(self.o[n],**{key:v});d=self.declaration(n);d['oracle_text']=r['oracle_text']
                with self.assertRaises(ValueError):m.spec(d,r)
            d=self.declaration(n);d['params']['mechanism']['initial']=True
            with self.assertRaises(ValueError):m.spec(d,self.o[n])
    def test_primaries_are_implemented_and_static_hosts_honest(self):
        m.validate_witnesses(self.o,self.rows)
        self.assertEqual(set(m.pins()['honest_static_only_dummy_witnesses']),{'Hill Giant','Horned Turtle','Giant Spider','Ornithopter'})
        for n in m.pins()['sources']:self.assertNotIn(self.rows[n]['handler'],('card_dummy','NULL',None))
    def test_all520_cases_have_full_clauses_both_controllers_and_valid_DSL(self):
        count=0
        for n in m.pins()['sources']:
            d=self.declaration(n);p=m.spec(d,self.o[n]);vs=m.variants(p);self.assertEqual({v[-1] for v in vs},{'0','1'});cases=m.expand(n,p['csvid'],d,p,vs,self.rows,self.o);count+=len(cases)
            for c in cases:
                self.assertEqual(c['_recipe']['covered_clauses'],self.o[n]['oracle_text'].splitlines());self.assertEqual(len(c['setup']),len({e['ref'] for e in c['setup']}))
                for s in c['steps']:
                    self.assertIn(s['op'],OPS)
                    if 'property' in s:self.assertIn(s['property'],PROPERTIES)
                    self.assertNotIn(s['op'],('set_counter','set_counters','add_counter','remove_counter'))
        self.assertEqual(count,520)
    def test_exact_real_cast_costs_and_both_fixture_pools_zero(self):
        expected={'Blastoderm':[2,0,0,2,0,0],'Skyshroud Behemoth':[5,0,0,2,0,0],'Calciderm':[2,0,0,0,0,2],'Aven Riftwatcher':[2,0,0,0,0,1],'Keldon Marauders':[1,0,0,0,1,0],'Ancient Hydra':[4,0,0,0,1,0],'Jolting Merfolk':[2,0,2,0,0,0],'Woodripper':[3,0,0,2,0,0],'Cloudskate':[1,0,1,0,0,0],'Skyshroud Ridgeback':[0,0,0,1,0,0]}
        for n,cost in expected.items():
            self.assertEqual(m.spec(self.declaration(n),self.o[n])['mana'],cost)
            for owner in (0,1):
                s=self.case(n,'payment',owner)['steps'];i=next(i for i,x in enumerate(s) if x['op']=='cast')
                for pl in (0,1):
                    for color in range(6):self.assertIn(dict(op='set_mana',player=pl,color=color,value=0),s[:i])
                self.assertFalse(any(x['op']=='activate' for x in s))
    def test_unaffordable_casts_have_no_live_mana_escape_or_target_intent(self):
        for n in m.pins()['sources']:
            for mode in ('insufficient_mana','wrong_color'):
                c=self.case(n,mode);self.assertFalse(any(x['zone']=='battlefield' for x in c['setup']));s=c['steps'];self.assertIn(dict(op='cast',ref='source',value=0),s);self.assertFalse(any(x['op'].startswith('choose_') for x in s))
    def test_fading_zero_counter_stays_alive_through_opponent_then_failed_own_removal(self):
        for n,k in [('Blastoderm',3),('Skyshroud Behemoth',2),('Ancient Hydra',5),('Jolting Merfolk',4),('Woodripper',3),('Cloudskate',3),('Skyshroud Ridgeback',2)]:
            for owner in (0,1):
                s=self.case(n,'lifecycle',owner)['steps'];reads=[x['value'] for x in s if x['op']=='assert_counter' and x['ref']=='source'];expected=[k]
                for remaining in range(k,-1,-1):
                    expected.append(remaining)
                    if remaining:expected.append(remaining-1)
                self.assertEqual(reads,expected)
                self.assertEqual(sum(x['op']=='upkeep_step' and x['player']==owner for x in s),k+1);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[n]['csvid'],value=1),s)
                zero=self.case(n,'zero_boundary',owner)['steps'];self.assertEqual(sum(x['op']=='upkeep_step' and x['player']==owner for x in zero),k);self.assertIn(dict(op='assert_counter',ref='source',counter_kind='fade',value=0),zero);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows[n]['csvid'],value=1),zero)
    def test_vanishing_last_removal_sacrifices_on_Nth_not_Nplus1(self):
        for n,k in [('Calciderm',4),('Aven Riftwatcher',3),('Keldon Marauders',2)]:
            for owner in (0,1):
                s=self.case(n,'lifecycle',owner)['steps'];reads=[x['value'] for x in s if x['op']=='assert_counter' and x['ref']=='source'];expected=[k]
                for remaining in range(k,0,-1):
                    expected.append(remaining)
                    if remaining>1:expected.append(remaining-1)
                self.assertEqual(reads,expected);self.assertEqual(sum(x['op']=='upkeep_step' and x['player']==owner for x in s),k);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[n]['csvid'],value=1),s)
    def test_opponent_upkeep_does_not_debit_source(self):
        for n in m.pins()['sources']:
            p=m.spec(self.declaration(n),self.o[n]);s=self.case(n,'opponent_upkeep')['steps'];self.assertEqual([x['player'] for x in s if x['op']=='upkeep_step'],[1]);self.assertEqual([x['value'] for x in s if x['op']=='assert_counter' and x['ref']=='source'],[p['initial'],p['initial']])
    def test_real_upkeep_provenance_always_advances_then_untaps(self):
        for n in m.pins()['sources']:
            for mode in ('lifecycle','control_transfer') if 'Shroud' not in self.o[n]['keywords'] else ('lifecycle',):
                s=self.case(n,mode)['steps']
                for i,x in enumerate(s):
                    if x['op']=='upkeep_step':
                        untap=max(j for j in range(i) if s[j]['op']=='untap_step');self.assertEqual(s[untap]['player'],x['player']);self.assertEqual(s[untap-1]['op'],'advance_turn')
    def test_Snap_zero_uses_real_nontargeted_spell_and_distinct_timing(self):
        for n in m.pins()['sources']:
            s=self.case(n,'snap_zero')['steps'];snap=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='snap');self.assertFalse(any(x['op']=='choose_target' and x.get('ref')=='source' for x in s));self.assertIn(dict(op='cast',ref='snap',value=1),s)
            if 'Fading' in self.o[n]['keywords']:self.assertIn(dict(op='assert_counter',ref='source',counter_kind='fade',value=0),s[snap:]);self.assertEqual(sum(x['op']=='upkeep_step' and x['player']==0 for x in s),1)
            else:self.assertFalse(any(x['op']=='upkeep_step' for x in s));self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows[n]['csvid'],value=1),s[snap:])
    def test_entry_tapped_is_printed_only_and_cleared_by_real_untap(self):
        for n in m.pins()['sources']:
            s=self.case(n,'value')['steps'];self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=int(n=='Skyshroud Behemoth')),s)
        s=self.case('Skyshroud Behemoth','first_upkeep')['steps'];first=next(i for i,x in enumerate(s) if x['op']=='untap_step' and x['player']==0);self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),s[first:])
    def test_shroud_requires_specific_native_target_denial_not_proxy(self):
        for n in ('Blastoderm','Calciderm'):
            for mode in ('shroud_own','shroud_opponent'):
                s=self.case(n,mode)['steps'];self.assertIn(dict(op='can_target_creature',ref='probe',other='source',value=0),s);self.assertEqual(sum(x['op']=='cast' for x in s),1)
    def test_Aven_entry_and_real_exit_gain_life_once_each(self):
        for owner in (0,1):
            for mode in ('lifecycle','source_destroyed','source_bounced'):
                s=self.case('Aven Riftwatcher',mode,owner)['steps'];self.assertIn(dict(op='assert_life',player=owner,value=22),s);self.assertIn(dict(op='assert_life',player=owner,value=24),s)
            s=self.case('Aven Riftwatcher','control_transfer',owner)['steps'];self.assertIn(dict(op='assert_life',player=owner,value=22),s);self.assertIn(dict(op='assert_life',player=1-owner,value=22),s)
    def test_Keldon_targets_actual_player_and_initialized_planeswalker(self):
        for owner in (0,1):
            s=self.case('Keldon Marauders','lifecycle',owner)['steps'];self.assertEqual(sum(x==dict(op='choose_player',player=owner,value=1-owner) for x in s),2);self.assertIn(dict(op='assert_life',player=1-owner,value=18),s)
            s=self.case('Keldon Marauders','planeswalker',owner)['steps'];self.assertEqual(s[next(i for i,x in enumerate(s) if x['op']=='cast')]['ref'],'walker');self.assertEqual(sum(x==dict(op='choose_target',player=owner,ref='walker') for x in s),2);self.assertIn(dict(op='assert_counter',ref='walker',counter_kind='loyalty',value=1),s)
    def test_countered_sources_queue_no_unoffered_target_or_entry_effect(self):
        for n in m.pins()['sources']:
            for owner in (0,1):
                s=self.case(n,'countered',owner)['steps'];a=next(i for i,x in enumerate(s) if x['op']=='cast_response');self.assertEqual(s[a-1],dict(op='announce_spell',ref='source'));self.assertFalse(any(x['op'].startswith('choose_') for x in s));self.assertFalse(any(x['op']=='assert_counter' for x in s));self.assertIn(dict(op='assert_life',player=owner,value=20),s)
    def test_fade_cost_is_observed_before_effect_resolution(self):
        for n,k,fee in [('Ancient Hydra',5,1),('Jolting Merfolk',4,0),('Woodripper',3,1)]:
            for owner in (0,1):
                s=self.case(n,'ability_fresh',owner)['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');r=next(i for i in range(a,len(s)) if s[i]['op']=='resolve');self.assertIn(dict(op='assert_counter',ref='source',counter_kind='fade',value=k-1),s[a:r]);self.assertFalse(any(x['op']=='advance_turn' for x in s[:a]));self.assertIn(dict(op='activate',ref='source',value=1),s)
    def test_depletion_preserves_Fading_zero_then_next_upkeep(self):
        for n,k in [('Ancient Hydra',5),('Jolting Merfolk',4),('Woodripper',3)]:
            s=self.case(n,'deplete')['steps'];self.assertEqual(sum(x['op']=='activate' and x['value']==1 for x in s),k);self.assertIn(dict(op='activate',ref='source',value=0),s);self.assertIn(dict(op='assert_counter',ref='source',counter_kind='fade',value=0),s);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows[n]['csvid'],value=1),s)
            if n!='Jolting Merfolk':
                spender=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='zero_spender');upkeep=next(i for i,x in enumerate(s) if x['op']=='upkeep_step');self.assertLess(spender,upkeep)
    def test_printed_effects_damage_tap_and_artifact_destruction_are_distinct(self):
        s=self.case('Ancient Hydra','ability')['steps'];self.assertIn(dict(op='assert_life',player=1,value=19),s)
        s=self.case('Jolting Merfolk','ability')['steps'];self.assertIn(dict(op='assert_card',ref='target_0',property='tapped',value=1),s)
        s=self.case('Woodripper','ability')['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Ornithopter']['csvid'],value=1),s)
    def test_removal_of_source_does_not_remove_its_pending_ability(self):
        for n in ('Ancient Hydra','Jolting Merfolk','Woodripper'):
            s=self.case(n,'ability_source_destroyed')['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');d=next(i for i,x in enumerate(s) if x['op']=='destroy');self.assertLess(a,d);self.assertEqual(sum(x['op']=='activate' for x in s),1)
    def test_real_control_changes_upkeep_controller_and_original_owner_grave(self):
        for n in ('Aven Riftwatcher','Keldon Marauders','Ancient Hydra'):
            for owner in (0,1):
                s=self.case(n,'control_transfer',owner)['steps'];self.assertIn(dict(op='cast',ref='control',value=1),s);self.assertIn(dict(op='bind_permanent_copy',player=1-owner,value=self.rows[n]['csvid'],index=0,ref='source'),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[n]['csvid'],value=1),s)
    def test_recast_uses_real_new_entry_and_reinitialized_count(self):
        for n in ('Aven Riftwatcher','Ancient Hydra','Skyshroud Behemoth'):
            s=self.case(n,'bounce_recast')['steps'];self.assertEqual(sum(x['op']=='cast' and x['ref']=='source' for x in s),2);self.assertIn(dict(op='bind_hand_copy',player=0,value=self.rows[n]['csvid'],index=0,ref='source'),s)
    def test_combat_paths_always_complete_declaration_damage_end(self):
        for n in m.pins()['sources']:
            modes=['attack','block']+(['flying_evasion','flying_reach'] if 'Flying' in self.o[n]['keywords'] else [])+(['ability_tapped'] if m.mechanism(self.o[n])['activation']!='none' else [])
            for mode in modes:
                s=self.case(n,mode)['steps'];damage=next(i for i,x in enumerate(s) if x['op']=='combat_damage');advance=max(i for i in range(damage) if s[i]['op']=='advance_combat');end=next(i for i,x in enumerate(s) if x['op']=='end_combat');self.assertLess(advance,damage);self.assertLess(damage,end)
    def test_Flying_uses_real_ground_and_Reach_blocking(self):
        for mode,legal in [('flying_evasion',0),('flying_reach',1)]:
            s=self.case('Aven Riftwatcher',mode)['steps'];self.assertIn(dict(op='block',ref='blocker',other='source',value=legal),s)
            if legal:self.assertIn(dict(op='assert_card',ref='source',property='damage',value=2),s);self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),s)
    def test_snapshots_replay_actual_countdown_actions_and_observed_state(self):
        for n in m.pins()['sources']:
            for mode in ('ready_snapshot','pending_snapshot','partial_snapshot','expiry_snapshot'):
                s=self.case(n,mode)['steps'];self.assertEqual(sum(x['op']=='capture' for x in s),1);self.assertEqual(sum(x['op']=='restore' for x in s),1);self.assertEqual(s[-1],dict(op='assert_recorded_state'))

if __name__=='__main__':unittest.main()
