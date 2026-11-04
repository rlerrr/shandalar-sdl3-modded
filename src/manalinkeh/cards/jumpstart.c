#include "manalink.h"

static int allosaurus_legacy(int player, int card, event_t event);

static int kels_fight_fixer_effect(int player, int card, event_t event);
static int spiteful_prankster_legacy(int player, int card, event_t event);

#ifndef SHANDALAR

int card_towering_titan(int player, int card, event_t event);
int card_neyith_of_the_dire_hunt(int player, int card, event_t event);

#endif

/* ManalinkEh 2022: 0x2372700; original cards/jumpstart.c:3. */
static int allosaurus_legacy(int player, int card, event_t event)
{

  attached_set_pt(player, card, event, 5, 5);
  attached_is_additional_subtype(player, card, event, SUBTYPE_DINOSAUR);
  if (!eot_trigger(player, card, event))
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x2372e70; original cards/jumpstart.c:11. */
int card_allosaurus_shepherd(int player, int card, event_t event)
{
  /* Allosaurus Shepherd	|G
   * Creature - Elf Shaman 1/1
   * This spell can't be countered.
   * Green spells you control can't be countered.
   * |4|G|G: Until end of turn, each Elf creature you control has base power and toughness 5/5 and becomes a Dinosaur in addition to its other creature types. */

  cannot_be_countered(player, card, event);
  test_definition_t test;
  default_test_definition(&test, TYPE_ANY);
  test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
  type_uncounterable(player, card, event, player, 0, &test);

  if (event != EVENT_ACTIVATE &&
      1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION) && event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
    for (int c = 0; c < active_cards_count[player]; ++c)
      if (is_what(player, c, TYPE_CREATURE) && has_subtype(player, c, SUBTYPE_ELF))
        create_targetted_legacy_effect(player, card, allosaurus_legacy, player, c);

  return generic_activated_ability(player, card, event, 0, 4, 0, 0, 2, 0, 0,
                                   0, NULL, NULL);
}

/* ManalinkEh 2022: 0x2372ff0; original cards/jumpstart.c:37. */
int card_archaeomender(int player, int card, event_t event)
{
  /* Archaeomender	|2|U
   * Creature - Human Wizard 2/3
   * When this creature enters, return target artifact card from your graveyard to your hand. */

  int result;
  test_definition_t test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 0x40, "Select target artifact card.");
    result = count_graveyard_by_type(player, 0x40);
    if (result != 0)
    {
      result = graveyard_has_shroud(player);
      if (result == 0)
      {
        new_global_tutor(player, player, 2, 1, 1, 1, &test);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23730a0; original cards/jumpstart.c:56. */
int card_blessed_sanctuary(int player, int card, event_t event)
{
  /* Blessed Sanctuary	|3|W|W
   * Enchantment
   * Prevent all noncombat damage that would be dealt to you and creatures you control.
   * Whenever a nontoken creature you control enters, create a 2/2 white Unicorn creature token. */

  card_instance_t *instance;
  int result;
  test_definition_t this_test;

  instance = noncombat_damage_being_prevented(event);
  if ((instance != NULL) && (instance->damage_target_player == player))
  {
    prevent_damage(affected_card_controller, affected_card, instance->info_slot);
  }
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&this_test, 2);
    this_test.type_flag = 3;
    this_test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &this_test, player, card);
  }
  if (event == EVENT_ETB)
  {
    generate_token_by_id(player, card, CARD_ID_UNICORN);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23731b0; original cards/jumpstart.c:91. */
int card_brightmare(int player, int card, event_t event)
{
  /* Brightmare	|2|W
   * Creature - Unicorn 2/3
   * When this creature enters, tap up to one target creature. You gain life equal to that creature's power. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = 1 - player;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "TARGET_CREATURE", 0, 0);
    if (result != 0)
    {
      tap_card(instance->targets[0].player, instance->targets[0].card);
      result = get_toughness(instance->targets[0].player, instance->targets[0].card);
      gain_life(player, result);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x23732b0; original cards/jumpstart.c:116. */
int card_bruvac_the_grandiloquent(int player, int card, event_t event)
{
  /* Bruvac the Grandiloquent	|2|U
   * Legendary Creature - Human Advisor 1/4
   * If an opponent would mill one or more cards, they mill twice that many cards instead. (To mill a card, a player puts the top card of their library into their graveyard.) */

  enum
  {
    XTRIGGER_REPLACE_MILL_CARDS = 16778496
  };

  xtrigger_t current_trigger;

  check_legend_rule(player, card, event);
  enable_xtrigger_flags = enable_xtrigger_flags | ENABLE_XTRIGGER_REPLACE_MILL;
  current_trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    current_trigger = xtrigger_impl_value_dont_use_directly;
  }
  if ((((current_trigger == XTRIGGER_REPLACE_MILL_CARDS) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (((reason_for_trigger_controller == player && (1 - player == trigger_cause_controller)) &&
        (0 < replace_milled))))
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      if (event == EVENT_TRIGGER)
      {
        event_result = event_result | 2;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {
        replace_milled = replace_milled << 1;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2373370; original cards/jumpstart.c:136. */
int card_chained_brute(int player, int card, event_t event)
{
  /* Chained Brute	|1|R
   * Creature - Devil 4/3
   * This creature doesn't untap during your untap step.
   * |1, Sacrifice another creature: Untap this creature. Activate only during your turn. */

  int result;
  card_instance_t *instance;
  does_not_untap(player, card, event);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    untap_card(instance->parent_controller, instance->parent_card);
  }
  result = generic_activated_ability(player, card, event, 0x50200, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2373430; original cards/jumpstart.c:153. */
int card_corsair_captain(int player, int card, event_t event)
{
  /* Corsair Captain	|2|U
   * Creature - Human Pirate 2/2
   * When this creature enters, create a Treasure token. (It's an artifact with "|T, Sacrifice this token: Add one mana of any color.")
   * Other Pirates you control get +1/+1. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    generate_token_by_id(player, card, CARD_ID_TREASURE);
  }
  boost_subtype(player, card, event, SUBTYPE_PIRATE, 1, 1, 0, 0, BCT_CONTROLLER_ONLY);
  return 0;
}

/* ManalinkEh 2022: 0x23734d0; original cards/jumpstart.c:168. */
int card_emiel_the_blessed(int player, int card, event_t event)
{
  /* Emiel the Blessed	|2|W|W
   * Legendary Creature - Unicorn 4/4
   * |3: Exile another target creature you control, then return it to the battlefield under its owner's control.
   * Whenever another creature you control enters, you may pay |GW. If you do, put a +1/+1 counter on it. If it's a Unicorn, put two +1/+1 counters on it instead. (|GW can be paid with either |G or |W.) */

  int result;
  card_instance_t *instance;


  check_legend_rule(player, card, event);
  if ((((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.not_me = 1;
    result = duh_mode(player);
    etb_ability_global(player, event, (result != 0) + 1, TYPE_NONE, &test,
                       player, card);
  }
  if (event == EVENT_ETB)
  {
    result = has_mana_hybrid(player, 1, 3, 5, 0);
    if (result != 0)
    {
      result = charge_mana_hybrid_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, 1,
                                                  COLOR_GREEN, COLOR_WHITE, 0);
      if (result != 0)
      {
        result = has_subtype(trigger_cause_controller, trigger_cause, SUBTYPE_UNICORN);
        add_counters(player, card, trigger_cause_controller, trigger_cause,
                                 COUNTER_P1_P1, (result != 0) + 1);
      }
    }
  }
  else if (((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
           ((event == EVENT_RESOLVE_ACTIVATION || (event == EVENT_CLEANUP))))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.special = TARGET_SPECIAL_NOT_ME;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      instance = get_card_instance(player, card);
      result = valid_target(&td);
      if (result != 0)
      {
        blink_effect(instance->targets[0].player, instance->targets[0].card, NULL);
      }
    }
    result = generic_activated_ability(player, card, event, 4, 3, 0, 0, 0, 0, 0, 0,
                                       &td, "ASHNODS_BATTLEGEAR");
    return result;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2373780; original cards/jumpstart.c:208. */
int card_immolating_gyre(int player, int card, event_t event)
{
  /* Immolating Gyre	|4|R|R
   * Sorcery
   * ~ deals X damage to each creature and planeswalker you don't control, where X is the number of instant and sorcery cards in your graveyard. */

  int result;
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card != affected_card) || (player != affected_card_controller))
      {
        return 0;
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      result = count_graveyard_by_type(player, 0x38);
      new_damage_all(player, card, 1 - player, result, 0x21, NULL);
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x2373860; original cards/jumpstart.c:225. */
int card_inniaz_the_gale_force(int player, int card, event_t event)
{
  /* Inniaz, the Gale Force	|3|U|U
   * Legendary Creature - Djinn 4/4
   * Flying
   * |2|WU: Attacking creatures with flying get +1/+1 until end of turn. (|WU can be paid with either |W or |U.)
   * Whenever three or more creatures you control with flying attack, each player gains control of a nonland permanent of your choice controlled by the player to their right. */

  card_instance_t *instance;
  int result;
  int target_number;
  unsigned int ret_location;
  unsigned int turn_offset;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (event == EVENT_DECLARE_ATTACKERS)
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.keyword = 0x20;
    test.qty = 3;
    store_attackers(player, card, EVENT_DECLARE_ATTACKERS, DAT_RESOLVE_TRIGGER_MANDATORY, player,
                    -1, &test);
    dat_ability(player, card, EVENT_DECLARE_ATTACKERS, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  }
  else
  {
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      char buffer[150];

      turn_offset = 0;
      default_target_definition(player, card, &td, 0x47);
      td.illegal_type = TYPE_LAND;
      td.illegal_abilities = 0;
      instance = get_card_instance(player, card);
      do
      {
        td.allow_cancel = 0;
        ret_location = current_turn ^ turn_offset;
        td.allowed_controller = ret_location;
        if (ai_is_speculating == 1)
        {
          buffer[0] = '\0';
        }
        else if (player == ret_location)
        {
          scnprintf(buffer, 0x96, "Select a nonland permanent you control");
        }
        else
        {
          scnprintf(buffer, 0x96, "Select a nonland permanent controlled by %s", opponent_name);
        }
        result = can_target(&td);
        if (result != 0)
        {
          new_pick_target(&td, buffer, ret_location, 0x80);
        }
        turn_offset = turn_offset + 1;
      } while (turn_offset != 2);

      turn_offset = 0;
      do
      {
        target_number = 1 - (current_turn ^ turn_offset);
        td.allowed_controller = target_number;
        result = validate_target(player, card, &td, target_number);
        if (result != 0)
        {
          give_control(player, card, *(int *)((int)instance + target_number * 8 + 0x74),
                       *(int *)((int)instance + target_number * 8 + 0x78));
        }
        turn_offset = turn_offset + 1;
      } while (turn_offset != 2);
      instance->number_of_targets = '\0';
      return 0;
    }
    if (event == EVENT_CAN_ACTIVATE)
    {

      result = get_cost_mod_for_activated_abilities(player, card, 2, 0, 0, 0, 0, 1);
      result = has_mana_hybrid(player, 1, 5, 2, result);
      if (result == 0)
      {
        return 0;
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x800, 0, 0, 0, 0, 0, 0, 0,
                                       NULL, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {

      result = get_cost_mod_for_activated_abilities(player, card, 2, 0, 0, 0, 0, 1);
      charge_mana_hybrid(player, card, 1, 5, 2, result);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      return generic_activated_ability(player, card, EVENT_ACTIVATE, 0x800, 0, 0, 0, 0, 0, 0, 0,
                                       NULL, NULL);
    }
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    test_definition_t test;
    new_default_test_definition(&test, 2, "");
    test.keyword = 0x20;
    test.state = 4;
    pump_creatures_until_eot_merge_pt(player, card, current_turn, 1, 1, &test);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2372790; original cards/jumpstart.c:306. */
static int kels_fight_fixer_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;
  unsigned int sacrifice_triggers;
  card_instance_t *dying_permanent;
  test_definition_t test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (result = get_id((int)instance->damage_target_player, instance->damage_target_card),
           result != 0x54da))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (result = effect_follows_control_of_attachment(player, card, 0x4700), result != 0))
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (result = effect_follows_control_of_attachment(player, card, event), result != 0))
    {
      return 0;
    }
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {

      instance = get_card_instance(player, card);
      dying_permanent = in_play(affected_card_controller, affected_card);
      if ((dying_permanent != NULL) &&
          (result = is_this_dying(affected_card_controller, affected_card), result != 0))
      {

        if ((instance->damage_target_card == affected_card) &&
            (affected_card_controller == instance->damage_target_player))
        {
          instance->damage_target_card = -1;
          instance->damage_target_player = -1;
          remove_status(player, card, 0x10000);
          if (instance->targets[0xb].player < 0)
          {
            instance->targets[0xb].player = 0;
          }
          SET_BYTE2(instance->targets[0xb].player) = 1;
        }
        instance = get_card_instance(affected_card_controller, affected_card);
        if (instance->kill_code == '\x03')
        {
          default_test_definition(&test, 2);
          count_for_gfp_ability(player, card, 0x77, player, 0, &test);
        }
      }
    }
  }
  result = duh_mode(player);
  sacrifice_triggers =
      resolve_gfp_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if ((sacrifice_triggers != 0) && (result = check_status(player, card, 0x10000), result == 0))
  {

    sacrifice_triggers = sacrifice_triggers & 0xff;
    while (
        ((sacrifice_triggers != 0 && (result = has_mana_hybrid(player, 1, 2, 1, 0), result != 0)) &&
         (result = charge_mana_hybrid_while_resolving(player, card, event, 1, COLOR_BLUE,
                                                      COLOR_BLACK, 0),
          result != 0)))
    {
      draw_a_card(player);
      sacrifice_triggers = sacrifice_triggers - 1;
    }
    kill_card(player, card, KILL_EXILE);
  }
  if ((event == EVENT_CLEANUP) && (result = check_status(player, card, 0x10000), result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2373d40; original cards/jumpstart.c:366. */
int card_kels_fight_fixer(int player, int card, event_t event)
{
  /* Kels, Fight Fixer	|2|B|B
   * Legendary Creature - Azra Warlock 4/3
   * Menace
   * Whenever you sacrifice a creature, you may pay |UB. If you do, draw a card. (|UB can be paid with either |U or |B.)
   * |1, Sacrifice a creature: Kels gains indestructible until end of turn. */

  card_instance_t *instance;
  int c;
  int legacy_card_or_result;
  card_instance_t *candidate_legacy;
  unsigned int sacrifice_triggers;
  card_instance_t *kels;
  test_definition_t test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      bool legacy_found = false;
      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          candidate_legacy = in_play(player, c);

          if ((((candidate_legacy != NULL) && (is_what(player, c, 0x80) != 0)) &&
               (candidate_legacy = get_card_instance(player, c),
                candidate_legacy->info_slot_fn == kels_fight_fixer_effect)) &&
              (((candidate_legacy->damage_target_player == player &&
                 (candidate_legacy->damage_target_card == card)) ||
                ((candidate_legacy->targets[0].player == player &&
                  (candidate_legacy->targets[0].card == card))))))
          {
            legacy_found = true;
            break;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }

      if (!legacy_found)
      {
        legacy_card_or_result =
            create_targetted_legacy_effect(player, card, kels_fight_fixer_effect, player, card);
        instance = get_card_instance(player, legacy_card_or_result);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, legacy_card_or_result, 0x10000);
      }
    }
  }
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    instance = get_card_instance(affected_card_controller, affected_card);
    if (instance->kill_code == '\x03')
    {
      default_test_definition(&test, 2);
      count_for_gfp_ability(player, card, 0x77, player, 0, &test);
    }
  }
  kels = get_card_instance(player, card);
  if (event == EVENT_GRAVEYARD_FROM_PLAY && kels->kill_code != 0)
    return 0;
  if (kels->kill_code == 0)
  {
    legacy_card_or_result = duh_mode(player);
    sacrifice_triggers = resolve_gfp_ability(
        player, card, event, (legacy_card_or_result != 0) + RESOLVE_TRIGGER_OPTIONAL);
    sacrifice_triggers = sacrifice_triggers & 0xff;
    while (
        ((sacrifice_triggers != 0 && (legacy_card_or_result = has_mana_hybrid(player, 1, 2, 1, 0),
                                      legacy_card_or_result != 0)) &&
         (legacy_card_or_result = charge_mana_hybrid_while_resolving(player, card, event, 1,
                                                                     COLOR_BLUE, COLOR_BLACK, 0),
          legacy_card_or_result != 0)))
    {
      draw_a_card(player);
      sacrifice_triggers = sacrifice_triggers - 1;
    }
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
    pump_ability_until_eot_no_repeat(kels->parent_controller, kels->parent_card,
                                     kels->parent_controller, kels->parent_card, 0, 0x800);
  return generic_activated_ability(player, card, event, 0x200, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x23740f0; original cards/jumpstart.c:433. */
int card_lightning_phoenix(int player, int card, event_t event)
{
  /* Lightning Phoenix	|2|R
   * Creature - Phoenix 2/2
   * Flying, haste
   * This creature can't block.
   * At the beginning of your end step, if an opponent was dealt 3 or more damage this turn, you may pay |R. If you do, return this card from your graveyard to the battlefield. */

  enum
  {
    EVENT_GRAVEYARD_ABILITY_EOT = 1280
  };

  int result;

  haste(player, card, event);
  cannot_block(player, card, event);

  if ((((event == EVENT_GRAVEYARD_ABILITY_EOT) &&
        (result = get_trap_condition(1 - player, 0), 2 < result)) &&
       (result = has_mana(player, 4, 1), result != 0)) &&
      (((result = do_dialog(player, player, card, -1, -1,
                            " Return Lightning Phoenix\n Do not return Lightning Phoenix", 0),
         result == 0 && (charge_mana(player, COLOR_RED, 1), spell_fizzled != 1)) &&
        (result = grafdiggers_cages(player, TYPE_CREATURE, TUTOR_FROM_GRAVE), result == 0))))
  {
    remove_state(player, card, 0x20);
    put_into_play(player, card);
    return -1;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2374220; original cards/jumpstart.c:467. */
int card_lightning_core_excavator(int player, int card, event_t event)
{
  /* Lightning-Core Excavator	|1
   * Artifact Creature - Golem 0/3
   * |5, |T, Sacrifice this creature: It deals 3 damage to any target. */

  int result;
  target_definition_t td;
  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    damage_target0(player, card, 3);
  }
  result =
      generic_activated_ability(player, card, event, 0xd, 5, 0, 0, 0, 0, 0, 0, &td, "TARGET_ANY");
  return result;
}

/* ManalinkEh 2022: 0x2374350; original cards/jumpstart.c:488. */
int card_living_lightning(int player, int card, event_t event)
{
  /* Living Lightning	|3|R
   * Creature - Elemental Shaman 3/2
   * When this creature dies, return target instant or sorcery card from your graveyard to your hand. */

  int result;
  test_definition_t this_test;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    result = count_graveyard_by_type(player, 0x38);
    if (result != 0)
    {
      result = graveyard_has_shroud(player);
      if (result == 0)
      {
        new_default_test_definition(&this_test, 0x38, "Select an instant or sorcery card.");
        new_global_tutor(player, player, 2, 1, 0, 1, &this_test);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2374400; original cards/jumpstart.c:504. */
int card_muxus_goblin_grandee(int player, int card, event_t event)
{
  /* Muxus, Goblin Grandee	|4|R|R
   * Legendary Creature - Goblin Noble 4/4
   * When Muxus enters, reveal the top six cards of your library. Put all Goblin creature cards with mana value 5 or less from among them onto the battlefield and the rest on the bottom of your library in a random order.
   * Whenever Muxus attacks, it gets +1/+1 until end of turn for each other Goblin you control. */

  int i;
  int *deck;
  int other_goblins;
  int revealed_cmc;
  int revealed_iid;
  int revealed_remaining;
  test_definition_t test;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    deck = deck_ptr[player];

    revealed_remaining = count_deck(player);
    if (0 < revealed_remaining)
    {
      if (6 < revealed_remaining)
      {
        revealed_remaining = 6;
      }

      show_deck(0, deck, revealed_remaining, "Here\'s the first 6 card of your deck", 0,
                gs_done_008b40e0);
      i = revealed_remaining + -1;
      do
      {

        revealed_iid = deck[i];
        if (is_what(-1, revealed_iid, 2) != 0)
        {
          if (has_subtype_by_id(player, (unsigned int)cards_data[revealed_iid].id,
                                SUBTYPE_GOBLIN) != 0)
          {
            revealed_cmc = get_cmc_by_internal_id(revealed_iid);
            if (revealed_cmc < 6)
            {

              put_into_play(player, add_card_to_hand(player, revealed_iid));
              remove_card_from_deck(player, i);
              revealed_remaining = revealed_remaining + -1;
            }
          }
        }
        i = i + -1;
      } while (i != -1);
      put_top_x_on_bottom_in_random_order(player, revealed_remaining);
    }
    dat_ability(player, card, EVENT_ETB, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
    return 0;
  }
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }

  default_test_definition(&test, 0xff);
  test.subtype = 0x4b;
  test.not_me = 1;
  other_goblins = check_battlefield_for_special_card(player, card, player, 4, &test);
  pump_until_eot(player, card, player, card, other_goblins, other_goblins);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2374670; original cards/jumpstart.c:547. */
int card_neyith_of_the_dire_hunt(int player, int card, event_t event)
{
  /* Neyith of the Dire Hunt	|2|G|G
   * Legendary Creature - Human Warrior 3/3
   * Whenever one or more creatures you control fight or become blocked, draw a card.
   * At the beginning of combat on your turn, you may pay |2|RG. If you do, double target creature's power until end of turn. That creature must be blocked this combat if able. (|RG can be paid with either |R or |G.) */

  int choice;
  card_instance_t *instance;
  int blocker_count_or_result;
  int creature_or_cost;
  target_definition_t td;
  check_legend_rule(player, card, event);
  if (event == EVENT_FIGHT)
  {
    if (affected_card_controller == player)
    {
      draw_a_card(player);
    }
  }
  else if (event == EVENT_DECLARE_BLOCKERS)
  {
    if ((current_turn == player) &&
        (creature_or_cost = active_cards_count[player] + -1, -1 < creature_or_cost))
    {
      do
      {
        instance = in_play(player, creature_or_cost);
        if ((instance != NULL) &&
            (((blocker_count_or_result = is_what(player, creature_or_cost, 2),
               blocker_count_or_result != 0 &&
                   (blocker_count_or_result = is_attacking(player, creature_or_cost),
                    blocker_count_or_result != 0)) &&
              (blocker_count_or_result = count_my_blockers(player, creature_or_cost),
               0 < blocker_count_or_result))))
        {
          draw_cards(player, blocker_count_or_result);
        }
        creature_or_cost = creature_or_cost + -1;
      } while (creature_or_cost != -1);
    }
  }
  else if (event == EVENT_CHANGE_TYPE)
  {
    if (engine_any_legal_attacker(player) == 0)
    {
      event_flags = event_flags | EA_FORCE_ATTACK;
      return 0;
    }
  }
  else if (((event == EVENT_BEGIN_COMBAT) &&
            (creature_or_cost = is_humiliated(player, card, AL_TRIG), creature_or_cost == 0)) &&
           (current_turn == player))
  {
    default_target_definition(player, card, &td, 2);
    if (can_target(&td) != 0)
    {
      creature_or_cost = get_cost_mod_for_activated_abilities(player, card, 2, 0, 0, 1, 0, 0);
      if (has_mana_hybrid(player, 1, 4, 3, creature_or_cost) != 0)
      {
        blocker_count_or_result = engine_any_legal_attacker(player);
        choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_DISPLAY_FOR_AI, DLG_NO_CANCEL,
                             "Double a creature's power", 1, (blocker_count_or_result + 1) * 0xf,
                             "Pass", 1, 1, 0, 0, 0, 0);
        if (((choice == 1) &&
             (charge_mana_hybrid(player, card, 1, 4, 3, creature_or_cost), spell_fizzled != 1)) &&
            (new_pick_target(&td, "TARGET_CREATURE", 0, 1), spell_fizzled != 1))
        {
          instance = get_card_instance(player, card);
          creature_or_cost = get_power(instance->targets[0].player, instance->targets[0].card);
          pump_ability_until_eot(player, card, instance->targets[0].player,
                                 instance->targets[0].card, creature_or_cost, 0, 0, 0x1000);
        }
      }
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x2374990; original cards/jumpstart.c:599. */
int card_nocturnal_feeder(int player, int card, event_t event)
{
  /* Nocturnal Feeder	|2|B
   * Creature - Vampire Rogue 2/1
   * Flying
   * When this creature dies, each opponent loses 2 life and you gain 2 life. */

  int result;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  lose_life(1 - player, 2);
  gain_life(player, 2);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2374a00; original cards/jumpstart.c:613. */
int card_ormos_archive_keeper(int player, int card, event_t event)
{
  /* Ormos, Archive Keeper	|4|U|U
   * Legendary Creature - Sphinx 5/5
   * Flying
   * If you would draw a card while your library has no cards in it, instead put five +1/+1 counters on Ormos.
   * |1|U|U, Discard three cards with different names: Draw five cards. */

  check_legend_rule(player, card, event);
  if (trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) &&
      reason_for_trigger_controller == player && suppress_draw == 0 &&
      !is_humiliated(player, card, AL_STATIC) && deck_ptr[player][0] == -1)
  {
    if (event == EVENT_TRIGGER)
    {
      event_result |= 2;
      return 0;
    }
    if (event == EVENT_RESOLVE_TRIGGER)
    {
      add_counters(player, card, player, card, COUNTER_P1_P1, 5);
      suppress_draw = 1;
      return 0;
    }
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  int searched[2][3] = {{-1, -1, -1}, {-1, -1, -1}};
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (!generic_activated_ability(player, card, event, 0, 1, 0, 2, 0, 0, 0, 0, NULL, NULL))
      return 0;
    int count = 0;
    for (int i = 0; i < active_cards_count[player]; ++i)
    {
      if (in_hand(player, i) && test_not_in_array(get_original_internal_card_id(player, i),
                                                  (int32_t)(intptr_t)searched, -1, -1))
      {
        ++count;
        if (count == 3)
          return 1;
        searched[0][count] = get_original_internal_card_id(player, i);
      }
    }
    return 0;
  }
  if (event == EVENT_ACTIVATE)
  {
    test_definition_t test;
    new_default_test_definition(&test, 0xff, "Select a card to discard.");
    test.value_for_special_selection_function = (int32_t)(intptr_t)searched;
    test.special_selection_function = test_not_in_array;
    test.zone = 0x100;
    test.id_flag = 1;
    test.id = (uint16_t)cards_data[iid_draw_a_card].id;
    unsigned capacity = (unsigned)hand_count[player];
    int hand_copy[2][capacity];
    int count = 0;
    for (int phase = 0; phase < 2; ++phase)
    {
      int p = phase == 0 ? current_turn : 1 - current_turn;
      for (int c = active_cards_count[p] - 1; c >= 0; --c)
      {
        if (player == p && in_hand(player, c))
        {
          hand_copy[0][count] = get_original_internal_card_id(player, c);
          hand_copy[1][count++] = c;
        }
      }
    }
    if (count != 0)
    {
      int chosen = 0;
      do
      {
        int selected =
            select_card_from_zone(player, player, hand_copy[0], count, 0, 5, -1, &test);
        if (selected < 0 || selected == searched[1][0] || selected == searched[1][1])
        {
          if (chosen < 3)
          {
            spell_fizzled = 1;
            return 0;
          }
          break;
        }
        searched[0][chosen] = hand_copy[0][selected];
        searched[1][chosen] = hand_copy[1][selected];
        ++chosen;
        hand_copy[0][selected] = iid_draw_a_card;
      } while (chosen != 3);
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 1, 0, 2, 0, 0, 0, 0, NULL,
                                     NULL);
      if (spell_fizzled != 1)
      {
        discard_card(player, searched[1][0]);
        discard_card(player, searched[1][1]);
        discard_card(player, searched[1][2]);
        return 1;
      }
    }
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
    draw_cards(player, 5);
  return 0;
}
#endif

/* ManalinkEh 2022: 0x2374fe0; original cards/jumpstart.c:720. */
int card_release_the_dogs(int player, int card, event_t event)
{
  /* Release the Dogs	|3|W
   * Sorcery
   * Create four 1/1 white Dog creature tokens. */

  token_generation_t token;
  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_token_definition(player, card, CARD_ID_DOG, &token);
    token.color_forced = ' ';
    token.qty = '\x04';
    generate_token(&token);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23750b0; original cards/jumpstart.c:740. */
int card_scholar_of_the_lost_trove(int player, int card, event_t event)
{
  /* Scholar of the Lost Trove	|5|U|U
   * Creature - Sphinx 5/5
   * Flying
   * When this creature enters, you may cast target instant, sorcery, or artifact card from your graveyard without paying its mana cost. If an instant or sorcery spell cast this way would be put into your graveyard, exile it instead. */

  int result;
  test_definition_t test;

  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {
    default_test_definition(&test, 0x78);
    result = new_select_target_from_grave(player, card, player, SFG_0, 2, &test, 0);
    if (result != -1)
    {
      play_card_in_grave_for_free_and_exile_it(player, player, result);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2375170; original cards/jumpstart.c:758. */
int card_sethron_hurloon_general(int player, int card, event_t event)
{
  /* Sethron, Hurloon General	|3|R|R
   * Legendary Creature - Minotaur Warrior 4/4
   * Whenever Sethron or another nontoken Minotaur you control enters, create a 2/3 red Minotaur creature token.
   * |2|BR: Minotaurs you control get +1/+0 and gain menace and haste until end of turn. (|BR can be paid with either |B or |R.) */

  int result;
  test_definition_t test;
  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 0xff);
    test.subtype = 0x77;
    test.type_flag = 3;
    test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    generate_token_by_id(player, card, CARD_ID_MINOTAUR);
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    result = get_cost_mod_for_activated_abilities(player, card, 2, 1, 0, 0, 0, 0);
    result = has_mana_hybrid(player, 1, 1, 4, result);
    if (result != 0)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x800, 0, 0, 0, 0, 0, 0,
                                         0, NULL, NULL);
      return result;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    result = get_cost_mod_for_activated_abilities(player, card, 2, 1, 0, 0, 0, 0);
    charge_mana_hybrid(player, card, 1, 1, 4, result);
    if (spell_fizzled != 1)
    {
      result = generic_activated_ability(player, card, EVENT_ACTIVATE, 0x800, 0, 0, 0, 0, 0, 0, 0,
                                         NULL, NULL);
      return result;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    pump_subtype_until_eot(player, card, player, 0x77, 1, 0, 0, 0x10004);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2372ae0; original cards/jumpstart.c:807. */
static int spiteful_prankster_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  card_instance_t *dying_permanent;
  unsigned int death_count;
  int result;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (result = get_id((int)instance->damage_target_player, instance->damage_target_card),
           result != 0x54e6))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  instance = get_card_instance(player, card);
  if ((-1 < instance->damage_target_player) &&
      (result = effect_follows_control_of_attachment(player, card, event), result != 0))
  {
    return 0;
  }
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    instance = get_card_instance(player, card);
    dying_permanent = in_play(affected_card_controller, affected_card);

    if ((dying_permanent != NULL) &&
        (dying_permanent = get_card_instance(affected_card_controller, affected_card),
         dying_permanent->kill_code != '\0'))
    {
      test_definition_t this_test;

      if ((-1 < instance->damage_target_player) &&
          ((instance->damage_target_card == affected_card &&
            (instance->damage_target_player == affected_card_controller))))
      {
        instance->damage_target_card = -1;
        instance->damage_target_player = -1;
        remove_status(player, card, 0x10000);
        if (instance->targets[0xb].player < 0)
        {
          instance->targets[0xb].player = 0;
        }
        SET_BYTE2(instance->targets[0xb].player) = 1;
        return 0;
      }
      default_test_definition(&this_test, 2);
      count_for_gfp_ability(player, card, 0x77, 2, 0, &this_test);
    }
  }
  death_count = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (death_count == 0 && event == EVENT_GRAVEYARD_FROM_PLAY)
    return 0;
  if (death_count != 0)
  {
    result = check_status(player, card, 0x10000);
    if (result == 0)
    {
      target_definition_t td;

      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td.allow_cancel = 0;
      instance = get_card_instance(player, card);
      for (death_count = death_count & 0xff; death_count != 0; death_count = death_count - 1)
      {
        result = can_target(&td);
        if ((result != 0) && (result = new_pick_target(&td, "TARGET_ANY", 1, 0), result != 0))
        {
          damage_creature(instance->targets[1].player, instance->targets[1].card, 1, player, card);
          instance->number_of_targets = '\0';
        }
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) && (result = check_status(player, card, 0x10000), result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23754b0; original cards/jumpstart.c:874. */
int card_spiteful_prankster(int player, int card, event_t event)
{
  /* Spiteful Prankster	|2|R
   * Creature - Devil 3/2
   * During your turn, this creature has first strike.
   * Whenever another creature dies, this creature deals 1 damage to target player or planeswalker. */

  int c;
  card_instance_t *instance;
  unsigned int death_count;
  int legacy_card_or_result;

  if (event == EVENT_ABILITIES)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (current_turn == player))
    {
      event_result = event_result | 0x100;
    }
  }
  else if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      bool legacy_found = false;
      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          instance = in_play(player, c);

          if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
               (instance = get_card_instance(player, c),
                instance->info_slot_fn == spiteful_prankster_legacy)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            legacy_found = true;
            break;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }

      if (!legacy_found)
      {
        legacy_card_or_result =
            create_targetted_legacy_effect(player, card, spiteful_prankster_legacy, player, card);
        instance = get_card_instance(player, legacy_card_or_result);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, legacy_card_or_result, 0x10000);
      }
    }
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 2);
    this_test.not_me = 1;
    count_for_gfp_ability(player, card, 0x77, 2, 0, &this_test);
  }
  instance = get_card_instance(player, card);

  if ((instance->kill_code == '\0') &&
      (death_count = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY),
       death_count != 0))
  {
    target_definition_t td;

    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    for (death_count = death_count & 0xff; death_count != 0; death_count = death_count - 1)
    {
      legacy_card_or_result = can_target(&td);
      if ((legacy_card_or_result != 0) &&
          (legacy_card_or_result = pick_target(&td, "TARGET_ANY"), legacy_card_or_result != 0))
      {
        damage_target0(player, card, 1);
        instance->number_of_targets = '\0';
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2375750; original cards/jumpstart.c:942. */
int card_steel_plume_marshal(int player, int card, event_t event)
{
  /* Steel-Plume Marshal	|3|W|W
   * Creature - Bird Soldier 3/3
   * Flying
   * Whenever this creature attacks, other attacking creatures you control with flying get +2/+2 until end of turn. */

  test_definition_t test;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }
  default_test_definition(&test, 2);
  test.keyword = 0x20;
  test.state = 4;
  test.not_me = 1;
  pump_creatures_until_eot_merge_pt(player, card, player, 2, 2, &test);
  return 0;
}

/* ManalinkEh 2022: 0x2375800; original cards/jumpstart.c:963. */
int card_stone_haven_pilgrim(int player, int card, event_t event)
{
  /* Stone Haven Pilgrim	|1|W
   * Creature - Kor Cleric 2/2
   * Whenever this creature attacks, if you control an artifact or enchantment, this creature gets +1/+1 and gains lifelink until end of turn. */

  int result;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    result = check_battlefield_for_subtype(player, 0x44, -1);
    if (result != 0)
    {
      pump_ability_until_eot(player, card, player, card, 1, 1, 0, 2);
    }
  }
  else if (((((uint32_t)(event - EVENT_POW_BOOST) < 2) && (current_turn == player)) &&
            (current_phase < 0x16)) &&
           (result = check_battlefield_for_subtype(player, 0x44, -1), result != 0))
  {
    result = is_humiliated(player, card, AL_TRIG);
    return (unsigned int)(result == 0);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2375900; original cards/jumpstart.c:986. */
int thriving_land(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;
  color_test_t base_color_test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    instance = get_card_instance(player, card);
    result = get_deck_color(player, player);
    base_color_test = get_sleighted_color_test(player, card, (unsigned int)instance->card_color);
    result = choose_a_color_from(player, result,
                                 (base_color_test ^ ~COLOR_TEST_0) & COLOR_TEST_ANY_COLORED);
    create_card_name_legacy(player, card, result + 0x615a);
    result = color_to_color_test(result);
    instance->info_slot = result;
  }
  return plain_mana_producer_tapped(player, card, event);
}

/* ManalinkEh 2022: 0x23759d0; original cards/jumpstart.c:1039. */
int card_tinybones_trinket_thief(int player, int card, event_t event)
{
  /* Tinybones, Trinket Thief	|1|B
   * Legendary Creature - Skeleton Rogue 1/2
   * At the beginning of each end step, if an opponent discarded a card this turn, you draw a card and you lose 1 life.
   * |4|B|B: Each opponent with no cards in hand loses 10 life. */

  int result;
  check_legend_rule(player, card, event);
  result = eot_trigger(player, card, event);
  if ((result != 0) && (result = get_trap_condition(1 - player, 0x203), result != 0))
  {
    draw_a_card(player);
    lose_life(player, 1);
  }
  if (!IS_GAA_EVENT(event))
    return 0;
  if ((event == EVENT_RESOLVE_ACTIVATION) && (hand_count[1 - player] == 0))
  {
    lose_life(1 - player, 10);
  }
  result = generic_activated_ability(player, card, event, 0, 4, 2, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2375b00; original cards/jumpstart.c:1059. */
int card_towering_titan(int player, int card, event_t event)
{
  /* Towering Titan	|4|G|G
   * Creature - Giant 0/0
   * This creature enters with X +1/+1 counters on it, where X is the total toughness of other creatures you control.
   * Sacrifice a creature with defender: All creatures gain trample until end of turn. */

  int result;
  test_definition_t test;
  result = get_toughness_sum(player);
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, result);
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    pump_creatures_until_eot_no_repeat(player, card, 2, 0, 0x80, 0, NULL);
  }
  default_test_definition(&test, 2);
  test.keyword = KEYWORD_DEFENDER;
  result = generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_CREATURE, &test, SARV_0);
  return result;
}
#endif

/* ManalinkEh 2022: 0x2375c30; original cards/jumpstart.c:1082. */
int card_trusty_retriever(int player, int card, event_t event)
{
  /* Trusty Retriever	|3|W
   * Creature - Dog 2/3
   * When this creature enters, choose one -
   * * Put a +1/+1 counter on this creature.
   * * Return target artifact or enchantment card from your graveyard to your hand. */

  int result;
  unsigned int retrieve_available;
  test_definition_t this_test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    result = count_graveyard_by_type(player, 0x44);
    retrieve_available = 0;
    if (result != 0)
    {
      result = graveyard_has_shroud(player);
      retrieve_available = (unsigned int)(result == 0);
    }
    result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
                         DLG_AUTOCHOOSE_IF_1, "Add a +1/+1 counter", 1, 5, "Retrieve a card",
                         retrieve_available, 5, 0, 0, 0, 0);
    if (result == 1)
    {
      add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    }
    else if (result == 2)
    {
      new_default_test_definition(&this_test, 0x44, "Select target artifact or enchantment card.");
      result = new_special_count_grave(player, &this_test);
      if (0 < result)
      {
        result = graveyard_has_shroud(player);
        if (result == 0)
        {
          new_global_tutor(player, player, 2, 1, 1, 1, &this_test);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2375df0; original cards/jumpstart.c:1118. */
int card_witch_of_the_moors(int player, int card, event_t event)
{
  /* Witch of the Moors	|3|B|B
   * Creature - Human Warlock 4/4
   * Deathtouch
   * At the beginning of your end step, if you gained life this turn, each opponent sacrifices a creature of their choice and you return up to one target creature card from your graveyard to your hand. */

  int result;
  test_definition_t test;
  deathtouch(player, card, event);
  result = eot_trigger(player, card, event);
  if ((result != 0) && (current_turn == player))
  {
    result = get_trap_condition(player, 1);
    if (result != 0)
    {
      new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
      new_sacrifice(player, card, 1 - player, SAC_CAUSED | SAC_NO_CANCEL, &test);
      result = any_in_graveyard_by_type(player, 2);
      if (result != 0)
      {
        new_global_tutor(player, player, 2, 1, 1, 1, &test);
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2375ee0; original cards/jumpstart.c:1137. */
int card_zurzoth_chaos_rider(int player, int card, event_t event)
{
  /* Zurzoth, Chaos Rider	|2|R
   * Legendary Creature - Devil 2/3
   * Whenever an opponent draws their first card each turn, if it's not their turn, you create a 1/1 red Devil creature token with "When this token dies, it deals 1 damage to any target."
   * Whenever one or more Devils you control attack one or more players, you and those players each draw a card, then discard a card at random. */

  int result;
  int who_draws;
  unsigned int player_00;
  test_definition_t test;
  check_legend_rule(player, card, event);
  who_draws = 1 - player;
  result = card_drawn_trigger(player, card, event, who_draws, RESOLVE_TRIGGER_MANDATORY);
  if (((result != 0) && (who_draws != current_turn)) && (cards_drawn_this_turn[who_draws] == 1))
  {
    make_devil(player, card, player, 1, 1);
  }
  if (event != EVENT_DECLARE_ATTACKERS)
  {
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    result = current_turn;
    if (event != EVENT_DAT_ABILITY)
    {
      return 0;
    }
    draw_cards(current_turn, 1);
    discard(result, 1, player);
    player_00 = current_turn ^ 1;
    draw_cards(player_00, 1);
    discard(player_00, 1, player);
    return 0;
  }
  default_test_definition(&test, 2);
  test.subtype = 0x2e;
  store_attackers(player, card, EVENT_DECLARE_ATTACKERS, 0, player, -1, &test);
  dat_ability(player, card, EVENT_DECLARE_ATTACKERS, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  return 0;
}
#endif
