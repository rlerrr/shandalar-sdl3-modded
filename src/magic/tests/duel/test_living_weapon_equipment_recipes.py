"""Focused Living Weapon guards for true token generation and paid Equip."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import living_weapon_equipment_recipes as model


class LivingWeaponRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name,pin in model.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path, = (data / 'cases/cards').glob(f'*/{csvid:05d}-*.json')
            cls.homes[name] = json.loads(path.read_text(encoding='utf8'))['recipes'][0]

    def cases(self,name,*selected):
        declaration = self.homes[name]
        shape = model.spec(declaration,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],declaration,shape,selected,self.rows,self.oracle)

    def steps(self,name,variant):
        return self.cases(name,variant)[0]['steps']

    def values(self,steps,op,**fields):
        return [s['value'] for s in steps if s['op']==op and all(s.get(k)==v for k,v in fields.items())]

    def prop(self,steps,ref,property):
        return self.values(steps,'assert_card',ref=ref,property=property)

    def test_exact_original_oracle_and_complete_profiles(self):
        model.validate_witnesses(self.oracle,self.rows)
        self.assertEqual(self.rows[model.GERM]['csvid'],18741)
        expected = {
            'Flayer Husk':(1,2,1,1,''), 'Sickleslicer':(3,4,2,2,''),
            'Necropouncer':(6,2,3,1,'haste'), 'Skinwing':(4,6,2,2,'flying'),
            'Strandwalker':(5,4,2,4,'reach'),
        }
        names = []
        for name,d in self.homes.items():
            record = self.oracle[name]
            shape = model.spec(d,record)
            self.assertEqual(d['variants'],model.variants(shape))
            self.assertEqual((shape['cast_mana'][0],shape['equip_generic'],shape['power_delta'],
                              shape['toughness_delta'],shape['keyword']),expected[name])
            for case in self.cases(name,*d['variants']):
                self.assertEqual(case['_recipe']['covered_clauses'],record['oracle_text'].splitlines())
                self.assertNotIn(model.GERM,{e['name'] for e in case['setup']})
                names.append(case['name'])
        self.assertEqual(len(set(names)),242)
        name = 'Necropouncer'
        d,r = self.homes[name],self.oracle[name]
        with self.assertRaises(ValueError):
            model.spec(d,dict(r,oracle_id='stale'))
        for changes in (dict(version=True),dict(variants=['enter_0','enter_0']),
                        dict(variants=['unreviewed_0']),dict(params=dict(d['params'],keyword=''))):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                model.spec(dict(d,**changes),r)

    def test_generated_black_germ_is_bound_only_after_actual_entry(self):
        for name,d in self.homes.items():
            steps = self.steps(name,'enter_1')
            cast = steps.index(dict(op='cast',ref='source',value=1))
            resolve = steps.index(dict(op='resolve'),cast)
            bind = steps.index(dict(op='bind_permanent_copy',ref='germ',player=1,value=18741,index=0))
            self.assertLess(resolve,bind)
            clear = [s for s in steps if s['op']=='set_mana'][:12]
            self.assertEqual(clear,[dict(op='set_mana',player=p,color=c,value=0) for p in (0,1) for c in range(6)])
            for prop,value in (('artifact',0),('token',1),('color',2),('subtype_Phyrexian',1)):
                self.assertIn(dict(op='assert_matching_permanents',player=1,index=18741,property=prop,value=value),steps)
            self.assertEqual(self.prop(steps,'germ','power'),[d['params']['power_delta']])
            self.assertEqual(self.prop(steps,'germ','toughness'),[d['params']['toughness_delta']])
            self.assertNotIn('activate',{s['op'] for s in steps})

    def test_actual_short_and_countered_casts_never_generate_token(self):
        n = 'Flayer Husk'
        ss = self.steps(n,'cast_short_1')
        self.assertIn(dict(op='cast',ref='source',value=0),ss)
        self.assertNotIn('bind_permanent_copy',{s['op'] for s in ss})
        ss = self.steps(n,'countered_1')
        announce = ss.index(dict(op='announce_spell',ref='source'))
        response = ss.index(dict(op='cast_response',ref='answer',value=1))
        self.assertLess(announce,response)
        self.assertNotIn('choose_target',{s['op'] for s in ss[announce:response]})
        self.assertIn(dict(op='assert_stack',value=2),ss)
        self.assertNotIn('bind_permanent_copy',{s['op'] for s in ss})
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=11442,value=1),ss)

    def test_paid_equip_preserves_attached_germ_until_resolution(self):
        n = 'Sickleslicer'
        ss = self.steps(n,'move_1')
        at = ss.index(dict(op='activate',ref='source',value=1))
        resolve = ss.index(dict(op='resolve'),at)
        self.assertIn(dict(op='set_mana',player=1,color=0,value=4),ss[:at])
        self.assertIn(dict(op='assert_mana',player=1,color=0,value=0),ss[at:resolve])
        self.assertEqual(self.prop(ss[at:resolve],'germ','power'),[2])
        self.assertEqual(self.prop(ss[at:resolve],'ally','power'),[1])
        self.assertEqual(self.prop(ss[resolve:],'ally','power'),[3])
        self.assertFalse(self.prop(ss[resolve:],'germ','power'))
        self.assertIn(dict(op='assert_zone',player=1,zone='graveyard',value=0),ss[resolve:])

    def test_lawful_target_and_sorcery_timing_controls(self):
        n = 'Flayer Husk'
        for mode,left in (('equip_short_0',1),('offturn_0',2),('spell_pending_0',2),('cancel_0',2)):
            steps = self.steps(n,mode)
            self.assertIn(dict(op='activate',ref='source',value=0),steps)
            self.assertEqual(self.values(steps,'assert_mana',player=0,color=0)[-1],left)
            self.assertNotIn(dict(op='choose_target',player=0,ref='ally'),steps)
        steps = self.steps(n,'foreign_target_1')
        self.assertIn(dict(op='probe_activation_target',ref='source',other='enemy',index=-1,value=0),steps)
        self.assertIn(dict(op='choose_target',player=1,ref='ally'),steps)

    def test_real_removal_above_ability_leaves_previous_attachment_or_none(self):
        for mode,departing in (('equip_target_leaves_0','ally'),('source_lost_pending_0','source')):
            steps = self.steps('Skinwing',mode)
            activate = steps.index(dict(op='activate',ref='source',value=1))
            cast = steps.index(dict(op='cast',ref='answer',value=1))
            drain = steps.index(dict(op='resolve'),cast)
            self.assertLess(activate,cast)
            self.assertIn(dict(op='choose_target',player=1,ref=departing),steps[activate:cast])
            self.assertIn(dict(op='assert_stack',value=2),steps[cast:drain])
            self.assertEqual(self.prop(steps[drain:],'germ','toughness'),[2] if departing=='ally' else [])

    def test_existing_germ_survives_transfer_only_with_real_pump(self):
        steps = self.steps('Flayer Husk','keep_germ_until_cleanup_0')
        self.assertIn(dict(op='cast',ref='growth',value=1),steps)
        activate = steps.index(dict(op='activate',ref='source',value=1))
        cleanup = steps.index(dict(op='cleanup'))
        self.assertEqual(self.prop(steps[activate:cleanup],'germ','toughness'),[4,3])
        self.assertFalse(self.prop(steps[cleanup:],'germ','toughness'))
        self.assertEqual(self.prop(steps[cleanup:],'ally','toughness'),[3])

    def test_departure_bounce_recast_and_control_transfer_use_actual_objects(self):
        n = 'Sickleslicer'
        for mode,in_grave in (('destroy_germ_1',0),('bounce_germ_1',0),('remove_source_1',1),('host_lost_1',1)):
            steps = self.steps(n,mode)
            cast = steps.index(dict(op='cast',ref='answer',value=1))
            self.assertFalse(self.prop(steps[cast:],'germ','power'))
            self.assertEqual(self.values(steps[cast:],'assert_zone',player=1,zone='graveyard')[-1],in_grave)
        steps = self.steps(n,'bounce_source_1')
        self.assertEqual(self.values(steps,'cast',ref='source'),[1,1])
        self.assertEqual(len([s for s in steps if s['op']=='bind_permanent_copy' and s['ref']=='germ']),2)
        self.assertIn(dict(op='bind_hand_copy',ref='source',player=1,index=0,value=11684),steps)
        steps = self.steps(n,'control_transfer_1')
        self.assertIn(dict(op='bind_permanent',ref='source',player=0,value=11684),steps)
        self.assertIn(dict(op='choose_target',player=0,ref='enemy'),steps)
        self.assertEqual(self.prop(steps,'enemy','power')[-1],3)

    def test_haste_fresh_germ_and_real_fresh_host_only_with_source(self):
        for n,mode,ref,allowed,life in (('Necropouncer','fresh_attack_0','germ',1,17),
                                        ('Sickleslicer','fresh_attack_0','germ',0,20),
                                        ('Necropouncer','fresh_host_move_0','ally',1,16),
                                        ('Necropouncer','fresh_host_lost_0','ally',0,20)):
            steps = self.steps(n,mode)
            self.assertIn(dict(op='attack',ref=ref,value=allowed),steps)
            self.assertEqual(self.values(steps,'assert_life',player=1)[-1],life)

    def test_flying_and_reach_native_legal_combat_and_expiration(self):
        for name,ground,flying in (('Skinwing',0,1),('Strandwalker',1,1),('Flayer Husk',1,0)):
            steps = self.steps(name,'ready_ground_block_0')
            self.assertIn(dict(op='block',ref='enemy',other='germ',value=ground),steps)
            steps = self.steps(name,'opponent_flyer_0')
            self.assertIn(dict(op='block',ref='germ',other='enemy',value=flying),steps)
        for name in ('Skinwing','Strandwalker'):
            for mode,legal in (('transferred_keyword_0',1),('keyword_lost_0',0)):
                steps = self.steps(name,mode)
                self.assertIn(dict(op='block',ref='ally',other='enemy',value=legal),steps)

    def test_replay_reexecutes_actual_entry_or_equip(self):
        for mode,op in (('replay_entry_0','cast'),('replay_move_0','activate')):
            steps = self.steps('Skinwing',mode)
            start = steps.index(dict(op='capture'))
            record = steps.index(dict(op='record_state'))
            restore = steps.index(dict(op='restore'))
            self.assertEqual(steps[start+1:record],steps[restore+1:-1])
            self.assertIn(dict(op=op,ref='source',value=1),steps[start:record])
            self.assertEqual(steps[-1],dict(op='assert_recorded_state'))


class LivingWeaponRiderRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data/'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']:row for row in registry()}
        cls.homes = {}
        for name,pin in model.pins()['rider_sources'].items():
            csvid = pin['registry']['csvid']
            path, = (data/'cases/cards').glob(f'*/{csvid:05d}-*.json')
            cls.homes[name] = json.loads(path.read_bytes())['recipes'][0]

    def case(self,name,variant):
        d = self.homes[name]
        shape = model.spec(d,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],d,shape,[variant],self.rows,self.oracle)[0]

    def test_whole_oracle_records_and_real_rider_sources(self):
        model.validate_riders(self.oracle,self.rows)
        self.assertEqual(len(self.homes),5)
        for name,d in self.homes.items():
            r = self.oracle[name]
            self.assertEqual(d['oracle_text'],r['oracle_text'])
            self.assertEqual(d['variants'],model.variants(model.spec(d,r)))
            case = self.case(name,'enter_0')
            self.assertEqual(case['_recipe']['covered_clauses'],r['oracle_text'].splitlines())
            self.assertFalse(any(e['name']==model.GERM for e in case['setup']))
            with self.assertRaises(ValueError):
                model.spec(dict(d,params=dict(d['params'],toughness_delta=True)),r)
            with self.assertRaises(ValueError):
                model.spec(d,dict(r,oracle_text=r['oracle_text']+'\nFlying'))

    def test_original_living_weapon_enters_and_real_paid_equip_moves_it(self):
        for name,d in self.homes.items():
            steps = self.case(name,'move_0')['steps']
            cast = steps.index(dict(op='cast',ref='source',value=1))
            resolve = steps.index(dict(op='resolve'),cast)
            bind = steps.index(dict(op='bind_permanent_copy',ref='germ',player=0,index=0,value=model.GERM_ID))
            activate = steps.index(dict(op='activate',ref='source',value=1))
            self.assertLess(resolve,bind)
            self.assertLess(bind,activate)
            self.assertIn(dict(op='choose_target',player=0,ref='ally'),steps[bind:activate])
            self.assertIn(dict(op='assert_stack',value=1),steps[activate:])
            self.assertEqual([s for s in steps if s.get('ref')=='germ' and s.get('property')=='in_play'][-1]['value'],1)
            self.assertIn('germ',[s['ref'] for s in steps[activate:steps.index(dict(op='resolve'),activate)] if s['op']=='assert_card'])
            self.assertNotIn('germ',[s.get('ref') for s in steps[steps.index(dict(op='resolve'),activate):] if s['op']=='assert_card'])

    def test_dynamic_counts_exclude_nonmatching_cards_and_germ_dies_at_zero(self):
        for name,rise,shrink in [('Bonehoard','grave_grow','grave_to_zero'),('Lashwrithe','swamp_grow','swamp_to_zero')]:
            steps = self.case(name,rise+'_0')['steps']
            values = [s['value'] for s in steps if s.get('ref')=='germ' and s.get('property')=='toughness']
            self.assertEqual(values,[1,2])
            for mode in ('zero_count',shrink):
                steps = self.case(name,mode+'_1')['steps']
                self.assertIn(dict(op='assert_life',player=1,value=21),steps)
                self.assertIn(dict(op='assert_life',player=0,value=19),steps)
                self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',index=model.GERM_ID,value=0),steps)
                if mode=='zero_count':
                    self.assertFalse(any(s['op']=='bind_permanent_copy' for s in steps))
        steps = self.case('Lashwrithe','own_nonbasic_0')['steps']
        self.assertIn(dict(op='assert_card',ref='germ',property='toughness',value=1),steps)

    def test_lashwrithe_real_black_life_or_mixed_payment_and_low_life_denial(self):
        for mode,black,life,y in [('equip_black',2,1,None),('equip_mixed',1,18,1),('equip_life',0,16,2)]:
            steps = self.case('Lashwrithe',mode+'_1')['steps']
            at = steps.index(dict(op='activate',ref='source',value=1))
            stop = steps.index(dict(op='resolve'),at)
            self.assertIn(dict(op='assert_mana',player=1,color=1,value=black),steps[:at])
            self.assertIn(dict(op='assert_mana',player=1,color=1,value=0),steps[at:stop])
            self.assertIn(dict(op='assert_life',player=1,value=life),steps[at:stop])
            self.assertEqual([s['value'] for s in steps[:at] if s['op']=='choose_number'],[] if y is None else [y])
        denied = self.case('Lashwrithe','equip_insufficient_0')['steps']
        self.assertIn(dict(op='activate',ref='source',value=0),denied)
        self.assertIn(dict(op='assert_life',player=0,value=1),denied)

    def test_owned_return_and_combat_keywords(self):
        for mode in ('return','return_foreign','return_offturn','return_recast'):
            steps = self.case('Batterskull',mode+'_0')['steps']
            at = steps.index(dict(op='activate',ref='source',value=1))
            self.assertIn(dict(op='assert_stack',value=1),steps[at:])
            self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=11564,value=1),steps[at:])
        steps = self.case('Batterskull','germ_combat_1')['steps']
        attack = steps.index(dict(op='attack',ref='germ',value=1))
        self.assertIn(dict(op='assert_card',ref='germ',property='tapped',value=0),steps[attack:])
        self.assertIn(dict(op='assert_life',player=1,value=24),steps[attack:])
        self.assertIn(dict(op='assert_life',player=0,value=16),steps[attack:])

    def test_mortarpod_granted_sacrifice_precedes_pending_damage(self):
        for mode,host in [('ping_player','germ'),('ping_host','ally'),('ping_lifelink','ally')]:
            steps = self.case('Mortarpod',mode+'_0')['steps']
            at = max(i for i,s in enumerate(steps) if s==dict(op='activate',ref='source',value=1))
            resolution = steps.index(dict(op='resolve'),at)
            self.assertIn(dict(op='assert_stack',value=1),steps[at:resolution])
            target_zone = 'battlefield' if host=='germ' else 'graveyard'
            source_witness = {'ping_player':model.GERM_ID,'ping_host':self.rows['Mogg Fanatic']['csvid'],'ping_lifelink':self.rows['Child of Night']['csvid']}[mode]
            self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone=target_zone,index=source_witness,value=int(host=='ally')),steps[at:resolution])
            self.assertIn(dict(op='assert_life',player=1,value=20),steps[at:resolution])
            self.assertIn(dict(op='assert_life',player=1,value=19),steps[resolution:])
            if mode=='ping_lifelink':
                self.assertIn(dict(op='assert_life',player=0,value=21),steps[resolution:])

    def test_scytheclaw_uses_combat_player_hit_and_post_damage_rounding(self):
        for mode,life in [('even_remainder',9),('odd_remainder',8),('blocked_combat',20),('walker_combat',20)]:
            steps = self.case('Scytheclaw',mode+'_0')['steps']
            cut = steps.index(dict(op='combat_damage'))
            self.assertIn(dict(op='assert_life',player=1,value=life),steps[cut:])
        steps = self.case('Scytheclaw','noncombat_1')['steps']
        self.assertIn(dict(op='activate',ref='ally',value=1),steps)
        self.assertNotIn('combat_damage',[s['op'] for s in steps])
        self.assertIn(dict(op='assert_life',player=0,value=19),steps)

    def test_snapshots_repeat_actual_rider_actions(self):
        for name,variant,op in [('Batterskull','replay_move_0','activate'),('Mortarpod','ping_replay_1','activate'),('Scytheclaw','combat_replay_0','combat_damage')]:
            steps = self.case(name,variant)['steps']
            before = steps[steps.index(dict(op='capture'))+1:steps.index(dict(op='record_state'))]
            self.assertEqual(before,steps[steps.index(dict(op='restore'))+1:-1])
            self.assertIn(op,[s['op'] for s in before])
