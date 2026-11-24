"""Focused attack trigger checks for exact input and owner boundaries."""
from copy import deepcopy
import unittest

import attack_trigger_zones_and_library_recipes as model


class AttackZoneTests(unittest.TestCase):
    def scene(self, cid, mode, owner):
        pins = model.pins()['sources'] | model.pins()['witnesses']
        rows = {name: pin['registry'] for name, pin in pins.items()}
        oracle = {name: pin['oracle'] for name, pin in pins.items()}
        row, = (row for row in rows.values() if row['csvid'] == cid)
        play = model.Scene(row, owner, mode, rows, oracle)
        play.play()
        return play

    def test_complete_pinned_rules_required(self):
        self.assertEqual(set(model.MODES), {r['registry']['csvid'] for r in model.pins()['sources'].values()})
        for pin in model.pins()['sources'].values():
            record = pin['oracle']
            model.classify(record, pin['registry']['csvid'])
            changed = deepcopy(record)
            changed['oracle_text'] = changed['oracle_text'][1:]
            with self.assertRaisesRegex(ValueError, 'complete normal attack zone and library rules'):
                model.classify(changed, pin['registry']['csvid'])

    def test_whale_returns_on_departure_and_counts_defender_island(self):
        for seat in (0, 1):
            stayed = self.scene(model.WHALE, 'exile_stays', seat)
            departed = self.scene(model.WHALE, 'owner_return', seat)
            self.assertEqual(stayed.zones[1-seat]['exile'], ['Walking Corpse'])
            self.assertEqual(departed.zones[seat]['battlefield'], ['Walking Corpse'])
            self.assertEqual(departed.zones[1-seat]['exile'], [])
            for mode, life in (('islandwalk', 15), ('no_island', 20)):
                play = self.scene(model.WHALE, mode, seat)
                self.assertIn(dict(op='block', ref='blocker', other='source', value=int(mode=='no_island')),
                              play.steps)
                self.assertEqual(play.life[1-seat], life)

    def test_scuttler_and_grave_rules_use_distinct_owners(self):
        for seat in (0, 1):
            control = self.scene(model.SCUTTLER, 'borrowed', seat)
            self.assertEqual(control.zones[1-seat]['hand'], ['Ornithopter'])
            sentinel = self.scene(model.SENTINEL, 'enchantment', seat)
            self.assertIn('Glorious Anthem', sentinel.zones[seat]['battlefield'])
            miss = self.scene(model.SENTINEL, 'wrong_owner', seat)
            self.assertEqual(miss.zones[1-seat]['graveyard'], ['Fervor'])
            shroud = self.scene(model.THRULL, 'shroud', seat)
            self.assertIn('Glorious Anthem', shroud.zones[seat]['graveyard'])
            paid = self.scene(model.THRULL, 'extort_black', seat)
            self.assertEqual(paid.life[seat], 21)
            self.assertEqual(paid.life[1-seat], 15)
            self.assertIn('Glorious Anthem', paid.zones[seat]['hand'])

    def test_identity_exiles_only_nontoken_and_restores_source(self):
        for seat in (0, 1):
            play = self.scene(model.THIEF, 'copy', seat)
            self.assertEqual(play.zones[1-seat]['battlefield'], ['Hill Giant'])
            self.assertEqual(play.zones[1-seat]['exile'], [])
            source_ids = [s['value'] for s in play.steps if s.get('property')=='csvid' and s['ref']=='source']
            self.assertEqual(source_ids, [model.THIEF, 110, model.THIEF])
            tokens = self.scene(model.THIEF, 'token_only', seat)
            self.assertEqual(tokens.zones[seat]['battlefield'].count('Soldier'), 2)
            self.assertFalse(any(s['op']=='choose_target' for s in tokens.steps))

    def test_bane_uses_defender_choices_and_sage_compares_power(self):
        for seat in (0, 1):
            bane = self.scene(model.BANE, 'two', seat)
            picks = [s for s in bane.steps if s['op']=='choose_target']
            self.assertEqual(picks, [dict(op='choose_target', player=1-seat, ref=ref) for ref in ('first','second')])
            self.assertEqual(bane.zones[1-seat]['exile'], ['Forest','Darksteel Relic'])
            equal = self.scene(model.SAGE,'equal',seat)
            pumped = self.scene(model.SAGE,'prowess',seat)
            self.assertEqual(equal.zones[1-seat]['battlefield'], ['Lightning Elemental'])
            self.assertEqual(pumped.zones[1-seat]['hand'], ['Lightning Elemental'])
            self.assertEqual([s['value'] for s in pumped.steps if s.get('property')=='power'], [4,5,4])

    def test_draw_discard_and_linked_couriers_preserve_library_order(self):
        for seat in (0, 1):
            dragon = self.scene(model.DRAGON,'large_discard',seat)
            self.assertEqual(dragon.zones[seat]['library'], ['Forest'])
            self.assertEqual(dragon.zones[seat]['hand'], ['Island'])
            self.assertEqual(dragon.zones[seat]['graveyard'], ['Hill Giant'])
            self.assertEqual([s['value'] for s in dragon.steps if s.get('property')=='power'], [3,7,3])
            courier = self.scene(model.BOMAT,'two_couriers',seat)
            self.assertEqual(courier.zones[seat]['hand'], ['Forest'])
            self.assertEqual(courier.zones[seat]['exile'], ['Plains','Lightning Bolt'])
            self.assertEqual(courier.zones[seat]['library'], ['Island'])
            self.assertEqual(courier.zones[seat]['graveyard'], ['Bomat Courier','Mountain','Swamp'])
            self.assertEqual(courier.zones[seat]['battlefield'], ['Bomat Courier'])

    def test_sphinx_exact_name_mirroring_and_library_order(self):
        for seat in (0,1):
            for mode, human_hand, human_library in (
                ('match', ['Swamp','Lightning Bolt'], ['Plains','Forest']),
                ('miss', ['Swamp'], ['Plains','Forest','Lightning Bolt']),
            ):
                scene = self.scene(model.SPHINX,mode,seat)
                self.assertEqual(scene.zones[0]['hand'], human_hand)
                self.assertEqual(scene.zones[0]['library'], human_library)
                self.assertEqual(scene.zones[1]['hand'], ['Island'])
                self.assertEqual(scene.zones[1]['library'], ['Mountain','Unsummon'])
                decisions = [s['op'] for s in scene.steps if s['op'] in ('choose_card_name','choose_dialog')]
                self.assertEqual(decisions, ['choose_card_name','choose_dialog','choose_dialog'] if seat == 0
                                 else ['choose_dialog','choose_card_name','choose_dialog'])
                self.assertTrue(all(s['player']==0 for s in scene.steps if s['op']=='choose_card_name'))
            for mode in ('other_attacks','short_fee'):
                scene = self.scene(model.SPHINX,mode,seat)
                self.assertFalse(any(s['op']=='choose_card_name' for s in scene.steps))
                self.assertEqual(scene.zones[1]['library'], ['Island','Mountain','Unsummon'])

    def test_identity_battlefield_receipts_use_the_visible_copy_only_before_cleanup(self):
        for actor in (0, 1):
            for mode, copied in (('copy', 110), ('copy_flying', 1197)):
                case = self.scene(model.THIEF, mode, actor)
                start = next(i for i, s in enumerate(case.steps)
                             if s.get('ref') == 'source' and s.get('property') == 'csvid' and s['value'] == copied)
                end = next(i for i, s in enumerate(case.steps) if s['op'] == 'cleanup')
                def values(steps, cid):
                    return [s['value'] for s in steps if s['op'] == 'assert_zone_multiplicity'
                            and s['player'] == actor and s['zone'] == 'battlefield' and s['index'] == cid]
                self.assertEqual(values(case.steps[start:end], model.THIEF), [0, 0])
                self.assertEqual(values(case.steps[start:end], copied), [1, 1])
                self.assertEqual(values(case.steps[end:], model.THIEF), [1])
                self.assertEqual(values(case.steps[end:], copied), [int(mode == 'copy_flying')])
