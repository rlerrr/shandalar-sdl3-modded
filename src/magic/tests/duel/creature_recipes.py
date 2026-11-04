"""Whole-text creature components, with rules expectations independent of handlers."""
import re

KEYWORDS = ("Flying", "First strike", "Double strike", "Deathtouch", "Haste", "Vigilance",
            "Trample", "Reach", "Lifelink", "Defender", "Menace", "Flash")
REMINDERS = {
    "(Any amount of damage this deals to a creature is enough to destroy it.)",
    "(Attacking doesn't cause this creature to tap.)",
    "(Damage dealt by this creature also causes you to gain that much life.)",
    "(This creature can attack and {T} as soon as it comes under your control.)",
    "(This creature can block creatures with flying.)",
    "(This creature can deal excess combat damage to the player or planeswalker it's attacking.)",
    "(This creature can't be blocked except by creatures with flying or reach, and attacking doesn't cause this creature to tap.)",
    "(This creature can't be blocked except by two or more creatures.)",
    "(This creature deals combat damage before creatures without first strike.)",
    "(This creature can't attack.)",
    "(This creature can't be blocked except by creatures with flying or reach.)",
    "(You may cast this spell any time you could cast an instant.)",
}
BASE = ("cast", "payment", "insufficient_mana", "snapshot", "fresh_attack", "unblocked",
        "block_ground", "block_flying", "tapped_attacker", "tapped_blocker")


def oracle_keywords(record):
    if (not record or record.get("layout", "normal") != "normal" or record.get("card_faces")
            or record.get("selected_face") or "Creature" not in record.get("type_line", "")):
        raise ValueError("creature recipe requires a normal single-face creature")
    text = record.get("oracle_text", "")
    for reminder in re.findall(r"\([^\n]*?\)", text):
        if reminder not in REMINDERS:
            raise ValueError("unreviewed creature reminder text")
        text = text.replace(reminder, "")
    # Every rules clause must be one of these unconditional keywords. Presence
    # in Oracle's keywords array alone does not imply an unconditional ability.
    tokens = [t.strip().lower() for t in re.split(r",|\n", text.strip())] if text.strip() else []
    canonical = {k.lower(): k for k in KEYWORDS}
    if any(t not in canonical for t in tokens) or len(set(tokens)) != len(tokens):
        raise ValueError("creature has other rules text or duplicate keyword clauses")
    keywords = sorted(canonical[t] for t in tokens)
    if set(keywords) != set(record.get("keywords", [])):
        raise ValueError("creature keyword text and structured Oracle keywords disagree")
    for field in ("power", "toughness"):
        if not re.fullmatch(r"\d+", record.get(field, "")) or not 0 <= int(record[field]) <= 20:
            raise ValueError("creature recipe needs bounded numeric Oracle characteristics")
    if int(record["toughness"]) == 0:
        raise ValueError("zero toughness needs a dedicated entry/death scenario")
    return keywords


def classify(record):
    from recipes import cost_from_oracle
    keywords = oracle_keywords(record)
    return dict(family="creature_keywords", keywords=keywords, mana=cost_from_oracle(record),
                power=int(record["power"]), toughness=int(record["toughness"]))


def spec(declaration, record):
    if declaration.get("oracle_text") != record.get("oracle_text", ""):
        raise ValueError("creature Oracle text changed; review the declaration")
    result = classify(record)
    params = declaration.get("params")
    if (not isinstance(params, dict) or set(params) != {"keywords"}
            or params["keywords"] != result["keywords"]):
        raise ValueError("creature keywords must explicitly match the reviewed whole-text specification")
    return result


def variants(spec):
    result = list(BASE)
    keys = spec["keywords"]
    if "Flying" in keys and "Menace" not in keys and "Defender" not in keys:
        result += ["flying_evasion", "flying_reach"]
    if spec["power"] > 0:
        if any(k in keys for k in ("First strike", "Double strike")): result += ["first_strike"]
        if "Deathtouch" in keys: result += ["deathtouch"]
        if "Trample" in keys and "Menace" not in keys and "Defender" not in keys: result += ["trample"]
    if "Menace" in keys and "Defender" not in keys: result += ["menace_single", "menace_pair"]
    return result


def expand(name, csvid, declaration, spec, selected, registry):
    from recipes import card, step, observe, exact_mana
    cases = []
    keys, power, toughness, mana = set(spec["keywords"]), spec["power"], spec["toughness"], spec["mana"]
    component=spec["family"] in ("passive_keyword_creature", "self_pump_creature", "evasion_creature", "defensive_creature", "mana_creature", "regenerate_creature", "etb_value_creature", "blocking_creature", "tap_damage_creature", "combat_keyword_creature")
    prefix={"self_pump_creature":"PumpCreatureRecipe", "evasion_creature":"EvasionCreatureRecipe", "defensive_creature":"DefensiveCreatureRecipe", "mana_creature":"ManaCreatureRecipe", "regenerate_creature":"RegenerateCreatureRecipe", "etb_value_creature":"EtbValueCreatureRecipe"}.get(spec["family"],"CreatureRecipe")
    if spec["family"] == "blocking_creature": prefix = "BlockingCreatureRecipe"
    if spec["family"] == "tap_damage_creature": prefix = "TapDamageCreatureRecipe"
    if spec["family"] == "combat_keyword_creature": prefix = "CombatKeywordCreatureRecipe"
    for variant in selected:
        fixture = "stocked_zones" if component or variant == "snapshot" else "empty_main"
        setup, actions = [], []
        casting = variant in ("cast", "payment", "insufficient_mana", "snapshot", "fresh_attack")
        setup.append(card(name, "source", zone="hand" if casting else "battlefield"))
        if casting:
            if variant in ("payment", "insufficient_mana"):
                actions += exact_mana(mana if variant == "payment" else [0]*6)
            if variant == "snapshot": actions += [step("capture")]
            run = [step("cast", ref="source", value=int(variant != "insufficient_mana"))]
            if variant != "insufficient_mana":
                run += [step("assert_stack", value=1), step("resolve"), step("assert_choices_consumed"),
                        observe("source", "in_play", 1), observe("source", "in_hand", 0),
                        observe("source", "power", power), observe("source", "toughness", toughness),
                        observe("source", "tapped", 0), step("assert_stack", value=0),
                        step("assert_zone", player=0, zone="hand", value=4 if fixture == "stocked_zones" else 0),
                        step("assert_zone", player=0, zone="graveyard", value=4 if fixture == "stocked_zones" else 0)]
            else:
                run += [observe("source", "in_hand", 1), observe("source", "in_play", 0), step("assert_stack", value=0)]
            for player in (0, 1):
                run += [step("assert_life", player=player, value=20),
                        step("assert_zone", player=player, zone="library", value=7 if fixture == "stocked_zones" else 0),
                        step("assert_zone", player=player, zone="exile", value=2 if fixture == "stocked_zones" else 0)]
            run += [step("assert_mana_total", player=0,
                         value=0 if variant in ("payment", "insufficient_mana") else 60-sum(mana))]
            actions += run
            if variant == "snapshot":
                actions += [step("record_state"), step("ai_roundtrip"), step("restore")] + run + [step("assert_recorded_state")]
            if variant == "fresh_attack":
                allowed = int("Haste" in keys and "Defender" not in keys)
                actions += [step("begin_attackers"), step("attack", ref="source", value=allowed),
                            observe("source", "attacking", allowed),
                            observe("source", "tapped", int(allowed and "Vigilance" not in keys)),
                            step("finish_attackers")]
        elif variant in ("block_ground", "block_flying", "tapped_blocker", "first_strike", "deathtouch"):
            attacker = "Suntail Hawk" if variant == "block_flying" else "Scaled Wurm" if variant == "deathtouch" else "Llanowar Elves"
            setup += [card(attacker, "attacker", player=1)]
            if variant == "tapped_blocker": setup[0]["state"] = ["tapped"]
            legal = int(variant != "tapped_blocker" and (variant != "block_flying" or bool(keys & {"Flying", "Reach"})))
            actions += [step("turn", player=1, value=0), step("begin_attackers"),
                        step("attack", ref="attacker", value=1), step("finish_attackers"), step("begin_blockers"),
                        step("block", ref="source", other="attacker", value=legal),
                        observe("source", "blocking_count", legal), step("advance_combat")]
            if variant in ("first_strike", "deathtouch"):
                survives = variant == "first_strike" or bool(keys & {"First strike", "Double strike"}) or toughness > 7
                actions += [step("combat_damage"), observe("attacker", "in_play", 0),
                            step("assert_zone_contains", player=1, zone="graveyard", value=registry[attacker]["csvid"]),
                            observe("source", "in_play", int(survives)), step("assert_life", player=1, value=20)]
                if survives: actions += [observe("source", "damage", 0 if keys & {"First strike", "Double strike"} else 7)]
        else:
            allowed = int("Defender" not in keys and variant != "tapped_attacker")
            if variant == "tapped_attacker": setup[0]["state"] = ["tapped"]
            actions += [step("begin_attackers"), step("attack", ref="source", value=allowed),
                        observe("source", "attacking", allowed),
                        observe("source", "tapped", int(variant == "tapped_attacker" or allowed and "Vigilance" not in keys)),
                        step("finish_attackers"), step("begin_blockers")]
            if variant.startswith("flying_"):
                blocker = "Grizzly Bears" if variant == "flying_evasion" else "Giant Spider"
                setup += [card(blocker, "blocker", player=1)]
                actions += [step("block", ref="blocker", other="source", value=int(variant == "flying_reach"))]
            elif variant == "trample":
                setup += [card("Ornithopter", "blocker", player=1)]
                actions += [step("block", ref="blocker", other="source", value=1), step("advance_combat"), step("combat_damage")]
                total = power * (2 if "Double strike" in keys else 1)
                lethal = 1 if "Deathtouch" in keys else 2
                actions += [step("assert_life", player=1, value=20-max(0,total-lethal)),
                            observe("source", "in_play", 1), observe("source", "damage", 0),
                            observe("blocker", "in_play", int(total < lethal)),
                            step("assert_life", player=0, value=20+(total if "Lifelink" in keys else 0))]
            elif variant.startswith("menace_"):
                setup += [card("Suntail Hawk", "blocker", player=1)]
                if variant == "menace_pair": setup += [card("Suntail Hawk", "second", player=1)]
                # The 2022 selector rejects a lone blocker when no second
                # eligible creature is available. Final legality is unchanged.
                actions += [step("block", ref="blocker", other="source", value=int(variant == "menace_pair"))]
                if variant == "menace_pair": actions += [step("block", ref="second", other="source", value=1)]
                actions += [step("advance_combat"), observe("source", "blocked", int(variant == "menace_pair")),
                            observe("blocker", "blocking_count", int(variant == "menace_pair"))]
                if variant == "menace_pair": actions += [observe("second", "blocking_count", 1)]
            elif variant == "unblocked":
                total = power * (2 if "Double strike" in keys else 1) if allowed else 0
                actions += [step("advance_combat"), step("combat_damage"),
                            step("assert_life", player=1, value=20-total),
                            step("assert_life", player=0, value=20+(total if "Lifelink" in keys else 0)),
                            observe("source", "in_play", 1), observe("source", "damage", 0),
                            step("end_combat"), observe("source", "attacking", 0)]
        cases.append(dict(name=f"{prefix}_{csvid}_{variant}", card=name, fixture=fixture, seed=123,
                          setup=setup, steps=actions, _recipe=dict(family=spec["family"], version=2,
                          variant=variant, params=declaration["params"], oracle_text=declaration["oracle_text"],
                          untested_keywords=["Flash"] if "Flash" in keys else [],
                          **({"untested_abilities":spec["remaining_abilities"]} if spec["family"] in ("evasion_creature", "defensive_creature", "blocking_creature", "combat_keyword_creature") else {}))))
    return cases
