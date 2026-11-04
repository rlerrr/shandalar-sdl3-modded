#include "manalink.h"

int bonus_round_legacy(int player, int card, event_t event);

static void partner_with(int player, int card, event_t event, int id);

#ifndef SHANDALAR

int card_okaun_eye_of_chaos(int player, int card, event_t event);
int card_zndrsplt_eye_of_wisdom(int player, int card, event_t event);

#endif

/* Will Kenrith Emblem	""
 * Emblem - Will
 * Whenever you cast an instant or sorcery spell, copy it. You may choose new targets for the copy. */

/* Rowan Kenrith Emblem	""
 * Emblem - Rowan
 * Whenever you activate an ability that isn't a mana ability, copy it. You may choose new targets for the copy. */

#ifndef SHANDALAR
static int assist(int player, int card, event_t event)
{
  card_ptr_t *spell_cost;
  int cost_contribution_or_result, opponent_or_id, opponent_mana, updated_cost, card_name_or_token;
  card_instance_t *instance;
  char buffer[50];
  cost_contribution_or_result = get_updated_casting_cost(player, card, -1, event, -1);
  opponent_or_id = 1 - player;
  opponent_mana = has_mana(opponent_or_id, 7, 1);
  updated_cost = get_updated_casting_cost(player, card, -1, event, -1);
  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {

    if ((player != 1) || (ai_is_speculating != 1))
    {
      cost_contribution_or_result = get_id(player, card);
      spell_cost = cards_ptr[cost_contribution_or_result];
      cost_contribution_or_result = has_mana(opponent_or_id, 7, 1);
      cost_contribution_or_result = has_mana_multi(
          player,
          updated_cost - cost_contribution_or_result &
              (int)(updated_cost - cost_contribution_or_result ^ 0xffffffffU) >> 0x1f,
          (unsigned int)spell_cost->req.req_black, (unsigned int)spell_cost->req.req_blue,
          (unsigned int)spell_cost->req.req_green, (unsigned int)spell_cost->req.req_red,
          (unsigned int)spell_cost->req.req_white);
      if (cost_contribution_or_result != 0)
      {
        instance->eot_toughness = 1;
        null_casting_cost(player, card);
      }
    }
  }
  else
  {
    if (event != EVENT_CAST_SPELL)
    {
      return 0;
    }
    if ((card == affected_card) && (player == affected_card_controller))
    {
      card_name_or_token = played_for_free(player, card);
      if (card_name_or_token == 0)
      {
        card_name_or_token = is_token(player, card);
        if ((card_name_or_token == 0) && (instance->eot_toughness == 1))
        {

          card_name_or_token = get_card_name(player, card);
          scnprintf(buffer, 0x32, "Pay how much for %c?", card_name_or_token);
          if (cost_contribution_or_result < opponent_mana)
          {
            cost_contribution_or_result = opponent_mana;
          }
          cost_contribution_or_result =
              choose_a_number(opponent_or_id, buffer, cost_contribution_or_result);
          charge_mana(opponent_or_id, COLOR_ANY, cost_contribution_or_result);
          opponent_or_id = get_id(player, card);
          spell_cost = cards_ptr[opponent_or_id];
          charge_mana_multi(player, updated_cost - cost_contribution_or_result,
                            (unsigned int)spell_cost->req.req_black, (unsigned int)spell_cost->req.req_blue,
                            (unsigned int)spell_cost->req.req_green, (unsigned int)spell_cost->req.req_red,
                            (unsigned int)spell_cost->req.req_white);
        }
      }
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x2025fa0; original cards/battlebond.c:40. */
static void partner_with(int player, int card, event_t event, int id)
{
  int result;
  const char *partner_name;
  test_definition_t this_test;

  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event != EVENT_ETB)
  {
    return;
  }
  default_test_definition(&this_test, 2);
  this_test.id = id;
  partner_name = get_card_name_by_id(id);
  scnprintf(this_test.message, 100, "Select a card named %s.", partner_name);
  new_global_tutor(player, player, 1, 1, 0, 5, &this_test);
  return;
}

/* ManalinkEh 2022: 0x2026070; original cards/battlebond.c:67. */
int card_arcane_artisan(int player, int card, event_t event)
{
  /* Arcane Artisan	|2|U
   * Creature - Human Wizard 0/3
   * |2|U, |T: Target player draws a card, then exiles a card from their hand. If a creature card is exiled this way, that player creates a token that's a copy of that card.
   * When ~ leaves the battlefield, exile all tokens created with it at the beginning of the next end step. */

  int i;
  int csvid;
  int selected_hand_card;
  card_instance_t *instance;
  int scan_card_or_creature;
  int p;
  token_generation_t token;
  target_definition_t td;
  test_definition_t this_test;

  leaves_play(player, card, event, 1, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {

    for (i = 0; i < 2; ++i)
    {
      p = i == 0 ? player : 1 - player;
      for (scan_card_or_creature = active_cards_count[p] - 1; scan_card_or_creature >= 0;
           --scan_card_or_creature)
      {
        instance = in_play(p, scan_card_or_creature);
        if (instance != NULL)
        {
          if (is_what(p, scan_card_or_creature, 2) != 0)
          {

            if (is_token(p, scan_card_or_creature) != 0)
            {
              instance = get_card_instance(p, scan_card_or_creature);
              if (((-1 < instance->damage_source_player) &&
                   (player == instance->damage_source_player)) &&
                  (card == instance->damage_source_card))
              {
                kill_card(p, scan_card_or_creature, KILL_EXILE);
              }
            }
          }
        }
      }
    }
  }
  else if (((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
           ((event == EVENT_RESOLVE_ACTIVATION || (event == EVENT_CLEANUP))))
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      instance = get_card_instance(player, card);
      new_default_test_definition(&this_test, 0, "Select a card to exile.");
      draw_a_card(instance->targets[0].player);
      selected_hand_card = instance->targets[0].player;
      selected_hand_card =
          new_select_a_card(selected_hand_card, selected_hand_card, 4, 1, 3, -1, &this_test);
      if (-1 < selected_hand_card)
      {

        scan_card_or_creature = is_what(instance->targets[0].player, selected_hand_card, 2);
        csvid = get_original_id(instance->targets[0].player, selected_hand_card);
        rfg_card_in_hand(instance->targets[0].player, selected_hand_card);
        if (scan_card_or_creature != 0)
        {
          default_token_definition(player, card, csvid, &token);
          token.t_player = instance->targets[0].player;
          generate_token(&token);
        }
      }
    }
    return generic_activated_ability(player, card, event, 5, 2, 0, 1, 0, 0, 0, 0, &td,
                                     "TARGET_PLAYER");
  }
  return 0;
}

/* ManalinkEh 2022: 0x2026390; original cards/battlebond.c:118. */
int card_archfiend_of_despair(int player, int card, event_t event)
{
  /* Archfiend of Despair	|6|B|B
   * Creature - Demon 6/6
   * Flying
   * Your opponents can't gain life.
   * At the beginning of each end step, each opponent loses life equal to the life that player lost this turn. */

  int result;
  card_instance_t *instance;

  result = eot_trigger(player, card, event);
  if (result != 0)
  {
    result = get_trap_condition(1 - player, 2);
    lose_life(1 - player, result);
  }
  instance = in_play(player, card);
  if ((((event == EVENT_STATIC_EFFECTS) && (instance != NULL)) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      player_bits[(1 - player)] =
          player_bits[(1 - player)] | 0x200000;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2025f40; original cards/battlebond.c:148. */
static int archon_of_valors_reach_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (((event == EVENT_STATIC_EFFECTS) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player < '\0')
    {
      kill_card(player, card, KILL_EXILE);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2026450; original cards/battlebond.c:158. */
int card_archon_of_valors_reach(int player, int card, event_t event)
{
  /* Archon of Valor's Reach	|4|G|W
   * Creature - Archon 5/6
   * Flying, vigilance, trample
   * As ~ enters the battlefield, choose artifact, enchantment, instant, sorcery, or planeswalker.
   * Players can't cast spells of the chosen type. */

  card_instance_t *instance;
  int card_00;
  int legacycard;
  card_instance_t *scanned_legacy;
  card_instance_t *type_storage;
  type_t chosen_or_spell_types;
  int result;
  type_t types[5];

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  instance = get_card_instance(player, card);
  vigilance(player, card, event);
  if (event == EVENT_ETB)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {

      types[0] = TYPE_ARTIFACT;
      types[1] = TYPE_ENCHANTMENT;
      types[2] = TYPE_INTERRUPT | TYPE_INSTANT;
      types[3] = TYPE_SORCERY;
      types[4] = TARGET_TYPE_PLANESWALKER;
      result = internal_rand(4);
      result = do_dialog(player, player, card, -1, -1,
                         " Artifact\n Enchantment\n Instant\n Sorcery\n Planeswalker", result);
      card_00 = create_targetted_legacy_effect_no_repeat(
          player, card, archon_of_valors_reach_legacy, player, card);
      add_status(player, card_00, 0x10000);
      legacycard = create_targetted_legacy_effect(player, card, empty, player, card);
      alternate_legacy_text(result + 1, player, legacycard);
      type_storage = get_card_instance(player, card_00);
      if (type_storage->targets[1].player < 0)
      {
        type_storage->targets[1].player = 0;
      }
      if (instance->targets[1].player < 0)
      {
        instance->targets[1].player = 0;
      }
      chosen_or_spell_types = types[result];
      type_storage->targets[1].player = type_storage->targets[1].player | chosen_or_spell_types;
      instance->targets[1].player = instance->targets[1].player | chosen_or_spell_types;
    }
  }
  else if (event == EVENT_MODIFY_COST_GLOBAL)
  {

    type_storage = instance;
    if (0 < active_cards_count[player])
    {
      result = 0;
      do
      {
        scanned_legacy = in_play(player, result);
        if (((((scanned_legacy != NULL) &&
               (scanned_legacy->internal_card_id == LEGACY_EFFECT_CUSTOM)) &&
              (scanned_legacy->info_slot_fn == archon_of_valors_reach_legacy)) &&
             ((player == scanned_legacy->damage_source_player &&
               (card == scanned_legacy->damage_source_card)))) &&
            ((player == scanned_legacy->damage_target_player &&
              (card == scanned_legacy->damage_target_card))))
        {
          if (-1 < result)
          {
            type_storage = get_card_instance(player, result);
          }
          break;
        }
        result = result + 1;
      } while (result < active_cards_count[player]);
    }
    if ((-1 < type_storage->targets[1].player) &&
        (chosen_or_spell_types = get_type(affected_card_controller, affected_card),
         (type_storage->targets[1].player & chosen_or_spell_types) != TYPE_NONE))
    {
      infinite_casting_cost();
    }
    if ((-1 < instance->targets[1].player) &&
        (chosen_or_spell_types = get_type(affected_card_controller, affected_card),
         (type_storage->targets[1].player & chosen_or_spell_types) != TYPE_NONE))
    {
      infinite_casting_cost();
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x20266e0; original cards/battlebond.c:199. */
int card_arena_rector(int player, int card, event_t event)
{
  /* Arena Rector	|3|W
   * Creature - Human Cleric 1/2
   * When ~ dies, you may exile it. If you do, search your library for a planeswalker card, put it onto the battlefield, then shuffle your library. */

  int result;
  test_definition_t test;

  result = duh_mode(player);
  result = this_dies_trigger(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (result != 0)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      result = exile_from_owners_graveyard(player, card);
      if (result != 0)
      {
        new_default_test_definition(&test, 0xff, "Select a Planeswalker card.");
        test.subtype = 0x1006;
        new_global_tutor(player, player, 1, 4, 0, 2, &test);
      }
    }
  }
  return 0;
}

/* Aurora Champion	|2|W
 * Creature - Elf Warrior 3/2
 * Whenever ~ attacks, if your team controls another Warrior, tap target creature. */

/* Azra Bladeseeker	|2|R
 * Creature - Azra Warrior 3/2
 * When ~ enters the battlefield, each player on your team may discard a card, then each player who discarded a card this way draws a card. */

/* Azra Oddsmaker	|1|B|R
 * Creature - Azra Warrior 3/3
 * At the beginning of combat on your turn, you may discard a card. If you do, choose a creature. Whenever that creature deals combat damage to a player this turn, you draw two cards. */

/* Blaring Captain	|3|B
 * Creature - Azra Warrior 2/2
 * Partner with Blaring Recruiter
 * Whenever ~ attacks, attacking Warriors get +1/+1 until end of turn. */

/* Blaring Recruiter	|3|W
 * Creature - Elf Warrior 2/2
 * Partner with Blaring Captain
 * |2|W: Create a 1/1 |Swhite Warrior creature token. */

/* ManalinkEh 2022: 0x2025e30; original cards/battlebond.c:240. */
int bonus_round_legacy(int player, int card, event_t event)
{
  if (trigger_condition == TRIGGER_SPELL_CAST && card == affected_card &&
      player == affected_card_controller && !is_token(trigger_cause_controller, trigger_cause))
  {
    test_definition_t test;
    default_test_definition(&test, 0x38);
    test.type_flag = 2;
    if (new_specific_spell_played(player, card, event, 2, 2, &test))
      copy_spell_from_stack(trigger_cause_controller, trigger_cause_controller, trigger_cause);
  }
  if (eot_trigger(player, card, event))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

/* Bloodborn Scoundrels	|5|B
 * Creature - Vampire Rogue 4/4
 * Assist
 * When ~ enters the battlefield, target opponent loses 2 life and you gain 2 life. */

/* ManalinkEh 2022: 0x20267c0; original cards/battlebond.c:256. */
int card_bonus_round(int player, int card, event_t event)
{
  /* Bonus Round	|1|R|R
   * Sorcery
   * Until end of turn, whenever a player casts an instant or sorcery spell, that player copies it and may choose new targets for the copy. */

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
      create_legacy_effect(player, card, bonus_round_legacy);
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  result = basic_spell(player, card, event);
  return result;
}

/* Bountiful Promenade	""
 * Land
 * ~ enters the battlefield tapped unless you have two or more opponents.
 * |T: Add |G or |W. */

/* ManalinkEh 2022: 0x2026870; original cards/battlebond.c:274. */
int card_bramble_sovereign(int player, int card, event_t event)
{
  /* Bramble Sovereign	|2|G|G
   * Creature - Dryad 4/4
   * Whenever another nontoken creature enters the battlefield, you may pay |1|G. If you do, that creature's controller creates a token that's a copy of that creature. */

  int result;


  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = has_mana_multi(player, 1, 0, 0, 1, 0, 0);
    if (result != 0)
    {
      result = is_humiliated(player, card, AL_TRIG);
      if (result == 0)
      {
        test_definition_t test;
        default_test_definition(&test, 2);
        test.not_me = 1;
        test.type_flag = 3;
        result = duh_mode(player);
        etb_ability_global(2, event, (result != 0) + 1, TYPE_NONE, &test,
                           player, card);
      }
    }
  }
  if (event == EVENT_ETB)
  {
    result = has_mana_multi(player, 1, 0, 0, 1, 0, 0);
    if (result != 0)
    {
      result = charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, 1, 0,
                                                 0, 1, 0, 0);
      if (result != 0)
      {
        token_generation_t token;
        copy_token_definition(player, card, &token, trigger_cause_controller,
                              trigger_cause);
        token.t_player = trigger_cause_controller;
        generate_token(&token);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2026a70; original cards/battlebond.c:300. */
int card_brightling(int player, int card, event_t event)
{
  /* Brightling	|1|W|W
   * Creature - Shapeshifter 3/3
   * |W: ~ gains vigilance until end of turn.
   * |W: ~ gains lifelink until end of turn.
   * |W: Return ~ to its owner's hand.
   * |1: ~ gets +1/-1 or -1/+1 until end of turn. */

  enum
  {
    CHOICE_VIGILANCE = 1,
    CHOICE_LIFELINK = 2,
    CHOICE_BOUNCE = 3,
    CHOICE_P1_M1 = 4,
    CHOICE_M1_P1 = 5
  };

  int choice;
  card_instance_t *instance;
  int32_t parent_player;
  unsigned int special_keywords;
  int dying_or_sick;
  unsigned int stored_keywords;
  int power_priority;
  int32_t activation_source_card;
  int toughness_priority;
  int white_available_mode_or_power;
  int generic_available_or_toughness;
  int vigilance_priority;
  uint32_t lifelink_priority;

  if ((uint32_t)(event - EVENT_POW_BOOST) < 2)
  {
    if (current_turn == player)
    {
      generic_available_or_toughness = -1;
      white_available_mode_or_power = 1;
    }
    else
    {
      generic_available_or_toughness = 1;
      white_available_mode_or_power = -1;
    }
    return generic_shade(player, card, event, 0, 1, 0, 0, 0, 0, 0, white_available_mode_or_power,
                         generic_available_or_toughness, 0, 0);
  }
  if (event == EVENT_CLEANUP)
  {

    instance = get_card_instance(player, card);
    instance->targets[1].card = 0;
    instance->targets[1].player = 0;
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0,
                                         NULL, NULL);
      }

      white_available_mode_or_power = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0,
                                                                0, 0, 0, 0, 0, 1, 0, NULL, NULL);
      generic_available_or_toughness = generic_activated_ability(
          player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
      special_keywords = get_special_abilities(player, card);
      stored_keywords = instance->targets[1].card;
      special_keywords =
          special_keywords | stored_keywords & (int)(stored_keywords ^ 0xffffffff) >> 0x1f;
      vigilance_priority = 1;
      if ((special_keywords & 8) == 0 && current_phase <= 0x14 && current_turn == player)
      {
        dying_or_sick = is_sick(player, card);
        vigilance_priority = ((dying_or_sick == 0) ? 10U : 1U);
      }
      lifelink_priority = 1;
      if ((special_keywords & 2) == 0 && current_phase < 0x19)
        lifelink_priority = 10;
      dying_or_sick = is_this_dying(player, card);
      if (current_turn == player)
      {
        toughness_priority = 1;
        power_priority = ((current_phase == 0x18) ? 8U : 1U);
      }
      else
      {
        power_priority = 1;
        toughness_priority = ((current_phase == 0x18) ? 8U : 1U);
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Vigilance",
                           white_available_mode_or_power, vigilance_priority, "Lifelink",
                           white_available_mode_or_power, lifelink_priority, "Bounce",
                           white_available_mode_or_power,
                           ((dying_or_sick == 0) ? 1U : 10U), "+1/-1",
                           generic_available_or_toughness, power_priority, "-1/+1",
                           generic_available_or_toughness, toughness_priority, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice - 1U < 3)
      {
        if (charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 1) == 0)
        {
          return 0;
        }
        stored_keywords = instance->targets[1].card;
        if ((int)stored_keywords < -1)
        {
          stored_keywords = 0;
        }
        if (choice == CHOICE_VIGILANCE)
        {
          stored_keywords = stored_keywords | 8;
        }
        else if (choice == CHOICE_LIFELINK)
        {
          stored_keywords = stored_keywords | 2;
        }
        instance->targets[1].card = stored_keywords;
        return 0;
      }
      if (1 < choice - 4U)
      {
        return 0;
      }
      charge_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0);
      return 0;
    }
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      return 0;
    }
  }
  instance = get_card_instance(player, card);
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }

  white_available_mode_or_power = instance->info_slot;
  if (white_available_mode_or_power == CHOICE_VIGILANCE)
  {
    activation_source_card = instance->parent_card;
    parent_player = instance->parent_controller;
    white_available_mode_or_power = 8;
  }
  else
  {
    if (white_available_mode_or_power != CHOICE_LIFELINK)
    {
      if (white_available_mode_or_power == CHOICE_BOUNCE)
      {
        bounce_permanent(instance->parent_controller, instance->parent_card);
        return 0;
      }
      if (white_available_mode_or_power == CHOICE_P1_M1)
      {
        activation_source_card = instance->parent_card;
        parent_player = instance->parent_controller;
        generic_available_or_toughness = -1;
        white_available_mode_or_power = 1;
      }
      else
      {
        if (white_available_mode_or_power != CHOICE_M1_P1)
        {
          return 0;
        }
        activation_source_card = instance->parent_card;
        parent_player = instance->parent_controller;
        generic_available_or_toughness = 1;
        white_available_mode_or_power = -1;
      }
      pump_until_eot_merge_previous(parent_player, activation_source_card, parent_player,
                                    activation_source_card, white_available_mode_or_power,
                                    generic_available_or_toughness);
      return 0;
    }
    activation_source_card = instance->parent_card;
    parent_player = instance->parent_controller;
    white_available_mode_or_power = 2;
  }
  pump_ability_until_eot(parent_player, activation_source_card, parent_player,
                         activation_source_card, 0, 0, 0, white_available_mode_or_power);
  return 0;
}

/* Bring Down	|3|W
 * Sorcery
 * Assist
 * Destroy target creature with power 4 or greater. */

/* Bull-Rush Bruiser	|3|R
 * Creature - Minotaur Warrior 4/3
 * Whenever ~ attacks, if your team controls another Warrior, ~ gains first strike until end of turn. */

/* Chakram Retriever	|4|U
 * Creature - Elemental Hound 2/4
 * Partner with Chakram Slinger
 * Whenever you cast a spell during your turn, untap target creature. */

/* Chakram Slinger	|4|R
 * Creature - Human Warrior 2/4
 * Partner with Chakram Retriever
 * |R, |T: ~ deals 2 damage to target player or planeswalker. */

/* Charging Binox	|7|G
 * Creature - Beast 7/5
 * Assist
 * Trample */

/* Cheering Fanatic	|1|R
 * Creature - Goblin 2/2
 * Whenever ~ attacks, choose a card name. Spells with the chosen name cost |1 less to cast this turn. */

/* Combo Attack	|2|G
 * Sorcery
 * Two target creatures your team controls each deal damage equal to their power to target creature. */

/* Decorated Champion	|1|G
 * Creature - Elf Warrior 2/2
 * Whenever another Warrior enters the battlefield under your team's control, put a +1/+1 counter on ~. */

/* Dwarven Lightsmith	|5|W
 * Creature - Dwarf Cleric 3/4
 * Assist
 * When ~ enters the battlefield, creatures your team controls get +1/+1 until end of turn. */

/* Fan Favorite	|3|B
 * Creature - Human Rogue 2/2
 * Assist
 * |2: ~ gets +1/+1 until end of turn. Any player may activate this ability. */

/* Fumble	|1|U
 * Instant
 * Return target creature to its owner's hand. Gain control of all Auras and Equipment that were attached to it, then attach them to another creature. */

/* ManalinkEh 2022: 0x20270b0; original cards/battlebond.c:453. */
int card_game_plan(int player, int card, event_t event)
{
  /* Game Plan	|5|U
   * Sorcery
   * Assist
   * Each player shuffles their hand and graveyard into their library, then draws seven cards. Exile ~. */

  card_ptr_t *spell_cost;
  int cost_contribution_or_result;
  int opponent_or_id;
  int opponent_mana;
  int updated_cost;
  card_instance_t *instance;
  int card_name_or_token;
  char buffer[50];

  cost_contribution_or_result = get_updated_casting_cost(player, card, -1, event, -1);
  opponent_or_id = 1 - player;
  opponent_mana = has_mana(opponent_or_id, 7, 1);
  updated_cost = get_updated_casting_cost(player, card, -1, event, -1);
  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {

    if ((player != 1) || (ai_is_speculating != 1))
    {
      cost_contribution_or_result = get_id(player, card);
      spell_cost = cards_ptr[cost_contribution_or_result];
      cost_contribution_or_result = has_mana(opponent_or_id, 7, 1);
      if (has_mana_multi(player,
                         updated_cost - cost_contribution_or_result &
                             (int)(updated_cost - cost_contribution_or_result ^ 0xffffffffU) >>
                                 0x1f,
                         (unsigned int)spell_cost->req.req_black, (unsigned int)spell_cost->req.req_blue,
                         (unsigned int)spell_cost->req.req_green, (unsigned int)spell_cost->req.req_red,
                         (unsigned int)spell_cost->req.req_white) != 0)
      {
        instance->eot_toughness = 1;
        null_casting_cost(player, card);
      }
    }
  }
  else
  {
    if (event != EVENT_CAST_SPELL)
    {
      cost_contribution_or_result = resolve_timetwister(player, card, event);
      if (event == EVENT_RESOLVE_SPELL)
      {
        kill_card(player, card, KILL_EXILE);
      }
      return cost_contribution_or_result;
    }
    if ((card == affected_card) && (player == affected_card_controller))
    {
      if (played_for_free(player, card) == 0)
      {
        card_name_or_token = is_token(player, card);
        if ((card_name_or_token == 0) && (instance->eot_toughness == 1))
        {

          card_name_or_token = get_card_name(player, card);
          scnprintf(buffer, 0x32, "Pay how much for %c?", card_name_or_token);
          if (cost_contribution_or_result < opponent_mana)
          {
            cost_contribution_or_result = opponent_mana;
          }
          cost_contribution_or_result =
              choose_a_number(opponent_or_id, buffer, cost_contribution_or_result);
          charge_mana(opponent_or_id, COLOR_ANY, cost_contribution_or_result);
          opponent_or_id = get_id(player, card);
          spell_cost = cards_ptr[opponent_or_id];
          charge_mana_multi(player, updated_cost - cost_contribution_or_result,
                            (unsigned int)spell_cost->req.req_black, (unsigned int)spell_cost->req.req_blue,
                            (unsigned int)spell_cost->req.req_green, (unsigned int)spell_cost->req.req_red,
                            (unsigned int)spell_cost->req.req_white);
        }
      }
    }
  }
  return resolve_timetwister(player, card, event);
}

/* Gang Up	|X|B
 * Instant
 * Assist
 * Destroy target creature with power X or less. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2027370; original cards/battlebond.c:472. */
int card_generous_patron(int player, int card, event_t event)
{
  /* Generous Patron	|2|G
   * Creature - Elf Advisor 1/4
   * When ~ enters the battlefield, support 2.
   * Whenever you put one or more counters on a creature you don't control, draw a card. */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    if ((event == EVENT_COUNTERS) && (attacking_card_controller == player))
    {
      result = is_what(affected_card_controller, affected_card, 2);
      if ((result != 0) && (affected_card_controller != player))
      {
        result = is_humiliated(player, card, AL_TRIG);
        if (result == 0)
        {
          draw_a_card(player);
        }
      }
    }
    return 0;
  }
  support(player, card, 2);
  return 0;
}
#endif

/* Gorm the Great	|3|G
 * Legendary Creature - Giant Warrior 2/7
 * Partner with Virtus the Veiled
 * Vigilance
 * ~ must be blocked if able, and ~ must be blocked by two or more creatures if able. */

/* Grothama, All-Devouring	|3|G|G
 * Legendary Creature - Wurm 10/8
 * Other creatures have "Whenever this creature attacks, you may have it fight ~."
 * When ~ leaves the battlefield, each player draws cards equal to the amount of damage dealt to ~ this turn by sources they controlled. */

/* Huddle Up	|2|U
 * Sorcery
 * Assist
 * Two target players each draw a card. */

/* Impetuous Protege	|2|R
 * Creature - Human Warrior 0/4
 * Partner with Proud Mentor
 * Whenever ~ attacks, it gets +X/+0 until end of turn, where X is the greatest power among tapped creatures your opponents control. */

/* Inner Demon	|2|B|B
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+2, has flying, and is a Demon in addition to its other types.
 * When ~ enters the battlefield, all non-Demon creatures get -2/-2 until end of turn. */

/* Jubilant Mascot	|2|W
 * Creature - Homunculus 1/1
 * At the beginning of combat on your turn, you may pay |3|W. If you do, support 2. */

/* Jungle Wayfinder	|2|G
 * Creature - Elf Warrior 3/3
 * When ~ enters the battlefield, each player may search their library for a basic land card, reveal it, put it into their hand, then shuffle their library. */

/* ManalinkEh 2022: 0x2027430; original cards/battlebond.c:524. */
int card_khorvath_brightflame(int player, int card, event_t event)
{
  /* Khorvath Brightflame	|5|R
   * Legendary Creature - Dragon 3/4
   * Partner with Sylvia Brightspear
   * Flying, haste
   * Knights your team controls have flying and haste. */

  check_legend_rule(player, card, event);
  haste(player, card, event);
  partner_with(player, card, event, 0x48d3);
  boost_subtype(player, card, event, SUBTYPE_KNIGHT, 0, 0, 0x20, 4,
                BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  return 0;
}

/* Khorvath's Fury	|4|R
 * Sorcery
 * For each player, choose friend or foe. Each friend discards all cards from their hand, then draws that many cards plus one. ~ deals damage to each foe equal to the number of cards in their hand. */

/* ManalinkEh 2022: 0x20274c0; original cards/battlebond.c:543. */
int card_krav_the_unredeemed(int player, int card, event_t event)
{
  /* Krav, the Unredeemed	|4|B
   * Legendary Creature - Demon 3/3
   * Partner with Regna, the Redeemer
   * |B, Sacrifice X creatures: Target player draws X cards and gains X life. Put X +1/+1 counters on ~. */

  int sacrifice_count_or_parent_player;
  unsigned int chosen_sacrifice;
  int parent_card_or_available;
  card_instance_t *parent;
  int *sacrifice_cursor;
  int *next_sacrifice;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t test;
  int sacrifices[100];

  check_legend_rule(player, card, event);
  partner_with(player, card, event, 0x48c1);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (can_use_activated_abilities(player, card) == 0)
      {
        return 0;
      }
      if (has_mana_for_activated_ability(player, card, 0, 1, 0, 0, 0, 0) == 0)
      {
        return 0;
      }
      if (target_available(player, card, &td) == 0)
      {
        return 0;
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {
      if (charge_mana_for_activated_ability(player, card, 0, 1, 0, 0, 0, 0) == 0)
      {
        return 0;
      }
      if (pick_target(&td, "TARGET_PLAYER") == 0)
      {
        return 0;
      }

      new_default_test_definition(&test, 2, "Select creature to sacrifice.");
      sacrifice_count_or_parent_player = 0;
      instance->number_of_targets = '\x01';
      instance->info_slot = 0;

      while ((parent_card_or_available = new_can_sacrifice_as_cost(player, card, &test),
              parent_card_or_available != 0 &&
                  (chosen_sacrifice = new_sacrifice(
                       player, card, player,
                       SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK | SAC_DONE, &test),
                   chosen_sacrifice != 0)))
      {
        sacrifices[sacrifice_count_or_parent_player] = chosen_sacrifice >> 0x18;
        sacrifice_count_or_parent_player = sacrifice_count_or_parent_player + 1;
      }
      if (0 < sacrifice_count_or_parent_player)
      {
        sacrifice_cursor = sacrifices;
        do
        {
          next_sacrifice = sacrifice_cursor + 1;
          kill_card(player, *sacrifice_cursor, KILL_SACRIFICE);
          sacrifice_cursor = next_sacrifice;
        } while (next_sacrifice != sacrifices + sacrifice_count_or_parent_player);
      }
      instance->info_slot = sacrifice_count_or_parent_player;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0);
  }
  td.zone = TARGET_ZONE_PLAYERS;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    sacrifice_count_or_parent_player = instance->parent_controller;
    parent_card_or_available = instance->parent_card;
    parent = get_card_instance(sacrifice_count_or_parent_player, parent_card_or_available);
    if (0 < parent->info_slot)
    {
      draw_cards(parent->targets[0].player, parent->info_slot);
      gain_life(parent->targets[0].player, parent->info_slot);
      if ((sacrifice_count_or_parent_player != -1) && (parent_card_or_available != -1))
      {
        add_counters(sacrifice_count_or_parent_player, parent_card_or_available,
                     sacrifice_count_or_parent_player, parent_card_or_available, COUNTER_P1_P1,
                     parent->info_slot);
        return 0;
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20277e0; original cards/battlebond.c:610. */
int get_random_permanent(int player, test_definition_t *this_test)
{
  card_instance_t *instance;
  int selected_card;
  int candidate_count_or_result;
  int card;
  test_definition_t default_test;
  int prm[151];

  if (this_test == NULL)
  {
    this_test = &default_test;
    default_test_definition(this_test, 0x47);
  }
  candidate_count_or_result = active_cards_count[player];
  int32_t permanents = (uint32_t)candidate_count_or_result - (uint32_t)hand_count[player];
  if (permanents <= 0)
    return -1;
  if ((trace_mode & 2U) != 0)
  {
    if (current_turn == 1)
    {
      receive_network_packet(1, 24);
      return dblword_628C10;
    }
    prm[0] = -1;
  }
  else if (candidate_count_or_result < 1)
    return -1;
  selected_card = -1;
  if (candidate_count_or_result > 0)
  {
    prm[0] = -1;
    card = 0;
    candidate_count_or_result = 0;
    do
    {
      instance = in_play(player, card);
      if ((instance != NULL) && (new_make_test_in_play(player, card, -1, this_test) != 0))
      {
        prm[candidate_count_or_result] = card;
        candidate_count_or_result = candidate_count_or_result + 1;
      }
      card = card + 1;
    } while (card < active_cards_count[player]);
    selected_card = prm[0];
    if (1 < candidate_count_or_result)
    {
      candidate_count_or_result = internal_rand(candidate_count_or_result);
      selected_card = prm[candidate_count_or_result];
    }
  }
  if ((trace_mode & 2U) != 0)
  {
    byte_628C0C = '\x18';
    dblword_628C10 = selected_card;
    send_network_packet(0, 24);
    return selected_card;
  }
  return selected_card;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2027960; original cards/battlebond.c:644. */
int card_last_one_standing(int player, int card, event_t event)
{
  /* Last One Standing	|1|B|R
   * Sorcery
   * Choose a creature at random, then destroy the rest. */

  int chosen_player;
  int own_creatures_or_selected;
  int opponent_creatures;
  const char *chosen_name;
  char buffer[100];
  test_definition_t this_test;

  if (event == EVENT_CAN_CAST)
    return basic_spell(player, card, event);
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card)
    {
      return 0;
    }
    if (player != affected_card_controller)
    {
      return 0;
    }
    return basic_spell(player, card, event);
  }
  if (event != EVENT_RESOLVE_SPELL)
  {
    if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
    return basic_spell(player, card, event);
  }

  default_test_definition(&this_test, 2);
  own_creatures_or_selected = check_battlefield_for_subtype(player, 2, -1);
  chosen_player = 1 - player;
  opponent_creatures = check_battlefield_for_subtype(chosen_player, 2, -1);
  if (own_creatures_or_selected == 0 && opponent_creatures == 0)
    get_random_permanent(-1, &this_test);
  else
  {
    if (own_creatures_or_selected != 0)
      chosen_player = opponent_creatures != 0 ? internal_rand(1) : player;
    own_creatures_or_selected = get_random_permanent(chosen_player, &this_test);
    if (chosen_player != -1)
    {
      chosen_name = get_card_name_by_id(get_id(chosen_player, own_creatures_or_selected));
      scnprintf(buffer, 100, "Last One Standing chose %s at random", chosen_name);
      dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_FULLCARD(chosen_player,
                  own_creatures_or_selected), DLG_HEADER(buffer), DLG_NO_STORAGE, DLG_NO_CANCEL, 0, 0, 0,
                  0);
      state_untargettable(chosen_player, own_creatures_or_selected, 1);
      new_manipulate_all(player, card, 2, &this_test, ACT_KILL_DESTROY);
      state_untargettable(chosen_player, own_creatures_or_selected, 0);
    }
  }
  kill_card(player, card, KILL_DESTROY);
  return basic_spell(player, card, event);
}
#endif

/* Lava-Field Overlord	|7|R|R
 * Creature - Dragon 5/4
 * Assist
 * Flying
 * When ~ enters the battlefield, it deals 4 damage to target creature an opponent controls. */

/* Ley Weaver	|3|G
 * Creature - Human Druid 2/2
 * Partner with Lore Weaver
 * |T: Untap two target lands. */

/* Lore Weaver	|3|U
 * Creature - Human Wizard 2/2
 * Partner with Ley Weaver
 * |5|U|U: Target player draws two cards. */

/* Luxury Suite	""
 * Land
 * ~ enters the battlefield tapped unless you have two or more opponents.
 * |T: Add |B or |R. */

/* Magma Hellion	|6|R
 * Creature - Hellion 5/4
 * Assist
 * Trample, haste */

/* Mindblade Render	|1|B
 * Creature - Azra Warrior 1/3
 * Whenever your opponents are dealt combat damage, if any of that damage was dealt by a Warrior, you draw a card and you lose 1 life. */

/* Morphic Pool	""
 * Land
 * ~ enters the battlefield tapped unless you have two or more opponents.
 * |T: Add |U or |B. */

/* ManalinkEh 2022: 0x2027bc0; original cards/battlebond.c:710. */
int card_najeela_the_blade_blossom(int player, int card, event_t event)
{
  /* Najeela, the Blade-Blossom	|2|R
   * Legendary Creature - Human Warrior 3/2
   * Whenever a Warrior attacks, you may have its controller create a 1/1 |Swhite Warrior creature token that's tapped and attacking.
   * |W|U|B|R|G: Untap all attacking creatures. They gain trample, lifelink, and haste until end of turn. After this phase, there is an additional combat phase. Activate this ability only during combat. */

  int32_t attacker_count;
  xtrigger_t attack_trigger;
  int trigger_mode;
  card_instance_t *instance;

  check_legend_rule(player, card, event);
  attack_trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    attack_trigger = xtrigger_impl_value_dont_use_directly;
  }
  if ((event == EVENT_DECLARE_ATTACKERS) || (attack_trigger == XTRIGGER_ATTACKING))
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.subtype = 0xd2;
    store_attackers(player, card, event, 0, 2, -1, &test);
  }
  trigger_mode = duh_mode(player);
  dat_ability(player, card, event, (trigger_mode != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, 1);
  if (event == EVENT_DAT_ABILITY)
  {
    token_generation_t token;

    instance = get_card_instance(player, card);
    attacker_count = instance->info_slot;
    default_token_definition(current_turn, card, CARD_ID_WARRIOR, &token);
    token.qty = (char)attacker_count;
    token.action = 3;
    token.color_forced = ' ';
    generate_token(&token);
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 2);
    this_test.state = 4;
    new_manipulate_all(player, card, 2, &this_test, ACT_UNTAP);
    pump_creatures_until_eot_no_repeat(player, card, 2, 1, 0x80, 6, &this_test);
    extra_combat_effect(player, card, EVENT_RESOLVE_ACTIVATION, 0, NULL, 1);
  }
  else if ((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE) && (event != EVENT_CLEANUP))
  {
    return 0;
  }
  return generic_activated_ability(player, card, event, 0x1000, 0, 1, 1, 1, 1, 1, 0, NULL, NULL);
}

/* Nimbus Champion	|5|U
 * Creature - Avatar Warrior 4/4
 * Flying
 * Whenever ~ attacks, you may return target creature to its owner's hand if that creature's power is less than or equal to the number of Warriors your team controls. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2027e50; original cards/battlebond.c:757. */
int card_okaun_eye_of_chaos(int player, int card, event_t event)
{
  /* Okaun, Eye of Chaos	|4|R
   * Legendary Creature - Cyclops Berserker 3/3
   * Partner with Zndrsplt, Eye of Wisdom
   * At the beginning of combat on your turn, flip a coin until you lose a flip.
   * Whenever a player wins a coin flip, double ~'s power and toughness until end of turn. */

  int result;
  card_instance_t *instance;
  int power;
  check_legend_rule(player, card, event);
  partner_with(player, card, event, 0x48e0);
  if (event == EVENT_CHANGE_TYPE)
  {
    result = engine_any_legal_attacker(player);
    if ((result == 0) && (current_turn == player))
    {
      event_flags = event_flags | EA_FORCE_ATTACK;
    }
  }
  else if (event == EVENT_BEGIN_COMBAT)
  {
    if (current_turn == player)
    {
      do
      {
        result = flip_a_coin(player, card);
        instance = get_card_instance(player, card);
        instance->regen_status =
            instance->regen_status | (KEYWORD_RECALC_POWER | KEYWORD_RECALC_TOUGHNESS);
      } while (result == 1);
    }
  }
  else if ((event == EVENT_COIN_FLIP_WON_OR_LOST) && (affected_card == 1))
  {
    result = get_toughness(player, card);
    power = get_power(player, card);
    pump_until_eot_merge_previous(player, card, player, card, power, result);
  }
  return 0;
}
#endif

/* Out of Bounds	|3|U
 * Instant
 * Assist
 * Counter target spell. */

/* ManalinkEh 2022: 0x2027f60; original cards/battlebond.c:790. */
int card_pirs_whim(int player, int card, event_t event)
{
  /* Pir's Whim	|3|G
   * Sorcery
   * For each player, choose friend or foe. Each friend searches their library for a land card, puts it onto the battlefield tapped, then shuffles their library. Each foe sacrifices an artifact or enchantment they control. */

  int result;
  unsigned int player_00;
  char *buf;
  unsigned int player_pass;
  int foe[2];
  char buffer_caster[50];
  char buffer_opp[50];
  char buffer_player[50];
  test_definition_t this_test;

  if (event == EVENT_RESOLVE_SPELL)
  {
    buf = buffer_opp;
    foe[0] = -1;
    foe[1] = -1;
    scnprintf(buffer_caster, 0x32, "Which are you?");
    scnprintf(buf, 0x32, "Which is %s?", opponent_name);
    scnprintf(buffer_player, 0x32, "Which is the player?");
    result = dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_HEADER(buffer_caster), DLG_RANDOM,
                         DLG_NO_CANCEL, DLG_NO_STORAGE, "Friend", 1, 10, "Foe", 1, 0xfffffff6, 0, 0, 0, 0);
    foe[player] = result + -1;
    if (player != 0)
    {
      buf = buffer_player;
    }
    result = dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_HEADER(buf), DLG_RANDOM, DLG_NO_CANCEL,
                         DLG_NO_STORAGE, "Friend", 1, 0xfffffff6, "Foe", 1, 10, 0, 0, 0, 0);

    player_pass = 0;
    foe[1 - player] = result + -1;
    do
    {
      player_00 = current_turn ^ player_pass;
      if (foe[player_00] == 0)
      {
        tutor_lands(player_00, TUTOR_PLAY_TAPPED, 1);
      }
      else
      {
        default_test_definition(&this_test, 0x44);
        new_sacrifice(player, card, player_00, SAC_CAUSED | SAC_NO_CANCEL, &this_test);
      }
      player_pass = player_pass + 1;
    } while (player_pass != 2);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x20281f0; original cards/battlebond.c:825. */
int card_pir_imaginative_rascal(int player, int card, event_t event)
{
  /* Pir, Imaginative Rascal	|2|G
   * Legendary Creature - Human 1/1
   * Partner with Toothy, Imaginary Friend
   * If one or more counters would be put on a permanent your team controls, that many plus one of each of those kinds of counters are put on that permanent instead. */

  check_legend_rule(player, card, event);
  partner_with(player, card, event, 0x48d8);
  return 0;
}

/* Play of the Game	|6|W|W
 * Sorcery
 * Assist
 * Exile all nonland permanents. */

/* Proud Mentor	|2|W
 * Creature - Human Warrior 1/1
 * Partner with Impetuous Protege
 * |W, |T: Tap target creature. */

/* Regna's Sanction	|3|W
 * Sorcery
 * For each player, choose friend or foe. Each friend puts a +1/+1 counter on each creature they control. Each foe chooses one untapped creature they control, then taps the rest. */

/* ManalinkEh 2022: 0x2028230; original cards/battlebond.c:850. */
int card_regna_the_redeemer(int player, int card, event_t event)
{
  /* Regna, the Redeemer	|5|W
   * Legendary Creature - Angel 4/4
   * Partner with Krav, the Unredeemed
   * Flying
   * At the beginning of each end step, if your team gained life this turn, create two 1/1 |Swhite Warrior creature tokens. */

  int result;
  token_generation_t token;

  check_legend_rule(player, card, event);
  partner_with(player, card, event, 0x48af);
  result = eot_trigger(player, card, event);
  if (result != 0)
  {
    result = get_trap_condition(player, 1);
    if (result != 0)
    {
      default_token_definition(player, card, CARD_ID_WARRIOR, &token);
      token.qty = '\x02';
      generate_token(&token);
      return 0;
    }
  }
  return 0;
}

/* Rowan Kenrith	|4|R|R
 * Legendary Planeswalker - Rowan (4)
 * +2: During target player's next turn, each creature that player controls attacks if able.
 * -2: ~ deals 3 damage to each tapped creature target player controls.
 * -8: Target player gets an emblem with "Whenever you activate an ability that isn't a mana ability, copy it. You may choose new targets for the copy."
 * Partner with Will Kenrith
 * ~ can be your commander. */

/* ManalinkEh 2022: 0x20282d0; original cards/battlebond.c:882. */
int card_rushblade_commander(int player, int card, event_t event)
{
  /* Rushblade Commander	|B|R
   * Creature - Azra Warrior 2/2
   * Warriors your team controls have haste. */

  boost_subtype(player, card, event, SUBTYPE_WARRIOR, 0, 0, 0, 4,
                BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  return 0;
}

/* Saltwater Stalwart	|3|U
 * Creature - Merfolk Warrior 2/4
 * Whenever ~ deals damage to an opponent, target player draws a card. */

/* Sea of Clouds	""
 * Land
 * ~ enters the battlefield tapped unless you have two or more opponents.
 * |T: Add |W or |U. */

/* Sentinel Tower	|4
 * Artifact
 * Whenever an instant or sorcery spell is cast during your turn, ~ deals damage to any target equal to 1 plus the number of instant and sorcery spells cast before that spell this turn. */

/* Sickle Dancer	|2|B
 * Creature - Human Warrior 3/2
 * Whenever ~ attacks, if your team controls another Warrior, ~ gets +1/+1 until end of turn. */

/* Skystreamer	|4|W
 * Creature - Griffin 3/2
 * Assist
 * Flying
 * When ~ enters the battlefield, target player gains 4 life. */

/* Soaring Show-Off	|2|U
 * Creature - Bird Warrior 2/2
 * Flying
 * When ~ enters the battlefield, each player draws a card. */

/* Soulblade Corrupter	|4|B
 * Creature - Human Warrior 3/3
 * Partner with Soulblade Renewer
 * Deathtouch
 * Whenever a creature with a +1/+1 counter on it attacks one of your opponents, that creature gains deathtouch until end of turn. */

/* Soulblade Renewer	|4|G
 * Creature - Elf Warrior 2/2
 * Partner with Soulblade Corrupter
 * When ~ enters the battlefield, support 2. */

/* ManalinkEh 2022: 0x2028330; original cards/battlebond.c:930. */
int card_spellseeker(int player, int card, event_t event)
{
  /* Spellseeker	|2|U
   * Creature - Human Wizard 1/1
   * When ~ enters the battlefield, you may search your library for an instant or sorcery card with converted mana cost 2 or less, reveal it, put it into your hand, then shuffle your library. */

  int result;
  test_definition_t this_test;
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  new_default_test_definition(
      &this_test, 0x38, "Select an instant or sorcery card with converted mana cost 2 or less");
  this_test.cmc = 3;
  this_test.cmc_flag = 3;
  new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
  return 0;
}

/* Spellweaver Duo	|6|U
 * Creature - Human Wizard 4/4
 * Assist
 * When ~ enters the battlefield, you may return target tapped creature to its owner's hand. */

/* Spire Garden	""
 * Land
 * ~ enters the battlefield tapped unless you have two or more opponents.
 * |T: Add |R or |G. */

/* Stadium Vendors	|3|R
 * Creature - Goblin 3/3
 * When ~ enters the battlefield, choose a player. That player adds two mana of any one color they choose. */

/* ManalinkEh 2022: 0x20283f0; original cards/battlebond.c:960. */
int card_stolen_strategy(int player, int card, event_t event)
{
  /* Stolen Strategy	|4|R
   * Enchantment
   * At the beginning of your upkeep, exile the top card of each opponent's library. Until end of turn, you may cast nonland cards from among those exiled cards, and you may spend mana as though it were mana of any color to cast those spells. */

  int result;
  int opponent;

  upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {

      opponent = 1 - player;
      result = *deck_ptr[opponent];
      if (-1 < result)
      {
        rfg_top_card_of_deck(opponent);
        if ((cards_data[result].type & 1) == 0)
        {
          create_may_play_card_from_exile_effect(player, card, opponent,
                                                 (unsigned int)cards_data[result].id,
                                                 MPCFE_UNTIL_EOT | MPCFE_FOR_CMC);
        }
      }
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* Stunning Reversal	|3|B
 * Instant
 * The next time you would lose the game this turn, instead draw seven cards and your life total becomes 1.
 * Exile ~. */

/* ManalinkEh 2022: 0x20284c0; original cards/battlebond.c:983. */
int card_sylvia_brightspear(int player, int card, event_t event)
{
  /* Sylvia Brightspear	|2|W
   * Legendary Creature - Human Knight 2/2
   * Partner with Khorvath Brightflame
   * Double strike
   * Dragons your team controls have double strike. */

  check_legend_rule(player, card, event);
  partner_with(player, card, event, 0x48ad);
  boost_subtype(player, card, event, SUBTYPE_DRAGON, 0, 0, 0x40000000, 0,
                BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  return 0;
}

/* The Crowd Goes Wild	|X|G
 * Sorcery
 * Assist
 * Support X.
 * Each creature with a +1/+1 counter on it gains trample until end of turn. */

/* Thrasher Brute	|3|B
 * Creature - Orc Warrior 4/3
 * Whenever ~ or another Warrior enters the battlefield under your team's control, target opponent loses 1 life and you gain 1 life. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2028540; original cards/battlebond.c:1007. */
int card_thrilling_encore(int player, int card, event_t event)
{
  /* Thrilling Encore	|4|B
   * Instant
   * Put onto the battlefield under your control all creature cards in all graveyards that were put there from the battlefield this turn. */

  int result;
  if (event == EVENT_RESOLVE_SPELL)
  {
    seek_grave_to_reanimate_this_turn(player, 2, -0x7ffffffe, REANIMATE_DEFAULT);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}
#endif

/* Together Forever	|W|W
 * Enchantment
 * When ~ enters the battlefield, support 2.
 * |1: Choose target creature with a counter on it. When that creature dies this turn, return that card to its owner's hand. */

/* ManalinkEh 2022: 0x20285b0; original cards/battlebond.c:1024. */
int card_toothy_imaginary_friend(int player, int card, event_t event)
{
  /* Toothy, Imaginary Friend	|3|U
   * Legendary Creature - Illusion 1/1
   * Partner with Pir, Imaginative Rascal
   * Whenever you draw a card, put a +1/+1 counter on ~.
   * When ~ leaves the battlefield, draw a card for each +1/+1 counter on it. */

  int result;

  check_legend_rule(player, card, event);
  partner_with(player, card, event, 0x48bd);
  result = card_drawn_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  }
  leaves_play(player, card, event, 1, 1);
  if (event != EVENT_LEAVES_PLAY_ABILITY)
  {
    return 0;
  }
  result = count_counters(player, card, COUNTER_P1_P1);
  draw_cards(player, result);
  return 0;
}

/* Vampire Charmseeker	|6|U|B
 * Creature - Vampire Wizard 3/4
 * Assist
 * Flying
 * When ~ enters the battlefield, return target instant, sorcery, or creature card from a graveyard to its owner's hand. */

/* Victory Chimes	|3
 * Artifact
 * Untap ~ during each other player's untap step.
 * |T: A player of your choice adds |C. */

/* Virtus the Veiled	|2|B
 * Legendary Creature - Azra Assassin 1/1
 * Partner with Gorm the Great
 * Deathtouch
 * Whenever ~ deals combat damage to a player, that player loses half their life, rounded up. */

/* Virtus's Maneuver	|2|B
 * Sorcery
 * For each player, choose friend or foe. Each friend returns a creature card from their graveyard to their hand. Each foe sacrifices a creature they control. */

/* Will Kenrith	|4|U|U
 * Legendary Planeswalker - Will (4)
 * +2: Until your next turn, up to two target creatures each have base power and toughness 0/3 and lose all abilities.
 * -2: Target player draws two cards. Until your next turn, instant, sorcery, and planeswalker spells that player casts cost |2 less to cast.
 * -8: Target player gets an emblem with "Whenever you cast an instant or sorcery spell, copy it. You may choose new targets for the copy."
 * Partner with Rowan Kenrith
 * ~ can be your commander. */

/* Zndrsplt's Judgment	|4|U
 * Sorcery
 * For each player, choose friend or foe. Each friend creates a token that's a copy of a creature they control. Each foe returns a creature they control to its owner's hand. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2028690; original cards/battlebond.c:1080. */
int card_zndrsplt_eye_of_wisdom(int player, int card, event_t event)
{
  /* Zndrsplt, Eye of Wisdom	|4|U
   * Legendary Creature - Homunculus 1/4
   * Partner with Okaun, Eye of Chaos
   * At the beginning of combat on your turn, flip a coin until you lose a flip.
   * Whenever a player wins a coin flip, draw a card. */

  int result;
  check_legend_rule(player, card, event);
  partner_with(player, card, event, 0x48ba);
  if (event == EVENT_CHANGE_TYPE)
  {
    result = engine_any_legal_attacker(player);
    if ((result == 0) && (current_turn == player))
    {
      event_flags = event_flags | EA_FORCE_ATTACK;
    }
  }
  else if (event == EVENT_BEGIN_COMBAT)
  {
    if (current_turn == player)
    {
      do
      {
        result = flip_a_coin(player, card);
        if (result != 1)
        {
          return 0;
        }
        result = flip_a_coin(player, card);
      } while (result == 1);
    }
  }
  else if ((event == EVENT_COIN_FLIP_WON_OR_LOST) && (affected_card == 1))
  {
    draw_a_card(player);
  }
  return 0;
}
#endif
