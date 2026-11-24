"""Verify the complete Oracle and variant bindings for this recipe."""
import copy
import json
import unittest

from generate_cases import registry
import mandatory_attack_additional_riders_recipes as recipe


class PinnedRecipeTests(unittest.TestCase):
    def test_sources_bind_entire_printed_records(self):
        sources = recipe.profiles()['sources']
        self.assertEqual(len(sources), 4)
        for name, pin in sources.items():
            with self.subTest(card=name):
                record = pin['oracle']
                physical = False
                printed = record['selected_face'] if physical else record
                shape = recipe.classify(record, pin['back_oracle']) if physical else recipe.classify(record)
                variants = recipe.variants(shape)
                self.assertEqual(len(variants), len(set(variants)))
                self.assertGreater(len(variants), 2)
                declaration = dict(family=recipe.FAMILY, version=2, oracle_text=printed['oracle_text'],
                                   params=shape, variants=variants)
                self.assertEqual(recipe.spec(declaration, record)['family'], recipe.FAMILY)
                changed_oracle = copy.deepcopy(record)
                changed_printed = changed_oracle['selected_face'] if physical else changed_oracle
                changed_printed['oracle_text'] += '\nFlying'
                with self.assertRaises(ValueError):
                    recipe.spec(declaration, changed_oracle)
                declaration['variants'] = variants + [variants[0]]
                with self.assertRaises(ValueError):
                    recipe.spec(declaration, record)

    def test_madness_acknowledges_ai_discard_before_deliberate_choice(self):
        name = 'Insatiable Gorgers'
        pin = recipe.profiles()['sources'][name]
        record = pin['oracle']
        params = recipe.classify(record)
        variants = recipe.variants(params)
        declaration = dict(family=recipe.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=params, variants=variants)
        cards = json.loads((recipe.PROFILE.parent / 'oracle.json').read_bytes())['cards']
        rows = {row['name']: row for row in registry()}
        for mode, decision in (('madness_pay', 0), ('madness_decline', 1),
                               ('madness_short', 0), ('madness_replay', 0)):
            for owner in (0, 1):
                with self.subTest(mode=mode, seat=owner):
                    case, = recipe.expand(name, pin['registry']['csvid'], declaration, params,
                                          [f'{mode}_{owner}'], rows, cards)
                    steps = case['steps']
                    for index, step in enumerate(steps):
                        if step != dict(op='activate', ref='discarder', value=1):
                            continue
                        offered = [dict(op='choose_dialog', player=owner, value=decision)]
                        if owner == 1:
                            offered.insert(0, dict(op='choose_dialog', player=owner, value=0))
                        self.assertEqual(steps[index - len(offered):index], offered)

    def test_own_bestowed_host_has_production_end_turn_setup_before_paid_cast(self):
        name = "Mogis's Warhound"
        pin = recipe.profiles()['sources'][name]
        record = pin['oracle']
        params = recipe.classify(record)
        variants = recipe.variants(params)
        declaration = dict(family=recipe.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=params, variants=variants)
        cards = json.loads((recipe.PROFILE.parent / 'oracle.json').read_bytes())['cards']
        rows = {row['name']: row for row in registry()}
        for owner in (0, 1):
            case, = recipe.expand(name, pin['registry']['csvid'], declaration, params,
                                  [f'bestow_own_{owner}'], rows, cards)
            steps = case['steps']
            at = steps.index(dict(op='cleanup'))
            paid_cast = steps.index(dict(op='cast', ref='source', value=1))
            self.assertLess(at, paid_cast)
            self.assertEqual(steps[at + 1], dict(op='turn', player=owner, value=0))
            self.assertEqual({(s['player'], s['color'], s['value']) for s in steps[:at] if s['op'] == 'set_mana'},
                             {(p, c, 0) for p in (0, 1) for c in range(6)})
            declaration_step = steps.index(dict(op='begin_required_attackers'), paid_cast)
            self.assertIn(dict(op='can_finish_attackers', value=0), steps[declaration_step:])


if __name__ == '__main__':
    unittest.main()
