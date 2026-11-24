import json
import unittest
from pathlib import Path

import multi_creature_recolor_recipes as family
from generate_cases import registry
from recipes import expand


class MultiCreatureRecolorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.duel = Path(__file__).parent
        cls.oracle = json.loads((cls.duel/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']:r for r in registry()}
        cls.sources = family.profiles()['sources']

    def declaration(self,name):
        csvid = self.rows[name]['csvid']
        path, = (self.duel/f'data/cases/cards/{csvid//100:03}').glob(f'{csvid:05}-*.json')
        declaration, = json.loads(path.read_text(encoding='utf8'))['recipes']
        return declaration

    def case(self,name,mode,p=0):
        d = dict(self.declaration(name),variants=[f'{mode}_{p}'])
        return expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,self.oracle)[0]

    def has(self,steps,**fields):
        return any(all(s.get(k)==value for k,value in fields.items()) for s in steps)

    def permanent(self,case,ref):
        return next(item for item in case['setup'] if item['ref']==ref)

    def color(self,case,steps,ref,value):
        item = self.permanent(case,ref)
        self.assertTrue(self.has(steps,op='assert_matching_permanents',property='color',index=self.rows[item['name']]['csvid'],
                                 player=item['player'],value=value))

    def zone(self,case,steps,ref,zone,value):
        item = self.permanent(case,ref)
        self.assertTrue(self.has(steps,op='assert_zone_multiplicity',zone=zone,index=self.rows[item['name']]['csvid'],
                                 player=item['player'],value=value))

    def intrinsic(self,case,ref):
        return sum(family.COLOR_BITS[c] for c in self.oracle[self.permanent(case,ref)['name']]['colors'])

    def test_complete_pinned_instant_color_and_nonzero_minimum(self):
        family.validate_witnesses(self.oracle,self.rows)
        self.assertEqual({family.parameters(self.oracle[n])['color'] for n in self.sources},set('BUGRW'))
        for name in self.sources:
            r = self.oracle[name]
            p = family.spec(self.declaration(name),r)
            self.assertEqual((p['minimum'],p['color_operation'],p['duration']),(1,'replace','until_end_of_turn'))
            for text in (r['oracle_text'].replace('One or more','Any number of'),r['oracle_text']+' Draw a card.'):
                with self.assertRaises(ValueError): family.parameters(dict(r,oracle_text=text))

    def test_all_five_complete_matrices_use_only_actual_actions(self):
        total = 0
        for name in self.sources:
            d = self.declaration(name)
            self.assertEqual(d['variants'],family.variants(family.spec(d,self.oracle[name])))
            all_cases = expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,self.oracle)
            total += len(all_cases)
            self.assertFalse(any(s['op'] in ('set_life','set_counter','destroy','dispatch_gfp','set_color','move_card')
                                 for c in all_cases for s in c['steps']))
        self.assertEqual(total,170)

    def test_real_target_list_stops_only_after_one_or_more_and_stays_selective(self):
        for name in self.sources:
            value = family.COLOR_BITS[family.parameters(self.oracle[name])['color']]
            for p in (0,1):
                for mode,targets,stop in [('sole_target',['first'],None),('one_and_stop',['first'],'second'),
                                          ('two_and_stop',['first','second'],'untargeted'),
                                          ('all_three',['first','second','untargeted'],None)]:
                    c = self.case(name,mode,p)
                    self.assertEqual([s['ref'] for s in c['steps'] if s['op']=='choose_target'],targets)
                    self.assertEqual([s['ref'] for s in c['steps'] if s['op']=='choose_optional_target'],[] if stop is None else [stop])
                    self.assertTrue(self.has(c['steps'],op='assert_forced_target_choices',value=0))
                    tail = c['steps'][c['steps'].index(dict(op='resolve')):]
                    for ref in targets: self.color(c,tail,ref,value)
                    if mode!='sole_target' and 'untargeted' not in targets:
                        self.color(c,tail,'untargeted',self.intrinsic(c,'untargeted'))
                artifact = self.case(name,'colorless_target',p)
                self.color(artifact,artifact['steps'],'first',0)
                self.color(artifact,artifact['steps'],'first',value)
                self.assertTrue(self.has(artifact['steps'],op='assert_matching_permanents',property='artifact',
                                         index=self.rows['Ornithopter']['csvid'],value=1))

    def test_eleven_distinct_legal_targets_have_no_artificial_maximum(self):
        for name in self.sources:
            for p in (0,1):
                c = self.case(name,'eleven',p)
                selected = [s['ref'] for s in c['steps'] if s['op']=='choose_target']
                self.assertEqual(len(set(selected)),len(selected))
                self.assertEqual(len(selected),11)
                last = self.permanent(c,selected[-1])
                value = family.COLOR_BITS[family.parameters(self.oracle[name])['color']]
                self.assertNotEqual(self.intrinsic(c,last['ref']),value)
                self.assertFalse(self.has(c['steps'],op='choose_optional_target'))
                self.assertTrue(self.has(c['steps'],op='assert_unused_choices',value=0))
                tail = c['steps'][c['steps'].index(dict(op='resolve')):]
                self.color(c,tail,last['ref'],value)

    def test_actual_hexproof_response_invalidates_only_its_selected_target(self):
        for name in self.sources:
            value = family.COLOR_BITS[family.parameters(self.oracle[name])['color']]
            for p in (0,1):
                for mode in ('partial_hexproof','all_hexproof'):
                    c = self.case(name,mode,p)
                    s = c['steps']
                    parent = s.index(dict(op='announce_spell',ref='source'))
                    response = s.index(dict(op='cast_response',ref='response',value=1))
                    self.assertLess(parent,response)
                    self.assertTrue(self.has(s[parent:response],op='choose_target',player=1-p,ref='first'))
                    tail = s[s.index(dict(op='resolve_response')):]
                    self.color(c,tail,'first',self.intrinsic(c,'first'))
                    self.color(c,tail,'second',value if mode=='partial_hexproof' else self.intrinsic(c,'second'))
                    self.color(c,tail,'untargeted',self.intrinsic(c,'untargeted'))
                    self.zone(c,tail,'response','graveyard',1)

    def test_hibernation_uses_actual_current_green_in_both_directions(self):
        for name in self.sources:
            becomes_green = family.parameters(self.oracle[name])['color']=='G'
            for p in (0,1):
                for mode in ('hibernation','hibernation_expired'):
                    c = self.case(name,mode,p)
                    s = c['steps']
                    cast = s.index(dict(op='cast',ref='color_check',value=1))
                    if mode=='hibernation_expired':
                        self.assertTrue(self.has(s[:cast],op='advance_turn'))
                        bounced = not becomes_green
                    else:
                        bounced = becomes_green
                    self.zone(c,s[cast:],'first','battlefield',int(not bounced))
                    original = self.permanent(c,'first')
                    in_hand = sum(item['zone']=='hand' and item['player']==original['player'] and item['name']==original['name']
                                  for item in c['setup'])
                    self.zone(c,s[cast:],'first','hand',in_hand+int(bounced))
                    self.zone(c,s[cast:],'untargeted','battlefield',int(becomes_green))
                    self.zone(c,s[cast:],'color_check','graveyard',1)

    def test_cleanup_restores_original_color_and_replay_keeps_both_targets(self):
        for name in self.sources:
            value = family.COLOR_BITS[family.parameters(self.oracle[name])['color']]
            c = self.case(name,'cleanup')
            s = c['steps']
            resolve = s.index(dict(op='resolve')); cleanup = s.index(dict(op='cleanup'))
            self.color(c,s[resolve:cleanup],'first',value)
            self.color(c,s[cleanup:],'first',self.intrinsic(c,'first'))
            replay = self.case(name,'pending_snapshot')
            self.assertEqual(replay['steps'].count(dict(op='resolve')),2)
            self.assertEqual(replay['steps'][-1],dict(op='assert_recorded_state'))

    def test_denied_casts_queue_no_illegal_choices_and_off_turn_is_real(self):
        for name in self.sources:
            for p in (0,1):
                for mode in ('no_creature','shroud_only','insufficient_mana'):
                    c = self.case(name,mode,p)
                    self.assertTrue(self.has(c['steps'],op='cast',ref='source',value=0))
                    self.assertFalse(self.has(c['steps'],op='choose_target'))
                    self.zone(c,c['steps'],'source','hand',1)
                off_turn = self.case(name,'off_turn',p)
                self.assertTrue(self.has(off_turn['steps'],op='turn',player=1-p,value=0))
                self.assertTrue(self.has(off_turn['steps'],op='cast',ref='source',value=1))


if __name__=='__main__':
    unittest.main()
