import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import cast_untap_trigger_recipes as f

class CastUntapTriggerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
    def declaration(self,n,v='effect_0'):
        p=f.classify(self.o[n]);return dict(family=f.FAMILY,version=2,oracle_text=self.o[n]['oracle_text'],params=f.parameters(p),variants=[v])
    def case(self,n,v='effect_0'):
        d=self.declaration(n,v);return f.expand(n,self.rows[n]['csvid'],d,f.spec(d,self.o[n]),[v],self.rows,self.o)[0]
    def final(self,c,op,**kw):return next(s['value'] for s in reversed(c['steps']) if s['op']==op and all(s.get(k)==v for k,v in kw.items()))
    def tapped(self,c,ref):return self.final(c,'assert_card',ref=ref,property='tapped')
    def test_full_oracle_identity_typed_parameters_and_ui_are_pinned(self):
        f.validate_witnesses(self.o,self.rows)
        for n in f.pins()['sources']:
            d=self.declaration(n)
            for field,value in [('optional',1),('scope','opponent'),('unknown',True)]:
                changed=copy.deepcopy(d);changed['params'][field]=value
                with self.assertRaises(ValueError):f.spec(changed,self.o[n])
            for field,value in [('mana_cost','{0}'),('layout','transform'),('oracle_text',self.o[n]['oracle_text']+'\nDraw a card.')]:
                r=dict(self.o[n]);r[field]=value
                with self.assertRaises(ValueError):f.spec(d,r)
            r=dict(self.o[n]);r['oracle_text']+='\n{T}: Add {G}.'
            with self.assertRaises(ValueError):f.classify(r)
        rows=copy.deepcopy(self.rows);rows['Hematite Talisman']['handler']='other'
        with self.assertRaises(ValueError):f.validate_witnesses(self.o,rows)
    def test_any_caster_is_distinct_from_owned_spell_watchers(self):
        for owner in (0,1):
            c=self.case('Hematite Talisman',f'opponent_cast_{owner}');self.assertEqual(self.tapped(c,'own_creature'),0)
            self.assertEqual(self.final(c,'assert_mana_total',player=owner),177)
            for n in ('Mirran Spy','Nettle Sentinel','Paradox Engine','Traxos, Scourge of Kroog'):
                c=self.case(n,f'opponent_cast_{owner}');self.assertEqual(self.tapped(c,'source'),int(n!='Mirran Spy'))
                self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
    def test_paid_choice_has_no_outer_optional_offer_and_actual_cancel(self):
        c=self.case('Hematite Talisman','exact_trigger_payment_0')
        self.assertFalse(any(s['op']=='choose_trigger_source' for s in c['steps']))
        self.assertEqual(self.final(c,'assert_mana_total',player=0),0);self.assertEqual(self.tapped(c,'own_creature'),0)
        for v in ('decline_0','trigger_unaffordable_0','witness_payment_0'):
            c=self.case('Hematite Talisman',v);self.assertIn(dict(op='choose_cancel_mana',player=0),c['steps'])
            self.assertEqual(self.tapped(c,'own_creature'),1);self.assertFalse(any(s['op']=='choose_target' for s in c['steps']))
        c=self.case('Hematite Talisman','decline_0');self.assertEqual(self.tapped(c,'payment_land'),0)
        self.assertFalse(any(x.get('state') for x in c['setup'] if x['ref'] in ('source','own_artifact','payment_land')))
    def test_any_target_categories_are_oracle_legal_and_constrained(self):
        for owner in (0,1):
            for mode,target in [('enemy_target','enemy_creature'),('already_untapped','own_creature'),('source_target','source'),('land_target','own_land'),('artifact_target','own_artifact'),('enchantment_target','own_enchantment'),('planeswalker_target','own_walker')]:
                c=self.case('Hematite Talisman',f'{mode}_{owner}')
                self.assertIn(dict(op='choose_target',player=owner,ref=target),c['steps']);self.assertEqual(self.tapped(c,target),0)
                self.assertIn(dict(op='assert_unused_choices',value=0),c['steps'])
                if mode in ('enemy_target','already_untapped'):
                    self.assertFalse(any(x.get('state') for x in c['setup'] if x['player']==owner and x['zone']=='battlefield'))
    def test_optional_creature_and_self_inputs_match_reviewed_presentations(self):
        for n in ('Mirran Spy','Nettle Sentinel'):
            c=self.case(n);self.assertIn(dict(op='choose_trigger_source',player=0,ref='source'),c['steps'])
            c=self.case(n,'decline_0');self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
        c=self.case('Mirran Spy','enemy_target_0');self.assertIn(dict(op='choose_target',player=0,ref='enemy_creature'),c['steps'])
        c=self.case('Traxos, Scourge of Kroog');self.assertFalse(any(s['op']=='choose_trigger_source' for s in c['steps']))
        self.assertFalse(f.classify(self.o['Traxos, Scourge of Kroog'])['optional'])
    def test_repeated_spells_remain_separate_even_when_target_already_untapped(self):
        c=self.case('Hematite Talisman','repeat_same_turn_0')
        self.assertEqual(sum(s['op']=='announce_spell' for s in c['steps']),3)
        self.assertEqual(sum(s['op']=='choose_target' for s in c['steps']),3)
        self.assertEqual(self.final(c,'assert_mana_total',player=0),168)
    def test_spell_predicates_reject_nonmatching_spells_without_invented_history(self):
        for n in ('Hematite Talisman','Mirran Spy','Nettle Sentinel','Traxos, Scourge of Kroog'):
            c=self.case(n,'nonmatching_spell_0');self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
            self.assertEqual(sum(s['op']=='announce_spell' for s in c['steps']),1)
        for n in ('Sol Ring','Isamaru, Hound of Konda','Song of Freyalise','Hope of Ghirapur'):self.assertTrue(f.matches(self.o[n],'historic'))
        self.assertFalse(f.matches(self.o['Grizzly Bears'],'historic'));self.assertFalse(f.matches(self.o['Dryad Arbor'],'any'))
    def test_historic_union_has_independent_legendary_saga_and_overlap_receipts(self):
        for mode,witness in [('effect','Sol Ring'),('alternate_spell','Isamaru, Hound of Konda'),('saga_spell','Song of Freyalise'),('overlap_once','Hope of Ghirapur')]:
            c=self.case('Traxos, Scourge of Kroog',mode+'_1')
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='spell_0'),witness)
            self.assertEqual(self.tapped(c,'source'),0);self.assertEqual(sum(s['op']=='announce_spell' for s in c['steps']),1)
    def test_sweep_untaps_only_owned_nonlands_and_precedes_spell_entry(self):
        c=self.case('Paradox Engine','land_creature_excluded_1')
        for ref in ('source','own_creature','own_artifact','own_enchantment','own_walker'):self.assertEqual(self.tapped(c,ref),0)
        for ref in ('own_land','own_land_creature','enemy_creature','enemy_artifact'):self.assertEqual(self.tapped(c,ref),1)
        c=self.case('Paradox Engine','enters_after_trigger_0');self.assertEqual(self.tapped(c,'spell_0'),1)
        self.assertLess(next(i for i,s in enumerate(c['steps']) if s['op']=='announce_spell'),next(i for i,s in enumerate(c['steps']) if s['op']=='resolve'))
    def test_restricted_untap_is_real_two_turn_sequence_with_zero_pools(self):
        for n in ('Nettle Sentinel','Traxos, Scourge of Kroog'):
            c=self.case(n,'untap_step_only_0');self.assertEqual(self.tapped(c,'source'),1);self.assertEqual(self.tapped(c,'own_creature'),0)
            self.assertEqual(sum(s['op']=='advance_turn' for s in c['steps']),2);self.assertEqual(sum(s['op']=='untap_step' for s in c['steps']),2)
            self.assertEqual(self.final(c,'assert_mana_total',player=0),0)
            c=self.case(n,'untap_step_then_cast_1');self.assertEqual(self.tapped(c,'source'),0)
            first=next(i for i,s in enumerate(c['steps']) if s['op']=='advance_turn');self.assertTrue(any(s['op']=='set_mana' and s['value']==0 for s in c['steps'][:first]))
    def test_traxos_enters_tapped_by_rule_without_fixture_override(self):
        for v in ('effect_0','entry_only_1'):
            c=self.case('Traxos, Scourge of Kroog',v);source=next(x for x in c['setup'] if x['ref']=='source');self.assertNotIn('state',source)
        c=self.case('Traxos, Scourge of Kroog','entry_only_1');self.assertEqual(self.tapped(c,'source'),1)
        self.assertEqual(sum(s['op']=='cast' for s in c['steps']),1);self.assertEqual(sum(s['op']=='announce_spell' for s in c['steps']),1)
    def test_functional_untap_receipt_uses_real_mana_activation(self):
        for n in ('Hematite Talisman','Paradox Engine'):
            c=self.case(n,'functional_target_0');self.assertIn(dict(op='activate',ref='own_artifact',value=1),c['steps'])
            self.assertEqual(self.tapped(c,'own_artifact'),1)
            self.assertEqual(self.final(c,'assert_mana_total',player=0),178 if n=='Hematite Talisman' else 181)
    def test_denied_cast_does_not_queue_trigger_inputs_or_announce(self):
        for n in f.pins()['sources']:
            for mode in ('witness_insufficient_mana','source_insufficient_mana'):
                c=self.case(n,mode+'_0');self.assertFalse(any(s['op'].startswith('choose_') or s['op']=='announce_spell' for s in c['steps']))
    def test_generic_denial_blocks_real_ready_producers_not_only_float(self):
        for n,mode in [('Mirran Spy','witness_insufficient_mana'),('Traxos, Scourge of Kroog','witness_insufficient_mana'),('Hematite Talisman','source_insufficient_mana')]:
            for owner in (0,1):
                c=self.case(n,f'{mode}_{owner}');ring=next(x for x in c['setup'] if x['ref']=='own_artifact')
                self.assertEqual(ring['state'],['tapped']);self.assertEqual(self.tapped(c,'own_artifact'),1)
                self.assertEqual(self.final(c,'assert_mana_total',player=owner),0)
                self.assertFalse(any(s['op']=='activate' or s['op'].startswith('choose_') for s in c['steps']))
        # Colored negatives already cannot be paid by the ready colorless Ring.
        for n,mode in [('Nettle Sentinel','witness_insufficient_mana'),('Mirran Spy','source_insufficient_mana')]:
            self.assertNotIn('state',next(x for x in self.case(n,mode+'_0')['setup'] if x['ref']=='own_artifact'))
    def test_ai_unfunded_optional_fee_has_no_partial_mana_activation_available(self):
        c=self.case('Hematite Talisman','witness_payment_1');self.assertEqual(self.tapped(c,'own_artifact'),1)
        self.assertEqual(self.final(c,'assert_mana_total',player=1),0);self.assertEqual(self.tapped(c,'own_creature'),1)
        self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
        c=self.case('Hematite Talisman','witness_payment_0');self.assertEqual(self.tapped(c,'own_artifact'),0)
        self.assertIn(dict(op='choose_cancel_mana',player=0),c['steps'])
    def test_absent_late_and_countered_spell_use_zone_receipts(self):
        for n in f.pins()['sources']:
            c=self.case(n,'source_removed_before_0');self.assertFalse(any(s['op']=='assert_card' and s.get('ref')=='source' for s in c['steps']))
            self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
            c=self.case(n,'source_arrives_late_0');self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
            c=self.case(n,'countered_witness_0');self.assertIn(dict(op='resolve_response'),c['steps'])
            witness=f.SPELLS[f.classify(self.o[n])['predicate']][0]
            before=sum(x['zone']=='graveyard' and x['player']==0 and x['name']==witness for x in c['setup'])
            self.assertEqual(self.final(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows[witness]['csvid']),before+1)
    def test_snapshots_replay_real_steps_and_no_synthetic_untap_or_history(self):
        for n in f.pins()['sources']:
            for v in ('ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0'):
                c=self.case(n,v);self.assertIn(dict(op='capture'),c['steps']);self.assertIn(dict(op='assert_recorded_state'),c['steps'])
                self.assertFalse(any(s['op'] in ('set_storm','set_history','untap','set_tapped') for s in c['steps']))
        c=self.case('Nettle Sentinel','untap_step_snapshot_0');self.assertEqual(sum(s['op']=='untap_step' for s in c['steps']),4)
    def test_passive_clauses_are_retained_and_printed_costs_distinct(self):
        self.assertEqual(f.classify(self.o['Mirran Spy'])['keywords'],['Flying']);self.assertEqual(f.classify(self.o['Traxos, Scourge of Kroog'])['keywords'],['Trample'])
        c=self.case('Hematite Talisman','source_payment_0')
        self.assertIn(dict(op='assert_mana_total',player=0,value=0),c['steps']);self.assertEqual(self.final(c,'assert_mana_total',player=0),176)
        c=self.case('Nettle Sentinel','cleanup_0');i=next(i for i,s in enumerate(c['steps']) if s['op']=='cleanup')
        self.assertTrue(any(s['op']=='assert_mana_total' and s['value']>0 for s in c['steps'][:i]));self.assertEqual(self.final(c,'assert_mana_total',player=0),0)

if __name__=='__main__':unittest.main()
