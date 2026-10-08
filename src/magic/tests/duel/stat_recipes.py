"""Oracle-authored reductions, lethal toughness and cleanup scenarios."""


def spec(parameters, mana):
    required = {"power", "toughness"}
    optional = {"draw", "gain_life", "lose_life", "tap"}
    if (not isinstance(parameters, dict) or not required <= set(parameters)
            or set(parameters) - required - optional
            or any(type(parameters[k]) is not int or not -10 <= parameters[k] <= 10 for k in required)
            or not any(parameters[k] < 0 for k in required)):
        raise ValueError("reductions require reviewed signed power/toughness integers")
    for k in ("draw", "gain_life", "lose_life"):
        if k in parameters and (type(parameters[k]) is not int or not 1 <= parameters[k] <= (3 if k == "draw" else 20)):
            raise ValueError("invalid reduction secondary amount")
    if "tap" in parameters and parameters["tap"] is not True:
        raise ValueError("tap requires explicit true")
    return dict(family="reduce_creature", mana=mana, **parameters)


def expand(name, csvid, declaration, parameters, variants, registry):
    from recipes import card, step, observe, exact_mana
    cases = []
    for variant in variants:
        if variant == "lethal" and parameters["toughness"] >= 0:
            raise ValueError("lethal requires a negative toughness adjustment")
        if variant == "negative_power" and parameters["power"] >= 0:
            raise ValueError("negative_power requires a negative power adjustment")
        fixture = "stocked_zones" if variant == "snapshot" else "empty_main"
        initial_hand, initial_grave = (4, 4) if fixture == "stocked_zones" else (1, 0)
        order = ["Forest", "Plains", "Grizzly Bears", "Forest", "Mountain", "Island", "Lightning Bolt"] if fixture == "stocked_zones" else ["Plains", "Lightning Bolt", "Forest"]
        owner = int(variant != "friendly_cleanup")
        victim, power, toughness = (("Llanowar Elves", 1, 1) if variant == "lethal" else
                                   ("Wall of Stone", 0, 8) if variant == "negative_power" else ("Scaled Wurm", 7, 6))
        if toughness + parameters["toughness"] <= 0 and variant != "lethal":
            raise ValueError("survivor witness would die; author a different scenario")
        setup = [card(name, "source", zone="hand")] + declaration.get("setup", [])
        if fixture == "empty_main":
            for p in (0, 1):
                setup.append(card("Hill Giant", f"kept_{p}", player=p, zone="hand"))
                setup.extend(card(n, f"library_{p}_{i}", player=p, zone="library") for i, n in enumerate(order))
        if variant != "empty":
            recipient = card("Sol Ring" if variant == "wrong_target" else victim, "recipient", player=owner)
            recipient["state"] = ["untapped"]
            setup.append(recipient)
        negative = variant in ("empty", "wrong_target", "insufficient_mana")
        if not negative: setup.append(card("Hill Giant", "unselected", player=owner))
        hand, grave, life, drawn = [initial_hand]*2, [initial_grave]*2, [20,20], 0
        hand[0] += 1
        if negative:
            actions = (exact_mana([0]*6) + [step("cast", ref="source", value=0), step("assert_stack", value=0)]
                       if variant == "insufficient_mana" else [step("can_cast", ref="source", value=0)])
            actions += [observe("source", "in_hand", 1), step("assert_mana_total", player=0, value=0 if variant == "insufficient_mana" else 60)]
            if variant != "empty": actions += [observe("recipient", "in_play", 1)]
        else:
            effect = [step("choose_target", ref="recipient", player=0), step("cast", ref="source", value=1), step("assert_stack", value=1),
                      observe("recipient", "in_play", 1), observe("recipient", "power", power), observe("recipient", "toughness", toughness),
                      observe("recipient", "tapped", 0), step("assert_life", player=0, value=20), step("assert_life", player=1, value=20)]
            if variant == "lost_target": effect.append(step("destroy", ref="recipient"))
            else: effect += [step("choose_dialog", player=0, value=n) for n in declaration.get("resolve_dialogs", [])]
            effect += [step("resolve"), step("assert_choices_consumed")]
            hand[0] -= 1; grave[0] += 1
            if variant in ("lethal", "lost_target"):
                grave[owner] += 1
                effect += [observe("recipient", "in_play", 0), step("assert_zone_contains", player=owner, zone="graveyard", value=registry[victim]["csvid"])]
            else:
                effect += [observe("recipient", "in_play", 1), observe("recipient", "power", power + parameters["power"]),
                           observe("recipient", "toughness", toughness + parameters["toughness"]), observe("recipient", "damage", 0),
                           observe("recipient", "tapped", int(parameters.get("tap",False)))]
            if variant != "lost_target":
                drawn = parameters.get("draw", 0); hand[0] += drawn
                life[0] += parameters.get("gain_life",0) - parameters.get("lose_life",0)
            effect += [observe("unselected", "in_play", 1), observe("unselected", "power", 3), observe("unselected", "toughness", 3),
                       step("assert_zone_contains", player=0, zone="graveyard", value=csvid)]
            if variant.endswith("cleanup"):
                effect += [step("cleanup"), observe("recipient", "power", power), observe("recipient", "toughness", toughness),
                           observe("recipient", "tapped", int(parameters.get("tap",False)))]
            actions = (exact_mana(parameters["mana"]) if variant == "payment" else []) + effect
            actions += ([step("assert_mana", player=0, color=i, value=0) for i in range(6)] if variant == "payment"
                        else [step("assert_mana_total", player=0, value=60-sum(parameters["mana"]))])
        checks = []
        for p in (0,1):
            library = order[drawn:] if p == 0 else order
            checks += [step("assert_zone", player=p, zone="hand", value=hand[p]), step("assert_zone", player=p, zone="graveyard", value=grave[p]),
                       step("assert_zone", player=p, zone="library", value=len(library)), step("assert_zone", player=p, zone="exile", value=2 if fixture=="stocked_zones" else 0),
                       step("assert_life", player=p, value=life[p])]
            checks += [step("assert_zone_card", player=p, zone="library", index=i, value=registry[n]["csvid"]) for i,n in enumerate(library)]
            if fixture=="empty_main": checks.append(step("assert_zone_contains", player=p, zone="hand", value=registry["Hill Giant"]["csvid"]))
        checks += [step("assert_zone_contains", player=0, zone="hand", value=registry[n]["csvid"]) for n in order[:drawn]]
        actions += checks
        if variant == "snapshot":
            actions = [step("capture")] + actions + [step("record_state"), step("ai_roundtrip"), step("restore")] + effect + checks + [step("assert_recorded_state")]
        cases.append(dict(name=f"Recipe_{csvid}_reduce_creature_{variant}",card=name,fixture=fixture,setup=setup,steps=actions,seed=123,
                          _recipe=dict(family="reduce_creature",version=2,variant=variant,params=declaration["params"],oracle_text=declaration["oracle_text"],
                                       **{k:declaration[k] for k in ("setup","resolve_dialogs") if declaration.get(k)})))
    return cases
