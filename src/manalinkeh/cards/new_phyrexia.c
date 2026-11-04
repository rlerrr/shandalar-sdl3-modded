#include "manalink.h"

static void cathedral_membrane_legacy(int player, int card, int t_player, int t_card);

/* Canonical recovered helper declarations. */
int has_phyrexian_mana(int player, int cless, int black, int blue, int green, int red,
                            int white, int clred_artifact);
int has_phyrexian_mana_for_activated_ability(int player, int card, int cless, int black,
                                                  int blue, int green, int red, int white);
int charge_phyrexian_mana(int player, int card, event_t event, int cless, int black, int blue,
                               int green, int red, int white, int clred_artifact);
int charge_phyrexian_mana_for_activated_ability(int player, int card, int cless, int black,
                                                     int blue, int green, int red, int white);
int phyrexian_casting_costs(int player, int card, event_t event, int cless, int black,
                                 int blue, int green, int red, int white, int clred_artifact);
static int charge_phyrexian_mana_impl(int player, int cless, int black, int blue, int green,
                                           int red, int white, int clred_artifact);

// ---- GLOBAL FUNCTIONS ----

static int generic_splicer(int player, int card, event_t event, int pow, int tou, int keyword, int golems ){

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		generate_tokens_by_id(player, card, CARD_ID_GOLEM, golems);
	}

	return boost_creature_type(player, card, event, SUBTYPE_GOLEM, pow, tou, keyword, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
}

static void np_shrines(int player, int card, event_t event, int clr)
{

  /* Shrine of [Something]	|[cost]
   * Artifact
   * At the beginning of your upkeep or whenever you cast a [clr] spell, put a charge counter on ~.
   * |[act_cost] |T, Sacrifice ~: [do something] */

  // This shouldn't include resolution or construction of the target object - you can't factor out a
  // function by special-casing all of its callers.

  upkeep_trigger_ability(player, card, event, player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    add_counters(player, card, player, card, COUNTER_CHARGE, 1);
  }

  if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card))
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_ANY);
    test.color = get_sleighted_color_test(player, card, clr);
    sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_SPA)
    add_counters(player, card, player, card, COUNTER_CHARGE, 1);
}

static int is_poisoned(int player){
	if( POISON_COUNTERS(player) > 0 ){
		return 1;
	}
	return 0;
}

//---- CARDS ----

int card_batterskull(int player, int card, event_t event)
{
  /* Batterskull	|5
   * Artifact - Equipment
   * Living weapon
   * Equipped creature gets +4/+4 and has vigilance and lifelink.
   * |3: Return ~ to its owner's hand.
   * Equip |5 */

  enum
  {
    CHOICE_EQUIP = 1,
    CHOICE_BOUNCE = 2
  };

  card_instance_t *instance;
  int choice;
  char equipped_player;
  card_instance_t *equipment;
  int equip_available;
  int bounce_available;
  card_instance_t *equipped;
  unsigned int special_keywords;
  unsigned int keywords;
  int bounce_priority;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_PHYREXIAN_GERM, &token);
    token.action = TOKEN_ACTION_EQUIP;
    generate_token(&token);
  }
  instance = in_play(player, card);
  if (!instance && event == EVENT_ETB)
    return 0;
  if (instance && !is_humiliated(player, card, AL_STATIC))
  {
    equipment = is_equipping(player, card);
    if (equipment && (equipment = get_card_instance(player, card), (uint32_t)(event - EVENT_POWER) < 3))
    {
      if (equipment->damage_target_card == affected_card &&
          equipment->damage_target_player == affected_card_controller)
        return vanilla_equipment(player, card, event, 5, 4, 4, 0, 10);
      return 0;
    }
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 3, 0, 0, 0, 0, 0, 0, NULL,
                                    NULL) == 0)
      {
        return basic_equipment(player, card, EVENT_CAN_ACTIVATE, 5);
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {
      equip_available = can_activate_basic_equipment(player, card, EVENT_ACTIVATE, 5);
      bounce_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 3, 0, 0, 0,
                                                   0, 0, 0, NULL, NULL);
      equipped = is_equipping(player, card);
      if (equipped == NULL)
      {
        target_definition_t td;
        default_target_definition(player, card, &td, 2);
        td.preferred_controller = player;
        td.allowed_controller = player;
        bounce_priority = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 5, 0, 0, 0,
                                                    0, 0, 0, &td, NULL);
        bounce_priority = ((bounce_priority == 0) ? 10U : 1U);
      }
      else
      {
        equipped_player = instance->damage_target_player;
        bounce_priority = instance->damage_target_card;
        special_keywords = get_special_abilities((int)equipped_player, bounce_priority);
        keywords = get_abilities((int)equipped_player, bounce_priority, EVENT_ABILITIES, -1);
        bounce_priority = 1;
        if ((keywords & 0x800000) != 0)
        {
          bounce_priority = (((special_keywords & 0x20) == 0) ? 1U : 10U);
        }
      }
      choice =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Equip", equip_available, 5,
                      "Return Batterskul to hand", bounce_available, bounce_priority, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_EQUIP)
      {
        return basic_equipment(player, card, EVENT_ACTIVATE, 5);
      }
      if (choice != CHOICE_BOUNCE)
      {
        return 0;
      }
      ai_modifier = ai_modifier + (unsigned int)((1 - player == current_turn) ? 0 : -24) + 0x30;
      return generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 3, 0, 0, 0, 0, 0, 0, NULL,
                                       NULL);
    }
    return 0;
  }
  if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->info_slot == CHOICE_EQUIP)
    {
      return basic_equipment(player, card, EVENT_RESOLVE_ACTIVATION, 5);
    }
    if (instance->info_slot == CHOICE_BOUNCE)
    {
      bounce_permanent(instance->parent_controller, instance->parent_card);
    }
  }
  return 0;
}

int card_blade_splicer(int player, int card, event_t event ){
	/* Blade Splicer	|2|W
	 * Creature - Human Artificer 1/1
	 * When ~ enters the battlefield, put a 3/3 colorless Golem artifact creature token onto the battlefield.
	 * Golem creatures you control have first strike. */

	return generic_splicer(player, card, event, 0, 0, KEYWORD_FIRST_STRIKE, 1);
}

int card_beast_within(int player, int card, event_t event ){
	/* Beast Within	|2|G
	 * Instant
	 * Destroy target permanent. Its controller puts a 3/3 |Sgreen Beast creature token onto the battlefield. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_BEAST, &token);
			token.t_player = instance->targets[0].player;
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
}

/* ManalinkEh 2022: 0x226ce90; original cards/new_phyrexia.c:418. */
static int bludgeon_brawl_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  int target_player;
  card_instance_t *equipment;
  card_instance_t *equipped;
  int mana_value_or_valid;
  int equipment_card;
  target_definition_t td;

  instance = get_card_instance(player, card);
  target_player = (int)instance->damage_target_player;
  equipment_card = instance->damage_target_card;
  equipment = in_play(target_player, equipment_card);
  if ((equipment != NULL) && (equipped = is_equipping(target_player, equipment_card), equipped != NULL))
  {
    mana_value_or_valid = get_cmc(target_player, equipment_card);
    modify_pt_and_abilities((int)equipment->damage_target_player, equipment->damage_target_card,
                            event, mana_value_or_valid, 0, 0);
  }
  equipment = get_card_instance(player, card);
  if (equipment->targets[1].player >= 0)
  {
    other_leaves_play(player, card, instance->targets[1].player, instance->targets[1].card, event,
                      0, 0);
    bool detach = event == EVENT_LEAVES_PLAY_ABILITY;
    if (!detach && event == EVENT_STATIC_EFFECTS)
    {
      if (!is_humiliated(instance->targets[1].player, instance->targets[1].card, AL_STATIC))
        return 0;
      detach = true;
    }
    else if (!detach && event == EVENT_CARDCONTROLLED)
      detach = instance->targets[1].card == affected_card &&
               instance->targets[1].player == affected_card_controller;
    if (detach)
    {
      unattach(target_player, equipment_card);
      kill_card(player, card, KILL_EXILE);
    }
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = player;
  td.preferred_controller = player;
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (mana_value_or_valid = validate_target(player, card, &td, 0), mana_value_or_valid != 0))
  {
    equip_target_creature(target_player, equipment_card, instance->targets[0].player,
                          instance->targets[0].card);
  }
  return granted_generic_activated_ability(player, card, target_player, equipment_card, event, 0x102,
                                           get_cmc(target_player, equipment_card), 0, 0, 0, 0, 0, 0,
                                           &td, "Select target creature you control.");
}

// Chancellor of the Tangle code is in "rules_engine.c" in "functions" folder

/* ManalinkEh 2022: 0x226e590; original cards/new_phyrexia.c:458. */
int card_bludgeon_brawl(int player, int card, event_t event)
{
  /* Bludgeon Brawl	|2|R
   * Enchantment
   * Each noncreature, non-Equipment artifact is an Equipment with equip |X and "Equipped creature gets +X/+0," where X is that artifact's mana value. */

  int c;
  bool legacy_found;
  int eligible;
  card_instance_t *instance;

  if (((((event == EVENT_CHANGE_TYPE) && (instance = in_play(player, card), instance != NULL)) &&
        (eligible = is_humiliated(player, card, AL_STATIC), eligible == 0)) &&
       ((eligible = is_what(affected_card_controller, affected_card, 0x40),
         eligible != 0 &&
             (eligible = is_what(affected_card_controller, affected_card, 2), eligible == 0)))) &&
      (eligible = is_equipment(affected_card_controller, affected_card), eligible == 0))
  {

    add_a_subtype(affected_card_controller, affected_card, SUBTYPE_EQUIPMENT);
    if (0 < active_cards_count[player])
    {
      c = 0;
      legacy_found = false;
      do
      {
        instance = in_play(player, c);

        if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
             (instance = get_card_instance(player, c),
              instance->info_slot_fn == bludgeon_brawl_effect)) &&
            (((instance->damage_target_player == affected_card_controller &&
               (instance->damage_target_card == affected_card)) ||
              ((affected_card_controller == instance->targets[6].player &&
                (instance->targets[6].card == affected_card))))))
        {
          legacy_found = true;
        }
        c = c + 1;
      } while (c < active_cards_count[player]);
      if (legacy_found)
        return global_enchantment(player, card, event);
    }

    instance = get_card_instance(
        player, create_targetted_legacy_activate(player, card, bludgeon_brawl_effect,
                                                 affected_card_controller, affected_card));
    instance->targets[6].player = affected_card_controller;
    instance->targets[6].card = affected_card;
    instance->targets[1].player = player;
    instance->targets[1].card = card;
    instance->number_of_targets = '\x01';
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x226d1a0; original cards/new_phyrexia.c:492. */
static int deal_6_damage_legacy(int player, int card, event_t event)
{
  int p;
  int player_00;
  int card_00;
  card_instance_t *instance;
  int source_card;
  int dying_or_removed;
  int remaining_hits;

  instance = get_card_instance(player, card);
  if ((((event == EVENT_GRAVEYARD_FROM_PLAY) &&
        (source_card = instance->targets[0].card, source_card == affected_card)) &&
       (player_00 = instance->targets[0].player, player_00 == affected_card_controller)) &&
      ((dying_or_removed = graveyard_from_play(player_00, source_card, EVENT_GRAVEYARD_FROM_PLAY),
        dying_or_removed != 0 &&
            (dying_or_removed = check_for_death_trigger_removal(player_00, source_card),
             dying_or_removed == 0))))
  {

    p = instance->targets[1].player;
    card_00 = instance->targets[1].card;
    instance = in_play(p, card_00);
    if (instance != NULL)
    {

      if (is_what(player_00, source_card, 2) == 0)
      {
        damage_creature(p, card_00, 6, player_00, source_card);
      }
      else
      {
        remaining_hits = card_counts[player_00][CCOUNTS_TEYSA_KARLOV] + 1;
        do
        {
          damage_creature(p, card_00, 6, player_00, source_card);
          remaining_hits = remaining_hits + -1;
        } while (remaining_hits != 0);
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  if (end_of_combat_trigger(player, card, event, 2) != 0)
  {
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x226d140; original cards/new_phyrexia.c:519. */
static void cathedral_membrane_legacy(int player, int card, int t_player, int t_card)
{
  int leg;
  card_instance_t *instance;

  leg = create_targetted_legacy_effect(player, card, deal_6_damage_legacy, t_player, t_card);
  instance = get_card_instance(player, leg);
  instance->targets[0].player = player;
  instance->targets[0].card = card;
  instance->targets[1].player = t_player;
  instance->targets[1].card = t_card;
  return;
}

int card_cathedral_membrane(int player, int card, event_t event ){
  /* Cathedral Membrane |1|{WP}
   * Artifact Creature — Wall 0/3
   * ({WP} can be paid with either {W} or 2 life.)
   * Defender
   * When Cathedral Membrane dies during combat, it deals 6 damage to each creature it blocked this combat. */

	phyrexian_casting_costs(player, card, event, MANACOST_XW(1, 1), 1);
	if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card, AL_TRIG) && 1-current_turn == player && blocking(player, card, event)){
		for_each_creature_blocked_by_me(player, card, cathedral_membrane_legacy, player, card);
	}
	return 0;
}

int card_deceiver_exarch(int player, int card, event_t event ){
  /* Deceiver Exarch |2|U
   * Creature — Cleric 1/4
   * Flash (You may cast this spell any time you could cast an instant.)
   * When Deceiver Exarch enters the battlefield, choose one —
   * • Untap target permanent you control.
   * • Tap target permanent an opponent controls. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.preferred_controller = ANYBODY;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_PERMANENT", 0, 0) ){
			int p = instance->targets[0].player;
			int c = instance->targets[0].card;
			if( is_what(p, c, TYPE_LAND) ){
				ai_modifier -= 24;
			}
			if( is_what(p, c, TYPE_ENCHANTMENT | TARGET_TYPE_PLANESWALKER) ){
				ai_modifier -= 96;
			}
			if( p == player ){
				if( ! is_tapped(p, c) ){
					ai_modifier -= 96;
				}
				if( get_id(p, c) == CARD_ID_KIKI_JIKI_MIRROR_BREAKER ){
					ai_modifier += 128;
				}
				ai_modifier += (get_cmc(p, c) * 8);
			}
			else{
				if( is_tapped(p, c) ){
					ai_modifier -= 96;
				}
				ai_modifier += (get_cmc(p, c) * 8);
			}
			action_on_target(player, card, 0, p == player ? ACT_UNTAP : ACT_TAP);
		}
	}

	return flash(player, card, event);
}

int card_dismember(int player, int card, event_t event ){
/* CARD_ID_DISMEMBER	11592
Dismember	|1|PB|PB
Instant
(|PB can be paid with either |B or 2 life.)
Target creature gets -5/-5 until end of turn. */

	if (event == EVENT_CHECK_PUMP )
	{
		return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, -5, -5, 0, 0);
	}

	if( event == EVENT_MODIFY_COST && affect_me(player, card) ){
		return phyrexian_casting_costs(player, card, event, MANACOST_XB(1, 2), 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int life_before = life[player];
		int result = 1;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			result = charge_phyrexian_mana(player, card, event, MANACOST_XB(1, 2), 0);
		}
		if( result ){
			if( life[player] < life_before ){
				td.allow_cancel = 0;
			}
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -5, -5);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	 return 0;
}

int card_dispatch(int player, int card, event_t event ){
  /* Dispatch	|W
   * Instant
   * Tap target creature.
   * Metalcraft - If you control three or more artifacts, exile that creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += ((current_phase < PHASE_DECLARE_BLOCKERS || metalcraft(player, card)) ? 24 : -24);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
			if( metalcraft(player, card) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_elesh_norn(int player, int card, event_t event ){
  /* Elesh Norn	|2|W|W
   * Legendary Creature - Phyrexian Praetor 3/5
   * Vigilance
   * Whenever a source an opponent controls deals damage to you or a permanent you control, that source's controller loses 2 life unless they pay |1.
   * |2|W, Sacrifice three other creatures: Exile ~, then return it to the battlefield transformed under its owner's control. Activate only as a sorcery. */

	check_legend_rule(player, card, event);

	vigilance(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) ){
			if( affect_me(player, card) ){
				return 0;
			}
			if( affected_card_controller == player ){
				event_result+=2;
			}
			else{
				 event_result-=2;
			}
		}
	}

	return 0;
}
int card_fresh_meat(int player, int card, event_t event){
	/* Fresh Meat	|3|G
	 * Instant
	 * Put a 3/3 |Sgreen Beast creature token onto the battlefield for each creature put into your graveyard from the battlefield this turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_BEAST, get_dead_count(player, TYPE_CREATURE));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_geths_verdict(int player, int card, event_t event ){
  /* Geth's Verdict	|B|B
   * Instant
   * Target player sacrifices a creature of their choice and loses 1 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			new_sacrifice(player, card, instance->targets[0].player, SAC_CAUSED | SAC_NO_CANCEL, &test);
			lose_life(instance->targets[0].player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_gitaxian_probe(int player, int card, event_t event ){
  /* Gitaxian Probe	|UP
   * Sorcery
   * (|UP can be paid with either |U or 2 life.)
   * Look at target player's hand.
   * Draw a card. */

	if( event == EVENT_MODIFY_COST ){
		return phyrexian_casting_costs(player, card, event, MANACOST_U(1), 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int result = 1;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			result = charge_phyrexian_mana(player, card, event, MANACOST_U(1), 0);
		}
		if( ! result ){
			spell_fizzled = 1;
			return 0;
		}
		ai_modifier += 24;
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}


	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if( player != instance->targets[0].player ){
				reveal_target_player_hand(instance->targets[0].player);
			}
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static const char* target_must_have_counters(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  return count_counters(player, card, -1) ? NULL : "must have counters";
}

int card_hex_parasite(int player, int card, event_t event)
{
  /* Hex Parasite	|1
   * Artifact Creature - Insect 1/1
   * |X|PB: Remove up to X counters from target permanent. For each counter removed this way, ~ gets +1/+0 until end of turn. */

  if (event == EVENT_POW_BOOST)
  {
    int mana = has_mana(player, 7, 1);
    int counters = count_counters_by_counter_and_card_type(player, ~COUNTER_P1_P1, TYPE_PERMANENT);
    return counters < mana ? counters : mana;
  }
  if ((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) > 1 && event != EVENT_ACTIVATE)
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  target_definition_t td;
  default_target_definition(player, card, &td, 0x47);
  td.preferred_controller = 2;
  bool ai = ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0);
  if (ai && event != EVENT_RESOLVE_ACTIVATION)
  {
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (int32_t)target_must_have_counters;
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (!has_phyrexian_mana_for_activated_ability(player, card, ai ? 1 : 0, 1, 0, 0, 0, 0))
      return 0;
    return can_target(&td) != 0;
  }
  if (event == EVENT_ACTIVATE)
  {
    instance->number_of_targets = 0;
    int selected = pick_target(&td, "TARGET_PERMANENT");
    int saved_max = max_x_value;
    if (!selected)
      return 0;
    if (ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0))
      max_x_value =
          count_counters(instance->targets[0].player, instance->targets[0].card, ~COUNTER_P1_P1);
    charge_phyrexian_mana_for_activated_ability(player, card, -1, 1, 0, 0, 0, 0);
    max_x_value = saved_max;
    if (spell_fizzled == 1)
    {
      instance->number_of_targets = 0;
      return 0;
    }
    instance->info_slot = x_value;
    return 0;
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
    return 0;
  if (valid_target(&td))
  {
    int removed = 0;
    if (instance->info_slot > 0)
    {
      do
      {
        if (!count_counters(instance->targets[0].player, instance->targets[0].card, ~COUNTER_P1_P1))
          break;
        counter_t type = choose_existing_counter_type(
            player, player, card, instance->targets[0].player, instance->targets[0].card,
            CECT_REMOVE | CECT_CONSIDER_ALL | CECT_AI_CAN_CANCEL | CECT_HUMAN_CAN_CANCEL, -1, -1);
        if (type == COUNTER_invalid)
          break;
        ++removed;
        remove_counters(player, card, instance->targets[0].player, instance->targets[0].card, type,
                        1);
        uint32_t remaining = (uint32_t)instance->info_slot - 1U;
        instance->info_slot = (int32_t)remaining;
        if ((int32_t)remaining <= 0)
          break;
      } while (true);
      if (removed > 0 && in_play(instance->parent_controller, instance->parent_card))
      {
        spell_fizzled = 0;
        pump_until_eot_merge_previous(player, card, instance->parent_controller,
                                      instance->parent_card, removed, 0);
        return 0;
      }
    }
    spell_fizzled = 1;
  }
  return 0;
}

int card_jin_gitaxias_core_augur(int player, int card, event_t event){

	/* Jin-Gitaxias, Core Augur	|8|U|U
	 * Legendary Creature - Praetor 5/4
	 * Flash
	 * At the beginning of your end step, draw seven cards.
	 * Each opponent's maximum hand size is reduced by seven. */

	check_legend_rule(player, card, event);

	if( current_turn == player && eot_trigger(player, card, event) ){
		draw_cards(player, 7);
		if( hand_count[player] > 7 ){
			multidiscard(player, hand_count[player] - 7, 0);
		}
	}

	if( event == EVENT_MAX_HAND_SIZE && in_play(player, card) && current_turn == 1-player && ! is_humiliated(player, card, AL_STATIC) ){
		event_result-=7;
	}

	return flash(player, card, event);
}

int card_karn_liberated(int player, int card, event_t event)
{
  /* Karn Liberated	|7
   * Legendary Planeswalker - Karn (6)
   * +4: Target player exiles a card from their hand.
   * -3: Exile target permanent.
   * -14: Restart the game, leaving in exile all non-Aura permanent cards exiled with Karn. Then put those cards onto the battlefield under your control. */

  enum
  {
    CHOICE_EXILE_CARD_IN_HAND = 1,
    CHOICE_EXILE_PERMANENT = 2,
    CHOICE_RESTART_GAME = 3
  };

  typedef struct
  {
    int edh_card;
    int general1_id;
    int general2_id;
    int8_t general1_ebk;
    int8_t general2_ebk;
  } edh_data_t;
  STATIC_ASSERT(sizeof(edh_data_t) == 16, karn_native_commander_snapshot_bytes);
  STATIC_ASSERT(offsetof(edh_data_t, general1_ebk) == 12,
                karn_native_commander_flag_offset);
  int exiled_by_karn[2][151];

  int32_t *commander_flags;
  int *exile_end;
  unsigned short csvid;
  int32_t first_commander_iid;
  bool plane_removed;
  uint8_t card_category;
  int value;
  int owner_or_opponent;
  int result;
  int32_t selected_card;
  card_instance_t *record;
  card_instance_t *commander;
  card_instance_t *selected;
  card_instance_t *instance;
  unsigned int *remembered_entry;
  int state_or_saved_turn;
  int original_iid;
  unsigned char commander_bit;
  unsigned char commander_bit_base;
  uint32_t hand_priority;
  unsigned int turn_or_csvid;
  unsigned int restart_player;
  int *exile_cursor;
  bool commander_exiled;
  unsigned int pass;
  int exile_source_player;
  unsigned int other_pass;
  int legacy;
  int entry_index;
  int exiled_count[2];
  edh_data_t commanders[2];
  target_definition_t player_target;
  target_definition_t permanent_target;
  test_definition_t hand_test;
  commander_bit = 0;
  check_legend_rule(player, card, event);
  if (event == EVENT_RESOLVE_ACTIVATION || event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &player_target, 0);
    player_target.zone = TARGET_ZONE_PLAYERS;
    default_target_definition(player, card, &permanent_target, 0x47);
    value = can_target(&permanent_target);
    owner_or_opponent = 1 - player;
    result = would_validate_arbitrary_target(&player_target, owner_or_opponent, -1);
    hand_priority = 0;
    if ((result != 0) && (hand_priority = 0, hand_count[owner_or_opponent] != 0))
    {
      hand_priority = 0xf;
    }
    result = can_target(&player_target);
    value =
        dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, "Exile a card in target's hand",
                    result, hand_priority, 4, "Exile a permanent", value, 10, 0xfffffffd,
                    "Restart the game", 1, 0x14, 0xfffffff2, 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (value == 0)
      {
        return 0;
      }
      return planeswalker(player, card, event, 6);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      if (value == CHOICE_EXILE_PERMANENT)
      {
        pick_target(&permanent_target, "TARGET_PERMANENT");
      }
      else if (value == CHOICE_RESTART_GAME)
      {
        value = count_counters(player, card, COUNTER_LOYALTY);
        if ((value < 0xf) || (instance->kill_code != '\0'))
        {
          value = exiledby_detach(player, card);
          instance->targets[1].card = value;
        }
      }
      else if (value == CHOICE_EXILE_CARD_IN_HAND)
      {
        pick_target(&player_target, "TARGET_PLAYER");
      }
    }
    else if (value == CHOICE_EXILE_PERMANENT)
    {
      if (valid_target(&permanent_target) != 0)
      {
        value = is_token(instance->targets[0].player, instance->targets[0].card);
        if ((value == 0) &&
            (record = in_play(instance->parent_controller, instance->parent_card), record != NULL))
        {
          if (check_special_flags2(instance->targets[0].player, instance->targets[0].card,
                                   0x1000000) == 0)
          {
            selected_card = instance->targets[0].card;
            exile_card_and_remember_it_on_exiledby(instance->parent_controller,
                                                   instance->parent_card,
                                                   instance->targets[0].player, selected_card);
          }
          else
          {
            record = get_card_instance(instance->parent_controller, instance->parent_card);
            commander = get_card_instance(instance->targets[0].player, instance->targets[0].card);
            value = commander->internal_card_id;
            owner_or_opponent = get_owner(instance->targets[0].player, instance->targets[0].card);
            result = locate_edh_card(owner_or_opponent);
            commander = get_card_instance(owner_or_opponent, result);
            selected_card = commander->targets[2].card;
            record->targets[owner_or_opponent + 5].player = commander->targets[2].player;
            record->targets[owner_or_opponent + 5].card = selected_card;
            if (record->targets[7].card == -1)
            {
              record->targets[7].card = 0;
            }
            selected_card = instance->targets[0].card;
            if ((selected_card == commander->targets[4].player) ||
                (commander->targets[2].player == value))
            {
              kill_card(instance->targets[0].player, selected_card, KILL_EXILE);
              result = check_rfg(owner_or_opponent, (unsigned int)cards_data[value].id);
              if ((result != 0) && ((char)commander->targets[3].player == '\x04'))
              {
                exiledby_remember(instance->parent_controller, instance->parent_card,
                                  owner_or_opponent, value, NULL, NULL);
                commander_flags = &record->targets[7].card;
                *commander_flags =
                    *commander_flags | 1 << ((char)owner_or_opponent * '\x02' & 0x1fU);
              }
            }
            else
            {
              if ((selected_card != commander->targets[4].card) &&
                  (commander->targets[2].card != value))
              {
                exile_card_and_remember_it_on_exiledby(instance->parent_controller,
                                                       instance->parent_card,
                                                       instance->targets[0].player, selected_card);
              }
              else
              {
                kill_card(instance->targets[0].player, selected_card, KILL_EXILE);
                result = check_rfg(owner_or_opponent, (unsigned int)cards_data[value].id);
                if ((result != 0) && ((char)SET_BYTE1(commander->targets[3].player) == '\x04'))
                {
                  exiledby_remember(instance->parent_controller, instance->parent_card,
                                    owner_or_opponent, value, NULL, NULL);
                  commander_flags = &record->targets[7].card;
                  *commander_flags =
                      *commander_flags | 1 << ((char)owner_or_opponent * '\x02' + 1U & 0x1f);
                }
              }
            }
          }
        }
        else
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
        }
      }
    }
    else if (value == CHOICE_RESTART_GAME)
    {
      exiled_count[0] = 0;
      memset(exiled_by_karn, 0xff, sizeof(exiled_by_karn));
      exiled_count[1] = 0;
      value = instance->targets[1].card;
      if (value < 0)
      {
        exile_source_player = instance->parent_controller;
        value = instance->parent_card;
      }
      else
      {
        exile_source_player = player + -2;
      }
      legacy = 0;
      entry_index = 0;
      while (remembered_entry = (unsigned int *)exiledby_find_any(exile_source_player, value,
                                                                  &legacy, &entry_index),
             remembered_entry != NULL)
      {
        turn_or_csvid = *remembered_entry;
        *remembered_entry = 0xffffffff;
        state_or_saved_turn = (int)turn_or_csvid >> 0x1f;
        result = exiled_count[-state_or_saved_turn];
        exiled_count[-state_or_saved_turn] = result + 1;
        exiled_by_karn[-state_or_saved_turn][result] = turn_or_csvid & 0x7fffffff;
      }
      value = instance->targets[1].card;
      if (-1 < value)
      {
        exiledby_destroy_detached(player, value);
      }
      stack_size = 0;
      value = 0;
      do
      {
        result = stack_cards[value].player;
        if (((result != -1) &&
             (state_or_saved_turn = stack_cards[value].card, state_or_saved_turn != -1)) &&
            ((state_or_saved_turn != card || (result != player))))
        {
          result = check_state(result, state_or_saved_turn, 0x10000000);
          if (result == 0)
          {
            kill_card(stack_cards[value].player, stack_cards[value].card, KILL_EXILE);
          }
          else
          {
            add_state(stack_cards[value].player, stack_cards[value].card, 0x40000000);
          }
        }
        stack_phase[value] = -1;
        value = value + 1;
      } while (value != 0x20);
      memset(commanders, 0xff, sizeof(commanders));
      pass = 0;
      plane_removed = false;
      turn_or_csvid = current_turn;
      do
      {
        restart_player = pass ^ turn_or_csvid;

        value = active_cards_count[restart_player] - 1;
        if (value >= 0)
        {
          commander_bit_base = (char)restart_player * 2;
          for (; value >= 0; --value)
          {
            if (in_play(restart_player, value) && has_subtype(restart_player, value, SUBTYPE_AURA))
              put_on_top_of_deck(restart_player, value);
            else if (get_id(restart_player, value) == CARD_ID_ELDER_DRAGON_HIGHLANDER)
            {
              commanders[restart_player].edh_card = value;
              remove_all_counters(restart_player, value, restart_player, value, ~COUNTER_P1_P1);
              record = get_card_instance(restart_player, value);
              csvid = cards_data[record->targets[2].player].id;
              commanders[restart_player].general1_id = (unsigned int)csvid;
              commanders[restart_player].general2_id =
                  (unsigned int)cards_data[record->targets[2].card].id;
              turn_or_csvid = instance->targets[7].card;
              if (0 < (int)turn_or_csvid)
              {
                commander_exiled = false;
                if (((1 << (commander_bit_base & 0x1f) & turn_or_csvid) != 0) &&
                    ((char)record->targets[3].player == '\x04'))
                {
                  result = check_rfg(restart_player, (unsigned int)csvid);
                  commander_exiled = result != 0;
                  turn_or_csvid = instance->targets[7].card;
                }
                commanders[restart_player].general1_ebk = commander_exiled;
                commander_exiled = false;
                if (((1 << (commander_bit_base + 1 & 0x1f) & turn_or_csvid) != 0) &&
                    ((char)SET_BYTE1(record->targets[3].player) == '\x04'))
                {
                  result = check_rfg(restart_player, commanders[restart_player].general2_id);
                  commander_exiled = result != 0;
                }
                commanders[restart_player].general2_ebk = commander_exiled;
                instance->targets[7].card = 0;
              }
              *(uint8_t *)&record->targets[3].player = 1;

              if (commanders[restart_player].general2_id >= 0)
                SET_BYTE1(record->targets[3].player) = 1;
            }
          }
          value = active_cards_count[restart_player] - 1;
          turn_or_csvid = current_turn;
          while (value >= 0)
          {
            if (in_hand(restart_player, value))
            {
              if (commanders[restart_player].edh_card == -1 ||
                  (result = get_id(restart_player, value),
                   commanders[restart_player].general1_id != result &&
                       commanders[restart_player].general2_id != result))
                put_on_top_of_deck(restart_player, value);
              else
              {
                obliterate_card(restart_player, value);
                --value;
                add_csvid_to_rfg(restart_player, result);
                turn_or_csvid = current_turn;
                continue;
              }
            }
            else if (in_play(restart_player, value) && card != value)
            {
              result = get_original_internal_card_id(restart_player, value);
              csvid = cards_data[result].id;
              state_or_saved_turn = is_what(restart_player, value, 0x80);
              card_category = cards_data[result].cc[2];
              if (state_or_saved_turn && card_category != 'c' && card_category != '\x03' &&
                  csvid != 0x394)
              {
                record = get_card_instance(restart_player, value);
                if (record->info_slot_fn != companion_effect)
                  kill_card(restart_player, value, KILL_EXILE);
              }
              else if (card_category == '\x03')
              {
                if (cards_data[result].id != CARD_ID_ELDER_DRAGON_HIGHLANDER)
                {
                  if (!is_plane_card((unsigned int)csvid))
                    remove_all_counters(restart_player, value, restart_player, value,
                                        ~COUNTER_P1_P1);
                  else
                  {
                    kill_card(restart_player, value, KILL_EXILE);
                    plane_removed = true;
                  }
                }
              }
              else if (is_what(restart_player, value, 0x47))
              {
                if (!check_special_flags2(restart_player, value, 0x1000000))
                  put_on_top_of_deck(restart_player, value);
                else
                  obliterate_card(restart_player, value);
              }
            }
            --value;
            turn_or_csvid = current_turn;
          }
        }
        pass = pass + 1;
      } while (pass != 2);
      reshuffle_grave_into_deck(turn_or_csvid, 1);
      reshuffle_grave_into_deck(current_turn ^ 1, 1);
      value = current_turn;
      result = get_starting_life_total(current_turn);
      life[value] = result;
      raw_set_poison(value, '\0');
      raw_set_experience(value, '\0');
      turn_or_csvid = current_turn ^ 1;
      // Keep the native player-indexed energy reset on both sides.
      player_counters[player].energy_ = 0;
      value = get_starting_life_total(turn_or_csvid);
      life[turn_or_csvid] = value;
      raw_set_poison(turn_or_csvid, '\0');
      raw_set_experience(turn_or_csvid, '\0');
      other_pass = 0;
      player_counters[player].energy_ = 0;
      do
      {
        turn_or_csvid = other_pass ^ current_turn;
        value = exiled_count[turn_or_csvid];
        if (0 < value)
        {
          exile_cursor = exiled_by_karn[turn_or_csvid];
          do
          {
            result = is_what(-1, *exile_cursor, 0x47);
            if ((result == 0) ||
                (result = has_subtype_by_id(player, (unsigned int)cards_data[*exile_cursor].id,
                                            SUBTYPE_AURA),
                 result != 0))
            {
              remove_card_from_rfg(turn_or_csvid, (unsigned int)cards_data[*exile_cursor].id);
              raw_put_iid_on_top_of_deck(turn_or_csvid, *exile_cursor);
              *exile_cursor = -1;
            }
            exile_cursor = exile_cursor + 1;
          } while ((exiled_by_karn[turn_or_csvid] + value) != exile_cursor);
        }
        other_pass = other_pass + 1;
      } while (other_pass != 2);
      stack_size = 0;
      value = 0;
      do
      {
        result = stack_cards[value].player;
        if (((result != -1) &&
             (state_or_saved_turn = stack_cards[value].card, state_or_saved_turn != -1)) &&
            ((result != player || (state_or_saved_turn != card))))
        {
          result = check_state(result, state_or_saved_turn, 0x10000000);
          if (result == 0)
          {
            kill_card(stack_cards[value].player, stack_cards[value].card, KILL_EXILE);
          }
          else
          {
            add_state(stack_cards[value].player, stack_cards[value].card, 0x40000000);
          }
        }
        stack_phase[value] = -1;
        value = value + 1;
      } while (value != 0x20);
      pass = 0;
      turn_or_csvid = current_turn;
      do
      {
        restart_player = pass ^ turn_or_csvid;
        if (0 < exiled_count[restart_player])
        {
          exile_cursor = exiled_by_karn[restart_player];
          exile_end = (exiled_by_karn[restart_player] + exiled_count[restart_player]);

          do
          {
            value = *exile_cursor;
            // Native code loads this packed word before checking the sentinel.
            turn_or_csvid = (uint16_t)cards_data[value].id |
                            0;
            *exile_cursor = -1;
            if (value >= 0)
            {
              turn_or_csvid &= 0xffff;
              if (check_rfg(restart_player, turn_or_csvid))
              {
                value = add_card_to_hand(player, value);
                if (player != restart_player)
                {
                  if (player == 1)
                    remove_state(1, value, 0x1000);
                  else
                    add_state(player, value, 0x1000);
                }
                remove_card_from_rfg(restart_player, turn_or_csvid);
                bool commander_returned = false;
                if ((commanders[0].edh_card & commanders[1].edh_card) != 0xffffffff)
                {
                  if ((commanders[player].general1_id == turn_or_csvid) &&
                      (commanders[player].general1_ebk != '\0'))
                  {
                    instance = get_card_instance(player, commanders[player].edh_card);
                    *(uint8_t *)&instance->targets[3].player = 4;
                    commander_enters_the_battlefield(player, commanders[player].edh_card, player,
                                                     value, 1);
                    put_into_play(player, value);
                    commanders[player].general1_ebk = -1;
                    commander_returned = true;
                  }
                  else if ((commanders[owner_or_opponent].general1_id == turn_or_csvid) &&
                           (commanders[owner_or_opponent].general1_ebk != '\0'))
                  {
                    instance = get_card_instance(owner_or_opponent,
                                                 commanders[owner_or_opponent].edh_card);
                    *(uint8_t *)&instance->targets[3].player = 4;
                    commander_enters_the_battlefield(owner_or_opponent,
                                                     commanders[owner_or_opponent].edh_card, player,
                                                     value, 1);
                    put_into_play(player, value);
                    commanders[owner_or_opponent].general1_ebk = -1;
                    commander_returned = true;
                  }
                  else if ((commanders[player].general2_id == turn_or_csvid) &&
                           (commanders[player].general2_ebk != '\0'))
                  {
                    instance = get_card_instance(player, commanders[player].edh_card);
                    SET_BYTE1(instance->targets[3].player) = 4;
                    commander_enters_the_battlefield(player, commanders[player].edh_card, player,
                                                     value, 2);
                    put_into_play(player, value);
                    commanders[player].general2_ebk = -1;
                    commander_returned = true;
                  }
                  else if (commanders[owner_or_opponent].general2_id == turn_or_csvid &&
                           commanders[owner_or_opponent].general2_ebk != '\0')
                  {
                    instance = get_card_instance(owner_or_opponent,
                                                 commanders[owner_or_opponent].edh_card);
                    SET_BYTE1(instance->targets[3].player) = 4;
                    commander_enters_the_battlefield(owner_or_opponent,
                                                     commanders[owner_or_opponent].edh_card, player,
                                                     value, 2);
                    put_into_play(player, value);
                    commanders[owner_or_opponent].general2_ebk = -1;
                    commander_returned = true;
                  }
                }

                if (!commander_returned)
                {
                  ++exile_cursor;
                  put_into_play(player, value);
                  turn_or_csvid = current_turn;
                  continue;
                }
              }
            }
            ++exile_cursor;
            turn_or_csvid = current_turn;
          } while (exile_end != exile_cursor);
        }
        pass = pass + 1;
      } while (pass != 2);
      // Native cleanup starts at the returned count, inclusive.
      value = count_rfg(turn_or_csvid);
      if (-1 < value)
      {
        do
        {
          owner_or_opponent = value + -1;
          remove_card_from_rfg_by_position(turn_or_csvid, value);
          value = owner_or_opponent;
        } while (owner_or_opponent != -1);
      }
      turn_or_csvid = current_turn ^ 1;
      value = count_rfg(turn_or_csvid);
      if (-1 < value)
      {
        do
        {
          owner_or_opponent = value + -1;
          remove_card_from_rfg_by_position(turn_or_csvid, value);
          value = owner_or_opponent;
        } while (owner_or_opponent != -1);
      }
      other_pass = 0;
      do
      {
        turn_or_csvid = other_pass ^ current_turn;
        value = 7;
        shuffle(turn_or_csvid);
        owner_or_opponent = active_cards_count[turn_or_csvid] + -1;

        while (owner_or_opponent >= 0)
        {
          original_iid = get_original_internal_card_id(turn_or_csvid, owner_or_opponent);
          state_or_saved_turn = current_turn;
          result = event_result;
          current_turn = state_or_saved_turn;
          if (cards_data[original_iid].cc[2] == '\x03')
          {
            current_turn = turn_or_csvid;
            call_card_function(turn_or_csvid, owner_or_opponent, EVENT_MAX_HAND_SIZE);
            if (event_result - 1U < 0x62)
              value = event_result - 7 + value;
            --owner_or_opponent;
            event_result = result;
            current_turn = state_or_saved_turn;
          }
          else
          {
            --owner_or_opponent;
            event_result = result;
          }
        }
        draw_cards(turn_or_csvid, value);
        other_pass = other_pass + 1;
      } while (other_pass != 2);
      memset(mana_pool, 0, 0x40);
      cards_drawn_this_turn[0] = 0;
      cards_drawn_this_turn[1] = 0;
      land_can_be_played = land_can_be_played & 0xfffffffe;
      turn_count = 0;
      lands_played = 0;
      game_startup();
      turn_or_csvid = 0;
      do
      {
        restart_player = current_turn ^ turn_or_csvid;
        value = active_cards_count[restart_player] + -1;
        if (-1 < value)
        {
          do
          {
            instance = in_play(restart_player, value);
            if (instance != NULL)
            {
              instance = get_card_instance(restart_player, value);
              instance->state = instance->state & 0xfffcffff;
            }
            value = value + -1;
          } while (value != -1);
        }
        turn_or_csvid = turn_or_csvid + 1;
      } while (turn_or_csvid != 2);
      if (plane_removed)
      {
        value = get_new_plane(-1);
        planeswalk_to(player, value, 1);
      }
      if (current_turn == player)
      {
        untap_phase(player);
        upkeep_phase(player);
        current_phase = 0x14;
        update_duel_selection_display_if_human(player, 0x14);
      }
      else
      {
        end_the_turn(player, card, event);
      }
    }
    else if (((value == CHOICE_EXILE_CARD_IN_HAND) && (value = valid_target(&player_target), value != 0)) &&
             (value = instance->targets[0].player, hand_count[value] != 0))
    {
      if (value == 0)
      {
        default_test_definition(&hand_test, 0xff);
        selected_card = instance->targets[0].player;
        selected_card = new_select_a_card(selected_card, selected_card, 4, 1, 3, -1, &hand_test);
      }
      else
      {
        default_test_definition(&hand_test, 0x47);
        value = instance->targets[0].player;
        hand_test.type_flag = 1;
        selected_card = new_select_a_card(value, value, 4, 1, 3, -1, &hand_test);
        if (selected_card == -1)
        {
          default_test_definition(&hand_test, 0xff);
          selected_card = instance->targets[0].player;
          selected_card = new_select_a_card(selected_card, selected_card, 4, 1, 3, -1, &hand_test);
        }
      }
      record = in_play(instance->parent_controller, instance->parent_card);
      if (record == NULL)
      {
        rfg_card_in_hand(instance->targets[0].player, selected_card);
      }
      else
      {
        value = locate_edh_card(instance->targets[0].player);
        if (value != -1)
        {
          record = get_card_instance(instance->parent_controller, instance->parent_card);
          commander = get_card_instance(instance->targets[0].player, value);
          value = instance->targets[0].player;
          first_commander_iid = commander->targets[2].player;
          record->targets[value + 5].card = commander->targets[2].card;
          record->targets[value + 5].player = first_commander_iid;
          if (record->targets[7].card == -1)
          {
            record->targets[7].card = 0;
          }
          selected = get_card_instance(instance->targets[0].player, selected_card);
          value = instance->targets[0].player;
          if ((selected->internal_card_id == commander->targets[2].player) &&
              ((char)commander->targets[3].player == '\x04'))
          {
            commander_flags = &record->targets[7].card;
            *commander_flags = *commander_flags | 1 << ((char)value * '\x02' & 0x1fU);
          }
          else
          {
            selected = get_card_instance(value, selected_card);
            if ((selected->internal_card_id == commander->targets[2].card) &&
                ((char)SET_BYTE1(commander->targets[3].player) == '\x04'))
            {
              commander_flags = &record->targets[7].card;
              *commander_flags =
                  *commander_flags | 1 << ((char)instance->targets[0].player * '\x02' + 1U & 0x1f);
            }
          }
        }
        exile_card_and_remember_it_on_exiledby(instance->parent_controller, instance->parent_card,
                                               instance->targets[0].player, selected_card);
      }
    }
  }
  if ((((event == EVENT_CAST_SPELL) &&
        ((card != affected_card || (player != affected_card_controller)))) &&
       (instance = in_play(player, card),
        -1 < instance->targets[affected_card_controller + 5].player)) &&
      (-1 < instance->targets[7].card))
  {
    value = get_original_internal_card_id(affected_card_controller, affected_card);
    if (value == instance->targets[affected_card_controller + 5].player)
    {
      turn_or_csvid = instance->targets[7].card;
      commander_bit = (char)affected_card_controller * '\x02';
      if (((int)turn_or_csvid >> (commander_bit & 0x1f) & 1U) != 0)
      {
        commander_bit = commander_bit & 0x1f;
        instance->targets[7].card = turn_or_csvid & ~(1u << commander_bit);
        return planeswalker(player, card, event, 6);
      }
    }
    if (value == instance->targets[affected_card_controller + 5].card)
    {
      turn_or_csvid = instance->targets[7].card;
      commander_bit = (char)affected_card_controller * '\x02' + 1;
      if (((int)turn_or_csvid >> (commander_bit & 0x1f) & 1U) != 0)
      {
        commander_bit = commander_bit & 0x1f;
        instance->targets[7].card = turn_or_csvid & ~(1u << commander_bit);
      }
    }
  }
  value = planeswalker(player, card, event, 6);
  return value;
}

int card_lashwrithe(int player, int card, event_t event){

	/* Lashwrithe	|4
	 * Artifact - Equipment
	 * Living weapon
	 * Equipped creature gets +1/+1 for each |H2Swamp you control.
	 * Equip |PB|PB */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_GERM, &token);
		token.action = TOKEN_ACTION_EQUIP;
		generate_token(&token);
	}

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( (event== EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			event_result += count_subtype(player, TYPE_PERMANENT, SUBTYPE_SWAMP);
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_updated_equip_cost(player, card, get_cost_mod_for_activated_abilities(player, card, 0, 2, 0, 0, 0, 0));
		if( has_phyrexian_mana(player, MANACOST_XB(c1, 2), 0) && basic_equipment(player, card, event, 0) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		int c1 = get_updated_equip_cost(player, card, get_cost_mod_for_activated_abilities(player, card, 0, 2, 0, 0, 0, 0));
		int good = 0;
		if( metalcraft(player, card) && check_battlefield_for_id(player, CARD_ID_PURESTEEL_PALADIN) ){
			good = 1;
		}
		if( good != 1 ){
			charge_phyrexian_mana( player, card, event, MANACOST_XB(c1, 2), 0);
		}
		if( spell_fizzled != 1 ){
			return basic_equipment(player, card, event, 0);
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		return basic_equipment(player, card, event, 0);
	}

	return 0;
}

int card_immolating_souleater(int player, int card, event_t event);
int card_master_splicer(int player, int card, event_t event ){
	/* Master Splicer	|3|W
	 * Creature - Human Artificer 1/1
	 * When ~ enters the battlefield, put a 3/3 colorless Golem artifact creature token onto the battlefield.
	 * Golem creatures you control get +1/+1. */

	return generic_splicer(player, card, event, 1, 1, 0, 1);
}

int card_melira_sylvok_outcast(int player, int card, event_t event ){
  /* Melira, Sylvok Outcast	|1|G
   * Legendary Creature - Human Scout 2/2
   * You can't get poison counters.
   * Creatures you control can't have -1/-1 counters put on them.
   * Creatures your opponents control lose infect. */

	check_legend_rule(player, card, event);

	card_instance_t *instance = get_card_instance(player, card);

	// Deal with Infect
	if(event == EVENT_ABILITIES && affected_card_controller == 1-player &&
		is_what(affected_card_controller, affected_card, TYPE_CREATURE) && !is_humiliated(player, card, AL_STATIC) )
	{
		event_result &= ~KEYWORD_INFECT;
	}

	// Deal with Poison Counters
	if( event == EVENT_RESOLVE_SPELL ){
		instance->targets[9].player = POISON_COUNTERS(player);
	}

	if( instance->targets[9].player > -1 && POISON_COUNTERS(player) != instance->targets[9].player  ){
		if( POISON_COUNTERS(player) > instance->targets[9].player && !is_humiliated(player, card, AL_STATIC) ){
			raw_set_poison(player, instance->targets[9].player);
		}
		else{
			 instance->targets[9].player = POISON_COUNTERS(player);
		}
	}

	// The interaction with -1/-1 counters is coded in counters.c

	return 0;
}

static const char* cmc_is_1(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	if( get_cmc(player, card) == 1 ){
		return NULL;
	}
	return "must be a spell with CMC 1";
}

int card_mental_misstep(int player, int card, event_t event ){
  /* Mental Misstep	|UP
   * Instant
   * (|UP can be paid with either |U or 2 life.)
   * Counter target spell with mana value 1. */

	if( event == EVENT_MODIFY_COST ){
		return phyrexian_casting_costs(player, card, event, MANACOST_U(1), 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(cmc_is_1);

	if( event == EVENT_CAN_CAST ){
		return counterspell(player, card, event, &td, 0);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		phyrexian_casting_costs(player, card, event, MANACOST_U(1), 0);
		if( spell_fizzled != 1 ){
			return counterspell(player, card, event, &td, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		return counterspell(player, card, event, &td, 0);
	}

	return 0;
}

int card_moltensteel_dragon(int player, int card, event_t event)
{
  /* Moltensteel Dragon	|4|PR|PR
   * Artifact Creature - Dragon 4/4
   * Flying
   * |PR: ~ gets +1/+0 until end of turn. */

  phyrexian_casting_costs(player, card, event, MANACOST_XR(4, 2), 1);

  return card_immolating_souleater(player, card, event);
}

int card_norns_annex(int player, int card, event_t event ){
  /* Norn's Annex	|3|WP|WP
   * Artifact
   * (|WP can be paid with either |W or 2 life.)
   * Creatures can't attack you or planeswalkers you control unless their controller pays |WP for each of those creatures. */

	phyrexian_casting_costs(player, card, event, MANACOST_XW(3, 2), 1);

	tax_attack(player, card, event);

	return 0;
}

int card_noxious_revival(int player, int card, event_t event){
/* CARD_ID_NOXIOUS_REVIVAL	11647
Noxious Revival	|PG
Instant
(|PG can be paid with either |G or 2 life.)
Put target card from a graveyard on top of its owner's library. */

	if( event == EVENT_MODIFY_COST ){
		return phyrexian_casting_costs(player, card, event, MANACOST_G(1), 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select target card to put on top of deck.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &this_test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		     phyrexian_casting_costs(player, card, event, MANACOST_G(1), 0);
			 select_target_from_either_grave(player, card, 0, AI_MIN_VALUE, AI_MAX_VALUE, &this_test, 0, 1);
	}

	if( event == EVENT_RESOLVE_SPELL){
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			from_graveyard_to_deck(instance->targets[0].player, selected, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_phyrexian_metamorph(int player, int card, event_t event)
{
  /* Phyrexian Metamorph	|3|PU
   * Artifact Creature - Shapeshifter 0/0
   * You may have ~ enter the battlefield as a copy of any artifact or creature on the battlefield, except it's an artifact in addition to its other types. */

  phyrexian_casting_costs(player, card, event, MANACOST_XU(3,1), 1);

  target_definition_t td;
  if (event == EVENT_RESOLVE_SPELL)
	base_target_definition(player, card, &td, TYPE_CREATURE | TYPE_ARTIFACT);

  enters_the_battlefield_as_copy_of_any(player, card, event, &td, "SELECT_AN_ARTIFACT_OR_CREATURE");
  // type artifact added in cloning()

  return 0;
}

int card_phyrexian_obliterator(int player, int card, event_t event ){
  /* Phyrexian Obliterator |B|B|B|B
   * Creature — Horror 5/5
   * Trample
   * Whenever a source deals damage to Phyrexian Obliterator, that source's controller sacrifices that many permanents. */

	// An approximation - tells the AI that anything with at least 1 power that blocks this or is blocked by this will be destroyed.
	if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG)){
		get_card_instance(player, card)->destroys_if_blocked |= DIFB_ASK_CARD;
	}
	if (event == EVENT_CHECK_DESTROY_IF_BLOCKED && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG)
		&& get_power(attacking_card_controller, attacking_card) >= 1){
		event_result |= 1;
	}

	if( event == EVENT_DEAL_DAMAGE){
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && instance->info_slot != current_phase ){
			if( damage->damage_target_card == card && damage->damage_target_player == player && damage->info_slot > 0 ){
				if( instance->targets[8].player < 0 ){
					instance->targets[8].player = 0;
				}
				instance->targets[8].player++;

				int p = damage->damage_source_player;
				if( instance->targets[p+1].player < 0 ){
					instance->targets[p+1].player = 0;
				}
				instance->targets[p+1].player += damage->info_slot;
			}
		}
	}

	if( resolve_damage_trigger(player, card, event, DDBM_0, player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		APNAP(p,{
					if( instance->targets[p+1].player > 0 ){
						int amount = instance->targets[p+1].player;
						instance->targets[p+1].player = 0;

						test_definition_t test;
						default_test_definition(&test, TYPE_PERMANENT);
						test.qty = amount;
						if( amount == 1 ){
							scnprintf(test.message, 100, "Select a permanent to sacrifice.");
						}
						else{
							scnprintf(test.message, 100, "Select %d permanents to sacrifice.", amount);
						}
						new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test);
					}
				};
		);
	}


	return 0;
}

int card_phyrexian_swarmlord(int player, int card, event_t event){
  /* Phyrexian Swarmlord	|4|G|G
   * Creature - Phyrexian Insect Horror 4/4
   * Infect (This creature deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.)
   * At the beginning of your upkeep, create a 1/1 green Phyrexian Insect creature token with infect for each poison counter your opponents have. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_INSECT, &token);
		token.legacy = 1;
		token.special_code_for_legacy = &empty;
		token.special_infos = 67;
		token.qty = POISON_COUNTERS(1-player);
		generate_token(&token);
	}

	return 0;
}

int card_phyrexian_unlife(int player, int card, event_t event)
{
  /* Phyrexian Unlife	|2|W
   * Enchantment
   * You don't lose the game for having 0 or less life.
   * As long as you have 0 or less life, all damage is dealt to you as though its source had infect. (Damage is dealt to you in the form of poison counters.) */

  cannot_lose_the_game_for_having_less_than_0_life(player, card, event, player);

  card_instance_t* damage = damage_being_dealt(event), *instance;
  if (damage
	  && damage->damage_target_card == -1 && damage->damage_target_player == player
	  && (instance = in_play(player, card))
	  && instance->targets[5].player > 0)
	damage->regen_status |= KEYWORD_INFECT;

  return global_enchantment(player, card, event);
}

int card_postmortem_lunge(int player, int card, event_t event){
  /* Postmortem Lunge	|X|BP
   * Sorcery
   * (|BP can be paid with either |B or 2 life.)
   * Return target creature card with mana value X from your graveyard to the battlefield. It gains haste. Exile it at the beginning of the next end step. */

	if( event == EVENT_MODIFY_COST ){
		return phyrexian_casting_costs(player, card, event, MANACOST_B(1), 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card");

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL) ){
			int mod = true_get_updated_casting_cost(player, card, -1, event, -1);
			int count = 0;
			int iid;
			while( (iid = get_grave(player)[count]) > -1 ){
					if( is_what(-1, iid, TYPE_CREATURE) ){
						if( has_phyrexian_mana(player, card, MANACOST_XB(get_cmc_by_internal_id(iid)-mod, 1)) ){
							return 1;
						}
					}
					count++;
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int mod = true_get_updated_casting_cost(player, card, -1, event, -1);
		int available[2][100];
		int ac = 0;
		int count = 0;
		int iid;
		while( (iid = get_grave(player)[count]) > -1 ){
				if( is_what(-1, iid, TYPE_CREATURE) ){
					if( has_phyrexian_mana(player, card, MANACOST_XB(get_cmc_by_internal_id(iid)-mod, 1)) ){
						available[0][ac] = iid;
						available[1][ac] = count;
						ac++;
					}
				}
				count++;
		}
		int selected = select_card_from_zone(player, player, available[0], ac, 0, AI_MAX_CMC, -1, &this_test);
		if( selected == -1 ){
			spell_fizzled = 1;
			return 0;
		}
		instance->targets[0].player = available[1][selected];
		instance->targets[0].card = graveyard_source[player][available[1][selected]];
		charge_phyrexian_mana(player, card, event, MANACOST_XB(get_cmc_by_internal_id(available[0][selected]), 1), 0);
	}

	if( event == EVENT_RESOLVE_SPELL){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_HASTE_AND_EXILE_AT_EOT);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_puresteel_paladin(int player, int card, event_t event ){
  /* Puresteel Paladin	|W|W
   * Creature - Human Knight 2/2
   * Whenever an Equipment you control enters, you may draw a card.
   * Metalcraft - Equipment you control have equip |0 as long as you control three or more artifacts. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_EQUIPMENT;
		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_sheoldred_whispering_one(int player, int card, event_t event){
  /* Sheoldred, Whispering One	|5|B|B
   * Legendary Creature - Phyrexian Praetor 6/6
   * Swampwalk (This creature can't be blocked as long as defending player controls a Swamp.)
   * At the beginning of your upkeep, return target creature card from your graveyard to the battlefield.
   * At the beginning of each opponent's upkeep, that player sacrifices a creature of their choice. */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, 2);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( current_turn == player && count_graveyard_by_type(player, TYPE_CREATURE) > 0 && ! graveyard_has_shroud(2) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
			this_test.ai_selection_mode = AI_MAX_CMC;

			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_CMC, &this_test);
		}
		if( current_turn != player ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature to sacrifice.");
			new_sacrifice(player, card, current_turn, SAC_CAUSED | SAC_NO_CANCEL, &this_test);
		}
	}

	return 0;
}

int card_birthing_pod(int player, int card, event_t event){
/* Birthing Pod	|3|PG
 * Artifact
 * |1|PG, |T, Sacrifice a creature: Search your library for a creature card with converted mana cost equal to 1 plus the sacrificed
 *		creature's converted mana cost, put that card onto the battlefield, then shuffle your library.
 *		Activate this ability only any time you could cast a sorcery. */

	phyrexian_casting_costs(player, card, event, MANACOST_XG(3, 1), 1);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( has_phyrexian_mana_for_activated_ability(player, card, MANACOST_XG(1,1)) ){
			return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED | GAA_UNTAPPED | GAA_SACRIFICE_CREATURE |
											GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE){
		if( charge_phyrexian_mana_for_activated_ability(player, card, MANACOST_XG(1,1)) ){
			int sac = new_sacrifice(player, card, player, SAC_AS_COST | SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
			if( sac ){
				add_state(player, card, STATE_TAPPED);
				tapped_for_mana_color = -1;
				instance->info_slot = get_cmc(BYTE2(sac), BYTE3(sac));
				if( instance->info_slot < 6 ){
					ai_modifier += (instance->info_slot * 8);
				}
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int amount = instance->info_slot;

		test_definition_t test2;
		default_test_definition(&test2, TYPE_CREATURE);
		test2.cmc = amount+1;
		scnprintf(test2.message, 100, "Select a creature card with CMC %d.", amount + 1);

		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test2);
	}

	return 0;
}

int card_shrine_of_boundless_growth(int player, int card, event_t event){

	/* Shrine of Boundless Growth	|3
	 * Artifact
	 * At the beginning of your upkeep or whenever you cast a |Sgreen spell, put a charge counter on ~.
	 * |T, Sacrifice ~: Add |1 to your mana pool for each charge counter on ~. */

	np_shrines(player, card, event, COLOR_TEST_GREEN);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int amount = count_counters(player, card, COUNTER_CHARGE);
		if( amount < 3 ){
			ai_modifier -= 48;
		}
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			produce_mana_tapped(player, card, COLOR_COLORLESS, amount);
		}
	}

	if (event == EVENT_COUNT_MANA && affect_me(player, card) && mana_producer(player, card, EVENT_CAN_ACTIVATE) ){
		int counters = count_counters(player, card, COUNTER_CHARGE);
		if (counters > 0){
			declare_mana_available(player, COLOR_COLORLESS, counters);
		}
	}

	return 0;
}

int card_shrine_of_burning_rage(int player, int card, event_t event ){
	/* Shrine of Burning Rage	|2	0x2004125
	 * Artifact
	 * At the beginning of your upkeep or whenever you cast a |Sred spell, put a charge counter on ~.
	 * |3, |T, Sacrifice ~: It deals damage equal to the number of charge counters on it to any target. */
	np_shrines(player, card, event, COLOR_TEST_RED);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = count_counters(player, card, COUNTER_CHARGE);
		if( instance->info_slot < 3 ){
			ai_modifier -= 48;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_X(3), 0,
									&td, "TARGET_ANY");
}

int card_shrine_of_limitless_power(int player, int card, event_t event ){
	/* Shrine of Limitless Power	|3
	 * Artifact
	 * At the beginning of your upkeep or whenever you cast a |Sblack spell, put a charge counter on ~.
	 * |4, |T, Sacrifice ~: Target player discards a card for each charge counter on ~. */

	np_shrines(player, card, event, COLOR_TEST_BLACK);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = count_counters(player, card, COUNTER_CHARGE);
		if( instance->info_slot < 3 ){
			ai_modifier -= 48;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			new_multidiscard(instance->targets[0].player, instance->info_slot, 0, player);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_X(4), 0,
									&td, "TARGET_PLAYER");
}

int card_shrine_of_loyal_legions(int player, int card, event_t event ){
	/* Shrine of Loyal Legions	|2
	 * Artifact
	 * At the beginning of your upkeep or whenever you cast a |Swhite spell, put a charge counter on ~.
	 * |3, |T, Sacrifice ~: Put a 1/1 colorless Myr artifact creature token onto the battlefield for each charge counter on ~. */

	np_shrines(player, card, event, COLOR_TEST_WHITE);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = count_counters(player, card, COUNTER_CHARGE);
		if( instance->info_slot < 3 ){
			ai_modifier -= 48;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_MYR, &token);
		token.qty = instance->info_slot;
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(3), 0, NULL, NULL);
}

int card_shrine_of_piercing_vision(int player, int card, event_t event ){
	/* Shrine of Piercing Vision	|2
	 * Artifact
	 * At the beginning of your upkeep or whenever you cast a |Sblue spell, put a charge counter on ~.
	 * |T, Sacrifice ~: Look at the top X cards of your library, where X is the number of charge counters on ~. Put one of those cards into your hand and the
	 * rest on the bottom of your library in any order. */

	np_shrines(player, card, event, COLOR_TEST_BLUE);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = count_counters(player, card, COUNTER_CHARGE);
		if( instance->info_slot < 3 ){
			ai_modifier -= 48;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		impulse_effect(player, instance->info_slot, 0);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_spined_thopter(int player, int card, event_t event ){
  /* Spined Thopter	|2|UP
   * Artifact Creature - Phyrexian Thopter 2/1
   * (|UP can be paid with either |U or 2 life.)
   * Flying */

	phyrexian_casting_costs(player, card, event, 2, 0, 1, 0, 0, 0, 1);

	return 0;
}

int card_sword_of_war_and_peace(int player, int card, event_t event)
{
  /* Sword of War and Peace	|3
   * Artifact - Equipment
   * Equipped creature gets +2/+2 and has protection from |Sred and from |Swhite.
   * Whenever equipped creature deals combat damage to a player, ~ deals damage to that player equal to the number of cards in his or her hand and you gain 1
   * life for each card in your hand.
   * Equip |2 */

   if (equipped_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_TRACE_DAMAGED_PLAYERS))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
	  instance->targets[1].player = 0;
	  int p;
	  for (p = 0; p <= 1; ++p)
		for (; times_damaged[p] > 0; --times_damaged[p])
		  {
			damage_player(p, hand_count[p], player, card);
			gain_life(player, hand_count[player]);
		  }
	}

	return vanilla_equipment(player, card, event, 2, 2, 2, KEYWORD_PROT_RED | KEYWORD_PROT_WHITE, 0);
}

int card_surgical_extraction(int player, int card, event_t event){

	/* Surgical Extraction	|PB
	 * Instant
	 * Choose target card in a graveyard other than a basic land card. Search its owner's graveyard, hand, and library for any number of cards with the same
	 * name as that card and exile them. Then that player shuffles his or her library. */

	if( event == EVENT_MODIFY_COST ){
		phyrexian_casting_costs(player, card, event, MANACOST_B(1), 0);
	}

	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	card_instance_t *instance= get_card_instance(player, card);

	test_definition_t this_test;
	new_default_test_definition(&this_test, 0, "Select target card other than a basic land.");
	this_test.subtype = SUBTYPE_BASIC;
	this_test.subtype_flag = 1;

	if( event == EVENT_CAN_CAST ){
		if( IS_AI(player) ){
			return generic_spell(player, card, event, GS_GRAVE_RECYCLER_OPP_GRAVE, NULL, NULL, 1, &this_test);
		}
		else{
			return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &this_test);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		phyrexian_casting_costs(player, card, event, MANACOST_B(1), 0);
		if( spell_fizzled != 1 ){
			if( IS_AI(player) ){
				return generic_spell(player, card, event, GS_GRAVE_RECYCLER_OPP_GRAVE, NULL, NULL, 1, &this_test);
			}
			else{
				return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &this_test);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( IS_AI(player) ){
			int selected = validate_target_from_grave_source(player, card, 1-player, 0);
			if( selected != -1 ){
				int id = cards_data[get_grave(1-player)[selected]].id;
				lobotomy_effect(player, 1-player, id, -1, 0);
			}
		}
		else{
			int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
			if( selected != -1 ){
				const int *grave = get_grave(instance->targets[0].player);
				int id = cards_data[grave[selected]].id;
				lobotomy_effect(player, instance->targets[0].player, id, -1, 0);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_tezzerets_gambit(int player, int card, event_t event ){
  /* Tezzeret's Gambit	|3|UP
   * Sorcery
   * (|UP can be paid with either |U or 2 life.)
   * Draw two cards, then proliferate. (Choose any number of permanents and/or players, then give each another counter of each kind already there.) */

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);
		proliferate(player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return phyrexian_casting_costs(player, card, event, MANACOST_XU(3, 1), 0);
}

int card_urabrask_the_hidden(int player, int card, event_t event){

	/* Urabrask the Hidden	|3|R|R
	 * Legendary Creature - Praetor 4/4
	 * Creatures you control have haste.
	 * Creatures your opponents control enter the battlefield tapped. */

	check_legend_rule(player, card, event);

	boost_subtype(player, card, event, -1, 0,0, 0,SP_KEYWORD_HASTE, BCT_CONTROLLER_ONLY|BCT_INCLUDE_SELF);

	permanents_enters_battlefield_tapped(player, card, event, 1-player, TYPE_CREATURE, NULL);

	return 0;
}

int card_vault_skirge(int player, int card, event_t event ){
  /* Vault Skirge	|1|BP
   * Artifact Creature - Phyrexian Imp 1/1
   * (|BP can be paid with either |B or 2 life.)
   * Flying
   * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */

	phyrexian_casting_costs(player, card, event, MANACOST_XB(1, 1), 1);

	lifelink(player, card, event);

	return 0;
}

int card_vital_splicer(int player, int card, event_t event){
	/* Vital Splicer	|3|G
	 * Creature - Human Artificer 1/1
	 * When ~ enters the battlefield, put a 3/3 colorless Golem artifact creature token onto the battlefield.
	 * |1: Regenerate target Golem you control. */

	if( ! IS_GAA_EVENT(event) ){
		return generic_splicer(player, card, event, 0, 0, 0, 1);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_GOLEM;
	td.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( land_can_be_played & LCBP_REGENERATION ){
			if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td, NULL) ){
				return 99;
			}
		}
		return 0;
	}

	if( event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(1), 0,
										&td, "Select target Golem.");
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target( instance->targets[0].player, instance->targets[0].card );
		}
	}

	return 0;
}

int card_volt_charge(int player, int card, event_t event ){
	/* Volt Charge	|2|R	0x20037b6
	 * Instant
	 * ~ deals 3 damage to any target. Proliferate. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 3);
			proliferate(player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_vorinclex_voice_of_hunger(int player, int card, event_t event ){
/* CARD_ID_VORINCLEX_VOICE_OF_HUNGER	11713
Vorinclex, Voice of Hunger	|6|G|G
Legendary Creature - Praetor 7/6
Trample
Whenever you tap a land for mana, add one mana to your mana pool of any type that land produced.
Whenever an opponent taps a land for mana, that land doesn't untap during its controller's next untap step. */

	check_legend_rule(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( event == EVENT_TAP_CARD && tapped_for_mana_color >= 0 && is_what(affected_card_controller, affected_card, TYPE_LAND) )
		{
			if( affected_card_controller == player ){
				card_mana_flare(affected_card_controller, affected_card, EVENT_TAP_CARD);
			}
			else{
				does_not_untap_effect(player, card, affected_card_controller, affected_card, 0, 1);
			}
		}

		if( event == EVENT_COUNT_MANA && affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_LAND)&&
			! is_tapped(affected_card_controller, affected_card) )
		{
			mana_producer(affected_card_controller, affected_card, EVENT_COUNT_MANA);
		}
	}

	return 0;
}

int card_wing_splicer(int player, int card, event_t event ){
	/* Wing Splicer	|3|U
	 * Creature - Human Artificer 1/1
	 * When ~ enters the battlefield, put a 3/3 colorless Golem artifact creature token onto the battlefield.
	 * Golem creatures you control have flying. */

	return generic_splicer(player, card, event, 0, 0, KEYWORD_FLYING, 1);
}

int card_act_of_aggression(int player, int card, event_t event){
  /* Act of Aggression	|3|RP|RP
   * Instant
   * (|RP can be paid with either |R or 2 life.)
   * Gain control of target creature an opponent controls until end of turn. Untap that creature. It gains haste until end of turn. */

	if( event == EVENT_MODIFY_COST ){
		phyrexian_casting_costs(player, card, event, MANACOST_XR(3, 2), 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.allowed_controller = td.preferred_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int life_before = life[player];
		phyrexian_casting_costs(player, card, event, MANACOST_XR(3, 2), 0);
		if( spell_fizzled != 1 ){
			td.allow_cancel = (life[player] < life_before ? 0 : 1);
			generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature your opponent controls",
						1, NULL);
			if( spell_fizzled != 1 ){
				ai_modifier_for_act_of_treason(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_apostles_blessing(int player, int card, event_t event){

	/* Apostle's Blessing	|1|PW
	 * Instant
	 * Target artifact or creature you control gains protection from artifacts or from the color of your choice until end of turn. */

	if( event == EVENT_MODIFY_COST ){
		phyrexian_casting_costs(player, card, event, MANACOST_XW(1, 1), 0);
	}

	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_ARTIFACT);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.allow_cancel = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);

	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int life_before = life[player];
		phyrexian_casting_costs(player, card, event, MANACOST_XW(1, 1), 0);
		if( spell_fizzled != 1 ){
			if( life_before > life[player] ){
				td.allow_cancel = 0;
				return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
									"Select target artifact or creature you control", 1, NULL);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int choice = do_dialog(player, player, card, -1, -1,
									" Prot. from Black\n Prot. from Blue\n Prot. from Green\n Prot from Red\n Prot. from White\n Prot from Artifacts",
									get_deck_color(player, 1-player) - 1);
			int key = (1 << (11+choice)) | KEYWORD_RECALC_SET_COLOR;
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, key, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						 "Select target artifact or creature you control", 1, NULL);
}

static int arm_with_aether_legacy(int player, int card, event_t event)
{

  if (get_card_instance(player, card)->damage_target_player > -1)
  {
    card_instance_t *instance = get_card_instance(player, card);

    int p = instance->damage_target_player;
    int c = instance->damage_target_card;

    card_instance_t *damage = damage_being_dealt(event);
    if (damage)
    {
      if (damage->damage_source_player == p && damage->damage_source_card == c)
      {
        if (damage->damage_target_player == 1 - p && damage->damage_target_card == -1 &&
            !damage_is_to_planeswalker(damage))
        {
          if (instance->targets[8].player < 0)
          {
            instance->targets[8].player = 0;
          }
          instance->targets[8].player++;
        }
      }
    }

    if (resolve_damage_trigger(player, card, event, !duh_mode(player), player, card))
    {
      target_definition_t td;
      default_target_definition(player, card, &td, TYPE_CREATURE);
      td.allowed_controller = td.preferred_controller = 1 - player;
      td.illegal_abilities = get_protections_from(p, c);
      if (can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0))
      {
        bounce_permanent(instance->targets[0].player, instance->targets[0].card);
      }
      instance->number_of_targets = 0;
    }
  }

  if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_arm_with_aether(int player, int card, event_t event){
  /* Arm with Æther |2|U
   * Sorcery
   * Until end of turn, creatures you control gain "Whenever this creature deals damage to an opponent,
   * 		you may return target creature that player controls to its owner's hand." */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( current_phase < PHASE_AFTER_BLOCKING ){
			ai_modifier += (count_subtype(player, TYPE_CREATURE, -1) * 8);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, &arm_with_aether_legacy);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_artillerize(int player, int card, event_t event){
	/* Artillerize	|3|R	0x2004625
	 * Instant
	 * As an additional cost to cast this spell, sacrifice an artifact or creature.
	 * ~ deals 5 damage to any target. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE | TYPE_ARTIFACT, "Select an artifact or creature to sacrifice.");

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST && new_can_sacrifice_as_cost(player, card, &test) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance(player, card)->number_of_targets = 0;
		int sac = new_sacrifice(player, card, player, SAC_AS_COST | SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
		if( sac ){
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
			generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
			if( spell_fizzled != 1 ){
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 5);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_auriok_survivors(int player, int card, event_t event){
/* Auriok Survivors	|5|W
 * Creature - Human Soldier 4/6
 * When ~ enters the battlefield, you may return target Equipment card from your graveyard to the battlefield.
 * If you do, you may attach it to ~. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select target Equipment card.");
		test.subtype = SUBTYPE_EQUIPMENT;
		if( ! graveyard_has_shroud(player) && new_special_count_grave(player, &test) ){
			int equip = new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
			if( equip < 0 ){
				return 0;
			}
			int priority_equip = 10;
			if( check_for_ability(player, card, KEYWORD_DEFENDER) && check_for_special_ability(player, card, SP_KEYWORD_CANNOT_BLOCK) ){
				priority_equip = 1;
			}
			int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_FULLCARD(player, equip), DLG_NO_STORAGE, DLG_NO_CANCEL,
								"Attach to Auriok Survivors", 1, priority_equip,
								"Decline", 1, 5);
			if( choice == 1 ){
				equip_target_creature(player, equip, player, card);
			}
		}
	}

	return 0;
}

int card_blind_zealot(int player, int card, event_t event){
  /* Blind Zealot |1|B|B
   * Creature — Human Cleric 2/2
   * Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)
   * Whenever Blind Zealot deals combat damage to a player, you may sacrifice it. If you do, destroy target creature that player controls. */

	intimidate(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | RESOLVE_TRIGGER_AI(player));
	if( result && ! check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED) ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					if( times_damaged[p] && (! IS_AI(player) || p != player) ){
						target_definition_t td;
						default_target_definition(player, card, &td, TYPE_CREATURE);
						td.allowed_controller = td.preferred_controller = p;

						char msg[100];
						if( p == player ){
							scnprintf(msg, 100, "Select target creature you control.");
						}
						else{
							scnprintf(msg, 100, "Select target creature your opponent controls.");
						}
						card_instance_t *instance = get_card_instance(player, card);

						if( new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
							kill_card(player, card, KILL_SACRIFICE);
							kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
							return 0;
						}
					}
				}
		);
	}

	return 0;
}

int card_blinding_souleater(int player, int card, event_t event){

	/* Blinding Souleater	|3
	 * Artifact Creature - Cleric 1/3
	 * |PW, |T: Tap target creature. */

	if (!IS_ACTIVATING(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( has_phyrexian_mana_for_activated_ability(player, card, MANACOST_W(1)) ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DO_NOT_RECALCULATE_ACTIVATION_COST,
											MANACOST0, 0, &td, NULL);
		}
	}

	if (event == EVENT_ACTIVATE){
		instance->number_of_targets = 0;
		int life_before = life[player];
		if( charge_phyrexian_mana_for_activated_ability(player, card, MANACOST_W(1)) ){
			td.allow_cancel = (life_before < life[player] ? 0 : 1);
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DO_NOT_RECALCULATE_ACTIVATION_COST,
											MANACOST0, 0, &td, "TARGET_CREATURE");
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION){
		if (valid_target(&td)){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_brutalizer_exarch(int player, int card, event_t event){
  /* Brutalizer Exarch	|5|G
   * Creature - Phyrexian Cleric 3/3
   * When this creature enters, choose one -
   * * Search your library for a creature card, reveal it, then shuffle and put that card on top.
   * * Put target noncreature permanent on the bottom of its owner's library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_CREATURE;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_STORAGE, DLG_NO_CANCEL,
							"Tutor Creature", 1, 1,
							"Remove permanent", can_target(&td), 1);
		if( choice == 1 ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_DECK, 0, AI_MAX_VALUE, &test);
		}
		if( choice == 2 ){
			if( new_pick_target(&td, "Select target non-creature permanent.", 0, GS_LITERAL_PROMPT) ){
				put_on_bottom_of_deck(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_caged_sun(int player, int card, event_t event)
{
  /* Caged Sun	|6
   * Artifact
   * As this artifact enters, choose a color.
   * Creatures you control of the chosen color get +1/+1.
   * Whenever a land's ability causes you to add one or more mana of the chosen color, add an additional one mana of that color. */

  card_instance_t *instance;
  card_instance_t *sun;
  int result;
  card_data_t *card_data;
  unsigned int creature_color;

  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = choose_a_color_and_show_legacy(player, card, player, -1);
    instance->targets[9].player = result;
    instance->targets[9].card = 1 << ((unsigned char)result & 0x1f);
  }
  sun = in_play(player, card);
  if (sun != NULL)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if ((((uint32_t)(event - EVENT_COUNT_MANA) & 0xfffffffd) == 0 && result == 0) &&
        (affected_card_controller == player))
    {
      result = is_what(player, affected_card, 1);
      if (result != 0)
      {
        card_data = get_card_data(affected_card_controller, affected_card);
        if (event == EVENT_TAP_CARD)
        {
          result = instance->targets[9].player;
          if ((-1 < result) &&
              ((result == tapped_for_mana_color ||
                ((tapped_for_mana_color == 0x100 && (0 < tapped_for_mana[result]))))))
          {
            produce_mana(affected_card_controller, result, 1);
          }
        }
        else if ((instance->targets[9].card & (unsigned int)card_data->color) != 0)
        {
          result = is_tapped(affected_card_controller, affected_card);
          if (result == 0)
          {
            result = is_animated_and_sick(affected_card_controller, affected_card);
            if (result == 0)
            {
              result = can_produce_mana(affected_card_controller, affected_card);
              if (result != 0)
              {
                declare_mana_available(affected_card_controller, instance->targets[9].player, 1);
              }
            }
          }
        }
      }
    }
  }
  sun = in_play(player, card);
  if (sun != NULL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (((result == 0) && ((uint32_t)(event - EVENT_POWER) < 2)) && (affected_card_controller == player))
    {
      creature_color = get_color(player, affected_card);
      if ((instance->targets[9].card & creature_color) != 0)
      {
        event_result = event_result + 1;
      }
    }
  }
  return 0;
}

int card_caress_of_phyrexia(int player, int card, event_t event){
  /* Caress of Phyrexia |3|B|B
   * Sorcery
   * Target player draws three cards, loses 3 life, and gets three poison counters. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			if( instance->targets[0].player == player ){
				if( life[player]-3 < 6 || POISON_COUNTERS(player)+3 >= 6 ){
					ai_modifier -= 96;
				}
				else{
					ai_modifier += 24;
				}
			}
			else{
				if( life[1-player] <= 3|| POISON_COUNTERS(1-player) >= 7 ){
					ai_modifier += 96;
				}
				else{
					ai_modifier -= 96;
				}
			}
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(instance->targets[0].player, 3);
			lose_life(instance->targets[0].player, 3);
			poison(instance->targets[0].player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_chained_throatseeker(int player, int card, event_t event)
{
  /* Chained Throatseeker	|5|U
   * Creature - Phyrexian Horror 5/5
   * Infect (This creature deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.)
   * This creature can't attack unless defending player is poisoned. */

	if (event == EVENT_ATTACK_LEGALITY && affect_me(player, card) && !is_poisoned(1-player) && ! is_humiliated(player, card, AL_STATIC) )
		event_result = 1;

	return 0;
}

int card_coa_special_effect(int player, int card, event_t event){

	if( new_specific_spell_played(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		int kill = 1;
		if( has_mana(1-player, COLOR_ANY, 1) && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, 1-player, COLOR_ANY, 1) ){
			kill = 0;
		}
		if( kill ){
			real_counter_a_spell(player, card, trigger_cause_controller, trigger_cause);
		}
		if( is_what(player, card, TYPE_EFFECT) ){
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_chancellor_of_the_annex(int player, int card, event_t event){
	/* Chancellor of the Annex	|4|W|W|W
	 * Creature - Angel 5/6
	 * You may reveal this card from your opening hand. If you do, when each opponent casts his or her first spell of the game, counter that spell unless that player pays |1.
	 * Flying
	 * Whenever an opponent casts a spell, counter it unless that player pays |1. */

	return card_coa_special_effect(player, card, event);
}

int card_chancellor_of_the_forge(int player, int card, event_t event){
	/* Chancellor of the Forge	|4|R|R|R
	 * Creature - Giant 5/5
	 * You may reveal this card from your opening hand. If you do, at the beginning of the first upkeep, put a 1/1 |Sred Goblin creature token with haste onto the battlefield.
	 * When ~ enters the battlefield, put X 1/1 |Sred Goblin creature tokens with haste onto the battlefield, where X is the number of creatures you control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int amount = count_permanents_by_type(player, TYPE_CREATURE);
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_GOBLIN, &token);
		token.qty = amount;
		token.s_key_plus = SP_KEYWORD_HASTE;
		generate_token(&token);
	}

	return 0;
}

int card_chancellor_of_the_spires(int player, int card, event_t event){
/* Chancellor of the Spires	|4|U|U|U
 * Creature - Sphinx 5/7
 * You may reveal this card from your opening hand. If you do, at the beginning of the first upkeep, each opponent puts the top seven cards
 * 		of his or her library into his or her graveyard.
 * Flying
 * When ~ enters the battlefield, you may cast target instant or sorcery card from an opponent's graveyard without paying its mana cost. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( !graveyard_has_shroud(1-player) ){
			const int *grave = get_grave(1-player);
			int pa[2][count_graveyard(1-player)];
			int pac = 0;
			int i = 0;
			while( grave[i] != -1 ){
					if( can_legally_play_iid(player, grave[i]) ){
						pa[0][pac] = grave[i];
						pa[1][pac] = i;
						pac++;
					}
					i++;
			}
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_SPELL, "Select target instant or sorcery card.");
			int selected = select_card_from_zone(player, player, pa[0], pac, 0, AI_MAX_CMC, -1, &this_test);
			if( selected != -1 ){
				play_card_in_grave_for_free(player, 1-player, pa[1][selected]);
			}
		}
	}

	return 0;
}

int card_chancellor_of_the_tangle(int player, int card, event_t event){
  /* Chancellor of the Tangle	|4|G|G|G
   * Creature - Phyrexian Beast 6/7
   * You may reveal this card from your opening hand. If you do, at the beginning of your first main phase of the game, add |G.
   * Reach, vigilance */

	vigilance(player, card, event);
	return 0;
}

int card_conversion_chamber(int player, int card, event_t event)
{
  /* Conversion Chamber	|3
   * Artifact
   * |2, |T: Exile target artifact card from a graveyard. Put a charge counter on ~.
   * |2, |T, Remove a charge counter from ~: Put a 3/3 colorless Golem artifact creature token onto the battlefield. */

  if (IS_ACTIVATING(event))
	{
	  if (event == EVENT_CAN_ACTIVATE && !(CAN_TAP(player, card) && CAN_ACTIVATE(player, card, MANACOST_X(2))))
		return 0;

	  enum
	  {
		CHOICE_EXILE = 1,
		CHOICE_GOLEM
	  } choice = DIALOG(player, card, event,
						"Exile an artifact", any_in_graveyard_by_type(ANYBODY, TYPE_ARTIFACT) && !graveyard_has_shroud(ANYBODY), 1,
						"Golem token", count_counters(player, card, COUNTER_CHARGE), 2);

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (event == EVENT_ACTIVATE)
		{
		  if (charge_mana_for_activated_ability(player, card, MANACOST_X(2)))
			switch (choice)
			  {
				case CHOICE_EXILE:
				  ;test_definition_t test;
				  new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact card to exile.");

				  if (select_target_from_either_grave(player, card, 0, AI_MIN_VALUE, AI_MAX_VALUE, &test, 0, 1) != -1)
					{
					  tap_card(player, card);
					  if (player == AI && !(current_turn == 1-player && current_phase == PHASE_DISCARD))
						ai_modifier -= 64;
					}
				  break;

				case CHOICE_GOLEM:
				  remove_counters(player, card, player, card, COUNTER_CHARGE, 1);
				  tap_card(player, card);
				  break;
			  }
		}
	  else	// EVENT_RESOLVE_ACTIVATION
		switch (choice)
		  {
			case CHOICE_EXILE:
			  ;int selected, tgt_gy = get_card_instance(player, card)->targets[0].player;
			  if ((selected = validate_target_from_grave_source(player, card, tgt_gy, 1)) != -1)
				{
				  rfg_card_from_grave(tgt_gy, selected);
				  add_counters(player, card, player, card, COUNTER_CHARGE, 1);
				}
			  break;

			case CHOICE_GOLEM:
			  generate_token_by_id(player, card, CARD_ID_GOLEM);
			  break;
		  }
	}

  if (event == EVENT_SHOULD_AI_PLAY)
	ai_modifier += (player == AI ? 24 : -24) * count_counters(player, card, COUNTER_CHARGE);

  return 0;
}

int card_corrosive_gale(int player, int card, event_t event){
  /* Corrosive Gale	|X|GP
   * Sorcery
   * (|GP can be paid with either |G or 2 life.)
   * ~ deals X damage to each creature with flying. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_MODIFY_COST ){
		return phyrexian_casting_costs(player, card, event, MANACOST_G(1), 0);
	}

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		phyrexian_casting_costs(player, card, event, MANACOST_G(1), 0);
		if( ! played_for_free(player, card) && ! is_token(player, card) && spell_fizzled != 1 ){
			charge_mana(player, COLOR_COLORLESS, -1);
			if( spell_fizzled != 1 ){
				set_x_for_x_spells(player, card, event, x_value);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot > 0 ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.keyword = KEYWORD_FLYING;
			new_damage_all(player, card, ANYBODY, instance->info_slot, 0, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_corrupted_resolve(int player, int card, event_t event){
  /* Corrupted Resolve	|1|U
   * Instant
   * Counter target spell if its controller is poisoned. */

	if( event == EVENT_CAN_CAST ){
		if( counterspell(player, card, event, NULL, 0) ){
			if( is_poisoned(card_on_stack_controller) ){
				return 99;
			}
		}
		return 0;
	}

	return counterspell(player, card, event, NULL, 0);
}

int card_darksteel_relic(int player, int card, event_t event){
  /* Darksteel Relic	|0
   * Artifact
   * Indestructible (Effects that say "destroy" don't destroy this artifact.) */

	indestructible(player, card, event);

	return 0;
}

int card_death_hood_cobra(int player, int card, event_t event){
  /* Death-Hood Cobra	|1|G
   * Creature - Phyrexian Snake 2/2
   * |1|G: This creature gains reach until end of turn.
   * |1|G: This creature gains deathtouch until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_REACH = 1,
		CHOICE_DEATHTOUCH,
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_XG(1, 1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE){
		if( charge_mana_for_activated_ability(player, card, MANACOST_XG(1, 1)) ){
			int priorities[3] = {
								0,
								current_phase < PHASE_DECLARE_BLOCKERS ? 10 : 1,
								current_phase == PHASE_AFTER_BLOCKING && check_state(player, card, STATE_BLOCKING) ? 10 : 1
			};
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Gains Reach", 1, priorities[1],
								"Gains Deathtouch", 1, priorities[2]);
			if( ! choice ){
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int key = instance->info_slot == CHOICE_REACH ? KEYWORD_REACH : 0;
		int s_key = instance->info_slot == CHOICE_DEATHTOUCH ? SP_KEYWORD_DEATHTOUCH : 0;
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								0, 0, key, s_key);
	}

	return 0;
}

int card_defensive_stance(int player, int card, event_t event){
  /* Defensive Stance	|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets -1/+1. */

	return generic_aura(player, card, event, player, -1, 1, 0, 0, 0, 0, 0);
}

int card_dementia_bat(int player, int card, event_t event){
  /* Dementia Bat	|4|B
   * Creature - Phyrexian Bat 2/2
   * Flying
   * |4|B, Sacrifice this creature: Target player discards two cards. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			new_multidiscard(instance->targets[0].player, 2, 0, player);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_XB(4, 1), 0, &td, "TARGET_PLAYER");
}

int card_despise(int player, int card, event_t event ){
  /* Despise	|B
   * Sorcery
   * Target opponent reveals their hand. You choose a creature or planeswalker card from it. That player discards that card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER, "Select a creature or planeswalker card.");

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			new_effect_coercion(&this_definition, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

static int due_respect_legacy(int player, int card, event_t event ){

	permanents_enters_battlefield_tapped(player, card, event, ANYBODY, TYPE_PERMANENT, NULL);

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_due_respect(int player, int card, event_t event ){

	/* Due Respect	|1|W
	 * Instant
	 * Permanents enter the battlefield tapped this turn.
	 * Draw a card. */

	if( event == EVENT_CAN_CAST ){
		int result = basic_spell(player, card, event);
		if( result ){
			if( card_on_stack_controller != -1 && card_on_stack_controller != -1 &&
				is_what(card_on_stack_controller, card_on_stack_controller, TYPE_PERMANENT) )
			{
				result = 99;
			}
			if( result ){
				if( player == AI && current_turn != player ){
					return result;
				}
				if( player == HUMAN ){
					return result;
				}
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn == 1-player ? 48 : -48);
		card_instance_t *instance = get_card_instance(player, card);
		instance->targets[0].player = card_on_stack_controller;
		instance->targets[0].card = card_on_stack;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->targets[0].player > -1 && instance->targets[0].card > -1 &&
			is_what(instance->targets[0].player, instance->targets[0].card, TYPE_PERMANENT) )
		{
			add_state(instance->targets[0].player, instance->targets[0].card, STATE_TAPPED);
		}
		create_legacy_effect(player, card, &due_respect_legacy);
		draw_cards(player, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_card_entomber_exarch(int player, int card, event_t event){
/*
Entomber Exarch |2|B|B
Creature — Cleric 2/2
When Entomber Exarch enters the battlefield, choose one —
• Return target creature card from your graveyard to your hand.
• Target opponent reveals his or her hand, you choose a noncreature card from it, then that player discards that card.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");

		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;

		int can_raise_dead = ! graveyard_has_shroud(player) && new_special_count_grave(player, &test) ? 1 : 0;
		int can_target_opponent = would_validate_arbitrary_target(&td, 1-player, -1);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_raise_dead || can_raise_dead ){
			int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_AUTOCHOOSE_IF_1,
								"Raise dead", can_raise_dead, 5,
								"Opponent discards", can_target_opponent, hand_count[1-player] > 0 ? 5 : 1);
			if( choice == 1 ){
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
			}
			if( choice == 2 ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_CREATURE, "Select a noncreature card.");
				this_test.type_flag = DOESNT_MATCH;

				ec_definition_t this_definition;
				default_ec_definition(instance->targets[0].player, player, &this_definition);
				new_effect_coercion(&this_definition, &this_test);
			}
		}
	}

	return 0;
}

int card_etched_monstrosity(int player, int card, event_t event){

	/* Etched Monstrosity	|5
	 * Artifact Creature - Golem 10/10
	 * ~ enters the battlefield with five -1/-1 counters on it.
	 * |W|U|B|R|G, Remove five -1/-1 counters from ~: Target player draws three cards. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_M1_M1, 5);

	if( (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST) && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.preferred_controller = ANYBODY;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, 0, 1, 1, 1, 1, 1, GVC_COUNTERS(COUNTER_M1_M1, 5),
										&td, NULL) )
		{
			return 5;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, 0, 1, 1, 1, 1, 1, GVC_COUNTERS(COUNTER_M1_M1, 5),
									&td, "TARGET_PLAYER");
		if( spell_fizzled != 1 ){
			if( instance->targets[0].player == 1-player ){
				ai_modifier += (count_deck(1-player) <= 3 ? 96 : -96);
			}
			else{
				ai_modifier += (count_subtype(player, TYPE_LAND, -1)*8);
			}
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			draw_cards(instance->targets[0].player, 3);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, 0, 1, 1, 1, 1, 1, GVC_COUNTERS(COUNTER_M1_M1, 5), &td, NULL);
}

int card_exclusion_ritual(int player, int card, event_t event){
  /* Exclusion Ritual	|4|W|W
   * Enchantment
   * Imprint - When this enchantment enters, exile target nonland permanent.
   * Players can't cast spells with the same name as the exiled card. */

	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance(player, card);
		int i;
		for(i=1; i<instance->info_slot; i++){
			if( has_same_name(affected_card_controller, affected_card, -1, cards_data[instance->targets[i].card].id) ){
				infinite_casting_cost();
			}
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_LAND;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td)  ){
			if( new_pick_target(&td, "Select target nonland permanent.", 0, GS_LITERAL_PROMPT) ){
				int pos = MAX(1, instance->info_slot);
				instance->targets[pos].card = get_card_name(instance->targets[0].player, instance->targets[0].card);
				rfg_target_permanent(instance->targets[0].player, instance->targets[0].card);
				create_card_name_legacy(player, card, instance->targets[pos].card);
				instance->info_slot = pos+1;
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_fallen_ferromancer(int player, int card, event_t event){
	/* Fallen Ferromancer	|3|R	0x20046a7
	 * Creature - Human Shaman 1/1
	 * Infect
	 * |1|R, |T: ~ deals 1 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if (valid_target(&td)){
			damage_target0(player, card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XR(1, 1), 0,
									&td, "TARGET_ANY");
}

int card_forced_worship(int player, int card, event_t event){
  /* Forced Worship	|1|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature can't attack.
   * |2|W: Return this Aura to its owner's hand. */

	if (IS_GAA_EVENT(event)){
	    if (event == EVENT_RESOLVE_ACTIVATION){
	        card_instance_t* instance = get_card_instance(player, card);
	        if (in_play(instance->parent_controller, instance->parent_card)){ bounce_permanent(instance->parent_controller, instance->parent_card); }
	    }
	    return generic_activated_ability(player, card, event, 0, MANACOST_XW(2, 1), 0, NULL, NULL);
	}
	if (event == EVENT_ABILITIES){
	    card_instance_t* instance = in_play(player, card);
	    if (instance && instance->damage_target_player >= 0 && affect_me(instance->damage_target_player, instance->damage_target_card) && !is_humiliated(player, card, AL_STATIC)){
	        cannot_attack(instance->damage_target_player, instance->damage_target_card, event);
	    }
	}
	if (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS){ return disabling_aura(player, card, event); }
	return 0;
}

int card_furnace_scamp(int player, int card, event_t event){
  /* Furnace Scamp	|R
   * Creature - Phyrexian Beast 1/1
   * Whenever this creature deals combat damage to a player, you may sacrifice it. If you do, this creature deals 3 damage to that player. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | RESOLVE_TRIGGER_AI(player));
	if( result && ! check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED) ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					if( times_damaged[p] && (! IS_AI(player) || p != player) ){
						char msg[100];
						if( p == player ){
							scnprintf(msg, 100, "Sac & deal 3 damage to yourself.");
						}
						else{
							scnprintf(msg, 100, "Sac & deal 3 damage to opponent.");
						}
						int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
											msg, 1, 5,
											"Decline", 1, 1);
						if( choice == 1 ){
							damage_player(p, 3, player, card);
							kill_card(player, card, KILL_SACRIFICE);
							return 0;
						}
					}
				}
		);
	}

	return 0;
}

int card_glissas_scorn(int player, int card, event_t event){
  /* Glissa's Scorn	|1|G
   * Instant
   * Destroy target artifact. Its controller loses 1 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			lose_life(instance->targets[0].player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
}

int card_glistening_oil(int player, int card, event_t event){
  /* Glistening Oil	|B|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has infect.
   * At the beginning of your upkeep, put a -1/-1 counter on enchanted creature.
   * When this Aura is put into a graveyard from the battlefield, return it to its owner's hand. */

	immortal_enchantment(player, card, event);

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		upkeep_trigger_ability(player, card, event, player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			add_counters(player, card, t_player, t_card, COUNTER_M1_M1, 1);
		}
	}

	return generic_aura(player, card, event, player, 0, 0, KEYWORD_INFECT, 0, 0, 0, 0);
}

int card_greenhilt_trainee(int player, int card, event_t event){
  /* Greenhilt Trainee	|3|G
   * Creature - Elf Warrior 2/3
   * |T: Target creature gets +4/+4 until end of turn. Activate only if this creature's power is 4 or greater. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 4, 4);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_gremlin_mine(int player, int card, event_t event)
{
  /* Gremlin Mine	|1
   * Artifact
   * |1, |T, Sacrifice ~: ~ deals 4 damage to target artifact creature.
   * |1, |T, Sacrifice ~: Remove up to four charge counters from target noncreature artifact. */

  enum
  {
    CHOICE_DAMAGE = 1,
    CHOICE_REMOVE_COUNTERS = 2
  };

  int choice;
  card_instance_t *instance;
  int value;
  target_definition_t td;
  target_definition_t td2;

  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td, 0x40);
  td.special = TARGET_SPECIAL_ARTIFACT_CREATURE;
  default_target_definition(player, card, &td2, 0x40);
  if (player == 1)
  {
    td2.special = TARGET_SPECIAL_REQUIRES_COUNTER;
    td2.extra = 0x7b;
  }
  td2.illegal_type = TYPE_CREATURE;
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9, 1, 0, 0, 0, 0, 0, 0, NULL,
                                  NULL) == 0)
    {
      return 0;
    }
    value = can_target(&td);
    if ((value == 0) && (value = can_target(&td2), value == 0))
    {
      return 0;
    }
    return 1;
  }
  if (event == EVENT_ACTIVATE)
  {
    instance->info_slot = 0;
    value = can_target(&td2);
    choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE,
                         "Damage artifact creature", can_target(&td), 10, "Remove Charge counters",
                         value, 5, 0, 0, 0, 0);
    if (choice == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (choice == CHOICE_DAMAGE)
    {
      if (charge_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0) == 0)
      {
        return 0;
      }
      if (new_pick_target(&td, "Select target artifact creature.", 0, 0x81) == 0)
      {
        return 0;
      }
      add_state(player, card, 0x10);
      tapped_for_mana_color = -1;
      instance->info_slot = 1;
    }
    else
    {
      if (choice != CHOICE_REMOVE_COUNTERS)
      {
        return 0;
      }
      if (charge_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0) == 0)
      {
        return 0;
      }
      if (new_pick_target(&td2, "Select target target noncreature artifact.", 0, 0x81) == 0)
      {
        return 0;
      }
      add_state(player, card, 0x10);
      tapped_for_mana_color = -1;
      instance->info_slot = 2;
    }
    kill_card(player, card, KILL_SACRIFICE);
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    value = instance->info_slot;
    if (value == 1)
    {
      if (valid_target(&td) != 0)
      {
        damage_creature(instance->targets[0].player, instance->targets[0].card, 4, player, card);
      }
      value = instance->info_slot;
    }
    if ((value == 2) && (value = valid_target(&td2), value != 0))
    {
      remove_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                      COUNTER_CHARGE, 4);
      return 0;
    }
  }
  return 0;
}

int card_grim_affliction(int player, int card, event_t event ){
/* CARD_ID_GRIM_AFFLICTION	11612
Grim Affliction	|2|B
Instant
Put a -1/-1 counter on target creature, then proliferate.
(You choose any number of permanents and/or players with counters on them, then give each another counter of a kind already there.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_M1_M1, 1);
			proliferate(player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_gut_shot(int player, int card, event_t event){
	/* Gut Shot	|PR	0x20046cf
	 * Instant
	 * ~ deals 1 damage to any target. */
	if( event == EVENT_MODIFY_COST ){
		phyrexian_casting_costs(player, card, event, MANACOST_R(1), 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		phyrexian_casting_costs(player, card, event, MANACOST_R(1), 0);
		if( spell_fizzled != 1 ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_ichor_explosion(int player, int card, event_t event){
/* CARD_ID_ICHOR_EXPLOSION	11616
Ichor Explosion	|5|B|B
Sorcery
As an additional cost to cast ~, sacrifice a creature.
All creatures get -X/-X until end of turn, where X is the sacrificed creature's power. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_SAC_CREATURE_AS_COST, NULL, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		instance->info_slot = get_power(BYTE2(sac), BYTE3(sac));
		kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = instance->info_slot;
		if( amount > 0 ){
			APNAP(p,{ pump_subtype_until_eot(player, card, p, -1, -amount, -amount, 0, 0); };);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_immolating_souleater(int player, int card, event_t event)
{
  /* Immolating Souleater	|2
   * Artifact Creature - Hound 1/1
   * |PR: ~ gets +1/+0 until end of turn. */
  // Used externally
  if (event == EVENT_POW_BOOST)
	{
	  if (!can_pay_life(player, 2))
		return generic_shade_amt_can_pump(player, card, 1, 0, MANACOST_R(1), -1);
	  else
		{
		  int mod = get_cost_mod_for_activated_abilities(player, card, MANACOST_X(0));
		  if (mod == 0)
			return generic_shade_amt_can_pump(player, card, 1, 0, MANACOST_R(1), -1) + generic_shade_amt_can_pump(player, card, 1, 2, MANACOST_X(0), -1);
		  else
			{
			  // This isn't going to be perfectly accurate due to mana sources that can tap for variable amounts of mana, but should be close.
			  int red_available = has_mana(player, COLOR_RED, 1);
			  int cless_available = has_mana(player, COLOR_ANY, 1);
			  int life_available = (life[player] - 1) / 2;
			  if (life_available >= cless_available / mod)
				return cless_available / mod;
			  else
				{
				  int amt = life_available;
				  cless_available -= life_available * mod;

				  if (red_available < cless_available / (mod + 1))
					amt += red_available;
				  else
					amt += cless_available / (mod + 1);

				  return amt;
				}
			}
		}
	}

  if (event == EVENT_CAN_ACTIVATE)
	return can_use_activated_abilities(player, card) && has_phyrexian_mana_for_activated_ability(player, card, MANACOST_R(1));

  if (event == EVENT_ACTIVATE)
	charge_phyrexian_mana_for_activated_ability(player, card, MANACOST_R(1));

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (in_play(instance->parent_controller, instance->parent_card))
		pump_until_eot_merge_previous(player, card, instance->parent_controller, instance->parent_card, 1,0);
	}

  return 0;
}

int card_impaler_shrike(int player, int card, event_t event)
{
  /* Impaler Shrike	|2|U|U
   * Creature - Bird 3/1
   * Flying
   * Whenever ~ deals combat damage to a player, you may sacrifice it. If you do, draw three cards. */

	int mode = RESOLVE_TRIGGER_OPTIONAL;
	if( IS_AI(player) ){
		mode = hand_count[player] <= 2 ? RESOLVE_TRIGGER_MANDATORY : 0;
	}
	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | mode);
	if( result && ! check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED) ){
		draw_cards(player, 3);
		kill_card(player, card, KILL_SACRIFICE);
	}

	return 0;
}

int card_inquisitor_exarch(int player, int card, event_t event){
/* CARD_ID_INQUISITOR_EXARCH	11619
Inquisitor Exarch	|W|W
Creature - Cleric 2/2
When ~ enters the battlefield, choose one -
* You gain 2 life.
* Target opponent loses 2 life. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_NONE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allowed_controller = 1-player;

		int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_AUTOCHOOSE_IF_1,
							"Gain 2 life", 1, life[player] < 6 ? 10 : 1,
							"Opponent lose 2 life", would_validate_arbitrary_target(&td, 1-player, -1), 5);
		if( choice == 1 ){
			gain_life(player, 2);
		}
		if( choice == 2 ){
			 lose_life(1-player, 2);
		}
	}

	return 0;
}

int card_insatiable_souleater(int player, int card, event_t event){

	/* Insatiable Souleater	|4
	 * Artifact Creature - Beast 5/1
	 * |PG: ~ gains trample until end of turn. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( has_phyrexian_mana_for_activated_ability(player, card, MANACOST_G(1)) ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if (event == EVENT_ACTIVATE){
		charge_phyrexian_mana_for_activated_ability(player, card, MANACOST_G(1));
	}

	if (event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);
		if (in_play(instance->parent_controller, instance->parent_card)){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									0, 0, KEYWORD_TRAMPLE, 0);
		}
	}

	return 0;
}

int card_invader_parasite(int player, int card, event_t event){
  /* Invader Parasite |3|R|R
   * Creature — Insect 3/2
   * Imprint — When Invader Parasite enters the battlefield, exile target land.
   * Whenever a land with the same name as the exiled card enters the battlefield under an opponent's control, Invader Parasite deals 2 damage
   * 		to that player. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int trig = 0;
		int i;
		for(i=1; i<BYTE0(instance->info_slot); i++){
			if( get_card_name(trigger_cause_controller, trigger_cause) == instance->targets[i].card ){
				trig = 1;
				break;
			}
		}
		if( trig ){
			SET_BYTE1(instance->info_slot) = 1;
			etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_LAND, NULL, player, card);
			SET_BYTE1(instance->info_slot) = 0;
		}
	}

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( BYTE1(instance->info_slot) == 1 ){
			damage_player(trigger_cause_controller, 2, player, card);
		}
		else{
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_LAND);
			td.allow_cancel = 0;

			if( can_target(&td) && new_pick_target(&td, "TARGET_LAND", 0, 0) ){
				int pos = MAX(BYTE0(instance->info_slot), 1);
				instance->targets[pos].card = get_card_name(instance->targets[0].player, instance->targets[0].card);
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
				create_card_name_legacy(player, card, instance->targets[pos].card);
				SET_BYTE0(instance->info_slot) = pos+1;
			}
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_jor_kadeen_the_prevailer(int player, int card, event_t event){
/* CARD_ID_JOR_KADEEN_THE_PREVAILER	11624
Jor Kadeen, the Prevailer	|3|R|W
Legendary Creature - Human Warrior 5/4
First strike
Metalcraft - Creatures you control get +3/+0 as long as you control three or more artifacts. */

	check_legend_rule(player, card, event);

	if( metalcraft(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		boost_creature_type(player, card, event, -1, 3, 0, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
	}

	return 0;
}

int card_kiln_walker(int player, int card, event_t event)
{
	/* Kiln Walker	|3
	 * Artifact Creature - Construct 0/3
	 * Whenever ~ attacks, it gets +3/+0 until end of turn. */

	return when_attacks_pump_self(player, card, event, 3,0);
}

int card_leeching_bite(int player, int card, event_t event){
/* CARD_ID_LEECHING_BITE	11628
Leeching Bite	|1|G
Instant
Target creature gets +1/+1 until end of turn. Another target creature gets -1/-1 until end of turn. */

	if( event == EVENT_CHECK_PUMP ){
		return vanilla_instant_pump(player, card, event, ANYBODY, player, 1, 1, 0, 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		td.preferred_controller = player;
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature for +1/+1.", 1, NULL);
		if( spell_fizzled != 1 ){
			td.preferred_controller = 1-player;
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			new_pick_target(&td, "Select target creature for -1/-1.", 1, 1 | GS_LITERAL_PROMPT);
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 1, 1);
		}
		if( validate_target(player, card, &td, 1) ){
			pump_until_eot(player, card, instance->targets[1].player, instance->targets[1].card, -1, -1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_lifes_finale(int player, int card, event_t event){
/* CARD_ID_LIFES_FINALE	11629
Life's Finale	|4|B|B
Sorcery
Destroy all creatures, then search target opponent's library for up to three creature cards and put them into his or her graveyard.
Then that player shuffles his or her library. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
		new_manipulate_all(player, card, ANYBODY, &test, KILL_DESTROY);

		int i;
		for(i=0; i<3; i++){
			if( new_global_tutor(player, 1-player, TUTOR_FROM_DECK, TUTOR_GRAVE, 0, AI_MAX_VALUE, &test) < 0 ){
				break;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

int card_marrow_shards(int player, int card, event_t event){
/* CARD_ID_MARROW_SHARDS	11632
Marrow Shards	|PW
Instant
(|PW can be paid with either |W or 2 life.)
~ deals 1 damage to each attacking creature. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.state = STATE_ATTACKING;
		new_damage_all(player, card, ANYBODY, 1, 0, &test);
		kill_card(player, card, KILL_DESTROY);
	}

	return phyrexian_casting_costs(player, card, event, MANACOST_W(1), 0);
}

int card_maul_splicer(int player, int card, event_t event ){
	/* Maul Splicer	|6|G
	 * Creature - Human Artificer 1/1
	 * When ~ enters the battlefield, put two 3/3 colorless Golem artifact creature tokens onto the battlefield.
	 * Golem creatures you control have trample. */

	return generic_splicer(player, card, event, 0, 0, KEYWORD_TRAMPLE, 2);
}

int card_mindcrank(int player, int card, event_t event ){
/* CARD_ID_MINDCRANK	11637
Mindcrank	|2
Artifact
Whenever an opponent loses life, that player puts that many cards from the top of his or her library into his or her graveyard.
(Damage dealt by sources without infect causes loss of life.) */

	return 0;
}

int card_mindculling(int player, int card, event_t event){
/* CARD_ID_MINDCULLING	11638
Mindculling	|5|U
Sorcery
You draw two cards and target opponent discards two cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(player, 2);
			new_multidiscard(1-player, 2, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_mortis_dogs(int player, int card, event_t event)
{
/* CARD_ID_MORTIS_DOGS	11640
Mortis Dogs	|3|B
Creature - Hound 2/2
Whenever ~ attacks, it gets +2/+0 until end of turn.
When ~ dies, target player loses life equal to its power. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
	pump_until_eot(player, card, player, card, 2, 0);

  else if (event == EVENT_POW_BOOST && current_turn == player && current_phase <= PHASE_DECLARE_ATTACKERS)
	return 2;

  // When ~ dies, target player loses life equal to its power.
  if (this_dies_trigger(player, card, event, 2)
	  && pick_player_duh(player, card, 1-player, 0))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  lose_life(instance->targets[0].player, instance->power);
	}

  return 0;
}

int card_mutagenic_growth(int player, int card, event_t event){
  /* Mutagenic Growth	|GP
   * Instant
   * (|GP can be paid with either |G or 2 life.)
   * Target creature gets +2/+2 until end of turn. */

	if( event == EVENT_CHECK_PUMP ){
		return vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 2, 0, 0);
	}

	if( event == EVENT_MODIFY_COST ){
		phyrexian_casting_costs(player, card, event, MANACOST_G(1), 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		phyrexian_casting_costs(player, card, event, MANACOST_G(1), 0);
		if( spell_fizzled != 1 ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_mycosynth_fiend(int player, int card, event_t event){
  /* Mycosynth Fiend	|2|G
   * Creature - Phyrexian Horror 2/2
   * This creature gets +1/+1 for each poison counter your opponents have. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
			event_result += POISON_COUNTERS(1-player);
		}
	}

	return 0;
}

int card_mycosynth_wellsprin(int player, int card, event_t event){
  /* Mycosynth Wellspring	|2
   * Artifact
   * When this artifact enters or is put into a graveyard from the battlefield, you may search your library for a basic land card, reveal it, put it into your hand, then shuffle. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		tutor_basic_land(player, 0, 0);
	}

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		tutor_basic_land(player, 0, 0);
	}

	return 0;
}

int card_necropouncer(int player, int card, event_t event){
  /* Necropouncer	|6
   * Artifact - Equipment
   * Living weapon (When this Equipment enters, create a 0/0 black Phyrexian Germ creature token, then attach this to it.)
   * Equipped creature gets +3/+1 and has haste.
   * Equip |2 */

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( (event == EVENT_ABILITIES || event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			return vanilla_equipment(player, card, event, 2, 3, 1, 0, SP_KEYWORD_HASTE);
		}
	}

	return living_weapon(player, card, event, 2);
}

int card_numbing_dose(int player, int card, event_t event)
{
  /* Numbing Dose	|3|U|U
   * Enchantment - Aura
   * Enchant artifact or creature
   * Enchanted permanent doesn't untap during its controller's untap step.
   * At the beginning of the upkeep of enchanted permanent's controller, that player loses 1 life. */

  char attached_player;
  card_instance_t *instance;
  int priority;
  int mana_value;
  target_definition_t td;

  instance = in_play(player, card);
  if (((instance != NULL) &&
       (instance = get_card_instance(player, card), instance->damage_target_player != -1)) &&
      (priority = is_humiliated(player, card, AL_STATIC), priority == 0))
  {
    instance = get_card_instance(player, card);
    does_not_untap((int)instance->damage_target_player, instance->damage_target_card, event);
  }
  instance = in_play(player, card);
  if (instance &&
      (instance = get_card_instance(player, card), instance->damage_target_player != -1) &&
      !is_humiliated(player, card, AL_TRIG))
  {
    instance = get_card_instance(player, card);
    attached_player = instance->damage_target_player;
    upkeep_trigger_ability(player, card, event, (int)attached_player);
    if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
      lose_life((int)attached_player, 1);
  }
  if (event == EVENT_SHOULD_AI_PLAY)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player == 0)
    {
      priority = get_cmc(0, instance->damage_target_card);
      ai_modifier += priority * 4;
    }
  }

  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  default_target_definition(player, card, &td, 0x42);
  instance = get_card_instance(player, card);
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      instance->number_of_targets = '\0';
      if (new_pick_target(&td, "Select target artifact or creature.", 0, 0x81) == 0)
      {
        return 0;
      }
      mana_value = get_cmc(instance->targets[0].player, instance->targets[0].card);
      priority = mana_value * 8;
      if (is_tapped(instance->targets[0].player, instance->targets[0].card) == 0)
      {
        priority = mana_value * -8;
      }
      ai_modifier = ai_modifier + priority;
      return 0;
    }
  }
  return targeted_aura_custom_prompt(player, card, event, &td,
                                     "Select target artifact or creature.");
}

int card_omen_machine(int player, int card, event_t event){
/* Omen Machine	|6
 * Artifact
 * Players can't draw cards.
 * At the beginning of each player's draw step, that player exiles the top card of his or her library.
 * If it's a land card, the player puts it onto the battlefield.
 * Otherwise, the player casts it without paying its mana cost if able. */

	if( ! is_humiliated(player, card, AL_STATIC) && trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) &&
		event == EVENT_TRIGGER )
	{
		suppress_draw = 1;	// Deliberately do this during EVENT_TRIGGER, not EVENT_RESOLVE_TRIGGER, so nothing else can replace the (nonexistent) draw
	}

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int iid = deck_ptr[current_turn][0];
		if( iid > -1 ){
			rfg_top_card_of_deck(current_turn);
			if( is_what(-1, iid, TYPE_PERMANENT) ){
				remove_card_from_rfg(current_turn, cards_data[iid].id);
				int card_added = add_card_to_hand(current_turn, iid);
				put_into_play(current_turn, card_added);
			}
			else{
				if( can_legally_play_iid(current_turn, iid) ){
					play_card_in_exile_for_free(current_turn, current_turn, cards_data[iid].id);
				}
			}
		}
	}

	return 0;
}

int card_parasitic_implant(int player, int card, event_t event){
	/* Parasitic Implant	|3|B
	 * Enchantment - Aura
	 * Enchant creature
	 * At the beginning of your upkeep, enchanted creature's controller sacrifices it and you put a 1/1 colorless Myr artifact creature token onto the battlefield. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( current_turn == player && upkeep_trigger(player, card, event) ){
			generate_token_by_id(player, card, CARD_ID_MYR);
			kill_card(instance->damage_target_player, instance->damage_target_card, KILL_SACRIFICE);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	return disabling_aura(player, card, event);
}

int card_pestilent_souleater(int player, int card, event_t event){

	/* Pestilent Souleater	|5
	 * Artifact Creature - Insect 3/3
	 * |PB: ~ gains infect until end of turn. */

	if (event == EVENT_CAN_ACTIVATE){
		if( has_phyrexian_mana_for_activated_ability(player, card, MANACOST_B(1)) ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if (event == EVENT_ACTIVATE){
		if( charge_phyrexian_mana_for_activated_ability(player, card, MANACOST_B(1)) ){
			if( is_attacking(player, card) && is_unblocked(player, card) && current_phase == PHASE_AFTER_BLOCKING ){
				ai_modifier += 48;
			}
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);
		if (in_play(instance->parent_controller, instance->parent_card)){
			pump_ability_until_eot(player, card, instance->parent_controller, instance->parent_card, 0,0, KEYWORD_INFECT,0);
		}
	}

	return 0;
}

int card_phyrexias_core(int player, int card, event_t event){

	/* Phyrexia's Core	""
	 * Land
	 * |T: Add |1 to your mana pool.
	 * |1, |T, Sacrifice an artifact: You gain 1 life. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_GAIN_LIFE
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL) &&
			new_can_sacrifice_as_cost(player, card, &test) )
		{
			choice = CHOICE_GAIN_LIFE;
		}

		if( choice == CHOICE_GAIN_LIFE && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Sac artifact & gain life", 1, 5);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_GAIN_LIFE ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
				return 0;
			}
			if( ! new_sacrifice(player, card, player, SAC_AS_COST, &test)){
				remove_state(player, card, STATE_TAPPED);
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_GAIN_LIFE ){
			gain_life(player, 1);
		}
	}

	return 0;
}

int card_phyrexian_ingester(int player, int card, event_t event){
  /* Phyrexian Ingester	|6|U
   * Creature - Phyrexian Beast 3/3
   * Imprint - When this creature enters, you may exile target nontoken creature.
   * This creature gets +X/+Y, where X is the exiled creature card's power and Y is its toughness. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			int pos = MAX(1, instance->info_slot);
			if( pos < 10 ){
				instance->targets[pos].player = get_owner(instance->targets[0].player, instance->targets[0].card);
				instance->targets[pos].card = get_original_internal_card_id(instance->targets[0].player, instance->targets[0].card);
				create_card_name_legacy(player, card, cards_data[instance->targets[pos].card].id);
				rfg_target_permanent(instance->targets[0].player, instance->targets[0].card);
				instance->info_slot = pos+1;
			}
		}
		instance->number_of_targets = 0;
	}

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) && get_card_instance(player, card)->info_slot > 1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( event == EVENT_POWER && affect_me(player, card) ){
			int max_pow = 0;
			int i;
			for(i=1; i<instance->info_slot; i++){
				int owner = instance->targets[i].player;
				int iid = instance->targets[i].card;
				int this_pow = get_base_power_iid(owner, iid);
				if( this_pow > max_pow ){
					max_pow = this_pow;
				}
			}
			event_result += max_pow;
		}
		if( event == EVENT_TOUGHNESS && affect_me(player, card) ){
			int max_tou = 0;
			int i;
			for(i=1; i<instance->info_slot; i++){
				int owner = instance->targets[i].player;
				int iid = instance->targets[i].card;
				int this_tou = get_base_toughness_iid(owner, iid);
				if( this_tou > max_tou ){
					max_tou = this_tou;
				}
			}
			event_result += max_tou;
		}
	}

	return 0;
}

int card_pith_driller(int player, int card, event_t event){
  /* Pith Driller	|4|BP
   * Artifact Creature - Phyrexian Horror 2/4
   * (|BP can be paid with either |B or 2 life.)
   * When this creature enters, put a -1/-1 counter on target creature. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.allow_cancel = 0;

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_M1_M1, 1);
		}
		instance->number_of_targets = 0;
	}

	return phyrexian_casting_costs(player, card, event, MANACOST_XB(4, 1), 1);
}

int card_porcelain_legionnaire(int player, int card, event_t event){
  /* Porcelain Legionnaire	|2|WP
   * Artifact Creature - Phyrexian Soldier 3/1
   * (|WP can be paid with either |W or 2 life.)
   * First strike */

	return phyrexian_casting_costs(player, card, event, MANACOST_XW(2, 1), 1);
}

int card_praetors_grasp(int player, int card, event_t event)
{
  /* Praetor's Grasp	|1|B|B
   * Sorcery
   * Search target opponent's library for a card and exile it face down. Then that player shuffles his or her library.
   * You may look at and play that card for as
   * long as it remains exiled. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
		ai_modifier += 96;

	if (event == EVENT_RESOLVE_SPELL)
	{
		if( valid_target(&td) ){
			test_definition_t test;
			new_default_test_definition(&test, 0, "Select a card to exile.");

			int csvid = new_global_tutor(player, 1-player, TUTOR_FROM_DECK, TUTOR_RFG, 1, AI_MAX_VALUE, &test);
			if (csvid >= 0)
			{
				create_may_play_card_from_exile_effect(player, card, 1-player, csvid, MPCFE_FACE_DOWN);
			}
		}
		else
			spell_fizzled = 1;

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_priest_of_urabrask(int player, int card, event_t event){
  /* Priest of Urabrask	|2|R
   * Creature - Phyrexian Human Cleric 2/1
   * When this creature enters, add |R|R|R. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		produce_mana(player, COLOR_RED, 3);
	}

	return 0;
}

int card_pristine_talisman(int player, int card, event_t event){
  /* Pristine Talisman	|3
   * Artifact
   * |T: Add |C. You gain 1 life. */

	if (event == EVENT_QUERY && attacking_card == 3){
		return 2;
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 1);
	}

	return mana_producer(player, card, event);
}

int card_psychic_barrier(int player, int card, event_t event){
  /* Psychic Barrier	|U|U
   * Instant
   * Counter target creature spell. Its controller loses 1 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, &td, 0) ){
			lose_life(instance->targets[0].player, 1);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
}

int card_rage_extractor(int player, int card, event_t event){
	/* Rage Extractor	|4|PR	0x200476f
	 * Artifact
	 * Whenever you cast a spell with |P in its mana cost, ~ deals damage equal to that spell's converted mana cost to any target. */
	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)
		&& is_phyrexian(get_id(trigger_cause_controller, trigger_cause))){
		sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
	}
	if (event == EVENT_SPA){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;
		card_instance_t* instance = get_card_instance(player, card);
		if (can_target(&td) && new_pick_target(&td, "TARGET_ANY", 0, 0)){
			damage_target0(player, card, get_cmc(trigger_cause_controller, trigger_cause));
		}
		instance->number_of_targets = 0;
	}
	return phyrexian_casting_costs(player, card, event, MANACOST_XR(4, 1), 1);
}

int card_reaper_of_sheoldred(int player, int card, event_t event){
  /* Reaper of Sheoldred	|4|B
   * Creature - Phyrexian Horror 2/5
   * Infect (This creature deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.)
   * Whenever a source deals damage to this creature, that source's controller gets a poison counter. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		card_instance_t *damage = damage_being_dealt(event);
		if( damage ){
			if( damage->damage_target_card == card && damage->damage_target_player == player && damage->info_slot > 0 ){
				card_instance_t *instance = get_card_instance(player, card);
				if( instance->targets[8].player < 0 ){
					instance->targets[8].player = 0;
				}
				instance->targets[8].player++;
				if( instance->targets[1].player < 0 ){
					instance->targets[1].player = 0;
				}
				if( damage->damage_source_player == 0 ){
					SET_BYTE0(instance->targets[1].player)++;
				}
				if( damage->damage_source_player == 1 ){
					SET_BYTE1(instance->targets[1].player)++;
				}
			}
		}
	}

	if( resolve_damage_trigger(player, card, event, DDBM_TRACE_DAMAGED_PLAYERS, player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int player_who_damaged_me[2] = {BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player)};
		instance->targets[1].player = 0;
		APNAP(p,{
					if( player_who_damaged_me[p] ){
						poison(p, 1);
					}
				}
		);
	}

	return 0;
}

int card_remember_the_fallen(int player, int card, event_t event)
{
  /* Remember the Fallen	|2|W
   * Sorcery
   * Choose one or both - Return target creature card from your graveyard to your hand; and/or return target artifact card from your graveyard to your hand. */

  return spell_return_one_or_two_cards_from_gy_to_hand(player, card, event, TYPE_CREATURE, TYPE_ARTIFACT);
}

int card_ruthless_invasion(int player, int card, event_t event){
  /* Ruthless Invasion	|3|RP
   * Sorcery
   * (|RP can be paid with either |R or 2 life.)
   * Nonartifact creatures can't block this turn. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			int count = active_cards_count[i]-1;
			while( count > -1 ){
					if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) && !  is_what(i, count, TYPE_ARTIFACT) ){
						pump_ability_until_eot(player, card, i, count, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
					}
					count--;
			}
		}
	}

	return phyrexian_casting_costs(player, card, event, MANACOST_XR(3, 1), 0);
}

int card_scrapyard_salvo(int player, int card, event_t event){
	/* Scrapyard Salvo	|1|R|R	0x200477e
	 * Sorcery
	 * ~ deals damage to target player or planeswalker equal to the number of artifact cards in your graveyard. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, count_graveyard_by_type(player, TYPE_ARTIFACT));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
}

int card_sensor_splicer(int player, int card, event_t event ){
	/* Sensor Splicer	|4|W
	 * Creature - Artificer 1/1
	 * When ~ enters the battlefield, put a 3/3 colorless Golem artifact creature token onto the battlefield.
	 * Golem creatures you control have vigilance. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_ABILITIES && affected_card_controller == player &&
			has_subtype(affected_card_controller, affected_card, SUBTYPE_GOLEM) )
		{
			vigilance(affected_card_controller, affected_card, event);
		}
	}

	return generic_splicer(player, card, event, 0, 0, 0, 1);
}

int card_shattered_angel(int player, int card, event_t event ){
  /* Shattered Angel	|3|W|W
   * Creature - Phyrexian Angel 3/3
   * Flying
   * Whenever a land an opponent controls enters, you may gain 3 life. */

	etb_ability_global(1-player, event, RESOLVE_TRIGGER_AI(player), TYPE_LAND, 0, player, card);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 3);
	}

	return 0;
}

int card_sickleslicer(int player, int card, event_t event){
  /* Sickleslicer	|3
   * Artifact - Equipment
   * Living weapon (When this Equipment enters, create a 0/0 black Phyrexian Germ creature token, then attach this to it.)
   * Equipped creature gets +2/+2.
   * Equip |4 */

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			return vanilla_equipment(player, card, event, 4, 2, 2, 0, 0);
		}
	}

	return living_weapon(player, card, event, 4);
}

int card_slag_fiend(int player, int card, event_t event)
{
  /* Slag Fiend	|R
   * Creature - Construct 100/100
   * ~'s power and toughness are each equal to the number of artifact cards in all graveyards. */

  int newval;
  if (((((uint32_t)(event - EVENT_POWER) < 2) && (player != -1)) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    newval = count_graveyard_by_type(2, 0x40);
    set_characteristic_pt(newval);
  }
  return 0;
}

int card_slash_panter(int player, int card, event_t event){
  /* Slash Panther	|4|RP
   * Artifact Creature - Phyrexian Cat 4/2
   * (|RP can be paid with either |R or 2 life.)
   * Haste */

	haste(player, card, event);

	return phyrexian_casting_costs(player, card, event, MANACOST_XR(4, 1), 1);
}
int card_soul_conduit(int player, int card, event_t event){
  /* Soul Conduit |6
   * Artifact
   * {6}, {T}: Two target players exchange life totals. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( would_validate_arbitrary_target(&td, player, -1) &&  would_validate_arbitrary_target(&td, 1-player, -1) ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(6), 0, NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		ai_modifier += ((life[player] - life[1-player]) * 8);
		generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(6), 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			instance->targets[0].player = 1-player;
			instance->targets[0].card = -1;
			instance->targets[1].player = player;
			instance->targets[1].card = -1;
			instance->number_of_targets = 2;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td, 1) ){
			exchange_life_totals(instance->targets[0].player, instance->targets[1].player);
		}
	}

	return 0;
}

static const char *can_legally_change_target_to_me(int who_chooses, int player, int card,
                                                   int targeting_player, int targeting_card)
{
  card_instance_t *eff = get_card_instance(player, card);
  if (eff->number_of_targets == 0)
    return "must have at least 1 target";
  eff->targets[9].player = targeting_player;
  eff->targets[9].card = targeting_card;
  set_special_flags2(player, card, SF2_SPELLSKITE);
  int rvalue = call_card_function(player, card, EVENT_CAN_CAST);
  eff->targets[9].player = -1;
  eff->targets[9].card = -1;
  remove_special_flags2(player, card, SF2_SPELLSKITE);
  return rvalue ? NULL : "cannot target Spellskite";
}

int card_spellskite(int player, int card, event_t event)
{
  /* Spellskite	|2
   * Artifact Creature - Phyrexian Horror 0/4
   * |UP: Change a target of target spell or ability to this creature. (|UP can be paid with either |U or 2 life.) */

  target_definition_t td_spell;
  counterspell_target_definition(player, card, &td_spell, TYPE_ANY);
  td_spell.extra = target_predicate_extra(can_legally_change_target_to_me);
  td_spell.special |= TARGET_SPECIAL_EXTRA_FUNCTION;

  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_CAN_ACTIVATE)
  {
    if (has_phyrexian_mana_for_activated_ability(player, card, 0, 0, 1, 0, 0, 0))
    {
      if (generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, 0, 0, 0, 0, 0, 0, 0,
                                    &td_spell, "TARGET_SPELL"))
      {
        return 99;
      }
    }
  }

  if (event == EVENT_ACTIVATE)
  {
    if (charge_phyrexian_mana_for_activated_ability(player, card, 0, 0, 1, 0, 0, 0))
    {
      instance->targets[0].player = card_on_stack_controller;
      instance->targets[0].card = card_on_stack;
      instance->number_of_targets = 1;
      set_special_flags2(instance->targets[0].player, instance->targets[0].card, SF2_SPELLSKITE);
      int rvalue = call_card_function(instance->targets[0].player, instance->targets[0].card,
                                      EVENT_CHANGE_TARGET);
      remove_special_flags2(instance->targets[0].player, instance->targets[0].card, SF2_SPELLSKITE);
      if (rvalue != -1)
      {
        instance->info_slot = rvalue;
      }
      else
      {
        spell_fizzled = 1;
      }
    }
  }

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (counterspell_validate(player, card, &td_spell, 0))
    {
      card_instance_t *spell =
          get_card_instance(instance->targets[0].player, instance->targets[0].card);
      spell->targets[instance->info_slot].player = player;
      spell->targets[instance->info_slot].card = instance->parent_card;
    }
  }

  return 0;
}

static int artifact_natively_use_charge_counters(int player, int card)
{
	return get_counter_type_by_id(get_id(player, card)) == COUNTER_CHARGE ? 1 : 0;
}

int card_surge_node(int player, int card, event_t event)
{
  /* Surge Node	|1
   * Artifact
   * ~ enters the battlefield with six charge counters on it.
   * |1, |T, Remove a charge counter from ~: Put a charge counter on target artifact. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, 6);

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(1), GVC_COUNTER(COUNTER_CHARGE),
									&td, "TARGET_ARTIFACT");
		if( spell_fizzled != 1 ){
			if( artifact_natively_use_charge_counters(instance->targets[0].player, instance->targets[0].card) &&
				get_id(instance->targets[0].player, instance->targets[0].card) != CARD_ID_SURGE_NODE )
			{
				ai_modifier += 48;
			}
			else{
				ai_modifier -= 96;
			}
		}
		return 0;
	}

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
		add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_CHARGE, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(1), GVC_COUNTER(COUNTER_CHARGE),
									&td, "TARGET_ARTIFACT");
}

int card_suture_priest(int player, int card, event_t event)
{
  /* Suture Priest	|1|W
   * Creature - Phyrexian Cleric 1/1
   * Whenever another creature you control enters, you may gain 1 life.
   * Whenever a creature an opponent controls enters, you may have that player lose 1 life. */

  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card))
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_CREATURE);
    if (trigger_cause_controller == player)
      test.not_me = 1;
    etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_AI(player), TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    if (trigger_cause_controller == player)
      gain_life(player, 1);
    else
      lose_life(1-player, 1);
  }
  return 0;
}

int card_thundering_thanadon(int player, int card, event_t event ){
  /* Thundering Tanadon	|4|GP|GP
   * Artifact Creature - Phyrexian Beast 5/4
   * (|GP can be paid with either |G or 2 life.)
   * Trample */

	return phyrexian_casting_costs(player, card, event, MANACOST_XG(4, 2), 1);
}

int card_tormentor_exarch(int player, int card, event_t event){
  /* Tormentor Exarch	|3|R
   * Creature - Phyrexian Cleric 2/2
   * When this creature enters, choose one -
   * * Target creature gets +2/+0 until end of turn.
   * * Target creature gets -0/-2 until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.preferred_controller = 1-player;
		td1.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_AUTOCHOOSE_IF_1,
							"Give +2/+0", can_target(&td), 5,
							"Give +0/-2", can_target(&td1), 5);
		if( choice == 1 ){
			if( new_pick_target(&td, "Select target creature.", 0, GS_LITERAL_PROMPT) ){
				pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 0);
			}
		}
		if( choice == 2 ){
			if( new_pick_target(&td1, "Select target creature.", 0, GS_LITERAL_PROMPT) ){
				pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, -2);
			}
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_trespassing_souleater(int player, int card, event_t event){

	/* Trespassing Souleater	|3
	 * Artifact Creature - Construct 2/2
	 * |PU: ~ is unblockable this turn. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( has_phyrexian_mana_for_activated_ability(player, card, MANACOST_U(1)) ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}
	if (event == EVENT_ACTIVATE){
		if( charge_phyrexian_mana_for_activated_ability(player, card, MANACOST_U(1)) ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);
		if (in_play(instance->parent_controller, instance->parent_card)){
			pump_ability_until_eot(player, card, instance->parent_controller, instance->parent_card, 0,0, 0,SP_KEYWORD_UNBLOCKABLE);
		}
	}

	return 0;
}

int card_triumph_of_the_horde(int player, int card, event_t event){
/* CARD_ID_TRIUMPH_OF_THE_HORDES	11702
Triumph of the Hordes	|2|G|G
Sorcery
Until end of turn, creatures you control get +1/+1 and gain trample and infect.
(Creatures with infect deal damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.) */

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 1, 1, KEYWORD_TRAMPLE | KEYWORD_INFECT, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_unwinding_clock(int player, int card, event_t event){
  /* Unwinding Clock |4
   * Artifact
   * Untap all artifacts you control during each other player's untap step. */

	if( beginning_of_phase(player, card, event, 1-player, -1, PHASE_UNTAP) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		new_manipulate_all(player, card, player, &test, ACT_UNTAP);
	}

	return 0;
}

int card_vapor_snag(int player, int card, event_t event){
/* CARD_ID_VAPOR_SNAG	11705
Vapor Snag	|U
Instant
Return target creature to its owner's hand. Its controller loses 1 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 1);
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_victorious_destruction(int player, int card, event_t event ){
/* CARD_ID_VICTORIOUS_DESTRUCTION	11707
Victorious Destruction	|4|R
Sorcery
Destroy target artifact or land. Its controller loses 1 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 1);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target artifact or creature", 1, NULL);
}

int card_viral_drake(int player, int card, event_t event){
/* CARD_ID_VIRAL_DRAKE	11708
Viral Drake	|3|U
Creature - Drake 1/4
Flying
Infect (This creature deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.)
|3|U: Proliferate. (You choose any number of permanents and/or players with counters on them, then give each another counter of a kind already there.) */

	if( event == EVENT_ACTIVATE ){
		if( POISON_COUNTERS(1-player) || count_minus1_minus1_counters(1-player, -1) ){
			ai_modifier += 48;
		}
		if( count_1_1_counters(player, -1) ){
			ai_modifier += 24;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		proliferate(player, card);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XU(3, 1), 0, NULL, NULL);
}

int card_viridian_betrayers(int player, int card, event_t event)
{
/* CARD_ID_VIRIDIAN_BETRAYERS	11709
Viridian Betrayers	|1|G|G
Creature - Elf Warrior 3/1
~ has infect as long as an opponent is poisoned. (It deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.) */

	if( event == EVENT_ABILITIES && affect_me(player, card) && is_poisoned(1-player) && ! is_humiliated(player, card, AL_STATIC) )
		event_result |= KEYWORD_INFECT;

	return 0;
}

static int viridian_harvest_legacy(int player, int card, event_t event){

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		gain_life(player, 6);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_viridian_harvest(int player, int card, event_t event)
{
  /* CARD_ID_VIRIDIAN_HARVEST	11710
   * Viridian Harvest	|G
   * Enchantment - Aura
   * Enchant artifact
   * When enchanted artifact is put into a graveyard, you gain 6 life. */

  int result;
  target_definition_t td;
  result = attached_creature_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    gain_life(player, 6);
  }

  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  default_target_definition(player, card, &td, 0x40);
  td.preferred_controller = player;
  return targeted_aura(player, card, event, &td, "TARGET_ARTIFACT");
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int deal_6_damage_legacy(int player, int card, event_t event);

int card_war_report(int player, int card, event_t event ){
/* CARD_ID_WAR_REPORT	11715
War Report	|3|W
Instant
You gain life equal to the number of creatures on the battlefield plus the number of artifacts on the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = count_subtype(ANYBODY, TYPE_ARTIFACT, -1) + count_subtype(ANYBODY, TYPE_CREATURE, -1);
		gain_life(player, amount);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_whipflare(int player, int card, event_t event ){
/* CARD_ID_WHIPFLARE	11716
Whipflare	|1|R
Sorcery
~ deals 2 damage to each nonartifact creature. */

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p,{
					int c;
					for(c = active_cards_count[p]-1; c > -1; c--){
						if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) && ! is_what(p, c, TYPE_ARTIFACT) ){
							damage_creature(p, c, 2, player, card);
						}
					}
				}
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int bludgeon_brawl_effect(int player, int card, event_t event);

int card_whispering_specter(int player, int card, event_t event){
  /* Whispering Specter |1|B|B
   * Creature — Specter 1/1
   * Flying
   * Infect (This creature deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.)
   * Whenever Whispering Specter deals combat damage to a player, you may sacrifice it. If you do, that player discards a card for each
   * 	poison counter he or she has. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_TRIGGER_OPTIONAL);
	if( result && ! check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED) ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					if( times_damaged[p] ){
						int choice = 2;
						if( POISON_COUNTERS(p) > 1 && p != player ){
							choice = 1;
						}
						char msg[100];
						if( p == player ){
							scnprintf(msg, 100, "Sac and you discard");
						}
						else{
							scnprintf(msg, 100, "Sac and opponent discards");
						}
						choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
										msg, 1, 1,
										"Decline", 1, 1);
						if( choice == 1 ){
							kill_card(player, card, KILL_SACRIFICE);
							new_multidiscard(p, POISON_COUNTERS(p), 0, player);
							return 0;
						}
					}
				}
		);
	}

	return 0;
}

int card_xenograft(int player, int card, event_t event)
{
  /* Xenograft |4|U
   * Enchantment
   * As Xenograft enters the battlefield, choose a creature type.
   * Each creature you control is the chosen type in addition to its other types. */

  if (event == EVENT_RESOLVE_SPELL)
  {
    uint32_t chosen = select_a_subtype(player, card);
    set_inst_info(get_card_instance(player, card), CARD_ID_XENOGRAFT, chosen, 0);
  }
  else if (event == EVENT_CHANGE_TYPE && affected_card_controller == player && in_play(player, affected_card) &&
           is_what(affected_card_controller, affected_card, TYPE_CREATURE) && !is_humiliated(player, card, AL_STATIC))
  {
    card_instance_t* instance = get_card_instance(player, card);
    if (!has_subtype(affected_card_controller, affected_card, inst_info(instance, CARD_ID_XENOGRAFT).data))
      add_a_subtype(affected_card_controller, affected_card, inst_info(instance, CARD_ID_XENOGRAFT).data);
  }
  return global_enchantment(player, card, event);
}

#ifndef SHANDALAR
/* Recovered October 2022 Phyrexian cost helpers. */
int charge_mana_called_from_pay_phyrexian_mana;

/* ManalinkEh 2022: 0x226d7a0; original cards/new_phyrexia.c:5. */
int has_phyrexian_mana(int player, int cless, int black, int blue, int green, int red, int white,
                       int clred_artifact)
{
  int life_needed_or_available;
  unsigned int payable;
  int i;
  int argument2;
  int manacost[5];
  manacost[1] = blue;
  manacost[0] = black;
  manacost[2] = green;
  manacost[3] = red;
  manacost[4] = white;
  if (has_mana_multi(player, cless, black, blue, green, red, white) == 0)
  {
    life_needed_or_available =
        has_mana(player, (unsigned int)(clred_artifact == 0 ? COLOR_COLORLESS : COLOR_ARTIFACT), cless);
    payable = 0;
    if ((life_needed_or_available != 0) &&
        (life_needed_or_available = can_pay_life(player, 1), life_needed_or_available != 0))
    {
      life_needed_or_available = 0x32;
      argument2 = 0;
      while (true)
      {
        argument2 = argument2 + 1;
        i = 4;
        if (black < 1)
        {
          life_needed_or_available = life_needed_or_available + -10;
        }
        else
        {
          do
          {
            for (; black <= i; i = i + -1)
            {
              life_needed_or_available = life_needed_or_available + -2;
            }
            if (has_mana(player, argument2, i + 1) != 0)
            {
              life_needed_or_available = life_needed_or_available + -2;
            }
            i = i + -1;
          } while (-1 < i);
        }
        if (argument2 == 5)
          break;
        black = manacost[argument2];
      }
      if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
      {
        payable = (unsigned int)(6 < life[player] - life_needed_or_available);
      }
      else
      {
        payable = (unsigned int)(life_needed_or_available <= life[player]);
      }
    }
    return payable;
  }
  return 1;
}

/* ManalinkEh 2022: 0x226d900; original cards/new_phyrexia.c:50. */
int has_phyrexian_mana_for_activated_ability(int player, int card, int cless, int black,
                                                  int blue, int green, int red, int white)
{
  int result;

  result = get_cost_mod_for_activated_abilities(
      player, card, (int)(cless ^ 0xffffffffU) >> 0x1f & cless, black, blue, green, red, white);
  return has_phyrexian_mana(player, result, black, blue, green, red, white, 0);
}

/* ManalinkEh 2022: 0x226caa0; original cards/new_phyrexia.c:60. */
static int charge_phyrexian_mana_impl(int player, int cless, int black, int blue, int green,
                                      int red, int white, int clred_artifact)
{
  static const char *words[6] = {"colorless", "black", "blue", "green", "red", "white"};
  int life_available;
  int color;
  int chosen_or_mana;
  unsigned int minimum_life_symbols;
  int argument3;
  int life_payment_or_available;
  int generic_cost;
  int manacost[6];
  int manacost_modified[6];
  char buffer[200];

  manacost[3] = green;
  manacost[5] = white;
  manacost_modified[3] = green;
  manacost[0] = 0;
  manacost[2] = blue;
  manacost[4] = red;
  manacost_modified[0] = 0;
  manacost_modified[2] = blue;
  manacost_modified[4] = red;
  manacost_modified[5] = white;
  manacost[1] = black;
  manacost_modified[1] = black;
  life_payment_or_available = can_pay_life(player, 2);
  life_available = how_much_life_can_i_pay(player);
  if (has_mana(player, (unsigned int)(clred_artifact == 0 ? COLOR_COLORLESS : COLOR_ARTIFACT), cless) == 0)
  {
    spell_fizzled = 1;
    return 0;
  }
  if (life_payment_or_available != 0)
  {
    color = 1;
    life_payment_or_available = 0;
    while (true)
    {
      if (black != 0)
      {
        argument3 = (life_available - life_payment_or_available) / 2;
        if (black < argument3)
        {
          argument3 = black;
        }
        chosen_or_mana = has_mana(player, color, 1);
        minimum_life_symbols =
            black - chosen_or_mana & (int)(black - chosen_or_mana ^ 0xffffffffU) >> 0x1f;
        sprintf(buffer, " Pay 2 life for how much %s mana? (max %d, min %d)",
                words[color], argument3, minimum_life_symbols);
        while (true)
        {
          chosen_or_mana = choose_a_number(player, buffer, argument3);
          if (chosen_or_mana == -1)
          {
            spell_fizzled = 1;
            return 0;
          }
          if ((chosen_or_mana <= argument3) && ((int)minimum_life_symbols <= chosen_or_mana))
            break;
          if (player == 1)
          {
            spell_fizzled = 1;
            ai_modifier = ai_modifier + -100;
            return 0;
          }
        }
        if ((((life_available - life_payment_or_available) + -6) / 2 <= chosen_or_mana) &&
            (player == 1))
        {
          ai_modifier = ai_modifier + -999;
          spell_fizzled = 1;
          return 0;
        }
        life_payment_or_available = life_payment_or_available + chosen_or_mana * 2;
        manacost_modified[color] = manacost_modified[color] - chosen_or_mana;
      }
      color = color + 1;
      if (color == 6)
        break;
      black = manacost[color];
    }
    white = manacost_modified[5];
    green = manacost_modified[3];
    black = manacost_modified[1];
    red = manacost_modified[4];
    blue = manacost_modified[2];
  }
  charge_mana_called_from_pay_phyrexian_mana = 1;
  generic_cost = cless;
  if (clred_artifact != 0)
  {
    generic_cost = 0;
    clred_artifact = cless;
  }
  charge_mana_multi_a(player, generic_cost, black, blue, green, red, white, clred_artifact);
  charge_mana_called_from_pay_phyrexian_mana = 0;
  if (spell_fizzled != 1)
  {
    lose_life(player, life_payment_or_available);
    return 1;
  }
  charge_mana_called_from_pay_phyrexian_mana = 0;
  return 0;
}

/* ManalinkEh 2022: 0x226d990; original cards/new_phyrexia.c:135. */
int charge_phyrexian_mana(int player, int card, event_t event, int cless, int black, int blue,
                               int green, int red, int white, int clred_artifact)
{
  int result;
  result = get_updated_casting_cost(player, card, -1, event, cless);
  result = charge_phyrexian_mana_impl(player, result, black, blue, green, red, white,
                                           clred_artifact);
  return result;
}

/* ManalinkEh 2022: 0x226da10; original cards/new_phyrexia.c:141. */
int charge_phyrexian_mana_for_activated_ability(int player, int card, int cless, int black,
                                                int blue, int green, int red, int white)
{
  int updated_or_paid;
  int variable_cost_or_result;
  int colorless;

  variable_cost_or_result = 0;
  if (cless < 1)
  {
    variable_cost_or_result = cless;
  }
  updated_or_paid = get_cost_mod_for_activated_abilities(
      player, card, (int)(cless ^ 0xffffffffU) >> 0x1f & cless, black, blue, green, red, white);
  colorless = variable_cost_or_result;
  if ((cless < 0) && (updated_or_paid == 0))
  {
    colorless = 0;
    updated_or_paid = variable_cost_or_result;
  }
  updated_or_paid =
      charge_phyrexian_mana_impl(player, updated_or_paid, black, blue, green, red, white, 0);
  variable_cost_or_result = 0;
  if ((updated_or_paid != 0) && (variable_cost_or_result = 1, colorless != 0))
  {
    variable_cost_or_result = charge_mana_multi(player, colorless, 0, 0, 0, 0, 0);
    return (unsigned int)(variable_cost_or_result != 0);
  }
  return variable_cost_or_result;
}

/* ManalinkEh 2022: 0x226db00; original cards/new_phyrexia.c:156. */
int phyrexian_casting_costs(int player, int card, event_t event, int cless, int black, int blue,
                            int green, int red, int white, int clred_artifact)
{
  card_instance_t *instance;
  int cost_or_life_needed;
  int color_or_available;
  int i;
  int manacost[5];

  instance = get_card_instance(player, card);
  manacost[0] = black;
  manacost[1] = blue;
  manacost[2] = green;
  manacost[3] = red;
  manacost[4] = white;
  if (event != EVENT_MODIFY_COST)
  {
    if (event == EVENT_CAN_CAST)
    {
      return 1;
    }
    if (event != EVENT_CAST_SPELL)
    {
      return 0;
    }
    if (card != affected_card)
    {
      return 0;
    }
    if (player != affected_card_controller)
    {
      return 0;
    }
    if (instance->info_slot != 1)
    {
      return 0;
    }
    cost_or_life_needed = played_for_free(player, card);
    color_or_available = is_token(player, card);
    if (cost_or_life_needed != 0 || color_or_available != 0)
    {
      return 0;
    }
    if (charge_phyrexian_mana(player, card, EVENT_CAST_SPELL, cless, black, blue, green, red, white,
                              clred_artifact) != 0)
    {
      return 0;
    }
    spell_fizzled = 1;
    return 0;
  }
  if (card != affected_card)
  {
    return 0;
  }
  if (player != affected_card_controller)
  {
    return 0;
  }
  cost_or_life_needed = get_updated_casting_cost(player, card, -1, EVENT_MODIFY_COST, -1);
  if (clred_artifact == 0)
  {
    color_or_available =
        has_mana_multi(player, cost_or_life_needed, black, blue, green, red, white);
  }
  else
  {
    color_or_available =
        has_mana_multi_a(player, cost_or_life_needed, black, blue, green, red, white);
  }
  if (color_or_available != 0)
  {
    null_casting_cost(player, card);
    instance->info_slot = 1;
    return 0;
  }
  if (clred_artifact == 0)
  {
    if (has_mana(player, 0, cost_or_life_needed) == 0)
    {
      instance->info_slot = 0;
      return 0;
    }
  }
  else
  {
    if (has_mana(player, 6, cost_or_life_needed) == 0)
    {
      instance->info_slot = 0;
      infinite_casting_cost();
      return 0;
    }
  }
  if (can_pay_life(player, 1) != 0)
  {
    color_or_available = 0;
    cost_or_life_needed = 0x32;
    while (true)
    {
      color_or_available = color_or_available + 1;
      if (black < 1)
      {
        cost_or_life_needed = cost_or_life_needed + -10;
      }
      else
      {
        i = 4;
        do
        {
          for (; black <= i; i = i + -1)
          {
            cost_or_life_needed = cost_or_life_needed + -2;
          }
          if (has_mana(player, color_or_available, i + 1) != 0)
          {
            cost_or_life_needed = cost_or_life_needed + -2;
          }
          i = i + -1;
        } while (-1 < i);
      }
      if (color_or_available == 5)
        break;
      black = manacost[color_or_available];
    }
    if (player == 1)
    {
      if (6 < life[1] - cost_or_life_needed)
      {
        null_casting_cost(player, card);
        instance->info_slot = 1;
        return 0;
      }
    }
    else if (cost_or_life_needed <= life[player])
    {
      null_casting_cost(player, card);
      instance->info_slot = 1;
      return 0;
    }
  }
  instance->info_slot = 0;
  if (clred_artifact == 0)
  {
    return 0;
  }
  infinite_casting_cost();
  return 0;
}

#endif
