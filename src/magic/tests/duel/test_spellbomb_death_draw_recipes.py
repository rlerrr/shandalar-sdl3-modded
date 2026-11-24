import json
import unittest
from pathlib import Path

import spellbomb_death_draw_recipes as family
from generate_cases import registry
from recipes import expand


class SpellbombDeathDrawTests(unittest.TestCase):
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

    def has(self,case,**fields):
        return any(all(s.get(k)==v for k,v in fields.items()) for s in case['steps'])

    def check_zone(self,case,p,zone,value):
        self.assertTrue(self.has(case,op='assert_zone',player=p,zone=zone,value=value))

    def check_card(self,case,p,zone,name,value):
        self.assertTrue(self.has(case,op='assert_zone_multiplicity',player=p,zone=zone,index=self.rows[name]['csvid'],value=value))

    def test_source_selection_matches_whole_pinned_grammar(self):
        family.validate_witnesses(self.oracle,self.rows)
        for name,pin in self.sources.items():
            r = self.oracle[name]
            self.assertEqual(pin['oracle'],r)
            d = self.declaration(name)
            self.assertEqual(d['params'],family.parameters(r))
            self.assertTrue(d['params']['tap'] and d['params']['sacrifice'])
            with self.assertRaises(ValueError):
                family.parameters(dict(r,oracle_text=r['oracle_text']+' Draw a card.'))
        with self.assertRaises(ValueError): family.parameters(self.oracle['Aether Spellbomb'])

    def test_complete_selected_matrix_contains_only_real_actions(self):
        total = 0
        for name in self.sources:
            d = self.declaration(name)
            self.assertEqual(d['variants'],family.variants(family.spec(d,self.oracle[name])))
            cases = expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,self.oracle)
            total += len(cases)
            self.assertFalse(any(s['op'] in ('destroy','dispatch_gfp','set_life','move_card','set_counter')
                                 for c in cases for s in c['steps']))
        self.assertEqual(total,141)

    def test_real_colored_payment_and_wrong_resource_at_both_death_paths(self):
        for name in self.sources:
            model = family.parameters(self.oracle[name]); needed,basic = family.BASICS[model['draw_color']]
            for p in (0,1):
                for mode in ('activate_pay','destroy_pay','foreign_owned_destroy'):
                    c = self.case(name,mode,p)
                    self.assertTrue(any(item['name']==basic and item['player']==p and item['ref']=='draw_land' for item in c['setup']))
                    self.assertTrue(self.has(c,op='assert_card',ref='draw_land',property='tapped',value=1))
                    self.assertEqual(self.has(c,op='choose_mana_source',ref='draw_land'),p==0)
                    self.assertFalse(self.has(c,op='choose_trigger'))
                    owner = 1-p if mode=='foreign_owned_destroy' else p
                    self.check_card(c,owner,'graveyard',name,1)
                    self.check_zone(c,p,'hand',6 if name=='Horizon Spellbomb' and mode=='activate_pay' else 5)
                for mode in ('activate_wrong_color','destroy_wrong_color'):
                    c = self.case(name,mode,p)
                    self.assertFalse(any(item['ref']=='draw_land' for item in c['setup']))
                    self.assertTrue(self.has(c,op='assert_mana',player=p,color=needed,value=0))
                    self.assertTrue(any(s['op']=='assert_mana' and s['player']==p and s['color'] not in (0,needed) and s['value']==1 for s in c['steps']))
                    self.check_zone(c,p,'hand',5 if name=='Horizon Spellbomb' and mode=='activate_wrong_color' else 4)

    def test_real_human_cancel_and_nongrave_destinations_do_not_draw(self):
        for name in self.sources:
            for mode in ('activate_decline','destroy_decline'):
                c = self.case(name,mode)
                self.assertTrue(self.has(c,op='choose_cancel_mana',player=0))
                self.assertTrue(self.has(c,op='assert_card',ref='draw_land',property='tapped',value=0))
                self.check_card(c,0,'graveyard',name,1)
                self.check_zone(c,0,'hand',5 if name=='Horizon Spellbomb' and mode=='activate_decline' else 4)
            for p in (0,1):
                for mode,zone in [('exile','exile'),('bounce','hand')]:
                    c = self.case(name,mode,p)
                    self.assertTrue(self.has(c,op='cast',ref='removal',value=1))
                    self.assertFalse(self.has(c,op='choose_mana_source'))
                    self.check_card(c,p,zone,name,1)
                    self.check_card(c,p,'graveyard',name,0)
                    self.assertTrue(self.has(c,op='assert_card',ref='draw_land',property='tapped',value=0))
                    self.check_zone(c,p,'hand',5 if mode=='bounce' else 4)

    def test_horizon_real_basic_search_shuffle_and_paid_draw_are_separate(self):
        name = 'Horizon Spellbomb'
        for p in (0,1):
            paid = self.case(name,'activate_pay',p)
            self.assertTrue(self.has(paid,op='set_mana',player=p,color=0,value=2))
            self.assertTrue(self.has(paid,op='choose_zone_card',player=p,value=self.rows['Forest']['csvid']))
            self.check_card(paid,p,'hand','Hill Giant',1)
            self.check_card(paid,p,'hand','Forest',1)
            self.check_card(paid,p,'library','Radiant Fountain',1)
            self.assertTrue(self.has(paid,op='assert_shuffle_count',player=p,value=1))
            none = self.case(name,'no_basic',p)
            self.assertFalse(self.has(none,op='choose_zone_card'))
            self.check_card(none,p,'hand','Forest',0)
            self.check_card(none,p,'library','Radiant Fountain',1)
            self.assertTrue(self.has(none,op='assert_shuffle_count',player=p,value=1))
        canceled = self.case(name,'search_decline')
        self.assertTrue(self.has(canceled,op='choose_optional_zone',value=self.rows['Forest']['csvid']))
        self.check_card(canceled,0,'hand','Hill Giant',1)
        self.check_card(canceled,0,'library','Forest',1)

    def test_nihil_uses_the_selected_grave_and_origin_creates_an_exact_myr(self):
        for p in (0,1):
            c = self.case('Nihil Spellbomb','self_target',p)
            self.assertTrue(self.has(c,op='choose_player',player=p,value=p))
            self.check_zone(c,p,'graveyard',0)
            self.check_card(c,p,'exile','Nihil Spellbomb',1)
            self.check_zone(c,1-p,'graveyard',4)
            token = self.case('Origin Spellbomb','activate_pay',p)
            self.assertTrue(self.has(token,op='set_mana',player=p,color=0,value=1))
            for prop,val in [('token',1),('artifact',1),('power',1),('toughness',1),('color',0),('subtype_Myr',1),('tapped',0)]:
                self.assertTrue(self.has(token,op='assert_matching_permanents',player=p,index=18792,property=prop,value=val))
            self.check_card(token,1-p,'battlefield','Myr',0)
            unfunded = self.case('Origin Spellbomb','activation_unaffordable',p)
            self.assertTrue(self.has(unfunded,op='activate',ref='source',value=0))
            self.check_card(unfunded,p,'battlefield','Myr',0)

    def test_flight_and_panic_change_real_block_legality_and_expire(self):
        for name,target in [('Flight Spellbomb','host'),('Panic Spellbomb','blocker')]:
            for p in (0,1):
                hit = self.case(name,'activate_pay',p)
                self.assertTrue(self.has(hit,op='choose_target',player=p,ref=target))
                self.assertTrue(self.has(hit,op='block',ref='blocker',other='host',value=0))
                self.assertTrue(self.has(hit,op='assert_life',player=1-p,value=17))
                expired = self.case(name,'expired',p)
                self.assertTrue(self.has(expired,op='advance_turn'))
                self.assertTrue(self.has(expired,op='block',ref='blocker',other='host',value=1))
                self.check_card(expired,1-p,'graveyard','Runeclaw Bear',1)
                unavailable = self.case(name,'no_target',p)
                self.assertFalse(self.has(unavailable,op='choose_target'))
                self.assertTrue(self.has(unavailable,op='activate',ref='source',value=0))


if __name__=='__main__':
    unittest.main()
