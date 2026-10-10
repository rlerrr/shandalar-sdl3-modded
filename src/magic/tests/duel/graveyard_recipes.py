"""Oracle-reviewed single-card recovery, exercised through real spell responses."""

TYPES = ("creature", "instant", "sorcery", "artifact", "enchantment", "land")
WITNESSES = ("Forest", "Lightning Bolt", "Sol Ring", "Glorious Anthem", "Grizzly Bears",
             "Hill Giant", "Watchwolf", "Drudge Skeletons", "Divination", "Scathe Zombies",
             "Serra Angel", "Shivan Dragon")
LIBRARY = ("Forest", "Plains", "Grizzly Bears", "Forest", "Mountain", "Island", "Lightning Bolt")
STOCK_GRAVE = ("Grizzly Bears", "Giant Growth", "Forest", "Sol Ring")
STOCK_HAND = ("Grizzly Bears", "Giant Growth", "Lightning Bolt", "Plains")


def spec(parameters, mana):
    required = {"targets", "destination"}
    optional = {"graveyard", "colors", "value_max", "gain_life", "draw", "lose_life_from_value",
                "self_destination", "counters", "bonus_subtype", "bonus_counters", "bonus_draw", "target_witness"}
    if not isinstance(parameters, dict) or not required <= set(parameters) or set(parameters)-required-optional:
        raise ValueError("unknown or missing graveyard recovery parameters")
    targets = parameters["targets"]
    if (not isinstance(targets, list) or not targets
            or any(not isinstance(t, str) or t not in TYPES for t in targets) or len(set(targets)) != len(targets)):
        raise ValueError("graveyard recovery requires reviewed card types")
    if parameters["destination"] not in ("hand", "battlefield", "library_top"):
        raise ValueError("unsupported graveyard recovery destination")
    if parameters["destination"] == "battlefield" and any(t in ("instant", "sorcery") for t in targets):
        raise ValueError("battlefield recovery requires permanent card types")
    if parameters.get("graveyard", "self") not in ("self", "any"):
        raise ValueError("unsupported graveyard ownership restriction")
    if parameters.get("self_destination", "graveyard") not in ("graveyard", "exile"):
        raise ValueError("unsupported recovery spell destination")
    if "target_witness" in parameters and (not isinstance(parameters["target_witness"], str) or parameters["target_witness"] not in WITNESSES):
        raise ValueError("unknown recovery target witness")
    colors = parameters.get("colors", [])
    if (not isinstance(colors, list) or any(not isinstance(c, str) or c not in ("W", "U", "B", "R", "G", "multicolored") for c in colors)
            or len(set(colors)) != len(colors)):
        raise ValueError("unsupported graveyard color restriction")
    for key in ("value_max", "gain_life", "draw", "counters", "bonus_counters", "bonus_draw"):
        if key in parameters and (type(parameters[key]) is not int or not 1 <= parameters[key] <= (3 if "draw" in key else 10)):
            raise ValueError("graveyard recovery amounts require reviewed positive integers")
    if "lose_life_from_value" in parameters and type(parameters["lose_life_from_value"]) is not bool:
        raise ValueError("reanimation life loss requires an explicit boolean")
    if "bonus_subtype" in parameters and parameters["bonus_subtype"] not in ("Angel", "Dragon", "Zombie"):
        raise ValueError("unsupported recovery bonus subtype")
    if bool(parameters.get("bonus_subtype")) != bool(parameters.get("bonus_counters") or parameters.get("bonus_draw")):
        raise ValueError("recovery subtype and bonus must be paired")
    if (parameters.get("counters") or parameters.get("bonus_counters")) and parameters["destination"] != "battlefield":
        raise ValueError("recovery counters require battlefield entry")
    return dict(family="recover_card", mana=mana, **parameters)


def eligible(record, parameters, ignore=None):
    colors = record.get("colors", [])
    return ((ignore == "type" or any(t.title() in record.get("type_line", "") for t in parameters["targets"]))
            and (ignore == "color" or not parameters.get("colors") or any(
                len(colors) > 1 if c == "multicolored" else c in colors for c in parameters["colors"]))
            and (ignore == "value" or record["cmc"] <= parameters.get("value_max", 10)))


def validate_witnesses(oracle, registry):
    if not isinstance(oracle, dict):
        raise ValueError("recovery recipes require pinned witness Oracle context")
    for name in WITNESSES:
        record = oracle.get(name, {})
        if (name not in registry or record.get("layout", "normal") != "normal" or record.get("card_faces")
                or not isinstance(record.get("cmc"), (int, float)) or not 0 <= record["cmc"] <= 10
                or not any(t.title() in record.get("type_line", "") for t in TYPES)):
            raise ValueError(f"recovery witness Oracle characteristics changed: {name}")
        if "Creature" in record["type_line"] and any(not record.get(p, "").isdigit() for p in ("power", "toughness")):
            raise ValueError(f"recovery witness needs numeric Oracle characteristics: {name}")
    if oracle.get("Reclaim", {}).get("oracle_text") != "Put target card from your graveyard on top of your library." or "Reclaim" not in registry:
        raise ValueError("recovery response witness changed: Reclaim")


def variants(parameters, oracle):
    candidates = [n for n in WITNESSES if eligible(oracle[n], parameters)]
    if not candidates:
        raise ValueError("recovery declaration has no reviewed eligible card")
    result = ["return", "payment", "insufficient_mana", "empty", "lost_target", "snapshot"]
    if parameters.get("target_witness", candidates[0]) not in candidates:
        raise ValueError("recovery target witness violates Oracle-reviewed restrictions")
    if len(candidates) > 1:
        result.append("alternate")
    if WITNESSES.index(candidates[-1]) > 0:
        result.append("shifted_target")
    if parameters.get("graveyard", "self") == "any":
        result.append("opponent_graveyard")
    else:
        result.append("wrong_owner")
    for variant, ignore in (("wrong_type", "type"), ("wrong_color", "color"), ("wrong_value", "value")):
        if any(not eligible(oracle[n], parameters) and eligible(oracle[n], parameters, ignore) for n in WITNESSES):
            result.append(variant)
    for category in TYPES:
        if any(category.title() in oracle[n]["type_line"] for n in candidates):
            result.append("eligible_type_"+category)
            if category.title() not in oracle[parameters.get("target_witness", candidates[0])]["type_line"]:
                result.append("type_"+category)
    if parameters.get("bonus_subtype"):
        if not any(parameters["bonus_subtype"] in oracle[n]["type_line"] for n in candidates):
            raise ValueError("recovery subtype bonus has no eligible witness")
        result.append("subtype_bonus")
    if parameters["destination"] == "battlefield":
        result.append("death_owner")
        if parameters.get("graveyard") == "any":
            result.append("opponent_death_owner")
    return result


def expand(name, csvid, declaration, parameters, selected, registry, oracle):
    from recipes import card, step, observe, exact_mana, cost_from_oracle
    cases = []
    candidates = [n for n in WITNESSES if eligible(oracle[n], parameters)]
    for variant in selected:
        fixture = "stocked_zones" if variant == "snapshot" else "empty_main"
        stock = fixture == "stocked_zones"
        owner = int(variant in ("opponent_graveyard", "opponent_death_owner"))
        target = candidates[-1] if variant in ("alternate", "shifted_target") else parameters.get("target_witness", candidates[0])
        probe = variant.startswith("eligible_type_")
        if probe:
            target = next(n for n in candidates if variant[len("eligible_type_"):].title() in oracle[n]["type_line"])
        if variant.startswith("type_"):
            target = next(n for n in candidates if variant[5:].title() in oracle[n]["type_line"])
        if variant == "subtype_bonus":
            target = next(n for n in candidates if parameters["bonus_subtype"] in oracle[n]["type_line"])
        negative = variant in ("empty", "wrong_owner", "wrong_type", "wrong_color", "wrong_value", "insufficient_mana")
        graves = [list(STOCK_GRAVE) if stock else [] for _ in (0, 1)]
        for p in (0, 1):
            names = list(WITNESSES)
            if variant == "empty" or variant == "wrong_owner" and p == 0:
                names = []
            if variant.startswith("wrong_") and variant != "wrong_owner":
                ignore = {"wrong_type": "type", "wrong_color": "color", "wrong_value": "value"}[variant]
                names = [n for n in names if not eligible(oracle[n], parameters) and eligible(oracle[n], parameters, ignore)]
            if probe:
                names = [target]
            graves[p] += names
        index = (len(STOCK_GRAVE) if stock else 0) + list(WITNESSES).index(target)
        libraries = [list(LIBRARY), list(LIBRARY)]
        hands = [list(STOCK_HAND) if stock else ["Plains"] for _ in (0, 1)]
        hands[0].append(name)
        setup = [card(name, "source", zone="hand")]
        if not stock:
            for p in (0, 1):
                setup += [card("Plains", f"kept_{p}", player=p, zone="hand")]
                setup += [card(n, f"library_{p}_{i}", player=p, zone="library") for i, n in enumerate(LIBRARY)]
                setup += [card(n, f"exile_{p}_{i}", player=p, zone="exile") for i, n in enumerate(("Island", "Llanowar Elves"))]
        for p in (0, 1):
            setup += [card(n, f"grave_{p}_{i}", player=p, zone="graveyard") for i, n in enumerate(graves[p][len(STOCK_GRAVE) if stock else 0:])]
        response = variant in ("lost_target", "shifted_target")
        if response:
            setup.append(card("Reclaim", "response", zone="hand")); hands[0].append("Reclaim")
        actions = [step("capture")] if stock else []
        if variant == "payment":
            actions += exact_mana(parameters["mana"])
        if variant == "insufficient_mana":
            actions += exact_mana([0]*6)
        effect = []
        self_exiled = not negative and not probe and variant != "lost_target" and parameters.get("self_destination") == "exile"
        if probe:
            effect += [step("can_cast", ref="source", value=1), step("assert_stack", value=0), step("assert_choices_consumed")]
        elif negative:
            effect += ([step("cast", ref="source", value=0)] if variant == "insufficient_mana" else [step("can_cast", ref="source", value=0)])
            effect += [step("assert_stack", value=0), observe("source", "in_hand", 1)]
        else:
            if parameters.get("graveyard") == "any":
                effect.append(step("choose_player", player=-1, value=owner))
            effect.append(step("choose_zone", player=-1, value=index))
            if response:
                effect += [step("choose_priority", ref="response"), step("choose_zone", player=-1, value=index if variant == "lost_target" else 0)]
                moved = graves[0].pop(index if variant == "lost_target" else 0)
                libraries[0].insert(0, moved)
                hands[0].remove("Reclaim"); graves[0].append("Reclaim")
            effect += [step("cast", ref="source", value=1), step("assert_stack", value=1), step("resolve"), step("assert_choices_consumed")]
            hands[0].remove(name)
            if variant != "lost_target":
                graves[owner].pop(index - int(variant == "shifted_target"))
                if parameters["destination"] == "hand":
                    hands[owner].append(target)
                elif parameters["destination"] == "library_top":
                    libraries[owner].insert(0, target)
            if not self_exiled:
                graves[0].append(name)
        returned = not negative and not probe and variant != "lost_target"
        bonus = returned and parameters.get("bonus_subtype", "NEVER") in oracle[target]["type_line"]
        draws = parameters.get("draw", 0) + (parameters.get("bonus_draw", 0) if bonus else 0) if returned else 0
        hands[0] += libraries[0][:draws]; del libraries[0][:draws]
        life = 20 + (parameters.get("gain_life", 0) - (int(oracle[target]["cmc"]) if parameters.get("lose_life_from_value") else 0) if returned else 0)
        reanimated = returned and parameters["destination"] == "battlefield"
        checks = [step("assert_stack", value=0), step("assert_life", player=0, value=life), step("assert_life", player=1, value=20)]
        if reanimated:
            checks += [step("bind_permanent", ref="returned", player=0, value=registry[target]["csvid"], index=-1), observe("returned", "tapped", 0)]
            if "Creature" in oracle[target]["type_line"]:
                counters = parameters.get("counters", 0) + (parameters.get("bonus_counters", 0) if bonus else 0)
                checks += [observe("returned", p, int(oracle[target][p])+counters) for p in ("power", "toughness")]
                checks += [observe("returned", "sick", 1), observe("returned", "damage", 0)]
            if variant.endswith("death_owner"):
                checks.append(step("destroy", ref="returned")); graves[owner].append(target); reanimated = False
        for p in (0, 1):
            for zone, names in (("hand", hands[p]), ("graveyard", graves[p]), ("library", libraries[p])):
                checks.append(step("assert_zone", player=p, zone=zone, value=len(names)))
                # Hands are multisets: freed active-card slots can alter display ordering.
                if zone == "hand":
                    checks += [step("assert_hand_graveyard_total", player=p, index=registry[n]["csvid"], value=names.count(n)+graves[p].count(n)) for n in dict.fromkeys(names)]
                else:
                    checks += [step("assert_zone_card", player=p, zone=zone, index=i, value=registry[n]["csvid"]) for i, n in enumerate(names)]
            checks += [step("assert_zone", player=p, zone="battlefield", value=int(reanimated and p == 0)),
                       step("assert_zone", player=p, zone="exile", value=2+int(p == 0 and self_exiled))]
            for i, n in enumerate(("Island", "Llanowar Elves")):
                checks.append(step("assert_zone_card", player=p, zone="exile", index=i, value=registry[n]["csvid"]))
        if self_exiled:
            checks.append(step("assert_zone_card", player=0, zone="exile", index=2, value=csvid))
        mana = 0 if variant in ("payment", "insufficient_mana") else 60 - (sum(parameters["mana"]) if not negative and not probe else 0) - (sum(cost_from_oracle(oracle["Reclaim"])) if response else 0)
        checks += [step("assert_mana_total", player=0, value=mana), step("assert_mana_total", player=1, value=60)]
        actions += effect+checks
        if stock:
            actions += [step("record_state"), step("ai_roundtrip"), step("restore")]+effect+checks+[step("assert_recorded_state")]
        cases.append(dict(name=f"RecoveryRecipe_{csvid}_{variant}", card=name, fixture=fixture, seed=123,
                          setup=setup, steps=actions, _recipe=dict(family="recover_card", version=2,
                          variant=variant, params=declaration["params"], oracle_text=declaration["oracle_text"])))
    return cases
