"""Real entry choices, turn gates, modal order and keyword legality controls."""
import copy
import json
import unittest
from pathlib import Path

from generate_cases import OPS,PROPERTIES,registry
import entry_tap_trigger_recipes as model


class EntryTapTriggerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

    def declaration(self,n):
        s=model.pins()['sources'][n]
        return dict(family=model.FAMILY,version=2,oracle_text=self.oracle[n]['oracle_text'],variants=['tap_0'],params=dict(schema=1,mechanism=model.mechanism(self.oracle[n]),presentation=copy.deepcopy(s['presentation'])))

    def parameters(self,n):return model.spec(self.declaration(n),self.oracle[n])

    def case(self,n,mode='tap',owner=0):
        d=self.declaration(n);d['variants']=[f'{mode}_{owner}'];p=model.spec(d,self.oracle[n]);return model.expand(n,self.rows[n]['csvid'],d,p,d['variants'],self.rows,self.oracle)[0]

    def test_all_twelve_expand_504_whole_oracle_valid_cases(self):
        total=0
        for n in model.pins()['sources']:
            d=self.declaration(n);p=model.spec(d,self.oracle[n]);d['variants']=model.variants(p);cases=model.expand(n,self.rows[n]['csvid'],d,p,d['variants'],self.rows,self.oracle);total+=len(cases)
            self.assertEqual({int(c['_recipe']['variant'][-1]) for c in cases},{0,1})
            for c in cases:
                self.assertEqual(c['_recipe']['oracle_text'],self.oracle[n]['oracle_text'])
                self.assertEqual(set(c['_recipe']['covered_clauses']),set(self.oracle[n]['oracle_text'].splitlines()))
                for s in c['steps']:
                    self.assertIn(s['op'],OPS)
                    if 'property' in s:self.assertIn(s['property'],PROPERTIES)
        self.assertEqual(total,504)

    def test_actual_type_cost_and_full_other_clauses_cannot_change(self):
        for n in model.pins()['sources']:
            d=self.declaration(n)
            for key,value in [('type_line','Instant'),('mana_cost','{0}'),('oracle_text',self.oracle[n]['oracle_text']+'\nDraw a card.'),('layout','split'),('power','99')]:
                r=dict(self.oracle[n],**{key:value})
                with self.assertRaises(ValueError):model.spec(dict(d,oracle_text=r['oracle_text']),r)
        self.assertTrue(self.parameters('Pestermite')['flying']);self.assertTrue(self.parameters('Pestermite')['flash']);self.assertTrue(self.parameters('Bounding Krasis')['flash'])

    def test_strict_predicate_schema_and_real_ui_annotations(self):
        for mutate in [lambda d:d['params'].__setitem__('schema',True),lambda d:d['params']['mechanism'].__setitem__('optional',1),lambda d:d['params']['presentation'].__setitem__('human_optional_trigger',1)]:
            d=self.declaration('Bounding Krasis');mutate(d)
            with self.assertRaises(ValueError):model.spec(d,self.oracle['Bounding Krasis'])
        self.assertEqual(self.parameters('Frost Lynx')['controller'],'opponent');self.assertEqual(self.parameters('Stensia Innkeeper')['scope'],'land')

    def test_target_predicates_do_not_conflate_any_creature_permanent_land_or_controller(self):
        frost=self.parameters('Frost Lynx');pest=self.parameters('Pestermite');krasis=self.parameters('Bounding Krasis');land=self.parameters('Stensia Innkeeper')
        self.assertTrue(model.target_matches(self.oracle['Elvish Mystic'],frost,0,1));self.assertFalse(model.target_matches(self.oracle['Elvish Mystic'],frost,0,0))
        for n in ['Sol Ring','Swamp','Seal of Cleansing']:
            self.assertTrue(model.target_matches(self.oracle[n],pest,0,0));self.assertFalse(model.target_matches(self.oracle[n],krasis,0,0))
        self.assertTrue(model.target_matches(self.oracle['Swamp'],land,0,1));self.assertFalse(model.target_matches(self.oracle['Sol Ring'],land,0,1))

    def test_exact_real_cost_and_no_owned_source_can_repair_denied_color(self):
        for n in model.pins()['sources']:
            p=self.parameters(n)
            for owner in (0,1):
                s=self.case(n,'payment',owner)['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
                for color,a in enumerate(p['mana']):self.assertIn(dict(op='set_mana',player=owner,color=color,value=a),s[:cast])
                for mode in ['insufficient_mana','wrong_color']:
                    c=self.case(n,mode,owner);steps=c['steps'];self.assertIn(dict(op='cast',ref='source',value=0),steps)
                    self.assertFalse(any(x['op'].startswith('choose_') for x in steps))
                    missing=next(i for i in range(1,6) if p['mana'][i]);self.assertNotEqual(missing,1)
                    lands=[x['name'] for x in c['setup'] if x['zone']=='battlefield' and x['player']==owner and 'Land' in self.oracle[x['name']]['type_line'].partition(' — ')[0].split()]
                    self.assertEqual(lands,['Swamp'])

    def test_only_actual_optional_entry_wrapper_is_chosen(self):
        for owner in (0,1):
            for n in model.pins()['sources']:
                s=self.case(n,owner=owner)['steps'];triggers=[i for i,x in enumerate(s) if x['op']=='choose_trigger']
                self.assertEqual(len(triggers),int(n in ('Bounding Krasis','Niblis of the Mist') and owner==0))
                if triggers:self.assertLess(triggers[0],next(i for i,x in enumerate(s) if x['op']=='choose_target'))

    def test_twiddle_target_then_real_menu_precedes_entry_resolution_for_both_choosers(self):
        for n in ['Pestermite','Bounding Krasis']:
            for owner in (0,1):
                for mode,value in [('tap',0),('untap',1)]:
                    s=self.case(n,mode,owner)['steps'];target=next(i for i,x in enumerate(s) if x['op']=='choose_target');dialog=next(i for i,x in enumerate(s) if x['op']=='choose_dialog');resolve=next(i for i,x in enumerate(s) if x['op']=='resolve')
                    self.assertLess(target,dialog);self.assertLess(dialog,resolve);self.assertEqual(s[dialog],dict(op='choose_dialog',player=owner,value=value))
                    self.assertIn(dict(op='present_ai_dialogs',value=1),s[:target])

    def test_optional_target_refusal_has_no_invented_modal_choice_or_tap(self):
        for n in ['Leonin Snarecaster','Pestermite','Bounding Krasis']:
            for owner in (0,1):
                s=self.case(n,'decline',owner)['steps'];self.assertEqual(sum(x['op']=='choose_optional_target' for x in s),1)
                self.assertFalse(any(x['op']=='choose_dialog' for x in s))
                self.assertIn(dict(op='assert_forced_target_choices',value=0),s)
                self.assertIn(dict(op='assert_card',ref=f'creature_{1-owner}',property='tapped',value=0),s)

    def test_mandatory_no_legal_target_casts_then_clears_only_observed_unused_probe(self):
        for n in ['Frost Lynx','Heavy Infantry','Stensia Innkeeper']:
            for owner in (0,1):
                c=self.case(n,'no_targets',owner);s=c['steps'];probe=next(i for i,x in enumerate(s) if x['op']=='choose_optional_target');resolve=next(i for i,x in enumerate(s) if x['op']=='resolve');unused=next(i for i,x in enumerate(s) if x['op']=='assert_unused_choices')
                self.assertLess(probe,resolve);self.assertLess(resolve,unused);self.assertEqual(s[unused]['value'],1)
                self.assertIn(dict(op='cast',ref='source',value=1),s[:probe]);self.assertIn(dict(op='assert_choices_consumed'),s[unused:])
                legal=[x for x in c['setup'] if x['zone']=='battlefield' and model.target_matches(self.oracle[x['name']],self.parameters(n),owner,x['player'])];self.assertFalse(legal)

    def test_freeze_uses_first_and_second_actual_controller_untap_steps_after_source_return(self):
        for n in ['Frost Lynx','Stensia Innkeeper']:
            for owner in (0,1):
                s=self.case(n,'source_departure_lock',owner)['steps'];untaps=[(i,x['player']) for i,x in enumerate(s) if x['op']=='untap_step'];self.assertEqual([pl for i,pl in untaps],[1-owner,owner,1-owner])
                bind=next(i for i,x in enumerate(s) if x['op']=='bind_hand_copy');self.assertLess(bind,untaps[0][0])
                ref=f'land_{1-owner}' if n=='Stensia Innkeeper' else f'creature_{1-owner}'
                self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=1),s[untaps[0][0]:untaps[1][0]])
                self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=0),s[untaps[-1][0]:])
                for i,pl in untaps:self.assertEqual(s[i-1],dict(op='advance_turn'))

    def test_cleanup_does_not_consume_next_untap_lock(self):
        s=self.case('Frost Lynx','freeze_cleanup')['steps'];cleanup=next(i for i,x in enumerate(s) if x['op']=='cleanup');untap=next(i for i,x in enumerate(s) if x['op']=='untap_step')
        self.assertIn(dict(op='assert_card',ref='creature_1',property='tapped',value=1),s[cleanup:untap])

    def test_post_untap_combat_establishes_same_player_main_without_extra_aging(self):
        for owner in (0,1):
            for n,mode,current,advances in [('Frost Lynx','frozen_attack',1-owner,1),('Pestermite','flying_evasion',owner,2),('Pestermite','flying_reach',owner,2)]:
                s=self.case(n,mode,owner)['steps'];begin=next(i for i,x in enumerate(s) if x['op']=='begin_attackers')
                self.assertEqual(s[begin-1],dict(op='turn',player=current,value=0))
                self.assertEqual(sum(x['op']=='advance_turn' for x in s[:begin]),advances)
                self.assertEqual(sum(x['op']=='untap_step' for x in s[:begin]),advances)
                last=max(i for i,x in enumerate(s[:begin]) if x['op']=='untap_step');self.assertEqual(s[last]['player'],current)
                self.assertFalse(any(x['op'] in ('advance_turn','untap_step') for x in s[last+1:begin]))

    def test_flash_casts_offturn_with_printed_cost_not_ready_flags(self):
        for n in ['Pestermite','Bounding Krasis']:
            for owner in (0,1):
                c=self.case(n,'offturn',owner);s=c['steps'];self.assertEqual(s[0],dict(op='turn',player=1-owner,value=0))
                self.assertIn(dict(op='cast',ref='source',value=1),s)
                source=next(x for x in c['setup'] if x['ref']=='source');self.assertEqual(source['zone'],'hand');self.assertFalse(source.get('state'))

    def test_flying_block_control_real_flash_cast_occurs_between_attackers_and_blockers(self):
        for owner in (0,1):
            c=self.case('Pestermite','block_flying',owner);s=c['steps'];attack=next(i for i,x in enumerate(s) if x['op']=='attack');cast=next(i for i,x in enumerate(s) if x['op']=='cast');block=next(i for i,x in enumerate(s) if x['op']=='block')
            self.assertLess(attack,cast);self.assertLess(cast,block);self.assertEqual(s[block]['value'],1)
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']==f'creature_{1-owner}'),'Suntail Hawk')
            self.assertIn(dict(op='assert_card',ref='source',property='blocking_count',value=1),s)

    def test_flying_ground_and_reach_blocks_after_actual_later_own_turn(self):
        for mode,expected,witness in [('flying_evasion',0,'Elvish Mystic'),('flying_reach',1,'Giant Spider')]:
            c=self.case('Pestermite',mode);s=c['steps'];attack=next(i for i,x in enumerate(s) if x['op']=='attack');self.assertEqual(sum(x['op']=='advance_turn' for x in s[:attack]),2)
            self.assertEqual(sum(x['op']=='untap_step' for x in s[:attack]),2)
            self.assertIn(dict(op='block',ref='creature_1',other='source',value=expected),s)
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='creature_1'),witness)

    def test_untapped_artifact_performs_real_mana_activation_and_real_followup_cast(self):
        for owner in (0,1):
            c=self.case('Pestermite','artifact_function',owner);s=c['steps'];target=f'artifact_{owner}'
            self.assertEqual(next(x['state'] for x in c['setup'] if x['ref']==target),['tapped'])
            self.assertIn(dict(op='activate',ref=target,value=1),s);self.assertIn(dict(op='cast',ref='functional_stone',value=1),s)
            self.assertIn(dict(op='assert_mana',player=owner,color=0,value=2),s);self.assertIn(dict(op='assert_mana',player=owner,color=0,value=0),s)

    def test_snapshots_replay_exact_inputs_and_counter_uses_real_response_context(self):
        for n in model.pins()['sources']:
            for mode in ['ready_snapshot','pending_snapshot']:
                s=self.case(n,mode)['steps'];capture=next(i for i,x in enumerate(s) if x['op']=='capture');record=next(i for i,x in enumerate(s) if x['op']=='record_state');restore=next(i for i,x in enumerate(s) if x['op']=='restore')
                self.assertEqual(s[capture+1:record],s[restore+1:-1])
            s=self.case(n,'countered')['steps'];ops=[x['op'] for x in s]
            self.assertLess(ops.index('announce_spell'),ops.index('cast_response'));self.assertLess(ops.index('cast_response'),ops.index('resolve_response'))
            self.assertFalse(any(x['op'].startswith('choose_') for x in s))

    def test_every_expanded_combat_advance_damage_end_follows_complete_lifecycle(self):
        complete=[];declarations=[];count=0
        for n in model.pins()['sources']:
            d=self.declaration(n);p=model.spec(d,self.oracle[n]);d['variants']=model.variants(p)
            for c in model.expand(n,self.rows[n]['csvid'],d,p,d['variants'],self.rows,self.oracle):
                count+=1;state=None;advanced=damaged=ended=0
                for s in c['steps']:
                    op=s['op']
                    if op=='begin_attackers':self.assertIn(state,(None,'ended'));state='attackers'
                    elif op=='finish_attackers':self.assertEqual(state,'attackers');state='before_blockers'
                    elif op=='begin_blockers':self.assertEqual(state,'before_blockers');state='blockers'
                    elif op=='advance_combat':self.assertEqual(state,'blockers');state='after_blockers';advanced+=1
                    elif op=='combat_damage':self.assertEqual(state,'after_blockers');state='damage';damaged+=1
                    elif op=='end_combat':self.assertEqual(state,'damage');state='ended';ended+=1
                self.assertEqual(advanced,damaged,c['name']);self.assertEqual(damaged,ended,c['name'])
                if advanced:self.assertEqual(state,'ended');complete.append(c['_recipe']['variant'])
                elif state:
                    # The unchanged accepted Flash-block smoke cases explicitly
                    # stop at legal declarations; they never advance/end combat.
                    self.assertEqual(state,'blockers');self.assertTrue(c['_recipe']['variant'].startswith('block_flying_'));declarations.append(c['name'])
        self.assertEqual(count,504);self.assertEqual(len(complete),28);self.assertEqual(len(declarations),8)

    def test_completed_flying_combat_has_real_player_damage_or_death_and_marked_survivor(self):
        for owner in (0,1):
            for mode in ['flying_evasion','flying_reach']:
                s=self.case('Pestermite',mode,owner)['steps'];damage=next(i for i,x in enumerate(s) if x['op']=='combat_damage');end=next(i for i,x in enumerate(s) if x['op']=='end_combat')
                self.assertLess(damage,end)
                for segment in [s[damage:end],s[end:]]:
                    self.assertIn(dict(op='assert_life',player=owner,value=20),segment)
                    self.assertIn(dict(op='assert_life',player=1-owner,value=18 if mode=='flying_evasion' else 20),segment)
                    self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=int(mode=='flying_evasion')),segment)
                    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=9234,value=int(mode=='flying_reach')),segment)
                    self.assertIn(dict(op='assert_card',ref=f'creature_{1-owner}',property='damage',value=2 if mode=='flying_reach' else 0),segment)
            s=self.case('Frost Lynx','frozen_attack',owner)['steps'];damage=next(i for i,x in enumerate(s) if x['op']=='combat_damage')
            self.assertIn(dict(op='assert_life',player=owner,value=20),s[damage:]);self.assertIn(dict(op='assert_life',player=1-owner,value=20),s[damage:])

    def test_positive_blocked_receipt_follows_actual_completed_blocker_declaration(self):
        for owner in (0,1):
            s=self.case('Pestermite','flying_reach',owner)['steps']
            block=next(i for i,x in enumerate(s) if x['op']=='block');advance=next(i for i,x in enumerate(s) if x['op']=='advance_combat');damage=next(i for i,x in enumerate(s) if x['op']=='combat_damage')
            receipts=[i for i,x in enumerate(s) if x==dict(op='assert_card',ref='source',property='blocked',value=1)]
            self.assertEqual(len(receipts),1);self.assertLess(block,advance);self.assertLess(advance,receipts[0]);self.assertLess(receipts[0],damage)

    def test_stitched_mangler_actually_enters_tapped_and_untaps_on_its_own_step(self):
        self.assertTrue(self.parameters('Stitched Mangler')['enters_tapped'])
        for owner in (0,1):
            for mode in ['tap','blink_source']:
                s=self.case('Stitched Mangler',mode,owner)['steps'];self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s)
            s=self.case('Stitched Mangler','next_untap',owner)['steps'];own=next(i for i,x in enumerate(s) if x==dict(op='untap_step',player=owner))
            self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s[:own]);self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),s[own:])

    def test_chillbringer_survives_spider_with_two_damage_and_marks_spider_three(self):
        p=self.parameters('Chillbringer');self.assertEqual((p['power'],p['toughness']),(3,3));self.assertEqual(p['mana'],[4,0,1,0,0,0])
        self.assertEqual(self.parameters('Spire Patrol')['mana'],[2,0,1,0,0,1])
        for owner in (0,1):
            for n,survives,spider_damage in [('Chillbringer',True,3),('Spire Patrol',False,3),('Niblis of the Mist',False,2)]:
                s=self.case(n,'flying_reach',owner)['steps'];damage=next(i for i,x in enumerate(s) if x['op']=='combat_damage');end=next(i for i,x in enumerate(s) if x['op']=='end_combat')
                for segment in [s[damage:end],s[end:]]:
                    self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=int(survives)),segment)
                    self.assertIn(dict(op='assert_card',ref=f'creature_{1-owner}',property='damage',value=spider_damage),segment)
                    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[n]['csvid'],value=int(not survives)),segment)
                    if survives:self.assertIn(dict(op='assert_card',ref='source',property='damage',value=2),segment)

    def test_nonflash_flying_block_casts_on_own_turn_before_real_later_opponent_combat(self):
        for n in ['Niblis of the Mist','Spire Patrol','Chillbringer']:
            self.assertFalse(self.parameters(n)['flash'])
            for owner in (0,1):
                c=self.case(n,'block_flying',owner);s=c['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast');advance=next(i for i,x in enumerate(s) if x['op']=='advance_turn');attack=next(i for i,x in enumerate(s) if x['op']=='attack')
                self.assertEqual(s[0],dict(op='turn',player=owner,value=0));self.assertLess(cast,advance);self.assertLess(advance,attack)
                self.assertIn(dict(op='untap_step',player=1-owner),s[advance:attack]);self.assertEqual(s[attack]['ref'],'flying_attacker')
                self.assertIn(dict(op='block',ref='source',other='flying_attacker',value=1),s)
                self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='flying_attacker'),'Suntail Hawk')

    def test_flying_freeze_sources_wait_for_blocker_second_real_untap_before_attacking(self):
        for n in ['Spire Patrol','Chillbringer']:
            for owner in (0,1):
                for mode in ['flying_evasion','flying_reach']:
                    s=self.case(n,mode,owner)['steps'];attack=next(i for i,x in enumerate(s) if x['op']=='attack');untaps=[(i,x['player']) for i,x in enumerate(s[:attack]) if x['op']=='untap_step']
                    self.assertEqual([player for i,player in untaps],[1-owner,owner,1-owner,owner])
                    ref=f'creature_{1-owner}';self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=1),s[untaps[0][0]:untaps[1][0]])
                    self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=0),s[untaps[2][0]:attack])


if __name__=='__main__':unittest.main()
