#include "manalink.h"

static int iid_has_cycling(int iid, intptr_t me, int player, int card);

static int luminous_broodmoth_effect(int player, int card, event_t event);

/* ManalinkEh 2022: 0x2288800; original cards/ikoria.c:2. */
int triome(int player, int card, event_t event)
{
  int result;

  result = is_humiliated(player, card, AL_TRIG);
  if (result == 0)
  {
    comes_into_play_tapped(player, card, event);
  }
  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = plain_mana_producer(player, card, event);
    return result;
  }
  return cycling(player, card, event, 3, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x22888b0; original cards/ikoria.c:191. */
int card_colossification(int player, int card, event_t event)
{
  /* Colossification	|5|G|G
   * Enchantment - Aura
   * Enchant creature
   * When this Aura enters, tap enchanted creature.
   * Enchanted creature gets +20/+20. */

  card_instance_t *instance;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    instance = get_card_instance(player, card);
    tap_card((int)instance->damage_target_player, instance->damage_target_card);
  }
  else
  {
    if (event == EVENT_CAN_CAST)
      return generic_aura(player, card, event, player, 0x14, 0x14, 0, 0, 0, 0, 0);
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
        return generic_aura(player, card, event, player, 0x14, 0x14, 0, 0, 0, 0, 0);
    }
    else if ((event == EVENT_RESOLVE_SPELL) ||
             ((event == EVENT_CAN_CHANGE_TARGET || (event == EVENT_CHANGE_TARGET))))
      return generic_aura(player, card, event, player, 0x14, 0x14, 0, 0, 0, 0, 0);
  }
  if ((((event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA) &&
       (((2 < (uint32_t)(event - EVENT_POWER) && (event != EVENT_RESOLVE_MOVING_AURA)) &&
         (event != EVENT_CHANGE_TYPE)))) &&
      (((event != EVENT_SET_COLOR && (event != EVENT_GET_SELECTED_CARD)) &&
        (event != EVENT_STATIC_EFFECTS))))
  {
    return 0;
  }
  return generic_aura(player, card, event, player, 0x14, 0x14, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x2288a10; original cards/ikoria.c:232. */
int card_cunning_nightbonder(int player, int card, event_t event)
{
  /* Cunning Nightbonder	|UB|UB
   * Creature - Human Rogue 2/2
   * Flash
   * Spells with flash you cast cost |1 less to cast and can't be countered. */

  int result;

  if (event == EVENT_MODIFY_COST_GLOBAL ||
      (event == EVENT_CAST_SPELL && !(card == affected_card && player == affected_card_controller)))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result != 0)
      return hybrid(player, card, event);

    if (((affected_card_controller == player) &&
         (result = is_what(player, affected_card, 0x47), result != 0)) &&
        ((result = get_original_internal_card_id(affected_card_controller, affected_card),
          (cards_data[result].type & 0x10) != 0 ||
              ((result = is_what(affected_card_controller, affected_card, 2),
                result != 0 && (result = check_battlefield_for_id(affected_card_controller, CARD_ID_TEFERI_MAGE_OF_ZHALFIR),
                                result != 0))))))
    {
      if (event == EVENT_MODIFY_COST_GLOBAL)
      {
        COST_COLORLESS = COST_COLORLESS + -1;
        return hybrid(player, card, event);
      }
      if (event == EVENT_CAST_SPELL)
      {
        cannot_be_countered(affected_card_controller, affected_card, EVENT_CAST_SPELL);
        return hybrid(player, card, event);
      }
    }
  }
  if (event == EVENT_CAN_CAST)
  {
    return flash(player, card, EVENT_CAN_CAST);
  }
  return hybrid(player, card, event);
}

/* ManalinkEh 2022: 0x2288b90; original cards/ikoria.c:270. */
int card_dire_tactics(int player, int card, event_t event)
{
  /* Dire Tactics	|W|B
   * Instant
   * Exile target creature. If you don't control a Human, you lose life equal to that creature's toughness. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      result = check_battlefield_for_subtype(player, 0x47, 0x5b);
      if (result == 0)
      {
        result = get_toughness(instance->targets[0].player, instance->targets[0].card);
        lose_life(player, result);
      }
      kill_target0(player, card, KILL_EXILE);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x2288d20; original cards/ikoria.c:309. */
int card_drannith_magistrate(int player, int card, event_t event)
{
  /* Drannith Magistrate	|1|W
   * Creature - Human Wizard 1/3
   * Your opponents can't cast spells from anywhere other than their hands. */

  card_instance_t *instance;
  int result;

  if ((event == EVENT_MODIFY_COST_GLOBAL) && (affected_card_controller != player))
  {
    instance = in_hand(affected_card_controller, affected_card);
    if (instance != NULL)
    {
      result = check_state(affected_card_controller, affected_card, 0x20);
      if (result == 0)
      {
        infinite_casting_cost();
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2288d90; original cards/ikoria.c:350. */
int card_eerie_ultimatum(int player, int card, event_t event)
{
  /* Eerie Ultimatum	|W|W|B|B|B|G|G
   * Sorcery
   * Return any number of permanent cards with different names from your graveyard to the battlefield. */

  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    test_definition_t test;
    /* Native selection storage is not initialized before the predicate reads it. */
    int chosen[2][151];
    new_default_test_definition(&test, 0x47, "Select a permanent card.");
    test.value_for_special_selection_function = (intptr_t)chosen[1];
    test.no_shuffle = 1;
    test.special_selection_function = test_not_in_array;
    test.id = 0x388;
    test.id_flag = 1;
    int shroud = graveyard_has_shroud(player);
    if (shroud == 0)
    {
      int num_chosen = 0;
      while (1)
      {
        int selected = new_select_a_card(player, player, 5, 0, 5, 0, &test);
        if (selected == -1)
          break;
        int iid = graveyard_ptr_mutable[player][selected];
        if (has_subtype_by_id(player, (unsigned int)cards_data[iid].id, SUBTYPE_AURA))
        {
          int temporary = add_card_to_hand(player, iid);
          if (temporary < 0)
            continue;
          add_state(player, temporary, 0x20);
          set_special_flags3(player, card, 0x40);
          --hand_count[player];
          int can_cast = call_card_function(player, temporary, EVENT_CAN_CAST);
          obliterate_card_and_recycle(player, temporary);
          if (can_cast == 0)
            continue;
        }
        chosen[0][num_chosen] = selected;
        chosen[1][num_chosen] = turn_card_in_grave_face_down(player, selected);
        ++num_chosen;
      }
      for (int i = 0; i < num_chosen; ++i)
        turn_card_in_grave_face_up(player, chosen[0][i], chosen[1][i]);

      /* Native zero-selection path also sorts a zero-length GNU VLA. */
      int to_reanimate[num_chosen];
      for (int i = 0; i < num_chosen; ++i)
        to_reanimate[i] = chosen[0][i];
      descending_qsort(num_chosen, to_reanimate);
      if (num_chosen > 0)
      {
        int reanimated[num_chosen];
        for (int i = 0; i < num_chosen; ++i)
        {
          reanimated[i] = reanimate_permanent(player, card, player, to_reanimate[i],
                                                   REANIMATE_DEFAULT);
          state_untargettable(player, reanimated[i], 1);
        }
        for (int i = shroud; i < num_chosen; ++i)
          state_untargettable(player, reanimated[i], 0);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  if (event == EVENT_CAN_CAST || event == EVENT_CAST_SPELL || event == EVENT_RESOLVE_SPELL ||
      event == EVENT_CAN_CHANGE_TARGET || event == EVENT_CHANGE_TARGET)
    return basic_spell(player, card, event);
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2289140; original cards/ikoria.c:411. */
int card_emergent_ultimatum(int player, int card, event_t event)
{
  /* Emergent Ultimatum	|B|B|G|G|G|U|U
   * Sorcery
   * Search your library for up to three monocolored cards with different names and exile them. An opponent chooses one of those cards. Shuffle that card into your library. You may cast the other cards without paying their mana costs. Exile ~. */

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if (card != affected_card || player != affected_card_controller)
        return 0;
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      int number = count_deck(player);
      if (number > 0)
      {
        if (number > 3)
          number = 3;
        iid_t searched[3] = {-1, -1, -1};
        test_definition_t test;
        new_default_test_definition(&test, TYPE_ANY, "Select a monocolored card.");
        test.value_for_special_selection_function = (intptr_t)searched;
        test.color_flag = 3;
        test.special_selection_function = test_not_in_array;
        int num_chosen = 0;
        do
        {
          int selected = new_select_a_card(player, player, 1, 0, 2, -1, &test);
          if (selected < 0)
          {
            if (num_chosen == 0)
              return 0;
            break;
          }
          int iid = deck_ptr[player][selected];
          searched[num_chosen++] = iid;
          add_csvid_to_rfg(player, (uint16_t)cards_data[iid].id);
          remove_card_from_deck(player, selected);
        } while (num_chosen < number);
        test.special_selection_function = NULL;
        memcpy(test.message, "Select a card to shuffle back into opponent's library.", 55);
        test.value_for_special_selection_function = -1;
        int selected =
            select_card_from_zone(1 - player, player, searched, num_chosen, 1, 2, 0, &test);
        remove_card_from_rfg(player, (uint16_t)cards_data[searched[selected]].id);
        put_iid_under_the_first_x_cards_of_library(player, searched[selected], 0);
        searched[selected] = -1;
        shuffle(player);
        --num_chosen;
        if (num_chosen == 0)
          return 0;
        int to_cast[num_chosen];
        int cast_count = 0;
        for (int i = 0; i != 3; ++i)
          if (searched[i] != -1)
            to_cast[cast_count++] = searched[i];
        memcpy(test.message, "Select a card to play for free", 31);
        while (cast_count > 0)
        {
          test_definition_t this_test2;
          new_default_test_definition(&this_test2, TYPE_ANY, "Select a card to play for free");
          int s = select_card_from_zone(player, player, to_cast, cast_count, 0, 2, -1, &test);
          if (s == -1)
            break;
          unsigned int csvid = (uint16_t)cards_data[to_cast[s]].id;
          for (int k = s; k < cast_count; ++k)
            to_cast[k] = to_cast[k + 1];
          --cast_count;
          play_card_in_exile_for_free(player, player, csvid);
        }
      }
      kill_card(player, card, KILL_EXILE);
    }
    else if (event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
      return 0;
  }
  return basic_spell(player, card, event);
}
#endif

/* ManalinkEh 2022: 0x22894f0; original cards/ikoria.c:564. */
int card_fire_prophecy(int player, int card, event_t event)
{
  /* Fire Prophecy	|1|R
   * Instant
   * ~ deals 3 damage to target creature. You may put a card from your hand on the bottom of your library. If you do, draw a card. */

  int result;
  target_definition_t td;
  test_definition_t test;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      damage_target0(player, card, 3);
      default_test_definition(&test, 0xff);
      result = new_global_tutor(player, player, 4, 10, 0, 3, &test);
      if (result != -2)
      {
        draw_a_card(player);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x2289690; original cards/ikoria.c:653. */
int card_general_kudro_of_drannith(int player, int card, event_t event)
{
  /* General Kudro of Drannith	|1|W|B
   * Legendary Creature - Human Soldier 3/3
   * Other Humans you control get +1/+1.
   * Whenever General Kudro or another Human you control enters, exile target card from an opponent's graveyard.
   * |2, Sacrifice two Humans: Destroy target creature with power 4 or greater. */

  int opponent_or_valid;
  int grave_target_or_result;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  boost_creature_type(player, card, event, SUBTYPE_HUMAN, 1, 1, 0, BCT_CONTROLLER_ONLY);
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 0x47);
    test.subtype = 0x5b;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    opponent_or_valid = 1 - player;
    if ((*graveyard_ptr_mutable[opponent_or_valid] != -1) &&
        (grave_target_or_result = graveyard_has_shroud(opponent_or_valid),
         grave_target_or_result == 0))
    {

      new_default_test_definition(&test, 0xff, "Select target card.");
      grave_target_or_result = new_select_target_from_grave(player, card, opponent_or_valid,
                                                            SFG_CANNOT_CANCEL, 1, &test, 0);
      if (grave_target_or_result != -1)
      {
        rfg_card_from_grave(opponent_or_valid, grave_target_or_result);
      }
    }
  }
  else if (((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
           ((event == EVENT_RESOLVE_ACTIVATION || (event == EVENT_CLEANUP))))
  {
    default_target_definition(player, card, &td, 2);
    td.power_requirement = 0x1004;
    if ((event == EVENT_RESOLVE_ACTIVATION) &&
        (opponent_or_valid = valid_target(&td), opponent_or_valid != 0))
    {
      kill_target0(player, card, KILL_DESTROY);
    }
    new_default_test_definition(&test, 0x47, "Select a Human to sacrifice.");
    test.subtype = 0x5b;
    test.qty = 2;
    return generic_activated_ability_with_sacrifice_as_cost(
        player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, 2, 0, 0, 0, 0, 0, 0, &td,
        "Select target creature with power 4 or greater.", TYPE_NONE, &test, SARV_0);
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2289940; original cards/ikoria.c:705. */
int card_genesis_ultimatum(int player, int card, event_t event)
{
  /* Genesis Ultimatum	|G|G|U|U|U|R|R
   * Sorcery
   * Look at the top five cards of your library. Put any number of permanent cards from among them onto the battlefield and the rest into your hand. Exile ~. */

  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card || player != affected_card_controller)
      return 0;
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    test_definition_t test;
    new_default_test_definition(&test, 0x47, "Select a permanent card to put into play.");
    int to_look = count_deck(player);
    if (to_look != 0)
    {
      if (to_look > 5)
        to_look = 5;
      int added[to_look];
      int count = 0;
      int remaining = to_look;
      do
      {
        test.create_minideck = (unsigned short)remaining;
        int selected = new_select_a_card(player, player, 1, 0, 5, -1, &test);
        if (selected == -1)
          break;
        added[count] = deck_ptr[player][selected];
        remove_card_from_deck(player, selected);
        ++count;
        --remaining;
      } while (remaining != 0);
      for (int i = 0; i < count; ++i)
      {
        int card_added = add_card_to_hand(player, added[i]);
        set_special_flags(player, card_added, 0x10000);
        put_into_play(player, card_added);
      }
      for (int i = 0; i < remaining; ++i)
      {
        add_card_to_hand(player, *deck_ptr[player]);
        remove_card_from_deck(player, 0);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  else if (event != EVENT_CAN_CAST && event != EVENT_CAN_CHANGE_TARGET &&
           event != EVENT_CHANGE_TARGET)
  {
    return 0;
  }
  return basic_spell(player, card, event);
}
#endif

/* ManalinkEh 2022: 0x2289b70; original cards/ikoria.c:753. */
int card_glimmerbell(int player, int card, event_t event)
{
  /* Glimmerbell	|1|U
   * Creature - Elemental Jellyfish 1/3
   * Flying
   * |1|U: Untap this creature. */

  card_instance_t *instance;
  int result;
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    untap_card(instance->parent_controller, instance->parent_card);
  }
  result = generic_activated_ability(player, card, event, 0, 1, 0, 1, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2289c30; original cards/ikoria.c:800. */
int card_gyruda_doom_of_depths(int player, int card, event_t event)
{
  /* Gyruda, Doom of Depths	|4|UB|UB
   * Legendary Creature - Demon Kraken 6/6
   * Companion - Your starting deck contains only cards with even mana values. (If this card is your chosen companion, you may put it into your hand from outside the game for |3 as a sorcery.)
   * When Gyruda enters, each player mills four cards. Put a creature card with an even mana value from among the milled cards onto the battlefield under your control. */

  int *deck_end;
  int i;
  unsigned int card_cmc;
  int *deck_cursor;

  if (event == EVENT_QUERY)
  {
    if (attacking_card == 2)
    {

      deck_cursor = deck_ptr[player];
      deck_end = deck_cursor + 500;
      do
      {
        if (*deck_cursor == -1)
          break;
        card_cmc = get_cmc_by_internal_id(*deck_cursor);
        if ((card_cmc & 1) != 0)
        {
          return 0;
        }
        deck_cursor = deck_cursor + 1;
      } while (deck_cursor != deck_end);
      i = 0;
      if (0 < active_cards_count[player])
      {
        do
        {
          card_cmc = get_cmc(player, i);
          if ((card_cmc & 1) != 0)
          {
            return 0;
          }
          i = i + 1;
        } while (i < active_cards_count[player]);
      }
      return 1;
    }
    check_legend_rule(player, card, EVENT_QUERY);
    etb_ability(player, card, EVENT_QUERY, RESOLVE_TRIGGER_MANDATORY);
  }
  else
  {
    check_legend_rule(player, card, event);
    etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (event == EVENT_ETB)
    {
      special_mill(player, card, 0x5337, 2, 4);
    }
  }
  return hybrid(player, card, event);
}

/* ManalinkEh 2022: 0x2289d70; original cards/ikoria.c:888. */
int card_imposing_vantasaur(int player, int card, event_t event)
{
  /* Imposing Vantasaur	|5|W
   * Creature - Dinosaur 3/6
   * Vigilance
   * Cycling |1 (|1, Discard this card: Draw a card.) */

  int result;
  vigilance(player, card, event);
  result = cycling(player, card, event, 1, 0, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x2289de0; original cards/ikoria.c:919. */
int card_inspired_ultimatum(int player, int card, event_t event)
{
  /* Inspired Ultimatum	|U|U|R|R|R|W|W
   * Sorcery
   * Target player gains 5 life, ~ deals 5 damage to any target, then you draw five cards. */

  card_instance_t *instance;
  int result;
  target_definition_t td_any;
  target_definition_t td_player;
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td_any, 0x102);
    td_any.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    default_target_definition(player, card, &td_player, 0);
    td_player.zone = TARGET_ZONE_PLAYERS;
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td_any, NULL, 2, NULL);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td_any, 0x102);
      td_any.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      default_target_definition(player, card, &td_player, 0);
      td_player.zone = TARGET_ZONE_PLAYERS;
      if ((card == affected_card) &&
          ((player == affected_card_controller &&
            (result = new_pick_target(&td_player, "Select target player to gain life.", 0, 0x81),
             result != 0))))
      {
        new_pick_target(&td_any, "Select target for damage.", 1, 0x81);
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td_any, 0x102);
    td_any.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    default_target_definition(player, card, &td_player, 0);
    td_player.zone = TARGET_ZONE_PLAYERS;
    if (event == EVENT_RESOLVE_SPELL)
    {
      instance = get_card_instance(player, card);
      result = valid_target(&td_player);
      if (result != 0)
      {
        gain_life(instance->targets[0].player, 5);
      }
      result = validate_target(player, card, &td_any, 1);
      if (result != 0)
      {
        damage_creature(instance->targets[1].player, instance->targets[1].card, 5, player, card);
      }
      result = valid_target(&td_player);
      if ((result != 0) || (result = validate_target(player, card, &td_any, 1), result != 0))
      {
        draw_cards(player, 5);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td_any, 0x102);
    td_any.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    default_target_definition(player, card, &td_player, 0);
  }
  return 0;
}

/* ManalinkEh 2022: 0x228a120; original cards/ikoria.c:966. */
int card_kaheera_the_orphanguard(int player, int card, event_t event)
{
  /* Kaheera, the Orphanguard	|1|GW|GW
   * Legendary Creature - Cat Beast 3/2
   * Companion - Each creature card in your starting deck is a Cat, Elemental, Nightmare, Dinosaur, or Beast card. (If this card is your chosen companion, you may put it into your hand from outside the game for |3 as a sorcery.)
   * Vigilance
   * Each other creature you control that's a Cat, Elemental, Nightmare, Dinosaur, or Beast gets +1/+1 and has vigilance. */

  int i;
  int *deck_end;
  int iid_or_eligible;
  card_instance_t *instance;
  int creature_or_eligible;
  int *deck_cursor;

  if (event == EVENT_QUERY)
  {
    if (attacking_card == 2)
    {

      deck_cursor = deck_ptr[player];
      deck_end = deck_cursor + 500;
      do
      {
        iid_or_eligible = *deck_cursor;
        if (iid_or_eligible == -1)
          break;
        if ((((((cards_data[iid_or_eligible].type & 2) != 0) &&
               (iid_or_eligible = has_subtype_by_id(
                    player, (unsigned int)cards_data[iid_or_eligible].id, SUBTYPE_CAT),
                iid_or_eligible == 0)) &&
              (iid_or_eligible = has_subtype_by_id(
                   player, (unsigned int)cards_data[*deck_cursor].id, SUBTYPE_ELEMENTAL),
               iid_or_eligible == 0)) &&
             ((iid_or_eligible = has_subtype_by_id(
                   player, (unsigned int)cards_data[*deck_cursor].id, SUBTYPE_NIGHTMARE),
               iid_or_eligible == 0 &&
                   (iid_or_eligible = has_subtype_by_id(
                        player, (unsigned int)cards_data[*deck_cursor].id, SUBTYPE_DINOSAUR),
                    iid_or_eligible == 0)))) &&
            (iid_or_eligible = has_subtype_by_id(player, (unsigned int)cards_data[*deck_cursor].id,
                                                 SUBTYPE_BEAST),
             iid_or_eligible == 0))
        {
          return 0;
        }
        deck_cursor = deck_cursor + 1;
      } while (deck_cursor != deck_end);
      i = 0;
      if (0 < active_cards_count[player])
      {
        do
        {
          creature_or_eligible = is_what(player, i, 2);
          if (((creature_or_eligible != 0) &&
               (creature_or_eligible = has_subtype(player, i, SUBTYPE_CAT),
                creature_or_eligible == 0)) &&
              ((creature_or_eligible = has_subtype(player, i, SUBTYPE_ELEMENTAL),
                creature_or_eligible == 0 &&
                    (((creature_or_eligible = has_subtype(player, i, SUBTYPE_NIGHTMARE),
                       creature_or_eligible == 0 &&
                           (creature_or_eligible = has_subtype(player, i, SUBTYPE_DINOSAUR),
                            creature_or_eligible == 0)) &&
                      (creature_or_eligible = has_subtype(player, i, SUBTYPE_BEAST),
                       creature_or_eligible == 0))))))
          {
            return 0;
          }
          i = i + 1;
        } while (i < active_cards_count[player]);
      }
      return 1;
    }
    check_legend_rule(player, card, EVENT_QUERY);
    vigilance(player, card, EVENT_QUERY);
  }
  else
  {
    check_legend_rule(player, card, event);
    vigilance(player, card, event);
    if (((((uint32_t)(event - EVENT_POWER) < 3) && (instance = in_play(player, card), instance != NULL)) &&
         (iid_or_eligible = is_humiliated(player, card, AL_STATIC), iid_or_eligible == 0)) &&
        ((((affected_card_controller == player && (affected_card != card)) &&
           (iid_or_eligible = is_what(player, affected_card, 2), iid_or_eligible != 0)) &&
          (((iid_or_eligible = has_subtype(affected_card_controller, affected_card, SUBTYPE_CAT),
             iid_or_eligible != 0 ||
                 (iid_or_eligible =
                      has_subtype(affected_card_controller, affected_card, SUBTYPE_ELEMENTAL),
                  iid_or_eligible != 0)) ||
            ((iid_or_eligible =
                  has_subtype(affected_card_controller, affected_card, SUBTYPE_NIGHTMARE),
              iid_or_eligible != 0 ||
                  ((iid_or_eligible =
                        has_subtype(affected_card_controller, affected_card, SUBTYPE_DINOSAUR),
                    iid_or_eligible != 0 ||
                        (iid_or_eligible =
                             has_subtype(affected_card_controller, affected_card, SUBTYPE_BEAST),
                         iid_or_eligible != 0))))))))))
    {
      if (event < EVENT_ABILITIES)
      {
        event_result = event_result + 1;
      }
      else
      {
        vigilance(affected_card_controller, affected_card, EVENT_ABILITIES);
      }
    }
  }
  return hybrid(player, card, event);
}

/* ManalinkEh 2022: 0x228a4f0; original cards/ikoria.c:1043. */
int card_keruga_the_macrosage(int player, int card, event_t event)
{
  /* Keruga, the Macrosage	|3|GU|GU
   * Legendary Creature - Dinosaur Hippo 5/4
   * Companion - Your starting deck contains only cards with mana value 3 or greater and land cards. (If this card is your chosen companion, you may put it into your hand from outside the game for |3 as a sorcery.)
   * When Keruga enters, draw a card for each other permanent you control with mana value 3 or greater. */

  int i;
  int *deck_end;
  int iid_or_cmc;
  int land_or_cmc;
  int *deck_cursor;
  test_definition_t test;

  if (event == EVENT_QUERY)
  {
    if (attacking_card == 2)
    {

      deck_cursor = deck_ptr[player];
      deck_end = deck_cursor + 500;
      do
      {
        iid_or_cmc = *deck_cursor;
        if (iid_or_cmc == -1)
          break;
        if (((cards_data[iid_or_cmc].type & 1) == 0) &&
            (iid_or_cmc = get_cmc_by_internal_id(iid_or_cmc), iid_or_cmc < 3))
        {
          return 0;
        }
        deck_cursor = deck_cursor + 1;
      } while (deck_end != deck_cursor);
      i = 0;
      if (0 < active_cards_count[player])
      {
        do
        {
          land_or_cmc = is_what(player, i, 1);
          if ((land_or_cmc == 0) && (land_or_cmc = get_cmc(player, i), land_or_cmc < 3))
          {
            return 0;
          }
          i = i + 1;
        } while (i < active_cards_count[player]);
      }
      return 1;
    }
    check_legend_rule(player, card, EVENT_QUERY);
    etb_ability(player, card, EVENT_QUERY, RESOLVE_TRIGGER_MANDATORY);
  }
  else
  {
    check_legend_rule(player, card, event);
    etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (event == EVENT_ETB)
    {
      default_test_definition(&test, 0x47);
      test.cmc = 2;
      test.cmc_flag = 2;
      draw_cards(player, check_battlefield_for_special_card(player, card, player, 4, &test));
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x228a680; original cards/ikoria.c:1090. */
int card_kinnan_bonder_prodigy(int player, int card, event_t event)
{
  /* Kinnan, Bonder Prodigy	|G|U
   * Legendary Creature - Human Druid 2/2
   * Whenever you tap a nonland permanent for mana, add one mana of any type that permanent produced.
   * |5|G|U: Look at the top five cards of your library. You may put a non-Human creature card from among them onto the battlefield. Put the rest on the bottom of your library in a random order. */

  int color_count_or_index;
  card_instance_t *instance;
  int color_bit;
  color_test_t col;
  test_definition_t test;

  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      if (is_humiliated(player, card, AL_TRIG) != 0)
      {
        return 0;
      }
      if (event == EVENT_COUNT_MANA)
      {
        if (is_what(affected_card_controller, affected_card, 1) != 0)
        {
          return 0;
        }
        instance = in_play(player, card);
        if (instance == NULL)
        {
          return 0;
        }
        if (is_tapped(affected_card_controller, affected_card) != 0)
        {
          return 0;
        }
        if (is_animated_and_sick(affected_card_controller, affected_card) != 0)
        {
          return 0;
        }
        if (can_produce_mana(affected_card_controller, affected_card) == 0)
        {
          return 0;
        }
        instance = get_card_instance(affected_card_controller, affected_card);
        color_bit = 0;
        col = (color_test_t)instance->card_color;
        color_count_or_index = 0;
        do
        {
          color_count_or_index =
              (color_count_or_index + 1) -
              (unsigned int)(((int)col >> ((unsigned char)color_bit & 0x1f) & 1U) == 0);
          color_bit = color_bit + 1;
        } while (color_bit != 7);
        if (0 < color_count_or_index)
        {
          declare_mana_available_hex(affected_card_controller, col, 1);
          return 0;
        }
        color_count_or_index = single_color_test_bit_to_color(col);
        declare_mana_available(affected_card_controller, color_count_or_index, 1);
        return 0;
      }
      if (EVENT_COUNT_MANA < event)
      {
        if (event != EVENT_TAP_CARD)
        {
          return 0;
        }

        if (is_what(affected_card_controller, affected_card, 1) != 0)
        {
          return 0;
        }
        instance = in_play(player, card);
        if (instance == NULL)
        {
          return 0;
        }
        produce_mana_of_any_type_tapped_for(player, card, 1);
        return 0;
      }
      if (event != EVENT_CAST_SPELL)
      {
        return (unsigned int)(event == EVENT_CAN_CAST);
      }
      if (card == affected_card)
      {
        if (player == affected_card_controller)
        {
          ai_modifier = ai_modifier + 0x30;
          return 0;
        }
        return 0;
      }
      return 0;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      new_default_test_definition(&test, 2, "Select a non-Human creature card.");
      test.subtype = 0x5b;
      test.subtype_flag = 1;
      reveal_top_cards_of_library_and_choose(player, card, player, 5, 0, TUTOR_PLAY, 0,
                                             TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test);
    }
  }
  return generic_activated_ability(player, card, event, 0, 5, 0, 1, 1, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x228a9b0; original cards/ikoria.c:1171. */
int card_kogla_the_titan_ape(int player, int card, event_t event)
{
  /* Kogla, the Titan Ape	|3|G|G|G
   * Legendary Creature - Ape 7/6
   * When Kogla enters, it fights up to one target creature you don't control.
   * Whenever Kogla attacks, destroy target artifact or enchantment defending player controls.
   * |1|G: Return target Human you control to its owner's hand. Kogla gains indestructible until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  check_legend_rule(player, card, event);
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - player;
    result = can_target(&td);
    if ((result != 0) &&
        (result = new_pick_target(&td, "Select target creature you don\'t control.", 0, 0x80),
         result != 0))
    {

      instance = get_card_instance(player, card);
      result = instance->targets[0].card;
      instance->number_of_targets = '\0';
      fight(player, card, instance->targets[0].player, result);
    }
    dat_ability(player, card, EVENT_ETB, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  }
  else
  {
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
    if (event == EVENT_DAT_ABILITY)
    {
      default_target_definition(player, card, &td, 0x44);
      td.allowed_controller = 1 - player;
      td.allow_cancel = 0;
      result = can_target(&td);
      if ((result != 0) && (result = pick_target(&td, "TARGET_ARTIFACT_ENCHANTMENT"), result != 0))
      {
        kill_target0(player, card, KILL_DESTROY);
      }
      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
      return 0;
    }
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.required_subtype = 0x5b;
  td.allowed_controller = player;
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {

    instance = get_card_instance(player, card);
    bounce_permanent(instance->targets[0].player, instance->targets[0].card);
    pump_ability_until_eot_no_repeat(player, card, instance->parent_controller,
                                     instance->parent_card, 0, 0x800);
  }
  return generic_activated_ability(player, card, event, 6, 1, 0, 0, 1, 0, 0, 0, &td,
                                   "Select target Human you control.");
}

/* ManalinkEh 2022: 0x228acb0; original cards/ikoria.c:1231. */
int card_lava_serpent(int player, int card, event_t event)
{
  /* Lava Serpent	|5|R
   * Creature - Elemental Serpent 5/5
   * Haste
   * Cycling |2 (|2, Discard this card: Draw a card.) */

  int result;
  haste(player, card, event);
  result = cycling(player, card, event, 2, 0, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x228ad20; original cards/ikoria.c:1262. */
int card_lukka_coppercoat_outcast(int player, int card, event_t event)
{
  /* Lukka, Coppercoat Outcast	|3|R|R
   * Legendary Planeswalker - Lukka (5)
   * +1: Exile the top three cards of your library. Creature cards exiled this way gain "You may cast this card from exile as long as you control a Lukka planeswalker."
   * -2: Exile target creature you control, then reveal cards from the top of your library until you reveal a creature card with greater mana value. Put that card onto the battlefield and the rest on the bottom of your library in a random order.
   * -7: Each creature you control deals damage equal to its power to each opponent. */

  enum
  {
    CHOICE_LIBRARY = 1,
    CHOICE_CREATURE = 2,
    CHOICE_ULTIMATE = 3
  };

  enum
  {
    MPCFE_LUKKA = 512
  };

  card_instance_t *instance;
  int result;
  int creature_index;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = can_target(&td);
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Exile top 3", 1,
                           10, 1, "Exile a creature", result, 10, 0xfffffffe, "Creature smash", 1,
                           0x14, 0xfffffff9, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_CREATURE)
      {
        new_pick_target(&td, "TARGET_CREATURE", 0, 1);
      }
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    result = instance->info_slot;
    if (result == 1)
    {
      default_test_definition(&test, 2);
      exile_top_x_card_you_may_play_them(instance->parent_controller, instance->parent_card, 3,
                                         &test, player, MPCFE_LUKKA);
    }
    else if (result == 2)
    {
      result = valid_target(&td);
      if (result != 0)
      {

        result = get_cmc(instance->targets[0].player, instance->targets[0].card);
        kill_target0(player, card, KILL_EXILE);
        default_test_definition(&test, 2);
        test.cmc_flag = 2;
        test.cmc = result;
        reveal_card_until_you_reveal_test(player, player, &test, 0x44);
      }
    }
    else
    {

      if ((result == 3) && (creature_index = (active_cards_count[player] + -1),
                            -1 < creature_index))
      {
        do
        {
          instance = in_play(player, creature_index);
          if ((instance != NULL) && (result = is_what(player, creature_index, 2), result != 0))
          {
            result = get_power(player, creature_index);
            damage_player(1 - player, result, player, creature_index);
          }
          creature_index = creature_index - 1;
        } while (creature_index != -1);
      }
    }
  }

  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x2288080; original cards/ikoria.c:1339. */
static int luminous_broodmoth_effect(int player, int card, event_t event)
{

  card_instance_t *instance;
  int pending_or_result;
  card_instance_t *dying;
  int grave_owner;
  int t_card;
  target_t *stored_death_cursor;
  test_definition_t test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (pending_or_result =
               get_id((int)instance->damage_target_player, instance->damage_target_card),
           pending_or_result != 0x5357))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (pending_or_result = effect_follows_control_of_attachment(player, card, 0x4700),
         pending_or_result != 0))
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (pending_or_result = effect_follows_control_of_attachment(player, card, event),
         pending_or_result != 0))
    {
      return 0;
    }
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {

      instance = get_card_instance(player, card);
      dying = in_play(affected_card_controller, affected_card);

      if ((dying != NULL) && (dying = get_card_instance(affected_card_controller, affected_card),
                              dying->kill_code != '\0'))
      {

        if ((-1 < instance->damage_target_player) &&
            ((instance->damage_target_card == affected_card &&
              (instance->damage_target_player == affected_card_controller))))
        {
          instance->damage_target_card = -1;
          instance->damage_target_player = -1;
          remove_status(player, card, 0x10000);
        }
        default_test_definition(&test, 2);
        test.keyword = 0x20;
        test.keyword_flag = 1;
        count_for_gfp_ability_and_store_values(player, card, 0x77, player, 0, &test, 0x90, 0);
      }
    }
  }
  pending_or_result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_or_result != 0)
  {
    if (check_status(player, card, 0x10000) == 0)
    {

      instance = get_card_instance(player, card);
      if (0 < pending_or_result)
      {
        stored_death_cursor = instance->targets + 1;
        do
        {

          grave_owner = stored_death_cursor->player;
          t_card = seek_grave_for_id_to_reanimate(
              grave_owner, -1, grave_owner, (unsigned int)cards_data[stored_death_cursor->card].id,
              REANIMATE_DEFAULT);
          if (t_card != -1)
          {
            add_counters(player, card, grave_owner, t_card, COUNTER_FLYING, 1);
          }
          stored_death_cursor = stored_death_cursor + 1;
        } while (stored_death_cursor != instance->targets + pending_or_result + 1);
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) &&
      (pending_or_result = check_status(player, card, 0x10000), pending_or_result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x228b0d0; original cards/ikoria.c:1398. */
int card_luminous_broodmoth(int player, int card, event_t event)
{
  /* Luminous Broodmoth	|2|W|W
   * Creature - Insect 3/4
   * Flying
   * Whenever a creature you control without flying dies, return it to the battlefield under its owner's control with a flying counter on it. */

  int i;

  int player_00;
  card_instance_t *instance;
  int scan_pending_or_created;
  int t_card;
  int effect_type;
  test_definition_t test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      bool legacy_found = false;
      if (0 < active_cards_count[player])
      {
        scan_pending_or_created = 0;
        do
        {
          instance = in_play(player, scan_pending_or_created);

          if ((((instance != NULL) &&
                (effect_type = is_what(player, scan_pending_or_created, 0x80), effect_type != 0)) &&
               (instance = get_card_instance(player, scan_pending_or_created),
                instance->info_slot_fn == luminous_broodmoth_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            legacy_found = true;
            break;
          }
          scan_pending_or_created = scan_pending_or_created + 1;
        } while (scan_pending_or_created < active_cards_count[player]);
      }

      if (!legacy_found)
      {
        scan_pending_or_created =
            create_targetted_legacy_effect(player, card, luminous_broodmoth_effect, player, card);
        instance = get_card_instance(player, scan_pending_or_created);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, scan_pending_or_created, 0x10000);
      }
    }
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    default_test_definition(&test, 2);
    test.keyword = 0x20;
    test.keyword_flag = 1;
    count_for_gfp_ability_and_store_values(player, card, 0x77, player, 0, &test, 0x90, 0);
  }
  instance = get_card_instance(player, card);

  if ((instance->kill_code == '\0') && (scan_pending_or_created = resolve_gfp_ability(
                                            player, card, event, RESOLVE_TRIGGER_MANDATORY),
                                        0 < scan_pending_or_created))
  {
    i = 0;
    do
    {

      instance = get_card_instance(player, card);
      player_00 = instance->targets[i].player;
      t_card = seek_grave_for_id_to_reanimate(
          player_00, -1, player_00, (unsigned int)cards_data[instance->targets[i].card].id,
          REANIMATE_DEFAULT);
      if (t_card != -1)
      {
        add_counters(player, card, player_00, t_card, COUNTER_FLYING, 1);
      }
      i = i + 1;
    } while (scan_pending_or_created != i);
    return 0;
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x228b340; original cards/ikoria.c:1464. */
int card_lurrus_of_the_dream_den(int player, int card, event_t event)
{
  /* Lurrus of the Dream-Den	|1|WB|WB
   * Legendary Creature - Cat Nightmare 3/2
   * Companion - Each permanent card in your starting deck has mana value 2 or less. (If this card is your chosen companion, you may put it into your hand from outside the game for |3 as a sorcery.)
   * Lifelink
   * Once during each of your turns, you may cast a permanent spell with mana value 2 or less from your graveyard. */

  if (event == EVENT_QUERY && attacking_card == 2)
  {
    const int *deck = deck_ptr[player];
    int lurrus_count = 0;
    for (int i = 0; i < 500 && deck[i] != -1; ++i)
    {
      if (is_what(-1, deck[i], 0x47) && get_cmc_by_internal_id(deck[i]) > 2)
      {
        int iid = deck[i];
        int lurrus_iid = get_internal_card_id_from_csv_id(CARD_ID_LURRUS_OF_THE_DREAM_DEN);
        if (lurrus_count != 0 || iid != lurrus_iid)
          return 0;
        lurrus_count = 1;
      }
    }
    for (int i = 0; i < active_cards_count[player]; ++i)
    {
      if (!is_what(player, i, 0x47) || get_cmc(player, i) < 3)
        return 0;
      int id = get_id(player, i);
      if (lurrus_count != 0 || id != 0x5359)
        return 0;
      lurrus_count = 1;
    }
    return 1;
  }
  check_legend_rule(player, card, event);
  lifelink(player, card, event);
  if (event == EVENT_QUERY)
    return 0;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (is_humiliated(player, card, AL_STATIC) || current_turn != player ||
        instance->info_slot == 0x42)
      return 0;
    const int *grave = get_grave(player);
    int result = 0;
    for (int i = 0; i < count_graveyard(player); ++i)
    {
      if (is_what(-1, grave[i], 0x47) && !is_what(-1, grave[i], 1) &&
          get_cmc_by_internal_id(grave[i]) <= 2 &&
          has_mana_to_cast_iid(player, EVENT_CAN_ACTIVATE, grave[i]) &&
          (is_what(-1, grave[i], 0x30) || can_sorcery_be_played(player, EVENT_CAN_ACTIVATE)))
      {
        int legal = can_legally_play_iid(player, grave[i]);
        if (legal == 99)
          return 99;
        if (legal != 0)
          result = legal;
      }
    }
    return result;
  }
  if (event == EVENT_ACTIVATE)
  {
    unsigned capacity = (unsigned)count_graveyard(player);
    int playable[2][capacity];
    const int *grave = get_grave(player);
    int count = 0;
    for (int i = 0; i < count_graveyard(player); ++i)
    {
      if (is_what(-1, grave[i], 0x47) && !is_what(-1, grave[i], 1) &&
          get_cmc_by_internal_id(grave[i]) < 3 &&
          has_mana_to_cast_iid(player, EVENT_ACTIVATE, grave[i]) &&
          (is_what(-1, grave[i], 0x30) || can_sorcery_be_played(player, EVENT_ACTIVATE)) &&
          can_legally_play_iid(player, grave[i]))
      {
        playable[0][count] = grave[i];
        playable[1][count++] = i;
      }
    }
    test_definition_t test;
    _Static_assert(sizeof(test) == (sizeof(void *) == 8 ? 232 : 216), "native Lurrus filter");
    new_default_test_definition(&test, 0xff, "Select a permanent card to play.");
    int selected = select_card_from_zone(player, player, playable[0], count, 0, 1, -1, &test);
    if (selected != -1 && charge_mana_from_id(player, -1, EVENT_ACTIVATE,
                                              (uint16_t)cards_data[playable[0][selected]].id))
    {
      instance->info_slot = 0x42;
      play_card_in_grave_for_free(player, player, playable[1][selected]);
      cant_be_responded_to = 1;
      return 0;
    }
    spell_fizzled = 1;
  }
  else if (event == EVENT_CLEANUP)
    instance->info_slot = 0;
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x228b8f0; original cards/ikoria.c:1560. */
int card_lutri_the_spellchaser(int player, int card, event_t event)
{
  /* Lutri, the Spellchaser	|1|UR|UR
   * Legendary Creature - Elemental Otter 3/2
   * Companion - Each nonland card in your starting deck has a different name. (If this card is your chosen companion, you may put it into your hand from outside the game for |3 as a sorcery.)
   * Flash
   * When Lutri enters, if you cast it, copy target instant or sorcery spell you control. You may choose new targets for the copy. */

  int available_or_result;
  card_instance_t *instance;
  int cast_priority;
  target_definition_t td;

  if ((event == EVENT_QUERY) && (attacking_card == 2))
  {
    int options[available_slots];
    memset(options, 0, available_slots);
    int *deck = deck_ptr[player];
    for (int i = 0; i != 500; ++i)
    {
      int iid = deck[i];
      if (iid == -1)
        return 1;
      if ((cards_data[iid].type & TYPE_LAND) == 0)
      {
        unsigned int csvid = (uint16_t)cards_data[iid].id;
        if (options[csvid] != 0)
          return 0;
        options[csvid] = 1;
      }
    }
    return 1;
  }
  check_legend_rule(player, card, event);
  if ((((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((instance = get_card_instance(player, card),
        instance->info_slot == 1 &&
            (available_or_result = check_special_flags(player, card, 0x40000),
             available_or_result == 0))))
  {
    etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  }
  if (event == EVENT_ETB)
  {
    counterspell_target_definition(player, card, &td, 0);
    if (counterspell_validate(player, card, &td, 0) != 0)
    {
      instance = get_card_instance(player, card);
      copy_spell_from_stack(player, instance->targets[0].player, instance->targets[0].card);
    }
    available_or_result = hybrid(player, card, event);
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      instance = get_card_instance(player, card);
      counterspell_target_definition(player, card, &td, 0);
      td.allowed_controller = player;
      available_or_result = counterspell(player, card, EVENT_CAN_CAST, &td, 0);
      if (available_or_result == 0)
      {
        cast_priority = 1;
      }
      else
      {
        cast_priority = 99;
      }
      instance->info_slot = (unsigned int)(available_or_result != 0);
      return cast_priority;
    }
    if (event == EVENT_CAST_SPELL)
    {
      if ((card != affected_card) || (player != affected_card_controller))
        return hybrid(player, card, event);
      instance = get_card_instance(player, card);
      if ((card == affected_card) &&
          ((player == affected_card_controller &&
            (hybrid(player, card, EVENT_CAST_SPELL), spell_fizzled != 1))))
      {
        counterspell_target_definition(player, card, &td, 0);
        td.allowed_controller = player;
        if (instance->info_slot == 1)
        {
          counterspell(player, card, EVENT_CAST_SPELL, &td, 0);
        }
        else
        {
          ai_modifier = ai_modifier + -0x1e;
        }
      }
    }
    else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      get_card_instance(player, card);
    }
    else
    {
      if (event != EVENT_CHANGE_TARGET)
        return hybrid(player, card, event);
      get_card_instance(player, card);
    }
    available_or_result = 0;
  }
  return available_or_result;
}
#endif

/* ManalinkEh 2022: 0x228bc70; original cards/ikoria.c:1763. */
int card_obosh_the_preypiercer(int player, int card, event_t event)
{
  /* Obosh, the Preypiercer	|3|BR|BR
   * Legendary Creature - Hellion Horror 3/5
   * Companion - Your starting deck contains only cards with odd mana values and land cards. (If this card is your chosen companion, you may put it into your hand from outside the game for |3 as a sorcery.)
   * If a source you control with an odd mana value would deal damage to a permanent or player, it deals double that damage to that permanent or player instead. */

  int i;
  int *deck_end;
  card_instance_t *instance;
  int deck_iid;
  unsigned int card_cmc;
  int land;
  int *deck_cursor;

  if ((event != EVENT_QUERY) || (attacking_card != 2))
  {
    check_legend_rule(player, card, event);
    instance = damage_being_dealt(event);
    if ((instance != NULL) &&
        ((instance->damage_source_player == player &&
          (card_cmc = get_cmc(player, instance->damage_source_card), (card_cmc & 1) != 0))))
    {
      instance->info_slot = instance->info_slot * 2;
    }
    return hybrid(player, card, event);
  }

  deck_cursor = deck_ptr[player];
  deck_end = deck_cursor + 500;
  do
  {
    deck_iid = *deck_cursor;
    if (deck_iid == -1)
      break;
    if ((cards_data[deck_iid].type == '\0') &&
        (card_cmc = get_cmc_by_internal_id(deck_iid), (card_cmc & 1) == 0))
    {
      return 0;
    }
    deck_cursor = deck_cursor + 1;
  } while (deck_end != deck_cursor);
  i = 0;
  if (0 < active_cards_count[player])
  {
    do
    {
      land = is_what(player, i, 1);
      if ((land == 0) && (card_cmc = get_cmc(player, i), (card_cmc & 1) == 0))
      {
        return 0;
      }
      i = i + 1;
    } while (i < active_cards_count[player]);
  }
  return 1;
}

/* ManalinkEh 2022: 0x2288060; original cards/ikoria.c:1927. */
static int iid_has_cycling(int iid, intptr_t me, int player, int card)
{
  return (unsigned int)(cards_data[iid].cc[2] == '\x04');
}

/* ManalinkEh 2022: 0x228bda0; original cards/ikoria.c:1933. */
int card_rooting_moloch(int player, int card, event_t event)
{
  /* Rooting Moloch	|4|R
   * Creature - Lizard 4/4
   * When this creature enters, exile target card with a cycling ability from your graveyard. Until the end of your next turn, you may play that card.
   * Cycling |2 (|2, Discard this card: Draw a card.) */

  int exiled_iid;
  int selected_grave_index;
  test_definition_t test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  new_default_test_definition(&test, 0xff, "Select a card with Cycling.");
  test.special_selection_function = iid_has_cycling;
  if (event == EVENT_ETB)
  {
    if (new_special_count_grave(player, &test) != 0)
    {

      selected_grave_index = new_select_a_card(player, player, 2, 1, 1, 0, &test);
      if (selected_grave_index != -1)
      {

        exiled_iid = graveyard_ptr_mutable[player][selected_grave_index];
        rfg_card_from_grave(player, selected_grave_index);
        create_may_play_card_from_exile_effect(player, card, player,
                                               (unsigned int)cards_data[exiled_iid].id,
                                               MPCFE_UNTIL_END_OF_YOUR_NEXT_TURN);
      }
    }
  }
  return cycling(player, card, event, 2, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x228bee0; original cards/ikoria.c:1955. */
int card_ruinous_ultimatum(int player, int card, event_t event)
{
  /* Ruinous Ultimatum	|R|R|W|W|W|B|B
   * Sorcery
   * Destroy all nonland permanents your opponents control. */

  int result;
  test_definition_t test;
  if (event != EVENT_CAN_CAST)
  {
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
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      default_test_definition(&test, 1);
      test.type_flag = 1;
      new_manipulate_all(player, card, 1 - player, &test, ACT_KILL_DESTROY);
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

/* ManalinkEh 2022: 0x228bfc0; original cards/ikoria.c:2029. */
int card_shark_typhoon(int player, int card, event_t event)
{
  /* Shark Typhoon	|5|U
   * Enchantment
   * Whenever you cast a noncreature spell, create an X/X blue Shark creature token with flying, where X is that spell's mana value.
   * Cycling |X|1|U (|X|1|U, Discard this card: Draw a card.)
   * When you cycle this card, create an X/X blue Shark creature token with flying. */

  int32_t token_size;
  int cycling_result_or_cmc;
  card_instance_t *instance;

  if (event == EVENT_CAN_ACTIVATE_FROM_HAND)
  {
    return cycling(player, card, EVENT_CAN_ACTIVATE_FROM_HAND, 1, 0, 1, 0, 0, 0);
  }
  if (event == EVENT_ACTIVATE_FROM_HAND)
  {

    cycling_result_or_cmc = cycling(player, card, EVENT_ACTIVATE_FROM_HAND, 1, 0, 1, 0, 0, 0);
    charge_mana(player, COLOR_COLORLESS, -1);
    if (spell_fizzled == 1)
    {
      return cycling_result_or_cmc;
    }
    instance = get_card_instance(player, card);
    instance->info_slot = x_value;
    return cycling_result_or_cmc;
  }
  if (event == EVENT_RESOLVE_ACTIVATION_FROM_HAND)
  {
    if ((spell_fizzled == 0) && ((((unsigned int)player_bits[player] >> 24 & 0xff) & 2) == 0))
    {

      instance = get_card_instance(player, card);
      token_size = instance->info_slot;
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_SHARK, &token);
      token.key_plus = 0x20;
      token.pow = token_size;
      token.tou = token_size;
      generate_token(&token);
      return cycling(player, card, EVENT_RESOLVE_ACTIVATION_FROM_HAND, 1, 0, 1, 0, 0, 0);
    }
  }
  if (trigger_condition == TRIGGER_SPELL_CAST && card == affected_card && player == affected_card_controller)
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.type_flag = DOESNT_MATCH;
    sp_ability_trigger(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_SPA)
  {

    token_generation_t token;
    cycling_result_or_cmc = get_cmc(trigger_cause_controller, trigger_cause);
    default_token_definition(player, card, CARD_ID_SHARK, &token);
    token.key_plus = 0x20;
    token.pow = cycling_result_or_cmc;
    token.tou = cycling_result_or_cmc;
    generate_token(&token);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x228c2c0; original cards/ikoria.c:2169. */
int card_sprite_dragon(int player, int card, event_t event)
{
  /* Sprite Dragon	|U|R
   * Creature - Faerie Dragon 1/1
   * Flying, haste
   * Whenever you cast a noncreature spell, put a +1/+1 counter on this creature. */

  int result;

  haste(player, card, event);
  result = card_deeproot_champion(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22884c0; original cards/ikoria.c:2222. */
static int has_counter(int unused1, intptr_t unused2, int player, int card)
{
  return count_counters(player, card, -1);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x228c300; original cards/ikoria.c:2225. */
int card_the_ozolith(int player, int card, event_t event)
{
  /* The Ozolith	|1
   * Legendary Artifact
   * Whenever a creature you control leaves the battlefield, if it had counters on it, put those counters on ~.
   * At the beginning of combat on your turn, if ~ has counters on it, you may move all counters from ~ onto target creature. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  default_test_definition(&test, 2);
  test.special_selection_function = has_counter;
  test_leaves_play(player, card, event, player, &test, 1, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    move_counters(player, card, player, card, trigger_cause_controller, trigger_cause,
                              ~COUNTER_P1_P1, -1);
    return 0;
  }
  if (event == EVENT_CHANGE_TYPE)
  {
    result = engine_any_legal_attacker(player);
    if (result == 0)
    {
      event_flags = event_flags | 0x1000000;
      return 0;
    }
  }
  else if (event == EVENT_BEGIN_COMBAT)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if ((result == 0) && (current_turn == player))
    {

      default_target_definition(player, card, &td, 2);
      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
      new_pick_target(&td, "Select a creature for The Ozolith\'s counters.", 0, 0x81);
      if (spell_fizzled != 1)
      {
        move_counters(player, card, instance->targets[0].player,
                                  instance->targets[0].card, player, card, ~COUNTER_P1_P1, -1);
      }
      instance->number_of_targets = '\0';
    }
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x228c4f0; original cards/ikoria.c:2289. */
int card_umori_the_collector(int player, int card, event_t event)
{
  /* Umori, the Collector	|2|BG|BG
   * Legendary Creature - Ooze 4/5
   * Companion - Each nonland card in your starting deck shares a card type. (If this card is your chosen companion, you may put it into your hand from outside the game for |3 as a sorcery.)
   * As Umori enters, choose a card type.
   * Spells you cast of the chosen type cost |1 less to cast. */

  int i;
  int choice;
  bool companion_skipped;
  int iid_type_or_result;
  card_instance_t *instance;
  int id_or_result;
  type_t card_types;
  int *deck_cursor;
  int *deck_end;
  type_t shared_types;
  int types[7];
  if (event == EVENT_QUERY)
  {
    if (attacking_card == 2)
    {
      deck_cursor = deck_ptr[player];
      shared_types =
          TARGET_TYPE_PLANESWALKER | TYPE_ARTIFACT | TYPE_SPELL | TYPE_ENCHANTMENT | TYPE_CREATURE;
      companion_skipped = false;
      deck_end = deck_cursor + 500;
      do
      {
        if (*deck_cursor == -1)
          break;
        if (is_what(-1, *deck_cursor, 1) == 0)
        {
          iid_type_or_result = *deck_cursor;
          id_or_result = get_internal_card_id_from_csv_id(CARD_ID_UMORI_THE_COLLECTOR);
          if ((companion_skipped) || (iid_type_or_result != id_or_result))
          {
            card_types = get_type_with_iid(-1, -1, *deck_cursor);
            shared_types = shared_types & card_types;
            if (shared_types == TYPE_NONE)
            {
              return 0;
            }
          }
          else
          {
            companion_skipped = true;
          }
        }
        deck_cursor = deck_cursor + 1;
      } while (deck_cursor != deck_end);
      i = 0;
      if (0 < active_cards_count[player])
      {
        do
        {
          while (id_or_result = is_what(player, i, 1), id_or_result == 0)
          {
            id_or_result = get_id(player, i);
            if ((!companion_skipped) && (id_or_result == 0x53b2))
            {
              companion_skipped = true;
              break;
            }
            card_types = get_type(player, card);
            shared_types = shared_types & card_types;
            if (shared_types == TYPE_NONE)
            {
              return 0;
            }
            i = i + 1;
            if (active_cards_count[player] <= i)
            {
              return 1;
            }
          }
          i = i + 1;
        } while (i < active_cards_count[player]);
      }
      return 1;
    }
    check_legend_rule(player, card, EVENT_QUERY);
    return hybrid(player, card, event);
  }
  check_legend_rule(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {
    memcpy(types, (const int[]){2, 4, 8, 48, 64, 256, 9}, sizeof types);
    choice =
        do_dialog(player, player, card, -1, -1,
                  " Creature\n Enchantment\n Sorcery\n Instant\n Artifact\n Planeswalker\n Tribal",
                  internal_rand(5));
    instance = get_card_instance(player, card);
    instance->targets[1].card = types[choice];
    return hybrid(player, card, event);
  }
  if (((event != EVENT_MODIFY_COST_GLOBAL) || (affected_card_controller != player)) ||
      (iid_type_or_result = is_humiliated(player, card, AL_STATIC), iid_type_or_result != 0))
    return hybrid(player, card, event);
  instance = get_card_instance(player, card);
  iid_type_or_result = instance->targets[1].card;
  if (iid_type_or_result < 0)
    return hybrid(player, card, event);
  if (iid_type_or_result == 9)
    iid_type_or_result = is_tribal(affected_card_controller, affected_card);
  else
    iid_type_or_result = is_what(affected_card_controller, affected_card, iid_type_or_result);
  if (iid_type_or_result != 0)
  {
    COST_COLORLESS = COST_COLORLESS + -1;
  }
  return hybrid(player, card, event);
}
#endif

/* ManalinkEh 2022: 0x22884e0; original cards/ikoria.c:2380. */
static int vivien_library_ability(int player, int card, event_t event)
{
  enum
  {
    CHOICE_REVEAL = 1,
    CHOICE_PLAY = 2
  };

  int top_iid_or_result;
  int artifact_or_available;
  unsigned int play_available;
  uint32_t view_available;

  if (event == EVENT_CAN_ACTIVATE)
  {

    top_iid_or_result = *deck_ptr[player];
    if (top_iid_or_result != -1)
    {
      artifact_or_available = is_what(-1, top_iid_or_result, 0x40);
      if (((artifact_or_available != 0) && (artifact_or_available = has_mana_to_cast_iid(
                                                player, EVENT_CAN_ACTIVATE, top_iid_or_result),
                                            artifact_or_available != 0)) &&
          (top_iid_or_result =
               can_legally_play_iid_now(player, top_iid_or_result, EVENT_CAN_ACTIVATE),
           top_iid_or_result != 0))
      {
        return top_iid_or_result;
      }
      top_iid_or_result = *deck_ptr[player];
    }
    return (unsigned int)(top_iid_or_result != -1);
  }
  if (event != EVENT_ACTIVATE)
  {
    return 0;
  }

  top_iid_or_result = *deck_ptr[player];
  bool ai = ai_is_speculating == 1 || (player == 1 && (((unsigned char)trace_mode & 2) == 0));
  view_available = !ai && top_iid_or_result != -1;
  play_available = 0;
  if (top_iid_or_result != -1)
  {
    top_iid_or_result = is_what(-1, top_iid_or_result, 2);
    play_available = 0;
    if (top_iid_or_result != 0)
    {
      top_iid_or_result = can_legally_play_iid_now(player, *deck_ptr[player], EVENT_ACTIVATE);
      play_available = 0;
      if (top_iid_or_result != 0)
      {
        top_iid_or_result = has_mana_to_cast_iid(player, EVENT_ACTIVATE, *deck_ptr[player]);
        play_available = (unsigned int)(top_iid_or_result != 0);
      }
    }
    top_iid_or_result = *deck_ptr[player];
  }
  top_iid_or_result =
      dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_STORAGE, DLG_SMALLCARD_ID(
                  top_iid_or_result), "Check top card of deck", view_available, 0, "Play top card",
                  play_available, 10, 0, 0, 0, 0);
  if (top_iid_or_result == 0)
  {
    spell_fizzled = 1;
    return 0;
  }
  if (top_iid_or_result != CHOICE_REVEAL)
  {
    if ((top_iid_or_result == CHOICE_PLAY) &&
        (top_iid_or_result = charge_mana_from_id(player, -1, EVENT_ACTIVATE,
                                                 (unsigned int)cards_data[*deck_ptr[player]].id),
         top_iid_or_result != 0))
    {
      play_card_in_deck_for_free(player, player, 0);
      cant_be_responded_to = 1;
      spell_fizzled = 1;
    }
    return 0;
  }
  show_deck(player, deck_ptr[player], 1, "Here\'s the top card of your deck", 0, gs_ok_00924800);
  spell_fizzled = 1;
  return 0;
}

/* ManalinkEh 2022: 0x22883c0; original cards/ikoria.c:2425. */
static int vivien_doubling_legacy(int player, int card, event_t event)
{
  int result;
  test_definition_t test;

  sp_ability_trigger(player, event, 2, TYPE_CREATURE, NULL, player, card);
  if (event == EVENT_SPA)
  {

    result = get_cmc(trigger_cause_controller, trigger_cause);
    default_test_definition(&test, 2);
    test.cmc_flag = 3;
    test.cmc = result;
    sprintf(test.message, "Select a creature card with mana value %d or less.", result + -1);
    new_global_tutor(player, player, 1, 4, 1, 2, &test);
  }
  else if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x228c7f0; original cards/ikoria.c:2445. */
int card_vivien_monsters_advocate(int player, int card, event_t event)
{
  /* Vivien, Monsters' Advocate	|3|G|G
   * Legendary Planeswalker - Vivien (3)
   * You may look at the top card of your library any time.
   * You may cast creature spells from the top of your library.
   * +1: Create a 3/3 green Beast creature token. Put your choice of a reach counter, a vigilance counter, or a trample counter on it.
   * -2: When you next cast a creature spell this turn, search your library for a creature card with lesser mana value, put it onto the battlefield, then shuffle. */

  enum
  {
    CHOICE_BEAST = 1,
    CHOICE_DOUBLE = 2
  };

  int choice;
  card_instance_t *instance;
  int c;
  int creature_or_cmc;
  int maximum_payable_cmc;
  int counters[4];
  token_generation_t token;

  check_legend_rule(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {
    create_targetted_legacy_activate(player, card, vivien_library_ability, player, card);
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_ACTIVATE)
      {
        instance->number_of_targets = '\0';
        instance->info_slot = 0;
        maximum_payable_cmc = 0;
        if ((player == 1) && (0 < active_cards_count[1]))
        {
          c = 0;
          do
          {
            instance = in_hand(1, c);
            if ((instance != NULL) && (creature_or_cmc = is_what(1, c, 2), creature_or_cmc != 0))
            {
              creature_or_cmc =
                  has_mana_to_cast_iid(1, EVENT_ACTIVATE, get_original_internal_card_id(1, c));
              if ((creature_or_cmc != 0) &&
                  (creature_or_cmc = get_cmc(1, c), maximum_payable_cmc < creature_or_cmc))
              {
                maximum_payable_cmc = creature_or_cmc;
              }
            }
            c = c + 1;
          } while (c < active_cards_count[1]);
        }
        if (dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Create a beast", 1,
                        3, 1, "Double your next creature", 1, maximum_payable_cmc, 0xfffffffe, 0, 0,
                        0, 0) == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        return planeswalker(player, card, event, 3);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return planeswalker(player, card, event, 3);
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (instance->info_slot == CHOICE_BEAST)
      {
        choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, "Vigilance", 1,
                             1, "Reach", 1, 1, "Trample", 1, 1, 0, 0, 0, 0);
        counters[0] = 0;
        counters[1] = 0x9a;
        counters[2] = 0x98;
        counters[3] = 0x99;
        default_token_definition(player, card, CARD_ID_BEAST, &token);
        token.action_argument = 1;
        token.action = TOKEN_ACTION_ADD_COUNTERS;
        token.action_argument = (((unsigned int)(unsigned short)counters[choice] << 16) | 1U);
        generate_token(&token);
      }
      else if (instance->info_slot == CHOICE_DOUBLE)
      {
        create_legacy_effect(player, card, vivien_doubling_legacy);
      }
    }
  }
  return planeswalker(player, card, event, 3);
}
#endif

/* ManalinkEh 2022: 0x228cb30; original cards/ikoria.c:2542. */
int card_whirlwind_of_thought(int player, int card, event_t event)
{
  /* Whirlwind of Thought	|1|U|R|W
   * Enchantment
   * Whenever you cast a noncreature spell, draw a card. */

  int result;

  prowess_mode(player, card, event, 2);
  if (event == EVENT_SPA)
  {
    draw_a_card(player);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x228cb90; original cards/ikoria.c:2582. */
int card_winota_joiner_of_forces(int player, int card, event_t event)
{
  /* Winota, Joiner of Forces	|2|R|W
   * Legendary Creature - Human Warrior 4/4
   * Whenever a non-Human creature you control attacks, look at the top six cards of your library. You may put a Human creature card from among them onto the battlefield tapped and attacking. It gains indestructible until end of turn. Put the rest of the cards on the bottom of your library in a random order. */

  int t_card;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if (event != EVENT_DECLARE_ATTACKERS)
  {
    dat_ability(player, card, event, DAT_SEPARATE_TRIGGERS | DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {

      default_test_definition(&test, 2);
      test.subtype = 0x5b;
      test.subtype_flag = 0;
      t_card =
          reveal_top_cards_of_library_and_choose(player, card, player, 6, 0, TUTOR_PLAY_ATTACKING,
                                                 0, TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test);
      if (-1 < t_card)
      {
        pump_ability_until_eot_no_repeat(player, card, player, t_card, 0, 0x800);
      }
    }
    return 0;
  }
  default_test_definition(&test, 2);
  test.subtype = 0x5b;
  test.subtype_flag = 1;
  store_attackers(player, card, EVENT_DECLARE_ATTACKERS,
                  DAT_SEPARATE_TRIGGERS | DAT_RESOLVE_TRIGGER_MANDATORY, player, -1, &test);
  dat_ability(player, card, EVENT_DECLARE_ATTACKERS,
              DAT_SEPARATE_TRIGGERS | DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  return 0;
}

/* ManalinkEh 2022: 0x22887d0; original cards/ikoria.c:2619. */
static const char *permanent_you_control_and_own(int who_chooses, int player, int card,
                                                 int targeting_player, int targeting_card)
{
  int result;
  result = get_owner(player, card);
  if (result != targeting_player)
  {
    return "owner";
  }
  return NULL;
}

/* ManalinkEh 2022: 0x228cd10; original cards/ikoria.c:2625. */
int card_yorion_sky_nomad(int player, int card, event_t event)
{
  /* Yorion, Sky Nomad	|3|WU|WU
   * Legendary Creature - Bird Serpent 4/5
   * Companion - Your starting deck contains at least twenty cards more than the minimum deck size. (If this card is your chosen companion, you may put it into your hand from outside the game for |3 as a sorcery.)
   * Flying
   * When Yorion enters, exile any number of other nonland permanents you own and control. Return those cards to the battlefield at the beginning of the next end step. */

  if (event == EVENT_QUERY && attacking_card == 2)
  {
    int min_deck_size = singleplayer_mode == 2 ? 0x3b :
                        (other_game_mode == OGM_EDH ? 117 : 79);
    int deck_size = count_deck(player);
    return min_deck_size < deck_size;
  }
  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x47);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.special = TARGET_SPECIAL_NOT_ME | TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (intptr_t)permanent_you_control_and_own;
    td.illegal_abilities = 0;
    td.allow_cancel = 3;
    char flicker_array[2][151];
    memset(flicker_array, 0, sizeof(flicker_array));
    int num = mark_up_to_n_targets_noload(
        &td, "Select a nonland permanent you own and control.", -1, flicker_array);
    if (num > 0)
      for (int c = 0; c < active_cards_count[player]; ++c)
        if (flicker_array[player][c])
          alternate_legacy_text(2, player, remove_until_eot(player, card, player, c));
  }
  return hybrid(player, card, event);
}

/* ManalinkEh 2022: 0x228cf40; original cards/ikoria.c:2700. */
int card_zirda_the_dawnwaker(int player, int card, event_t event)
{
  /* Zirda, the Dawnwaker	|1|RW|RW
   * Legendary Creature - Elemental Fox 3/3
   * Companion - Each permanent card in your starting deck has an activated ability. (If this card is your chosen companion, you may put it into your hand from outside the game for |3 as a sorcery.)
   * Abilities you activate that aren't mana abilities cost |2 less to activate. This effect can't reduce the mana in that cost to less than one mana.
   * |1, |T: Target creature can't block this turn. */

  int i;
  target_definition_t td;
  int *deck_end;
  int iid_or_available;
  card_instance_t *instance;
  int iid_or_ability;
  int *deck_cursor;

  if (event == EVENT_QUERY)
  {
    if (attacking_card == 2)
    {

      deck_cursor = deck_ptr[player];
      deck_end = deck_cursor + 500;
      do
      {
        iid_or_available = *deck_cursor;
        if (iid_or_available == -1)
          break;
        if (((cards_data[iid_or_available].type & 0x47) != 0) &&
            (iid_or_available = has_an_activated_ability(iid_or_available), iid_or_available == 0))
        {
          return 0;
        }
        deck_cursor = deck_cursor + 1;
      } while (deck_cursor != deck_end);
      i = 0;
      if (0 < active_cards_count[player])
      {
        do
        {

          iid_or_ability = get_original_internal_card_id(player, i);
          if (((cards_data[iid_or_ability].type & 0x47) != 0) &&
              (iid_or_ability = has_an_activated_ability(iid_or_ability), iid_or_ability == 0))
          {
            return 0;
          }
          i = i + 1;
        } while (i < active_cards_count[player]);
      }
      return 1;
    }
  }
  check_legend_rule(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {
    test_definition_t this_test2;
    default_test_definition(&this_test2, 0x47);
    set_cost_mod_for_activated_abilities(player, -1, 2, &this_test2);
  }
  else if (event == EVENT_CAST_SPELL && is_what(affected_card_controller, affected_card, 0x47))
    set_cost_mod_for_activated_abilities(affected_card_controller, affected_card, 2, NULL);
  leaves_play(player, card, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 0x47);
    remove_cost_mod_for_activated_abilities(player, -1, 2, &this_test);
    return hybrid(player, card, event);
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return hybrid(player, card, event);
  default_target_definition(player, card, &td, 2);
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (iid_or_available = valid_target(&td), iid_or_available != 0))
  {

    instance = get_card_instance(player, card);
    alternate_legacy_text(2, player,
                          pump_ability_until_eot_no_repeat(player, card,
                                                           instance->targets[0].player,
                                                           instance->targets[0].card, 0, 0x20));
  }
  return generic_activated_ability(player, card, event, 5, 1, 0, 0, 0, 0, 0, 0, &td,
                                   "TARGET_CREATURE");
}
