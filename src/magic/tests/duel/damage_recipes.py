"""Reviewed targeted damage components, with normal stack and combat decisions."""
WITNESSES = ("Scaled Wurm", "Mahamoti Djinn", "Savannah Lions", "Suntail Hawk",
             "Hill Giant", "Drudge Skeletons", "Obsianus Golem", "Darksteel Myr",
             "Darksteel Gargoyle", "Wall of Stone", "Wall of Bone", "Shivan Dragon")
LIBRARY = ("Forest", "Plains", "Grizzly Bears", "Forest", "Mountain", "Island", "Lightning Bolt")
GRAVE = ("Grizzly Bears", "Giant Growth", "Forest", "Sol Ring")
EXILE = ("Island", "Llanowar Elves")


def spec(parameters, mana):
    required = {"amount", "targets"}
    optional = {"colors", "exclude_colors", "flying", "controller", "combat", "gain_life",
                "controller_damage", "scry", "scry_on_your_turn", "artifact_amount", "exile_on_death",
                "same_name_damage", "prevent_regeneration"}
    if not isinstance(parameters, dict) or not required <= set(parameters) or set(parameters)-required-optional:
        raise ValueError("unknown or missing targeted damage parameters")
    for key in ("amount", "gain_life", "controller_damage", "artifact_amount", "scry"):
        if key in parameters and (type(parameters[key]) is not int or not 1 <= parameters[key] <= (2 if key == "scry" else 13)):
            raise ValueError("targeted damage amounts require reviewed positive integers")
    targets = parameters["targets"]
    if (not isinstance(targets, list) or not targets
            or any(not isinstance(t, str) or t not in ("creature", "player") for t in targets)
            or len(set(targets)) != len(targets)):
        raise ValueError("damage targets require reviewed creature/player components")
    for key in ("colors", "exclude_colors"):
        if key in parameters and (not isinstance(parameters[key], list) or not parameters[key]
                or any(not isinstance(c, str) or c not in ("W", "U", "B", "R", "G") for c in parameters[key])
                or len(set(parameters[key])) != len(parameters[key])):
            raise ValueError("damage color predicates require reviewed colors")
    for key in ("flying", "scry_on_your_turn", "exile_on_death", "same_name_damage", "prevent_regeneration"):
        if key in parameters and type(parameters[key]) is not bool:
            raise ValueError("damage flags require explicit booleans")
    if parameters.get("controller", "any") not in ("any", "opponent") or parameters.get("combat", "any") not in ("any", "attacking", "attacking_or_blocking"):
        raise ValueError("unreviewed damage controller/combat predicate")
    predicates = ("colors", "exclude_colors", "flying", "controller", "combat", "artifact_amount", "controller_damage", "same_name_damage")
    if any(k in parameters for k in predicates) and "creature" not in targets:
        raise ValueError("damage creature components require creature targets")
    if parameters.get("scry_on_your_turn") and not parameters.get("scry"):
        raise ValueError("conditional scry requires a reviewed amount")
    if parameters.get("same_name_damage") and parameters.get("combat"):
        raise ValueError("same-name sweep needs separate combat cases")
    return dict(family="damage_spell", mana=mana, **parameters)


def eligible(name, parameters, oracle, ignore=None):
    r = oracle[name]
    return ((ignore == "color" or (not parameters.get("colors") or any(c in r["colors"] for c in parameters["colors"]))
             and not any(c in r["colors"] for c in parameters.get("exclude_colors", [])))
            and (ignore == "flying" or "flying" not in parameters or ("Flying" in r.get("keywords", [])) == parameters["flying"]))


def validate_witnesses(oracle, registry):
    expected = {"Scaled Wurm": ("7", "6", ["G"], []), "Mahamoti Djinn": ("5", "6", ["U"], ["Flying"]),
                "Savannah Lions": ("2", "1", ["W"], []), "Suntail Hawk": ("1", "1", ["W"], ["Flying"]),
                "Hill Giant": ("3", "3", ["R"], []), "Drudge Skeletons": ("1", "1", ["B"], ["Heal", "Regenerate"]),
                "Obsianus Golem": ("4", "6", [], []), "Darksteel Myr": ("0", "1", [], ["Indestructible"]),
                "Darksteel Gargoyle": ("3", "3", [], ["Flying", "Indestructible"]),
                "Wall of Stone": ("0", "8", ["R"], ["Defender"]), "Wall of Bone": ("1", "4", ["B"], ["Heal", "Regenerate", "Defender"]),
                "Shivan Dragon": ("5", "5", ["R"], ["Flying"])}
    if not isinstance(oracle, dict):
        raise ValueError("damage recipes require pinned witness Oracle context")
    for name, values in expected.items():
        r = oracle.get(name, {})
        if name not in registry or "Creature" not in r.get("type_line", "") or (r.get("power"), r.get("toughness"), r.get("colors"), r.get("keywords")) != values:
            raise ValueError(f"damage witness characteristics changed: {name}")
    if (oracle.get("Unsummon", {}).get("oracle_text") != "Return target creature to its owner's hand."
            or oracle["Unsummon"].get("mana_cost") != "{U}" or "Unsummon" not in registry):
        raise ValueError("damage response witness changed: Unsummon")
    if (oracle.get("Giant Growth", {}).get("oracle_text") != "Target creature gets +3/+3 until end of turn."
            or oracle["Giant Growth"].get("mana_cost") != "{G}" or "Giant Growth" not in registry):
        raise ValueError("regeneration expiration pump witness changed: Giant Growth")
    for name in ("Drudge Skeletons", "Wall of Bone"):
        if "{B}: Regenerate this creature." not in oracle[name].get("oracle_text", ""):
            raise ValueError(f"damage regeneration witness changed: {name}")


def variants(p, oracle):
    result = ["payment", "insufficient_mana", "snapshot"]
    if "player" in p["targets"]:
        result += ["opponent_player", "self_player"]
    if "creature" not in p["targets"]:
        return result
    candidates = [n for n in WITNESSES if eligible(n, p, oracle)]
    survivors = [n for n in candidates if int(oracle[n]["toughness"]) > p["amount"] or "Indestructible" in oracle[n].get("keywords", [])]
    if not survivors:
        raise ValueError("damage component needs a reviewed surviving target")
    result += ["survivor", "lethal", "lost_target"]
    if p.get("controller", "any") == "any":
        result.append("friendly")
    else:
        result.append("wrong_controller")
    if "player" not in p["targets"]:
        result += ["empty", "wrong_type"]
        for variant, ignore in (("wrong_color", "color"), ("wrong_flying", "flying")):
            if any(not eligible(n, p, oracle) and eligible(n, p, oracle, ignore) for n in WITNESSES):
                result.append(variant)
    if p.get("combat"):
        result += ["noncombat", "lost_combat"]
        if p["combat"] == "attacking_or_blocking":
            result.append("blocking")
    if p.get("artifact_amount"):
        result.append("artifact")
    if p.get("exile_on_death"):
        result += ["later_death", "expired_death", "indestructible"]
    if p.get("same_name_damage"):
        result.append("same_name")
    if p.get("scry"):
        result.append("scry_bottom")
        if p.get("scry_on_your_turn"):
            result.append("other_turn")
    # The engine's documented legacy timing regenerates during destruction.
    if not p.get("prevent_regeneration") and not p.get("combat") and p.get("controller", "any") == "any" and eligible("Drudge Skeletons", p, oracle):
        result.append("regenerate")
    if p.get("prevent_regeneration") and not p.get("combat") and p["amount"] < 7 and eligible("Wall of Bone", p, oracle):
        result.append("regeneration_expired")
    return result


def expand(name, csvid, declaration, p, selected, registry, oracle):
    from recipes import card, step, observe, exact_mana
    from collections import Counter
    cases = []
    candidates = [n for n in WITNESSES if eligible(n, p, oracle)]
    survivors = [n for n in candidates if int(oracle[n]["toughness"]) > p["amount"] or "Indestructible" in oracle[n].get("keywords", [])]
    lethal = [n for n in candidates if int(oracle[n]["toughness"]) <= p["amount"] and "Indestructible" not in oracle[n].get("keywords", []) and n not in ("Drudge Skeletons", "Wall of Bone")]
    for variant in selected:
        stock = variant == "snapshot"
        player_target = "creature" not in p["targets"] or variant in ("opponent_player", "self_player")
        owner = 0 if variant in ("friendly", "self_player", "regenerate", "regeneration_expired") else 1
        target = survivors[0] if survivors else "Scaled Wurm"
        if variant == "lethal":
            if not lethal:
                raise ValueError("damage lethal scenario needs a nonregenerating target")
            target = lethal[0]
        if variant == "regenerate": target = "Drudge Skeletons"
        if variant == "regeneration_expired": target = "Wall of Bone"
        if variant == "artifact": target = "Obsianus Golem"
        if variant == "indestructible": target = "Darksteel Myr"
        if variant in ("wrong_color", "wrong_flying"):
            target = next(n for n in WITNESSES if not eligible(n, p, oracle) and eligible(n, p, oracle, "color" if variant == "wrong_color" else "flying"))
        if variant == "wrong_type": target = "Sol Ring"
        if variant == "wrong_controller": owner = 0
        negative = variant in ("empty", "wrong_type", "wrong_color", "wrong_flying", "wrong_controller", "noncombat", "insufficient_mana")
        # Combat witnesses cannot be defenders; an indestructible flying witness can survive large amounts.
        if p.get("combat") and "Defender" in oracle[target].get("keywords", []):
            target = "Darksteel Gargoyle"
        if variant in ("artifact", "indestructible") and not eligible(target, p, oracle):
            raise ValueError("damage variant witness violates reviewed predicates")
        libraries = [list(LIBRARY), list(LIBRARY)]
        graves = [list(GRAVE), list(GRAVE)]
        exiles = [list(EXILE), list(EXILE)]
        hands = [list(("Grizzly Bears", "Giant Growth", "Lightning Bolt", "Plains") if stock else ("Plains",)) for _ in (0, 1)]
        hands[0].append(name)
        setup = [card(name, "source", zone="hand")]
        if not stock:
            for player in (0, 1):
                setup.append(card("Plains", f"kept_{player}", player, "hand"))
                for zone, names in (("library", LIBRARY), ("graveyard", GRAVE), ("exile", EXILE)):
                    setup += [card(n, f"{zone}_{player}_{i}", player, zone) for i, n in enumerate(names)]
        has_target = not player_target and variant != "empty"
        if has_target: setup.append(card(target, "recipient", owner))
        if not negative:
            setup.append(card("Hill Giant", "unselected", 1-owner))
        if variant == "same_name":
            setup += [card(target, "duplicate_0", 0), card(target, "duplicate_1", 1)]
        if variant == "lost_target":
            setup.append(card("Unsummon", "response", zone="hand")); hands[0].append("Unsummon")
        if variant == "regeneration_expired":
            setup.append(card("Giant Growth", "boost", zone="hand")); hands[0].append("Giant Growth")
        pre = []
        if variant == "payment": pre += exact_mana(p["mana"])
        if variant == "insufficient_mana": pre += exact_mana([0]*6)
        if variant == "other_turn": pre += [step("turn", player=1, value=0)]
        if p.get("combat") and not negative:
            turn = 0 if variant == "blocking" else owner
            pre += [step("turn", player=turn, value=0), step("begin_attackers"),
                    step("attack", ref="unselected" if variant == "blocking" else "recipient", value=1),
                    step("finish_attackers"), step("begin_blockers")]
            if variant == "blocking": pre += [step("block", ref="recipient", other="unselected", value=1)]
            pre.append(step("advance_combat"))
        effect = []
        if variant == "regeneration_expired":
            effect += [step("choose_target", player=0, ref="recipient"), step("cast", ref="boost", value=1), step("resolve"), step("assert_choices_consumed")]
            hands[0].remove("Giant Growth"); graves[0].append("Giant Growth")
        if negative:
            effect += [step("cast" if variant == "insufficient_mana" else "can_cast", ref="source", value=0), observe("source", "in_hand", 1)]
        else:
            effect += [step("choose_player", player=0, value=owner)] if player_target else [step("choose_target", player=0, ref="recipient")]
            if variant == "lost_target":
                effect += [step("choose_priority", ref="response"), step("choose_target", player=-1, ref="recipient")]
                hands[0].remove("Unsummon"); graves[0].append("Unsummon"); hands[owner].append(target)
            effect += [step("cast", ref="source", value=1), step("assert_stack", value=1)]
            if variant == "lost_combat": effect.append(step("remove_from_combat", ref="recipient"))
            resolved = variant not in ("lost_target", "lost_combat")
            scry = p.get("scry", 0) if resolved and (not p.get("scry_on_your_turn") or variant != "other_turn") else 0
            if scry:
                effect += [step("choose_zone", player=0, value=0)] * scry if variant == "scry_bottom" else [step("choose_zone", player=0, value=-1)]
                if scry > 1 and variant != "scry_bottom": effect += [step("choose_zone", player=0, value=-1)]
                if variant == "scry_bottom": libraries[0] = libraries[0][scry:] + libraries[0][:scry]
            if variant == "regenerate": effect.append(step("choose_priority", ref="recipient"))
            effect += [step("assert_life", player=i, value=20) for i in (0, 1)]
            effect += [step("resolve"), step("assert_choices_consumed")]
            hands[0].remove(name); graves[0].append(name)
        resolved = not negative and variant not in ("lost_target", "lost_combat")
        amount = p.get("artifact_amount", p["amount"]) if variant == "artifact" else p["amount"]
        dead = resolved and not player_target and variant not in ("regenerate", "regeneration_expired") and int(oracle[target]["toughness"]) <= amount and "Indestructible" not in oracle[target].get("keywords", [])
        checks = [step("assert_stack", value=0), step("assert_choices_consumed")]
        life = [20, 20]
        if resolved:
            life[0] += p.get("gain_life", 0)
            life[owner] -= amount if player_target else p.get("controller_damage", 0)
        if has_target:
            checks.append(observe("recipient", "in_play", int(not dead and variant != "lost_target")))
            if dead:
                (exiles if p.get("exile_on_death") else graves)[owner].append(target)
            elif variant != "lost_target":
                checks += [observe("recipient", "damage", amount if resolved and variant != "regenerate" else 0)]
                if variant == "regenerate": checks.append(observe("recipient", "tapped", 1))
                if variant == "regeneration_expired":
                    checks += [observe("recipient", "toughness", 7), step("cleanup"),
                               observe("recipient", "in_play", 1), observe("recipient", "toughness", 4), observe("recipient", "damage", 0),
                               step("choose_priority", ref="recipient"), step("destroy", ref="recipient"),
                               step("assert_choices_consumed"), observe("recipient", "in_play", 1), observe("recipient", "damage", 0), observe("recipient", "tapped", 1)]
                if variant in ("later_death", "expired_death"):
                    if variant == "expired_death": checks.append(step("cleanup"))
                    checks += [step("destroy", ref="recipient"), observe("recipient", "in_play", 0)]
                    (exiles if variant == "later_death" else graves)[owner].append(target)
                elif variant in ("survivor", "friendly", "indestructible", "artifact"):
                    checks += [step("cleanup"), observe("recipient", "damage", 0), observe("recipient", "in_play", 1)]
        if not negative: checks += [observe("unselected", "in_play", 1), observe("unselected", "damage", 0)]
        if variant == "same_name":
            for player in (0, 1):
                checks += [observe(f"duplicate_{player}", "damage", amount), observe(f"duplicate_{player}", "in_play", 1)]
        checks += [step("assert_life", player=i, value=life[i]) for i in (0, 1)]
        mana = 0 if variant in ("payment", "insufficient_mana") else 60 - (sum(p["mana"]) if not negative else 0) - int(variant == "lost_target") - int(variant == "regenerate") - 2*int(variant == "regeneration_expired")
        checks += [step("assert_mana_total", player=0, value=mana), step("assert_mana_total", player=1, value=60)]
        for player in (0, 1):
            for zone, names in (("hand", hands[player]), ("graveyard", graves[player]), ("library", libraries[player]), ("exile", exiles[player])):
                checks.append(step("assert_zone", player=player, zone=zone, value=len(names)))
                if zone == "hand":
                    for n in Counter(names):
                        checks.append(step("assert_hand_graveyard_total", player=player, index=registry[n]["csvid"], value=names.count(n)+graves[player].count(n)))
                else:
                    checks += [step("assert_zone_card", player=player, zone=zone, index=i, value=registry[n]["csvid"]) for i, n in enumerate(names)]
        actions = pre + ([step("capture")] if stock else []) + effect + checks
        if stock: actions += [step("record_state"), step("ai_roundtrip"), step("restore")] + effect + checks + [step("assert_recorded_state")]
        cases.append(dict(name=f"DamageRecipe_{csvid}_{variant}", card=name, fixture="stocked_zones" if stock else "empty_main", seed=123,
                          setup=setup, steps=actions, _recipe=dict(family="damage_spell", version=2, variant=variant,
                          params=declaration["params"], oracle_text=declaration["oracle_text"])))
    return cases
