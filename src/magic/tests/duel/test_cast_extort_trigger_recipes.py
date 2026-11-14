import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import cast_extort_trigger_recipes as f

class CastExtortTriggerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
    def declaration(self,n,v='effect_0'):
        return dict(family=f.FAMILY,version=2,oracle_text=self.o[n]['oracle_text'],params=f.parameters(f.classify(self.o[n])),variants=[v])
    def case(self,n='Syndic of Tithes',v='effect_0'):
        d=self.declaration(n,v);return f.expand(n,self.rows[n]['csvid'],d,f.spec(d,self.o[n]),[v],self.rows,self.o)[0]
    def final(self,c,op,**kw):return next(s['value'] for s in reversed(c['steps']) if s['op']==op and all(s.get(k)==v for k,v in kw.items()))
    def life(self,c,w):return self.final(c,'assert_life',player=w)
    def test_full_oracle_stats_keywords_costs_and_ui_are_pinned(self):
        f.validate_witnesses(self.o,self.rows)
        for n in f.pins()['sources']:
            d=self.declaration(n)
            for field,value in [('scope','any'),('fee','{1}'),('keywords',[]),('unknown',1)]:
                changed=copy.deepcopy(d);changed['params'][field]=value
                if f.exact(changed,d):continue
                with self.assertRaises(ValueError):f.spec(changed,self.o[n])
            for field,value in [('mana_cost','{0}'),('power','0'),('layout','transform'),('oracle_text',self.o[n]['oracle_text']+'\nDraw a card.')]:
                r=dict(self.o[n]);r[field]=value
                with self.assertRaises(ValueError):f.spec(d,r)
            r=dict(self.o[n]);r['oracle_text']+='\n{T}: Add {W}.'
            with self.assertRaises(ValueError):f.classify(r)
        rows=copy.deepcopy(self.rows);rows['Syndic of Tithes']['handler']='different'
        with self.assertRaises(ValueError):f.validate_witnesses(self.o,rows)
        self.assertEqual((f.classify(self.o['Basilica Guards'])['power'],f.classify(self.o['Basilica Guards'])['toughness']),(1,4))
    def test_optional_hybrid_fee_is_exact_and_not_printed_source_cost(self):
        for n in f.pins()['sources']:
            for owner in (0,1):
                for mode,color in [('fee_white',5),('fee_black',1)]:
                    c=self.case(n,f'{mode}_{owner}');self.assertEqual((self.life(c,owner),self.life(c,1-owner)),(21,19))
                    self.assertEqual(self.final(c,'assert_mana_total',player=owner),0)
                    self.assertFalse(any(s['op']=='choose_payment' for s in c['steps']))
                    if owner==0:self.assertIn(dict(op='choose_trigger_source',player=0,ref='source'),c['steps'])
    def test_human_both_color_choices_name_actual_payment_vectors(self):
        for mode,paid,left in [('both_pay_white',5,1),('both_pay_black',1,5)]:
            c=self.case(v=mode+'_0');self.assertIn(dict(op='choose_payment',player=0,value=1<<(4*paid)),c['steps'])
            self.assertEqual(self.final(c,'assert_mana',player=0,color=paid),0);self.assertEqual(self.final(c,'assert_mana',player=0,color=left),1)
            self.assertEqual((self.life(c,0),self.life(c,1)),(21,19))
    def test_payment_cancel_is_actual_hybrid_dialog_and_not_mana_cancel(self):
        c=self.case(v='payment_cancel_0');steps=c['steps'];accept=next(i for i,s in enumerate(steps) if s['op']=='choose_trigger_source')
        self.assertEqual(steps[accept+1],dict(op='choose_dialog',player=0,value=0))
        self.assertFalse(any(s['op']=='choose_cancel_mana' for s in steps));self.assertEqual((self.life(c,0),self.life(c,1)),(20,20))
        for color in (1,5):self.assertEqual(self.final(c,'assert_mana',player=0,color=color),1)
        self.assertEqual(self.final(c,'assert_card',ref='spell_0',property='in_play'),1)
    def test_optional_outer_decline_has_no_payment_or_effect(self):
        c=self.case(v='decline_0');self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
        self.assertEqual((self.life(c,0),self.life(c,1)),(20,20))
        self.assertEqual(self.final(c,'assert_mana',player=0,color=5),30)
    def test_generic_wrong_color_and_zero_budget_cannot_pay_hybrid(self):
        for owner in (0,1):
            for mode in ('fee_unaffordable','fee_wrong_color','witness_payment'):
                c=self.case(v=f'{mode}_{owner}');self.assertEqual((self.life(c,owner),self.life(c,1-owner)),(20,20))
                self.assertFalse(any(s['op'] in ('choose_payment','choose_dialog','choose_cancel_mana') for s in c['steps']))
                for color in (1,5):self.assertEqual(self.final(c,'assert_mana',player=owner,color=color),0)
    def test_real_spell_types_draw_receipts_and_opposing_negative(self):
        for owner in (0,1):
            for mode,draw in [('effect',0),('spell_creature',0),('spell_enchantment',0),('spell_instant',1),('spell_sorcery',2)]:
                c=self.case(v=f'{mode}_{owner}');self.assertEqual((self.life(c,owner),self.life(c,1-owner)),(21,19))
                before=sum(x['zone']=='library' and x['player']==owner for x in c['setup'])
                self.assertEqual(self.final(c,'assert_zone',player=owner,zone='library'),before-draw)
                self.assertEqual(sum(s['op']=='cast' for s in c['steps']),1);self.assertEqual(sum(s['op']=='announce_spell' for s in c['steps']),1)
            c=self.case(v=f'opponent_cast_{owner}');self.assertEqual((self.life(c,0),self.life(c,1)),(20,20));self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
    def test_countered_spell_earns_exactly_one_real_cast_trigger(self):
        for owner in (0,1):
            c=self.case(v=f'countered_witness_{owner}');s=c['steps'];self.assertEqual((self.life(c,owner),self.life(c,1-owner)),(21,19))
            self.assertLess(next(i for i,x in enumerate(s) if x['op']=='announce_spell'),next(i for i,x in enumerate(s) if x['op']=='cast_response'))
            self.assertIn(dict(op='resolve_response'),s)
            witness='Sol Ring';before=sum(x['name']==witness and x['zone']=='graveyard' and x['player']==owner for x in c['setup'])
            self.assertEqual(self.final(c,'assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[witness]['csvid']),before+1)
    def test_two_sources_and_limited_budgets_pay_per_instance(self):
        for owner in (0,1):
            for mode,delta in [('two_sources',2),('two_sources_budget_one',1),('two_sources_budget_two',2)]:
                c=self.case(v=f'{mode}_{owner}');self.assertEqual((self.life(c,owner),self.life(c,1-owner)),(20+delta,20-delta))
                if 'budget' in mode:self.assertEqual(self.final(c,'assert_mana_total',player=owner),0)
            c=self.case(v=f'repeat_same_turn_{owner}');self.assertEqual((self.life(c,owner),self.life(c,1-owner)),(23,17))
            self.assertEqual(sum(s['op']=='announce_spell' for s in c['steps']),3)
        c=self.case(v='two_sources_budget_one_0');self.assertEqual(sum(s['op']=='choose_trigger_source' for s in c['steps']),1)
        c=self.case(v='two_sources_0');self.assertEqual([s['ref'] for s in c['steps'] if s['op']=='choose_trigger_source'],['source','copy'])
    def test_mixed_sources_trigger_only_for_their_own_caster(self):
        for owner in (0,1):
            c=self.case(v=f'mixed_sources_{owner}');self.assertEqual((self.life(c,0),self.life(c,1)),(20,20))
            self.assertEqual(sum(s['op']=='announce_spell' for s in c['steps']),2)
            self.assertEqual(sum(s['op']=='choose_trigger_source' for s in c['steps']),1)
    def test_source_cost_and_real_producer_denials_are_independent(self):
        for n in f.pins()['sources']:
            for owner in (0,1):
                c=self.case(n,f'source_payment_{owner}');self.assertIn(dict(op='assert_mana_total',player=owner,value=0),c['steps'])
                self.assertEqual((self.life(c,owner),self.life(c,1-owner)),(21,19))
                for mode in ('source_insufficient_mana','source_wrong_color','witness_insufficient_mana','witness_wrong_color'):
                    c=self.case(n,f'{mode}_{owner}');self.assertFalse(any(s['op'].startswith('choose_') or s['op']=='announce_spell' for s in c['steps']))
                    self.assertEqual(next(x for x in c['setup'] if x['ref']=='control_ring')['state'],['tapped'])
    def test_source_absence_late_entry_and_destruction_keep_zone_receipts(self):
        for n in f.pins()['sources']:
            for mode in ('source_removed_before','source_arrives_late'):
                c=self.case(n,mode+'_0');self.assertEqual((self.life(c,0),self.life(c,1)),(20,20));self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
            c=self.case(n,'source_removed_before_0');self.assertFalse(any(s['op']=='assert_card' and s.get('ref')=='source' for s in c['steps']))
            c=self.case(n,'source_removed_after_announcement_0');self.assertEqual((self.life(c,0),self.life(c,1)),(21,19))
    def test_defender_full_printed_one_power_and_actual_attack_rejection(self):
        c=self.case('Basilica Guards','combat_clause_1');self.assertIn(dict(op='attack',ref='source',value=0),c['steps'])
        self.assertEqual(self.final(c,'assert_card',ref='source',property='power'),1);self.assertEqual((self.life(c,1),self.life(c,0)),(21,19))
    def test_flying_evasion_reach_and_flying_block_are_actual_combat_gates(self):
        for owner in (0,1):
            c=self.case('Basilica Screecher',f'combat_clause_{owner}');self.assertIn(dict(op='block',ref='blocker',other='source',value=0),c['steps'])
            self.assertEqual((self.life(c,owner),self.life(c,1-owner)),(21,18))
            c=self.case('Basilica Screecher',f'flying_reach_{owner}');self.assertIn(dict(op='block',ref='blocker',other='source',value=1),c['steps'])
            c=self.case('Basilica Screecher',f'block_flyer_{owner}');self.assertIn(dict(op='block',ref='source',other='attacker',value=1),c['steps'])
    def test_vigilance_attack_and_lifelink_damage_are_separate_from_extort(self):
        for owner in (0,1):
            c=self.case('Knight of Obligation',f'combat_clause_{owner}');self.assertEqual(self.final(c,'assert_card',ref='source',property='tapped'),0)
            self.assertEqual((self.life(c,owner),self.life(c,1-owner)),(21,17))
            c=self.case('Tithe Drinker',f'combat_clause_{owner}');self.assertEqual((self.life(c,owner),self.life(c,1-owner)),(23,17))
            c=self.case('Tithe Drinker',f'effect_{owner}');self.assertEqual((self.life(c,owner),self.life(c,1-owner)),(21,19))
    def test_legal_flying_blocks_complete_real_lethal_damage_before_end_combat(self):
        for owner in (0,1):
            for mode in ('block_flyer','flying_reach'):
                c=self.case('Basilica Screecher',f'{mode}_{owner}');s=c['steps']
                advance=next(i for i,x in enumerate(s) if x['op']=='advance_combat')
                self.assertEqual([x['op'] for x in s[advance:advance+3]],['advance_combat','combat_damage','end_combat'])
                self.assertEqual((self.life(c,owner),self.life(c,1-owner)),(21,19))
                dead,who,live=('Suntail Hawk',1-owner,'source') if mode=='block_flyer' else ('Basilica Screecher',owner,'blocker')
                before=sum(x['name']==dead and x['zone']=='graveyard' and x['player']==who for x in c['setup'])
                self.assertEqual(self.final(c,'assert_zone_multiplicity',player=who,zone='graveyard',index=self.rows[dead]['csvid']),before+1)
                self.assertEqual(self.final(c,'assert_card',ref=live,property='damage'),1)
                self.assertEqual(self.final(c,'assert_card',ref=live,property='in_play'),1)
                end=next(i for i,x in enumerate(s) if x['op']=='end_combat')
                self.assertFalse(any(x['op']=='assert_card' and x.get('ref')==('attacker' if mode=='block_flyer' else 'source') for x in s[end+1:]))
    def test_fresh_source_attack_is_not_fabricated_ready(self):
        for n in f.pins()['sources']:
            c=self.case(n,'fresh_attack_0');self.assertEqual(next(x['zone'] for x in c['setup'] if x['ref']=='source'),'hand')
            self.assertIn(dict(op='cast',ref='source',value=1),c['steps']);self.assertIn(dict(op='attack',ref='source',value=0),c['steps'])
    def test_cleanup_and_real_turn_preserve_paid_life_changes(self):
        for owner in (0,1):
            c=self.case(v=f'cleanup_{owner}');self.assertEqual((self.life(c,owner),self.life(c,1-owner)),(21,19))
            self.assertEqual(self.final(c,'assert_mana_total',player=owner),0);i=next(i for i,s in enumerate(c['steps']) if s['op']=='cleanup')
            self.assertTrue(any(s['op']=='assert_mana_total' and s['value']>0 for s in c['steps'][:i]))
            c=self.case(v=f'real_turn_{owner}');self.assertEqual((self.life(c,owner),self.life(c,1-owner)),(22,18));self.assertEqual(sum(s['op']=='advance_turn' for s in c['steps']),2)
    def test_snapshots_use_real_lifecycle_and_never_synthetic_history_or_targets(self):
        for n in f.pins()['sources']:
            for v in ('ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0'):
                c=self.case(n,v);self.assertIn(dict(op='capture'),c['steps']);self.assertIn(dict(op='assert_recorded_state'),c['steps'])
                self.assertFalse(any(s['op'] in ('choose_target','choose_player','set_storm','set_history') for s in c['steps']))
    def test_empty_zones_libraries_and_complete_expansion(self):
        for n in f.pins()['sources']:
            p=f.classify(self.o[n]);d=self.declaration(n);d['variants']=f.variants(p);cases=f.expand(n,self.rows[n]['csvid'],d,f.spec(d,self.o[n]),d['variants'],self.rows,self.o)
            self.assertEqual(len(cases),len({c['name'] for c in cases}))
            self.assertTrue(all(c['_recipe']['oracle_text']==self.o[n]['oracle_text'] and c['_recipe']['params']['fee']=='{W/B}' for c in cases))
        c=self.case(v='empty_library_0');self.assertEqual(self.final(c,'assert_zone',player=0,zone='library'),0);self.assertEqual((self.life(c,0),self.life(c,1)),(21,19))

if __name__=='__main__':unittest.main()
