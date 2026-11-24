import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
from recipes import expand
import sacrifice_damage_activation_recipes as family


class SacrificeDamageActivationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.duel = Path(__file__).parent
        cls.oracle = json.loads((cls.duel/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry = {row['name']:row for row in registry()}
        cls.sources = family.pins()['sources']

    def declaration(self, name, selected):
        record = self.oracle[name]
        return dict(family=family.FAMILY, version=2, oracle_text=record['oracle_text'],
                    params=dict(schema=1, **family.oracle_model(record)), variants=selected)

    def case(self, name, mode, p=0):
        declaration = self.declaration(name, [f'{mode}_{p}'])
        return expand(name, self.registry[name]['csvid'], declaration, self.oracle[name], self.registry, self.oracle)[0]

    def has(self, steps, **fields):
        return any(all(s.get(k) == v for k, v in fields.items()) for s in steps)

    def zone(self, steps, name, player, zone, value):
        return self.has(steps, op='assert_zone_multiplicity', player=player, zone=zone,
                        index=self.registry[name]['csvid'], value=value)

    def test_exact_whole_pins_and_printed_costs(self):
        family.validate_witnesses(self.oracle, self.registry)
        expected = {'Goblin Bombardment':('',False,'creature',1), 'Barrage of Expendables':('{R}',False,'creature',1),
                    'Skirsdag Cultist':('{R}',True,'creature',2), 'Skull Catapult':('{1}',True,'creature',2),
                    'Barrage Ogre':('',True,'artifact',2), 'Orcish Vandal':('',True,'artifact',2),
                    'Orcish Mechanics':('',True,'artifact',2)}
        self.assertEqual(set(expected), set(self.sources))
        for n, values in expected.items():
            model = family.oracle_model(self.oracle[n])
            self.assertEqual(tuple(model[x] for x in ('activation_cost','tap','sacrifice','damage')), values)
            d = self.declaration(n, ['activation_0']); d['params']['tap'] = int(model['tap'])
            with self.assertRaises(ValueError): family.spec(d, self.oracle[n])
            with self.assertRaises(ValueError): family.oracle_model(dict(self.oracle[n], oracle_text=self.oracle[n]['oracle_text']+' Draw a card.'))
        changed = copy.deepcopy(self.oracle)
        changed['Safe Passage']['oracle_text'] = 'changed'
        with self.assertRaises(ValueError): family.validate_witnesses(changed, self.registry)

    def test_all_reserved_homes_public_generation_without_seeded_rules_state(self):
        total = 0
        for n, pin in self.sources.items():
            ident = pin['registry']['csvid']
            path = self.duel/f'data/cases/cards/{ident//100:03}/{ident:05}-{n.lower().replace(" ", "-")}.json'
            declaration, = json.loads(path.read_text(encoding='utf8'))['recipes']
            self.assertEqual(declaration['variants'], family.variants(family.spec(declaration, self.oracle[n])))
            cases = expand(n, ident, declaration, self.oracle[n], self.registry, self.oracle)
            for c in cases:
                self.assertTrue(self.has(c['steps'], op='assert_choices_consumed'))
                self.assertFalse(any(s['op'] in ('destroy','move_card','set_counter','dispatch_gfp','set_life') for s in c['steps']))
            total += len(cases)
        self.assertEqual(total,349)

    def test_human_cost_and_damage_input_order_matches_each_real_helper(self):
        for n, pin in self.sources.items():
            c = self.case(n, 'activation')
            choices = [s['ref'] for s in c['steps'] if s['op']=='choose_target']
            self.assertEqual(choices, ['fodder','recipient'] if pin['sacrifice_first'] else ['recipient','fodder'])
            c = self.case(n, 'activation', 1)
            self.assertEqual([s['ref'] for s in c['steps'] if s['op']=='choose_target'], ['recipient'])

    def test_costs_are_paid_on_pending_stack_and_damage_waits_for_resolution(self):
        for n in self.sources:
            p = family.oracle_model(self.oracle[n]); c = self.case(n,'activation'); steps = c['steps']
            at = next(i for i,s in enumerate(steps) if s['op']=='activate')
            resolved = next(i for i,s in enumerate(steps[at:],at) if s['op']=='resolve')
            pending = steps[at:resolved]
            self.assertTrue(self.has(pending,op='assert_stack',value=1))
            victim = 'Muck Rats' if p['sacrifice']=='creature' else 'Howling Mine'
            self.assertTrue(self.zone(pending,victim,0,'graveyard',1))
            self.assertTrue(self.has(pending,op='assert_card',ref='source',property='tapped',value=int(p['tap'])))
            self.assertTrue(self.has(pending,op='assert_card',ref='recipient',property='damage',value=0))
            self.assertTrue(self.has(steps[resolved:],op='assert_card',ref='recipient',property='damage',value=p['damage']))
            self.assertTrue(all(self.has(pending,op='assert_mana',player=0,color=i,value=0) for i in range(6)))

    def test_sacrificed_controlled_card_goes_to_owner_and_token_does_not_remain(self):
        for n in self.sources:
            victim = 'Muck Rats' if family.oracle_model(self.oracle[n])['sacrifice']=='creature' else 'Howling Mine'
            c = self.case(n,'stolen_cost')
            self.assertTrue(any(s['ref']=='fodder' and 'opponent_owned' in s['state'] for s in c['setup']))
            self.assertTrue(self.zone(c['steps'],victim,1,'graveyard',1))
            self.assertTrue(self.zone(c['steps'],victim,0,'graveyard',0))
            c = self.case(n,'token_cost')
            self.assertTrue(any(s['ref']=='fodder' and 'token' in s['state'] for s in c['setup']))
            self.assertTrue(self.zone(c['steps'],victim,0,'graveyard',0))

    def test_cultist_can_tap_and_sacrifice_itself_before_own_damage(self):
        for p in (0,1):
            c = self.case('Skirsdag Cultist','self_cost',p); steps = c['steps']
            at = next(i for i,s in enumerate(steps) if s['op']=='resolve')
            self.assertTrue(self.zone(steps[:at],'Skirsdag Cultist',p,'graveyard',1))
            self.assertTrue(self.has(steps[:at],op='assert_life',player=1-p,value=20))
            self.assertTrue(self.has(steps[at:],op='assert_life',player=1-p,value=18))
            self.assertEqual(self.has(steps,op='choose_target',player=p,ref='source'),p==0)

    def test_actual_damage_to_player_planeswalker_and_lethal_indestructible_creatures(self):
        for n in self.sources:
            amount = family.oracle_model(self.oracle[n])['damage']
            for mode, player in [('opponent_player',1),('self_player',0)]:
                c = self.case(n,mode)
                self.assertTrue(self.has(c['steps'],op='assert_life',player=player,value=20-amount))
            c = self.case(n,'planeswalker'); loyalty = int(self.oracle['Chandra, Pyrogenius']['loyalty'])
            self.assertTrue(self.has(c['steps'],op='assert_counter',ref='recipient',counter_kind='loyalty',value=loyalty-amount))
            c = self.case(n,'lethal'); self.assertTrue(self.zone(c['steps'],'Muck Rats',1,'graveyard',1))
            c = self.case(n,'indestructible')
            self.assertTrue(self.has(c['steps'],op='assert_card',ref='recipient',property='damage',value=amount))
            self.assertTrue(self.zone(c['steps'],'Darksteel Myr',1,'battlefield',1))

    def test_rejection_has_no_interaction_or_partial_cost_and_cancel_keeps_fodder(self):
        c = self.case('Barrage Ogre','wrong_type')
        self.assertTrue(self.has(c['steps'],op='activate',ref='source',value=0))
        self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
        self.assertTrue(self.zone(c['steps'],'Muck Rats',0,'battlefield',1))
        for mode in ('cancel_damage_target','cancel_cost'):
            c = self.case('Goblin Bombardment',mode)
            self.assertTrue(self.has(c['steps'],op='choose_cancel_target',player=0))
            self.assertTrue(self.zone(c['steps'],'Muck Rats',0,'battlefield',1))
            self.assertTrue(self.zone(c['steps'],'Muck Rats',0,'graveyard',0))

    def test_real_prevention_and_counterspell_context(self):
        for n in self.sources:
            c = self.case(n,'prevention')
            self.assertTrue(self.has(c['steps'],op='cast',ref='shield',value=1))
            self.assertFalse(any(s['op']=='assert_card' and s.get('ref')=='recipient' and s.get('property')=='damage' and s.get('value') for s in c['steps']))
            c = self.case(n,'countered'); ops = [s['op'] for s in c['steps']]
            self.assertLess(ops.index('announce_spell'),ops.index('cast_response'))
            self.assertNotIn('choose_target',ops)
            self.assertTrue(self.zone(c['steps'],n,0,'graveyard',1))

    def test_pending_snapshot_replays_damage_after_cost_and_uses_no_new_input(self):
        c = self.case('Barrage Ogre','pending_snapshot',1); steps = c['steps']; ops = [s['op'] for s in steps]
        self.assertLess(ops.index('activate'),ops.index('capture'))
        self.assertLess(ops.index('capture'),ops.index('resolve'))
        self.assertEqual(ops.count('activate'),1)
        self.assertEqual(ops.count('resolve'),2)
        self.assertTrue(self.has(steps,op='assert_recorded_state'))


if __name__ == '__main__':
    unittest.main()
