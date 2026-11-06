import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from library_search_recipes import eligible,variants,PINS


class LibrarySearchRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

    def declaration(self,name='Goblin Matron',selected=None):
        configs={
          'Goblin Matron':dict(scope='goblin'),
          'Avarax':dict(scope='named',names=['Avarax'],keywords=['Haste']),
          'Blightspeaker':dict(mode='activated',scope='rebel_permanent',limit=dict(field='mana_value',op='le',value=3),destination='battlefield',optional=False,tap=True,activation_cost='{4}'),
          'Trinket Mage':dict(scope='artifact',limit=dict(field='mana_value',op='le',value=1)),
          'Fierce Empath':dict(scope='creature',limit=dict(field='mana_value',op='ge',value=6)),
          'Recruiter of the Guard':dict(scope='creature',limit=dict(field='toughness',op='le',value=2)),
          'Imperial Recruiter':dict(scope='creature',limit=dict(field='power',op='le',value=2),optional=False),
          'Wood Elves':dict(scope='forest',destination='battlefield',optional=False),
          'Enlightened Tutor':dict(mode='spell',scope='artifact_or_enchantment',destination='top',optional=False),
          'Demonic Tutor':dict(mode='spell',scope='card',optional=False),
          'Vampiric Tutor':dict(mode='spell',scope='card',destination='top',optional=False,lose_life=2),
          'Captain Sisay':dict(mode='activated',scope='legendary_card',optional=False,tap=True),
          'Planar Bridge':dict(mode='activated',scope='permanent',destination='battlefield',optional=False,tap=True,activation_cost='{8}'),
          'Thalia\'s Lancers':dict(scope='legendary_card',keywords=['First strike']),
        }
        p=dict(mode='entry',scope='card',limit=None,destination='hand',optional=True,activation_cost='',tap=False,sacrifice=False,pay_life=0,keywords=[],names=[],lose_life=0);p.update(configs[name])
        d=dict(family='library_search',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['search_0'])
        if p['mode']=='entry' and p['optional']:d['trigger_priority']=True
        return d

    def cases(self,name='Goblin Matron',selected=None,d=None,oracle=None):
        return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],self.rows,oracle_context=oracle or self.oracle)

    def test_full_clause_and_parameter_types_fail_closed(self):
        d=self.declaration()
        for patch in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],optional=1)),dict(params=dict(d['params'],scope=[])),dict(params=dict(d['params'],scope='elf')),dict(params=dict(d['params'],keywords={})),dict(params=dict(d['params'],limit=dict(field='power',op='le',value=True)))):
            with self.assertRaises(ValueError):self.cases(d=dict(d,**patch))
        r=dict(self.oracle['Goblin Matron'],oracle_text=d['oracle_text']+'\nWhen this creature dies, draw a card.')
        with self.assertRaises(ValueError):explicit_spec(dict(d,oracle_text=r['oracle_text']),r)

    def test_subtype_search_includes_noncreature_kindred_and_changelings(self):
        p=explicit_spec(self.declaration(),self.oracle['Goblin Matron'])
        for nm in ("Mons's Goblin Raiders",'Tarfire','Crib Swap','Avian Changeling'):self.assertTrue(eligible(self.oracle[nm],p),nm)
        self.assertFalse(eligible(self.oracle['Grizzly Bears'],p))

    def test_mana_limits_are_inclusive_and_intersect_with_type(self):
        p=explicit_spec(self.declaration('Trinket Mage'),self.oracle['Trinket Mage'])
        for nm,allowed in [('Ornithopter',True),('Sol Ring',True),('Darksteel Citadel',True),('Vulshok Morningstar',False),('Giant Growth',False)]:self.assertEqual(eligible(self.oracle[nm],p),allowed,nm)
        p=explicit_spec(self.declaration('Fierce Empath'),self.oracle['Fierce Empath'])
        self.assertTrue(eligible(self.oracle['Phyrexian Hulk'],p));self.assertFalse(eligible(self.oracle['Air Elemental'],p));self.assertFalse(eligible(self.oracle['Gilded Lotus'],p))

    def test_power_and_toughness_limits_are_distinct(self):
        a=explicit_spec(self.declaration('Imperial Recruiter'),self.oracle['Imperial Recruiter']);b=explicit_spec(self.declaration('Recruiter of the Guard'),self.oracle['Recruiter of the Guard'])
        self.assertTrue(eligible(self.oracle['Kraken Hatchling'],a));self.assertFalse(eligible(self.oracle['Kraken Hatchling'],b))

    def test_unrestricted_search_requires_finding_but_quality_search_may_decline(self):
        a=explicit_spec(self.declaration('Demonic Tutor'),self.oracle['Demonic Tutor']);b=explicit_spec(self.declaration(),self.oracle['Goblin Matron'])
        self.assertNotIn('decline_0',variants(a));self.assertNotIn('no_match_0',variants(a));self.assertIn('decline_0',variants(b))
        s=self.cases('Demonic Tutor')[0]['steps'];self.assertIn(dict(op='assert_offered_selection_required',value=1),s)
        self.assertIn(dict(op='assert_offered_selection_required',value=0),self.cases()[0]['steps'])

    def test_top_tutors_preserve_library_quantity_and_put_chosen_card_on_top(self):
        c=self.cases('Enlightened Tutor')[0];s=c['steps']
        self.assertTrue(any(x['op']=='assert_zone_card' and x['player']==0 and x['zone']=='library' and x['index']==0 for x in s))
        self.assertFalse(any(x['op']=='bind_permanent_copy' for x in s))

    def test_spell_life_loss_is_not_an_activation_cost_or_countered_effect(self):
        s=self.cases('Vampiric Tutor')[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=18),s)
        s=self.cases('Vampiric Tutor',['countered_0'])[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=20),s)

    def test_entry_tutor_is_cast_and_stays_on_battlefield(self):
        s=self.cases()[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=1),s)
        self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),s)
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=1),s)
        self.assertFalse(any(x['op']=='activate' and x['ref']=='source' for x in s))

    def test_reentry_library_copies_are_provisioned_before_execution(self):
        for v in ('bounce_recast_0','blink_1'):
            c=self.cases(selected=[v])[0];source_choices=[x for x in c['steps'] if x['op']=='choose_zone_card']
            self.assertEqual(len(source_choices),2)
            first=source_choices[0]['value'];count=sum(x['zone']=='library' and x['player']==int(v[-1]) and self.rows[x['name']]['csvid']==first for x in c['setup'])
            self.assertGreaterEqual(count,2)

    def test_destroying_source_does_not_remove_the_fetched_hand_card(self):
        s=self.cases(selected=['destroy_source_1'])[0]['steps'];self.assertIn(dict(op='destroy',ref='source'),s)
        self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x['zone']=='graveyard' and x['index']==self.rows['Goblin Matron']['csvid'] and x['value']==1 for x in s))

    def test_namesake_recast_binds_a_copy_when_search_already_filled_hand(self):
        s=self.cases('Avarax',['bounce_recast_0'])[0]['steps']
        self.assertIn(dict(op='bind_hand_copy',player=0,value=self.rows['Avarax']['csvid'],index=0,ref='source'),s)
        self.assertEqual(sum(x['op']=='cast' and x['ref']=='source' for x in s),2)
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Avarax']['csvid'],value=2),s)

    def test_other_free_ability_does_not_imply_unaffordable_tutor_is_available(self):
        p=explicit_spec(self.declaration('Blightspeaker'),self.oracle['Blightspeaker'])
        self.assertNotIn('insufficient_mana_0',variants(p))
        self.assertIn('tapped_0',variants(p))
        self.assertIn('payment_0',variants(p))

    def test_legendary_activated_source_does_not_create_illegal_duplicate(self):
        p=explicit_spec(self.declaration('Captain Sisay'),self.oracle['Captain Sisay']);self.assertNotIn('two_sources_0',variants(p));self.assertIn('repeat_0',variants(p))

    def test_battlefield_palette_defers_aura_attachment_but_filter_still_sees_auras(self):
        c=self.cases('Planar Bridge')[0]
        self.assertTrue(any(x['name']=='Pacifism' and x['zone']=='library' for x in c['setup']))
        self.assertIn(dict(op='assert_offered_legal_card',index=self.rows['Pacifism']['csvid'],value=1),c['steps'])
        # The friendly Grizzly Bears already occupies copy zero.
        self.assertIn(dict(op='bind_permanent_copy',player=0,value=self.rows['Grizzly Bears']['csvid'],index=1,ref='found_1'),c['steps'])

    def test_optional_trigger_and_menu_annotations_are_bounded(self):
        d=self.declaration()
        for patch in (dict(trigger_players=[True]),dict(trigger_players=[0,0]),dict(gate_menu=True),dict(entry_dialogs={'0':[-1]}),dict(activation_dialogs={'0':[0]})):
            with self.assertRaises(ValueError):self.cases(d=dict(d,**patch))

    def test_snapshot_and_controller_scopes(self):
        s=self.cases(selected=['pending_snapshot_1'])[0]['steps']
        for op in ('capture','record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertTrue(any(x['op']==op for x in s))
        self.assertIn(dict(op='turn',player=1,value=0),s)

    def test_pins_and_inputs_are_not_silently_mutated(self):
        d=self.declaration();old=copy.deepcopy(d);self.cases(d=d);self.assertEqual(d,old)
        o=copy.deepcopy(self.oracle);o['Crib Swap']['keywords']=[]
        with self.assertRaises(ValueError):self.cases(oracle=o)
        for vs in ([],['unknown_0'],['search_0','search_0']):
            with self.assertRaises(ValueError):self.cases(d=dict(d,variants=vs))


class LibrarySearchWave6Test(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from library_search_recipes import wave6_data
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()};cls.profiles=wave6_data()[0]['profiles']

    def declaration(self,name,variant='search_0'):
        return dict(family='library_search',version=2,params=copy.deepcopy(self.profiles[name]['params']),oracle_text=self.oracle[name]['oracle_text'],variants=[variant])

    def cases(self,name,variant='search_0',d=None,oracle=None):
        return expand(name,self.rows[name]['csvid'],d or self.declaration(name,variant),self.oracle[name],self.rows,oracle_context=oracle or self.oracle)

    def has(self,steps,**fields):return any(all(s.get(k)==v for k,v in fields.items()) for s in steps)
    def receipt(self,steps,name,player,zone,count):
        return self.has(steps,op='assert_zone_multiplicity',player=player,zone=zone,index=self.rows[name]['csvid'],value=count)

    def test_full_profile_and_pins_fail_closed(self):
        for name in self.profiles:
            d=self.declaration(name)
            for patch in ({'quantity':99},{'up_to':not d['params']['up_to']},{'witness_universe':[]},{'schema':True}):
                bad=copy.deepcopy(d);bad['params'].update(patch)
                with self.assertRaises(ValueError):self.cases(name,d=bad)
            bad=copy.deepcopy(d);bad['oracle_text']+='\nDraw a card.'
            with self.assertRaises(ValueError):self.cases(name,d=bad)
        oracle=copy.deepcopy(self.oracle);oracle['Azorius Guildgate']['type_line']='Land'
        with self.assertRaises(ValueError):self.cases('Circuitous Route',oracle=oracle)

    def test_boolean_aliases_cannot_impersonate_numeric_pins(self):
        for name in self.profiles:
            original=self.declaration(name)
            for key,value in original['params'].items():
                if type(value) is int and value in (0,1):
                    bad=copy.deepcopy(original);bad['params'][key]=bool(value)
                    with self.assertRaises(ValueError,msg=(name,key)):self.cases(name,d=bad)
        oracle=copy.deepcopy(self.oracle);oracle['Darksteel Citadel']['cmc']=False
        with self.assertRaises(ValueError):self.cases('Circuitous Route',oracle=oracle)

    def test_exact_untyped_quantity_and_short_library_destinations(self):
        for owner in (0,1):
            steps=self.cases('Final Parting',f'search_{owner}')[0]['steps']
            self.assertTrue(self.receipt(steps,'Craw Wurm',owner,'hand',1));self.assertTrue(self.receipt(steps,'Grizzly Bears',owner,'graveyard',1))
            self.assertTrue(self.has(steps,op='assert_offered_selection_required',value=1))
            steps=self.cases('Final Parting',f'single_available_{owner}')[0]['steps']
            self.assertEqual(sum(s['op']=='choose_zone_card' for s in steps),1)
            self.assertTrue(self.receipt(steps,'Craw Wurm',owner,'hand',1));self.assertTrue(self.receipt(steps,'Craw Wurm',owner,'graveyard',0))
            self.assertFalse(any(s['op']=='assert_offered_selection_required' for s in steps))
        p=explicit_spec(self.declaration('Final Parting'),self.oracle['Final Parting'])
        self.assertNotIn('decline_0',variants(p))

    def test_typed_quantity_can_decline_without_losing_cards(self):
        for owner in (0,1):
            steps=self.cases('Buried Alive',f'decline_{owner}')[0]['steps']
            self.assertFalse(any(s['op']=='choose_zone_card' for s in steps))
            self.assertTrue(self.has(steps,op='choose_optional_zone',player=owner))
            self.assertTrue(self.has(steps,op='assert_offered_selection_required',value=0))
            self.assertTrue(self.receipt(steps,'Craw Wurm',owner,'library',1))
            steps=self.cases('Buried Alive',f'search_{owner}')[0]['steps']
            self.assertEqual(sum(s['op']=='choose_zone_card' for s in steps),3)
            for n in ('Craw Wurm','Grizzly Bears','Hill Giant'):self.assertTrue(self.receipt(steps,n,owner,'graveyard',1))

    def test_basic_or_gate_union_and_actual_tapped_entry(self):
        from library_search_recipes import wave6_match
        p=self.profiles['Circuitous Route']['params']
        for n,expected in [('Forest',True),('Azorius Guildgate',True),('Darksteel Citadel',False),('Giant Growth',False)]:self.assertEqual(wave6_match(self.oracle[n],p),expected,n)
        for owner in (0,1):
            steps=self.cases('Circuitous Route',f'gate_only_{owner}')[0]['steps']
            self.assertTrue(self.has(steps,op='choose_zone_card',player=owner,value=self.rows['Azorius Guildgate']['csvid']))
            self.assertTrue(self.has(steps,op='assert_card',ref='found_0',property='tapped',value=1))

    def test_sacrifice_payment_precedes_pending_library_receipt(self):
        steps=self.cases('Armillary Sphere','pending_snapshot_1')[0]['steps']
        cut=next(i for i,s in enumerate(steps) if s['op']=='capture')
        self.assertTrue(self.has(steps[:cut],op='activate',ref='source',value=1))
        self.assertTrue(self.receipt(steps[:cut],'Armillary Sphere',1,'graveyard',1))
        self.assertTrue(self.has(steps[:cut],op='assert_zone',player=1,zone='library',value=12))
        self.assertTrue(self.has(steps,op='assert_recorded_state'))

    def test_pending_spell_retains_physical_hand_source(self):
        steps=self.cases('Buried Alive','pending_snapshot_0')[0]['steps']
        cut=next(i for i,s in enumerate(steps) if s['op']=='capture')
        self.assertTrue(self.receipt(steps[:cut],'Buried Alive',0,'hand',1))
        self.assertTrue(self.receipt(steps[:cut],'Buried Alive',0,'graveyard',0))
        self.assertTrue(self.receipt(steps[cut:],'Buried Alive',0,'graveyard',1))

    def test_countered_search_never_selects_or_fetches(self):
        for owner in (0,1):
            steps=self.cases('Grow from the Ashes',f'kicked_countered_{owner}')[0]['steps']
            self.assertFalse(any(s['op']=='choose_zone_card' for s in steps))
            self.assertTrue(self.has(steps,op='cast_response',ref='counter',value=1));self.assertTrue(self.has(steps,op='resolve_response'))
            self.assertTrue(self.receipt(steps,'Grow from the Ashes',owner,'graveyard',1))
            self.assertTrue(self.has(steps,op='assert_mana',player=owner,color=0,value=26))

    def test_kicker_changes_quantity_and_exact_payment(self):
        for owner in (0,1):
            ordinary=self.cases('Grow from the Ashes',f'payment_{owner}')[0]['steps']
            kicked=self.cases('Grow from the Ashes',f'kicked_payment_{owner}')[0]['steps']
            self.assertEqual(sum(s['op']=='choose_zone_card' for s in ordinary),1);self.assertEqual(sum(s['op']=='choose_zone_card' for s in kicked),2)
            self.assertTrue(self.has(kicked,op='choose_dialog',player=owner,value=0))
            self.assertTrue(self.has(kicked,op='assert_mana',player=owner,color=0,value=0))
            self.assertTrue(self.has(kicked,op='assert_card',ref='found_1',property='tapped',value=0))
            self.assertFalse(any(s['op']=='choose_dialog' for s in ordinary))

    def test_payment_denials_cannot_autotap_board_controls(self):
        for name in self.profiles:
            for owner in (0,1):
                for mode in ('insufficient_mana','payment'):
                    case=self.cases(name,f'{mode}_{owner}')[0]
                    producers=[c for c in case['setup'] if c['player']==owner and c['zone']=='battlefield' and c['name'] in ('Sol Ring','Forest')]
                    self.assertEqual(len(producers),0 if self.profiles[name]['params']['mode']=='entry' else 2)
                    for c in producers:self.assertIn('tapped',c.get('state',[]))

    def test_borrowed_creature_returns_to_original_owner(self):
        for owner in (0,1):
            self.assertFalse(any(s['op']=='choose_player' for s in self.cases('Bribery',f'search_{owner}')[0]['steps']))
            for mode,zone in [('borrowed_death','graveyard'),('borrowed_bounce','hand')]:
                steps=self.cases('Bribery',f'{mode}_{owner}')[0]['steps']
                self.assertTrue(self.receipt(steps,'Craw Wurm',1-owner,zone,1));self.assertTrue(self.receipt(steps,'Craw Wurm',owner,zone,0))
                self.assertTrue(self.has(steps,op='bind_permanent_copy',player=owner,value=self.rows['Craw Wurm']['csvid'],index=0,ref='found_0'))
            steps=self.cases('Bribery',f'target_shrouded_pending_{owner}')[0]['steps']
            self.assertTrue(self.has(steps,op='cast_response',ref='shield',value=1));self.assertFalse(any(s['op']=='choose_zone_card' for s in steps))

    def test_shuffle_receipts_are_scoped_to_new_scenarios(self):
        self.assertFalse(any(s['op']=='assert_shuffle_count' for s in self.cases('Bribery')[0]['steps']))
        for owner in (0,1):
            steps=self.cases('Bribery',f'shuffle_pending_snapshot_{owner}')[0]['steps']
            cut=next(i for i,s in enumerate(steps) if s['op']=='capture')
            self.assertTrue(self.has(steps[:cut],op='assert_shuffle_count',player=1-owner,value=0))
            self.assertTrue(self.has(steps[cut:],op='assert_shuffle_count',player=1-owner,value=1))
            self.assertTrue(self.has(steps[cut:],op='assert_shuffle_count',player=owner,value=0))

    def test_top_quantity_preserves_library_and_exact_chosen_order(self):
        for owner in (0,1):
            steps=self.cases('Congregation at Dawn',f'search_{owner}')[0]['steps']
            self.assertEqual(sum(s['op']=='choose_zone_card' for s in steps),3)
            for i,n in enumerate(('Hill Giant','Grizzly Bears','Craw Wurm')):
                self.assertTrue(self.has(steps,op='assert_zone_card',player=owner,zone='library',index=i,value=self.rows[n]['csvid']))
            self.assertTrue(self.has(steps,op='assert_zone',player=owner,zone='library',value=16))

    def test_resource_effects_survive_fail_find_but_not_countering(self):
        for owner in (0,1):
            for name,op,amount in [("Nissa's Renewal",'assert_life',27),('Attune with Aether','assert_energy',2)]:
                self.assertTrue(self.has(self.cases(name,f'no_match_{owner}')[0]['steps'],op=op,player=owner,value=amount))
                self.assertTrue(self.has(self.cases(name,f'countered_{owner}')[0]['steps'],op=op,player=owner,value=20 if op=='assert_life' else 0))

    def test_discard_is_an_actual_paid_cost_with_named_receipts(self):
        for owner in (0,1):
            steps=self.cases('Greenseeker',f'pending_snapshot_{owner}')[0]['steps']
            cut=next(i for i,s in enumerate(steps) if s['op']=='capture')
            discarded=min(('Divination','Giant Growth'),key=lambda n:self.rows[n]['csvid'])
            self.assertTrue(self.has(steps[:cut],op='discard_policy',value=1))
            self.assertTrue(self.receipt(steps[:cut],discarded,owner,'graveyard',1))
            denied=self.cases('Greenseeker',f'empty_hand_{owner}')[0]['steps']
            self.assertTrue(self.has(denied,op='require_can_activate',ref='source',value=0));self.assertTrue(self.has(denied,op='activate',ref='source',value=0))
        ai=self.cases('Greenseeker','search_1')[0]
        self.assertEqual([c['name'] for c in ai['setup'] if c['player']==1 and c['zone']=='hand'],['Giant Growth','Giant Growth'])
        human=self.cases('Greenseeker','search_0')[0]
        self.assertEqual([c['name'] for c in human['setup'] if c['player']==0 and c['zone']=='hand'],['Divination','Giant Growth'])

    def test_forest_quality_includes_nonbasic_and_sequential_declines_are_complete(self):
        from library_search_recipes import wave6_match
        p=self.profiles["Gaea's Bounty"]['params']
        for n,allowed in [('Forest',True),('Taiga',True),('Savannah',True),('Darksteel Citadel',False),('Island',False)]:self.assertEqual(wave6_match(self.oracle[n],p),allowed,n)
        for owner in (0,1):
            steps=self.cases('Fork in the Road',f'decline_{owner}')[0]['steps']
            self.assertEqual(sum(s['op']=='choose_optional_zone' for s in steps),2)

    def test_activated_source_can_really_be_cast_before_sacrifice(self):
        for owner in (0,1):
            case=self.cases('Armillary Sphere',f'source_cast_{owner}')[0]
            source=next(c for c in case['setup'] if c['ref']=='source')
            self.assertEqual(source['zone'],'hand')
            steps=case['steps'];self.assertTrue(self.has(steps,op='cast',ref='source',value=1));self.assertTrue(self.has(steps,op='activate',ref='source',value=1))
            self.assertTrue(self.has(steps,op='assert_mana',player=owner,color=0,value=26))

    def test_sole_found_card_can_be_assigned_to_graveyard(self):
        for owner in (0,1):
            for name,found in [('Final Parting','Craw Wurm'),('Fork in the Road','Forest')]:
                steps=self.cases(name,f'single_to_grave_{owner}')[0]['steps']
                decisions=[s['op'] for s in steps if s['op'] in ('choose_optional_zone','choose_zone_card')]
                self.assertEqual(decisions,['choose_optional_zone','choose_zone_card'])
                self.assertTrue(self.receipt(steps,found,owner,'graveyard',1));self.assertTrue(self.receipt(steps,found,owner,'hand',0))

    def test_required_total_has_an_observable_incomplete_search_probe(self):
        for owner in (0,1):
            for name,destination in [('Final Parting','hand'),('Entomb','graveyard')]:
                steps=self.cases(name,f'required_decline_{owner}')[0]['steps']
                self.assertTrue(self.has(steps,op='choose_optional_zone',player=owner,value=self.rows['Craw Wurm']['csvid']))
                self.assertTrue(self.has(steps,op='assert_forced_zone_choices',value=1))
                self.assertTrue(self.receipt(steps,'Craw Wurm',owner,destination,1));self.assertTrue(self.receipt(steps,'Craw Wurm',owner,'library',0))
            steps=self.cases('Final Parting',f'single_required_decline_{owner}')[0]['steps']
            self.assertEqual(sum(s['op']=='choose_optional_zone' for s in steps),2)
            self.assertTrue(self.receipt(steps,'Craw Wurm',owner,'graveyard',1));self.assertTrue(self.receipt(steps,'Craw Wurm',owner,'library',0))

    def test_threshold_and_mastery_use_actual_graveyard_boundaries(self):
        for owner in (0,1):
            for mode,count in [('threshold6',1),('threshold7',3),('threshold8',3),('threshold7_decline',0),('threshold7_partial',2)]:
                steps=self.cases('Far Wanderings',f'{mode}_{owner}')[0]['steps']
                self.assertEqual(sum(s['op']=='choose_zone_card' for s in steps),count)
            for mode,count in [('mastery1',2),('mastery2',3),('mastery3',3),('mastery2_partial',2)]:
                case=self.cases("Nissa's Pilgrimage",f'{mode}_{owner}')[0]
                self.assertEqual(sum(s['op']=='choose_zone_card' for s in case['steps']),count)
                self.assertTrue(self.has(case['steps'],op='assert_card',ref='found_0',property='tapped',value=1))
            case=self.cases('Far Wanderings',f'threshold6_{owner}')[0]
            self.assertEqual(sum(c['player']==owner and c['zone']=='graveyard' for c in case['setup']),6)

    def test_variable_quantities_count_only_own_matching_entities(self):
        for owner in (0,1):
            for name in ('Boundless Realms','Harvest Season','Traverse the Outlands'):
                for mode,count in [('context_zero',0),('context_three',3),('context_partial',2),('opposing_context',0)]:
                    steps=self.cases(name,f'{mode}_{owner}')[0]['steps']
                    self.assertEqual(sum(s['op']=='choose_zone_card' for s in steps),count,(name,mode))
                    self.assertTrue(self.has(steps,op='assert_shuffle_count',player=owner,value=1))
            self.assertEqual(sum(s['op']=='choose_zone_card' for s in self.cases('Traverse the Outlands',f'context_max_{owner}')[0]['steps']),6)
            self.assertFalse(any(s['op']=='choose_zone_card' for s in self.cases('Traverse the Outlands',f'context_zero_power_{owner}')[0]['steps']))

    def test_delirium_counts_distinct_types_and_enables_both_type_branches(self):
        for owner in (0,1):
            low=self.cases('Traverse the Ulvenwald',f'delirium3_{owner}')[0]['steps']
            self.assertTrue(self.receipt(low,'Forest',owner,'hand',1));self.assertTrue(self.receipt(low,'Craw Wurm',owner,'hand',0))
            for mode in ('delirium4','artifact_creature_types'):
                steps=self.cases('Traverse the Ulvenwald',f'{mode}_{owner}')[0]['steps']
                self.assertTrue(self.receipt(steps,'Craw Wurm',owner,'hand',1))
                self.assertTrue(self.has(steps,op='assert_offered_legal_card',index=self.rows['Darksteel Citadel']['csvid'],value=1))

    def test_undergrowth_has_real_mana_value_zero_and_exact_positive_boundaries(self):
        for owner in (0,1):
            for mode,found,too_high in [('creatures0','Slaughter Pact','Dark Ritual'),('creatures1','Dark Ritual','Black Knight'),('limit_boundary','Black Knight','Scathe Zombies'),('creatures3','Scathe Zombies','Gravedigger')]:
                steps=self.cases('Mausoleum Secrets',f'{mode}_{owner}')[0]['steps']
                self.assertTrue(self.receipt(steps,found,owner,'hand',1))
                self.assertTrue(self.has(steps,op='assert_offered_legal_card',index=self.rows[too_high]['csvid'],value=0))
                self.assertTrue(self.has(steps,op='assert_offered_legal_card',index=self.rows['Grizzly Bears']['csvid'],value=0))

    def test_basic_forest_is_a_conjunction_and_morbid_uses_real_deaths(self):
        from library_search_recipes import wave6_match
        p=self.profiles["Nissa's Pilgrimage"]['params']
        for n,allowed in [('Forest',True),('Snow-Covered Forest',True),('Taiga',False),('Snow-Covered Island',False)]:self.assertEqual(wave6_match(self.oracle[n],p),allowed,n)
        for owner in (0,1):
            for mode in ('own_creature_died','opposing_creature_died'):
                steps=self.cases('Caravan Vigil',f'{mode}_{owner}')[0]['steps']
                self.assertTrue(self.has(steps,op='reset_death_history'));self.assertTrue(self.has(steps,op='destroy'))
                self.assertTrue(self.has(steps,op='bind_permanent_copy',player=owner,value=self.rows['Forest']['csvid']))
            steps=self.cases('Caravan Vigil',f'artifact_died_{owner}')[0]['steps']
            self.assertTrue(self.receipt(steps,'Forest',owner,'hand',1));self.assertFalse(any(s['op']=='bind_permanent_copy' for s in steps))
        d=self.declaration('Caravan Vigil');d['variants']=['own_creature_died_0','search_1'];cases=self.cases('Caravan Vigil',d=d)
        self.assertTrue(self.receipt(cases[1]['steps'],'Forest',1,'hand',1))

    def test_typed_discard_cost_choices_and_query_bindings_are_explicit(self):
        for owner in (0,1):
            case=self.cases('Fauna Shaman',f'query_purity_{owner}')[0]
            steps=case['steps'];cost=self.rows['Grizzly Bears']['csvid']
            self.assertEqual(sum(s['op']=='require_can_activate' and s['value']==1 for s in steps),3)
            self.assertEqual(sum(s['op']=='choose_zone_card' and s['value']==cost for s in steps),5 if owner==1 else 1)
            self.assertTrue(self.receipt(steps,'Grizzly Bears',owner,'graveyard',1))
            untap=self.cases('Fauna Shaman',f'untap_{owner}')[0]['steps']
            first_resolve=next(i for i,s in enumerate(untap) if s['op']=='resolve')
            queries=[s for s in untap[:first_resolve] if s['op']=='choose_zone_card' and s['value']==cost]
            self.assertEqual(len(queries),int(owner==1))
            for name in ('Fauna Shaman','Survival of the Fittest',"Artificer's Intuition"):
                steps=self.cases(name,f'wrong_discard_{owner}')[0]['steps']
                self.assertTrue(self.has(steps,op='require_can_activate',ref='source',value=0))
                self.assertFalse(any(s['op']=='choose_zone_card' for s in steps))

    def test_artifact_cost_is_independent_of_searched_mana_value(self):
        for owner in (0,1):
            steps=self.cases("Artificer's Intuition",f'limit_boundary_{owner}')[0]['steps']
            self.assertTrue(self.receipt(steps,'Phyrexian Hulk',owner,'graveyard',1))
            self.assertTrue(self.receipt(steps,'Sol Ring',owner,'hand',1))
            self.assertTrue(self.has(steps,op='assert_offered_legal_card',index=self.rows['Darksteel Citadel']['csvid'],value=1))
            self.assertTrue(self.has(steps,op='assert_offered_legal_card',index=self.rows['Phyrexian Hulk']['csvid'],value=0))

    def test_survival_ai_preference_does_not_become_a_printed_type_expectation(self):
        steps=self.cases('Survival of the Fittest','payment_1')[0]['steps']
        self.assertTrue(self.receipt(steps,'Ornithopter',1,'hand',1))
        self.assertFalse(any(s['op']=='assert_offered_legal_card' for s in steps))
        steps=self.cases('Survival of the Fittest','opposing_turn_1')[0]['steps']
        self.assertTrue(self.receipt(steps,'Craw Wurm',1,'hand',1))
        self.assertTrue(self.has(steps,op='assert_offered_legal_card',index=self.rows['Craw Wurm']['csvid'],value=1))
        self.assertEqual(self.rows['Elvish Mystic']['csvid'],13284)

    def test_single_quantity_duplicate_control_keeps_one_real_copy(self):
        for owner in (0,1):
            steps=self.cases('Bribery',f'duplicate_available_{owner}')[0]['steps']
            self.assertTrue(self.receipt(steps,'Craw Wurm',1-owner,'library',1))
            self.assertEqual(sum(s['op']=='choose_zone_card' for s in steps),1)
        steps=self.cases('Burnished Hart','source_cast_1')[0]['steps']
        self.assertTrue(self.has(steps,op='cast',ref='source',value=1));self.assertTrue(self.has(steps,op='assert_card',ref='source',property='sick',value=1))
        self.assertTrue(self.has(steps,op='activate',ref='source',value=1))

    def test_targeted_library_exile_uses_library_owner_and_legal_player_input(self):
        for owner in (0,1):
            for mode,target in [('search',1-owner),('own_target',owner)]:
                steps=self.cases('Extract',f'{mode}_{owner}')[0]['steps']
                self.assertTrue(self.has(steps,op='choose_player',player=owner,value=target))
                self.assertTrue(self.receipt(steps,'Craw Wurm',target,'exile',1))
                self.assertTrue(self.receipt(steps,'Craw Wurm',1-target,'exile',0))
                self.assertTrue(self.has(steps,op='assert_shuffle_count',player=target,value=1))
            steps=self.cases('Denying Wind',f'search_{owner}')[0]['steps']
            self.assertEqual(sum(s['op']=='choose_zone_card' for s in steps),7)
            self.assertFalse(any(s['op']=='choose_zone_card' for s in self.cases('Denying Wind',f'decline_{owner}')[0]['steps']))

    def test_sacrament_paid_kicker_changes_three_to_fifteen_and_short_library(self):
        for owner in (0,1):
            for mode,count in [('search',3),('kicked_payment',15),('kicked_short',5)]:
                steps=self.cases('Sadistic Sacrament',f'{mode}_{owner}')[0]['steps']
                self.assertEqual(sum(s['op']=='choose_zone_card' for s in steps),count)
            steps=self.cases('Sadistic Sacrament',f'kicked_payment_{owner}')[0]['steps']
            player=next(i for i,s in enumerate(steps) if s['op']=='choose_player')
            kicker=next(i for i,s in enumerate(steps) if s['op']=='choose_dialog')
            self.assertLess(player,kicker)
            for color in range(6):self.assertTrue(self.has(steps,op='assert_mana',player=owner,color=color,value=0))

    def test_cap_pays_sacrifice_and_probes_real_shuffle_damage(self):
        for owner in (0,1):
            with self.assertRaises(ValueError):self.cases("Jester's Cap",f'two_selected_{owner}')
        for owner in (0,1):
            case=self.cases("Jester's Cap",f'shuffle_trigger_{owner}')[0]
            self.assertTrue(any(c['name']=='Psychogenic Probe' and c['zone']=='battlefield' for c in case['setup']))
            self.assertTrue(self.has(case['steps'],op='assert_life',player=1-owner,value=18))
            self.assertTrue(self.receipt(case['steps'],"Jester's Cap",owner,'graveyard',1))
            steps=self.cases("Jester's Cap",f'required_decline_{owner}')[0]['steps']
            self.assertEqual(sum(s['op']=='choose_optional_zone' for s in steps),3)
            self.assertTrue(self.has(steps,op='assert_forced_zone_choices',value=3))
            steps=self.cases("Jester's Cap",f'target_shrouded_pending_{owner}')[0]['steps']
            self.assertTrue(self.receipt(steps,"Jester's Cap",owner,'graveyard',1));self.assertFalse(any(s['op']=='choose_zone_card' for s in steps))
            self.assertTrue(self.has(steps,op='cast',ref='shield',value=1));self.assertFalse(any(s['op']=='cast_response' for s in steps))

    def test_acquire_has_noncreature_artifact_and_artifact_land_positive_receipts(self):
        for owner in (0,1):
            for mode,found in [('ordinary_artifact','Sol Ring'),('artifact_land','Darksteel Citadel')]:
                steps=self.cases('Acquire',f'{mode}_{owner}')[0]['steps']
                self.assertTrue(self.has(steps,op='bind_permanent_copy',player=owner,value=self.rows[found]['csvid']))
                self.assertFalse(any(s['op']=='choose_player' for s in steps))

    def test_ordered_categories_have_independent_qualified_slots(self):
        for owner in (0,1):
            for name,expected in [('Gem of Becoming',['Island','Swamp','Mountain']),('Conflux',['Dark Ritual','Divination','Giant Growth','Lightning Bolt','Holy Strength']),("Firemind's Foresight",['Cancel','Counterspell','Giant Growth'])]:
                steps=self.cases(name,f'search_{owner}')[0]['steps']
                actual=[s['value'] for s in steps if s['op']=='choose_zone_card']
                self.assertEqual(actual,[self.rows[n]['csvid'] for n in expected])
                steps=self.cases(name,f'decline_{owner}')[0]['steps']
                self.assertEqual(sum(s['op']=='choose_optional_zone' for s in steps),len(expected))
                self.assertTrue(self.has(steps,op='assert_shuffle_count',player=owner,value=1))
                steps=self.cases(name,f'skip_first_{owner}')[0]['steps']
                self.assertEqual(sum(s['op']=='choose_zone_card' for s in steps),len(expected)-1)

    def test_category_alternates_cover_real_nonbasic_and_multicolor_cards(self):
        for owner in (0,1):
            for name,found in [('Gem of Becoming',['Tundra','Underground Sea','Badlands']),('Conflux',['Slaughter Pact','Coiling Oracle','Grizzly Bears','Lightning Bolt','Crib Swap']),("Firemind's Foresight",['Crib Swap','Negate','Lightning Bolt'])]:
                steps=self.cases(name,f'alternate_categories_{owner}')[0]['steps']
                choices=[s['value'] for s in steps if s['op']=='choose_zone_card']
                self.assertEqual(choices,[self.rows[n]['csvid'] for n in found])
            steps=self.cases('Conflux',f'invalid_last_category_{owner}')[0]['steps']
            self.assertTrue(self.has(steps,op='choose_zone_card',player=owner,value=self.rows['Tundra']['csvid']))
            self.assertTrue(self.receipt(steps,'Tundra',owner,'hand',0));self.assertTrue(self.receipt(steps,'Tundra',owner,'library',1))

    def test_group_constraints_and_nested_numeric_pins_fail_closed(self):
        from library_search_recipes import wave6_match
        instant=dict(scope='instant',exact_value=3)
        for name,expected in [('Cancel',True),('Crib Swap',True),('Divination',False),('Grizzly Bears',False),('Slaughter Pact',False)]:self.assertEqual(wave6_match(self.oracle[name],instant),expected,name)
        white=dict(scope='color_card',color='W')
        self.assertTrue(wave6_match(self.oracle['Holy Strength'],white));self.assertFalse(wave6_match(self.oracle['Tundra'],white))
        d=self.declaration("Firemind's Foresight");d['params']['groups'][2]['exact_value']=True
        with self.assertRaises(ValueError):self.cases("Firemind's Foresight",d=d)

    def test_entry_branch_casts_real_source_and_uses_private_zones(self):
        for name in ('Djeru, With Eyes Open','Godo, Bandit Warlord','General Tazri','Aurochs Herd','Mwonvuli Beast Tracker'):
            for owner in (0,1):
                case=self.cases(name,f'search_{owner}')[0];steps=case['steps']
                self.assertTrue(self.has(steps,op='cast',ref='source',value=1))
                self.assertTrue(self.has(steps,op='assert_card',ref='source',property='in_play',value=1))
                self.assertFalse(any(c['name']=='Llanowar Elves' for c in case['setup']))
                self.assertTrue(self.has(steps,op='assert_shuffle_count',player=owner,value=1))

    def test_attempted_invalid_entry_candidate_can_only_be_legally_declined(self):
        for owner in (0,1):
            steps=self.cases('General Tazri',f'card_3_{owner}')[0]['steps']
            self.assertTrue(self.has(steps,op='choose_zone_card',player=owner,value=self.rows['Crib Swap']['csvid']))
            self.assertTrue(self.receipt(steps,'Crib Swap',owner,'hand',0))
            steps=self.cases('Aurochs Herd',f'card_3_{owner}')[0]['steps']
            self.assertTrue(self.has(steps,op='choose_zone_card',player=owner,value=self.rows['Crib Swap']['csvid']))
            self.assertTrue(self.receipt(steps,'Crib Swap',owner,'hand',1))

    def test_keyword_creature_union_and_subtype_intersection_are_independent(self):
        from library_search_recipes import wave6_match
        p=self.profiles['Mwonvuli Beast Tracker']['params']
        for n,allowed in [('Typhoid Rats',True),('Primal Huntbeast',True),('Giant Spider',True),('War Mammoth',True),('Craw Wurm',False),('Suntail Hawk',False),('Steadfast Guard',False),('Holy Strength',False)]:self.assertEqual(wave6_match(self.oracle[n],p),allowed,n)
        self.assertTrue(wave6_match(self.oracle['Avian Changeling'],self.profiles['General Tazri']['params']))
        self.assertFalse(wave6_match(self.oracle['Crib Swap'],self.profiles['General Tazri']['params']))
        self.assertTrue(wave6_match(self.oracle['Crib Swap'],self.profiles['Aurochs Herd']['params']))

    def test_optional_trigger_skip_and_accepted_fail_find_have_distinct_receipts(self):
        for owner in (0,1):
            god=self.cases('Godo, Bandit Warlord',f'decline_{owner}')[0]['steps']
            self.assertTrue(self.has(god,op='choose_dialog',player=owner,value=1));self.assertTrue(self.has(god,op='assert_shuffle_count',player=owner,value=0))
            djeru=self.cases('Djeru, With Eyes Open',f'decline_{owner}')[0]['steps']
            self.assertTrue(self.has(djeru,op='assert_shuffle_count',player=owner,value=1))
        for owner,count in [(0,0),(1,1)]:
            steps=self.cases('Forerunner of the Heralds',f'decline_{owner}')[0]['steps']
            self.assertTrue(self.has(steps,op='assert_shuffle_count',player=owner,value=count))

    def test_other_tribal_entry_context_is_explicitly_excluded(self):
        for name in ('Forerunner of the Coalition','Forerunner of the Empire','Forerunner of the Heralds','Forerunner of the Legion'):
            p=explicit_spec(self.declaration(name),self.oracle[name])
            self.assertNotIn('two_sources_0',variants(p));self.assertNotIn('two_sources_1',variants(p))
        d=self.declaration('Godo, Bandit Warlord');d['params']['entry_dialogs']['0'][0]=False
        with self.assertRaises(ValueError):self.cases('Godo, Bandit Warlord',d=d)


if __name__=='__main__':unittest.main()
