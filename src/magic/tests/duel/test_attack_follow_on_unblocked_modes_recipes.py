"""Focused checks for selected unblocked combat and allocation effects."""
from copy import deepcopy
import unittest

import attack_follow_on_unblocked_modes_recipes as model


class UnblockedModesTests(unittest.TestCase):
    def scene(self, cid, mode, owner):
        pins = model.pins()['sources'] | model.pins()['witnesses']
        rows = {name: pin['registry'] for name, pin in pins.items()}
        oracle = {name: pin['oracle'] for name, pin in pins.items()}
        row, = (r for r in rows.values() if r['csvid'] == cid)
        scene = model.Scene(row, owner, mode, rows, oracle)
        scene.play()
        return scene

    def test_full_oracle_and_source_identity(self):
        self.assertEqual(set(model.MODES), {p['registry']['csvid'] for p in model.pins()['sources'].values()})
        for pin in model.pins()['sources'].values():
            record, cid = pin['oracle'], pin['registry']['csvid']
            model.classify(record, cid)
            changed = deepcopy(record)
            changed['oracle_text'] = changed['oracle_text'][1:]
            with self.assertRaisesRegex(ValueError, 'complete normal unblocked combat rules'):
                model.classify(changed, cid)

    def test_wurm_three_cards_and_either_player_own_cost(self):
        for seat in (0, 1):
            for mode in ('foe_pay', 'self_pay', 'block_foe_pay', 'block_self_pay'):
                scene = self.scene(model.WURM, mode, seat)
                who = seat if 'self_pay' in mode else 1-seat
                self.assertEqual(scene.zones[who]['exile'], ['Forest', 'Mountain', 'Hill Giant'])
                choices = [s['player'] for s in scene.steps if s['op']=='choose_zone_card']
                self.assertEqual(choices, [who] * 3)
            short = self.scene(model.WURM, 'short', seat)
            self.assertEqual(short.zones[1-seat]['graveyard'], ['Lightning Bolt', 'Forest'])
            self.assertFalse(any(s['op']=='choose_zone_card' for s in short.steps))
            self.assertEqual(short.life[1-seat],14)

    def test_zealot_damage_and_keeper_life_loss_use_distinct_combat_windows(self):
        for seat in (0, 1):
            zealot = self.scene(model.ZEALOT, 'nonlethal', seat)
            self.assertIn(dict(op='assert_card', ref='recipient', property='damage', value=3), zealot.steps)
            self.assertEqual(zealot.life[1-seat],2)
            decline = self.scene(model.ZEALOT, 'decline', seat)
            self.assertEqual(decline.life[1-seat],18)
            keeper = self.scene(model.KEEPER, 'unblocked', seat)
            pre = keeper.steps.index(dict(op='advance_combat'))
            self.assertEqual(keeper.steps[pre+3],dict(op='assert_life',player=1-seat,value=18))
            self.assertEqual(keeper.life[1-seat],18)

    def test_pirates_take_defender_artifact_and_preserve_ownership(self):
        for seat in (0, 1):
            takes = self.scene(model.PIRATES, 'artifact', seat)
            self.assertEqual(takes.zones[seat]['battlefield'], ['Darksteel Relic','Mind Stone'])
            self.assertIn('Kukemssa Pirates',takes.zones[seat]['graveyard'])
            self.assertEqual(takes.life[1-seat],20)
            returns = self.scene(model.PIRATES, 'borrow_owner', seat)
            self.assertEqual(returns.zones[1-seat]['hand'], ['Ornithopter'])
            self.assertEqual(returns.zones[seat]['battlefield'], ['Darksteel Relic','Kukemssa Pirates'])

    def test_hippo_drains_real_land_mana_into_second_main(self):
        for seat in (0, 1):
            hippo = self.scene(model.HIPPO, 'paid', seat)
            ops = [s['op'] for s in hippo.steps]
            phase = ops.index('end_combat_step')
            self.assertLess(ops.index('combat_damage'),phase)
            self.assertIn(dict(op='activate',ref='spent_land',value=1),hippo.steps)
            self.assertIn(dict(op='assert_mana',player=1-seat,color=3,value=1),hippo.steps[:phase])
            self.assertIn(dict(op='assert_mana',player=seat,color=0,value=0),hippo.steps[:phase])
            self.assertIn(dict(op='assert_mana',player=seat,color=0,value=3),hippo.steps[phase:])
            self.assertEqual(hippo.life[1-seat],20)

    def test_wildfire_casts_spell_and_afflicts_only_when_blocked(self):
        for seat in (0, 1):
            free = self.scene(model.WILDFIRE,'free_sorcery',seat)
            self.assertEqual(free.zones[seat]['hand'], ['Island','Mountain'])
            self.assertEqual(free.zones[seat]['graveyard'], ['Divination'])
            self.assertEqual(free.life[1-seat],19)
            self.assertIn(dict(op='choose_target',ref='free_spell',player=seat),free.steps)
            blocked = self.scene(model.WILDFIRE,'blocked_afflict',seat)
            self.assertEqual(blocked.life[1-seat],16)
            self.assertEqual(blocked.zones[seat]['hand'], ['Reach Through Mists'])
            self.assertFalse(any(s.get('ref')=='free_spell' for s in blocked.steps))
