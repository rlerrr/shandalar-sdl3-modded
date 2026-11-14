import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import draw_value_trigger_recipes as f

class DrawValueTriggerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
    def declaration(self,name,variant='effect_0'):
        p=f.classify(self.oracle[name]);return dict(family=f.FAMILY,version=2,oracle_text=self.oracle[name]['oracle_text'],params=f.parameters(p),variants=[variant])
    def case(self,name,variant='effect_0'):
        d=self.declaration(name,variant);return f.expand(name,self.rows[name]['csvid'],d,f.spec(d,self.oracle[name]),d['variants'],self.rows,self.oracle)[0]
    def final(self,c,op,**fields):return next(s['value'] for s in reversed(c['steps']) if s['op']==op and all(s.get(k)==v for k,v in fields.items()))
    def library_delta(self,c,who):return sum(x['zone']=='library' and x['player']==who for x in c['setup'])-self.final(c,'assert_zone',player=who,zone='library')
    def test_complete_sources_typed_parameters_and_optional_presentations_are_pinned(self):
        f.validate_witnesses(self.oracle,self.rows)
        for name in f.pins()['sources']:
            d=self.declaration(name)
            for key,value in [('amount',True),('optional',0),('condition',{'controlled_color':'U'}),('unknown',1)]:
                changed=copy.deepcopy(d);changed['params'][key]=value
                if f.exact(changed,d):continue
                with self.assertRaises(ValueError):f.spec(changed,self.oracle[name])
            for key,value in [('oracle_text','Whenever you draw a card, draw two cards.'),('mana_cost','{0}'),('layout','transform')]:
                r=copy.deepcopy(self.oracle[name]);r[key]=value
                with self.assertRaises(ValueError):f.spec(d,r)
            r=copy.deepcopy(self.oracle[name]);r['oracle_text']+='\n{T}: Draw a card.'
            with self.assertRaises(ValueError):f.classify(r)
        rows=copy.deepcopy(self.rows);rows['Mind\'s Eye']['handler']='card_other'
        with self.assertRaises(ValueError):f.validate_witnesses(self.oracle,rows)
    def test_draw_life_counter_and_damage_rewards_follow_actual_card_quantity_and_drawer(self):
        for owner in (0,1):
            c=self.case('Lorescale Coatl',f'alternate_spell_{owner}')
            self.assertEqual(self.final(c,'assert_card',ref='source',property='plus1_counters'),2)
            self.assertEqual(self.final(c,'assert_card',ref='source',property='power'),4)
            self.assertEqual(self.library_delta(c,owner),2)
            c=self.case('Horizon Chimera',f'alternate_spell_{owner}');self.assertEqual(self.final(c,'assert_life',player=owner),22)
            c=self.case('Fate Unraveler',f'alternate_spell_{owner}');self.assertEqual(self.final(c,'assert_life',player=1-owner),18)
            self.assertEqual(self.final(c,'assert_life',player=owner),20)
            c=self.case('Lorescale Coatl',f'other_drawer_{owner}');self.assertEqual(self.final(c,'assert_card',ref='source',property='plus1_counters'),0)
    def test_self_target_mill_interleaves_individual_draws_before_the_next_card(self):
        for owner in (0,1):
            c=self.case("Jace's Erasure",f'interleaved_self_two_{owner}')
            self.assertEqual(self.final(c,'assert_zone',player=owner,zone='library'),1)
            self.assertEqual(self.final(c,'assert_zone_card',player=owner,zone='library',index=0),self.rows['Island']['csvid'])
            for name,n in [('Forest',1),('Giant Growth',2)]:self.assertEqual(self.final(c,'assert_zone_multiplicity',player=owner,zone='hand',index=self.rows[name]['csvid']),n)
            for name in ('Plains','Lightning Bolt'):self.assertEqual(self.final(c,'assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[name]['csvid']),1)
            self.assertEqual(sum(s['op']=='choose_player' and s['value']==owner for s in c['steps']),2)
    def test_opposing_mill_is_bounded_and_never_becomes_a_draw(self):
        for owner in (0,1):
            for name in ("Jace's Erasure",'Psychic Corrosion'):
                for mode,n in [('short_mill_library',1),('empty_mill_library',0)]:
                    c=self.case(name,f'{mode}_{owner}');self.assertEqual(self.final(c,'assert_zone',player=1-owner,zone='library'),0)
                    before=sum(x['zone']=='graveyard' and x['player']==1-owner for x in c['setup'])
                    self.assertEqual(self.final(c,'assert_zone',player=1-owner,zone='graveyard'),before+n)
            c=self.case('Psychic Corrosion',f'alternate_spell_{owner}');self.assertEqual(self.library_delta(c,1-owner),4);self.assertEqual(self.library_delta(c,owner),2)
    def test_opposing_draw_rewards_nest_at_each_real_draw_and_cost_separately(self):
        for owner in (0,1):
            c=self.case('Consecrated Sphinx',f'alternate_spell_{owner}');self.assertEqual(self.library_delta(c,owner),4);self.assertEqual(self.library_delta(c,1-owner),2)
            c=self.case("Mind's Eye",f'trigger_payment_{owner}');self.assertEqual(self.library_delta(c,owner),2);self.assertEqual(self.library_delta(c,1-owner),2)
            self.assertEqual(self.final(c,'assert_mana_total',player=owner),0);self.assertEqual(self.final(c,'assert_mana_total',player=1-owner),0)
            c=self.case("Mind's Eye",f'trigger_unaffordable_{owner}');self.assertEqual(self.library_delta(c,owner),0);self.assertEqual(self.library_delta(c,1-owner),1)
    def test_optional_acceptance_decline_and_real_payment_cancel_are_distinct(self):
        for name in ('Consecrated Sphinx',"Mind's Eye", "Jace's Erasure",'Kederekt Parasite'):
            accept=self.case(name);decline=self.case(name,'decline_0')
            self.assertIn(dict(op='choose_trigger_source',player=0,ref='source'),accept['steps'])
            self.assertFalse(any(s['op']=='choose_trigger_source' for s in decline['steps']))
        c=self.case("Mind's Eye",'payment_decline_0');self.assertIn(dict(op='choose_cancel_mana',player=0),c['steps'])
        self.assertEqual(self.library_delta(c,0),0);self.assertEqual(self.final(c,'assert_card',ref='payment_land',property='tapped'),0)
    def test_unaffordable_human_payment_cancels_the_actual_offer_without_adding_mana_permission(self):
        c=self.case("Mind's Eye",'trigger_unaffordable_0');s=c['steps']
        accept=next(i for i,x in enumerate(s) if x['op']=='choose_trigger_source')
        self.assertEqual(s[accept+1],dict(op='choose_cancel_mana',player=0))
        self.assertEqual(s[accept+2],dict(op='resolve'))
        self.assertEqual(self.library_delta(c,0),0);self.assertEqual(self.library_delta(c,1),1)
        self.assertEqual(self.final(c,'assert_mana_total',player=0),0)
        self.assertFalse(any(x['zone']=='battlefield' and x['name']=='Forest' for x in c['setup']))
        self.assertEqual(self.final(c,'assert_card',ref='mana_control',property='tapped'),1)
        other=self.case("Mind's Eye",'trigger_unaffordable_1')
        self.assertFalse(any(x['op']=='choose_cancel_mana' for x in other['steps']))
    def test_red_condition_uses_controlled_permanents_not_hand_color_or_enemy_cards(self):
        for owner in (0,1):
            for mode,life in [('effect',19),('no_red_permanent',20),('enemy_red_only',20),('red_removed_before_draw',20),('other_drawer',20)]:
                c=self.case('Kederekt Parasite',f'{mode}_{owner}');self.assertEqual(self.final(c,'assert_life',player=1-owner),life)
    def test_actual_activation_and_nondraw_hand_return_separate_events_from_hand_size(self):
        for owner in (0,1):
            c=self.case('Lorescale Coatl',f'activation_draw_{owner}')
            self.assertIn(dict(op='activate',ref='tome',value=1),c['steps']);self.assertFalse(any(s['op']=='cast' for s in c['steps']))
            self.assertEqual(self.final(c,'assert_card',ref='tome',property='tapped'),1);self.assertEqual(self.final(c,'assert_mana_total',player=owner),176)
            self.assertEqual(self.final(c,'assert_card',ref='source',property='plus1_counters'),1)
            c=self.case('Lorescale Coatl',f'not_drawn_to_hand_{owner}');self.assertEqual(self.final(c,'assert_zone',player=owner,zone='hand'),5)
            self.assertEqual(self.library_delta(c,owner),0);self.assertEqual(self.final(c,'assert_card',ref='source',property='plus1_counters'),0)
    def test_pending_casts_and_countered_draw_spells_have_no_premature_draw_rewards(self):
        for owner in (0,1):
            c=self.case('Horizon Chimera',f'effect_{owner}');s=c['steps'];resolve=next(i for i,x in enumerate(s) if x['op']=='resolve')
            self.assertEqual(next(x['value'] for x in reversed(s[:resolve]) if x['op']=='assert_life' and x['player']==owner),20)
            self.assertEqual(self.final(c,'assert_life',player=owner),21)
            for mode in ('countered_witness','source_removed_before','source_removed_pending','source_arrives_late'):
                c=self.case('Horizon Chimera',f'{mode}_{owner}');self.assertEqual(self.final(c,'assert_life',player=owner),20)
            c=self.case('Horizon Chimera',f'source_removed_after_draw_{owner}');self.assertEqual(self.final(c,'assert_life',player=owner),21)
    def test_denied_casts_and_snapshots_never_synthesize_draw_or_counter_history(self):
        for name in f.pins()['sources']:
            for mode in ('witness_insufficient_mana','witness_wrong_color','source_insufficient_mana'):
                c=self.case(name,mode+'_0');self.assertFalse(any(s['op'] in ('choose_trigger_source','choose_player','choose_zone_card') for s in c['steps']))
            for mode in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
                c=self.case(name,mode+'_1');self.assertIn(dict(op='ai_roundtrip'),c['steps']);self.assertEqual(c['steps'][-1],dict(op='assert_recorded_state'))
                self.assertFalse(any(s['op'] in ('draw','set_counter','set_draw_count','reset_death_history','emit_event') for s in c['steps']))
    def test_witness_characteristics_and_existing_source_keyword_clauses_are_complete(self):
        for name,key,value in [('Reach Through Mists','oracle_text','Draw two cards.'),('Jayemdae Tome','oracle_text','{T}: Draw a card.'),('Raise Dead','oracle_text','Draw a card.'),('Seal of Fire','colors',['U'])]:
            altered=copy.deepcopy(self.oracle);altered[name][key]=value
            with self.assertRaises(ValueError):f.validate_witnesses(altered,self.rows)
        p=f.classify(self.oracle['Horizon Chimera']);self.assertEqual(set(p['keywords']),{'Flash','Flying','Trample'})
        p=f.classify(self.oracle['Consecrated Sphinx']);self.assertEqual(p['keywords'],['Flying'])
    def test_underworld_exact_followon_has_BBB_cost_per_card_opponent_damage_and_own_draw_negative(self):
        p=f.classify(self.oracle['Underworld Dreams'])
        self.assertEqual(p['mana'],[0,3,0,0,0,0]);self.assertEqual(len(f.variants(p)),52)
        self.assertEqual(f.parameters(p),f.parameters(f.classify(self.oracle['Fate Unraveler'])))
        for owner in (0,1):
            for mode,damage in [('effect',1),('alternate_spell',2),('other_drawer',0)]:
                c=self.case('Underworld Dreams',f'{mode}_{owner}')
                self.assertEqual(self.final(c,'assert_life',player=1-owner),20-damage)
                self.assertEqual(self.final(c,'assert_life',player=owner),20)
            c=self.case('Underworld Dreams',f'source_payment_{owner}');s=c['steps']
            source=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source')
            refill=next(i for i,x in enumerate(s) if i>source and x['op']=='set_mana')
            for color in range(6):self.assertIn(dict(op='assert_mana',player=owner,color=color,value=0),s[source:refill])
            c=self.case('Underworld Dreams',f'source_wrong_color_{owner}')
            self.assertIn(dict(op='cast',ref='source',value=0),c['steps'])
            self.assertEqual(self.final(c,'assert_mana',player=owner,color=0),3)
            self.assertEqual(self.final(c,'assert_mana',player=owner,color=1),0)

if __name__=='__main__':unittest.main()
