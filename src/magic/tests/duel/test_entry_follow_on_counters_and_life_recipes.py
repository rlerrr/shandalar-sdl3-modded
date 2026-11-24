"""Guard real departure, source payments, devotion and combat receipts."""
from copy import deepcopy
import unittest

from entry_follow_on_counters_and_life_recipes import (
    Board, DELUSIONS, GLIMMERPOST, HUNTER, WURM, FAMILY, CLOUDPOST,
    profiles, spec, classify, validate_witnesses, variants,
)


class CountersAndLifePilotTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pinned = {**profiles()['sources'], **profiles()['witnesses']}
        cls.oracle = {name: row['oracle'] for name, row in pinned.items()}
        cls.rows = {name: row['registry'] for name, row in pinned.items()}
        cls.names = {row['registry']['csvid']: name for name, row in profiles()['sources'].items()}

    def case(self, csvid, mode, seat):
        game = Board(self.rows[self.names[csvid]], seat, mode, self.rows, self.oracle)
        game.run()
        return game

    def test_exact_primary_and_typed_variant_options(self):
        record = self.oracle[self.names[GLIMMERPOST]]
        shape = classify(record, GLIMMERPOST)
        recipe = dict(family=FAMILY, version=2, params=shape, variants=variants(shape), oracle_text=record['oracle_text'])
        self.assertEqual(spec(recipe, record), dict(shape, family=FAMILY))
        for bad in (dict(recipe, version=2.0), dict(recipe, params=dict(shape, csvid=float(GLIMMERPOST))),
                    dict(recipe, variants=['mana_0', 'mana_0'])):
            with self.assertRaises(ValueError):
                spec(bad, record)
        with self.assertRaises(ValueError):
            spec(recipe, dict(record, oracle_text=record['oracle_text']+' '))

    def test_counting_witness_is_entire_exact_locus_record(self):
        validate_witnesses(self.oracle, self.rows)
        changed = deepcopy(self.oracle)
        changed[CLOUDPOST]['type_line'] = 'Land'
        with self.assertRaisesRegex(ValueError, CLOUDPOST):
            validate_witnesses(changed, self.rows)

    def test_leave_life_and_replay(self):
        for seat in (0, 1):
            case = self.case(DELUSIONS, 'replay_departure', seat)
            wallets = [op['value'] for op in case.steps if op['op'] == 'assert_life' and op['player'] == seat]
            self.assertEqual(wallets, [20,30,20,30,20])
            self.assertEqual(case.cards['source']['zone'], 'graveyard')
            self.assertEqual(self.case(DELUSIONS, 'exile', seat).cards['source']['zone'], 'exile')
            self.assertEqual(self.case(DELUSIONS, 'recast', seat).life[seat], 30)

    def test_locus_count_includes_both_seats_and_real_land_limit(self):
        for seat in (0, 1):
            for mode, gained in (('solo',1),('friendly',2),('opposing',2),('mixed',3),('second_turn',3)):
                case = self.case(GLIMMERPOST, mode, seat)
                self.assertEqual(case.life[seat], 20+gained)
                if mode == 'second_turn':
                    self.assertEqual([op['value'] for op in case.steps if op['op'] == 'play_land'], [1,0,1])

    def test_locus_generated_mana_pays_without_refunding(self):
        for seat in (0, 1):
            case = self.case(GLIMMERPOST, 'mana', seat)
            start = next(i for i, op in enumerate(case.steps) if op['op'] == 'activate')
            end = next(i for i, op in enumerate(case.steps) if op['op'] == 'cast' and op['ref'] == 'colorless_purchase')
            self.assertFalse(any(op['op'] == 'set_mana' for op in case.steps[start:end]))
            self.assertEqual([op['value'] for op in case.steps if op['op'] == 'can_activate'], [1,0])

    def test_devotion_uses_live_printed_pips_on_owner_only(self):
        for seat in (0, 1):
            for mode, counters in (('alone',[1]),('owner',[4]),('foe',[1]),('mixed',[6]),('recast',[4,4,1])):
                case = self.case(HUNTER, mode, seat)
                self.assertEqual([op['value'] for op in case.steps if op['op'] == 'assert_counter' and op['ref']=='source'],counters)

    def test_wurm_trample_and_real_dies_draw(self):
        for seat in (0, 1):
            for mode, foe_life, drawn in (('trample',15,False),('prevented',20,False),
                                          ('combat_death',20,True),('die_draw',20,True),('bounce',20,False)):
                case = self.case(WURM, mode, seat)
                self.assertEqual(case.life, [27,foe_life] if seat==0 else [foe_life,27])
                self.assertEqual(case.cards[f'first_{seat}']['zone'], 'hand' if drawn else 'library')

    def test_abzan_creates_real_flying_spirit_after_own_nontoken_death(self):
        from entry_follow_on_counters_and_life_recipes import ABZAN
        for seat in (0, 1):
            case = self.case(ABZAN, 'own_death', seat)
            self.assertEqual(case.life[1-seat], 19)
            self.assertEqual(case.cards['spirit']['name'], 'Spirit')
            self.assertEqual(case.cards['ally']['zone'], 'graveyard')
            for mode in ('enemy_death', 'token_death'):
                self.assertNotIn('spirit', self.case(ABZAN, mode, seat).cards)

    def test_distinct_phyrexian_cost_choices(self):
        from entry_follow_on_counters_and_life_recipes import PITH
        for seat in (0, 1):
            for mode, lost in (('life',2),('black',0)):
                case = self.case(PITH, mode, seat)
                self.assertEqual(case.life[seat], 20-lost)
                self.assertEqual([op['value'] for op in case.steps if op['op'] == 'choose_number'], [lost//2])

    def test_demon_guard_applies_separately_to_entry_and_leave(self):
        from entry_follow_on_counters_and_life_recipes import ONI
        for seat in (0, 1):
            for mode, remaining in (('no_demon',14),('own_demon',20),('foe_demon',14),('exorcised',17)):
                self.assertEqual(self.case(ONI, mode, seat).life[seat], remaining)

    def test_sanctuary_only_watches_own_angels_and_produces_colorless(self):
        from entry_follow_on_counters_and_life_recipes import SANCTUARY
        for seat in (0, 1):
            self.assertEqual(self.case(SANCTUARY, 'own_angel', seat).life[seat], 22)
            for mode in ('foe_angel','nonangel','mana'):
                self.assertEqual(self.case(SANCTUARY, mode, seat).life[seat], 21)

    def test_bolster_tie_and_support_bounded_optional_targets(self):
        from entry_follow_on_counters_and_life_recipes import LAGAC, RELIEF, SANDCRAFTER
        for seat in (0, 1):
            tie = self.case(SANDCRAFTER, 'tie', seat)
            self.assertEqual([op['ref'] for op in tie.steps if op['op'] == 'choose_target'], ['chosen'])
            for source, maximum in ((RELIEF,3),(LAGAC,2)):
                part = self.case(source, 'partial', seat)
                self.assertEqual(sum(op['op'] == 'choose_optional_target' for op in part.steps), maximum-1)
                cap = self.case(source, 'cap', seat)
                self.assertEqual(sum(op['op'] == 'choose_target' for op in cap.steps), maximum)

    def test_counter_recipient_scope_follows_owned_types_and_existing_counters(self):
        from entry_follow_on_counters_and_life_recipes import PATRON, STROMKIRK, THALIA
        for seat in (0, 1):
            patron = self.case(PATRON, 'boost', seat)
            self.assertEqual([op['value'] for op in patron.steps if op['op']=='assert_counter' and op['ref']=='with_counter'],[2,3])
            vampire = self.case(STROMKIRK, 'scope', seat)
            self.assertEqual([op['ref'] for op in vampire.steps if op['op']=='choose_target'],['own_vampire'])
            for mode, gained in (('own_entry',1),('foe_entry',0),('nonhuman',0)):
                human = self.case(THALIA, mode, seat)
                self.assertEqual([op['value'] for op in human.steps if op['op']=='assert_counter' and op['ref']=='source'],[0,gained])

    def test_ammit_lifelink_and_crocodile_haste_in_real_combat(self):
        from entry_follow_on_counters_and_life_recipes import AMMIT, CROCODILE
        for seat in (0, 1):
            ammit = self.case(AMMIT, 'combat', seat)
            self.assertEqual(ammit.life[seat], 24)
            self.assertEqual(ammit.life[1-seat], 20)
            crocodile = self.case(CROCODILE, 'combat', seat)
            self.assertEqual(crocodile.life[1-seat],15)
            self.assertFalse(any(op['op']=='advance_turn' for op in crocodile.steps))
