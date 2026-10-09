"""Reviewed, deliberately narrow Oracle-to-behavior recipes. No engine handlers.

Versioned declarations expand at build time; unsupported text fails closed.
The registry supplies identity only. Costs come from Oracle; explicit parameters
can describe a reviewed component of a more complex spell.
"""
import re
from collections import Counter

VERSION = 1
COLORS = {"B": 1, "U": 2, "G": 3, "R": 4, "W": 5}
WORDS = {"a": 1, "two": 2, "three": 3, "four": 4}
VARIANTS = {
    "creature_keywords": ("cast", "payment", "insufficient_mana", "snapshot", "fresh_attack", "unblocked",
                          "block_ground", "block_flying", "tapped_attacker", "tapped_blocker"),
    "reduce_creature": ("friendly_cleanup", "opponent_cleanup", "negative_power", "lethal", "lost_target", "empty", "wrong_target", "payment", "insufficient_mana", "snapshot"),
    "move_permanent": ("opponent", "friendly", "untapped", "lost_target", "empty", "wrong_target", "payment", "insufficient_mana", "snapshot"),
    "destroy_permanent": ("opponent", "friendly", "lost_target", "empty", "wrong_target", "payment", "insufficient_mana", "snapshot"),
    "draw_cards": ("ordered", "payment", "insufficient_mana", "snapshot"),
    "gain_life": ("gain", "injured", "payment", "insufficient_mana", "snapshot"),
    "pump_creature": ("friendly_cleanup", "opponent", "no_creature", "payment", "insufficient_mana", "snapshot"),
    "damage_any_target": ("survivor", "lethal", "opponent", "self", "lost_target", "payment", "insufficient_mana", "snapshot"),
}


def validate_context(oracle, registry, fixtures):
    """Fail closed if a pinned witness or shared board no longer fits a recipe."""
    for name, category in {"Sol Ring":"Artifact", "Gilded Lotus":"Artifact", "Glorious Anthem":"Enchantment",
                           "Honor of the Pure":"Enchantment", "Forest":"Land", "Plains":"Land"}.items():
        if name not in registry or category not in oracle.get(name,{}).get("type_line",""):
            raise ValueError(f"destruction witness no longer has reviewed Oracle type: {name}")
    for name, pt in {"Grizzly Bears": ("2", "2"), "Hill Giant": ("3", "3"),
                     "Shivan Dragon": ("5", "5"), "Llanowar Elves": ("1", "1"),
                     "Scaled Wurm": ("7", "6"), "Wall of Stone": ("0", "8"),
                     "Drudge Skeletons": ("1", "1"), "Erhnam Djinn": ("4", "5"),
                     "Suntail Hawk": ("1", "1"), "Giant Spider": ("2", "4"),
                     "Ornithopter": ("0", "2")}.items():
        record = oracle.get(name, {})
        if name not in registry or (record.get("power"), record.get("toughness")) != pt:
            raise ValueError(f"recipe witness no longer has reviewed Oracle characteristics: {name}")
    for name, colors, keyword in (("Grizzly Bears", ["G"], ""),
                                  ("Hill Giant", ["R"], ""),
                                  ("Shivan Dragon", ["R"], "Flying"),
                                  ("Wall of Stone", ["R"], "Defender"),
                                  ("Drudge Skeletons", ["B"], ""),
                                  ("Erhnam Djinn", ["G"], "")):
        record = oracle.get(name, {})
        if "Creature" not in record.get("type_line", "") or record.get("colors") != colors or keyword not in record.get("oracle_text", ""):
            raise ValueError(f"destruction predicate witness changed: {name}")
    for name, keyword in (("Suntail Hawk", "Flying"), ("Giant Spider", "Reach"), ("Ornithopter", "Flying")):
        if oracle[name].get("keywords") != [keyword] or "Creature" not in oracle[name].get("type_line", ""):
            raise ValueError(f"creature combat witness changed: {name}")
    if oracle.get("Lightning Bolt", {}).get("oracle_text") != "Lightning Bolt deals 3 damage to any target.":
        raise ValueError("recipe injury witness no longer matches reviewed Oracle")
    for name in ("empty_main", "stocked_zones"):
        f = fixtures[name]
        if f.get("life") != [20, 20] or f.get("mana") != [[10] * 6] * 2 or f.get("turn") != 0 or f.get("combat", False):
            raise ValueError(f"recipe fixture assumptions changed: {name}")
        if name == "empty_main" and f["cards"]:
            raise ValueError("recipe empty_main must be empty")
        if name == "stocked_zones":
            zones = {"hand": ["Grizzly Bears", "Giant Growth", "Lightning Bolt", "Plains"],
                     "library": ["Forest", "Plains", "Grizzly Bears", "Forest", "Mountain", "Island", "Lightning Bolt"],
                     "graveyard": ["Grizzly Bears", "Giant Growth", "Forest", "Sol Ring"],
                     "exile": ["Island", "Llanowar Elves"]}
            for player in (0, 1):
                for zone, expected in zones.items():
                    if [c["name"] for c in f["cards"] if c["player"] == player and c["zone"] == zone] != expected:
                        raise ValueError(f"recipe stocked_zones assumptions changed: player {player} {zone}")
            if any(c["zone"] == "battlefield" for c in f["cards"]):
                raise ValueError("recipe stocked_zones must not have battlefield permanents")


def cost_from_oracle(record):
    cost = record.get("mana_cost", "")
    symbols = re.findall(r"\{(\d+|[BUGRW])\}", cost)
    if not symbols or "".join("{" + s + "}" for s in symbols) != cost:
        raise ValueError("mana cost requires an unreviewed payment recipe")
    pools = [0] * 6
    for symbol in symbols:
        pools[0 if symbol.isdigit() else COLORS[symbol]] += int(symbol) if symbol.isdigit() else 1
    if not 1 <= sum(pools) <= 10:
        raise ValueError("mana cost exceeds reviewed fixture range")
    return pools


def classify(name, oracle):
    """Return supported parameters or a precise reason for manual authoring."""
    if not oracle:
        return None, "missing Oracle mapping"
    if oracle.get("selected_face") or oracle.get("card_faces") or oracle.get("layout", "normal") != "normal":
        return None, "multi-face or alternate-layout record"
    if oracle.get("type_line") not in ("Instant", "Sorcery") or oracle.get("keywords"):
        return None, "type or keywords outside reviewed spell families"
    try:
        mana = cost_from_oracle(oracle)
    except ValueError as error:
        return None, str(error)
    text = oracle.get("oracle_text", "")
    match = re.fullmatch(r"Draw (a|two|three|four) cards?\.", text)
    if match:
        # Keep singular/plural grammar strict rather than normalizing text.
        n = WORDS[match[1]]
        if text != f"Draw {match[1]} {'card' if n == 1 else 'cards'}.":
            return None, "unreviewed draw wording"
        return dict(family="draw_cards", amount=n, mana=mana), None
    match = re.fullmatch(r"You gain (\d+) life\.", text)
    if match and 1 <= int(match[1]) <= 20:
        return dict(family="gain_life", amount=int(match[1]), mana=mana), None
    match = re.fullmatch(r"Target creature gets \+(\d+)/\+(\d+) until end of turn\.", text)
    if match and all(0 <= int(x) <= 10 for x in match.groups()) and sum(map(int, match.groups())) > 0:
        return dict(family="pump_creature", power=int(match[1]), toughness=int(match[2]), mana=mana), None
    # Escape the *Oracle* name, including punctuation. No substring matching.
    match = re.fullmatch(re.escape(oracle.get("name", name)) + r" deals (\d+) damage to any target\.", text)
    if match and 1 <= int(match[1]) <= 4:
        return dict(family="damage_any_target", amount=int(match[1]), mana=mana), None
    return None, "Oracle text or amount outside reviewed wording patterns"


def step(op, **values):
    return dict(op=op, **values)


def card(name, ref, player=0, zone="battlefield"):
    return dict(name=name, ref=ref, player=player, zone=zone)


def observe(ref, property, value):
    return step("assert_card", ref=ref, property=property, value=value)


def cast(ref="source", target=None, recipient=None):
    """Queue choices before the real cast, then drain the real stack."""
    choices = ([step("choose_target", ref=target, player=0)] if target else
               [step("choose_player", player=0, value=recipient)] if recipient is not None else [])
    return choices + [step("cast", ref=ref, value=1), step("resolve"), step("assert_choices_consumed")]


def exact_mana(pools):
    return [step("set_mana", player=0, color=i, value=n) for i, n in enumerate(pools)]


def explicit_spec(declaration, oracle):
    """Validate author-reviewed parameters without parsing Oracle prose.

    The full text pin forces re-review on Oracle refresh; it is not proof that
    the author's parameters or selected scenarios accurately describe the card.
    """
    family = declaration.get("family")
    if family not in VARIANTS:
        raise ValueError("unknown explicit recipe family")
    if family == "creature_keywords":
        from creature_recipes import spec
        return spec(declaration, oracle)
    if (not oracle or oracle.get("selected_face") or oracle.get("card_faces")
            or oracle.get("layout", "normal") != "normal"
            or oracle.get("type_line") not in ("Instant", "Sorcery")):
        raise ValueError("explicit recipe requires a normal single-face Instant or Sorcery")
    text = declaration.get("oracle_text")
    if not isinstance(text, str) or not text or text != oracle.get("oracle_text"):
        raise ValueError("explicit recipe Oracle text changed or is missing; review the declaration")
    parameters = declaration.get("params")
    if family == "reduce_creature":
        from stat_recipes import spec
        return spec(parameters, cost_from_oracle(oracle))
    if family == "move_permanent":
        from movement_recipes import spec
        return spec(parameters, cost_from_oracle(oracle))
    if family == "destroy_permanent":
        from destruction_recipes import spec
        return spec(parameters, cost_from_oracle(oracle))
    limits = ({"power": (0, 10), "toughness": (0, 10)} if family == "pump_creature" else
              {"amount": (1, {"draw_cards": 4, "gain_life": 20, "damage_any_target": 4}[family])})
    required = set(limits)
    optional = ({"draw": (1, 4)} if family == "gain_life" else
                {"draw": (1, 4), "gain_life": (1, 20), "self_damage": (1, 4), "lose_life": (1, 10)}
                if family in ("pump_creature", "damage_any_target") else {})
    limits.update(optional)
    if (not isinstance(parameters, dict) or not required <= set(parameters) or set(parameters) - set(limits)
            or any(type(parameters[key]) is not int or not low <= parameters[key] <= high
                   for key, (low, high) in limits.items() if key in parameters)
            or family == "pump_creature" and parameters["power"] + parameters["toughness"] == 0):
        raise ValueError("explicit recipe parameters are missing, unknown, or outside reviewed ranges")
    if "variants" not in declaration:
        raise ValueError("explicit recipe requires reviewed variants for this card's additional effects")
    return dict(family=family, mana=cost_from_oracle(oracle), **parameters)


def expand(card_name, csvid, declaration, oracle, registry):
    if not isinstance(declaration, dict):
        raise ValueError("recipe declaration must be an object")
    version = declaration.get("version")
    if type(version) is not int or version not in (VERSION, 2):
        raise ValueError("recipe requires explicit supported version")
    allowed = {"family", "version", "variants"} | ({"params", "oracle_text", "resolve_dialogs", "resolve_zones", "cast_dialogs", "setup"} if version == 2 else set())
    if set(declaration) - allowed:
        raise ValueError("unknown recipe declaration fields")
    dialogs = declaration.get("resolve_dialogs", [])
    if (not isinstance(dialogs, list) or any(type(n) is not int or not 0 <= n <= 32 for n in dialogs)):
        raise ValueError("resolve_dialogs must contain reviewed zero-based dialog choices")
    support = declaration.get("setup", [])
    if (not isinstance(support,list) or any(not isinstance(c,dict) or set(c)-{"name","ref","player","zone","state"}
            or not {"name","ref","player","zone"} <= set(c)
            or c["name"] not in registry or not isinstance(c["ref"],str) or not c["ref"].startswith("support_")
            or type(c["player"]) is not int or c["player"] not in (0,1) or c["zone"]!="battlefield"
            or c.get("state",[]) not in ([],["tapped"]) for c in support)
            or len({c["ref"] for c in support}) != len(support)):
        raise ValueError("recipe setup requires unique support_ refs for reviewed battlefield cards")
    if version == 2:
        spec = explicit_spec(declaration, oracle)
    else:
        spec, reason = classify(card_name, oracle)
        if not spec or declaration.get("family") != spec["family"]:
            raise ValueError(f"recipe does not match pinned Oracle: {reason or 'family mismatch'}")
    family = spec["family"]
    variants = declaration.get("variants", list(VARIANTS[family]))
    supported_variants = set(VARIANTS[family]) | ({"lost_target"} if version == 2 and family == "pump_creature" else set())
    if family == "creature_keywords":
        from creature_recipes import variants as creature_variants
        supported_variants = set(creature_variants(spec))
    if family == "destroy_permanent": supported_variants |= {"target_"+t for t in spec["targets"]}
    if family == "move_permanent": supported_variants |= {"target_"+t for t in spec["targets"]} | {"wrong_controller", "wrong_state"}
    if (not isinstance(variants, list) or not variants or any(v not in supported_variants for v in variants)
            or len(set(variants)) != len(variants)):
        raise ValueError("unknown, empty, or duplicate recipe variants")
    resolve_zones = declaration.get("resolve_zones", {})
    if (not isinstance(resolve_zones, dict)
            or any(v not in variants or v == "lost_target" or not isinstance(choices, list)
                   or not choices or len(choices) > 32
                   or any(type(n) is not int or not -1 <= n <= 499 for n in choices)
                   for v, choices in resolve_zones.items())):
        raise ValueError("resolve_zones requires reviewed resolution variants and zone choices")

    def with_zone_choices(cases):
        for case in cases:
            variant = case["_recipe"]["variant"]
            choices = resolve_zones.get(variant, [])
            if not choices:
                continue
            steps = []
            pending_source = False
            source_resolutions = 0
            for action in case["steps"]:
                if action["op"] == "cast":
                    pending_source = action.get("ref", "source") == "source"
                if action["op"] == "resolve":
                    if pending_source:
                        steps += [step("choose_zone", player=0, value=n) for n in choices]
                        source_resolutions += 1
                    pending_source = False
                steps.append(action)
            if not source_resolutions:
                raise ValueError("resolve_zones variant does not resolve a spell")
            case["steps"] = steps
            case["_recipe"]["resolve_zones"] = choices
        return cases
    cast_dialogs = declaration.get("cast_dialogs", {})
    if (not isinstance(cast_dialogs, dict)
            or any(v not in variants or not isinstance(choices, list)
                   or any(type(n) is not int or not 0 <= n <= 32 for n in choices) for v, choices in cast_dialogs.items())
            or cast_dialogs and family != "move_permanent"):
        raise ValueError("cast_dialogs requires reviewed movement variants and zero-based choices")

    def identity(name):
        if name not in registry:
            raise ValueError(f"recipe witness missing from registry: {name}")
        return registry[name]["csvid"]

    if family == "creature_keywords":
        if dialogs or support or resolve_zones:
            raise ValueError("creature recipes require dedicated cases for custom decisions/setup")
        from creature_recipes import expand as creatures
        return creatures(card_name, csvid, declaration, spec, variants, registry)

    if family == "move_permanent":
        from movement_recipes import expand as movement
        return with_zone_choices(movement(card_name, csvid, declaration, spec, variants, registry))
    if family == "reduce_creature":
        from stat_recipes import expand as reductions
        return with_zone_choices(reductions(card_name, csvid, declaration, spec, variants, registry))
    if family == "destroy_permanent":
        if dialogs or support: raise ValueError("destruction custom setup/dialogs need dedicated authored cases")
        from destruction_recipes import expand as destruction
        return with_zone_choices(destruction(card_name, csvid, declaration, spec, variants, registry))

    # Witness characteristics are reviewed separately in test_recipes.py.
    source = card(card_name, "source", zone="hand")
    amount = spec.get("amount", 0)
    mana = spec["mana"]
    total = sum(mana)
    cases = []
    for variant in variants:
        setup = [source] + list(support)
        fixture = "stocked_zones" if variant == "snapshot" else "empty_main"
        target, recipient = None, None
        before, effect, assertions = [], [], []
        hand = 4 if fixture == "stocked_zones" else 0
        grave = 4 if fixture == "stocked_zones" else 0
        extra_draw = spec.get("draw", 0)
        life_change = (spec.get("gain_life", 0) - spec.get("self_damage", 0) - spec.get("lose_life", 0)
                       if variant != "lost_target" else 0)
        if family == "draw_cards":
            ordered = ["Forest", "Plains", "Grizzly Bears", "Forest", "Mountain", "Island", "Lightning Bolt"]
            if fixture == "empty_main":
                setup += [card(n, f"library_{i}", zone="library") for i, n in enumerate(ordered)]
                setup += [card("Hill Giant", "kept", zone="hand")]
                hand = 1
            assertions += [step("assert_zone", player=0, zone="hand", value=hand + amount),
                           step("assert_zone", player=0, zone="library", value=7 - amount),
                           step("assert_zone_card", player=0, zone="library", index=0, value=identity(ordered[amount]))]
            for n in dict.fromkeys(ordered[:amount]):
                assertions.append(step("assert_zone_contains", player=0, zone="hand", value=identity(n)))
                initial = (Counter(["Grizzly Bears", "Giant Growth", "Lightning Bolt", "Plains",
                                    "Grizzly Bears", "Giant Growth", "Forest", "Sol Ring"])[n]
                           if fixture == "stocked_zones" else 0)
                assertions.append(step("assert_hand_graveyard_total", player=0, index=identity(n),
                                       value=initial + ordered[:amount].count(n)))
            if fixture == "empty_main":
                assertions.append(step("assert_zone_contains", player=0, zone="hand", value=identity("Hill Giant")))
        elif family == "gain_life":
            life = 20
            if variant == "injured":
                setup.append(card("Lightning Bolt", "injury", zone="hand"))
                before = cast("injury", recipient=0) + [step("assert_life", player=0, value=17)]
                life = 17
                grave += 1
            assertions += [step("assert_life", player=0, value=life + amount), step("assert_life", player=1, value=20)]
        elif family == "pump_creature":
            if variant == "no_creature":
                setup += [card("Sol Ring", "artifact", player=1)]
                effect = [step("can_cast", ref="source", value=0), observe("source", "in_hand", 1),
                          observe("artifact", "in_play", 1), step("assert_mana_total", player=0, value=60)]
            else:
                target = "recipient"
                setup += [card("Grizzly Bears", target, player=int(variant == "opponent")),
                          card("Hill Giant", "unselected", player=1)]
                if variant == "lost_target":
                    effect = [step("choose_target", ref=target, player=0), step("cast", ref="source", value=1),
                              step("assert_stack", value=1), step("destroy", ref=target), step("resolve"),
                              step("assert_choices_consumed")]
                    assertions += [observe(target, "in_play", 0),
                                   step("assert_zone_contains", player=0, zone="graveyard", value=identity("Grizzly Bears"))]
                    grave += 1
                else:
                    assertions += [observe(target, "power", 2 + spec["power"]),
                                   observe(target, "toughness", 2 + spec["toughness"])]
                assertions += [observe("unselected", "power", 3), observe("unselected", "toughness", 3)]
                if variant == "friendly_cleanup":
                    assertions += [step("cleanup"), observe(target, "power", 2), observe(target, "toughness", 2)]
        else:
            if variant in ("opponent", "self"):
                recipient = int(variant == "opponent")
                assertions += [step("assert_life", player=recipient, value=20 - amount + (life_change if recipient == 0 else 0)),
                               step("assert_life", player=1 - recipient, value=20 + (life_change if recipient == 1 else 0))]
            else:
                target = "recipient"
                victim = "Llanowar Elves" if variant == "lethal" else "Shivan Dragon"
                setup += [card(victim, target, player=1), card("Hill Giant", "unselected", player=1)]
                if variant == "lost_target":
                    effect = [step("choose_target", ref=target, player=0), step("cast", ref="source", value=1),
                              step("assert_stack", value=1), step("destroy", ref=target), step("resolve"),
                              step("assert_choices_consumed")]
                if variant in ("lethal", "lost_target"):
                    assertions += [observe(target, "in_play", 0),
                                   step("assert_zone_contains", player=1, zone="graveyard", value=identity(victim))]
                else:
                    assertions += [observe(target, "in_play", 1), observe(target, "damage", amount)]
                assertions += [observe("unselected", "in_play", 1), observe("unselected", "damage", 0),
                               step("assert_life", player=1, value=20)]

        if any(k in spec for k in ("gain_life", "self_damage", "lose_life")) and recipient is None:
            assertions += [step("assert_life", player=0, value=20 + life_change)]
        if extra_draw:
            ordered = ["Forest", "Plains", "Grizzly Bears", "Forest", "Mountain", "Island", "Lightning Bolt"]
            if fixture == "empty_main":
                setup += [card(n, f"library_{i}", zone="library") for i, n in enumerate(ordered)]
            drawn = 0 if variant == "lost_target" else extra_draw
            assertions += [step("assert_zone", player=0, zone="hand", value=hand + drawn),
                           step("assert_zone", player=0, zone="library", value=7 - drawn),
                           step("assert_zone_card", player=0, zone="library", index=0, value=identity(ordered[drawn]))]
            for n in dict.fromkeys(ordered[:drawn]):
                initial = (Counter(["Grizzly Bears", "Giant Growth", "Lightning Bolt", "Plains",
                                    "Grizzly Bears", "Giant Growth", "Forest", "Sol Ring"])[n]
                           if fixture == "stocked_zones" else 0)
                assertions += [step("assert_zone_contains", player=0, zone="hand", value=identity(n)),
                               step("assert_hand_graveyard_total", player=0, index=identity(n),
                                    value=initial + ordered[:drawn].count(n))]

        if variant == "insufficient_mana":
            # can_cast queries targets, not affordability; cast exercises payment.
            actions = exact_mana([0] * 6) + [step("can_cast", ref="source", value=1),
                         step("cast", ref="source", value=0), observe("source", "in_hand", 1),
                         step("assert_mana_total", player=0, value=0), step("assert_stack", value=0)]
            if target:
                actions.append(observe(target, "damage" if family == "damage_any_target" else "power", 0 if family == "damage_any_target" else 2))
            if family == "gain_life":
                actions.append(step("assert_life", player=0, value=20))
        elif variant == "no_creature":
            actions = effect
        else:
            if not effect:
                effect = cast(target=target, recipient=recipient)
                # Choice positions describe presentation, never rules expectations.
                # A spell with an illegal sole target must not ask resolution choices.
                if dialogs:
                    index = next(i for i, s in enumerate(effect) if s["op"] == "resolve")
                    effect[index:index] = [step("choose_dialog", player=0, value=n) for n in dialogs]
            if variant == "payment":
                before += exact_mana(mana)
            actions = before + effect
            # Check payment before cleanup empties the mana pool.
            if variant in ("payment", "ordered", "gain", "survivor", "opponent", "self", "lethal"):
                if variant == "payment":
                    actions += [step("assert_mana", player=0, color=i, value=0) for i in range(6)]
                else:
                    actions += [step("assert_mana_total", player=0, value=60 - total)]
            actions += assertions + [step("assert_zone", player=0, zone="graveyard", value=grave + 1),
                                      step("assert_zone_contains", player=0, zone="graveyard", value=csvid)]
            if variant == "snapshot":
                # Caller-owned snapshot must survive intervening AI backup/restore.
                actions = [step("capture")] + actions + [step("record_state"), step("ai_roundtrip"), step("restore")] + effect + assertions + [step("assert_recorded_state")]
        if variant in ("insufficient_mana", "no_creature"):
            if any(k in spec for k in ("gain_life", "self_damage", "lose_life")):
                actions += [step("assert_life", player=0, value=20), step("assert_life", player=1, value=20)]
            if extra_draw:
                actions += [step("assert_zone", player=0, zone="library", value=7),
                            step("assert_zone", player=0, zone="hand", value=hand + 1)]
        cases.append(dict(name=f"Recipe_{csvid}_{family}_{variant}", card=card_name, fixture=fixture,
                          setup=setup, steps=actions, seed=123,
                          _recipe=dict(family=family, version=version, variant=variant,
                                       **({"params": declaration["params"], "oracle_text": declaration["oracle_text"]}
                                          if version == 2 else {}),
                                       **({"resolve_dialogs": dialogs} if dialogs else {}),
                                       **({"setup": support} if support else {}))))
    return with_zone_choices(cases)
