"""Guard whole Oracle, distinct prompts and combat scope."""

from copy import deepcopy
import unittest

import attack_trigger_block_and_target_recipes as model


class AttackTriggerGuards(unittest.TestCase):
    def fixtures(self):
        records = {}
        for group in model.profiles().values():
            records.update(group)
        return {name: pins['oracle'] for name, pins in records.items()}, {name: pins['registry'] for name, pins in records.items()}

    def case(self, name, mode, actor):
        oracle, rows = self.fixtures()
        csvid = rows[name]['csvid']
        factory = model.FollowOn if csvid in model.FOLLOWERS else model.Board
        board = factory(oracle[name], csvid, mode, actor, rows, oracle)
        board.play()
        return board

    def test_exact_whole_record_and_variant_grammar(self):
        oracle, rows = self.fixtures()
        model.validate_witnesses(oracle, rows)
        record = oracle['Goblin Heelcutter']
        params = model.classify(record, model.HEEL)
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=params, variants=model.variants(params))
        self.assertEqual(model.spec(declaration, record)['csvid'], model.HEEL)
        bad = deepcopy(record)
        bad['oracle_text'] = bad['oracle_text'].splitlines()[0]
        with self.assertRaises(ValueError):
            model.spec(declaration, bad)
        with self.assertRaises(ValueError):
            model.spec(declaration | {'variants':['dash_0','dash_0']}, record)

    def test_human_mandatory_and_optional_trigger_inputs_distinct(self):
        tusker = self.case('Avalanche Tusker', 'named_only', 0)
        self.assertNotIn('choose_trigger_source', [x['op'] for x in tusker.steps])
        self.assertIn('choose_target', [x['op'] for x in tusker.steps])
        nefashu = self.case('Nefashu', 'zero', 0)
        self.assertIn('choose_trigger_source', [x['op'] for x in nefashu.steps])
        self.assertIn('choose_cancel_target', [x['op'] for x in nefashu.steps])

    def test_cost_modes_target_the_live_guard_and_keep_dash_return(self):
        dash = self.case('Goblin Heelcutter', 'dash', 1)
        normal = self.case('Goblin Heelcutter', 'normal', 1)
        self.assertEqual(dash.cards['source']['zone'], 'hand')
        self.assertEqual(normal.cards['source']['zone'], 'battlefield')
        for board, count in ((dash, 2), (normal, 3)):
            self.assertIn(count, [x['value'] for x in board.steps if x['op'] == 'set_mana' and x['color'] == 0])
            self.assertEqual([x['ref'] for x in board.steps if x['op'] == 'choose_target'], ['guard'])

    def test_ferret_skip_checks_only_paid_block(self):
        normal = self.case("Joven's Ferrets", 'blocker', 0)
        lethal = self.case("Joven's Ferrets", 'lethal', 0)
        self.assertEqual(sum(x['op'] == 'untap_step' for x in normal.steps), 2)
        self.assertNotIn('untap_step', [x['op'] for x in lethal.steps])
        self.assertEqual(lethal.cards['source']['zone'], 'graveyard')


if __name__ == '__main__':
    unittest.main()
