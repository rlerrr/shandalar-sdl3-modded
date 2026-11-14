import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand as public_expand,cost_from_oracle
import death_paid_value_recipes as family


class DeathPaidValueTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()};cls.data=family.pins()

    def declaration(self,name,variants):
        return dict(family=family.FAMILY,version=2,params=dict(schema=1,**family.oracle_model(self.oracle[name])),oracle_text=self.oracle[name]['oracle_text'],variants=variants)

    def case(self,name,mode='effect',player=0):
        d=self.declaration(name,[f'{mode}_{player}'])
        return public_expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,self.oracle)[0]

    def has(self,c,**fields):return any(all(s.get(k)==v for k,v in fields.items()) for s in c['steps'])

    def zone(self,c,name,player,zone,value):
        return self.has(c,op='assert_zone_multiplicity',index=self.rows[name]['csvid'],player=player,zone=zone,value=value)

    def test_whole_fixed_grammar_and_typed_declarations_fail_closed(self):
        for name in self.data['sources']:
            d=self.declaration(name,['effect_0']);family.spec(d,self.oracle[name])
            for field,value in [('schema',True),('amount',True),('not_sacrificed',0),('target_player',1)]:
                bad=copy.deepcopy(d);bad['params'][field]=value
                with self.assertRaises(ValueError):family.spec(bad,self.oracle[name])
            for text in (self.oracle[name]['oracle_text']+'\nFlying',self.oracle[name]['oracle_text']+' Draw a card.'):
                with self.assertRaises(ValueError):family.oracle_model(dict(self.oracle[name],oracle_text=text))
        for name in ('Vigil for the Lost','Nim Deathmantle','Flight Spellbomb','Auspicious Ancestor'):
            with self.assertRaises(ValueError):family.oracle_model(self.oracle[name])
        for name in ('Tablet of Epityr','Inheritance'):
            family.spec(self.declaration(name,['effect_0']),self.oracle[name])

    def test_all_full_pins_and_real_primary_handlers(self):
        family.validate_witnesses(self.oracle,self.rows)
        self.assertEqual(len(self.data['sources']),8)
        for name in ('Memnite','Child of Night','Safe Passage','Murder'):
            o=copy.deepcopy(self.oracle);o[name]['oracle_text']+='changed'
            with self.assertRaises(ValueError):family.validate_witnesses(o,self.rows)
        rows=copy.deepcopy(self.rows);rows['Tenacious Dead']['handler']='card_dummy'
        with self.assertRaises(ValueError):family.validate_witnesses(self.oracle,rows)

    def test_every_variant_public_router_without_rules_seeding(self):
        total=0
        for name,source in self.data['sources'].items():
            d=self.declaration(name,['effect_0']);m=family.spec(d,self.oracle[name]);d['variants']=family.variants(m)
            cases=public_expand(name,source['csvid'],d,self.oracle[name],self.rows,self.oracle);total+=len(cases)
            for c in cases:
                self.assertFalse(any(s['op'] in('destroy','move_card','set_life','set_counter','dispatch_gfp') for s in c['steps']))
                self.assertTrue(self.has(c,op='assert_unused_choices',value=0));self.assertTrue(self.has(c,op='assert_choices_consumed'))
                self.assertEqual(c['_recipe']['scope_gaps'],family.GAPS)
        self.assertEqual(total,594)

    def test_payment_vectors_come_from_complete_oracle_costs(self):
        for name in self.data['sources']:
            d=self.declaration(name,['effect_0']);m=family.spec(d,self.oracle[name])
            self.assertEqual(m['mana'],cost_from_oracle(self.oracle[name]))
            self.assertEqual(m['trigger_mana'],cost_from_oracle(dict(mana_cost=m['payment'])))
            for p in(0,1):
                c=self.case(name,player=p)
                self.assertTrue(self.has(c,op='cast',ref='death_spell',value=1))
                self.assertTrue(self.zone(c,'Murder',1-p,'graveyard',1))
                self.assertTrue(all(self.has(c,op='assert_mana',player=p,color=i,value=0) for i in range(6)))

    def test_actual_optional_live_dead_and_mandatory_presentations(self):
        c=self.case('Soul Net');self.assertTrue(self.has(c,op='choose_trigger_source',ref='source',player=0))
        c=self.case("Urza's Miter",'source_only');self.assertTrue(self.has(c,op='choose_trigger',ref='source',player=0))
        c=self.case('Drainpipe Vermin');self.assertTrue(self.has(c,op='choose_trigger',ref='source',player=0))
        c=self.case('Sigil of the New Dawn');self.assertFalse(any(s['op'].startswith('choose_trigger') for s in c['steps']))
        self.assertTrue(self.has(c,op='choose_zone_card',player=0,value=self.rows['Grizzly Bears']['csvid']))
        for name in self.data['sources']:
            c=self.case(name,player=1);self.assertFalse(any(s['op'].startswith('choose_trigger') for s in c['steps']))
        c=self.case('Soul Net','decline');self.assertTrue(self.has(c,op='choose_trigger_source',ref='source',player=0))
        self.assertTrue(self.has(c,op='choose_cancel_mana',player=0))
        c=self.case('Sigil of the New Dawn','decline');self.assertTrue(self.has(c,op='choose_optional_zone',player=0,value=self.rows['Grizzly Bears']['csvid']))

    def test_generic_source_and_trigger_real_mana_clicks_and_cancellation(self):
        for name in ('Soul Net',"Urza's Miter"):
            c=self.case(name,'generic_source_colored');n=int(self.oracle[name]['cmc'])
            self.assertEqual(sum(s['op']=='choose_mana' for s in c['steps']),n)
            c=self.case(name,'generic_trigger_colored');cost=1 if name=='Soul Net' else 3
            self.assertEqual(sum(s['op']=='choose_mana' for s in c['steps']),cost)
            self.assertTrue(self.has(c,op='assert_mana',player=0,color=3,value=0))
            c=self.case(name,'payment_cancel');self.assertTrue(self.has(c,op='choose_cancel_mana',player=0))
            self.assertTrue(self.has(c,op='assert_mana',player=0,color=3,value=cost))
            self.assertTrue(self.has(c,op='assert_life',player=0,value=20))

    def test_per_death_payment_and_budget_limited_partial_effects(self):
        for p in(0,1):
            for mode,n in(('two_deaths',2),('one_payment',1)):
                c=self.case('Soul Net',mode,p);self.assertTrue(self.has(c,op='assert_life',player=p,value=20+n))
                c=self.case("Urza's Miter",mode,p)
                self.assertTrue(self.zone(c,'Forest',p,'hand',1))
                self.assertTrue(self.zone(c,'Plains',p,'hand',2 if n==2 else 1))
                c=self.case('Kalastria Highborn',mode,p)
                self.assertTrue(self.has(c,op='assert_life',player=p,value=20+2*n))
                self.assertTrue(self.has(c,op='assert_life',player=1-p,value=20-2*n))
                self.assertEqual(sum(s['op']=='choose_player' for s in c['steps']),n)
                c=self.case('Sigil of the New Dawn',mode,p)
                self.assertTrue(self.zone(c,'Grizzly Bears',p,'hand',1+n))

    def test_miter_not_sacrificed_includes_noncreatures_and_nonsacrifice_deaths(self):
        for p in(0,1):
            c=self.case("Urza's Miter",'artifact_death',p)
            self.assertTrue(self.has(c,op='cast',ref='death_spell',value=1))
            self.assertTrue(any(x['name']=='Naturalize' and x['ref']=='death_spell' for x in c['setup']))
            self.assertTrue(self.zone(c,'Forest',p,'hand',1))
            c=self.case("Urza's Miter",'zero_toughness',p)
            self.assertTrue(any(x['name']=='Disfigure' for x in c['setup']));self.assertTrue(self.zone(c,'Forest',p,'hand',1))
            c=self.case("Urza's Miter",'sacrifice',p)
            self.assertTrue(self.has(c,op='activate',ref='altar',value=1))
            self.assertTrue(self.zone(c,'Forest',p,'hand',0))
            self.assertTrue(self.has(c,op='assert_mana',player=p,color=0,value=5))

    def test_vampire_exact_type_and_loss_ignores_real_damage_prevention(self):
        for p in(0,1):
            c=self.case('Kalastria Highborn','changeling',p)
            self.assertTrue(self.has(c,op='assert_life',player=1-p,value=18))
            c=self.case('Kalastria Highborn','nonmatching',p)
            self.assertTrue(self.has(c,op='assert_life',player=1-p,value=20))
            c=self.case('Kalastria Highborn','damage_prevented',p)
            self.assertTrue(any(x['name']=='Safe Passage' for x in c['setup']))
            self.assertTrue(self.has(c,op='assert_life',player=p,value=22))
            self.assertTrue(self.has(c,op='assert_life',player=1-p,value=18))
            c=self.case('Kalastria Highborn','target_self',p)
            self.assertTrue(self.has(c,op='choose_player',player=p,value=p))
            self.assertTrue(self.has(c,op='assert_life',player=p,value=20))

    def test_discard_target_chooses_own_real_hand_and_empty_hand_still_pays(self):
        for p in(0,1):
            c=self.case('Drainpipe Vermin',player=p)
            self.assertTrue(self.has(c,op='choose_player',player=p,value=1-p))
            choices=[s for s in c['steps'] if s['op']=='choose_zone_card']
            self.assertEqual(len(choices),int(p==1))
            if choices:self.assertEqual(choices[0]['player'],0)
            self.assertTrue(self.zone(c,'Grizzly Bears',1-p,'hand',0))
            self.assertTrue(self.zone(c,'Grizzly Bears',1-p,'graveyard',2))
            c=self.case('Drainpipe Vermin','empty_target_hand',p)
            self.assertFalse(self.has(c,op='choose_zone_card'))
            self.assertTrue(self.has(c,op='choose_player',player=p,value=1-p))
            self.assertTrue(self.has(c,op='assert_mana',player=p,color=1,value=0))

    def test_sigil_your_graveyard_is_owner_receipt_not_controller(self):
        for p in(0,1):
            c=self.case('Sigil of the New Dawn','stolen_death',p)
            self.assertFalse(self.has(c,op='choose_zone_card'))
            self.assertTrue(self.zone(c,'Grizzly Bears',1-p,'graveyard',2))
            c=self.case('Sigil of the New Dawn','lent_death',p)
            self.assertTrue(self.has(c,op='choose_zone_card',player=p,value=self.rows['Grizzly Bears']['csvid']))
            self.assertTrue(self.zone(c,'Grizzly Bears',p,'hand',2))
            self.assertTrue(self.zone(c,'Grizzly Bears',p,'graveyard',1))
            c=self.case('Sigil of the New Dawn','recast_returned',p)
            self.assertTrue(self.has(c,op='bind_hand_copy',ref='recast',player=p,value=self.rows['Grizzly Bears']['csvid'],index=1))
            self.assertTrue(self.has(c,op='cast',ref='recast',value=1))

    def test_tenacious_owner_tapped_new_object_second_death_and_actual_untap(self):
        for p in(0,1):
            c=self.case('Tenacious Dead','source_foreign_owner',p)
            self.assertTrue(self.has(c,op='bind_permanent_copy',ref='returned_1',player=1-p,value=self.rows['Tenacious Dead']['csvid']))
            self.assertTrue(self.has(c,op='assert_card',ref='returned_1',property='tapped',value=1))
            self.assertTrue(self.has(c,op='assert_card',ref='returned_1',property='sick',value=1))
            c=self.case('Tenacious Dead','new_object',p)
            self.assertTrue(any(x['name']=='Burst of Strength' for x in c['setup']))
            self.assertTrue(self.has(c,op='assert_card',ref='source',property='plus1_counters',value=1))
            self.assertTrue(self.has(c,op='assert_card',ref='returned_1',property='plus1_counters',value=0))
            c=self.case('Tenacious Dead','second_death',p)
            self.assertTrue(self.has(c,op='bind_permanent_copy',ref='returned_2',player=p,value=self.rows['Tenacious Dead']['csvid']))
            self.assertTrue(self.zone(c,'Murder',1-p,'graveyard',2))
            c=self.case('Tenacious Dead','return_untap',p)
            self.assertTrue(self.has(c,op='untap_step',player=p));self.assertTrue(self.has(c,op='attack',ref='returned_1',value=1))
            self.assertTrue(self.has(c,op='assert_life',player=1-p,value=19))

    def test_token_return_controls_explicitly_decline_but_do_not_seed_mv_zero(self):
        for name in ('Tenacious Dead','Sigil of the New Dawn'):
            for p in(0,1):
                c=self.case(name,'token_death',p)
                self.assertTrue(any('token' in x.get('state',[]) for x in c['setup']))
                self.assertFalse(self.has(c,op='bind_permanent_copy',ref='returned_1'))
                self.assertFalse(self.has(c,op='choose_zone_card'))
                self.assertIn('paid acceptance after a token corpse has ceased to exist for return effects',c['_recipe']['scope_gaps'])

    def test_no_death_bounce_exile_and_countered_source_never_pay(self):
        for name in self.data['sources']:
            for mode in('victim_bounced','victim_exiled','countered','trigger_unaffordable'):
                c=self.case(name,mode)
                if name in('Soul Net','Tablet of Epityr') and mode=='trigger_unaffordable':
                    self.assertTrue(self.has(c,op='choose_trigger_source',ref='source',player=0))
                else:self.assertFalse(any(s['op'].startswith('choose_trigger') for s in c['steps']))
                self.assertFalse(self.has(c,op='choose_player'))
            c=self.case(name,'countered')
            self.assertEqual([s['op'] for s in c['steps'] if s['op'] in('cast','announce_spell','cast_response','resolve_response')],
                             ['cast','announce_spell','cast_response','resolve_response'])

    def test_real_sweep_phase_and_sacrifice_selection_protocols(self):
        for name in ('Soul Net',"Urza's Miter",'Kalastria Highborn','Sigil of the New Dawn'):
            for p in(0,1):
                c=self.case(name,'two_deaths',p);s=c['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='sweep')
                lastturn=next(x for x in reversed(s[:cast]) if x['op']=='turn')
                self.assertEqual(lastturn['player'],1-p)
        for name in self.data['sources']:
            for p in(0,1):
                c=self.case(name,'sacrifice',p)
                self.assertEqual(sum(s['op']=='choose_target' for s in c['steps']),int(p==0))
                board=[x for x in c['setup'] if x['zone']=='battlefield' and 'Creature' in self.oracle[x['name']]['type_line']]
                self.assertLessEqual(len(board),1)

    def test_snapshots_replay_actual_choices_and_never_inspect_seeded_zone_refs(self):
        for name in self.data['sources']:
            for mode in('ready_snapshot','pending_snapshot','death_snapshot','resolved_snapshot'):
                c=self.case(name,mode)
                self.assertTrue(self.has(c,op='capture'));self.assertTrue(self.has(c,op='restore'));self.assertTrue(self.has(c,op='assert_recorded_state'))
                refs={x['ref'] for x in c['setup'] if x['zone'] in('library','graveyard','exile')}
                self.assertFalse(any(s['op'] in('assert_card','require_card') and s['ref'] in refs for s in c['steps']))

    def test_four_corrected_fixtures_keep_actual_lifelink_and_legal_payment_routes(self):
        for p in(0,1):
            c=self.case('Kalastria Highborn','combat_death',p)
            self.assertTrue(any(x['name']=='Child of Night' and x['ref']=='victim' for x in c['setup']))
            self.assertIn('Lifelink',self.oracle['Child of Night']['keywords'])
            self.assertTrue(self.has(c,op='assert_card',ref='blocker',property='damage',value=2))
            self.assertTrue(self.has(c,op='assert_life',player=p,value=24))
            self.assertTrue(self.has(c,op='assert_life',player=1-p,value=18))
        c=self.case('Soul Net','decline');s=c['steps']
        select=next(i for i,x in enumerate(s) if x['op']=='choose_trigger_source')
        self.assertEqual(s[select+1],dict(op='choose_cancel_mana',player=0))
        self.assertTrue(self.has(c,op='assert_life',player=0,value=20))
        self.assertTrue(self.has(c,op='assert_mana',player=0,color=3,value=1))
        c=self.case('Soul Net','trigger_unaffordable')
        self.assertTrue(self.has(c,op='choose_trigger_source',ref='source',player=0))
        self.assertFalse(self.has(c,op='choose_cancel_mana'))
        self.assertFalse(self.has(c,op='choose_mana'))
        self.assertTrue(self.has(c,op='assert_life',player=0,value=20))
        self.assertTrue(all(self.has(c,op='assert_mana',player=0,color=i,value=0) for i in range(6)))

    def test_sigil_departure_uses_real_optional_cancel_and_zero_ghost_offer_receipt(self):
        for p in(0,1):
            for mode in('source_only','source_removed'):
                c=self.case('Sigil of the New Dawn',mode,p);s=c['steps']
                observer=next(i for i,x in enumerate(s) if x['op']=='observe_search_filters')
                self.assertEqual(s[observer:observer+6],[dict(op='observe_search_filters',value=1),
                    dict(op='choose_optional_zone',player=p,value=self.rows['Grizzly Bears']['csvid']),dict(op='resolve'),dict(op='refresh'),
                    dict(op='assert_unused_choices',value=0),dict(op='assert_offered_count',value=0)])
                self.assertFalse(self.has(c,op='assert_unused_choices',value=1))
                self.assertFalse(self.has(c,op='choose_zone_card'))
                self.assertTrue(self.has(c,op='assert_mana',player=p,color=0,value=1))
                self.assertTrue(self.has(c,op='assert_mana',player=p,color=5,value=1))
            c=self.case('Sigil of the New Dawn','source_simultaneous',p);s=c['steps']
            observer=next(i for i,x in enumerate(s) if x['op']=='observe_search_filters')
            self.assertEqual(s[observer:observer+6],[dict(op='observe_search_filters',value=1),
                dict(op='choose_zone_card',player=p,value=self.rows['Grizzly Bears']['csvid']),dict(op='resolve'),dict(op='refresh'),
                dict(op='assert_unused_choices',value=0),dict(op='assert_offered_legal_card',index=self.rows['Grizzly Bears']['csvid'],value=1)])
            self.assertFalse(self.has(c,op='choose_optional_zone'))
            self.assertTrue(self.zone(c,'Grizzly Bears',p,'hand',2))
            self.assertTrue(self.has(c,op='assert_mana',player=p,color=0,value=0))
            self.assertTrue(self.has(c,op='assert_mana',player=p,color=5,value=0))
        for name in self.data['sources']:
            if name=='Sigil of the New Dawn':continue
            self.assertFalse(self.has(self.case(name),op='observe_search_filters'))

    def test_follower_paid_values_keep_global_creature_and_controlled_artifact_distinct(self):
        for p in(0,1):
            c=self.case('Inheritance','opponent_death',p)
            self.assertTrue(self.zone(c,'Forest',p,'hand',1))
            self.assertFalse(any(s['op'].startswith('choose_trigger') for s in c['steps']))
            c=self.case('Tablet of Epityr','opponent_death',p)
            self.assertTrue(self.has(c,op='assert_life',player=p,value=20))
            self.assertTrue(self.has(c,op='assert_mana',player=p,color=0,value=1))
            c=self.case('Tablet of Epityr','artifact_death',p)
            self.assertTrue(any(x['name']=='Naturalize' and x['ref']=='death_spell' for x in c['setup']))
            self.assertTrue(self.has(c,op='assert_life',player=p,value=21))
            c=self.case('Tablet of Epityr','sacrifice',p)
            self.assertTrue(self.has(c,op='assert_life',player=p,value=21))
            self.assertTrue(self.has(c,op='assert_mana',player=p,color=0,value=2))
            c=self.case('Tablet of Epityr','source_only',p)
            self.assertFalse(any(s['op'].startswith('choose_trigger') for s in c['steps']))
            self.assertTrue(self.has(c,op='assert_life',player=p,value=21))

    def test_follower_enrollment_uses_actual_payment_cancel_not_unpiloted_outer_pass(self):
        for name in ('Inheritance','Tablet of Epityr'):
            cs=self.rows[name]['csvid'];path=Path(__file__).parent/f'data/cases/cards/{cs//100:03}/{cs:05}-{name.lower().replace(" ","-")}.json'
            home=json.loads(path.read_text(encoding='utf8'));variants=home['recipes'][0]['variants']
            self.assertEqual(len(variants),74);self.assertNotIn('decline_0',variants);self.assertIn('payment_cancel_0',variants)
            self.assertEqual(self.data['follow_enrollment_notes'][name]['covered_decline'],'payment_cancel_0')
            c=self.case(name,'payment_cancel');cost=3 if name=='Inheritance' else 1
            self.assertTrue(self.has(c,op='choose_cancel_mana',player=0))
            self.assertTrue(self.has(c,op='assert_mana',player=0,color=3,value=cost))
            self.assertTrue(self.has(c,op='assert_life',player=0,value=20))
            self.assertTrue(self.zone(c,'Forest',0,'hand',0))
            self.assertEqual(self.has(c,op='choose_trigger_source',ref='source',player=0),name=='Tablet of Epityr')

    def test_tablet_zero_budget_accepts_actual_live_trigger_without_payment(self):
        c=self.case('Tablet of Epityr','trigger_unaffordable');s=c['steps']
        cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='death_spell')
        selected=next(i for i,x in enumerate(s) if x['op']=='choose_trigger_source')
        self.assertGreater(selected,cast)
        self.assertEqual(s[selected],dict(op='choose_trigger_source',ref='source',player=0))
        self.assertEqual(s[selected+1],dict(op='resolve'))
        self.assertFalse(any(x['op'] in('choose_mana','choose_cancel_mana','choose_payment','choose_dialog') for x in s))
        self.assertTrue(self.has(c,op='assert_life',player=0,value=20))
        self.assertTrue(all(self.has(c,op='assert_mana',player=0,color=i,value=0) for i in range(6)))
        self.assertTrue(self.zone(c,'Memnite',0,'graveyard',1))
        self.assertFalse(self.has(self.case('Tablet of Epityr','trigger_unaffordable',1),op='choose_trigger_source'))


if __name__=='__main__':unittest.main()
