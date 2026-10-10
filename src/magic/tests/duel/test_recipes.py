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
    def test_generated_case_rejects_unbounded_work_allowance(self):
        from generate_cases import build
        with tempfile.TemporaryDirectory(dir=Path.cwd()) as temporary:
            data=Path(temporary);cases=data/"cases";cases.mkdir()
            (data/"oracle.json").write_text(json.dumps(dict(version=1,cards={"Wrath of God":self.oracle["Wrath of God"]},source={})))
            (data/"oracle_aliases.json").write_text(json.dumps(dict(version=1,aliases={})))
            (data/"fixtures.json").write_text(json.dumps(dict(version=1,fixtures={"empty_main":dict(cards=[])})))
            for budget in (True,-1,199999,2000001,4294967295):
                (cases/"wrath.json").write_text(json.dumps(dict(version=1,card="Wrath of God",csvid=290,cases=[
                    dict(name="BudgetBoundary",fixture="empty_main",steps=[dict(op="set_budget",value=budget)])])))
                with self.assertRaisesRegex(ValueError,"scripted work budget"):
                    build(data,data/"generated")

    def test_sweep_overlapping_types_and_indestructible_witnesses(self):
        record=self.oracle["Whipflare"]
        declaration=dict(family="sweep",version=2,params=dict(effect="damage",targets=["creature"],amount=2,exclude_types=["artifact"]),
                         oracle_text=record["oracle_text"],variants=["mixed","injured"])
        cases=expand("Whipflare",11716,declaration,record,self.registry,oracle_context=self.oracle)
        self.assertEqual(cases[0]["steps"][0],dict(op="set_budget",value=2000000))
        from sweep_recipes import BOARD
        for player in (0,1):
            for name in ("Darksteel Myr","Darksteel Gargoyle","Obsianus Golem"):
                ref=f"permanent_{player}_{BOARD.index(name)}"
                self.assertIn(dict(op="assert_card",ref=ref,property="in_play",value=1),cases[0]["steps"])
                self.assertIn(dict(op="assert_card",ref=ref,property="damage",value=0),cases[0]["steps"])
            for name in ("Dryad Arbor","Nyxborn Eidolon"):
                self.assertIn(dict(op="assert_card",ref=f"permanent_{player}_{BOARD.index(name)}",property="in_play",value=0),cases[0]["steps"])
        self.assertIn(dict(op="cast",ref="injury",value=1),cases[1]["steps"])
        self.assertFalse(any(s["op"]=="destroy" for s in cases[1]["steps"]))

    def test_sweep_death_counts_and_caller_owned_snapshots(self):
        record=self.oracle["Fumigate"]
        declaration=dict(family="sweep",version=2,params=dict(effect="destroy",targets=["creature"],gain_per_destroyed=1),
                         oracle_text=record["oracle_text"],variants=["mixed","empty_board","snapshot"])
        cases=expand("Fumigate",16385,declaration,record,self.registry,oracle_context=self.oracle)
        self.assertIn(dict(op="assert_life",player=0,value=38),cases[0]["steps"])
        self.assertIn(dict(op="assert_zone",player=1,zone="battlefield",value=14),cases[0]["steps"])
        self.assertFalse(any(s==dict(op="assert_life",player=0,value=38) for s in cases[1]["steps"]))
        self.assertEqual(sum(s["op"]=="cast" for s in cases[2]["steps"]),2)
        self.assertIn(dict(op="assert_recorded_state"),cases[2]["steps"])
        altered=copy.deepcopy(self.registry);altered["Fumigate"]["mana_cost"]="{9}";altered["Fumigate"]["handler"]="unrelated"
        self.assertEqual(cases,expand("Fumigate",16385,declaration,record,altered,oracle_context=self.oracle))

    def test_sweep_exile_duration_and_genuinely_insufficient_mana(self):
        from sweep_recipes import MANA_SOURCES
        record=self.oracle["Anger of the Gods"]
        declaration=dict(family="sweep",version=2,params=dict(effect="damage",targets=["creature"],amount=3,exile_on_death=True),
                         oracle_text=record["oracle_text"],variants=["later_death","expired_death","insufficient_mana"])
        cases=expand("Anger of the Gods",13373,declaration,record,self.registry,oracle_context=self.oracle)
        self.assertIn(dict(op="assert_zone",player=1,zone="exile",value=9),cases[0]["steps"])
        self.assertIn(dict(op="assert_zone",player=1,zone="exile",value=8),cases[1]["steps"])
        cleanup=next(i for i,s in enumerate(cases[1]["steps"]) if s["op"]=="cleanup")
        destroy=next(i for i,s in enumerate(cases[1]["steps"]) if s["op"]=="destroy")
        self.assertLess(cleanup,destroy)
        self.assertFalse(any(c["name"] in MANA_SOURCES and c["zone"]=="battlefield" for c in cases[2]["setup"]))
        self.assertIn(dict(op="cast",ref="source",value=0),cases[2]["steps"])

    def test_sweep_declarations_fail_closed(self):
        from sweep_recipes import spec,variants
        record=self.oracle["Wrath of God"]
        params=dict(effect="destroy",targets=["creature"])
        declaration=dict(family="sweep",version=2,params=params,oracle_text=record["oracle_text"],variants=variants(spec(params,[0,0,0,0,0,4])))
        for changes in (dict(effect="exile"),dict(targets=[]),dict(targets=[{}]),dict(colors=[{}]),dict(amount=2),dict(flying=1),dict(scope="owner"),dict(nonbasic=True),dict(unknown=1)):
            with self.assertRaises(ValueError):expand("Wrath of God",290,dict(declaration,params=dict(params,**changes)),record,self.registry,oracle_context=self.oracle)
        with self.assertRaises(ValueError):expand("Wrath of God",290,declaration,record,self.registry)
        with self.assertRaises(ValueError):expand("Wrath of God",290,dict(declaration,oracle_text="Changed"),record,self.registry,oracle_context=self.oracle)
        altered=copy.deepcopy(self.oracle);altered["Dryad Arbor"]["type_line"]="Creature — Dryad"
        with self.assertRaises(ValueError):expand("Wrath of God",290,declaration,record,self.registry,oracle_context=altered)
        with self.assertRaises(ValueError):spec(dict(effect="damage",amount=2,targets=[],players="none"),[0]*6)

    def test_sweep_normal_cost_dialog_is_explicit_and_replayed(self):
        record=self.oracle["Planar Outburst"]
        declaration=dict(family="sweep",version=2,params=dict(effect="destroy",targets=["creature"],exclude_types=["land"]),
                         oracle_text=record["oracle_text"],variants=["mixed","empty_board","snapshot"],cast_dialogs=dict(mixed=[0],snapshot=[0]))
        cases=expand("Planar Outburst",15401,declaration,record,self.registry,oracle_context=self.oracle)
        self.assertEqual(sum(s["op"]=="choose_dialog" for s in cases[0]["steps"]),1)
        self.assertFalse(any(s["op"]=="choose_dialog" for s in cases[1]["steps"]))
        self.assertEqual(sum(s["op"]=="choose_dialog" for s in cases[2]["steps"]),2)
        self.assertEqual(cases[2]["_recipe"]["cast_dialogs"],[0])

    def test_stat_sweep_indestructibility_does_not_prevent_zero_toughness_death(self):
        from sweep_recipes import BOARD
        r=self.oracle["Languish"]
        d=dict(family="sweep",version=2,params=dict(effect="stats",targets=["creature"],power=-4,toughness=-4),
               oracle_text=r["oracle_text"],variants=["mixed","cleanup"])
        cases=expand("Languish",15170,d,r,self.registry,oracle_context=self.oracle)
        for player in (0,1):
            self.assertIn(dict(op="assert_card",ref=f"permanent_{player}_{BOARD.index('Darksteel Myr')}",property="in_play",value=0),cases[0]["steps"])
            ref=f"permanent_{player}_{BOARD.index('Scaled Wurm')}"
            self.assertIn(dict(op="assert_card",ref=ref,property="toughness",value=2),cases[0]["steps"])
            self.assertIn(dict(op="assert_card",ref=ref,property="toughness",value=6),cases[1]["steps"])

    def test_stat_sweep_stacking_new_arrivals_and_signed_power(self):
        from sweep_recipes import BOARD
        for name,power,toughness,scope in (("Glorious Charge",1,1,"self"),("Hell Swarm",-1,0,"all")):
            r=self.oracle[name];d=dict(family="sweep",version=2,params=dict(effect="stats",targets=["creature"],power=power,toughness=toughness,scope=scope),
                                      oracle_text=r["oracle_text"],variants=["late_arrival","stacking"])
            arrival,stacking=expand(name,self.registry[name]["csvid"],d,r,self.registry,oracle_context=self.oracle)
            self.assertIn(dict(op="cast",ref="additional",value=1),arrival["steps"])
            self.assertIn(dict(op="assert_card",ref="additional",property="power",value=2),arrival["steps"])
            ref=f"permanent_0_{BOARD.index('Darksteel Myr')}"
            self.assertIn(dict(op="assert_card",ref=ref,property="power",value=power*2),stacking["steps"])
            self.assertIn(dict(op="cleanup"),stacking["steps"])
            self.assertIn(dict(op="assert_card",ref=ref,property="power",value=0),stacking["steps"])

    def test_stat_sweep_rejects_mixed_effects_and_unreviewed_parameters(self):
        from sweep_recipes import spec
        p=dict(effect="stats",targets=["creature"],power=1,toughness=1)
        for change in (dict(power=True),dict(toughness=-8),dict(amount=2),dict(players="all"),dict(untap=1),dict(draw=True),
                       dict(exclude_subtypes=["Unknown"]),dict(opponent_power=-1),dict(opponent_power=-1,opponent_toughness=-1,scope="self")):
            with self.assertRaises(ValueError):spec(dict(p,**change),[0]*6)
        with self.assertRaises(ValueError):spec(dict(effect="damage",targets=["creature"],amount=2,power=1),[0]*6)

    def test_stat_sweep_subtype_and_opposing_changes_are_distinct(self):
        from sweep_recipes import BOARD
        r=self.oracle["Zealous Persecution"]
        d=dict(family="sweep",version=2,params=dict(effect="stats",targets=["creature"],power=1,toughness=1,opponent_power=-1,opponent_toughness=-1),
               oracle_text=r["oracle_text"],variants=["mixed"])
        steps=expand("Zealous Persecution",10378,d,r,self.registry,oracle_context=self.oracle)[0]["steps"]
        ref=f"permanent_0_{BOARD.index('Darksteel Myr')}"
        self.assertIn(dict(op="assert_card",ref=ref,property="power",value=1),steps)
        self.assertIn(dict(op="assert_card",ref=f"permanent_1_{BOARD.index('Darksteel Myr')}",property="in_play",value=0),steps)
        r=self.oracle["Eyeblight Massacre"];d.update(params=dict(effect="stats",targets=["creature"],power=-2,toughness=-2,exclude_subtypes=["Elf"]),oracle_text=r["oracle_text"])
        steps=expand("Eyeblight Massacre",15126,d,r,self.registry,oracle_context=self.oracle)[0]["steps"]
        for player in (0,1):self.assertIn(dict(op="assert_card",ref=f"elf_{player}",property="in_play",value=1),steps)

    def test_stat_sweep_global_death_effects_include_later_arrivals(self):
        for name,clause in (("Flaying Tendrils",dict(exile_on_death=True)),("Death Frenzy",dict(gain_per_destroyed=1))):
            r=self.oracle[name];d=dict(family="sweep",version=2,params=dict(effect="stats",targets=["creature"],power=-2,toughness=-2,**clause),
                                      oracle_text=r["oracle_text"],variants=["late_arrival_death"])
            c=expand(name,self.registry[name]["csvid"],d,r,self.registry,oracle_context=self.oracle)[0]
            self.assertIn(dict(op="assert_card",ref="additional",property="toughness",value=2),c["steps"])
            self.assertIn(dict(op="destroy",ref="additional"),c["steps"])
            if name=="Flaying Tendrils":self.assertIn(dict(op="assert_zone_contains",player=0,zone="exile",value=self.registry['Grizzly Bears']['csvid']),c['steps'])
            else:self.assertIn(dict(op="assert_life",player=0,value=33),c['steps'])

    def test_stat_sweep_repeated_casts_have_separate_optional_choices(self):
        r=self.oracle["Hideous Laughter"]
        d=dict(family="sweep",version=2,params=dict(effect="stats",targets=["creature"],power=-2,toughness=-2),
               oracle_text=r["oracle_text"],variants=["stacking"],cast_dialogs=dict(stacking=[0]),repeat_cast_dialogs=dict(stacking=[]))
        c=expand("Hideous Laughter",7231,d,r,self.registry,oracle_context=self.oracle)[0]
        self.assertEqual(sum(s['op']=='choose_dialog' for s in c['steps']),1)
        self.assertEqual(c['_recipe']['repeat_cast_dialogs'],[])
        for choices in ([0],dict(stacking=[True]),dict(stacking=[-1]),dict(mixed=[0])):
            with self.assertRaises(ValueError):expand("Hideous Laughter",7231,dict(d,repeat_cast_dialogs=choices),r,self.registry,oracle_context=self.oracle)

    def test_stat_sweep_two_large_costs_have_enough_generic_mana(self):
        r=self.oracle["Vitalizing Wind"]
        d=dict(family="sweep",version=2,params=dict(effect="stats",targets=["creature"],power=7,toughness=7,scope="self"),
               oracle_text=r['oracle_text'],variants=['stacking'])
        c=expand("Vitalizing Wind",4910,d,r,self.registry,oracle_context=self.oracle)[0]
        self.assertIn(dict(op='set_mana',player=0,color=0,value=16),c['steps'])
        self.assertIn(dict(op='set_mana',player=0,color=3,value=2),c['steps'])
        self.assertIn(dict(op='assert_mana_total',player=0,value=0),c['steps'])

    def test_damage_restrictions_real_combat_and_response(self):
        from damage_recipes import spec, variants
        from recipes import cost_from_oracle
        record = self.oracle["Sandblast"]
        params = dict(amount=5, targets=["creature"], combat="attacking_or_blocking")
        declaration = dict(family="damage_spell", version=2, params=params, oracle_text=record["oracle_text"],
                           variants=variants(spec(params, cost_from_oracle(record)), self.oracle))
        cases = expand("Sandblast",14617,declaration,record,self.registry,oracle_context=self.oracle)
        blocking = next(c for c in cases if c["name"].endswith("_blocking"))
        self.assertEqual([s["op"] for s in blocking["steps"][:7]],
                         ["turn", "begin_attackers", "attack", "finish_attackers", "begin_blockers", "block", "advance_combat"])
        lost = next(c for c in cases if c["name"].endswith("_lost_target"))
        self.assertIn(dict(op="choose_priority",ref="response"),lost["steps"])
        lost_combat = next(c for c in cases if c["name"].endswith("_lost_combat"))
        self.assertIn(dict(op="remove_from_combat",ref="recipient"),lost_combat["steps"])
        for variant in ("empty", "wrong_type", "noncombat"):
            self.assertIn(dict(op="can_cast",ref="source",value=0),next(c for c in cases if c["name"].endswith("_"+variant))["steps"])
        self.assertFalse(any(s["op"]=="combat_damage" for c in cases for s in c["steps"]))

    def test_damage_exile_duration_and_snapshot_replay(self):
        record=self.oracle["Magma Spray"]
        declaration=dict(family="damage_spell",version=2,params=dict(amount=2,targets=["creature"],exile_on_death=True),
                         oracle_text=record["oracle_text"],variants=["later_death","expired_death","indestructible","snapshot"])
        cases=expand("Magma Spray",10007,declaration,record,self.registry,oracle_context=self.oracle)
        self.assertIn(dict(op="assert_zone",player=1,zone="exile",value=3),cases[0]["steps"])
        self.assertIn(dict(op="assert_zone",player=1,zone="exile",value=2),cases[1]["steps"])
        self.assertIn(dict(op="assert_zone_card",player=1,zone="graveyard",index=4,value=self.registry["Scaled Wurm"]["csvid"]),cases[1]["steps"])
        self.assertLess(next(i for i,s in enumerate(cases[1]["steps"]) if s["op"]=="cleanup"), next(i for i,s in enumerate(cases[1]["steps"]) if s["op"]=="destroy"))
        self.assertIn(dict(op="assert_card",ref="recipient",property="in_play",value=1),cases[2]["steps"])
        self.assertEqual(sum(s["op"]=="cast" for s in cases[3]["steps"]),2)
        self.assertIn(dict(op="assert_recorded_state"),cases[3]["steps"])

    def test_damage_parameters_and_witnesses_fail_closed(self):
        record=self.oracle["Roast"]
        params=dict(amount=5,targets=["creature"],flying=False)
        declaration=dict(family="damage_spell",version=2,params=params,oracle_text=record["oracle_text"],variants=["wrong_flying"])
        for changes in (dict(amount=True),dict(amount=14),dict(targets=["land"]),dict(targets=[{}]),dict(colors=["C"]),dict(colors=[{}]),dict(controller="self"),dict(scry_on_your_turn=True),dict(flying=0),dict(unknown=1)):
            with self.assertRaises(ValueError):expand("Roast",14994,dict(declaration,params=dict(params,**changes)),record,self.registry,oracle_context=self.oracle)
        with self.assertRaises(ValueError):expand("Roast",14994,declaration,record,self.registry)
        altered=copy.deepcopy(self.oracle);altered["Mahamoti Djinn"]["keywords"]=[]
        with self.assertRaises(ValueError):expand("Roast",14994,declaration,record,self.registry,oracle_context=altered)
        with self.assertRaises(ValueError):expand("Roast",14994,dict(declaration,oracle_text="Changed"),record,self.registry,oracle_context=self.oracle)

    def test_damage_scry_and_gain_fizzle_with_last_target(self):
        record=self.oracle["Rage of Purphoros"]
        declaration=dict(family="damage_spell",version=2,params=dict(amount=4,targets=["creature"],scry=1,prevent_regeneration=True),
                         oracle_text=record["oracle_text"],variants=["scry_bottom","lost_target"])
        cases=expand("Rage of Purphoros",13529,declaration,record,self.registry,oracle_context=self.oracle)
        self.assertIn(dict(op="choose_zone",player=0,value=0),cases[0]["steps"])
        self.assertFalse(any(s["op"]=="choose_zone" for s in cases[1]["steps"]))
        self.assertIn(dict(op="assert_zone_card",player=0,zone="library",index=0,value=self.registry["Forest"]["csvid"]),cases[1]["steps"])
        record=self.oracle["Douse in Gloom"]
        declaration=dict(family="damage_spell",version=2,params=dict(amount=2,targets=["creature"],gain_life=2),
                         oracle_text=record["oracle_text"],variants=["friendly","lost_target","regenerate"])
        cases=expand("Douse in Gloom",1446,declaration,record,self.registry,oracle_context=self.oracle)
        self.assertIn(dict(op="assert_life",player=0,value=22),cases[0]["steps"])
        self.assertFalse(any(s==dict(op="assert_life",player=0,value=22) for s in cases[1]["steps"]))
        regen=cases[2]["steps"]
        self.assertLess(next(i for i,s in enumerate(regen) if s["op"]=="cast"),next(i for i,s in enumerate(regen) if s["op"]=="choose_priority"))
        self.assertIn(dict(op="assert_card",ref="recipient",property="tapped",value=1),regen)
        record=self.oracle["Engulfing Flames"]
        declaration=dict(family="damage_spell",version=2,params=dict(amount=1,targets=["creature"],prevent_regeneration=True),
                         oracle_text=record["oracle_text"],variants=["regeneration_expired"])
        expired=expand("Engulfing Flames",5558,declaration,record,self.registry,oracle_context=self.oracle)[0]["steps"]
        self.assertLess(next(i for i,s in enumerate(expired) if s["op"]=="cleanup"),next(i for i,s in enumerate(expired) if s["op"]=="choose_priority"))
        self.assertIn(dict(op="destroy",ref="recipient"),expired)

    def test_recovery_zone_indices_and_snapshot_multiplicities(self):
        from graveyard_recipes import spec, variants
        from recipes import cost_from_oracle
        record = self.oracle["Raise Dead"]
        declaration = dict(family="recover_card", version=2, params=dict(targets=["creature"], destination="hand"),
                           oracle_text=record["oracle_text"])
        declaration["variants"] = variants(spec(declaration["params"], cost_from_oracle(record)), self.oracle)
        cases = expand("Raise Dead",198,declaration,record,self.registry,oracle_context=self.oracle)
        ordinary = next(c for c in cases if c["name"].endswith("_return"))
        self.assertIn(dict(op="choose_zone",player=-1,value=4),ordinary["steps"])
        self.assertTrue(all(any(s["op"]=="choose_priority" for s in c["steps"]) for c in cases if c["name"].endswith(("lost_target","shifted_target"))))
        snapshot=next(c for c in cases if c["name"].endswith("snapshot"))
        self.assertIn(dict(op="choose_zone",player=-1,value=8),snapshot["steps"])
        self.assertIn(dict(op="assert_zone_card",player=0,zone="graveyard",index=0,value=106),snapshot["steps"])
        self.assertIn(dict(op="assert_hand_graveyard_total",player=0,index=106,value=3),snapshot["steps"])
        self.assertIn(dict(op="assert_recorded_state"),snapshot["steps"])
        altered=copy.deepcopy(self.registry);altered["Raise Dead"]["mana_cost"]="{9}"
        self.assertEqual(cases,expand("Raise Dead",198,declaration,record,altered,oracle_context=self.oracle))

    def test_recovery_parameters_and_owner_after_reanimation(self):
        record=self.oracle["Reanimate"]
        params=dict(targets=["creature"],destination="battlefield",graveyard="any",lose_life_from_value=True)
        declaration=dict(family="recover_card",version=2,params=params,oracle_text=record["oracle_text"],variants=["opponent_death_owner"])
        case=expand("Reanimate",self.registry["Reanimate"]["csvid"],declaration,record,self.registry,oracle_context=self.oracle)[0]
        self.assertIn(dict(op="destroy",ref="returned"),case["steps"])
        self.assertIn(dict(op="assert_zone_card",player=1,zone="graveyard",index=11,value=106),case["steps"])
        self.assertIn(dict(op="assert_life",player=0,value=18),case["steps"])
        for changes in (dict(value_max=True),dict(targets=["instant"]),dict(colors=["C"]),dict(bonus_subtype="Angel"),dict(destination="exile")):
            with self.assertRaises(ValueError):expand("Reanimate",999,dict(declaration,params=dict(params,**changes)),record,self.registry,oracle_context=self.oracle)
        with self.assertRaises(ValueError):expand("Reanimate",999,declaration,record,self.registry)
        with self.assertRaises(ValueError):expand("Reanimate",999,dict(declaration,oracle_text="Changed"),record,self.registry,oracle_context=self.oracle)

    def test_recovery_isolated_eligibility_and_fizzled_self_exile(self):
        from graveyard_recipes import WITNESSES
        record=self.oracle["Treasured Find"]
        parameters=dict(targets=["creature","instant","sorcery","artifact","enchantment","land"],destination="hand",self_destination="exile",target_witness="Grizzly Bears")
        declaration=dict(family="recover_card",version=2,params=parameters,oracle_text=record["oracle_text"],variants=["eligible_type_land","lost_target"])
        cases=expand("Treasured Find",12856,declaration,record,self.registry,oracle_context=self.oracle)
        probe=cases[0]
        self.assertEqual([c["name"] for c in probe["setup"] if c["zone"]=="graveyard"],["Forest","Forest"])
        self.assertIn(dict(op="can_cast",ref="source",value=1),probe["steps"])
        self.assertFalse(any(s["op"] in ("cast","choose_zone") for s in probe["steps"]))
        self.assertIn(dict(op="assert_zone",player=0,zone="exile",value=2),cases[1]["steps"])
        self.assertIn(dict(op="assert_zone_card",player=0,zone="graveyard",index=12,value=12856),cases[1]["steps"])
        for target in ("Unknown",[],"Lightning Bolt"):
            changes=dict(parameters,target_witness=target)
            if target=="Lightning Bolt":changes['targets']=["creature"]
            with self.assertRaises(ValueError):expand("Treasured Find",12856,dict(declaration,params=changes),record,self.registry,oracle_context=self.oracle)

    def test_recovery_shifted_target_has_independent_order_expectations(self):
        record=self.oracle["Reclaim"]
        declaration=dict(family="recover_card",version=2,params=dict(targets=["creature","instant","sorcery","artifact","enchantment","land"],destination="library_top"),oracle_text=record["oracle_text"],variants=["shifted_target"])
        case=expand("Reclaim",self.registry["Reclaim"]["csvid"],declaration,record,self.registry,oracle_context=self.oracle)[0]
        self.assertEqual([s["value"] for s in case["steps"] if s["op"]=="choose_zone"],[11,0])
        self.assertIn(dict(op="assert_zone_card",player=0,zone="library",index=0,value=self.registry["Shivan Dragon"]["csvid"]),case["steps"])
        self.assertIn(dict(op="assert_zone_card",player=0,zone="library",index=1,value=self.registry["Forest"]["csvid"]),case["steps"])
        self.assertFalse(any(s["op"]=="event" for s in case["steps"]))

    def test_counterspell_explicit_parameters_and_witness_context(self):
        from counter_recipes import spec, variants
        from recipes import cost_from_oracle
        record = self.oracle["Mana Leak"]
        parameters = dict(targets=["creature", "instant", "sorcery", "artifact", "enchantment", "aura"], tax=3)
        declaration = dict(family="counter_spell", version=2, params=parameters, oracle_text=record["oracle_text"],
                           variants=variants(spec(parameters, cost_from_oracle(record))))
        cases = expand("Mana Leak", 1078, declaration, record, self.registry, oracle_context=self.oracle)
        self.assertEqual(len(cases), 14)
        paid = next(c for c in cases if c["name"].endswith("tax_paid"))
        self.assertIn(dict(op="choose_dialog", player=1, value=0), paid["steps"])
        self.assertIn(dict(op="assert_mana_total", player=1, value=55), paid["steps"])
        self.assertFalse(any(s["op"] == "event" for c in cases for s in c["steps"]))
        changed = copy.deepcopy(self.registry)
        changed["Grizzly Bears"]["mana_cost"] = "{9}"
        self.assertEqual(cases, expand("Mana Leak",1078,declaration,record,changed,oracle_context=self.oracle))
        for changes in (dict(tax=True), dict(targets=["land"]), dict(value_exact=2,value_min=1),
                        dict(value_min=5),dict(mill=5),dict(target_damage_from_power=True)):
            with self.assertRaises(ValueError):
                expand("Mana Leak",1078,dict(declaration,params=dict(parameters,**changes)),record,self.registry,oracle_context=self.oracle)
        with self.assertRaises(ValueError):expand("Mana Leak",1078,declaration,record,self.registry)
        with self.assertRaises(ValueError):expand("Mana Leak",1078,dict(declaration,oracle_text=""),dict(record,oracle_text=""),self.registry,oracle_context=self.oracle)
        with self.assertRaises(ValueError):expand("Mana Leak",1078,{k:v for k,v in declaration.items() if k!="variants"},record,self.registry,oracle_context=self.oracle)
        changed_oracle=copy.deepcopy(self.oracle);changed_oracle["Grizzly Bears"]["cmc"]=5
        with self.assertRaises(ValueError):expand("Mana Leak",1078,declaration,record,self.registry,oracle_context=changed_oracle)
        with self.assertRaises(ValueError):expand("Mana Leak",1078,dict(declaration,oracle_text="Changed"),record,self.registry,oracle_context=self.oracle)

    def test_counterspell_destination_and_snapshot_expectations(self):
        record = self.oracle["Spell Crumple"]
        declaration = dict(family="counter_spell",version=2,oracle_text=record["oracle_text"],
                           params=dict(targets=["creature"],destination="library_bottom",self_destination="library_bottom"),
                           variants=["own_spell", "snapshot"])
        cases=expand("Spell Crumple",11730,declaration,record,self.registry,oracle_context=self.oracle)
        self.assertIn(dict(op="assert_zone_card",player=0,zone="library",index=7,value=106),cases[0]["steps"])
        self.assertIn(dict(op="assert_zone_card",player=0,zone="library",index=8,value=11730),cases[0]["steps"])
        snapshot=cases[1]["steps"]
        self.assertEqual(sum(s["op"]=="cast" for s in snapshot),2)
        self.assertTrue(all(dict(op=op) in snapshot for op in ("capture","ai_roundtrip","restore","assert_recorded_state")))

    def test_creature_whole_text_and_parameter_boundaries(self):
        from creature_recipes import classify as creature, variants
        record = self.oracle["Serra Angel"]
        expected = creature(record)
        self.assertEqual(expected["keywords"], ["Flying", "Vigilance"])
        declaration = dict(family="creature_keywords", version=2, params=dict(keywords=expected["keywords"]),
                           oracle_text=record["oracle_text"], variants=variants(expected))
        cases = expand("Serra Angel", 221, declaration, record, self.registry)
        self.assertIn("flying_evasion", [c["_recipe"]["variant"] for c in cases])
        self.assertTrue(any(s == dict(op="assert_card", ref="source", property="tapped", value=0)
                            for c in cases if c["_recipe"]["variant"]=="unblocked" for s in c["steps"]))
        for changes in (dict(oracle_text=record["oracle_text"]+"\nDraw a card."),
                        dict(oracle_text="If you control a Plains, this creature has flying."),
                        dict(oracle_text="Flying (Draw a card.)"), dict(power="*"), dict(toughness="0"),
                        dict(layout="transform"), dict(keywords=["Flying"]), dict(mana_cost="{W/P}")):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                creature(dict(record, **changes))
        for params in ({"keywords": ["Flying"]}, {"keywords": ["Flying", "Vigilance"], "power": 99}):
            with self.subTest(params=params), self.assertRaises(ValueError):
                expand("Serra Angel", 221, dict(declaration, params=params), record, self.registry)
        with self.assertRaises(ValueError):
            expand("Serra Angel", 221, dict(declaration, variants=["deathtouch"]), record, self.registry)

    def test_creature_expectations_use_structured_oracle_and_ignore_dispatch(self):
        from creature_recipes import classify as creature, variants
        record = dict(self.oracle["Grizzly Bears"], power="3", toughness="4")
        declaration = dict(family="creature_keywords", version=2, params=dict(keywords=[]),
                           oracle_text="", variants=variants(creature(record)))
        first = expand("Grizzly Bears", 106, declaration, record, self.registry)
        altered = copy.deepcopy(self.registry); altered["Grizzly Bears"]["handler"] = "card_dummy"
        self.assertEqual(first, expand("Grizzly Bears", 106, declaration, record, altered))
        self.assertIn(dict(op="assert_card", ref="source", property="power", value=3), first[0]["steps"])
        self.assertIn(dict(op="assert_card", ref="source", property="toughness", value=4), first[0]["steps"])
        snapshot = next(c for c in first if c["_recipe"]["variant"]=="snapshot")
        self.assertEqual(sum(s["op"]=="cast" for s in snapshot["steps"]), 2)
        self.assertTrue(any(s["op"]=="ai_roundtrip" for s in snapshot["steps"]))

    def test_destruction_witness_and_tapped_parameters(self):
        name = "Assassinate"
        declaration = dict(family="destroy_permanent", version=2,
                           params=dict(targets=["creature"], creature_witness="Hill Giant", target_tapped=True),
                           oracle_text=self.oracle[name]["oracle_text"], variants=["opponent", "snapshot", "wrong_target"])
        cases = expand(name, self.registry[name]["csvid"], declaration, self.oracle[name], self.registry)
        for case in cases[:2]:
            victim = next(c for c in case["setup"] if c["ref"] == "recipient")
            other = next(c for c in case["setup"] if c["ref"] == "unselected")
            self.assertEqual(victim["name"], "Hill Giant")
            self.assertEqual(victim["state"], ["tapped"])
            self.assertNotEqual(victim["name"], other["name"])
        victim = next(c for c in cases[2]["setup"] if c["ref"] == "recipient")
        self.assertEqual(victim["name"], "Sol Ring")
        self.assertNotIn("state", victim)
        for extra in (dict(creature_witness=[]), dict(creature_witness="Lightning Bolt"),
                      dict(target_tapped=1), dict(target_tapped="tapped")):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                expand(name, self.registry[name]["csvid"], dict(declaration, params=dict(targets=["creature"], **extra)),
                       self.oracle[name], self.registry)
        for extra in (dict(creature_witness="Grizzly Bears"), dict(target_tapped=True)):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                expand(name, self.registry[name]["csvid"], dict(declaration, params=dict(targets=["artifact"], **extra)),
                       self.oracle[name], self.registry)

    def test_resolution_zone_choices_are_variant_scoped_and_replayed(self):
        name = "Opt"
        declaration = dict(family="draw_cards", version=2, params=dict(amount=1),
                           oracle_text=self.oracle[name]["oracle_text"],
                           variants=["ordered", "snapshot"], resolve_zones={"snapshot": [-1]})
        cases = expand(name, self.registry[name]["csvid"], declaration, self.oracle[name], self.registry)
        self.assertFalse(any(s["op"] == "choose_zone" for s in cases[0]["steps"]))
        self.assertEqual(sum(s["op"] == "choose_zone" for s in cases[1]["steps"]), 2)
        for i, action in enumerate(cases[1]["steps"]):
            if action["op"] == "resolve":
                self.assertEqual(cases[1]["steps"][i-1], dict(op="choose_zone", player=0, value=-1))
        self.assertEqual(cases[1]["_recipe"]["resolve_zones"], [-1])

    def test_resolution_zone_choices_reject_invalid_or_nonresolving_variants(self):
        name = "Opt"
        declaration = dict(family="draw_cards", version=2, params=dict(amount=1),
                           oracle_text=self.oracle[name]["oracle_text"], variants=["ordered", "insufficient_mana"])
        for choices in ({"missing": [-1]}, {"ordered": [True]}, {"ordered": [-2]},
                        {"ordered": []}, {"ordered": [0]*33}, {"insufficient_mana": [-1]}):
            with self.subTest(choices=choices), self.assertRaisesRegex(ValueError, "resolve_zones"):
                expand(name, self.registry[name]["csvid"], dict(declaration, resolve_zones=choices),
                       self.oracle[name], self.registry)

    def test_resolution_zone_choices_do_not_apply_to_fixture_setup_spells(self):
        name = "Revitalize"
        declaration = dict(family="gain_life", version=2, params=dict(amount=3, draw=1),
                           oracle_text=self.oracle[name]["oracle_text"], variants=["injured"],
                           resolve_zones={"injured": [-1]})
        case = expand(name, self.registry[name]["csvid"], declaration, self.oracle[name], self.registry)[0]
        resolutions = [i for i, action in enumerate(case["steps"]) if action["op"] == "resolve"]
        self.assertEqual(len(resolutions), 2)
        self.assertNotEqual(case["steps"][resolutions[0]-1]["op"], "choose_zone")
        self.assertEqual(case["steps"][resolutions[1]-1], dict(op="choose_zone", player=0, value=-1))

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
