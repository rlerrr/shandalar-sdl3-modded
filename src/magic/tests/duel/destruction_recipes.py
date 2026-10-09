"""Reviewed destruction scenarios, authored from Oracle rather than handlers."""
WITNESSES = {"artifact": "Sol Ring", "enchantment": "Glorious Anthem",
             "creature": "Grizzly Bears", "land": "Forest"}
CREATURE_WITNESSES = {"Grizzly Bears", "Hill Giant", "Shivan Dragon", "Wall of Stone",
                      "Drudge Skeletons", "Erhnam Djinn"}


def spec(parameters, mana):
    optional = {"gain_life", "target_gain_life", "target_lose_life", "target_damage", "self_damage", "lose_life"}
    if not isinstance(parameters, dict) or set(parameters) - ({"targets", "creature_witness", "target_tapped"} | optional):
        raise ValueError("unknown destruction parameters")
    targets = parameters.get("targets")
    if (not isinstance(targets, list) or not targets or any(not isinstance(t,str) or t not in WITNESSES for t in targets)
            or len(set(targets)) != len(targets)):
        raise ValueError("destruction targets must be unique reviewed permanent types")
    if any(type(parameters[k]) is not int or not 1 <= parameters[k] <= 20 for k in optional & set(parameters)):
        raise ValueError("destruction life amounts must be reviewed integers from 1 to 20")
    if "creature_witness" in parameters and ("creature" not in targets
            or not isinstance(parameters["creature_witness"], str)
            or parameters["creature_witness"] not in CREATURE_WITNESSES):
        raise ValueError("destruction creature_witness must be a reviewed creature")
    if "target_tapped" in parameters and (type(parameters["target_tapped"]) is not bool
            or "creature" not in targets):
        raise ValueError("destruction target_tapped requires a creature and a boolean")
    return dict(family="destroy_permanent", mana=mana, **parameters)


def expand(name, csvid, declaration, parameters, variants, registry):
    from recipes import card, step, observe, cast, exact_mana
    targets = parameters["targets"]
    cases = []
    for variant in variants:
        fixture = "stocked_zones" if variant == "snapshot" else "empty_main"
        initial = 4 if fixture == "stocked_zones" else 0
        owner = int(variant != "friendly")
        category = variant.removeprefix("target_") if variant.startswith("target_") else targets[0]
        victim = WITNESSES[category]
        if category == "creature": victim = parameters.get("creature_witness", victim)
        setup = [card(name, "source", zone="hand")]
        actions, assertions = [], []
        negative = variant in ("empty", "wrong_target", "insufficient_mana")
        if variant == "wrong_target":
            complement = next((t for t in WITNESSES if t not in targets), None)
            if complement is None:
                raise ValueError("wrong_target requires a reviewed type outside declared scope")
            victim = WITNESSES[complement]
        if variant != "empty":
            setup += [card(victim, "recipient", player=owner)]
            if variant != "wrong_target" and parameters.get("target_tapped"):
                setup[-1]["state"] = ["tapped"]
        # A differently named permanent of the same broad type must survive.
        alternative = {"artifact": "Gilded Lotus", "enchantment": "Honor of the Pure",
                       "creature": "Hill Giant", "land": "Plains"}[category]
        if alternative == victim: alternative = "Grizzly Bears"
        if not negative:
            setup += [card(alternative, "unselected", player=owner)]
        if variant in ("empty", "wrong_target"):
            actions = [step("can_cast", ref="source", value=0), observe("source", "in_hand", 1),
                       step("assert_mana_total", player=0, value=60)]
            if variant == "wrong_target": actions += [observe("recipient", "in_play", 1)]
        elif variant == "insufficient_mana":
            actions = exact_mana([0]*6) + [step("cast", ref="source", value=0),
                       observe("source", "in_hand", 1), observe("recipient", "in_play", 1),
                       step("assert_stack", value=0), step("assert_mana_total", player=0, value=0)]
        else:
            effect = ([step("choose_target", ref="recipient", player=0), step("cast", ref="source", value=1),
                       step("assert_stack", value=1), step("destroy", ref="recipient"), step("resolve"),
                       step("assert_choices_consumed")] if variant == "lost_target" else cast(target="recipient"))
            # Positive scenarios must be affordable with exactly the printed cost.
            payment = exact_mana(parameters["mana"]) if variant == "payment" else []
            actions = payment + effect
            actions += ([step("assert_mana", player=0, color=i, value=0) for i in range(6)]
                        if variant == "payment" else [step("assert_mana_total", player=0, value=60-sum(parameters["mana"]))])
            assertions = [observe("recipient", "in_play", 0), observe("unselected", "in_play", 1),
                step("assert_zone_contains", player=owner, zone="graveyard", value=registry[victim]["csvid"]),
                step("assert_zone", player=owner, zone="graveyard", value=initial+1+int(owner==0)),
                step("assert_zone_contains", player=0, zone="graveyard", value=csvid)]
            if owner != 0: assertions += [step("assert_zone", player=0, zone="graveyard", value=initial+1)]
            actions += assertions
            if variant == "snapshot":
                actions = [step("capture")] + actions + [step("record_state"), step("ai_roundtrip"), step("restore")] + effect + assertions + [step("assert_recorded_state")]
        life = [20,20]
        if not negative and variant != "lost_target":
            life[0] += parameters.get("gain_life",0)-parameters.get("self_damage",0)-parameters.get("lose_life",0)
            life[owner] += parameters.get("target_gain_life",0)-parameters.get("target_lose_life",0)-parameters.get("target_damage",0)
        checks = [step("assert_life",player=p,value=n) for p,n in enumerate(life)]
        # Replay checks must precede state capture, not just inspect the last run.
        if variant == "snapshot":
            record = next(i for i,s in enumerate(actions) if s["op"]=="record_state")
            actions[record:record] = checks
            actions[-1:-1] = checks
        else: actions += checks
        cases.append(dict(name=f"Recipe_{csvid}_destroy_permanent_{variant}",card=name,fixture=fixture,
            setup=setup,steps=actions,seed=123,_recipe=dict(family="destroy_permanent",version=2,variant=variant,
                params=declaration["params"],oracle_text=declaration["oracle_text"])))
    return cases
