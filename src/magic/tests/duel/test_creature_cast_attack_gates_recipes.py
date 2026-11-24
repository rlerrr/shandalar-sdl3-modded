"""Guard real spell and combat histories, inheritance, and paired seat receipts."""
import json
import unittest

import creature_cast_attack_gates_recipes as model
from generate_cases import registry


class CreatureCastAttackGatesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']:r for r in registry()}

    def case(self, card, variant):
        source = self.oracle[card]
        params = model.parameters(source)
        declaration = dict(family=model.FAMILY, version=2, oracle_text=source['oracle_text'], params=params, variants=[variant])
        return model.expand(card,self.rows[card]['csvid'],declaration,model.spec(declaration,source),
                            [variant],self.rows,self.oracle)[0]

    @staticmethod
    def values(steps, op, **fields):
        return [s['value'] for s in steps if s['op']==op and all(s.get(k)==v for k,v in fields.items())]

    def test_full_printed_clauses_and_implemented_alias(self):
        model.validate_witnesses(self.oracle,self.rows)
        self.assertEqual({pin['registry']['csvid'] for pin in model.profiles()['sources'].values()}, {650,3207,7477,11089,15950})
        self.assertEqual(self.rows['Mogg Conscripts']['handler'],self.rows['Goblin Cohort']['handler'])
        for name,pin in model.profiles()['sources'].items():
            self.assertEqual(pin['oracle'],self.oracle[name])
            with self.assertRaises(ValueError):
                model.parameters(dict(self.oracle[name],oracle_text=pin['oracle']['oracle_text']+'\nDraw a card.'))
            steps=self.case(name,'wrong_color_1')['steps']
            self.assertIn(dict(op='cast',ref='source',value=0),steps)

    def test_type_player_and_same_turn_are_independent(self):
        for name in ('Mogg Conscripts','Goblin Cohort','Silburlind Snapper'):
            for owner in (0,1):
                for mode,expected in [('no_cast',0),('match',1),('mismatch',0),('prior_turn',0),('opponent_cast',0),('failed_cast',0)]:
                    body=self.case(name,f'{mode}_{owner}')
                    steps=body['steps']
                    self.assertEqual(self.values(steps,'attack',ref='source'),[expected])
                    if mode=='opponent_cast':
                        witness=next(s for s in body['setup'] if s['ref']=='witness')
                        self.assertEqual(witness['player'],1-owner)
                        self.assertEqual(self.values(steps,'turn'),[0])
                        self.assertEqual(len([s for s in steps if s['op']=='advance_turn']),0)
                    if mode=='prior_turn':
                        self.assertEqual(len([s for s in steps if s['op']=='advance_turn']),2)
                        self.assertLess(steps.index(dict(op='announce_spell',ref='witness')),
                                        next(i for i,s in enumerate(steps) if s['op']=='advance_turn'))

    def test_countered_cast_qualifies_without_resolving(self):
        for name in ('Goblin Cohort','Silburlind Snapper'):
            for owner in (0,1):
                body=self.case(name,f'countered_match_{owner}')
                steps=body['steps']
                self.assertLess(steps.index(dict(op='announce_spell',ref='witness')),
                                steps.index(dict(op='cast_response',ref='reply',value=1)))
                self.assertLess(steps.index(dict(op='resolve_response')),steps.index(dict(op='attack',ref='source',value=1)))
                witness=next(s['name'] for s in body['setup'] if s['ref']=='witness')
                self.assertEqual(self.values(steps,'assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[witness]['csvid']),[1,1])
                self.assertEqual(self.values(steps,'assert_life',player=1-owner)[0],20)

    def test_land_creature_and_paid_haste_prove_distinct_origins(self):
        for owner in (0,1):
            for name in ('Mogg Conscripts','Silburlind Snapper'):
                body=self.case(name,f'creature_land_{owner}')
                steps=body['steps']
                self.assertIn(('Dryad Arbor','land'),[(s['name'],s['ref']) for s in body['setup']])
                self.assertIn(dict(op='play_land',ref='land',value=1),steps)
                self.assertEqual(self.values(steps,'attack',ref='source'),[0])
                self.assertFalse(any(s['op']=='announce_spell' for s in steps))
                haste=self.case(name,f'self_cast_haste_{owner}')['steps']
                self.assertIn(dict(op='announce_spell',ref='source'),haste)
                self.assertEqual(self.values(haste,'attack',ref='source'),[int(name=='Mogg Conscripts')])

    def test_turtle_uses_real_prior_attacks_and_real_untap(self):
        for owner in (0,1):
            body=self.case('Giant Turtle',f'skip_then_resume_{owner}')['steps']
            self.assertEqual(self.values(body,'attack',ref='source'),[1,0,1])
            self.assertEqual([s['player'] for s in body if s['op']=='untap_step'],[1-owner,owner,1-owner,owner])
            blocked=self.case('Giant Turtle',f'blocking_{owner}')['steps']
            self.assertIn(dict(op='block',ref='source',other='actor',value=1),blocked)
            self.assertEqual(self.values(blocked,'attack',ref='source'),[1])

    def test_arbiter_both_directions_exempt_land_and_other_controller(self):
        for owner in (0,1):
            cast=self.case('Angelic Arbiter',f'countered_cast_{owner}')['steps']
            self.assertIn(dict(op='resolve_response'),cast)
            self.assertEqual(self.values(cast,'attack',ref='actor'),[0])
            before=self.case('Angelic Arbiter',f'attack_then_instant_{owner}')['steps']
            attack=before.index(dict(op='attack',ref='actor',value=1))
            stop=before.index(dict(op='cast',ref='instant',value=0))
            self.assertLess(attack,stop)
            self.assertGreater(before.index(dict(op='begin_blockers')),stop)
            own=self.case('Angelic Arbiter',f'other_cast_after_attack_{owner}')['steps']
            self.assertIn(dict(op='cast',ref='instant',value=1),own)
            self.assertEqual(self.values(own,'assert_zone_multiplicity',player=owner,zone='hand',index=126)[-1],1)
            land=self.case('Angelic Arbiter',f'attack_then_land_{owner}')['steps']
            self.assertGreater(land.index(dict(op='play_land',ref='land',value=1)),land.index(dict(op='end_combat')))
            flying=self.case('Angelic Arbiter',f'flying_{owner}')['steps']
            self.assertEqual(self.values(flying,'block',ref='ground',other='source'),[0])
            self.assertEqual(self.values(flying,'block',ref='reach',other='source'),[1])

    def test_snapshots_restore_genuine_history(self):
        for name in model.profiles()['sources']:
            steps=self.case(name,'snapshot_1')['steps']
            self.assertEqual(sum(s['op']=='capture' for s in steps),1)
            self.assertLess(next(i for i,s in enumerate(steps) if s['op'] in ('announce_spell','end_combat')),
                            steps.index(dict(op='capture')))
            self.assertEqual(steps[-1],dict(op='assert_recorded_state'))
