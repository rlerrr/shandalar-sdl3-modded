"""Guard exact Oracle pins and actual counter interactions."""
from copy import deepcopy
import unittest

from entry_interaction_counters_recipes import (
    Board, FAMILY, CORPS, MOA, MASTODON, TEACHER, CLASP, ENGINE, ELITE, GLADE,
    MARAUDER, GEARHULK, SKATE, GREATMAW, HAULER, profiles, spec, classify, validate_witnesses, variants,
)


class EntryInteractionCountersTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = {**profiles()['sources'], **profiles()['witnesses']}
        cls.oracle = {name: row['oracle'] for name, row in pins.items()}
        cls.rows = {name: row['registry'] for name, row in pins.items()}
        cls.names = {row['registry']['csvid']: name for name,row in profiles()['sources'].items()}

    def case(self, csvid, mode, seat):
        board = Board(self.rows[self.names[csvid]], seat, mode, self.rows, self.oracle)
        board.run()
        return board

    def counters(self, board, ref, kind):
        return [step['value'] for step in board.steps if step['op']=='assert_counter'
                and step['ref']==ref and step['counter_kind']==kind]

    def test_whole_oracle_and_variant_types(self):
        validate_witnesses(self.oracle,self.rows)
        changed = deepcopy(self.rows)
        changed['Hada Spy Patrol']['iid'] -= 1
        with self.assertRaisesRegex(ValueError,'Hada Spy Patrol'):
            validate_witnesses(self.oracle,changed)
        record = self.oracle[self.names[CORPS]]
        shape = classify(record,CORPS)
        declaration = dict(family=FAMILY,version=2,params=shape,oracle_text=record['oracle_text'],variants=variants(shape))
        self.assertEqual(spec(declaration,record),dict(shape,family=FAMILY))
        for bad in (dict(declaration,version=2.0),dict(declaration,params=dict(shape,csvid=float(CORPS))),
                    dict(declaration,variants=['split_0','split_0'])):
            with self.assertRaises(ValueError):
                spec(bad,record)

    def test_echo_uses_actual_land_payment_and_dies_scope(self):
        for seat in (0,1):
            pay = self.case(MOA,'echo_pay',seat)
            self.assertEqual([s['ref'] for s in pay.steps if s['op']=='choose_mana_source'],
                             [] if seat else ['generic_0','generic_1','green'])
            self.assertEqual(pay.cards['source']['zone'],'battlefield')
            for mode in ('echo_short','echo_decline'):
                declined = self.case(MOA,mode,seat)
                self.assertEqual(declined.cards['source']['zone'],'graveyard')
                self.assertEqual(self.counters(declined,'enemy','plus1'),[0,1])

    def test_bolster_and_true_level_thresholds(self):
        for seat in (0,1):
            self.assertEqual(self.counters(self.case(MASTODON,'tie',seat),'chosen','plus1'),[5])
            teacher = self.case(TEACHER,'threshold',seat)
            self.assertEqual([s['value'] for s in teacher.steps if s['op']=='assert_card'
                              and s['ref']=='leveler_a' and s['property']=='power'],[1,2,2,3])
            self.assertEqual(sum(s['op']=='activate' and s['ref']=='leveler_a' for s in teacher.steps),1)

    def test_proliferate_independently_tracks_permanent_and_player_types(self):
        for seat in (0,1):
            for csvid,repeats in ((CLASP,1),(ENGINE,2)):
                case = self.case(csvid,'proliferate',seat)
                self.assertEqual(self.counters(case,'charge_source','charge'),[3,3+repeats])
                self.assertEqual(self.counters(case,'other','minus1'),[1,1+repeats])
                self.assertEqual(case.energy[seat],3+repeats)
                self.assertEqual(sum(s['op']=='choose_player' and s['player']==0 and s['value']==0
                                     for s in case.steps),0 if seat else repeats)

    def test_counter_combat_and_death_riders(self):
        for seat in (0,1):
            elite = self.case(ELITE,'tap',seat)
            self.assertEqual([s['ref'] for s in elite.steps if s['op']=='choose_target'],['opposing'])
            self.assertEqual(self.case(GLADE,'death',seat).life[seat],22)
            self.assertEqual(self.case(GLADE,'foreign_death',seat).life[seat],20)
            self.assertEqual(self.case(MARAUDER,'full',seat).life[seat],24)
            self.assertEqual(self.case(GEARHULK,'trample',seat).life[1-seat],14)

    def test_two_real_counter_kinds_on_one_permanent(self):
        for seat in (0,1):
            case = self.case(SKATE,'mixed',seat)
            self.assertEqual(self.counters(case,'mixed','plus1'),[2,4])
            self.assertEqual(self.counters(case,'mixed','charge'),[1,2])
            self.assertEqual([s['player'] for s in case.steps if s['op']=='choose_target' and s['ref']=='mixed'],[0,seat])
            for mode, charge, plus in (('add_charge',2,1),('remove_charge',0,3)):
                hauler = self.case(HAULER,mode,seat)
                self.assertEqual(self.counters(hauler,'mixed','charge'),[1,charge])
                self.assertEqual(self.counters(hauler,'mixed','plus1'),[2,plus])

    def test_greatmaw_distinguishes_placement_and_removal(self):
        for seat in (0,1):
            case = self.case(GREATMAW,'clear',seat)
            self.assertEqual(self.counters(case,'source','minus1'),[2])
            self.assertEqual(self.counters(case,'own','minus1'),[1,0])
            self.assertEqual(self.counters(case,'opposing','minus1'),[0])

