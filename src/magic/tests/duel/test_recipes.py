"""Guard recipe boundaries, Oracle independence and existing test preservation."""
import copy
import json
import tempfile
import subprocess
import sys
import os
import unittest
from unittest.mock import patch
from types import SimpleNamespace
from pathlib import Path

from case_files import load_cases
from recipes import classify, expand, validate_context
from generate_cases import registry
from recipe_tools import enroll, review
from run_cases import run_case

DATA = Path(__file__).parent / "data"


class RecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((DATA / "oracle.json").read_text(encoding="utf-8"))["cards"]
        cls.registry = {r["name"]: r for r in registry() if r["kind"] != "internal"}
        cls.fixtures = json.loads((DATA / "fixtures.json").read_text())["fixtures"]

    def test_reviewed_witnesses_and_shared_boards_still_fit(self):
        validate_context(self.oracle, self.registry, self.fixtures)
        altered = copy.deepcopy(self.fixtures)
        altered["stocked_zones"]["cards"][0]["name"] = "Forest"
        with self.assertRaisesRegex(ValueError, "assumptions changed"):
            validate_context(self.oracle, self.registry, altered)

    def test_whole_text_match_rejects_additional_effects_and_restrictions(self):
        for name in ("Divination", "Giant Growth", "Lightning Bolt", "Sacred Nectar"):
            record = self.oracle[name]
            self.assertIsNotNone(classify(name, record)[0])
            for text in (record["oracle_text"] + "\nDraw a card.",
                         "If you control an Island, " + record["oracle_text"],
                         record["oracle_text"].replace("Target creature", "Target green creature") if name == "Giant Growth" else ""):
                altered = dict(record, oracle_text=text)
                self.assertIsNone(classify(name, altered)[0], text)

    def test_unreviewed_costs_layouts_and_amounts_do_not_get_approximate_tests(self):
        bolt = self.oracle["Lightning Bolt"]
        for changes in ({"mana_cost": "{X}{R}"}, {"mana_cost": "{R/P}"}, {"mana_cost": "{0}"}, {"layout": "transform"},
                        {"oracle_text": "Lightning Bolt deals 10 damage to any target."}, {"keywords": ["Rebound"]}):
            self.assertIsNone(classify("Lightning Bolt", dict(bolt, **changes))[0])

    def test_expectations_change_with_oracle_not_registry_handler(self):
        record = dict(self.oracle["Lightning Bolt"], oracle_text="Lightning Bolt deals 2 damage to any target.")
        declaration = dict(family="damage_any_target", version=1, variants=["opponent"])
        first = expand("Lightning Bolt", 145, declaration, record, self.registry)
        modified = copy.deepcopy(self.registry)
        modified["Lightning Bolt"]["handler"] = "unrelated_handler"
        self.assertEqual(first, expand("Lightning Bolt", 145, declaration, record, modified))
        self.assertIn(dict(op="assert_life", player=1, value=18), first[0]["steps"])

    def test_recipe_validation_is_strict(self):
        for declaration in ({"family": "damage_any_target"},
                            {"family": "gain_life", "version": 1},
                            {"family": "damage_any_target", "version": 1, "amount": 99},
                            {"family": "damage_any_target", "version": 1, "variants": ["typo"]},
                            {"family": "damage_any_target", "version": 1, "variants": ["self", "self"]}):
            with self.assertRaises(ValueError):
                expand("Lightning Bolt", 145, declaration, self.oracle["Lightning Bolt"], self.registry)

    def test_draw_expansion_is_independent_of_python_hash_seed(self):
        script = """import json
from test_recipes import RecipesTest
from recipes import expand
RecipesTest.setUpClass()
print(json.dumps(expand('Concentrate',999,dict(family='draw_cards',version=1),RecipesTest.oracle['Concentrate'],RecipesTest.registry)))
"""
        outputs = [subprocess.check_output([sys.executable, "-c", script],
                    cwd=Path(__file__).parent, env=dict(os.environ, PYTHONHASHSEED=str(seed))) for seed in (1, 42)]
        self.assertEqual(*outputs)

    def test_explicit_components_support_additional_effects_without_parsing_prose(self):
        for name, family, params, variant, expected in (
                ("Adamant Will", "pump_creature", dict(power=2, toughness=2), "friendly_cleanup",
                 dict(op="assert_card", ref="recipient", property="power", value=4)),
                ("Incinerate", "damage_any_target", dict(amount=3), "opponent",
                 dict(op="assert_life", player=1, value=17))):
            record = self.oracle[name]
            self.assertIsNone(classify(name, record)[0])
            declaration = dict(family=family, version=2, params=params,
                               oracle_text=record["oracle_text"], variants=[variant])
            generated = expand(name, self.registry[name]["csvid"], declaration, record, self.registry)
            self.assertIn(expected, generated[0]["steps"])
            self.assertEqual(generated[0]["_recipe"]["params"], params)
            self.assertEqual(generated[0]["_recipe"]["version"], 2)
            # Prose changes require deliberate review, but no new parser grammar.
            updated = dict(record, oracle_text=record["oracle_text"].replace(name, "This spell"))
            if updated["oracle_text"] == record["oracle_text"]:
                updated["oracle_text"] += "\nNew reminder wording."
            with self.assertRaisesRegex(ValueError, "Oracle text changed"):
                expand(name, 999, declaration, updated, self.registry)
            declaration["oracle_text"] = updated["oracle_text"]
            regenerated = expand(name, self.registry[name]["csvid"], declaration, updated, self.registry)
            self.assertEqual(generated[0]["steps"], regenerated[0]["steps"])

    def test_explicit_parameters_are_reviewed_not_inferred_and_reject_invalid_inputs(self):
        record = self.oracle["Incinerate"]
        declaration = dict(family="damage_any_target", version=2, params=dict(amount=2),
                           oracle_text=record["oracle_text"], variants=["opponent"])
        generated = expand("Incinerate", 999, declaration, record, self.registry)
        self.assertIn(dict(op="assert_life", player=1, value=18), generated[0]["steps"])
        for changes in (dict(params={}), dict(params=dict(amount=True)), dict(params=dict(amount=5)),
                        dict(params=dict(amount=3, extra=1)), dict(oracle_text=""), dict(family="typo"),
                        dict(variants=[]), dict(variants=["typo"]), dict(version=True)):
            with self.assertRaises(ValueError):
                expand("Incinerate", 999, dict(declaration, **changes), record, self.registry)
        missing = dict(declaration)
        del missing["variants"]
        with self.assertRaisesRegex(ValueError, "requires reviewed variants"):
            expand("Incinerate", 999, missing, record, self.registry)
        for changes in (dict(mana_cost="{X}{R}"), dict(layout="transform"), dict(type_line="Creature")):
            with self.assertRaises(ValueError):
                expand("Incinerate", 999, declaration, dict(record, **changes), self.registry)

    def test_loader_preserves_authored_cases_and_adds_stable_recipe_names(self):
        with tempfile.TemporaryDirectory(dir=Path.cwd()) as temporary:
            cases = Path(temporary) / "cases"
            cases.mkdir()
            path = cases / "bolt.json"
            original = dict(name="ExistingBolt", fixture="empty_main", steps=[])
            document = dict(version=1, card="Lightning Bolt", csvid=145, cases=[original],
                            recipes=[dict(family="damage_any_target", version=1, variants=["self"])])
            path.write_text(json.dumps(document))
            loaded = load_cases(cases, self.registry, self.oracle)
            self.assertEqual([c["name"] for c in loaded], ["ExistingBolt", "Recipe_145_damage_any_target_self"])
            self.assertEqual(loaded[0]["steps"], original["steps"])
            with self.assertRaisesRegex(ValueError, "pinned Oracle context"):
                load_cases(cases, self.registry)
            document["recipes"] *= 2
            path.write_text(json.dumps(document))
            with self.assertRaisesRegex(ValueError, "duplicate case"):
                load_cases(cases, self.registry, self.oracle)

    def test_coupled_effects_include_self_targets_fizzling_and_populated_libraries(self):
        def cases(name, family, params, variants):
            return expand(name, self.registry[name]["csvid"],
                dict(family=family, version=2, params=params, variants=variants,
                     oracle_text=self.oracle[name]["oracle_text"]), self.oracle[name], self.registry)
        helix = cases("Lightning Helix", "damage_any_target", dict(amount=3, gain_life=3),
                      ["self", "opponent", "lost_target", "insufficient_mana"])
        for c, life in zip(helix, (20, 23, 20, 20)):
            self.assertIn(dict(op="assert_life", player=0, value=life), c["steps"])
        blast = cases("Psionic Blast", "damage_any_target", dict(amount=4, self_damage=2), ["self"])
        self.assertIn(dict(op="assert_life", player=0, value=14), blast[0]["steps"])
        cannonade = cases("Orcish Cannonade", "damage_any_target", dict(amount=2, self_damage=3, draw=1),
                          ["survivor", "lost_target", "insufficient_mana", "snapshot"])
        for c, drawn in zip(cannonade, (1, 0, 0, 1)):
            self.assertIn(dict(op="assert_zone", player=0, zone="library", value=7-drawn), c["steps"])
            if c["fixture"] == "empty_main":
                self.assertEqual(len([x for x in c["setup"] if x["zone"]=="library"]),7)
        wildsize = cases("Wildsize", "pump_creature", dict(power=2, toughness=2, draw=1), ["friendly_cleanup"])
        self.assertIn(dict(op="assert_zone", player=0, zone="hand", value=1), wildsize[0]["steps"])
        fizzled = cases("Wildsize", "pump_creature", dict(power=2, toughness=2, draw=1), ["lost_target"])[0]
        self.assertIn(dict(op="assert_zone", player=0, zone="hand", value=0), fizzled["steps"])
        self.assertIn(dict(op="assert_zone", player=0, zone="graveyard", value=2), fizzled["steps"])
        boon = cases("Boon of Erebos", "pump_creature", dict(power=2, toughness=0, lose_life=2), ["opponent"])
        self.assertIn(dict(op="assert_life", player=0, value=18), boon[0]["steps"])

    def test_coupled_effect_parameters_reject_wrong_family_and_invalid_amounts(self):
        record=self.oracle["Wildsize"]
        base=dict(family="pump_creature",version=2,variants=["payment"],oracle_text=record["oracle_text"])
        for params in (dict(power=2,toughness=2,draw=True), dict(power=2,toughness=2,draw=0),
                       dict(power=2,toughness=2,gain_life=21), dict(power=0,toughness=0,draw=1)):
            with self.assertRaises(ValueError): expand("Wildsize",999,dict(base,params=params),record,self.registry)
        with self.assertRaises(ValueError):
            expand("Divination",999,dict(family="draw_cards",version=2,params=dict(amount=2,draw=1),
                variants=["ordered"],oracle_text=self.oracle["Divination"]["oracle_text"]),self.oracle["Divination"],self.registry)

    def test_resolution_dialogs_are_scripted_only_after_a_successful_legal_cast(self):
        record=self.oracle["Volt Charge"]
        declaration=dict(family="damage_any_target",version=2,params=dict(amount=3),oracle_text=record["oracle_text"],
                         variants=["survivor","lost_target","insufficient_mana","snapshot"],resolve_dialogs=[0])
        generated=expand("Volt Charge",11712,declaration,record,self.registry)
        for c,count in zip(generated,(1,0,0,2)):
            self.assertEqual(sum(s["op"]=="choose_dialog" for s in c["steps"]),count)
            for i,s in enumerate(c["steps"]):
                if s["op"]=="choose_dialog": self.assertEqual(c["steps"][i+1]["op"],"resolve")
        for dialogs in ([True],[-1],[33],"0"):
            with self.assertRaises(ValueError):
                expand("Volt Charge",11712,dict(declaration,resolve_dialogs=dialogs),record,self.registry)

    def test_destruction_scope_ownership_fizzling_and_snapshots(self):
        name="Nature's Claim";record=self.oracle[name];csvid=self.registry[name]["csvid"]
        declaration=dict(family="destroy_permanent",version=2,params=dict(targets=["artifact","enchantment"],target_gain_life=4),
            variants=["opponent","friendly","lost_target","wrong_target","target_enchantment","snapshot"],oracle_text=record["oracle_text"])
        generated=expand(name,csvid,declaration,record,self.registry)
        for case,owner,life in zip(generated[:3],(1,0,1),(24,24,20)):
            self.assertIn(dict(op="assert_life",player=owner,value=life),case["steps"])
        self.assertIn(dict(op="assert_zone",player=0,zone="graveyard",value=2),generated[1]["steps"])
        self.assertEqual(generated[3]["setup"][1]["name"],"Grizzly Bears")
        self.assertEqual(generated[4]["setup"][1]["name"],"Glorious Anthem")
        snapshot=generated[5]["steps"]
        self.assertEqual(sum(s==dict(op="assert_life",player=1,value=24) for s in snapshot),2)
        self.assertLess(next(i for i,s in enumerate(snapshot) if s==dict(op="assert_life",player=1,value=24)),
                        next(i for i,s in enumerate(snapshot) if s["op"]=="record_state"))
        for params in (dict(targets=[]),dict(targets=[{}]),dict(targets=["artifact","artifact"]),
                       dict(targets=["planeswalker"]),dict(targets=["artifact"],target_gain_life=True)):
            with self.assertRaises(ValueError):expand(name,csvid,dict(declaration,params=params),record,self.registry)

    def test_explicit_support_setup_is_bounded_and_keeps_zone_accounting_valid(self):
        name="Artificer's Epiphany";record=self.oracle[name]
        support=dict(name="Sol Ring",ref="support_artifact",player=0,zone="battlefield")
        declaration=dict(family="draw_cards",version=2,params=dict(amount=2),variants=["ordered","snapshot"],
                         oracle_text=record["oracle_text"],setup=[support])
        generated=expand(name,self.registry[name]["csvid"],declaration,record,self.registry)
        for c in generated:self.assertIn(support,c["setup"])
        for changes in (dict(ref="source"),dict(zone="hand"),dict(player=True),dict(name="Not a card")):
            with self.assertRaises(ValueError):expand(name,999,dict(declaration,setup=[dict(support,**changes)]),record,self.registry)
        with self.assertRaises(ValueError):expand(name,999,dict(declaration,setup=[support,support]),record,self.registry)

    def test_movement_tracks_owner_library_order_and_fizzle(self):
        name = "Oust"; record = self.oracle[name]
        declaration = dict(family="move_permanent", version=2, params=dict(targets=["creature"], destination="library_second", target_gain_life=3),
                           variants=["opponent", "friendly", "lost_target", "snapshot"], oracle_text=record["oracle_text"])
        cases = expand(name, self.registry[name]["csvid"], declaration, record, self.registry)
        bear = self.registry["Grizzly Bears"]["csvid"]
        for case, owner in zip(cases[:2], (1, 0)):
            self.assertIn(dict(op="assert_zone_card", player=owner, zone="library", index=1, value=bear), case["steps"])
            self.assertIn(dict(op="assert_life", player=owner, value=23), case["steps"])
            self.assertIn(dict(op="assert_zone", player=owner, zone="hand", value=1), case["steps"])
        fizzled = cases[2]["steps"]
        self.assertIn(dict(op="assert_life", player=1, value=20), fizzled)
        self.assertIn(dict(op="assert_zone", player=1, zone="graveyard", value=1), fizzled)
        self.assertIn(dict(op="assert_zone", player=1, zone="library", value=3), fizzled)
        self.assertEqual(sum(s==dict(op="assert_life", player=1, value=23) for s in cases[3]["steps"]), 2)

    def test_movement_controller_and_tapped_restrictions_fail_closed(self):
        name="Narrow Escape";record=self.oracle[name]
        declaration=dict(family="move_permanent", version=2, params=dict(targets=["artifact"], destination="hand", controller="self"),
                         variants=["friendly", "wrong_controller", "insufficient_mana"], oracle_text=record["oracle_text"])
        cases=expand(name, self.registry[name]["csvid"], declaration, record, self.registry)
        self.assertEqual(next(c for c in cases[0]["setup"] if c["ref"]=="recipient")["player"],0)
        self.assertEqual(next(c for c in cases[1]["setup"] if c["ref"]=="recipient")["player"],1)
        self.assertIn(dict(op="can_cast", ref="source", value=0), cases[1]["steps"])
        self.assertEqual(next(c for c in cases[2]["setup"] if c["ref"]=="recipient")["state"],["tapped"])
        for variant in ("opponent", "wrong_state"):
            with self.assertRaises(ValueError): expand(name, 999, dict(declaration, variants=[variant]), record, self.registry)
        declaration["params"]["target_tapped"]=True
        with self.assertRaises(ValueError): expand(name,999,dict(declaration,variants=["untapped"]),record,self.registry)

    def test_movement_draw_cannot_occur_when_the_only_target_is_illegal(self):
        name="Repulse";record=self.oracle[name]
        declaration=dict(family="move_permanent",version=2,params=dict(targets=["creature"],destination="hand",draw=1),
                         variants=["opponent","lost_target","snapshot"],oracle_text=record["oracle_text"])
        cases=expand(name,self.registry[name]["csvid"],declaration,record,self.registry)
        self.assertIn(dict(op="assert_zone",player=0,zone="hand",value=2),cases[0]["steps"])
        self.assertIn(dict(op="assert_zone_card",player=0,zone="library",index=0,value=self.registry["Lightning Bolt"]["csvid"]),cases[0]["steps"])
        self.assertIn(dict(op="assert_zone",player=0,zone="hand",value=1),cases[1]["steps"])
        self.assertIn(dict(op="assert_zone",player=0,zone="library",value=3),cases[1]["steps"])
        for changes in (dict(draw=True),dict(targets=[]),dict(destination="exile"),dict(controller="owner"),dict(target_tapped=False),dict(destination="library_top")):
            with self.assertRaises(ValueError): expand(name,999,dict(declaration,params=dict(declaration["params"],**changes)),record,self.registry)

    def test_movement_cast_choices_and_effects_wait_for_resolution(self):
        name="Cyclonic Rift";record=self.oracle[name]
        declaration=dict(family="move_permanent",version=2,params=dict(targets=["artifact"],destination="hand",controller="opponent"),
                         variants=["opponent","payment","lost_target","snapshot"],cast_dialogs=dict(opponent=[1],lost_target=[1],snapshot=[1]),oracle_text=record["oracle_text"])
        cases=expand(name,self.registry[name]["csvid"],declaration,record,self.registry)
        for case,count in zip(cases,(1,0,1,2)):
            self.assertEqual(sum(s["op"]=="choose_dialog" for s in case["steps"]),count)
            first_resolve=next(i for i,s in enumerate(case["steps"]) if s["op"]=="resolve")
            self.assertIn(dict(op="assert_card",ref="recipient",property="in_play",value=1),case["steps"][:first_resolve])
        for choices in (dict(unknown=[1]),dict(opponent=[True]),dict(opponent=[-1]),[1]):
            with self.assertRaises(ValueError):expand(name,999,dict(declaration,cast_dialogs=choices),record,self.registry)

    def test_reductions_cover_negative_power_lethality_and_cleanup(self):
        name="Grasp of Darkness";record=self.oracle[name]
        declaration=dict(family="reduce_creature",version=2,params=dict(power=-4,toughness=-4),
                         variants=["friendly_cleanup","negative_power","lethal","snapshot"],oracle_text=record["oracle_text"])
        cases=expand(name,self.registry[name]["csvid"],declaration,record,self.registry)
        for property,value in (("power",3),("toughness",2)):
            self.assertIn(dict(op="assert_card",ref="recipient",property=property,value=value),cases[0]["steps"])
        self.assertIn(dict(op="cleanup"),cases[0]["steps"])
        self.assertIn(dict(op="assert_card",ref="recipient",property="power",value=-4),cases[1]["steps"])
        self.assertIn(dict(op="assert_zone_contains",player=1,zone="graveyard",value=self.registry["Llanowar Elves"]["csvid"]),cases[2]["steps"])
        self.assertIn(dict(op="assert_recorded_state"),cases[3]["steps"])
        for params in (dict(power=True,toughness=-1),dict(power=0,toughness=0),dict(power=-11,toughness=0),dict(power=-1,toughness=0,tap=False)):
            with self.assertRaises(ValueError):expand(name,999,dict(declaration,params=params),record,self.registry)
        for variant,params in (("lethal",dict(power=-1,toughness=0)),("negative_power",dict(power=1,toughness=-1))):
            with self.assertRaises(ValueError):expand(name,999,dict(declaration,variants=[variant],params=params),record,self.registry)

    def test_reduction_coupled_effects_wait_for_resolution_and_fizzle(self):
        name="Ulcerate";record=self.oracle[name]
        declaration=dict(family="reduce_creature",version=2,params=dict(power=-3,toughness=-3,lose_life=3),
                         variants=["lethal","lost_target"],oracle_text=record["oracle_text"])
        cases=expand(name,self.registry[name]["csvid"],declaration,record,self.registry)
        self.assertIn(dict(op="assert_life",player=0,value=17),cases[0]["steps"])
        self.assertNotIn(dict(op="assert_life",player=0,value=17),cases[1]["steps"])
        for case in cases:
            first_resolve=next(i for i,s in enumerate(case["steps"]) if s["op"]=="resolve")
            self.assertIn(dict(op="assert_life",player=0,value=20),case["steps"][:first_resolve])
        name="Fleeting Distraction";record=self.oracle[name]
        declaration=dict(family="reduce_creature",version=2,params=dict(power=-1,toughness=0,draw=1),
                         variants=["opponent_cleanup","lost_target"],oracle_text=record["oracle_text"])
        cases=expand(name,self.registry[name]["csvid"],declaration,record,self.registry)
        self.assertIn(dict(op="assert_zone",player=0,zone="hand",value=2),cases[0]["steps"])
        self.assertIn(dict(op="assert_zone",player=0,zone="hand",value=1),cases[1]["steps"])

    def test_enrollment_is_explicit_idempotent_and_preserves_authored_bytes(self):
        with tempfile.TemporaryDirectory(dir=Path.cwd()) as temporary:
            data = Path(temporary)
            (data / "oracle.json").write_text(json.dumps(dict(cards=self.oracle, source={})))
            (data / "oracle_aliases.json").write_text(json.dumps(dict(aliases={})))
            path = data / "cases/cards/bolt.json"
            path.parent.mkdir(parents=True)
            raw = b'{\r\n "version":1,"card":"Lightning Bolt","csvid":145,\r\n "cases":[{"name":"AuthoredBolt","steps":[]}]\r\n}\r\n'
            path.write_bytes(raw)
            self.assertEqual(len(enroll(data, ["Lightning Bolt"], None, False, None, False)), 1)
            self.assertEqual(path.read_bytes(), raw)
            with self.assertRaisesRegex(ValueError, "unsupported/unknown"):
                enroll(data, ["Lightning Bolt", "Not a card"], None, False, None, True)
            self.assertEqual(path.read_bytes(), raw)
            enroll(data, ["Lightning Bolt"], None, False, None, True)
            expanded = path.read_bytes()
            self.assertTrue(expanded.startswith(raw.rstrip()[:-1].rstrip()))
            self.assertEqual(json.loads(expanded)["cases"], json.loads(raw)["cases"])
            self.assertEqual(enroll(data, ["Lightning Bolt"], None, False, None, True), [])
            self.assertEqual(path.read_bytes(), expanded)

    def test_review_keeps_new_failures_and_unexpected_passes_without_passing_logs(self):
        with tempfile.TemporaryDirectory(dir=Path.cwd()) as temporary:
            root = Path(temporary)
            log = root / "harness.log"
            log.write_text("DUEL_HARNESS_ERROR: missing target\n")
            cases = [dict(filter="Behavior.NewFailure", card="Lightning Bolt", status="failed", failure_kind="harness",
                          log=str(log), xml=str(root / "missing.xml")),
                     dict(filter="Behavior.NowFixed", card="Shock", status="unexpected_pass", failure_kind="passed",
                          log=str(log), xml=str(root / "missing.xml")),
                     dict(filter="Behavior.Passes", status="passed"),
                     dict(filter="Behavior.Known", status="known_failure"),
                     dict(filter="Behavior.Missing", status="unimplemented")]
            summary = root / "summary.json"
            summary.write_text(json.dumps(dict(results=cases, counts=dict(passed=1,known_failure=1,unimplemented=1,failed=1,unexpected_pass=1))))
            report = review([summary])["runs"][0]
            self.assertEqual([f["filter"] for f in report["failures"]], ["Behavior.NewFailure", "Behavior.NowFixed"])
            self.assertEqual(report["failures"][0]["diagnostics"], [dict(error="DUEL_HARNESS_ERROR: missing target")])

    def test_budget_baselines_do_not_accept_unscripted_choices_or_other_harness_errors(self):
        with tempfile.TemporaryDirectory(dir=Path.cwd()) as temporary:
            args = SimpleNamespace(output=Path(temporary), binary=Path(temporary)/"unused.exe",
                                   node=None, timeout=1, quiet=True)
            case = dict(filter="Behavior.BudgetProbe", card="Probe")
            baseline = {case["filter"]: dict(kind="budget", reason="Reviewed nonterminating engine call",
                patterns=[r"DUEL_HARNESS_ERROR: execution budget exhausted during input"])}
            for message, kind, status in (
                    ("execution budget exhausted during input", "budget", "known_failure"),
                    ("execution budget exhausted during RNG", "budget", "failed"),
                    ("unscripted choice kind=1", "harness", "failed"),
                    ("missing target", "harness", "failed")):
                result = SimpleNamespace(stdout="DUEL_HARNESS_ERROR: "+message+"\n", stderr="", returncode=1)
                with patch("run_cases.subprocess.run", return_value=result):
                    actual = run_case(args, case, baseline)
                self.assertEqual(actual["failure_kind"], kind)
                self.assertEqual(actual["status"], status)


if __name__ == "__main__":
    unittest.main()
