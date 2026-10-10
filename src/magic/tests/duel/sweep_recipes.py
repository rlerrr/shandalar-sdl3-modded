"""Oracle-reviewed sweeps against mixed, populated boards on both sides."""
import re
from collections import Counter

TYPES = ("creature", "artifact", "enchantment", "land")
BOARD = ("Savannah Lions", "Hill Giant", "Scaled Wurm", "Mahamoti Djinn", "Suntail Hawk",
         "Scathe Zombies", "Obsianus Golem", "Darksteel Myr", "Darksteel Gargoyle", "Nyxborn Eidolon",
         "Dryad Arbor", "Sol Ring", "Darksteel Ingot", "Island Sanctuary", "Greed", "Lifeforce",
         "Telepathy", "Fervor", "Forest", "Plains", "Island", "City of Brass", "Darksteel Citadel")
LIBRARY = ("Forest", "Plains", "Grizzly Bears", "Forest", "Mountain", "Island", "Lightning Bolt")
GRAVE = ("Grizzly Bears", "Giant Growth", "Forest", "Sol Ring")
EXILE = ("Island", "Llanowar Elves")
MANA_SOURCES = {"Dryad Arbor", "Sol Ring", "Darksteel Ingot", "Forest", "Plains", "Island", "City of Brass", "Darksteel Citadel"}


def spec(parameters, mana):
    required = {"effect", "targets"}
    optional = {"amount", "scope", "players", "flying", "colors", "exclude_colors", "exclude_types",
                "land_subtypes", "nonbasic", "tapped", "scry", "exile_on_death", "gain_damage",
                "gain_per_destroyed", "controller_loss_per_destroyed", "self_loss_per_destroyed", "self_damage_extra",
                "power", "toughness", "opponent_power", "opponent_toughness", "exclude_subtypes", "untap", "draw"}
    if not isinstance(parameters, dict) or not required <= set(parameters) or set(parameters)-required-optional:
        raise ValueError("unknown or missing sweep parameters")
    if parameters["effect"] not in ("damage", "destroy", "stats"):
        raise ValueError("unsupported sweep effect")
    targets = parameters["targets"]
    if (not isinstance(targets, list) or any(not isinstance(t, str) or t not in TYPES for t in targets)
            or len(set(targets)) != len(targets) or not targets and parameters.get("players", "none") == "none"):
        raise ValueError("sweep requires reviewed permanent types or damaged players")
    if parameters.get("scope", "all") not in ("all", "self", "opponent", "target_player", "target_opponent"):
        raise ValueError("unsupported sweep ownership scope")
    if parameters.get("players", "none") not in ("none", "all", "opponents"):
        raise ValueError("unsupported sweep player damage scope")
    for key in ("amount", "scry", "gain_per_destroyed", "controller_loss_per_destroyed", "self_loss_per_destroyed", "self_damage_extra"):
        if key in parameters and (type(parameters[key]) is not int or not 1 <= parameters[key] <= (1 if key == "scry" else 6)):
            raise ValueError("sweep amounts require reviewed positive integers")
    stats_fields = {"power", "toughness", "opponent_power", "opponent_toughness", "exclude_subtypes", "untap", "draw"}
    if parameters["effect"] != "stats" and stats_fields & set(parameters):
        raise ValueError("stat components require a stat sweep")
    if parameters["effect"] == "stats":
        if targets != ["creature"] or not {"power", "toughness"} <= set(parameters):
            raise ValueError("stat sweep requires explicit creature power and toughness changes")
        if any(k in parameters for k in ("amount", "players", "gain_damage", "self_damage_extra", "self_loss_per_destroyed",
                                        "controller_loss_per_destroyed", "land_subtypes", "nonbasic")):
            raise ValueError("unsupported stat sweep component")
        for key in ("power", "toughness", "opponent_power", "opponent_toughness"):
            if key in parameters and (type(parameters[key]) is not int or not -7 <= parameters[key] <= 7):
                raise ValueError("stat sweep changes require bounded signed integers")
        if parameters["power"] == parameters["toughness"] == 0:
            raise ValueError("stat sweep must change power or toughness")
        if ("opponent_power" in parameters) != ("opponent_toughness" in parameters) or ("opponent_power" in parameters and parameters.get("scope", "all") != "all"):
            raise ValueError("opposing stat changes require both values and all-player scope")
        if "exclude_subtypes" in parameters and parameters["exclude_subtypes"] != ["Elf"]:
            raise ValueError("stat subtype exclusion requires the reviewed Elf predicate")
        if "draw" in parameters and (type(parameters["draw"]) is not int or not 1 <= parameters["draw"] <= 3):
            raise ValueError("stat draw count requires a bounded positive integer")
    elif parameters["effect"] == "damage":
        if "amount" not in parameters or any(t != "creature" for t in targets):
            raise ValueError("damage sweep requires amount and creature-only permanent scope")
        if any(k in parameters for k in ("gain_per_destroyed", "controller_loss_per_destroyed", "self_loss_per_destroyed", "self_damage_extra", "land_subtypes", "nonbasic", "tapped")):
            raise ValueError("destruction components cannot accompany damage sweeps")
    elif any(k in parameters for k in ("amount", "players", "gain_damage", "exile_on_death")):
        raise ValueError("damage components cannot accompany destruction sweeps")
    for key, allowed in (("colors", ("W", "U", "B", "R", "G")), ("exclude_colors", ("W", "U", "B", "R", "G")),
                         ("exclude_types", TYPES), ("land_subtypes", ("Forest", "Plains", "Island", "Mountain", "Swamp"))):
        if key in parameters and (not isinstance(parameters[key], list) or not parameters[key]
                or any(not isinstance(v, str) or v not in allowed for v in parameters[key]) or len(set(parameters[key])) != len(parameters[key])):
            raise ValueError("sweep predicates require reviewed unique values")
    for key in ("flying", "nonbasic", "tapped", "exile_on_death", "gain_damage", "untap"):
        if key in parameters and type(parameters[key]) is not bool:
            raise ValueError("sweep flags require explicit booleans")
    if "flying" in parameters and targets != ["creature"]:
        raise ValueError("flying sweep requires creature-only scope")
    if any(k in parameters for k in ("land_subtypes", "nonbasic")) and targets != ["land"]:
        raise ValueError("land sweep predicates require land-only scope")
    if parameters.get("gain_damage") and parameters.get("players", "none") == "none":
        raise ValueError("sweep life gain requires player damage")
    if parameters.get("self_damage_extra") and not parameters.get("self_loss_per_destroyed"):
        raise ValueError("extra damage requires a death-count component")
    return dict(family="sweep", mana=mana, **parameters)


def types(record):
    return {t for t in TYPES if re.search(r"\b"+t.title()+r"\b", record.get("type_line", ""))}


def affected(name, owner, tapped, p, oracle, chosen):
    r = oracle[name]
    return (bool(types(r) & set(p["targets"]))
            and (p.get("scope", "all") == "all" or owner == chosen)
            and not types(r) & set(p.get("exclude_types", []))
            and not any(re.search(r"\b"+s+r"\b", r["type_line"]) for s in p.get("exclude_subtypes", []))
            and (not p.get("colors") or bool(set(r["colors"]) & set(p["colors"])))
            and not set(r["colors"]) & set(p.get("exclude_colors", []))
            and ("flying" not in p or ("Flying" in r.get("keywords", [])) == p["flying"])
            and ("tapped" not in p or tapped == p["tapped"])
            and (not p.get("land_subtypes") or any(re.search(r"\b"+t+r"\b", r["type_line"]) for t in p["land_subtypes"]))
            and (not p.get("nonbasic") or "Basic" not in r["type_line"]))


def validate_witnesses(oracle, registry):
    creature = {"Savannah Lions": ("2", "1", ["W"], []), "Hill Giant": ("3", "3", ["R"], []),
                "Scaled Wurm": ("7", "6", ["G"], []), "Mahamoti Djinn": ("5", "6", ["U"], ["Flying"]),
                "Suntail Hawk": ("1", "1", ["W"], ["Flying"]), "Scathe Zombies": ("2", "2", ["B"], []),
                "Obsianus Golem": ("4", "6", [], []), "Darksteel Myr": ("0", "1", [], ["Indestructible"]),
                "Darksteel Gargoyle": ("3", "3", [], ["Flying", "Indestructible"]),
                "Nyxborn Eidolon": ("2", "1", ["B"], ["Bestow"]), "Dryad Arbor": ("1", "1", ["G"], [])}
    if not isinstance(oracle, dict):
        raise ValueError("sweeps require pinned Oracle witness context")
    for name in BOARD:
        r = oracle.get(name, {})
        expected_types = ({"creature", "artifact"} if name in ("Obsianus Golem", "Darksteel Myr", "Darksteel Gargoyle") else
                          {"creature", "enchantment"} if name == "Nyxborn Eidolon" else
                          {"creature", "land"} if name == "Dryad Arbor" else {"creature"} if name in creature else
                          {"artifact", "land"} if name == "Darksteel Citadel" else {"artifact"} if name in ("Sol Ring", "Darksteel Ingot") else
                          {"land"} if name in ("Forest", "Plains", "Island", "City of Brass") else {"enchantment"})
        if name not in registry or types(r) != expected_types:
            raise ValueError(f"sweep witness type changed: {name}")
        if name in creature and (r.get("power"), r.get("toughness"), r.get("colors"), r.get("keywords")) != creature[name]:
            raise ValueError(f"sweep creature characteristics changed: {name}")
        if name not in creature and ("Indestructible" in r.get("keywords", [])) != (name in ("Darksteel Ingot", "Darksteel Citadel")):
            raise ValueError(f"sweep destruction witness changed: {name}")
    for name, color in (("Island Sanctuary", "W"), ("Greed", "B"), ("Lifeforce", "G"), ("Telepathy", "U"), ("Fervor", "R")):
        if oracle[name].get("colors") != [color]:
            raise ValueError(f"sweep enchantment color changed: {name}")
    for name in ("Forest", "Plains", "Island", "Dryad Arbor"):
        expected = name if name != "Dryad Arbor" else "Forest"
        if not re.search(r"\b"+expected+r"\b", oracle[name]["type_line"]) or ("Basic" in oracle[name]["type_line"]) != (name != "Dryad Arbor"):
            raise ValueError(f"sweep land subtype/supertype changed: {name}")
    if oracle.get("Lightning Bolt", {}).get("oracle_text") != "Lightning Bolt deals 3 damage to any target." or oracle["Lightning Bolt"].get("mana_cost") != "{R}":
        raise ValueError("sweep injury witness changed: Lightning Bolt")
    for name, power, toughness in (("Grizzly Bears", "2", "2"), ("Llanowar Elves", "1", "1")):
        if (oracle.get(name, {}).get("power"), oracle.get(name, {}).get("toughness")) != (power, toughness):
            raise ValueError(f"sweep arrival/subtype witness changed: {name}")
    if not re.search(r"\bElf\b", oracle["Llanowar Elves"]["type_line"]) or oracle["Grizzly Bears"].get("oracle_text"):
        raise ValueError("sweep arrival/subtype witness changed")


def variants(p):
    result = ["mixed", "friendly_only", "opponent_only", "empty_board", "empty_zones", "payment",
              "insufficient_mana", "snapshot", "cleanup", "tapped_all", "untapped_all"]
    if p["effect"] in ("damage", "stats") and p["targets"]: result.append("injured")
    if p["effect"] == "stats":
        result += ["late_arrival", "stacking"]
        if p.get("exile_on_death") or p.get("gain_per_destroyed"): result.append("late_arrival_death")
        if p.get("draw"): result.remove("empty_zones")
        if p.get("gain_per_destroyed"): result.remove("stacking")  # Multiple overlapping death triggers need their own expectations.
    if p.get("scope") == "target_player": result.append("target_self")
    if p.get("scry"): result.append("scry_bottom")
    if p.get("exile_on_death") or p["effect"] == "stats" and p.get("gain_per_destroyed"): result += ["later_death", "expired_death"]
    return result


def expand(name, csvid, declaration, p, selected, registry, oracle):
    from recipes import card, step, observe, exact_mana
    result = []
    for variant in selected:
        stock = variant == "snapshot"
        chosen = 0 if variant == "target_self" or p.get("scope") == "self" else 1
        empty = variant == "empty_zones"
        hands = [list(("Grizzly Bears", "Giant Growth", "Lightning Bolt", "Plains") if stock else () if empty else ("Plains",)) for _ in (0, 1)]
        libraries = [list(() if empty else LIBRARY) for _ in (0, 1)]
        graves = [list(() if empty else GRAVE) for _ in (0, 1)]
        exiles = [list(() if empty else EXILE) for _ in (0, 1)]
        hands[0].append(name)
        setup = [card(name, "source", zone="hand")]
        if not stock:
            for player in (0, 1):
                for zone, names in (("hand", hands[player][:-1] if player == 0 else hands[player]), ("library", libraries[player]), ("graveyard", graves[player]), ("exile", exiles[player])):
                    setup += [card(n, f"{zone}_{player}_{i}", player, zone) for i, n in enumerate(names)]
        entities = []
        for player in (0, 1):
            if variant == "empty_board" or variant == "friendly_only" and player == 1 or variant == "opponent_only" and player == 0: continue
            for i, n in enumerate(BOARD):
                # Empty mana pools must genuinely be insufficient; do not leave auto-payable lands/rocks.
                if variant == "insufficient_mana" and n in MANA_SOURCES: continue
                tapped = variant == "tapped_all" or variant != "untapped_all" and i % 2 == 1
                ref = f"permanent_{player}_{i}"
                c = card(n, ref, player); c["state"] = ["tapped" if tapped else "untapped"]
                setup.append(c); entities.append(dict(name=n, ref=ref, player=player, tapped=tapped, damage=0, alive=True))
        if p["effect"] == "stats" and p.get("exclude_subtypes"):
            for player in (0, 1):
                if variant == "empty_board" or variant == "friendly_only" and player == 1 or variant == "opponent_only" and player == 0: continue
                setup.append(card("Llanowar Elves", f"elf_{player}", player))
                entities.append(dict(name="Llanowar Elves", ref=f"elf_{player}", player=player, tapped=False, damage=0, alive=True))
                # Remove mana ability eligibility from the insufficient-pool variant.
                if variant == "insufficient_mana": setup[-1]["state"] = ["tapped"]; entities[-1]["tapped"] = True
        if variant in ("late_arrival", "late_arrival_death", "stacking"):
            extra = "Grizzly Bears" if variant.startswith("late_arrival") else name
            setup.append(card(extra, "additional", zone="hand")); hands[0].append(extra)
        # Large simultaneous-damage boards perform much more event dispatch than single-target cases.
        # Keep their allowance explicit, bounded, and part of the captured host state.
        pre = [step("set_budget", value=2000000)]
        exact_stacking = variant == "stacking" and p["mana"][0]*2 > 10
        if exact_stacking: pre += exact_mana([v*2 for v in p["mana"]])
        if variant == "payment": pre += exact_mana(p["mana"])
        if variant == "insufficient_mana": pre += exact_mana([0]*6)
        injury = variant == "injured"
        if injury:
            setup.append(card("Lightning Bolt", "injury", zone="hand")); hands[0].append("Lightning Bolt")
            victim = next(e for e in entities if e["name"] == "Scaled Wurm" and e["player"] == 0)
            pre += [step("choose_target", ref=victim["ref"], player=0), step("cast", ref="injury", value=1), step("resolve"), step("assert_choices_consumed"), observe(victim["ref"], "damage", 3)]
            hands[0].remove("Lightning Bolt"); graves[0].append("Lightning Bolt"); victim["damage"] = 3
        effect = []
        if variant == "insufficient_mana":
            effect += [step("cast", ref="source", value=0), observe("source", "in_hand", 1)]
        else:
            effect += [step("can_cast", ref="source", value=1)]
            effect += [step("choose_dialog", player=0, value=d) for d in declaration.get("cast_dialogs", {}).get(variant, [])]
            # A sole legal opponent is selected automatically by the engine.
            if p.get("scope") == "target_player": effect.append(step("choose_player", player=0, value=chosen))
            effect += [step("cast", ref="source", value=1), step("assert_stack", value=1)]
            if p.get("scry") and libraries[0]: effect.append(step("choose_zone", player=0, value=0 if variant == "scry_bottom" else -1))
            effect += [step("assert_life", player=i, value=20) for i in (0, 1)]
            if p["effect"] == "stats":
                for e in entities:
                    if "creature" in types(oracle[e["name"]]):
                        effect += [observe(e["ref"], "power", int(oracle[e["name"]]["power"])), observe(e["ref"], "toughness", int(oracle[e["name"]]["toughness"]))]
            effect += [step("resolve"), step("assert_choices_consumed")]
            hands[0].remove(name); graves[0].append(name)
            if variant == "scry_bottom": libraries[0] = libraries[0][1:] + libraries[0][:1]
            for _ in range(p.get("draw", 0)):
                hands[0].append(libraries[0].pop(0))
        repetitions = 1
        if variant == "stacking":
            effect += [step("choose_dialog", player=0, value=d) for d in declaration.get("repeat_cast_dialogs", {}).get(variant, declaration.get("cast_dialogs", {}).get(variant, []))]
            if p.get("scope") == "target_player": effect.append(step("choose_player", player=0, value=chosen))
            effect += [step("cast", ref="additional", value=1)]
            if p.get("scry"): effect.append(step("choose_zone", player=0, value=-1))
            effect += [step("resolve"), step("assert_choices_consumed")]
            hands[0].remove(name); graves[0].append(name); repetitions = 2
            for _ in range(p.get("draw", 0)): hands[0].append(libraries[0].pop(0))
        counts = [0, 0]
        for e in entities:
            hit = variant != "insufficient_mana" and affected(e["name"], e["player"], e["tapped"], p, oracle, chosen)
            r = oracle[e["name"]]
            if hit and p["effect"] == "damage": e["damage"] += p["amount"]
            if p["effect"] == "stats" and "creature" in types(r):
                e["power"] = int(r["power"]); e["toughness"] = int(r["toughness"])
                if hit:
                    prefix = "opponent_" if e["player"] == 1 and "opponent_power" in p else ""
                    e["power"] += p[prefix+"power"]*repetitions; e["toughness"] += p[prefix+"toughness"]*repetitions
                    if p.get("untap"): e["tapped"] = False
            dead = hit and ((p["effect"] == "stats" and e["toughness"] <= 0)
                           or "Indestructible" not in r.get("keywords", []) and (p["effect"] == "destroy" or e["damage"] >= int(e.get("toughness", r.get("toughness", "999")))))
            if dead:
                e["alive"] = False; counts[e["player"]] += 1
                (exiles if p.get("exile_on_death") else graves)[e["player"]].append(e["name"])
        life = [20, 20]
        if variant != "insufficient_mana":
            players = (0, 1) if p.get("players") == "all" else (1,) if p.get("players") == "opponents" else ()
            for player in players: life[player] -= p.get("amount", 0)
            if p.get("gain_damage"): life[0] += len(players)*p["amount"]
            life[0] += sum(counts)*p.get("gain_per_destroyed", 0) - sum(counts)*p.get("self_loss_per_destroyed", 0) - p.get("self_damage_extra", 0)
            for player in (0, 1): life[player] -= counts[player]*p.get("controller_loss_per_destroyed", 0)
        if variant.startswith("late_arrival"):
            effect += [step("cast", ref="additional", value=1), step("resolve"), step("assert_choices_consumed")]
            hands[0].remove("Grizzly Bears")
            entities.append(dict(name="Grizzly Bears", ref="additional", player=0, tapped=False, damage=0, alive=True, power=2, toughness=2))
        checks = [step("assert_stack", value=0), step("assert_choices_consumed")]
        for e in entities:
            checks.append(observe(e["ref"], "in_play", int(e["alive"])))
            if e["alive"] and "creature" in types(oracle[e["name"]]):
                checks += [observe(e["ref"], "damage", e["damage"]), observe(e["ref"], "power", e.get("power", int(oracle[e["name"]]["power"]))), observe(e["ref"], "toughness", e.get("toughness", int(oracle[e["name"]]["toughness"])))]
            if p["effect"] == "stats" and e["alive"]: checks.append(observe(e["ref"], "tapped", int(e["tapped"])))
        if variant == "late_arrival_death":
            entrant = entities[-1]
            checks += [step("destroy", ref="additional"), observe("additional", "in_play", 0)]
            entrant["alive"] = False
            (exiles if p.get("exile_on_death") else graves)[0].append("Grizzly Bears")
            life[0] += p.get("gain_per_destroyed", 0)
        if variant == "cleanup" or p["effect"] == "stats" and variant == "stacking":
            checks.append(step("cleanup"))
            for e in entities:
                if e["alive"] and "creature" in types(oracle[e["name"]]):
                    checks += [observe(e["ref"], "in_play", 1), observe(e["ref"], "damage", 0)]
                    if p["effect"] == "stats": checks += [observe(e["ref"], "power", int(oracle[e["name"]]["power"])), observe(e["ref"], "toughness", int(oracle[e["name"]]["toughness"]))]
        if variant in ("later_death", "expired_death"):
            if variant == "expired_death": checks.append(step("cleanup"))
            victim = next(e for e in entities if e["name"] == "Scaled Wurm" and e["player"] == 1)
            if not victim["alive"] or not affected(victim["name"], 1, victim["tapped"], p, oracle, chosen):
                raise ValueError("sweep duration variant needs an affected surviving creature")
            checks += [step("destroy", ref=victim["ref"]), observe(victim["ref"], "in_play", 0)]
            victim["alive"] = False
            (exiles if variant == "later_death" and p.get("exile_on_death") else graves)[1].append(victim["name"])
            if variant == "later_death": life[0] += p.get("gain_per_destroyed", 0)
        checks += [step("assert_life", player=i, value=life[i]) for i in (0, 1)]
        mana = 0 if variant in ("payment", "insufficient_mana") or exact_stacking else 60-sum(p["mana"])*repetitions-int(injury)-2*int(variant.startswith("late_arrival"))
        checks += [step("assert_mana_total", player=0, value=mana), step("assert_mana_total", player=1, value=60)]
        for player in (0, 1):
            checks.append(step("assert_zone", player=player, zone="battlefield", value=sum(e["alive"] and e["player"] == player for e in entities)))
            for n in dict.fromkeys(hands[player]+graves[player]):
                checks.append(step("assert_hand_graveyard_total", player=player, index=registry[n]["csvid"], value=hands[player].count(n)+graves[player].count(n)))
            for zone, names in (("hand", hands[player]), ("graveyard", graves[player]), ("library", libraries[player]), ("exile", exiles[player])):
                checks.append(step("assert_zone", player=player, zone=zone, value=len(names)))
                if zone in ("hand", "graveyard", "exile"):
                    # Simultaneous deaths may be ordered by their owner; assert exact multiset, not an arbitrary order.
                    checks += [step("assert_zone_contains", player=player, zone=zone, value=registry[n]["csvid"]) for n in Counter(names)]
                else:
                    checks += [step("assert_zone_card", player=player, zone=zone, index=i, value=registry[n]["csvid"]) for i, n in enumerate(names)]
        actions = pre + ([step("capture")] if stock else []) + effect + checks
        if stock: actions += [step("record_state"), step("ai_roundtrip"), step("restore")] + effect + checks + [step("assert_recorded_state")]
        provenance = dict(family="sweep", version=2, variant=variant, params=declaration["params"], oracle_text=declaration["oracle_text"])
        if variant in declaration.get("cast_dialogs", {}): provenance["cast_dialogs"] = declaration["cast_dialogs"][variant]
        if variant in declaration.get("repeat_cast_dialogs", {}): provenance["repeat_cast_dialogs"] = declaration["repeat_cast_dialogs"][variant]
        result.append(dict(name=f"SweepRecipe_{csvid}_{variant}", card=name, fixture="stocked_zones" if stock else "empty_main", seed=123,
                           setup=setup, steps=actions, _recipe=provenance))
    return result
