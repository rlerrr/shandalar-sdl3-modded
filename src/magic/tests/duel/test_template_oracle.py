"""Name-only token mappings must not fabricate Oracle stat expectations."""
import unittest
from generate_cases import template_stats


class TemplateOracleTest(unittest.TestCase):
    def test_real_vanilla_and_flying_creatures_keep_oracle_stats(self):
        face=dict(type_line='Creature — Ogre Warrior',oracle_text='',power='4',toughness='3',layout='normal')
        self.assertEqual(template_stats(face),(4,3))
        self.assertEqual(template_stats(dict(face,oracle_text='Flying')),(4,3))

    def test_ambiguous_token_name_does_not_select_one_of_several_stat_lines(self):
        for p in ('2','4'):
            self.assertIsNone(template_stats(dict(type_line='Token Creature — Bear',oracle_text='',power=p,toughness=p)))

    def test_variable_stats_and_nontrivial_text_require_authored_behavior(self):
        face=dict(type_line='Creature',oracle_text='',power='*',toughness='*')
        self.assertIsNone(template_stats(face))
        self.assertIsNone(template_stats(dict(face,power='2',toughness='2',oracle_text='Menace')))


if __name__=='__main__':
    unittest.main()
