"""Verify the complete Oracle and variant bindings for this recipe."""
import copy
import unittest

import werewolf_face_ability_riders_recipes as recipe


class PinnedRecipeTests(unittest.TestCase):
    def test_sources_bind_entire_printed_records(self):
        sources = recipe.profiles()['sources']
        self.assertEqual(len(sources), 7)
        for name, pin in sources.items():
            with self.subTest(card=name):
                record = pin['oracle']
                physical = True
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


if __name__ == '__main__':
    unittest.main()
