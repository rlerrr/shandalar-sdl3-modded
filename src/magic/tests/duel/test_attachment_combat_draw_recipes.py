import json
import unittest
from pathlib import Path

import attachment_combat_draw_recipes as family
from generate_cases import registry
from recipes import expand


class AttachmentCombatDrawTests(unittest.TestCase):
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

    def check_zone(self,case,p,zone,size):
        self.assertTrue(self.has(case,op='assert_zone',player=p,zone=zone,value=size))

    def test_complete_pinned_grammar_excludes_other_attachment_effects(self):
        family.validate_witnesses(self.oracle,self.rows)
        for name in self.sources:
            r = self.oracle[name]
            d = self.declaration(name)
            self.assertEqual(family.spec(d,r)['family'],family.FAMILY)
            with self.assertRaises(ValueError):
                family.parameters(dict(r,oracle_text=r['oracle_text']+'\nDraw a card.'))
        for name in ('Mask of Memory','Curious Obsession','Sword of Fire and Ice'):
            with self.assertRaises(ValueError): family.parameters(self.oracle[name])

    def test_matrix_covers_both_seats_and_human_optional_decline(self):
        total = 0
        for name in self.sources:
            d = self.declaration(name)
            self.assertEqual(d['variants'],family.variants(family.spec(d,self.oracle[name])))
            cases = expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,self.oracle)
            total += len(cases)
            self.assertFalse(any(s['op'] in ('set_life','set_counter','move_card','dispatch_gfp') for c in cases for s in c['steps']))
        self.assertEqual(total,104)

    def test_paid_real_equipment_and_aura_precede_combat(self):
        for name in self.sources:
            for p in (0,1):
                c = self.case(name,'hit',p)
                ops = [s['op'] for s in c['steps']]
                self.assertLess(ops.index('cast'),ops.index('combat_damage'))
                self.assertTrue(self.has(c,op='choose_target',ref='host',player=p))
                self.assertTrue(self.has(c,op='assert_life',player=1-p,value=18))
                self.check_zone(c,p,'hand',5)
                self.check_zone(c,p,'library',6)
                if family.parameters(self.oracle[name])['equip']:
                    self.assertLess(ops.index('activate'),ops.index('combat_damage'))
                    self.assertTrue(self.has(c,op='set_mana',player=p,color=0,value=2))
                    self.assertTrue(self.has(self.case(name,'equip_no_own',p),op='require_can_activate',ref='source',value=0))
                    self.check_zone(self.case(name,'moved',p),p,'library',7)

    def test_fear_rejects_green_and_accepts_black_and_artifact_blockers(self):
        for name in ('Mask of Riddles',"Sleeper's Robe"):
            for p in (0,1):
                for mode,legal,library in [('fear_green',0,6),('blocked',1,7),('fear_artifact',1,7)]:
                    c = self.case(name,mode,p)
                    self.assertTrue(self.has(c,op='block',ref='blocker',other='host',value=legal))
                    self.check_zone(c,p,'library',library)

    def test_draw_requires_live_attachment_and_player_combat_damage(self):
        for name in self.sources:
            for p in (0,1):
                c = self.case(name,'noncombat',p)
                self.assertTrue(self.has(c,op='activate',ref='grant',value=1))
                self.assertTrue(self.has(c,op='assert_life',player=1-p,value=19))
                self.assertFalse(self.has(c,op='combat_damage'))
                for mode in ('noncombat','other_attacker','source_removed'):
                    self.check_zone(self.case(name,mode,p),p,'library',7)
                decline = self.case(name,'decline')
                self.assertTrue(self.has(decline,op='combat_damage'))
                self.assertFalse(self.has(decline,op='choose_trigger_source'))
                self.check_zone(decline,0,'library',7)

    def test_granted_trigger_follows_host_and_robe_tests_aura_controllers_opponent(self):
        for p in (0,1):
            q = 1-p
            sixth = self.case('Sixth Sense','opposing_host',p)
            self.assertTrue(self.has(sixth,op='assert_life',player=p,value=18))
            self.check_zone(sixth,q,'library',6)
            self.check_zone(sixth,p,'library',7)
            if q==0: self.assertTrue(self.has(sixth,op='choose_trigger_source',player=q,ref='host'))
            robe = self.case("Sleeper's Robe",'opposing_host',p)
            self.assertTrue(self.has(robe,op='assert_life',player=p,value=18))
            for player in (p,q): self.check_zone(robe,player,'library',7)
            if p==0: self.assertTrue(self.has(robe,op='assert_unused_choices',value=1))

    def test_damage_replay_captures_real_declared_combat(self):
        for name in self.sources:
            c = self.case(name,'damage_snapshot')
            steps = [s['op'] for s in c['steps']]
            self.assertLess(steps.index('advance_combat'),steps.index('capture'))
            self.assertLess(steps.index('capture'),steps.index('combat_damage'))
            self.assertEqual(steps.count('combat_damage'),2)
            self.assertEqual(steps[-1],'assert_recorded_state')


if __name__=='__main__':
    unittest.main()
