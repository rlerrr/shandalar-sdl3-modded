"""Regressions for discovery, identity, and independent case authoring."""
import json
import tempfile
import unittest
from pathlib import Path

from case_files import load_cases,CaseStream,select_case_files


class CaseFilesTest(unittest.TestCase):
    def setUp(self):
        # CTest runs in its build directory; direct runs also stay inside the workspace.
        self.temp = tempfile.TemporaryDirectory(dir=Path.cwd())
        self.assertEqual(Path(self.temp.name).resolve().parent, Path.cwd().resolve())
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name) / "cases"
        self.registry = {"Infantry Veteran": {"csvid": 2723}, "Furnace Whelp": {"csvid": 7059}}

    def write(self, relative, document):
        path = self.directory / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(document), encoding="utf-8")

    def card_file(self, name="Infantry Veteran", csvid=2723, case="Veteran_Taps"):
        return {"version": 1, "card": name, "csvid": csvid,
                "cases": [{"name": case, "fixture": "combat", "steps": []}]}

    def test_card_identity_is_inherited_and_source_is_relative(self):
        self.write("cards/bucket/veteran.json", self.card_file())
        case, = load_cases(self.directory, self.registry)
        self.assertEqual(case["card"], "Infantry Veteran")
        self.assertEqual(case["_source_file"], "cases/cards/bucket/veteran.json")

    def test_nested_discovery_and_order_do_not_depend_on_card_homes(self):
        self.write("cards/z/veteran.json", self.card_file(case="A_Veteran"))
        self.write("cards/a/whelp.json", self.card_file("Furnace Whelp", 7059, "Z_Whelp"))
        original = load_cases(self.directory, self.registry)
        moved = self.directory / "cards/another-set/veteran.json"
        moved.parent.mkdir(parents=True)
        (self.directory / "cards/z/veteran.json").rename(moved)
        self.assertEqual([c["name"] for c in original], [c["name"] for c in load_cases(self.directory, self.registry)])

    def test_scenarios_can_reference_multiple_cards(self):
        self.write("scenarios/snapshots.json", {"version": 1, "cases": [
            {"name": "Snapshot_Veteran", "card": "Infantry Veteran"},
            {"name": "Snapshot_Whelp", "card": "Furnace Whelp"}]})
        self.assertEqual(len(load_cases(self.directory, self.registry)), 2)

    def test_duplicate_case_reports_both_sources(self):
        self.write("cards/veteran.json", self.card_file())
        self.write("scenarios/duplicate.json", {"version": 1, "cases": [
            {"name": "Veteran_Taps", "card": "Infantry Veteran"}]})
        with self.assertRaisesRegex(ValueError, r"duplicate case.*cards/veteran.json"):
            load_cases(self.directory, self.registry)

    def test_card_has_one_home_even_in_different_sets(self):
        self.write("cards/set-a/veteran.json", self.card_file(case="Veteran_One"))
        self.write("cards/set-b/veteran.json", self.card_file(case="Veteran_Two"))
        with self.assertRaisesRegex(ValueError, "duplicate card home"):
            load_cases(self.directory, self.registry)

    def test_mismatched_id_or_case_card_is_rejected(self):
        for document, message in [(self.card_file(csvid=7059), "name and CSVID"),
                                  (dict(self.card_file(), cases=[{"name": "Wrong", "card": "Furnace Whelp"}]), "conflicts")]:
            with self.subTest(message=message):
                self.write("cards/veteran.json", document)
                with self.assertRaisesRegex(ValueError, message):
                    load_cases(self.directory, self.registry)

    def test_missing_cases_and_unsupported_versions_are_not_silent(self):
        with self.assertRaisesRegex(ValueError, "No behavior case files"):
            load_cases(self.directory, self.registry)
        for document, message in [({"version": 2, "cases": [{}]}, "schema"),
                                  ({"version": 1, "cases": []}, "nonempty")]:
            with self.subTest(message=message):
                self.write("broken.json", document)
                with self.assertRaisesRegex(ValueError, message):
                    load_cases(self.directory, self.registry)

    def test_alternate_records_require_authored_rules_scenarios(self):
        self.registry["Infantry Veteran"]["kind"] = "alternate"
        doc = self.card_file()
        self.write("cards/alternate.json", doc)
        self.assertEqual(len(load_cases(self.directory, self.registry)), 1)
        self.write("cards/alternate.json", dict(doc, recipes=[{"family": "creature_keywords"}]))
        with self.assertRaisesRegex(ValueError, "require a primary card"):
            load_cases(self.directory, self.registry, {"Infantry Veteran": {}})

    def test_reviewed_registry_classification_applied(self):
        from generate_cases import registry
        rows = {r["csvid"]: r for r in registry()}
        self.assertEqual(rows[910]["kind"], "alternate")
        self.assertIn("Mishra's Factory", rows[910]["kind_reason"])
        self.assertEqual(rows[2723]["kind"], "card")

    def test_registry_classification_rejects_stale_or_unreviewed_entries(self):
        from unittest.mock import patch
        import generate_cases
        import import_duel_oracle
        base = {"name": "Assembly-Worker", "kind": "alternate", "reason": "Reviewed identity"}
        for doc in ([None], {"version": 2, "overrides": {}}, {"version": 1, "overrides": []},
                    {"version": 1, "overrides": {"999999": base}},
                    *({"version": 1, "overrides": {"910": dict(base, **change)}}
                      for change in ({"name": "Wrong"}, {"kind": "card"}, {"reason": ""}))):
            with self.subTest(doc=doc), patch.object(import_duel_oracle, "DATA", Path(self.temp.name)):
                (Path(self.temp.name) / "registry_kinds.json").write_text(json.dumps(doc), encoding="utf8")
                with self.assertRaises(ValueError):
                    generate_cases.registry()

    def test_stream_matches_eager_order_and_complete_case_data_with_one_file_cache(self):
        a=self.card_file(case='Alpha');a['cases'].append(dict(name='Zulu',fixture='combat',steps=[dict(op='assert_life',player=0,value=20)]))
        b=self.card_file('Furnace Whelp',7059,case='Beta');b['cases'].append(dict(name='Omega',fixture='combat',steps=[]))
        self.write('cards/a.json',a);self.write('cards/b.json',b)
        stream=CaseStream(self.directory,self.registry,cache_size=1)
        expected=load_cases(self.directory,self.registry)
        self.assertEqual(len(stream),4);self.assertEqual(stream.recipe_count,0)
        self.assertEqual(list(stream),expected);self.assertEqual(list(stream),expected)

    def test_stream_rejects_duplicate_names_and_card_homes_across_files(self):
        self.write('cards/a.json',self.card_file())
        self.write('cards/b.json',self.card_file('Furnace Whelp',7059))
        with self.assertRaisesRegex(ValueError,'duplicate case'):CaseStream(self.directory,self.registry)
        self.write('cards/b.json',self.card_file(case='AnotherCase'))
        with self.assertRaisesRegex(ValueError,'duplicate card home'):CaseStream(self.directory,self.registry)

    def test_stream_preserves_generated_provenance_and_counts_on_reload(self):
        from unittest.mock import patch
        def expand(name,csvid,d,record,registry,oracle_context):
            return [dict(name=d['family'],card=name,fixture='combat',steps=[],_recipe=dict(family=d['family']))]
        self.write('cards/a.json',dict(version=1,card='Infantry Veteran',csvid=2723,recipes=[dict(family='Alpha'),dict(family='Zulu')]))
        self.write('cards/b.json',dict(version=1,card='Furnace Whelp',csvid=7059,recipes=[dict(family='Beta'),dict(family='Omega')]))
        with patch('recipes.expand',side_effect=expand):
            stream=CaseStream(self.directory,self.registry,{},cache_size=1)
            self.assertEqual(stream.recipe_count,4)
            self.assertEqual(list(stream),load_cases(self.directory,self.registry,{}))

    def test_stream_checks_empty_inventory_cache_size_and_changed_names(self):
        with self.assertRaisesRegex(ValueError,'No behavior'):CaseStream(self.directory,self.registry)
        for size in (0,-1,True):
            with self.assertRaises(ValueError):CaseStream(self.directory,self.registry,cache_size=size)
        self.write('cards/a.json',self.card_file())
        stream=CaseStream(self.directory,self.registry)
        self.write('cards/a.json',self.card_file(case='Changed'))
        with self.assertRaisesRegex(ValueError,'inventory changed'):list(stream)

    def test_file_subset_expands_only_selected_recipes_and_retains_whole_home(self):
        from unittest.mock import patch
        self.write('cards/a.json',dict(version=1,card='Infantry Veteran',csvid=2723,
                                      recipes=[dict(family='Alpha'),dict(family='Zulu')]))
        # An unsupported recipe outside the subset must never be expanded.
        self.write('cards/b.json',dict(version=1,card='Furnace Whelp',csvid=7059,
                                      recipes=[dict(family='Unsupported')]))
        def expand(name,csvid,d,record,registry,oracle_context):
            self.assertNotEqual(d['family'],'Unsupported')
            return [dict(name=d['family'],card=name,fixture='combat',steps=[],_recipe=d)]
        files=select_case_files(self.directory,case_files=['cases/cards/a.json'])
        with patch('recipes.expand',side_effect=expand):
            stream=CaseStream(self.directory.resolve(),self.registry,{},files=files)
            self.assertEqual([c['name'] for c in stream],['Alpha','Zulu'])
            self.assertEqual(stream.recipe_count,2)
            self.assertEqual({c['_source_file'] for c in stream},{'cases/cards/a.json'})

    def test_file_and_csvid_selectors_form_deduplicated_union(self):
        self.write('cards/a.json',self.card_file(case='Alpha'))
        self.write('cards/b.json',self.card_file('Furnace Whelp',7059,'Beta'))
        self.write('scenarios/mentions.json',dict(version=1,cases=[dict(name='Mention',card='Infantry Veteran')]))
        one=select_case_files(self.directory,case_files=['cards/a.json','cases/cards/a.json'],csvids=[2723])
        self.assertEqual(one,[(self.directory/'cards/a.json').resolve()])
        union=select_case_files(self.directory,case_files=['cards/a.json'],csvids=[7059])
        self.assertEqual([p.name for p in union],['a.json','b.json'])
        self.assertEqual(select_case_files(self.directory,csvids=[2723]),one)

    def test_subset_selector_typos_empty_selection_and_path_escape_fail(self):
        self.write('cards/a.json',self.card_file())
        for selectors in ({},dict(case_files=[]),dict(csvids=[True]),dict(csvids=[-1]),
                          dict(csvids=[2723,99999]),dict(case_files=['cards/missing.json']),
                          dict(case_files=['../cases/cards/a.json']),
                          dict(case_files=[str((self.directory/'cards/a.json').resolve())])):
            with self.subTest(selectors=selectors),self.assertRaises(ValueError):
                select_case_files(self.directory,**selectors)


class UntapStepContractTests(unittest.TestCase):
    def test_explicit_players_are_supported(self):
        from generate_cases import validate_untap_step
        for player in (0, 1):
            validate_untap_step(dict(op='untap_step', player=player))

    def test_implicit_boolean_and_foreign_players_are_rejected(self):
        from generate_cases import validate_untap_step
        for player in (None, True, False, -1, 2, '0', 0.0):
            with self.subTest(player=player), self.assertRaises(ValueError):
                validate_untap_step(dict(op='untap_step', player=player))
        with self.assertRaises(ValueError):
            validate_untap_step(dict(op='untap_step'))

    def test_other_action_fields_cannot_silently_change_the_contract(self):
        from generate_cases import validate_untap_step
        for field in ('value', 'ref', 'color', 'index'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                validate_untap_step(dict(op='untap_step', player=0, **{field: 1}))

class ShuffleObservationContractTests(unittest.TestCase):
    def test_explicit_zero_and_positive_counts(self):
        from generate_cases import validate_shuffle_observation
        for player in (0,1):
            for value in (0,1,2147483647):
                validate_shuffle_observation(dict(op='assert_shuffle_count',player=player,value=value))

    def test_invalid_players_counts_and_unknown_fields_fail_closed(self):
        from generate_cases import validate_shuffle_observation
        for field,bad in (('player',True),('player',2),('player',-1),('player','0'),
                          ('value',False),('value',-1),('value',2147483648),('value',1.0),('value','1'),('ref','source')):
            step=dict(op='assert_shuffle_count',player=0,value=0);step[field]=bad
            with self.subTest(field=field,bad=bad),self.assertRaises(ValueError):validate_shuffle_observation(step)
        for missing in ('player','value'):
            step=dict(op='assert_shuffle_count',player=0,value=0);del step[missing]
            with self.assertRaises(ValueError):validate_shuffle_observation(step)

    def test_wolf_property_is_scoped_to_matching_permanent_observation(self):
        from generate_cases import TOKEN_PROPERTIES,PROPERTIES
        self.assertIn('subtype_Wolf',TOKEN_PROPERTIES)
        self.assertNotIn('subtype_Wolf',PROPERTIES)

    def test_replacement_token_subtypes_are_scoped_to_matching_observations(self):
        from generate_cases import TOKEN_PROPERTIES,PROPERTIES
        for name in ('Ape','Frog','Lizard'):
            with self.subTest(name=name):
                self.assertIn('subtype_'+name,TOKEN_PROPERTIES)
                self.assertNotIn('subtype_'+name,PROPERTIES)

if __name__ == "__main__":
    unittest.main()
