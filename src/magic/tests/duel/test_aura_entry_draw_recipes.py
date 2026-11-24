import json
import unittest
from pathlib import Path

import aura_entry_draw_recipes as family
from generate_cases import registry
from recipes import expand


class AuraEntryDrawTests(unittest.TestCase):
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

    def zone(self,case,p,zone,size):
        self.assertTrue(self.has(case,op='assert_zone',player=p,zone=zone,value=size))

    def multiplicity(self,case,p,zone,name,size):
        self.assertTrue(self.has(case,op='assert_zone_multiplicity',player=p,zone=zone,index=self.rows[name]['csvid'],value=size))

    def test_whole_pinned_source_clauses_and_exact_supported_shapes(self):
        family.validate_witnesses(self.oracle,self.rows)
        for name in self.sources:
            record = self.oracle[name]
            d = self.declaration(name)
            self.assertEqual(family.spec(d,record)['family'],family.FAMILY)
            with self.assertRaises(ValueError):
                family.parameters(dict(record,oracle_text=record['oracle_text']+'\nDraw a card.'))
        for name in ("Traveler's Cloak",'Curious Obsession','Sixth Sense'):
            with self.assertRaises(ValueError): family.parameters(self.oracle[name])

    def test_both_seats_and_complete_declared_matrix(self):
        total = 0
        for name in self.sources:
            d = self.declaration(name)
            self.assertEqual(d['variants'],family.variants(family.spec(d,self.oracle[name])))
            cases = expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,self.oracle)
            total += len(cases)
            self.assertFalse(any(step['op'] in ('set_life','move_card','destroy','dispatch_gfp','set_counter')
                                 for case in cases for step in case['steps']))
        self.assertEqual(total,196)

    def test_actual_mandatory_entry_draw_last_card_and_loss_from_empty(self):
        for name in self.sources:
            for p in (0,1):
                case = self.case(name,'entry',p)
                steps = case['steps']
                cast = next(i for i,s in enumerate(steps) if s['op']=='cast' and s['ref']=='source')
                resolve = next(i for i,s in enumerate(steps) if s['op']=='resolve')
                self.assertLess(cast,resolve)
                self.assertIn(dict(op='assert_zone',player=p,zone='library',value=7),steps[cast:resolve])
                self.zone(case,p,'library',6)
                self.zone(case,1-p,'library',7)
                last = self.case(name,'last_card',p)
                self.zone(last,p,'library',0)
                self.multiplicity(last,p,'hand','Forest',1)
                self.assertTrue(self.has(last,op='assert_life',player=p,value=20))
                empty = self.case(name,'empty_library',p)
                self.zone(empty,p,'library',0)
                self.multiplicity(empty,p,'hand','Forest',0)
                self.assertTrue(self.has(empty,op='assert_life',player=p,value=-99))
                self.assertTrue(self.has(empty,op='assert_life',player=1-p,value=20))

    def test_pending_aura_and_unplayed_helper_are_counted_by_the_observer(self):
        for p in (0,1):
            ordinary = self.case('Dragon Mantle','entry',p)
            fate = self.case('Fate Foretold','opposing_death',p)
            for case,pending_count in ((ordinary,5),(fate,6)):
                steps = case['steps']
                resolve = next(i for i,s in enumerate(steps) if s['op']=='resolve')
                self.zone(dict(steps=steps[:resolve]),p,'hand',pending_count)
                self.zone(dict(steps=steps[:resolve]),p,'library',7)
            steps = fate['steps']
            resolve = next(i for i,s in enumerate(steps) if s['op']=='resolve')
            murder = next(i for i,s in enumerate(steps) if s['op']=='cast' and s['ref']=='removal')
            self.zone(dict(steps=steps[resolve+1:murder]),p,'hand',6)
            self.multiplicity(dict(steps=steps[resolve+1:murder]),p,'hand','Murder',1)
            self.zone(dict(steps=steps[murder:]),p,'hand',5)
            self.multiplicity(dict(steps=steps[murder:]),p,'graveyard','Murder',1)

    def test_no_entry_draw_after_counter_or_loss_of_the_only_target(self):
        for name in self.sources:
            for p in (0,1):
                for mode,ref in [('countered','counter'),('target_lost','removal')]:
                    c = self.case(name,mode,p)
                    self.assertTrue(self.has(c,op='announce_spell',ref='source'))
                    self.assertTrue(self.has(c,op='cast_response',ref=ref,value=1))
                    self.assertTrue(self.has(c,op='resolve_response'))
                    self.zone(c,p,'library',7)
                    self.multiplicity(c,p,'graveyard',name,1)
                c = self.case(name,'no_legal_host',p)
                self.assertTrue(self.has(c,op='require_can_cast',ref='source',value=0))
                self.assertFalse(self.has(c,op='choose_target'))
        own = self.case('Cartouche of Knowledge','no_legal_host')
        self.assertTrue(any(x['ref']=='host' and x['player']==1 and x['name']=='Hill Giant' for x in own['setup']))

    def test_dragon_mantle_red_payment_stacks_and_expires(self):
        for p in (0,1):
            c = self.case('Dragon Mantle','pump_cycle',p)
            steps = c['steps']
            self.assertEqual(sum(s['op']=='activate' and s['ref']=='source' and s['value']==1 for s in steps),2)
            for v in (4,5):
                self.assertTrue(self.has(c,op='assert_card',ref='host',property='power',value=v))
            i = next(i for i,s in enumerate(steps) if s['op']=='cleanup')
            self.assertTrue(self.has(dict(steps=steps[i:]),op='assert_card',ref='host',property='power',value=3))
            self.assertTrue(self.has(c,op='assert_life',player=1-p,value=15))
            for model,value,debited in [('opposing_payer_funded',1,1-p),('opposing_payer_unfunded',0,p)]:
                probe = self.case('Dragon Mantle',model,p)
                self.assertTrue(self.has(probe,op='can_activate',ref='source',value=value))
                self.assertTrue(self.has(probe,op='set_mana',player=debited,color=4,value=1))
                self.assertFalse(self.has(probe,op='activate'))

    def test_fate_foretold_death_draw_follows_controller_and_grave_follows_owner(self):
        for p in (0,1):
            q = 1-p
            for mode,drawer,grave in [('own_death',p,p),('opposing_death',q,q),('foreign_owned_death',p,q)]:
                c = self.case('Fate Foretold',mode,p)
                self.assertTrue(self.has(c,op='cast',ref='removal',value=1))
                self.zone(c,drawer,'library',5 if drawer==p else 6)
                self.multiplicity(c,grave,'graveyard','Hill Giant',1)
                self.multiplicity(c,p,'graveyard','Fate Foretold',1)
            for mode in ('host_exiled','host_bounced','removed_before_death'):
                c = self.case('Fate Foretold',mode,p)
                self.assertFalse(self.has(c,op='assert_zone',player=p,zone='library',value=5))

    def test_combat_probes_and_protection_targeting(self):
        cases = {'Grisly Transformation':['intimidate_wrong_color','intimidate_same_color','intimidate_artifact'],
                 'Stratus Walk':['flying_ground_blocker','flying_reach_blocker','defend_ground','defend_flying'],
                 'Unquestioned Authority':['protection_attack','protection_defend'],
                 'Cartouche of Knowledge':['flying_ground_blocker','flying_reach_blocker']}
        for name,modes in cases.items():
            for mode in modes:
                _,defend,legal = family.COMBAT[mode]
                a,b = ('combatant','host') if defend else ('host','combatant')
                for p in (0,1):
                    c = self.case(name,mode,p)
                    self.assertTrue(self.has(c,op='attack',ref=a,value=1))
                    self.assertTrue(self.has(c,op='block',ref=b,other=a,value=int(legal)))
                    self.assertTrue(self.has(c,op='combat_damage'))
        c = self.case('Unquestioned Authority','protection_target')
        for source,target,legal in [('creature_source','host',0),('creature_source','alternate',1),('spell_source','host',1)]:
            self.assertTrue(self.has(c,op='can_target_creature',ref=source,other=target,value=legal))
        c = self.case('Unquestioned Authority','protection_defend')
        self.assertTrue(self.has(c,op='assert_card',ref='host',property='damage',value=0))
        self.assertTrue(self.has(c,op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Hill Giant']['csvid'],value=1))

    def test_cartouche_stats_removal_and_pending_replay(self):
        for p in (0,1):
            removed = self.case('Cartouche of Knowledge','aura_removed',p)
            for v in (3,4):
                self.assertTrue(self.has(removed,op='assert_card',ref='host',property='power',value=v))
            for name in self.sources:
                c = self.case(name,'pending_snapshot',p)
                ops = [s['op'] for s in c['steps']]
                self.assertLess(ops.index('cast'),ops.index('capture'))
                self.assertLess(ops.index('capture'),ops.index('resolve'))
                self.assertEqual(ops.count('resolve'),2)
                self.assertEqual(ops[-1],'assert_recorded_state')


if __name__=='__main__':
    unittest.main()
