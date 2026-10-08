"""Explicit Oracle-reviewed permanent movement; no implementation-derived rules."""
from destruction_recipes import WITNESSES

DESTINATIONS = ("hand", "library_top", "library_bottom", "library_second")
ORDER = ["Plains", "Lightning Bolt", "Forest"]
STOCKED_ORDER = ["Forest", "Plains", "Grizzly Bears", "Forest", "Mountain", "Island", "Lightning Bolt"]


def spec(parameters, mana):
    required = {"targets", "destination"}
    optional = {"controller", "draw", "gain_life", "target_gain_life", "target_lose_life", "target_tapped"}
    if not isinstance(parameters, dict) or not required <= set(parameters) or set(parameters) - required - optional:
        raise ValueError("unknown or missing movement parameters")
    targets = parameters["targets"]
    if (not isinstance(targets, list) or not targets
            or any(not isinstance(t, str) or t not in WITNESSES for t in targets)
            or len(set(targets)) != len(targets)):
        raise ValueError("movement targets require unique reviewed permanent types")
    if parameters["destination"] not in DESTINATIONS or parameters.get("controller", "any") not in ("any", "self", "opponent"):
        raise ValueError("unsupported movement destination or controller")
    for field in ("draw", "gain_life", "target_gain_life", "target_lose_life"):
        if field in parameters and (type(parameters[field]) is not int or not 1 <= parameters[field] <= (4 if field == "draw" else 20)):
            raise ValueError("movement amounts must be reviewed positive integers")
    if "target_tapped" in parameters and parameters["target_tapped"] is not True:
        raise ValueError("target_tapped is an explicit true restriction")
    if parameters.get("draw") and parameters["destination"] != "hand":
        raise ValueError("combined library movement and draw needs authored cases")
    return dict(family="move_permanent", mana=mana, **parameters)


def expand(name, csvid, declaration, parameters, variants, registry):
    from recipes import card, step, observe, exact_mana
    targets = parameters["targets"]
    controller = parameters.get("controller", "any")
    destination = parameters["destination"]
    cases = []
    for variant in variants:
        if variant == "friendly" and controller == "opponent" or variant == "opponent" and controller == "self":
            raise ValueError("positive movement variant violates controller restriction")
        if variant == "wrong_controller" and controller == "any":
            raise ValueError("wrong_controller requires a controller restriction")
        if variant == "wrong_state" and not parameters.get("target_tapped"):
            raise ValueError("wrong_state requires a tapped target restriction")
        if variant == "untapped" and parameters.get("target_tapped"):
            raise ValueError("untapped positive variant violates target restriction")
        fixture = "stocked_zones" if variant == "snapshot" else "empty_main"
        initial_hand, initial_grave = (4, 4) if fixture == "stocked_zones" else (1, 0)
        libraries = [list(STOCKED_ORDER if fixture == "stocked_zones" else ORDER) for _ in range(2)]
        owner = 0 if variant == "friendly" or controller == "self" else 1
        if variant == "wrong_controller": owner = 1 - owner
        category = variant.removeprefix("target_") if variant.startswith("target_") else targets[0]
        victim = WITNESSES[category]
        setup = [card(name, "source", zone="hand")] + declaration.get("setup", [])
        if fixture == "empty_main":
            for player in (0, 1):
                setup.append(card("Hill Giant", f"kept_{player}", player=player, zone="hand"))
                setup.extend(card(n, f"library_{player}_{i}", player=player, zone="library") for i, n in enumerate(ORDER))
        if variant == "wrong_target":
            complement = next((t for t in WITNESSES if t not in targets), None)
            if complement is None: raise ValueError("wrong_target requires an excluded permanent type")
            victim = WITNESSES[complement]
        if variant != "empty":
            recipient = card(victim, "recipient", player=owner)
            recipient["state"] = ["untapped" if variant in ("untapped", "wrong_state") else "tapped"]
            setup.append(recipient)
        negative = variant in ("empty", "wrong_target", "wrong_controller", "wrong_state", "insufficient_mana")
        if not negative:
            alternate = {"artifact": "Gilded Lotus", "enchantment": "Honor of the Pure", "creature": "Hill Giant", "land": "Plains"}[category]
            other = card(alternate, "unselected", player=owner); other["state"] = ["tapped"]
            setup.append(other)
        hand, grave, life = [initial_hand]*2, [initial_grave]*2, [20, 20]
        hand[0] += 1
        effect = []
        if negative:
            effect = (exact_mana([0]*6) + [step("cast", ref="source", value=0), step("assert_stack", value=0)]
                      if variant == "insufficient_mana" else [step("can_cast", ref="source", value=0)])
            effect += [observe("source", "in_hand", 1), step("assert_mana_total", player=0, value=0 if variant == "insufficient_mana" else 60)]
            if variant != "empty": effect += [observe("recipient", "in_play", 1)]
        else:
            effect = [step("choose_dialog", player=0, value=n) for n in declaration.get("cast_dialogs", {}).get(variant, [])]
            effect += [step("choose_target", ref="recipient", player=0), step("cast", ref="source", value=1), step("assert_stack", value=1)]
            # Moving a target and the spell's secondary effects occur on resolution.
            effect += [observe("recipient", "in_play", 1),
                       step("assert_life", player=0, value=20), step("assert_life", player=1, value=20)]
            effect += [step("assert_zone", player=p, zone="library", value=len(libraries[p])) for p in (0, 1)]
            if variant == "lost_target": effect += [step("destroy", ref="recipient")]
            else: effect += [step("choose_dialog", player=0, value=n) for n in declaration.get("resolve_dialogs", [])]
            effect += [step("resolve"), step("assert_choices_consumed")]
            hand[0] -= 1; grave[0] += 1
            if variant == "lost_target": grave[owner] += 1
            else:
                if destination == "hand": hand[owner] += 1
                else:
                    index = {"library_top": 0, "library_bottom": len(libraries[owner]), "library_second": 1}[destination]
                    libraries[owner].insert(index, victim)
                draw = parameters.get("draw", 0)
                hand[0] += draw; libraries[0] = libraries[0][draw:]
                life[0] += parameters.get("gain_life", 0)
                life[owner] += parameters.get("target_gain_life", 0) - parameters.get("target_lose_life", 0)
            effect += [observe("recipient", "in_play", 0), observe("unselected", "in_play", 1),
                       step("assert_zone_contains", player=0, zone="graveyard", value=csvid)]
            if variant != "lost_target":
                effect += [step("assert_zone_contains", player=owner, zone="hand" if destination == "hand" else "library", value=registry[victim]["csvid"])]
                if parameters.get("draw"):
                    original = STOCKED_ORDER if fixture == "stocked_zones" else ORDER
                    effect += [step("assert_zone_contains", player=0, zone="hand", value=registry[n]["csvid"]) for n in original[:parameters["draw"]]]
        checks = []
        for player in (0, 1):
            checks += [step("assert_zone", player=player, zone="hand", value=hand[player]),
                       step("assert_zone", player=player, zone="graveyard", value=grave[player]),
                       step("assert_zone", player=player, zone="library", value=len(libraries[player])),
                       step("assert_zone", player=player, zone="exile", value=2 if fixture == "stocked_zones" else 0),
                       step("assert_life", player=player, value=life[player])]
            checks += [step("assert_zone_card", player=player, zone="library", index=i, value=registry[n]["csvid"]) for i, n in enumerate(libraries[player])]
            if fixture == "empty_main": checks += [step("assert_zone_contains", player=player, zone="hand", value=registry["Hill Giant"]["csvid"])]
        if not negative:
            payment = (exact_mana(parameters["mana"]) if variant == "payment" else [])
            mana_checks = ([step("assert_mana", player=0, color=i, value=0) for i in range(6)] if variant == "payment"
                           else [step("assert_mana_total", player=0, value=60-sum(parameters["mana"]))])
            actions = payment + effect + checks + mana_checks
            if variant == "snapshot":
                actions = [step("capture")] + actions + [step("record_state"), step("ai_roundtrip"), step("restore")] + effect + checks + mana_checks + [step("assert_recorded_state")]
        else: actions = effect + checks
        cases.append(dict(name=f"Recipe_{csvid}_move_permanent_{variant}", card=name, fixture=fixture, setup=setup, steps=actions, seed=123,
                          _recipe=dict(family="move_permanent", version=2, variant=variant, params=declaration["params"], oracle_text=declaration["oracle_text"],
                                       **{k: declaration[k] for k in ("setup", "resolve_dialogs", "cast_dialogs") if declaration.get(k)})))
    return cases
