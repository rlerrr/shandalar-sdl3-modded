"""Guard paid entry, energy budgets and independent rider observations."""
from copy import deepcopy
import unittest

from entry_rider_energy_and_mana_recipes import (
    Board, FAMILY, SAGE, PUZZLE, SERVANT, DRAKE, TREACHERY, PALINCHRON, RENEGADE, DEADLOCK,
    PUMMELER, HERMIT, SENTRY, HELLION, WONDER, ARTIST, WHIRLER, THOPTER, profiles, spec, classify,
    validate_witnesses, variants,
)


class EntryEnergyManaTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = {**profiles()['sources'], **profiles()['witnesses']}
        cls.oracle = {name: row['oracle'] for name, row in pins.items()}
        cls.rows = {name: row['registry'] for name, row in pins.items()}
        cls.names = {row['registry']['csvid']: name for name, row in profiles()['sources'].items()}

    def case(self, csvid, mode, seat):
        board = Board(self.rows[self.names[csvid]], seat, mode, self.rows, self.oracle)
        board.run()
        return board

    def test_typed_exact_primary_and_token_witness(self):
        validate_witnesses(self.oracle, self.rows)
        changed = deepcopy(self.rows)
        changed[THOPTER]['iid'] -= 1
        with self.assertRaisesRegex(ValueError, THOPTER):
            validate_witnesses(self.oracle, changed)
        record = self.oracle[self.names[SENTRY]]
        shape = classify(record, SENTRY)
        decl = dict(family=FAMILY, version=2, params=shape, variants=variants(shape), oracle_text=record['oracle_text'])
        self.assertEqual(spec(decl,record),dict(shape,family=FAMILY))
        for bad in (dict(decl,version=2.0), dict(decl,params=dict(shape,csvid=float(SENTRY))),
                    dict(decl,variants=['tap_0','tap_0'])):
            with self.assertRaises(ValueError):
                spec(bad,record)
        with self.assertRaises(ValueError):
            spec(decl,dict(record,oracle_text=record['oracle_text']+' '))

    def test_input_repairs_use_only_real_colored_pool_payment(self):
        for seat in (0,1):
            for mode in ('capped','partial','opposing'):
                drake = self.case(DRAKE,mode,seat)
                self.assertEqual([s['color'] for s in drake.steps if s['op']=='choose_mana'], [] if seat else [4]*4)
            servant = self.case(SERVANT,'deplete',seat)
            self.assertEqual([s['color'] for s in servant.steps if s['op']=='choose_mana'], [] if seat else [3,3])
            sage = self.case(SAGE,'recast',seat)
            self.assertEqual(sage.energy[seat],6)
            artifact = self.case(PUZZLE,'snapshot',seat)
            self.assertEqual((artifact.energy[seat],artifact.life[seat]),(6,26))

    def test_untappers_keep_real_fees_and_control_lifecycle(self):
        for seat in (0,1):
            case = self.case(PALINCHRON,'bounce',seat)
            self.assertEqual(sum(s['op']=='cast' and s.get('ref')=='source' for s in case.steps),2)
            aura = self.case(TREACHERY,'release',seat)
            self.assertEqual([s['player'] for s in aura.steps if s['op']=='bind_permanent_copy'],[seat,1-seat])
            self.assertEqual(aura.cards['source']['zone'],'graveyard')

    def test_energy_pays_distinct_damage_cost_and_target_rules(self):
        for seat in (0,1):
            ping = self.case(RENEGADE,'ping',seat)
            self.assertEqual((ping.cards['victim']['zone'],ping.energy[seat]),('graveyard',2))
            burn = self.case(RENEGADE,'burn',seat)
            self.assertEqual((burn.life[1-seat],burn.energy[seat]),(14,0))
            walker = self.case(RENEGADE,'walker',seat)
            self.assertEqual(walker.cards['walker']['zone'],'graveyard')
            lock = self.case(DEADLOCK,'lock',seat)
            self.assertEqual([s['value'] for s in lock.steps if s['op']=='can_activate' and s['ref']=='victim'],[1,0,1])

    def test_pummeler_reads_dynamic_power_and_defender_expiry(self):
        for seat in (0,1):
            pump = self.case(PUMMELER,'scaled',seat)
            self.assertEqual([s['value'] for s in pump.steps if s['op']=='assert_card' and s['property']=='power'],[1,4,8,1])
            for mode, attack in (('pay',1),('defender',0),('expiry',0)):
                case = self.case(HERMIT,mode,seat)
                self.assertEqual([s['value'] for s in case.steps if s['op']=='attack'],[attack])

    def test_real_sentry_target_payment_and_hellion_end_steps(self):
        for seat in (0,1):
            case = self.case(SENTRY,'untap',seat)
            self.assertEqual([s['value'] for s in case.steps if s['op']=='assert_card' and s['ref']=='victim'
                              and s['property']=='tapped'],[1,0])
            for mode, energy in (('pay_then_empty',0),('decline',2)):
                case = self.case(HELLION,mode,seat)
                self.assertEqual((case.cards['source']['zone'],case.energy[seat]),('graveyard',energy))
                self.assertEqual(case.life[1-seat],16)

    def test_wonder_separate_modes_and_haste_target_scope(self):
        for seat in (0,1):
            for mode, values in (('vigilance',[0,1]),('lifelink',[0,2]),('power',[1,0]),('toughness',[1,1])):
                case = self.case(WONDER,mode,seat)
                self.assertEqual([s['value'] for s in case.steps if s['op']=='choose_dialog'],values)
            ally = self.case(ARTIST,'ally',seat)
            foe = self.case(ARTIST,'foe',seat)
            self.assertEqual((ally.life[1-seat],foe.life[seat]),(18,18))

    def test_real_thopter_activation_observes_every_trait(self):
        for seat in (0,1):
            case = self.case(WHIRLER,'repeat',seat)
            self.assertEqual(case.energy[seat],0)
            self.assertEqual([s['index'] for s in case.steps if s['op']=='bind_permanent_copy'],[0,1])
            self.assertEqual({s['property'] for s in case.steps if s['op']=='assert_matching_permanents'},
                             {'artifact','color','power','toughness','subtype_Thopter','token'})
