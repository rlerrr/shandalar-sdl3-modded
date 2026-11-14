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
# Independently authored extensions share one narrow dispatcher contract.
# Imports stay lazy so an unrelated worker never loads unfinished modules.
EXTENSION_MODULES = {
    'growth_counter_spell': 'growth_counter_spell_recipes',
    'counterspell_extension': 'counterspell_extension_recipes',
    'edict_spell': 'edict_recipes',
    'growth_counter_activation': 'growth_counter_activation_recipes',
    'reanimation': 'reanimation_recipes',
    'token_trigger_creation': 'token_trigger_creation_recipes',
    'prevention': 'prevention_recipes',
    'tap_spell': 'tap_spell_recipes',
    'token_spell_extension': 'token_spell_extension_recipes',
    'etb_removal_extension': 'etb_removal_extension_recipes',
    'graveyard_recovery_extension': 'graveyard_recovery_extension_recipes',
    'damage_spell_extension': 'damage_spell_extension_recipes',
    'discard_spell_extension': 'discard_spell_extension_recipes',
    'life_spell_extension': 'life_spell_extension_recipes',
    'aura_keyword_extension': 'aura_keyword_extension_recipes',
    'tribal_static_extension': 'tribal_static_extension_recipes',
    'graveyard_exile': 'graveyard_exile_recipes',
    'mana_spell_extension': 'mana_spell_extension_recipes',
    'entry_value_extension': 'entry_value_extension_recipes',
    'static_effect_extension': 'static_effect_extension_recipes',
    'combat_value_trigger': 'combat_value_trigger_recipes',
    'attack_value_trigger': 'attack_value_trigger_recipes',
    'death_value_trigger': 'death_value_trigger_recipes',
    'death_recurrence': 'death_recurrence_recipes',
    'cast_value_trigger': 'cast_value_trigger_recipes',
    'entry_trigger_value': 'entry_trigger_value_recipes',
    'permanent_value_activation': 'permanent_value_activation_recipes',
    'activated_token_creation': 'activated_token_creation_recipes',
    'draw_value_trigger': 'draw_value_trigger_recipes',
    'death_counter_trigger': 'death_counter_trigger_recipes',
    'battalion_trigger': 'battalion_trigger_recipes',
    'entry_tutor_trigger': 'entry_tutor_trigger_recipes',
    'return_saboteur_trigger': 'return_saboteur_trigger_recipes',
    'charge_counter_activation': 'charge_counter_activation_recipes',
    'death_counter_extended': 'death_counter_extended_recipes',
    'entry_land_tutor': 'entry_land_tutor_recipes',
    'cast_untap_trigger': 'cast_untap_trigger_recipes',
    'mentor_attack_counter': 'mentor_attack_counter_recipes',
    'combat_library_prefix': 'combat_library_prefix_recipes',
    'attack_team_reward': 'attack_team_reward_recipes',
    'combat_targeted_removal': 'combat_targeted_removal_recipes',
    'cast_extort_trigger': 'cast_extort_trigger_recipes',
    'permanent_plus1_activation': 'permanent_plus1_activation_recipes',
    'entry_tap_trigger': 'entry_tap_trigger_recipes',
    'death_self_destination': 'death_self_destination_recipes',
    'upkeep_value_trigger': 'upkeep_value_trigger_recipes',
    'storm_effect': 'storm_effect_recipes',
    'death_target_effect': 'death_target_effect_recipes',
    'entry_linked_exile': 'entry_linked_exile_recipes',
    'block_damage_destruction': 'block_damage_destruction_recipes',
    'activated_temporary_pt': 'activated_temporary_pt_recipes',
    'upkeep_permanent_count': 'upkeep_permanent_count_recipes',
    'death_player_value': 'death_player_value_recipes',
    'entry_own_return': 'entry_own_return_recipes',
    'cast_temporary_stat_trigger': 'cast_temporary_stat_trigger_recipes',
    'activated_temporary_modifier': 'activated_temporary_modifier_recipes',
    'block_stat_trigger': 'block_stat_trigger_recipes',
    'upkeep_hand_value': 'upkeep_hand_value_recipes',
    'entry_aura_freeze': 'entry_aura_freeze_recipes',
    'cast_counter_engine': 'cast_counter_engine_recipes',
    'death_library_tutor': 'death_library_tutor_recipes',
    'cast_reload_tap_activation': 'cast_reload_tap_activation_recipes',
    'upkeep_controlled_return': 'upkeep_controlled_return_recipes',
    'aura_restriction': 'aura_restriction_recipes',
    'death_paid_value': 'death_paid_value_recipes',
    'conditional_blocking_legality': 'conditional_blocking_legality_recipes',
    'printed_tap_mana_activation': 'printed_tap_mana_activation_recipes',
    'upkeep_echo_payment': 'upkeep_echo_payment_recipes',
    'cast_conjunctive_base_stat': 'cast_conjunctive_base_stat_recipes',
    'paid_unblockability_activation': 'paid_unblockability_activation_recipes',
    'aura_mana_enhancement': 'aura_mana_enhancement_recipes',
    'counted_tap_mana_activation': 'counted_tap_mana_activation_recipes',
    'death_modular_transfer': 'death_modular_transfer_recipes',
    'cascade_spell_effect': 'cascade_spell_effect_recipes',
    'death_modular_composition': 'death_modular_composition_recipes',
    'unblockability_composition': 'unblockability_composition_recipes',
    'countdown_counter_lifecycle': 'countdown_counter_lifecycle_recipes',
    'counter_condition_recurrence': 'counter_condition_recurrence_recipes',
    'cumulative_upkeep_payment': 'cumulative_upkeep_payment_recipes',
}

def extension_module(family):
    from importlib import import_module
    return import_module(EXTENSION_MODULES[family])

VARIANTS = {
    "variable_sweep": ("mixed_x0_0", "payment_x3_0", "pending_snapshot_x5_0"),
    "etb_damage_creature": ("entry_0", "payment_0", "pending_snapshot_0", "blink_0"),
    "death_damage_creature": ("destroy_0", "payment_0", "death_snapshot_0", "combat_death_0"),
    "variable_damage": ("survivor_x0_0", "payment_x3_0", "snapshot_x5_0"),
    "variable_value": ("effect_x0_0", "payment_x3_0", "pending_snapshot_x5_0"),
    "static_stats": ("static_0", "payment_0", "pending_snapshot_0"),
    "removal_activation": ("effect_0", "payment_0", "pending_snapshot_0"),
    "mass_removal": ("mixed_0", "payment_0", "pending_snapshot_0"),
    "value_flow": ("effect_0", "payment_0", "pending_snapshot_0"),
    "hand_exchange": ("effect_0", "payment_0", "pending_snapshot_0"),
    "target_removal": ("effect_0", "payment_0", "pending_snapshot_0"),
    "library_search": ("search_0", "payment_0", "pending_snapshot_0"),
    "landcycling": ("search_0", "payment_0", "pending_snapshot_0"),
    "land_search": ("search_0", "payment_0", "pending_snapshot_0"),
    "land_entry_mana": ("entry_0", "production_0_0", "pending_snapshot_0"),
    "entry_stats_creature": ("cast_0", "payment_0", "pending_snapshot_0", "blink_0"),
    "creature_conditions": ("entry_0", "cast_0", "combat_0", "untap_attack_0"),
    "mana_modes": ("entry_0", "zero_pool_fallback_0", "production_0_0_0_0"),
    "conditional_keyword": ("effect_0_0", "effect_3_0", "increase_0", "decrease_0"),
    "conditional_stats": ("effect_0_0", "effect_1_0", "snapshot_1_0", "increase_0"),
    "conditional_mana_land": ("entry_none_0", "production_none_0_0", "entry_snapshot_none_0", "mana_snapshot_0_0"),
    "mana_consequence": ("production_0_0", "payment_0_0", "prevention_0_0", "mana_snapshot_0_0"),
    "damage_activation": ("effect_0", "payment_0", "pending_snapshot_0", "prevention_0"),
    "tap_activation": ("effect_0", "payment_0", "pending_snapshot_0", "combat_block_0"),
    "token_creation": ("create_0", "payment_0", "pending_snapshot_0", "destroy_token_0"),
    "self_keyword_activation": ("effect_0", "payment_0", "pending_snapshot_0", "expired_probe_0"),
    "target_keyword_activation": ("effect_0", "payment_0", "pending_snapshot_0", "cleanup_0"),
    "target_stats_activation": ("effect_0", "payment_0", "pending_snapshot_0", "cleanup_0"),
    "passive_keyword_creature": ("cast_0", "payment_0", "snapshot_0", "unblocked_0"),
    "death_value_creature": ("destroy_0", "payment_0", "death_snapshot_0", "combat_death_0"),
    "etb_recovery_creature": ("entry_0", "payment_0", "pending_snapshot_0", "blink_0"),
    "etb_removal_creature": ("entry_0", "payment_0", "pending_snapshot_0", "blink_0"),
    "counter_damage_creature": ("unblocked_0", "blocked_0", "cleanup_0", "combat_snapshot_0"),
    "trigger_pump_creature": ("effect_0", "payment_0", "pending_snapshot_0", "cleanup_0"),
    "attack_pump_creature": ("attack_0", "payment_0", "declaration_snapshot_0", "cleanup_0"),
    "mass_stats": ("effect_0", "payment_0", "pending_snapshot_0", "cleanup_0"),
    "fixed_mill": ("effect_0", "payment_0", "pending_snapshot_0", "two_copies_0"),
    "value_activation": ("value_0", "payment_0", "pending_snapshot_0", "two_sources_0"),
    "equipment": ("attach_0", "equip_payment_0", "move_0", "pending_snapshot_0"),
    "permanent_mana": ("entry_0", "production_0_0", "payment_0_0", "ready_snapshot_0_0"),
    "cycling": ("cycle_0", "payment_0", "pending_snapshot_0", "two_copies_0"),
    "entry_counter_creature": ("cast_0", "payment", "snapshot_0", "blink_snapshot_0"),
    "lord_creature": ("cast_0", "payment", "snapshot_0", "combat_snapshot_0"),
    "etb_value_creature": ("cast", "payment", "snapshot", "bounce_recast"),
    "blocking_creature": ("cast", "payment", "snapshot", "combat_snapshot"),
    "combat_keyword_creature": ("cast", "payment", "snapshot", "combat_snapshot_0"),
    "tap_damage_creature": ("cast", "activation_payment", "pending_snapshot", "survivor"),
    "regenerate_creature": ("cast", "payment", "snapshot", "destroy"),
    "mana_creature": ("cast", "payment", "snapshot", "fresh_activation"),
    "defensive_creature": ("cast", "payment", "snapshot", "defense_snapshot"),
    "evasion_creature": ("cast", "payment", "snapshot", "combat_snapshot"),
    "self_pump_creature": ("activation", "activation_payment", "stacked_activations", "activation_snapshot"),
    "aura_control": ("friendly_cleanup", "payment", "insufficient_mana", "lost_target", "snapshot"),
    "aura_creature": ("friendly_cleanup", "payment", "insufficient_mana", "lost_target", "snapshot"),
    "grant_creature": ("friendly_cleanup", "payment", "insufficient_mana", "lost_target", "snapshot"),
    "sweep": ("mixed", "payment", "insufficient_mana", "snapshot"),
    "damage_spell": ("payment", "insufficient_mana", "snapshot"),
    "recover_card": ("return", "payment", "insufficient_mana", "empty", "lost_target", "snapshot"),
    "counter_spell": ("opponent", "own_spell", "payment", "insufficient_mana", "no_stack", "snapshot"),
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
VARIANTS.update({family: () for family in EXTENSION_MODULES})


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
    from hybrid_creature_recipes import applies, specification
    if applies(declaration):return specification(declaration,oracle)
    if declaration.get('family') in EXTENSION_MODULES:
        return extension_module(declaration['family']).spec(declaration,oracle)
    if declaration.get('family')=='variable_sweep':
        from variable_sweep_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='etb_damage_creature':
        from etb_damage_creature_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='death_damage_creature':
        from death_damage_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='variable_damage':
        from variable_damage_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='variable_value':
        from variable_value_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='static_stats':
        from static_stats_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='removal_activation':
        from removal_activation_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='mass_removal':
        from mass_removal_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='value_flow':
        from value_flow_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='hand_exchange':
        from hand_exchange_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='target_removal':
        from removal_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='library_search':
        from library_search_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='landcycling':
        from landcycling_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='land_search':
        from land_search_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='land_entry_mana':
        from land_entry_mana_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='entry_stats_creature':
        from entry_stats_creature_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='creature_conditions':
        from creature_conditions_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='mana_modes':
        from mana_modes_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='conditional_keyword':
        from conditional_keyword_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='conditional_stats':
        from conditional_stats_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='conditional_mana_land':
        from conditional_mana_land_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='mana_consequence':
        from mana_consequence_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='damage_activation':
        from damage_activation_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='tap_activation':
        from tap_activation_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='token_creation':
        from token_creation_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='self_keyword_activation':
        from self_keyword_activation_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='target_keyword_activation':
        from target_keyword_activation_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='target_stats_activation':
        from target_stats_activation_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='passive_keyword_creature':
        from passive_keyword_creature_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='death_value_creature':
        from death_value_creature_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='etb_recovery_creature':
        from etb_recovery_creature_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='etb_removal_creature':
        from etb_removal_creature_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='counter_damage_creature':
        from counter_damage_creature_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='trigger_pump_creature':
        from trigger_pump_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='attack_pump_creature':
        from attack_pump_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='mass_stats':
        from mass_stats_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='fixed_mill':
        from mill_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='value_activation':
        from value_activation_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='equipment':
        from equipment_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='permanent_mana':
        from permanent_mana_recipes import spec
        return spec(declaration,oracle)
    if declaration.get('family')=='cycling':
        from cycling_recipes import spec
        return spec(declaration,oracle)
    family = declaration.get("family")
    if family not in VARIANTS:
        raise ValueError("unknown explicit recipe family")
    if family in ("aura_creature", "aura_control"):
        allowed_types=("Enchantment — Aura", "Snow Enchantment — Aura")
        if family == "aura_control":allowed_types += ("Kindred Enchantment — Rebel Aura",)
        if (not oracle or oracle.get("type_line") not in allowed_types
                or oracle.get("layout", "normal") != "normal" or oracle.get("selected_face") or oracle.get("card_faces")
                or family == "aura_creature" and not oracle.get("oracle_text", "").startswith(("Enchant creature\n", "Enchant creature ("))):
            raise ValueError("Aura recipe requires a normal single-face creature Aura")
        if declaration.get("oracle_text") != oracle.get("oracle_text") or "variants" not in declaration:
            raise ValueError("Aura recipe requires pinned Oracle text and reviewed variants")
        from grant_recipes import spec
        if family == "aura_control":
            scopes={"creature":"Enchant creature", "permanent":"Enchant permanent", "nonland":"Enchant nonland permanent", "artifact_or_creature":"Enchant artifact or creature"}
            parameters=declaration.get("params")
            scope=parameters.get("scope") if isinstance(parameters,dict) else None
            if not isinstance(scope,str) or scope not in scopes or not any(line==scopes[scope] or line.startswith(scopes[scope]+" (") for line in oracle["oracle_text"].splitlines()):
                raise ValueError("Aura control scope requires the reviewed Oracle enchant line")
        return spec(declaration.get("params"), cost_from_oracle(oracle), family)
    if family in ("creature_keywords", "self_pump_creature", "evasion_creature", "defensive_creature", "mana_creature", "regenerate_creature", "etb_value_creature", "blocking_creature", "tap_damage_creature", "combat_keyword_creature", "lord_creature", "entry_counter_creature"):
        if family == "entry_counter_creature":
            from entry_counter_creature_recipes import spec
            return spec(declaration, oracle)
        if family == "lord_creature":
            from lord_creature_recipes import spec
            return spec(declaration, oracle)
        if family == "combat_keyword_creature":
            from combat_keyword_creature_recipes import spec
            return spec(declaration, oracle)
        if family == "tap_damage_creature":
            from tap_damage_creature_recipes import spec
            return spec(declaration, oracle)
        if family == "blocking_creature":
            from blocking_creature_recipes import spec
            return spec(declaration, oracle)
        if family == "etb_value_creature":
            from etb_value_creature_recipes import spec
            return spec(declaration, oracle)
        if family == "regenerate_creature":
            from regenerate_creature_recipes import spec
            return spec(declaration, oracle)
        if family == "mana_creature":
            from mana_creature_recipes import spec
            return spec(declaration, oracle)
        if family == "defensive_creature":
            from defensive_creature_recipes import spec
            return spec(declaration, oracle)
        if family == "evasion_creature":
            from evasion_creature_recipes import spec
            return spec(declaration, oracle)
        if family == "self_pump_creature":
            from pump_creature_recipes import spec
            return spec(declaration, oracle)
        from creature_recipes import spec
        return spec(declaration, oracle)
    if family == "counter_spell":
        if not oracle or oracle.get("type_line") != "Instant" or oracle.get("layout", "normal") != "normal" or oracle.get("selected_face") or oracle.get("card_faces"):
            raise ValueError("counterspell recipe requires a normal single-face Instant")
        if not isinstance(declaration.get("oracle_text"), str) or not declaration["oracle_text"] or declaration["oracle_text"] != oracle.get("oracle_text"):
            raise ValueError("counterspell Oracle text changed or is missing; review the declaration")
        if "variants" not in declaration:
            raise ValueError("counterspell recipe requires reviewed variants")
        from counter_recipes import spec
        return spec(declaration.get("params"), cost_from_oracle(oracle))
    if (not oracle or oracle.get("selected_face") or oracle.get("card_faces")
            or oracle.get("layout", "normal") != "normal"
            or oracle.get("type_line") not in ("Instant", "Sorcery")
               and not (family in ("sweep", "grant_creature") and oracle.get("type_line") in ("Instant — Arcane", "Sorcery — Arcane"))):
        raise ValueError("explicit recipe requires a normal single-face Instant or Sorcery")
    text = declaration.get("oracle_text")
    if not isinstance(text, str) or not text or text != oracle.get("oracle_text"):
        raise ValueError("explicit recipe Oracle text changed or is missing; review the declaration")
    parameters = declaration.get("params")
    if family == "grant_creature":
        if "variants" not in declaration:
            raise ValueError("ability-grant recipes require reviewed variants")
        from grant_recipes import spec
        return spec(parameters, cost_from_oracle(oracle))
    if family == "sweep":
        if "variants" not in declaration:
            raise ValueError("sweep recipes require reviewed variants")
        from sweep_recipes import spec
        return spec(parameters, cost_from_oracle(oracle))
    if family == "damage_spell":
        if "variants" not in declaration:
            raise ValueError("damage recipes require reviewed variants")
        from damage_recipes import spec
        return spec(parameters, cost_from_oracle(oracle))
    if family == "recover_card":
        if "variants" not in declaration:
            raise ValueError("recovery recipes require reviewed variants")
        from graveyard_recipes import spec
        return spec(parameters, cost_from_oracle(oracle))
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


def expand(card_name, csvid, declaration, oracle, registry, oracle_context=None):
    from hybrid_creature_recipes import applies, expand as hybrid
    if applies(declaration):return hybrid(card_name,csvid,declaration,oracle,registry,oracle_context)
    return _expand_plain(card_name,csvid,declaration,oracle,registry,oracle_context)


def _expand_plain(card_name, csvid, declaration, oracle, registry, oracle_context=None):
    if not isinstance(declaration, dict):
        raise ValueError("recipe declaration must be an object")
    version = declaration.get("version")
    if type(version) is not int or version not in (VERSION, 2):
        raise ValueError("recipe requires explicit supported version")
    allowed = {"family", "version", "variants"} | ({"params", "oracle_text", "resolve_dialogs", "resolve_zones", "cast_dialogs", "repeat_cast_dialogs", "setup", "eligibility_only", "cast_target_first", "cast_target_count"} if version == 2 else set())
    if version == 2 and declaration.get('family') in ('etb_removal_creature','etb_recovery_creature','library_search'):allowed |= {'trigger_priority'}
    if version == 2 and declaration.get('family') == 'self_pump_creature':allowed |= {'activation_dialog','activation_dialogs','activation_cancel_mana'}
    if version == 2 and declaration.get('family') in ('value_activation','hand_exchange','value_flow'):allowed |= {'activation_dialogs'}
    if version == 2 and declaration.get('family') in ('hand_exchange','value_flow'):allowed |= {'target_choices'}
    if version == 2 and declaration.get('family') in ('mass_removal','edict_spell','discard_spell_extension'):allowed |= {'target_choices'}
    if version == 2 and declaration.get('family') == 'removal_activation':allowed |= {'activation_dialogs'}
    if version == 2 and declaration.get('family') == 'land_search':allowed |= {'activation_dialogs'}
    if version == 2 and declaration.get('family') == 'library_search':allowed |= {'activation_dialogs','entry_dialogs','decline_entry_dialogs','trigger_players','gate_menu'}
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
    if family in EXTENSION_MODULES:
        module = extension_module(family)
        if family == 'token_trigger_creation':
            module.validate_witnesses(oracle_context,registry,spec)
        else:
            module.validate_witnesses(oracle_context,registry)
        supported_variants = set(module.variants(spec))
    if family == 'variable_sweep':
        from variable_sweep_recipes import variants as sweep_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(sweep_variants(spec))
    if family == 'etb_damage_creature':
        from etb_damage_creature_recipes import variants as entry_damage_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(entry_damage_variants(spec))
    if family == 'death_damage_creature':
        from death_damage_recipes import variants as death_damage_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(death_damage_variants(spec))
    if family == 'variable_damage':
        from variable_damage_recipes import variants as damage_variants,validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(damage_variants(spec))
    if family == 'variable_value':
        from variable_value_recipes import variants as value_variants,validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(value_variants(spec))
    if family == 'static_stats':
        from static_stats_recipes import variants as static_variants,validate_witnesses
        validate_witnesses(oracle_context,registry,spec)
        supported_variants=set(static_variants(spec))
    if family == 'removal_activation':
        from removal_activation_recipes import variants as removal_activation_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(removal_activation_variants(spec))
    if family == 'mass_removal':
        from mass_removal_recipes import variants as mass_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(mass_variants(spec))
    if family in ('hand_exchange','value_flow'):
        from hand_exchange_recipes import variants as exchange_variants, validate_witnesses
        if family == 'value_flow':
            from value_flow_recipes import variants as exchange_variants
        validate_witnesses(oracle_context,registry)
        supported_variants = set(exchange_variants(spec))
    if family == 'target_removal':
        from removal_recipes import variants as removal_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants = set(removal_variants(spec))
    if family == 'library_search':
        from library_search_recipes import variants as library_variants, validate_witnesses
        validate_witnesses(oracle_context,registry,spec)
        supported_variants = set(library_variants(spec))
    if family == 'landcycling':
        from landcycling_recipes import variants as landcycling_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants = set(landcycling_variants(spec))
    if family == 'land_search':
        from land_search_recipes import variants as search_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants = set(search_variants(spec))
    if family == 'land_entry_mana':
        from land_entry_mana_recipes import variants as land_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants = set(land_variants(spec))
    if family == 'entry_stats_creature':
        from entry_stats_creature_recipes import variants as entry_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants = set(entry_variants(spec))
    if family in ("grant_creature", "aura_creature", "aura_control"):
        from grant_recipes import variants as grant_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        if family in ("aura_creature", "aura_control"):
            for witness, cost, text in (("Disenchant", "{1}{W}", "Destroy target artifact or enchantment."),
                    ("Disperse", "{1}{U}", "Return target nonland permanent to its owner's hand.")):
                r=oracle_context.get(witness, {})
                if witness not in registry or (r.get("mana_cost"),r.get("oracle_text")) != (cost,text):
                    raise ValueError(f"Aura action witness changed: {witness}")
        if family == "aura_control":
            for witness, power, toughness, text in (("Furnace Whelp","2","2","Flying\n{R}: This creature gets +1/+0 until end of turn."),
                    ("Llanowar Elves","1","1","{T}: Add {G}."), ("Wirewood Elf","1","2","{T}: Add {G}.")):
                r=oracle_context.get(witness,{})
                if witness not in registry or "Creature" not in r.get("type_line","") or (r.get("power"),r.get("toughness"),r.get("oracle_text")) != (power,toughness,text):
                    raise ValueError(f"Aura activation witness changed: {witness}")
        supported_variants = set(grant_variants(spec))
    if family == "creature_conditions":
        from creature_conditions_recipes import variants as creature_condition_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(creature_condition_variants(spec))
    if family == "mana_modes":
        from mana_modes_recipes import variants as mana_modes_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(mana_modes_variants(spec))
    if family == "conditional_keyword":
        from conditional_keyword_recipes import variants as conditional_keyword_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(conditional_keyword_variants(spec))
    if family == "conditional_stats":
        from conditional_stats_recipes import variants as conditional_stat_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(conditional_stat_variants(spec))
    if family == "conditional_mana_land":
        from conditional_mana_land_recipes import variants as conditional_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(conditional_variants(spec))
    if family == "mana_consequence":
        from mana_consequence_recipes import variants as consequence_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(consequence_variants(spec))
    if family == "damage_activation":
        from damage_activation_recipes import variants as damage_activation_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(damage_activation_variants(spec))
    if family == "tap_activation":
        from tap_activation_recipes import variants as tap_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(tap_variants(spec))
    if family == "token_creation":
        from token_creation_recipes import variants as token_variants, validate_witnesses
        validate_witnesses(oracle_context,registry,spec)
        supported_variants=set(token_variants(spec))
    if family == "self_keyword_activation":
        from self_keyword_activation_recipes import variants as self_keyword_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants=set(self_keyword_variants(spec))
    if family == "target_keyword_activation":
        from target_keyword_activation_recipes import variants as keyword_activation_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants=set(keyword_activation_variants(spec))
    if family == "target_stats_activation":
        from target_stats_activation_recipes import variants as target_stats_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants=set(target_stats_variants(spec))
    if family == "passive_keyword_creature":
        from passive_keyword_creature_recipes import variants as passive_variants
        supported_variants=set(passive_variants(spec))
    if family == "creature_keywords":
        from creature_recipes import variants as creature_variants
        supported_variants = set(creature_variants(spec))
    if family == "death_value_creature":
        from death_value_creature_recipes import variants as death_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants=set(death_variants(spec))
    if family == "etb_removal_creature":
        from etb_removal_creature_recipes import variants as etb_removal_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants=set(etb_removal_variants(spec))
    if family == "etb_recovery_creature":
        from etb_recovery_creature_recipes import variants as recovery_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants=set(recovery_variants(spec))
    if family == "counter_damage_creature":
        from counter_damage_creature_recipes import variants as counter_damage_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants=set(counter_damage_variants(spec))
    if family == "entry_counter_creature":
        from entry_counter_creature_recipes import variants as entry_counter_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants = set(entry_counter_variants(spec))
    if family == "trigger_pump_creature":
        from trigger_pump_recipes import variants as trigger_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(trigger_variants(spec))
    if family == "attack_pump_creature":
        from attack_pump_recipes import variants as attack_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(attack_variants(spec))
    if family == "mass_stats":
        from mass_stats_recipes import variants as mass_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(mass_variants(spec))
    if family == "fixed_mill":
        from mill_recipes import variants as mill_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(mill_variants(spec))
    if family == "value_activation":
        from value_activation_recipes import variants as value_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(value_variants(spec))
    if family == "equipment":
        from equipment_recipes import variants as equipment_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(equipment_variants(spec))
    if family == "permanent_mana":
        from permanent_mana_recipes import variants as permanent_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(permanent_variants(spec))
    if family == "cycling":
        from cycling_recipes import variants as cycling_variants, validate_witnesses
        validate_witnesses(oracle_context,registry)
        supported_variants=set(cycling_variants(spec))
    if family == "lord_creature":
        from lord_creature_recipes import variants as lord_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants = set(lord_variants(spec))
    if family == "combat_keyword_creature":
        from combat_keyword_creature_recipes import variants as combat_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants = set(combat_variants(spec))
    if family == "tap_damage_creature":
        from tap_damage_creature_recipes import variants as tap_damage_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants = set(tap_damage_variants(spec))
    if family == "blocking_creature":
        from blocking_creature_recipes import variants as blocking_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants = set(blocking_variants(spec))
    if family == "etb_value_creature":
        from etb_value_creature_recipes import variants as entry_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants = set(entry_variants(spec))
    if family == "regenerate_creature":
        from regenerate_creature_recipes import variants as regeneration_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants = set(regeneration_variants(spec))
    if family == "mana_creature":
        from mana_creature_recipes import variants as mana_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants = set(mana_variants(spec))
    if family == "defensive_creature":
        from defensive_creature_recipes import variants as defensive_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants = set(defensive_variants(spec))
    if family == "evasion_creature":
        from evasion_creature_recipes import variants as evasion_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants = set(evasion_variants(spec))
    if family == "self_pump_creature":
        from pump_creature_recipes import variants as pump_variants, validate_witnesses
        validate_witnesses(oracle_context, registry)
        supported_variants = set(pump_variants(spec))
    if family == "counter_spell":
        from counter_recipes import variants as counter_variants
        supported_variants = set(counter_variants(spec))
    if family == "recover_card":
        from graveyard_recipes import validate_witnesses, variants as recovery_variants
        validate_witnesses(oracle_context, registry)
        supported_variants = set(recovery_variants(spec, oracle_context))
    if family == "damage_spell":
        from damage_recipes import validate_witnesses, variants as damage_variants
        validate_witnesses(oracle_context, registry)
        supported_variants = set(damage_variants(spec, oracle_context))
    if family == "sweep":
        from sweep_recipes import validate_witnesses, variants as sweep_variants
        validate_witnesses(oracle_context, registry)
        supported_variants = set(sweep_variants(spec))
    if family == "destroy_permanent": supported_variants |= {"target_"+t for t in spec["targets"]}
    if family == "move_permanent": supported_variants |= {"target_"+t for t in spec["targets"]} | {"wrong_controller", "wrong_state"}
    if (not isinstance(variants, list) or not variants or any(v not in supported_variants for v in variants)
            or len(set(variants)) != len(variants)):
        raise ValueError("unknown, empty, or duplicate recipe variants")
    eligibility = declaration.get("eligibility_only", [])
    if (not isinstance(eligibility, list) or any(not isinstance(v, str) for v in eligibility)
            or len(set(eligibility)) != len(eligibility)
            or any(v not in variants or v not in ("opponent_cleanup", "opponent_unblocked", "expired_flying") for v in eligibility)
            or eligibility and (family != "grant_creature" or spec.get("controller", "any") != "any")):
        raise ValueError("eligibility_only requires reviewed opposing-target grant variants")
    if "cast_target_first" in declaration and (declaration["cast_target_first"] is not True or family != "grant_creature"):
        raise ValueError("cast_target_first requires a reviewed grant decision order")
    if "cast_target_count" in declaration and (family != "aura_creature"
            or type(declaration["cast_target_count"]) is not int or declaration["cast_target_count"] not in (1,2)):
        raise ValueError("cast_target_count requires a reviewed Aura presentation sequence")
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
            or cast_dialogs and family not in ("move_permanent", "sweep", "grant_creature")):
        raise ValueError("cast_dialogs requires reviewed movement/sweep/grant variants and zero-based choices")
    repeat_dialogs = declaration.get("repeat_cast_dialogs", {})
    if (not isinstance(repeat_dialogs, dict)
            or any(v != "stacking" or v not in variants or not isinstance(choices, list)
                   or any(type(n) is not int or not 0 <= n <= 32 for n in choices) for v, choices in repeat_dialogs.items())
            or repeat_dialogs and family != "sweep"):
        raise ValueError("repeat_cast_dialogs requires reviewed sweep stacking choices")

    def identity(name):
        if name not in registry:
            raise ValueError(f"recipe witness missing from registry: {name}")
        return registry[name]["csvid"]

    if family in EXTENSION_MODULES:
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('extension recipes require dedicated cases for custom setup/decisions')
        return extension_module(family).expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == 'variable_sweep':
        if dialogs or support or resolve_zones or cast_dialogs:raise ValueError('variable sweeps need dedicated cases for custom setup/decisions')
        from variable_sweep_recipes import expand as variable_sweep
        return variable_sweep(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == 'etb_damage_creature':
        if dialogs or support or resolve_zones or cast_dialogs:raise ValueError('entry damage needs dedicated cases for custom setup/decisions')
        from etb_damage_creature_recipes import expand as entry_damage
        return entry_damage(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == 'death_damage_creature':
        if dialogs or support or resolve_zones or cast_dialogs:raise ValueError('death damage needs dedicated cases for custom setup/decisions')
        from death_damage_recipes import expand as death_damage
        return death_damage(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == 'variable_damage':
        if dialogs or support or resolve_zones or cast_dialogs:raise ValueError('variable damage needs dedicated cases for custom setup/decisions')
        from variable_damage_recipes import expand as variable_damage
        return variable_damage(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == 'variable_value':
        if dialogs or support or resolve_zones or cast_dialogs:raise ValueError('variable values need dedicated cases for custom setup/decisions')
        from variable_value_recipes import expand as variable_values
        return variable_values(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == 'static_stats':
        if dialogs or support or resolve_zones or cast_dialogs:raise ValueError('static stats need dedicated cases for custom setup/decisions')
        from static_stats_recipes import expand as statics
        return statics(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == 'removal_activation':
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('removal activation custom setup and decisions require dedicated cases')
        from removal_activation_recipes import expand as removal_activation_expand
        return removal_activation_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == 'mass_removal':
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('mass removal custom setup and decisions require dedicated cases')
        from mass_removal_recipes import expand as mass_expand
        return mass_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family in ('hand_exchange','value_flow'):
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('hand exchange custom setup and decisions require dedicated cases')
        from hand_exchange_recipes import expand as exchange_expand
        return exchange_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == 'target_removal':
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('removal custom setup and decisions require dedicated cases')
        from removal_recipes import expand as removal_expand
        return removal_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == 'library_search':
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('library tutor custom setup and decisions require dedicated cases')
        from library_search_recipes import expand as library_expand
        return library_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == 'landcycling':
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('landcycling custom setup and decisions require dedicated cases')
        from landcycling_recipes import expand as landcycling_expand
        return landcycling_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == 'land_search':
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('search custom setup and resolution decisions require dedicated cases')
        from land_search_recipes import expand as search_expand
        return search_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == 'land_entry_mana':
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('land entry custom decisions and support require dedicated cases')
        from land_entry_mana_recipes import expand as land_entry_expand
        return land_entry_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == 'entry_stats_creature':
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('entry stat custom decisions and support require dedicated cases')
        from entry_stats_creature_recipes import expand as entry_stats_expand
        return entry_stats_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "creature_conditions":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('creature-condition custom setup and decisions require dedicated cases')
        from creature_conditions_recipes import expand as creature_condition_expand
        return creature_condition_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "mana_modes":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('mana-mode custom setup and decisions require dedicated cases')
        from mana_modes_recipes import expand as mana_modes_expand
        return mana_modes_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "conditional_keyword":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('conditional-keyword custom setup and decisions require dedicated cases')
        from conditional_keyword_recipes import expand as conditional_keyword_expand
        return conditional_keyword_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "conditional_stats":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('conditional-stat custom setup and decisions require dedicated cases')
        from conditional_stats_recipes import expand as conditional_stat_expand
        return conditional_stat_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "conditional_mana_land":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('conditional land custom setup and decisions require dedicated cases')
        from conditional_mana_land_recipes import expand as conditional_land_expand
        return conditional_land_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "mana_consequence":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('mana consequence custom setup and decisions require dedicated cases')
        from mana_consequence_recipes import expand as mana_consequence_expand
        return mana_consequence_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "damage_activation":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('damage activation custom setup and decisions require dedicated cases')
        from damage_activation_recipes import expand as activated_damage
        return activated_damage(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "tap_activation":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('tap/untap custom setup and decisions require dedicated cases')
        from tap_activation_recipes import expand as tap_expand
        return tap_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "token_creation":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('token creation custom setup and decisions require dedicated cases')
        from token_creation_recipes import expand as token_expand
        return token_expand(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "self_keyword_activation":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("self keyword abilities require dedicated cases for custom decisions/setup")
        from self_keyword_activation_recipes import expand as self_keywords
        return self_keywords(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "target_keyword_activation":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("keyword abilities require dedicated cases for custom decisions/setup")
        from target_keyword_activation_recipes import expand as keyword_activation
        return keyword_activation(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "target_stats_activation":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("target stats abilities require dedicated cases for custom decisions/setup")
        from target_stats_activation_recipes import expand as targeted_stats
        return targeted_stats(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "passive_keyword_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("passive keyword components require dedicated cases for custom decisions/setup")
        from passive_keyword_creature_recipes import expand as passive_keywords
        return passive_keywords(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "death_value_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("death recipes require dedicated cases for custom decisions/setup")
        from death_value_creature_recipes import expand as death_receipt
        return death_receipt(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "etb_recovery_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("entry recovery recipes require dedicated cases for custom decisions/setup")
        from etb_recovery_creature_recipes import expand as entry_recovery
        return entry_recovery(card_name,csvid,declaration,spec,variants,registry,oracle_context)

    if family == "etb_removal_creature":
        if support or resolve_zones or cast_dialogs:
            raise ValueError("entry removal recipes require dedicated cases for custom setup/zone/cast decisions")
        from etb_removal_creature_recipes import expand as etb_removal
        return etb_removal(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "counter_damage_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("counter damage recipes require dedicated cases for custom decisions/setup")
        from counter_damage_creature_recipes import expand as counter_damage
        return counter_damage(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "entry_counter_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("entry counter recipes require dedicated cases for custom decisions/setup")
        from entry_counter_creature_recipes import expand as entry_counters
        return entry_counters(card_name, csvid, declaration, spec, variants, registry, oracle_context)
    if family == "trigger_pump_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("trigger pump recipes require dedicated cases for custom decisions/setup")
        from trigger_pump_recipes import expand as trigger
        return trigger(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "attack_pump_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("attack pump recipes require dedicated cases for custom decisions/setup")
        from attack_pump_recipes import expand as attack
        return attack(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "mass_stats":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("mass stats recipes require dedicated cases for custom decisions/setup")
        from mass_stats_recipes import expand as mass
        return mass(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "fixed_mill":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("fixed mill recipes require dedicated cases for custom decisions/setup")
        from mill_recipes import expand as mill
        return mill(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "value_activation":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("value activation recipes require dedicated cases for custom decisions/setup")
        from value_activation_recipes import expand as value_activation
        return value_activation(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "equipment":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("equipment recipes require dedicated cases for custom decisions/setup")
        from equipment_recipes import expand as equipment
        return equipment(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "permanent_mana":
        if dialogs or support or declaration.get("resolve_zones") or declaration.get("cast_dialogs") or declaration.get("repeat_cast_dialogs"):
            raise ValueError("mana permanent recipes require dedicated cases for custom decisions/setup")
        from permanent_mana_recipes import expand as permanent_mana
        return permanent_mana(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "cycling":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError('cycling recipes require dedicated cases for custom decisions/setup')
        from cycling_recipes import expand as cycling
        return cycling(card_name,csvid,declaration,spec,variants,registry,oracle_context)
    if family == "lord_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("lord recipes require dedicated cases for custom decisions/setup")
        from lord_creature_recipes import expand as lords
        return lords(card_name, csvid, declaration, spec, variants, registry, oracle_context)
    if family == "combat_keyword_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("combat keyword recipes require dedicated cases for custom decisions/setup")
        from combat_keyword_creature_recipes import expand as combat_creatures
        return combat_creatures(card_name, csvid, declaration, spec, variants, registry, oracle_context)
    if family == "tap_damage_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("tap damage recipes require dedicated cases for custom decisions/setup")
        from tap_damage_creature_recipes import expand as shooters
        return shooters(card_name, csvid, declaration, spec, variants, registry, oracle_context)
    if family == "blocking_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("blocking recipes require dedicated cases for custom decisions/setup")
        from blocking_creature_recipes import expand as blockers
        return blockers(card_name, csvid, declaration, spec, variants, registry, oracle_context)
    if family == "etb_value_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("entry recipes require dedicated cases for custom decisions/setup")
        from etb_value_creature_recipes import expand as entry_creatures
        return entry_creatures(card_name, csvid, declaration, spec, variants, registry, oracle_context)
    if family == "regenerate_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("regeneration recipes require dedicated cases for custom decisions/setup")
        from regenerate_creature_recipes import expand as regenerators
        return regenerators(card_name, csvid, declaration, spec, variants, registry, oracle_context)
    if family == "mana_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("mana recipes require dedicated cases for custom decisions/setup")
        from mana_creature_recipes import expand as mana_creatures
        return mana_creatures(card_name, csvid, declaration, spec, variants, registry, oracle_context)
    if family == "defensive_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("defensive recipes require dedicated cases for custom decisions/setup")
        from defensive_creature_recipes import expand as defenders
        return defenders(card_name, csvid, declaration, spec, variants, registry, oracle_context)
    if family == "evasion_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("evasion recipes require dedicated cases for custom decisions/setup")
        from evasion_creature_recipes import expand as evaders
        return evaders(card_name, csvid, declaration, spec, variants, registry, oracle_context)
    if family == "self_pump_creature":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("self-pump recipes require dedicated cases for custom casting decisions/setup")
        from pump_creature_recipes import expand as pumps
        return pumps(card_name, csvid, declaration, spec, variants, registry, oracle_context)
    if family == "creature_keywords":
        if dialogs or support or resolve_zones:
            raise ValueError("creature recipes require dedicated cases for custom decisions/setup")
        from creature_recipes import expand as creatures
        return creatures(card_name, csvid, declaration, spec, variants, registry)
    if family == "counter_spell":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("counterspell recipes require dedicated cases for custom decisions/setup")
        from counter_recipes import expand as counters, validate_witnesses
        validate_witnesses(oracle_context, registry)
        return counters(card_name, csvid, declaration, spec, variants, registry, oracle_context)

    if family == "recover_card":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("recovery recipes need dedicated cases for custom decisions/setup")
        from graveyard_recipes import expand as recovery
        return recovery(card_name, csvid, declaration, spec, variants, registry, oracle_context)

    if family == "damage_spell":
        if dialogs or support or resolve_zones or cast_dialogs:
            raise ValueError("damage recipes need dedicated cases for custom decisions/setup")
        from damage_recipes import expand as damage
        return damage(card_name, csvid, declaration, spec, variants, registry, oracle_context)

    if family == "sweep":
        if dialogs or support or resolve_zones:
            raise ValueError("sweeps need dedicated cases for custom decisions/setup")
        from sweep_recipes import expand as sweep
        return sweep(card_name, csvid, declaration, spec, variants, registry, oracle_context)

    if family in ("grant_creature", "aura_creature", "aura_control"):
        if dialogs or support or resolve_zones or repeat_dialogs:
            raise ValueError("ability grants require dedicated cases for custom setup/resolution")
        from grant_recipes import expand as grants
        return grants(card_name, csvid, declaration, spec, variants, registry, oracle_context)

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
