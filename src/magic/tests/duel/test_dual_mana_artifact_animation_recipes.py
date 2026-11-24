"""Checks for real payment, artifact/creature state, subtype and combat witnesses."""
import json
import unittest
from pathlib import Path

import dual_mana_artifact_animation_recipes as model
from generate_cases import OPS, TOKEN_PROPERTIES, registry


class DualManaArtifactAnimationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={row['name']:row for row in registry()}

    def spec(self,name):
        record=self.oracle[name]
        d=dict(family=model.FAMILY,version=2,oracle_text=record['oracle_text'],params=dict(schema=1,mechanism=model.mechanism(record)))
        return d,model.spec(d,record)

    def case(self,name,mode,owner=0):
        d,p=self.spec(name)
        return model.expand(name,p['csvid'],d,p,[f'{mode}_{owner}'],self.rows,self.oracle)[0]

    def test_whole_independent_records_bind_real_form_costs(self):
        expected={'Golgari Keyrune':(12734,'BG','Insect',2,2,'deathtouch',0),
                  'Rakdos Keyrune':(12795,'BR','Devil',3,1,'first_strike',0),
                  'Selesnya Keyrune':(12816,'GW','Wolf',3,3,'none',0),
                  'Dimir Keyrune':(12927,'UB','Horror',2,2,'unblockable',0),
                  'Simic Keyrune':(13059,'GU','Crab',2,3,'hexproof',0),
                  'Atarka Monument':(14850,'RG','Dragon',4,4,'flying',4),
                  'Azorius Keyrune':(12664,'WU','Bird',2,2,'flying',0),
                  'Dromoka Monument':(14903,'GW','Dragon',4,4,'flying',4),
                  'Kolaghan Monument':(14946,'BR','Dragon',4,4,'flying',4),
                  'Ojutai Monument':(14971,'WU','Dragon',4,4,'flying',4),
                  'Silumgar Monument':(15029,'UB','Dragon',4,4,'flying',4),
                  'Boros Keyrune':(12901,'RW','Soldier',1,1,'double_strike',0),
                  'Gruul Keyrune':(12965,'RG','Beast',3,2,'trample',0),
                  'Orzhov Keyrune':(13026,'WB','Thrull',1,4,'lifelink',0),
                  'Izzet Keyrune':(12755,'UR','Elemental',2,1,'loot',0)}
        self.assertEqual(set(model.pins()['sources']),set(expected))
        model.validate_witnesses(self.oracle,self.rows)
        for name,(csvid,colors,form,power,toughness,extra,generic) in expected.items():
            _,p=self.spec(name)
            self.assertEqual((p['csvid'],p['colors'],p['creature_type'],p['power'],p['toughness'],p['extra']),
                             (csvid,list(colors),form,power,toughness,extra))
            self.assertEqual(p['fee'],[generic]+[int(c in colors) for c in 'BUGRW'])
            for changed in (self.oracle[name]['oracle_text']+'\nDraw a card.',self.oracle[name]['oracle_text'].replace('artifact creature','creature')):
                with self.assertRaises(ValueError):
                    model.mechanism(dict(self.oracle[name],oracle_text=changed))

    def test_actual_lone_mana_source_pays_each_printed_color(self):
        for name in model.pins()['sources']:
            _,p=self.spec(name)
            for owner in (0,1):
                for mode,color in zip(('mana_first','mana_second'),p['colors']):
                    case=self.case(name,mode,owner)
                    spender=next(c for c in case['setup'] if c['ref']=='spender')
                    self.assertEqual(self.oracle[spender['name']]['mana_cost'],'{'+color+'}')
                    steps=case['steps'];cast=steps.index(dict(op='cast',ref='spender',value=1))
                    self.assertFalse(any(s['value'] for s in steps[:cast] if s['op']=='set_mana'))
                    self.assertEqual(sum(s['op']=='choose_mana_source' for s in steps[:cast]),int(owner==0))
                    self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),steps[cast:])

    def test_ordinary_stack_cut_typed_payment_and_tapped_boundary(self):
        for name in model.pins()['sources']:
            _,p=self.spec(name)
            for owner in (0,1):
                steps=self.case(name,'tapped_animate',owner)['steps']
                at=steps.index(dict(op='activate',ref='source',value=1));end=steps.index(dict(op='resolve'),at)
                self.assertIn(dict(op='assert_stack',value=1),steps[at:end])
                self.assertEqual([s['value'] for s in steps[:at] if s['op']=='set_mana' and s['player']==owner][-6:],p['fee'])
                self.assertEqual([s['value'] for s in steps[at:end] if s['op']=='assert_mana' and s['player']==owner],[0]*6)
                self.assertIn(dict(op='assert_matching_permanents',player=owner,index=p['csvid'],property='color',value=0),steps[at:end])
                self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),steps[end:])
                for mode in ('wrong_fee','short_fee'):
                    denied=self.case(name,mode,owner)
                    self.assertEqual(denied['setup'][0]['state'],['tapped'])
                    self.assertIn(dict(op='activate',ref='source',value=0),denied['steps'])

    def test_cleanup_removes_creature_form_and_keeps_artifact(self):
        for name in model.pins()['sources']:
            _,p=self.spec(name)
            for owner in (0,1):
                steps=self.case(name,'cleanup',owner)['steps'];cleanup=steps.index(dict(op='cleanup'))
                identity=dict(op='assert_matching_permanents',player=owner,index=p['csvid'])
                color=sum(1<<model.COLORS.index(c) for c in p['colors'])
                self.assertIn(dict(identity,property='color',value=color),steps[:cleanup])
                self.assertIn(dict(identity,property='color',value=0),steps[cleanup:])
                self.assertIn(dict(identity,property='artifact',value=1),steps[cleanup:])
                self.assertIn(dict(op='can_target_creature',ref='source',other='source',value=0),steps[cleanup:])

    def test_actual_evasion_first_strike_reach_and_deathtouch(self):
        for owner in (0,1):
            for name,legal in [('Atarka Monument',0),('Dimir Keyrune',0),('Rakdos Keyrune',1),('Golgari Keyrune',1)]:
                steps=self.case(name,'keyword_combat',owner)['steps'];ops=[s['op'] for s in steps]
                self.assertIn(dict(op='block',ref='blocker',other='source',value=legal),steps)
                self.assertLess(ops.index('advance_combat'),steps.index(dict(op='assert_card',ref='source',property='blocked',value=legal)))
                self.assertLess(ops.index('combat_damage'),ops.index('end_combat'))
                if legal:
                    witness='Grizzly Bears' if name=='Rakdos Keyrune' else 'Wall of Stone'
                    self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows[witness]['csvid'],value=1),steps)
                    self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),steps[ops.index('combat_damage'):])
            reach=self.case('Atarka Monument','reach_block',owner)['steps']
            self.assertIn(dict(op='block',ref='blocker',other='source',value=1),reach)
            fresh=self.case('Atarka Monument','fresh_attack',owner)['steps']
            self.assertIn(dict(op='attack',ref='source',value=0),fresh)

    def test_real_own_and_opposing_hexproof_casts_and_subtype_witness(self):
        for owner in (0,1):
            steps=self.case('Simic Keyrune','hexproof_casts',owner)['steps']
            self.assertIn(dict(op='cast',ref='foe_probe',value=0),steps)
            self.assertIn(dict(op='choose_target',player=owner,ref='source'),steps)
            self.assertIn(dict(op='cast',ref='own_probe',value=1),steps)
            self.assertIn(dict(op='assert_card',ref='source',property='toughness',value=6),steps)
            for name,sub in [('Simic Keyrune','Crab'),('Rakdos Keyrune','Devil')]:
                case=self.case(name,'shared_subtype',owner);steps=case['steps']
                self.assertIn(dict(op='cast',ref='coat',value=1),steps)
                witness=next(c for c in case['setup'] if c['ref']=='kin')
                self.assertIn(sub,self.oracle[witness['name']]['type_line'].split())
                self.assertEqual([s['value'] for s in steps if s.get('ref')=='kin' and s.get('property')=='power'],[1,1,2,1])

    def test_reviewed_keyword_effects_use_real_attack_and_defense(self):
        for owner in (0,1):
            for card,mode,recipient,life in (
                    ('Boros Keyrune','attack',1-owner,18),('Gruul Keyrune','keyword_combat',1-owner,19),
                    ('Gruul Keyrune','trample_no_excess',1-owner,20),('Orzhov Keyrune','attack',owner,21),
                    ('Orzhov Keyrune','lifelink_defending',owner,21),('Orzhov Keyrune','lifelink_prevented',owner,20)):
                steps=self.case(card,mode,owner)['steps']
                self.assertIn(dict(op='assert_life',player=recipient,value=life),steps[steps.index(dict(op='combat_damage')):])
            double=self.case('Boros Keyrune','keyword_combat',owner)['steps']
            self.assertIn(dict(op='assert_card',ref='blocker',property='damage',value=2),double)
            defend=self.case('Orzhov Keyrune','lifelink_defending',owner)['steps']
            self.assertIn(dict(op='block',ref='source',other='blocker',value=1),defend)
            for mode,library in [('attack',2),('loot_defending',3),('keyword_combat',3),('loot_prevented',3)]:
                steps=self.case('Izzet Keyrune',mode,owner)['steps']
                self.assertEqual(sum(s['op']=='choose_trigger' for s in steps),int(mode=='attack' and owner==0))
                self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=library),steps[steps.index(dict(op='combat_damage')):])
        decline=self.case('Izzet Keyrune','loot_decline')['steps']
        self.assertFalse(any(s['op']=='choose_trigger' for s in decline))
        choice=self.case('Izzet Keyrune','loot_choice')['steps']
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Forest']['csvid'],value=1),choice)
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Fog']['csvid'],value=1),choice)

    def test_full_clauses_valid_schema_and_repeatable_snapshots(self):
        count=0
        for name in model.pins()['sources']:
            d,p=self.spec(name)
            for case in model.expand(name,p['csvid'],d,p,model.variants(p),self.rows,self.oracle):
                count+=1
                self.assertEqual(case['_recipe']['covered_clauses'],self.oracle[name]['oracle_text'].splitlines())
                steps=case['steps'];ops=[s['op'] for s in steps]
                self.assertFalse(set(ops)-OPS)
                for step in steps:
                    if step['op']=='assert_matching_permanents':
                        self.assertIn(step['property'],TOKEN_PROPERTIES)
                if 'replay' in case['name']:
                    self.assertEqual(steps[ops.index('capture')+1:ops.index('record_state')],steps[ops.index('restore')+1:-1])
        self.assertEqual(count,568)
