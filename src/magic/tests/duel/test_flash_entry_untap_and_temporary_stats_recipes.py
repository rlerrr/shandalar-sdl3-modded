"""Verify exact source pins and the mass-untap recipient contrasts."""
import json
import unittest

from generate_cases import registry
from recipes import COLORS
import flash_entry_untap_and_temporary_stats_recipes as model


class FlashEntryUntapStatsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry() if row['kind'] != 'internal'}

    def steps(self, name, mode):
        csvid = self.rows[name]['csvid']
        oracle = self.oracle[name]
        shape = model.classify(oracle, csvid)
        declaration = dict(family=model.FAMILY, version=2, oracle_text=oracle['oracle_text'],
                           params=shape, variants=model.variants(shape))
        return model.expand(name, csvid, declaration, model.spec(declaration, oracle), [mode],
                            self.rows, self.oracle)[0]['steps']

    def test_whole_oracle_and_witness_pins(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, entry in model.profiles()['sources'].items():
            oracle = entry['oracle']
            with self.assertRaises(ValueError):
                model.classify(dict(oracle, oracle_text='Flash'), entry['registry']['csvid'])
        rows = dict(self.rows)
        rows[model.MYSTIC] = dict(rows[model.MYSTIC], handler='card_dummy')
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, rows)

    def test_bell_ringer_untaps_own_creatures_and_allows_real_mana(self):
        for owner in (0, 1):
            steps = self.steps('Village Bell-Ringer', f'reuse_mana_{owner}')
            self.assertFalse(any(item['op'].startswith('choose_') for item in steps[:steps.index(
                dict(op='activate', ref='host', value=1))]))
            for ref, tapped in (('host', 0), ('own_second', 0), ('rival', 1), ('own_land', 1)):
                self.assertIn(dict(op='assert_card', ref=ref, property='tapped', value=tapped), steps)
            self.assertIn(dict(op='assert_mana', player=owner, color=COLORS['G'], value=1), steps)
            self.assertIn(dict(op='cast', ref='growth', value=1), steps)

    def test_oak_guardian_foe_matching_set_and_real_reach_block(self):
        for owner in (0, 1):
            foe_steps = self.steps('Great Oak Guardian', f'foe_mass_{owner}')
            self.assertIn(dict(op='choose_player', player=owner, value=1-owner), foe_steps)
            self.assertIn(dict(op='assert_card', ref='rival', property='power', value=3), foe_steps)
            self.assertIn(dict(op='assert_card', ref='rival', property='power', value=1), foe_steps)
            self.assertNotIn(dict(op='assert_card', ref='host', property='power', value=4), foe_steps)
            block_steps = self.steps('Great Oak Guardian', f'reach_block_{owner}')
            self.assertIn(dict(op='choose_player', player=owner, value=owner), block_steps)
            self.assertIn(dict(op='block', ref='source', other='attacker', value=1), block_steps)
            self.assertIn(dict(op='assert_card', ref='source', property='damage', value=2), block_steps)


if __name__ == '__main__':
    unittest.main()
