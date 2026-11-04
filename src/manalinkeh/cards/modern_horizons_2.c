#include "manalink.h"

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x222b4b0; original cards/modern_horizons_2.c:4. */
static int evoke_with_cards_in_hand(int player, int card, event_t event, int color_test)
{
  int choice;
  int32_t iid;
  card_instance_t *instance;
  int index_or_csvid;
  card_instance_t *hand_card;
  unsigned int hand_card_color;
  int card_00;
  test_definition_t this_test;
  int fake_hand[100][2];

  if (event == EVENT_MODIFY_COST)
  {

    instance = get_card_instance(player, card);
    if (0 < active_cards_count[player])
    {
      index_or_csvid = 0;
      do
      {
        hand_card = in_hand(player, index_or_csvid);
        if (((card != index_or_csvid) && (hand_card != NULL)) &&
            (hand_card_color = get_color(player, index_or_csvid),
             (color_test & hand_card_color) != 0))
        {
          null_casting_cost(player, card);
          instance->eot_toughness0 = '\x01';
          return 0;
        }
        index_or_csvid = index_or_csvid + 1;
      } while (index_or_csvid < active_cards_count[player]);
    }
    instance->eot_toughness0 = '\0';
    return 0;
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

  instance = get_card_instance(player, card);
  if (played_for_free(player, card) != 0)
  {
    return 0;
  }
  if (is_token(player, card) != 0)
  {
    return 0;
  }
  if (instance->eot_toughness0 == '\x01')
  {

    index_or_csvid = get_id(player, card);
    if (has_mana_to_cast_id(player, EVENT_CAST_SPELL, index_or_csvid) != 0)
    {
      choice = do_dialog(player, player, card, -1, -1, " Play normally\n Evoke\n Cancel", 0);
      if (choice == 0)
      {
        charge_mana_from_id(player, card, EVENT_CAST_SPELL, get_id(player, card));
        return 0;
      }
      if (choice != 1)
      {
        spell_fizzled = 1;
        return 0;
      }
    }

    card_00 = 0;
    index_or_csvid = 0;
    if (0 < active_cards_count[player])
    {
      do
      {
        hand_card = in_hand(player, card_00);
        if (((card != card_00) && (hand_card != NULL)) &&
            (hand_card_color = get_color(player, card_00), (color_test & hand_card_color) != 0))
        {
          hand_card = get_card_instance(player, card_00);
          iid = hand_card->internal_card_id;
          fake_hand[1][index_or_csvid] = card_00;
          fake_hand[0][index_or_csvid] = iid;
          index_or_csvid = index_or_csvid + 1;
        }
        card_00 = card_00 + 1;
      } while (card_00 < active_cards_count[player]);
    }
    new_default_test_definition(&this_test, 0xff, "Select a card to exile.");
    index_or_csvid =
        select_card_from_zone(player, player, fake_hand[0], index_or_csvid, 0, 3, -1, &this_test);
    if (index_or_csvid < 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    rfg_card_in_hand(player, fake_hand[1][index_or_csvid]);
    instance->eot_toughness1 = '\x01';
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x222bce0; original cards/modern_horizons_2.c:67. */
int one_or_more_p1_p1_counters_are_putted_on_me(int player, int card, event_t event)
{
  xtrigger_t trigger;

  enable_xtrigger_flags = enable_xtrigger_flags | ENABLE_XTRIGGER_1_1_COUNTERS;
  trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    trigger = xtrigger_impl_value_dont_use_directly;
  }
  if (trigger != XTRIGGER_1_1_COUNTERS)
  {
    return 0;
  }
  if ((((card == affected_card) && (player == affected_card_controller)) &&
       (reason_for_trigger_controller == player)) &&
      ((trigger_cause_controller == player && (trigger_cause == card))))
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      if (event == EVENT_TRIGGER)
      {
        event_result = event_result | 2;
      }
      return (unsigned int)(event == EVENT_RESOLVE_TRIGGER);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x222bd80; original cards/modern_horizons_2.c:82. */
int basic_landcycling(int player, int card, event_t event, int cless, int black, int blue,
                           int green, int red, int white)
{
  int result;
  if ((uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND) < 2)
  {
    result = cycling(player, card, event, cless, black, blue, green, red, white);
    return result;
  }
  if (event != EVENT_RESOLVE_ACTIVATION_FROM_HAND)
  {
    return 0;
  }
  tutor_basic_land(player, 0, 0);
  return 0;
}

/* ManalinkEh 2022: 0x222bdd0; original cards/modern_horizons_2.c:202. */
void ward_granted(int player, int card, event_t event, int t_player, int t_card, type_t type,
                  subtype_t subtype, int flags, int cless, int black, int blue, int green, int red,
                  int white, int life_to_pay)
{
  int spell_or_target_card;
  card_instance_t *instance;
  int spell_player_or_result;
  int saved_fizzle_or_result;
  unsigned int counter_unless_paid;
  char buffer[100];

  spell_or_target_card = trigger_cause;
  spell_player_or_result = trigger_cause_controller;
  saved_fizzle_or_result = spell_fizzled;
  if (trigger_condition != TRIGGER_SPELL_CAST)
  {
    return;
  }
  if (((card == affected_card) && (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    if (event == EVENT_TRIGGER)
    {
      saved_fizzle_or_result = is_what(trigger_cause_controller, trigger_cause, 1);
      if ((saved_fizzle_or_result == 0) && (1 - t_player == trigger_cause_controller))
      {

        instance = get_card_instance(1 - t_player, trigger_cause);
        counter_unless_paid = 0;
        if (instance->number_of_targets != '\0')
        {
          saved_fizzle_or_result = 0;
          do
          {
            spell_player_or_result = instance->targets[saved_fizzle_or_result].player;
            if (((((t_player == spell_player_or_result) &&
                   (spell_or_target_card = instance->targets[saved_fizzle_or_result].card,
                    spell_or_target_card != -1)) &&
                  ((t_card == spell_or_target_card || (t_card == -1)))) &&
                 (((player != spell_player_or_result || card != spell_or_target_card ||
                    (((unsigned int)flags >> 1 & 1) == 0)) &&
                   ((type == TYPE_NONE || (spell_player_or_result = is_what(
                                               spell_player_or_result, spell_or_target_card, type),
                                           spell_player_or_result != 0)))))) &&
                ((subtype == SUBTYPE_INVALID ||
                  (spell_player_or_result =
                       has_subtype(instance->targets[saved_fizzle_or_result].player,
                                   instance->targets[saved_fizzle_or_result].card, subtype),
                   spell_player_or_result != 0))))
            {
              counter_unless_paid = 2;
              break;
            }
            saved_fizzle_or_result = saved_fizzle_or_result + 1;
          } while (saved_fizzle_or_result < (int)(unsigned int)instance->number_of_targets);
        }
        event_result = event_result | counter_unless_paid;
      }
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {

      counter_unless_paid = flags & 1;
      if (counter_unless_paid == 0)
      {
        counter_unless_paid = 1;
      }
      else if (-1 < hand_count[trigger_cause_controller])
      {

        discard(trigger_cause_controller, 6, player);
        counter_unless_paid = (unsigned int)(spell_fizzled == 1);
      }
      spell_fizzled = saved_fizzle_or_result;
      if ((life_to_pay != 0) &&
          (saved_fizzle_or_result = can_pay_life(spell_player_or_result, life_to_pay),
           saved_fizzle_or_result != 0))
      {

        scnprintf(buffer, 100, "Pay %d life", life_to_pay);
        saved_fizzle_or_result =
            dialog_impl(player, card, EVENT_RESOLVE_TRIGGER, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
                        DLG_WHO_CHOOSES(spell_player_or_result), buffer, 1,
                        ((5 < life[spell_player_or_result] - life_to_pay) ? 20U : 5U),
                        "Decline", 1, 10, 0, 0, 0, 0);
        if (saved_fizzle_or_result == 1)
        {
          counter_unless_paid = 0;
          lose_life(spell_player_or_result, life_to_pay);
        }
      }
      if (((((((cless < 1) && (black < 1)) && (blue < 1)) && ((green < 1 && (red < 1)))) &&
            (white < 1)) ||
           ((saved_fizzle_or_result =
                 has_mana_multi(spell_player_or_result, cless, black, blue, green, red, white),
             saved_fizzle_or_result == 0 ||
                 (saved_fizzle_or_result = charge_mana_multi(spell_player_or_result, cless, black,
                                                             blue, green, red, white),
                  saved_fizzle_or_result == 0)))) &&
          (counter_unless_paid != 0))
      {
        real_counter_a_spell(player, card, spell_player_or_result, spell_or_target_card);
      }
    }
  }
  return;
}

void ward(int player, int card, event_t event, int flags, int cless, int black, int blue, int green,
          int red, int white, int life_to_pay)
{
  ward_granted(player, card, event, player, card, TYPE_NONE, SUBTYPE_INVALID, flags, cless, black,
               blue, green, red, white, life_to_pay);
}

/* ManalinkEh 2022: 0x222c220; original cards/modern_horizons_2.c:231. */
void ward_mana(int player, int card, event_t event, int cless, int black, int blue, int green,
               int red, int white)
{
  ward_granted(player, card, event, player, card, TYPE_NONE, SUBTYPE_INVALID, 0, cless, black, blue,
               green, red, white, 0);
  return;
}

/* ManalinkEh 2022: 0x222c2a0; original cards/modern_horizons_2.c:235. */
void ward_life(int player, int card, event_t event, int life_to_pay)
{
  ward_granted(player, card, event, player, card, TYPE_NONE, SUBTYPE_INVALID, 0, 0, 0, 0, 0, 0, 0,
               life_to_pay);
  return;
}

/* ManalinkEh 2022: 0x222c320; original cards/modern_horizons_2.c:239. */
void ward_discard(int player, int card, event_t event)
{
  ward_granted(player, card, event, player, card, TYPE_NONE, SUBTYPE_INVALID, 1, 0, 0, 0, 0, 0, 0,
               0);
  return;
}

/* ManalinkEh 2022: 0x222c3a0; original cards/modern_horizons_2.c:243. */
void ward_multidiscard(int player, int card, event_t event, int number_to_discard)
{
  ward_granted(player, card, event, player, card, TYPE_NONE, SUBTYPE_INVALID,
               number_to_discard << 0x18 | 1, 0, 0, 0, 0, 0, 0, 0);
  return;
}

/* ManalinkEh 2022: 0x222c420; original cards/modern_horizons_2.c:247. */
int enrage(int player, int card, event_t event)
{
  int result;

  result = damage_dealt_to_me_arbitrary(player, card, event, DDBM_0, player, card);
  return result;
}

/* ManalinkEh 2022: 0x222c460; original cards/modern_horizons_2.c:253. */
int card_abiding_grace(int player, int card, event_t event)
{
  /* Abiding Grace	|2|W
   * Enchantment
   * At the beginning of your end step, choose one -
   * * You gain 1 life.
   * * Return target creature card with mana value 1 from your graveyard to the battlefield. */

  int result;
  test_definition_t test;

  result = eot_trigger(player, card, event);
  if ((result != 0) && (current_turn == player))
  {

    new_default_test_definition(&test, 2, "Select a creature with CMC 1 or less.");
    test.cmc = 2;
    test.cmc_flag = 3;
    result = new_special_count_grave(player, &test);
    if (result != 0)
    {
      result = graveyard_has_shroud(player);
      if (result == 0)
      {
        result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
                             "Gain 1 life", 1, ((life[player] < 6) ? 30U : 10U),
                             "Reanimate", 1, 0x14, 0, 0, 0, 0);
        if (result != 1)
        {
          if (result == 2)
          {
            new_global_tutor(player, player, 2, 4, 1, 1, &test);
          }
          return global_enchantment(player, card, event);
        }
      }
    }
    gain_life(player, 1);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x222a8c0; original cards/modern_horizons_2.c:281. */
static int count_lands_in_play_and_in_hand(int player)
{
  int land;
  card_instance_t *instance;
  int card;
  int count;

  if (active_cards_count[player] < 1)
  {
    return 0;
  }
  card = 0;
  count = 0;
  do
  {
    while (
        (land = is_what(player, card, 1),
         land != 0 && ((instance = in_hand(player, card),
                        instance != NULL || (instance = in_play(player, card), instance != NULL)))))
    {
      count = count + 1;
      card = card + 1;
      if (active_cards_count[player] <= card)
      {
        return count;
      }
    }
    card = card + 1;
  } while (card < active_cards_count[player]);
  return count;
}

/* ManalinkEh 2022: 0x222c600; original cards/modern_horizons_2.c:289. */
int card_abundant_harvest(int player, int card, event_t event)
{
  /* Abundant Harvest	|G
   * Sorcery
   * Choose land or nonland. Reveal cards from the top of your library until you reveal a card of the chosen kind. Put that card into your hand and the rest on the bottom of your library in a random order. */

  int result;
  test_definition_t this_test;

  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      ai_modifier = ai_modifier + 0x14;
      result = count_lands_in_play_and_in_hand(player);
      if (result < 6)
      {
        ai_modifier = ai_modifier + 0x1e;
      }
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {

    result = count_lands_in_play_and_in_hand(player);
    result =
        dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE, "Land",
                    1, ((result < 6) ? 30U : 10U), "Nonland", 1, 0x14, 0, 0, 0, 0);
    default_test_definition(&this_test, 1);
    if (result == 2)
    {
      this_test.type_flag = 1;
    }
    reveal_card_until_you_reveal_test(player, player, &this_test, 0x24);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x222c940; original cards/modern_horizons_2.c:361. */
int card_altar_of_the_goyf(int player, int card, event_t event)
{
  /* Altar of the Goyf	|5
   * Kindred Artifact - Lhurgoyf
   * Whenever a creature you control attacks alone, it gets +X/+X until end of turn, where X is the number of card types among cards in all graveyards.
   * Lhurgoyf creatures you control have trample. */

  card_instance_t *instance;
  int power;
  unsigned int card_00;

  store_attackers(player, card, event, DAT_ATTACKS_ALONE | DAT_TRACK, player, -1, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
  {

    instance = get_card_instance(player, card);
    card_00 = (unsigned int)(unsigned char)instance->targets[2].player;
    power = count_types_in_grave(2);
    instance = in_play(player, card_00);
    if (instance != NULL)
    {
      pump_until_eot(player, card, player, card_00, power, power);
    }
  }
  boost_creature_type(player, card, event, SUBTYPE_LHURGOYF, 0, 0, 0x80,
                      BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  return 0;
}

/* ManalinkEh 2022: 0x222ca50; original cards/modern_horizons_2.c:382. */
int card_arcbound_javelineer(int player, int card, event_t event)
{
  /* Arcbound Javelineer	|W
   * Artifact Creature - Soldier 0/1
   * |T, Remove X +1/+1 counters from this creature: It deals X damage to target attacking or blocking creature.
   * Modular 1 (This creature enters with a +1/+1 counter on it. When it dies, you may put its +1/+1 counters on target artifact creature.) */

  unsigned int packed_damage_and_counter_power;
  card_instance_t *inst;
  card_instance_t *target;
  uint32_t available_counters;
  int value;
  uint32_t data;
  inst_info_t damage_record;
  target_definition_t td;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_IN_COMBAT;
    inst = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0, 0, 0, 0,
                                       &td, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0x84, 0, 0, 0, 0, 0, 0, 0, &td,
                                "Select target attacking or blocking creature.");
      if (spell_fizzled == 1)
      {
        return 0;
      }

      value = get_toughness(inst->targets[0].player, inst->targets[0].card);
      target = get_card_instance(inst->targets[0].player, inst->targets[0].card);

      packed_damage_and_counter_power =
          (uint32_t)target->damage_on_card | ((uint32_t)(uint16_t)target->counter_power << 16);

      available_counters = count_counters(player, card, COUNTER_P1_P1);
      if (player == 0)
      {
        value = count_counters(0, card, COUNTER_P1_P1);
        data = choose_a_number(0, "How many +1/+1 counters you want to remove?", value);
      }
      else
      {
        data = value - (packed_damage_and_counter_power & 0xffff);
        if ((int)available_counters < (int)data)
        {
          data = available_counters;
        }
      }
      if ((-1 < (int)data) &&
          (value = count_counters(player, card, COUNTER_P1_P1), (int)data <= value))
      {
        set_inst_info(inst, 0x59ac, data, 0);
        remove_counters(player, card, player, card, COUNTER_P1_P1, data);
        return 0;
      }
      spell_fizzled = 1;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return modular(player, card, event, 1);
    }
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_IN_COMBAT;
    inst = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (value = valid_target(&td), value != 0))
  {
    damage_record = inst_info(inst, 0x59ac);
    damage_target0(player, card, damage_record.data);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x222cd80; original cards/modern_horizons_2.c:427. */
int card_arcbound_mouser(int player, int card, event_t event)
{
  /* Arcbound Mouser	|W
   * Artifact Creature - Cat 0/0
   * Lifelink
   * Modular 1 (This creature enters with a +1/+1 counter on it. When it dies, you may put its +1/+1 counters on target artifact creature.) */

  int result;
  lifelink(player, card, event);
  result = modular(player, card, event, 1);
  return result;
}

/* ManalinkEh 2022: 0x222cdd0; original cards/modern_horizons_2.c:437. */
int card_arcbound_prototype(int player, int card, event_t event)
{
  /* Arcbound Prototype	|1|W
   * Artifact Creature - Assembly-Worker 0/0
   * Modular 2 (This creature enters with two +1/+1 counters on it. When it dies, you may put its +1/+1 counters on target artifact creature.) */

  int result;
  result = modular(player, card, event, 2);
  return result;
}

/* ManalinkEh 2022: 0x222ce00; original cards/modern_horizons_2.c:445. */
int card_arcbound_shikari(int player, int card, event_t event)
{
  /* Arcbound Shikari	|1|R|W
   * Artifact Creature - Cat Soldier 0/0
   * First strike
   * When this creature enters, put a +1/+1 counter on each other artifact creature you control.
   * Modular 2 (This creature enters with two +1/+1 counters on it. When it dies, you may put its +1/+1 counters on target artifact creature.) */

  int c;
  card_instance_t *instance;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

  if ((event == EVENT_ETB) && (0 < active_cards_count[player]))
  {
    c = 0;
    do
    {
      if (card != c)
      {
        instance = in_play(player, c);
        if (instance != NULL)
        {
          if (is_what(player, c, 0x40) != 0)
          {
            if (is_what(player, c, 2) != 0)
            {
              add_counters(player, card, player, c, COUNTER_P1_P1, 1);
            }
          }
        }
      }
      c = c + 1;
    } while (c < active_cards_count[player]);
  }
  return modular(player, card, event, 2);
}

/* ManalinkEh 2022: 0x222cee0; original cards/modern_horizons_2.c:465. */
int card_arcbound_slasher(int player, int card, event_t event)
{
  /* Arcbound Slasher	|4|R
   * Artifact Creature - Cat 0/0
   * Modular 4 (This creature enters with four +1/+1 counters on it. When it dies, you may put its +1/+1 counters on target artifact creature.)
   * Riot (This creature enters with your choice of an additional +1/+1 counter or haste.) */

  int result;

  riot(player, card, event);
  result = modular(player, card, event, 4);
  return result;
}

/* ManalinkEh 2022: 0x222cf30; original cards/modern_horizons_2.c:475. */
int card_arcbound_tracker(int player, int card, event_t event)
{
  /* Arcbound Tracker	|2|R
   * Artifact Creature - Dog 0/0
   * Menace
   * Modular 2 (This creature enters with two +1/+1 counters on it. When it dies, you may put its +1/+1 counters on target artifact creature.)
   * Whenever you cast a spell other than your first spell each turn, put a +1/+1 counter on this creature. */

  int result;

  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      result = get_specific_storm_count(player);
      if (0 < result)
      {
        sp_ability_trigger(player, event, 2, TYPE_ANY, NULL, player, card);
      }
    }
  }
  if (event == EVENT_SPA)
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  }
  result = modular(player, card, event, 2);
  return result;
}

/* ManalinkEh 2022: 0x222d020; original cards/modern_horizons_2.c:497. */
int card_arcbound_whelp(int player, int card, event_t event)
{
  /* Arcbound Whelp	|3|R
   * Artifact Creature - Dragon 0/0
   * Flying
   * |R: This creature gets +1/+0 until end of turn.
   * Modular 2 (This creature enters with two +1/+1 counters on it. When it dies, you may put its +1/+1 counters on target artifact creature.) */

  unsigned int modular_result;
  unsigned int shade_result;
  modular_result = modular(player, card, event, 2);
  shade_result = generic_shade_merge_pt(player, card, event, 0, 0, 0, 0, 0, 1, 0, 1, 0);
  return shade_result | modular_result;
}

/* ManalinkEh 2022: 0x222d0c0; original cards/modern_horizons_2.c:507. */
int card_archfiend_of_sorrows(int player, int card, event_t event)
{
  /* Archfiend of Sorrows	|5|B|B
   * Creature - Demon 4/5
   * Flying
   * When this creature enters, creatures your opponents control get -2/-2 until end of turn.
   * Unearth |3|B|B (|3|B|B: Return this card from your graveyard to the battlefield. It gains haste. Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

  int result;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    pump_creatures_until_eot(player, card, 1 - player, 0, -2, -2, 0, 0, NULL);
  }
  result = unearth(player, event, 3, 2, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x222d190; original cards/modern_horizons_2.c:533. */
int card_archon_of_cruelty(int player, int card, event_t event)
{
  /* Archon of Cruelty	|6|B|B
   * Creature - Archon 6/6
   * Flying
   * Whenever this creature enters or attacks, target opponent sacrifices a creature or planeswalker of their choice, discards a card, and loses 3 life. You draw a card and gain 3 life. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if ((event == EVENT_DAT_ABILITY || event == EVENT_ETB) &&
      opponent_is_valid_target(player, card))
  {
    int opponent = 1 - player;
    player_sacrifices_a_permanent(player, card, opponent, 0x102, SAC_CAUSED);
    new_multidiscard(opponent, 1, player, 2);
    lose_life(opponent, 3);
    draw_cards(player, 1);
    gain_life(player, 3);
  }
  return 0;
}

/* ManalinkEh 2022: 0x222b9f0; original cards/modern_horizons_2.c:550. */
static char *doesnt_have_p1_p1_counter_and_can_be_activated(int who_chooses, int player, int card)
{
  int result;
  char *error;

  result = count_counters(player, card, COUNTER_P1_P1);
  error = "must not have +1/+1 counters.";
  if (result == 0)
  {
    result = can_use_activated_abilities(player, card);
    error = NULL;
    if (result == 0)
    {
      return "this can\'t use activated abilities.";
    }
  }
  return error;
}

/* ManalinkEh 2022: 0x222d290; original cards/modern_horizons_2.c:560. */
int card_arcus_acolyte(int player, int card, event_t event)
{
  /* Arcus Acolyte	|G|W
   * Creature - Human Cleric Archer 2/2
   * Reach, lifelink
   * Outlast |GW (|GW, |T: Put a +1/+1 counter on this creature. Outlast only as a sorcery.)
   * Each other creature you control without a +1/+1 counter on it has outlast |GW. */

  int c;
  int choice;
  uint8_t selected_mode;
  card_instance_t *creature;
  int self_available;
  int eligible;
  uint32_t another_available;
  unsigned int self_result;
  int index_or_cost;
  card_instance_t *instance;
  target_definition_t td;

  lifelink(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.illegal_state = TARGET_STATE_TAPPED | TARGET_STATE_SUMMONING_SICK;
    td.special = TARGET_SPECIAL_NOT_ME | TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (int32_t)doesnt_have_p1_p1_counter_and_can_be_activated;
    td.illegal_abilities = 0;
    td.preferred_controller = player;
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {

      if (can_sorcery_be_played(player, EVENT_CAN_ACTIVATE) == 0)
      {
        return 0;
      }

      self_available = has_mana_hybrid(
          player, 1, 3, 5, get_cost_mod_for_activated_abilities(player, card, 0, 0, 0, 1, 0, 0));
      self_result = 0;
      if (self_available != 0)
      {
        self_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x801, 0, 0, 0, 0,
                                                0, 0, 0, NULL, NULL);
      }
      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          creature = in_play(player, c);
          if ((((creature != NULL) &&
                (index_or_cost = is_what(player, c, 2), index_or_cost != 0)) &&
               (index_or_cost = check_state(player, c, 0x20010), index_or_cost == 0)) &&
              (index_or_cost = is_humiliated(player, c, AL_NM_ACT), index_or_cost == 0))
          {
            index_or_cost = has_mana_hybrid(
                player, 1, 3, 5, get_cost_mod_for_activated_abilities(player, c, 0, 0, 0, 1, 0, 0));
            if (index_or_cost != 0)
            {
              return self_result | 1;
            }
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
        return self_result;
      }
      return self_result;
    }
    if (event == EVENT_ACTIVATE)
    {

      index_or_cost = 0;
      instance->number_of_targets = '\0';
      instance->mode = '\0';
      self_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x801, 0, 0, 0,
                                                 0, 0, 0, 0, NULL, NULL);
      another_available = 0;
      if (0 < active_cards_count[player])
      {
        do
        {
          creature = in_play(player, index_or_cost);
          if (((creature != NULL) &&
               (eligible = is_what(player, index_or_cost, 2), eligible != 0)) &&
              ((eligible = check_state(player, index_or_cost, 0x20010),
                eligible == 0 &&
                    (eligible = is_humiliated(player, index_or_cost, AL_NM_ACT), eligible == 0))))
          {

            eligible = has_mana_hybrid(
                player, 1, 3, 5,
                get_cost_mod_for_activated_abilities(player, index_or_cost, 0, 0, 0, 1, 0, 0));
            if (eligible != 0)
            {
              another_available = 1;
              break;
            }
          }
          index_or_cost = index_or_cost + 1;
        } while (index_or_cost < active_cards_count[player]);
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1,
                           "Outlast", self_available, 10, "Another creature outlasts",
                           another_available, 0x14, 0, 0, 0, 0);
      if (choice == 1)
      {

        index_or_cost = get_cost_mod_for_activated_abilities(player, card, 0, 0, 0, 1, 0, 0);
        if (charge_mana_hybrid(player, card, 1, 3, 5, index_or_cost) != 0)
        {
          generic_activated_ability(player, card, EVENT_ACTIVATE, 0x801, 0, 0, 0, 0, 0, 0, 0, NULL,
                                    NULL);
        }
      }
      else if ((choice == 2) &&
               (index_or_cost = new_pick_target(
                    &td,
                    "Select anothe untapped, nonsick creature you control without +1/+1 counters.",
                    0, 0x80),
                index_or_cost != 0))
      {

        index_or_cost = get_cost_mod_for_activated_abilities(
            instance->targets[0].player, instance->targets[0].card, 0, 0, 0, 1, 0, 0);
        if (charge_mana_hybrid(player, card, 1, 3, 5, index_or_cost) != 0)
        {
          generic_activated_ability(instance->targets[0].player, instance->targets[0].card,
                                    EVENT_ACTIVATE, 0x801, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
          instance->number_of_targets = '\0';
        }
      }
      if (spell_fizzled == 1)
      {
        return 0;
      }
      instance->mode = (uint8_t)choice;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.illegal_state = TARGET_STATE_TAPPED | TARGET_STATE_SUMMONING_SICK;
    td.special = TARGET_SPECIAL_NOT_ME | TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (int32_t)doesnt_have_p1_p1_counter_and_can_be_activated;
    td.illegal_abilities = 0;
    td.preferred_controller = player;
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    selected_mode = instance->mode;
    if (selected_mode == '\x01')
    {
      creature = in_play(instance->parent_controller, instance->parent_card);
      if (creature != NULL)
      {
        add_counters(player, card, instance->parent_controller, instance->parent_card,
                     COUNTER_P1_P1, 1);
      }
      selected_mode = instance->mode;
    }
    if ((selected_mode == '\x02') &&
        (creature = in_play(instance->targets[0].player, instance->targets[0].card),
         creature != NULL))
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                   COUNTER_P1_P1, 1);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x222da30; original cards/modern_horizons_2.c:646. */
int card_asmoranomardicadaistinaculdacar(int player, int card, event_t event)
{
  /* Asmoranomardicadaistinaculdacar	""
   * Legendary Creature - Human Wizard 3/3
   * As long as you've discarded a card this turn, you may pay |BR to cast this spell.
   * When ~ enters, you may search your library for a card named The Underworld Cookbook, reveal it, put it into your hand, then shuffle.
   * Sacrifice two Foods: Target creature deals 6 damage to itself. */

  card_instance_t *instance;
  target_definition_t td2;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST)
  {
    if (card == affected_card && player == affected_card_controller)
    {
      bool payable = false;
      if (get_trap_condition(player, TRAP_DISCARDED_CARDS))
      {
        int generic = get_updated_casting_cost(player, card, -1, EVENT_MODIFY_COST, 0);
        if (has_mana_hybrid(player, 1, 1, 4, generic))
        {
          instance = get_card_instance(player, card);
          instance->info_slot = 1;
          null_casting_cost(player, card);
          payable = true;
        }
      }
      if (!payable)
      {
        infinite_casting_cost();
        instance = get_card_instance(player, card);
        instance->info_slot = 0;
      }
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if (card == affected_card && player == affected_card_controller &&
        check_special_flags(player, card, SF_NOT_CAST) == 0 && is_token(player, card) == 0 &&
        check_special_flags(player, card, SF_PLAYED_FOR_FREE) == 0)
    {
      instance = get_card_instance(player, card);
      if (instance->info_slot == 1)
        charge_mana_hybrid(player, card, 1, 1, 4, 0);
    }
  }
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 0xff, "Select a card named The Underworld Cookbook.");
    test.id = CARD_ID_THE_UNDERWORLD_COOKBOOK;
    new_global_tutor(player, player, 1, 1, 0, 5, &test);
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td2, TYPE_CREATURE);
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      default_test_definition(&test, 0x47);
      test.subtype = SUBTYPE_FOOD;
      test.qty = 2;
      if (new_can_sacrifice_as_cost(player, card, &test) == 0)
        return 0;
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET,
                                       0, 0, 0, 0, 0, 0, 0, &td2, NULL);
    }
  }
  else
  {
    if (event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
      return 0;
    default_target_definition(player, card, &td2, TYPE_CREATURE);
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_ACTIVATE)
  {
    new_default_test_definition(&test, 0x47, "Select a Food to sacrifice.");
    test.subtype = SUBTYPE_FOOD;
    unsigned int first_food =
        new_sacrifice(player, card, player, SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
    if (first_food != 0)
    {
      unsigned int second_food = new_sacrifice(
          player, card, player, SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
      unsigned int first_food_player = (int)first_food >> 0x10 & 0xff;
      state_untargettable(first_food_player, first_food >> 0x18, 0);
      if (second_food != 0)
      {
        unsigned int second_food_player = (int)second_food >> 0x10 & 0xff;
        state_untargettable(second_food_player, second_food >> 0x18, 0);
        if (spell_fizzled == 1)
          return 0;
        generic_activated_ability(player, card, EVENT_ACTIVATE, GAA_CAN_TARGET,
                                  0, 0, 0, 0, 0, 0, 0, &td2, "TARGET_CREATURE");
        if (spell_fizzled == 1)
          return 0;
        kill_card(first_food_player, first_food >> 0x18, KILL_SACRIFICE);
        kill_card(second_food_player, second_food >> 0x18, KILL_SACRIFICE);
        return 0;
      }
    }
    spell_fizzled = 1;
  }
  else if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td2) != 0)
  {
    int tgt_card = instance->targets[0].card;
    int tgt_player = instance->targets[0].player;
    damage_creature(tgt_player, tgt_card, 6, tgt_player, tgt_card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x222dfe0; original cards/modern_horizons_2.c:745. */
int card_bannerhide_krushok(int player, int card, event_t event)
{
  /* Bannerhide Krushok	|3|G
   * Creature - Beast 4/4
   * Trample
   * Reinforce 2-|1|G (|1|G, Discard this card: Put two +1/+1 counters on target creature.)
   * Scavenge |5|G|G (|5|G|G, Exile this card from your graveyard: Put a number of +1/+1 counters equal to this card's power on target creature. Scavenge only as a sorcery.) */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = scavenge(player, card, event, 5, 0, 0, 2, 0, 0);
    return result;
  }
  return reinforce(player, card, event, 2, 1, 0, 0, 1, 0, 0);
}

/* ManalinkEh 2022: 0x222e0a0; original cards/modern_horizons_2.c:761. */
int card_barbed_spike(int player, int card, event_t event)
{
  /* Barbed Spike	|1|W
   * Artifact - Equipment
   * When this Equipment enters, create a 1/1 colorless Thopter artifact creature token with flying, then attach this Equipment to it.
   * Equipped creature gets +1/+0.
   * Equip |2 */

  int result;
  token_generation_t token;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_token_definition(player, card, CARD_ID_THOPTER, &token);
    token.tou = 1;
    token.pow = 1;
    token.key_plus = 0x20;
    token.action = TOKEN_ACTION_EQUIP;
    generate_token(&token);
  }
  result = vanilla_equipment(player, card, event, 2, 1, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x222e160; original cards/modern_horizons_2.c:782. */
int card_batterbone(int player, int card, event_t event)
{
  /* Batterbone	|2
   * Artifact - Equipment
   * Living weapon (When this Equipment enters, create a 0/0 black Phyrexian Germ creature token, then attach this to it.)
   * Equipped creature gets +1/+1 and has vigilance and lifelink.
   * Equip |5 */

  card_instance_t *instance;
  int result;
  instance = is_equipping(player, card);
  if (instance != NULL)
  {
    result = vanilla_equipment(player, card, event, 5, 1, 1, 0, 10);
    return result;
  }
  result = living_weapon(player, card, event, 5);
  return result;
}

/* ManalinkEh 2022: 0x222e1f0; original cards/modern_horizons_2.c:797. */
int card_battle_plan(int player, int card, event_t event)
{
  /* Battle Plan	|3|R
   * Enchantment
   * At the beginning of combat on your turn, target creature you control gets +2/+0 until end of turn.
   * Basic landcycling |1|R (|1|R, Discard this card: Search your library for a basic land card, reveal it, put it into your hand, then shuffle.) */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = beginning_of_combat(player, card, event, player);
    if (result != 0)
    {

      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      td.allowed_controller = player;
      td.allow_cancel = 0;
      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
      result = can_target(&td);
      if (result != 0)
      {
        result = new_pick_target(&td, "Select target creature you control.", 0, 0x81);
        if (result != 0)
        {
          pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2,
                         0);
        }
      }
    }
    result = global_enchantment(player, card, event);
    return result;
  }
  result = basic_landcycling(player, card, event, 1, 0, 0, 0, 1, 0);
  return result;
}

/* ManalinkEh 2022: 0x222e340; original cards/modern_horizons_2.c:824. */
int card_blacksmiths_skill(int player, int card, event_t event)
{
  /* Blacksmith's Skill	|W
   * Instant
   * Target permanent gains hexproof and indestructible until end of turn. If it's an artifact creature, it gets +2/+2 until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.preferred_controller = player;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      result = is_what(instance->targets[0].player, instance->targets[0].card, 2);
      if ((result == 0) ||
          (result = is_what(instance->targets[0].player, instance->targets[0].card, 0x40),
           result == 0))
      {
        result = 0;
      }
      else
      {
        result = 2;
      }
      pump_ability_until_eot(player, card, instance->targets[0].player,
                             instance->targets[0].card, result, result, 0,
                             (SP_KEYWORD_INDESTRUCTIBLE | SP_KEYWORD_HEXPROOF));
    }
    kill_card(player, card, KILL_BURY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_PERMANENT", 1, NULL);
}

/* ManalinkEh 2022: 0x222e500; original cards/modern_horizons_2.c:851. */
int card_blazing_rootwalla(int player, int card, event_t event)
{
  /* Blazing Rootwalla	|R
   * Creature - Lizard 1/1
   * |R: This creature gets +2/+0 until end of turn. Activate only once each turn.
   * Madness |0 (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

  if ((uint32_t)(event - EVENT_CAN_PAY_MADNESS_COST) < 2)
    return madness(player, card, event, 0, 0, 0, 0, 0, 0);
  if ((uint32_t)(event - EVENT_POW_BOOST) < 2)
  {
    if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x2000,
                                  0, 0, 0, 0, 1, 0, 0, NULL, NULL))
      return 2;
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
    return generic_shade(player, card, EVENT_RESOLVE_ACTIVATION, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 0);
  return generic_activated_ability(player, card, event, 0x2000, 0, 0, 0, 0, 1, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x222e6e0; original cards/modern_horizons_2.c:875. */
int card_blessed_respite(int player, int card, event_t event)
{
  /* Blessed Respite	|1|G
   * Instant
   * Target player shuffles their graveyard into their library. Prevent all combat damage that would be dealt this turn. */

  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, 0x47);
  td.zone = TARGET_ZONE_PLAYERS;
  if (event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller)
  {
    if (current_turn == player)
    {
      td.preferred_controller = player;
      if (current_phase > 0x1d)
      {
        int grave_count = count_graveyard(player);
        ai_modifier += grave_count * 5;
      }
      else
        ai_modifier -= 0x32;
    }
    else
    {
      int opponent = 1 - player;
      td.preferred_controller = opponent;
      if (current_phase < 0x19)
      {
        int grave_count = count_graveyard(player);
        ai_modifier += grave_count * 5;
        for (int creature = 0; creature < active_cards_count[opponent]; ++creature)
          if (in_play(opponent, creature) && is_attacking(opponent, creature))
            ai_modifier += 10;
      }
      else
        ai_modifier -= 0x32;
    }
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      card_instance_t *instance = get_card_instance(player, card);
      reshuffle_grave_into_deck(instance->targets[0].player, 0);
      return card_fog(player, card, event);
    }
    kill_card(player, card, KILL_BURY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
}

/* ManalinkEh 2022: 0x222e960; original cards/modern_horizons_2.c:921. */
int card_bloodbraid_marauder(int player, int card, event_t event)
{
  /* Bloodbraid Marauder	|1|R
   * Creature - Human Berserker 3/1
   * This creature can't block.
   * Delirium - This spell has cascade as long as there are four or more card types among cards in your graveyard. (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.) */

  int result;

  cannot_block(player, card, event);
  if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = delirium(player);
    if (result != 0)
    {
      cascade(player, card, EVENT_CAST_SPELL);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x222b980; original cards/modern_horizons_2.c:936. */
int blossoming_calm_effect(int player, int card, event_t event)
{
  if (event == EVENT_ABILITIES)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      player_bits[player] =
          player_bits[player] | 0x20000;
    }
  }
  else if ((event == EVENT_CAN_SKIP_TURN) && (player == current_turn))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x222e9d0; original cards/modern_horizons_2.c:943. */
int card_blossoming_calm(int player, int card, event_t event)
{
  /* Blossoming Calm	|W
   * Instant
   * You gain hexproof until your next turn. You gain 2 life.
   * Rebound (If you cast this spell from your hand, exile it as it resolves. At the beginning of your next upkeep, you may cast this card from exile without paying its mana cost.) */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    create_legacy_effect(player, card, blossoming_calm_effect);
    gain_life(player, 2);
    rebound(player, card);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x222ea40; original cards/modern_horizons_2.c:959. */
int card_bone_shards(int player, int card, event_t event)
{
  /* Bone Shards	|B
   * Sorcery
   * As an additional cost to cast this spell, sacrifice a creature or discard a card.
   * Destroy target creature or planeswalker. */

  card_instance_t *instance;
  int result;
  unsigned int sacrificed;
  unsigned int player_00;
  target_definition_t td;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x102);
    new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
    get_card_instance(player, card);
    if ((1 < hand_count[player]) ||
        (result = new_can_sacrifice_as_cost(player, card, &test), result != 0))
    {
      return generic_spell(player, card, EVENT_CAN_CAST, 0x81, &td,
                           "Select target creature or planeswalker.", 1, NULL);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x102);
      new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
      get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {

        result = count_subtype(player, 2, -1);
        result =
            dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1,
                        "Sac a creature", 1, result * 10, "Discard a card", 1, 0x1e, 0, 0, 0, 0);
        if (result == 0)
        {
          spell_fizzled = 1;
        }
        else if (result == 1)
        {
          sacrificed = new_sacrifice(player, card, player,
                                     SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
          if (sacrificed == 0)
          {
            spell_fizzled = 1;
          }
          else
          {
            generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                          "Select target creature or planeswalker.", 1, NULL);
            if (-1 < (int)sacrificed)
            {
              player_00 = (int)sacrificed >> 0x10 & 0xff;
              if (spell_fizzled == 1)
              {
                state_untargettable(player_00, sacrificed >> 0x18, 0);
              }
              else
              {
                kill_card(player_00, sacrificed >> 0x18, KILL_SACRIFICE);
              }
            }
          }
        }
        else
        {
          if (result == 2)
          {
            discard(player, 0, 0);
          }
          generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                        "Select target creature or planeswalker.", 1, NULL);
        }
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x102);
    new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = validate_target(player, card, &td, 0);
      if (result != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x222ee70; original cards/modern_horizons_2.c:1019. */
int card_bottle_golems(int player, int card, event_t event)
{
  /* Bottle Golems	|4
   * Artifact Creature - Golem 3/3
   * Trample
   * When this creature dies, you gain life equal to its power. */

  int result;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  result = get_power(player, card);
  gain_life(player, result);
  return 0;
}

/* ManalinkEh 2022: 0x222eed0; original cards/modern_horizons_2.c:1030. */
int card_brainstone(int player, int card, event_t event)
{
  /* Brainstone	|1
   * Artifact
   * |2, |T, Sacrifice this artifact: Draw three cards, then put two cards from your hand on top of your library in any order. */

  int result;
  int selected;
  test_definition_t this_test;
  if (event == EVENT_ACTIVATE)
  {
    ai_modifier = ai_modifier + hand_count[player] * -10 + 0x46;
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    draw_cards(player, 3);
    new_default_test_definition(&this_test, 0xff, "Select a card to put on top.");
    result = hand_count[player] < 3 ? hand_count[player] : 2;
    while (result > 0)
    {
      selected = new_select_a_card(player, player, 4, 1, 3, -1, &this_test);
      if (selected != -1)
      {
        put_on_top_of_deck(player, selected);
        --result;
      }
    }
  }
  return generic_activated_ability(player, card, event, 9, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x222f020; original cards/modern_horizons_2.c:1058. */
int card_break_ties(int player, int card, event_t event)
{
  /* Break Ties	|2|W
   * Instant
   * Choose one -
   * * Destroy target artifact.
   * * Destroy target enchantment.
   * * Exile target card from a graveyard.
   * Reinforce 1-|W (|W, Discard this card: Put a +1/+1 counter on target creature.) */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = card_return_to_nature(player, card, event);
    return result;
  }
  return reinforce(player, card, event, 1, 0, 0, 0, 0, 0, 1);
}

/* ManalinkEh 2022: 0x222b8f0; original cards/modern_horizons_2.c:1077. */
static int is_snow_or_can_produce_colorless(int iid, int val, int player, int card)
{
  int result;
  unsigned int eligible;
  card_instance_t *instance;

  result = has_subtype(player, card, SUBTYPE_SNOW);
  eligible = 1;
  if (result == 0)
  {
    instance = get_card_instance(player, card);
    eligible = (unsigned int)((instance->card_color & 0x41) != 0);
  }
  return eligible;
}

/* ManalinkEh 2022: 0x222b940; original cards/modern_horizons_2.c:1084. */
static char *target_is_snow_or_can_produce_colorless(int who_chooses, int player, int card)
{
  int result;
  char *error;

  result = is_snow_or_can_produce_colorless(-1, -1, player, card);
  error = NULL;
  if (result == 0)
  {
    error = "must be snow or should produce |C";
  }
  return error;
}

/* ManalinkEh 2022: 0x222f3f0; original cards/modern_horizons_2.c:1146. */
int card_breathless_knight(int player, int card, event_t event)
{
  /* Breathless Knight	|1|W|B
   * Creature - Spirit Knight 2/2
   * Flying, lifelink
   * Whenever this creature or another creature you control enters, if that creature entered from a graveyard or you cast it from a graveyard, put a +1/+1 counter on this creature. */

  lifelink(player, card, event);
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    if (check_special_flags(trigger_cause_controller, trigger_cause, 0x10000000) ||
        check_special_flags3(trigger_cause_controller, trigger_cause, 1))
      etb_ability_global(player, event, 2, TYPE_CREATURE, NULL, player, card);
  }
  if (event != EVENT_ETB)
  {
    return 0;
  }
  add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  return 0;
}

/* ManalinkEh 2022: 0x222f4f0; original cards/modern_horizons_2.c:1164. */
int card_breyas_apprentice(int player, int card, event_t event)
{
  /* Breya's Apprentice	|2|R
   * Artifact Creature - Human Artificer 2/3
   * When this creature enters, create a 1/1 colorless Thopter artifact creature token with flying.
   * |T, Sacrifice an artifact: Choose one -
   * * Exile the top card of your library. Until the end of your next turn, you may play that card.
   * * Target creature gets +2/+0 until end of turn. */

  int choice;
  card_instance_t *instance;
  unsigned int sacrificed;
  int target_available;
  int in_combat;
  unsigned int player_00;
  target_definition_t *target;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_THOPTER, &token);
    token.tou = 1;
    token.pow = 1;
    token.key_plus = 32;
    generate_token(&token);
  }
  else
  {
    test_definition_t test;
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      new_default_test_definition(&test, 0x40, "Select an artifact to sacrifice.");
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        return generic_activated_ability_with_sacrifice_as_cost(
            player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, 0, 0, 0, 0, 0, 0, 0, NULL, NULL,
            TYPE_ARTIFACT, &test, SARV_REPORT_CMC);
      }
      if (event == EVENT_ACTIVATE)
      {

        instance->number_of_targets = '\0';
        instance->info_slot = 0;
        target_available = can_target(&td);
        choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1,
                             "+2/+0 to target creature", target_available,
                             -(unsigned int)(current_phase - 0x15U < 4) & 0x14, "Play from library",
                             1, 10, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        sacrificed = new_sacrifice(player, card, player,
                                   SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
        if (sacrificed != 0)
        {
          if (choice == 1)
          {
            new_pick_target(&td, "TARGET_CREATURE", 0, 1);
          }
          player_00 = (int)sacrificed >> 0x10 & 0xff;
          if ((spell_fizzled != 1) && (generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0,
                                                                 0, 0, 0, 0, 0, 0, NULL, NULL),
                                       spell_fizzled != 1))
          {
            if (choice == 1)
            {
              in_combat = is_attacking(instance->targets[0].player, instance->targets[0].card);
              if ((in_combat != 0) ||
                  (in_combat = is_blocking(instance->targets[0].player, instance->targets[0].card),
                   in_combat != 0))
              {
                ai_modifier = ai_modifier + 0x28;
              }
            }
            else if (choice == 2)
            {
              ai_modifier = ai_modifier + (hand_count[player] * 5 + 5) * -2 + 0x46;
            }
            kill_card(player_00, sacrificed >> 0x18, KILL_SACRIFICE);
            add_state(player, card, 0x10);
            tapped_for_mana_color = -1;
            instance->mode = (uint8_t)choice;
            return 0;
          }
          state_untargettable(player_00, sacrificed >> 0x18, 0);
          spell_fizzled = 1;
          return 0;
        }
        spell_fizzled = 1;
        return 0;
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      {
        return 0;
      }
      new_default_test_definition(&test, 0x40, "Select an artifact to sacrifice.");
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      instance = get_card_instance(player, card);
    }
    target = &td;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (instance->mode == '\x01')
      {
        if (valid_target(target) != 0)
        {
          pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                        instance->targets[0].player, instance->targets[0].card, 2,
                                        0);
        }
      }
      else if (instance->mode == '\x02')
      {
        exile_top_x_card_you_may_play_them(player, card, 1, NULL, player,
                                           MPCFE_UNTIL_END_OF_YOUR_NEXT_TURN);
        return 0;
      }
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x222fab0; original cards/modern_horizons_2.c:1252. */
int card_burdened_aerialist(int player, int card, event_t event)
{
  /* Burdened Aerialist	|2|U
   * Creature - Human Pirate 3/1
   * When this creature enters, create a Treasure token. (It's an artifact with "|T, Sacrifice this token: Add one mana of any color.")
   * Whenever you sacrifice a token, this creature gains flying until end of turn. */

  int result;
  card_instance_t *instance;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    generate_token_by_id(player, card, CARD_ID_TREASURE);
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {

    instance = in_play(affected_card_controller, affected_card);
    if ((instance != NULL) && (affected_card_controller == player))
    {
      if (affected_card != card)
      {
        result = is_what(player, affected_card, 0x47);
        if ((result != 0) && (instance->kill_code == '\x03'))
        {
          result = is_token(affected_card_controller, affected_card);
          if (result != 0)
          {
            result = check_special_flags(affected_card_controller, affected_card, 0x800);
            if (result == 0)
            {

              instance = get_card_instance(player, card);
              if (instance->targets[0xb].player < 0)
              {
                instance->targets[0xb].player = 0;
              }
              *(char *)&instance->targets[0xb].player =
                  (char)instance->targets[0xb].player + '\x01';
            }
          }
        }
      }
    }
  }
  result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  pump_ability_until_eot_no_repeat(player, card, player, card, 0x20, 0);
  return 0;
}

/* ManalinkEh 2022: 0x222fc40; original cards/modern_horizons_2.c:1288. */
int card_cabal_initiate(int player, int card, event_t event)
{
  /* Cabal Initiate	|1|B
   * Creature - Human Warlock 2/1
   * Discard a card: This creature gains lifelink until end of turn.
   * Threshold - This creature gets +1/+2 as long as there are seven or more cards in your graveyard. */

  card_instance_t *instance;
  int result;

  if (event == EVENT_POWER)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = has_threshold(player), result != 0)) &&
        (result = is_humiliated(player, card, AL_STATIC), result == 0))
    {
      event_result = event_result + 1;
    }
  }
  else
  {
    if (event == EVENT_TOUGHNESS)
    {
      if (card != affected_card)
      {
        return 0;
      }
      if (player != affected_card_controller)
      {
        return 0;
      }
      result = has_threshold(player);
      if (result == 0)
      {
        return 0;
      }
      result = is_humiliated(player, card, AL_STATIC);
      if (result != 0)
      {
        return 0;
      }
      event_result = event_result + 2;
      return 0;
    }
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x20, 0, 0, 0, 0, 0, 0,
                                         0, NULL, NULL);
      }
      if (event == EVENT_ACTIVATE)
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, 0x20, 0, 0, 0, 0, 0, 0, 0, NULL,
                                  NULL);
        if (spell_fizzled == 1)
        {
          return 0;
        }
        result = check_for_special_ability(player, card, 2);
        if (result != 0)
        {
          ai_modifier = ai_modifier + -0xc;
          return 0;
        }
        result = is_attacking(player, card);
        if ((result == 0) && (result = is_blocking(player, card), result == 0))
        {
          return 0;
        }
        ai_modifier = ai_modifier + life[player] * -5 + 100;
        return 0;
      }
      return 0;
    }
  }
  if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  pump_ability_until_eot_no_repeat(player, card, instance->parent_controller, instance->parent_card,
                                   0, 2);
  return 0;
}

/* ManalinkEh 2022: 0x222ff10; original cards/modern_horizons_2.c:1326. */
int card_calibrated_blast(int player, int card, event_t event)
{
  /* Calibrated Blast	|2|R
   * Instant
   * Reveal cards from the top of your library until you reveal a nonland card. Put the revealed cards on the bottom of your library in a random order. When you reveal a nonland card this way, ~ deals damage equal to that card's mana value to any target.
   * Flashback |3|R|R (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

  int result;
  target_definition_t td;
  test_definition_t this_test;
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card != affected_card) || (player != affected_card_controller))
      {
        return do_flashback(player, card, event, 3, 0, 0, 0, 2, 0);
      }
    }
    else
    {
      if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
      {
        default_target_definition(player, card, &td, 0x102);
        td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
        if (event == EVENT_RESOLVE_SPELL)
        {
          result = valid_target(&td);
          if (result != 0)
          {
            default_test_definition(&this_test, 1);
            this_test.type_flag = 1;
            result = reveal_card_until_you_reveal_test(player, player, &this_test, 0x204);
            if (-1 < result)
            {
              result = get_cmc_by_internal_id(result);
              damage_target0(player, card, result);
            }
          }
          result = get_flashback(player, card);
          if (result == 0)
          {
            kill_card(player, card, KILL_DESTROY);
          }
          else
          {
            kill_card(player, card, KILL_EXILE);
          }
        }
        return generic_spell(player, card, event, 0x81, &td,
                             "Select target creature, planeswalker or player.", 1, NULL);
      }
      if (event != EVENT_CHANGE_TARGET)
        return do_flashback(player, card, event, 3, 0, 0, 0, 2, 0);
    }
  }
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  return generic_spell(player, card, event, 0x81, &td,
                       "Select target creature, planeswalker or player.", 1, NULL);
}

/* ManalinkEh 2022: 0x222a950; original cards/modern_horizons_2.c:1364. */
static int devouring_type(int player, int card, int type)
{
  int count;
  int next_index;
  const char *permanent_type_text;
  card_instance_t *instance;
  int csvid;
  int last_index_or_result;
  unsigned int sacrificed;
  const char *article_suffix;
  int devoured_count;
  int *devoured_cursor;
  bool more_devoured;
  int desired_count;
  target_definition_t td;
  char msg[100];
  test_definition_t test;
  int devoured[151];

  base_target_definition(player, card, &td, type);
  td.illegal_abilities = 0;
  td.allowed_controller = player;
  td.preferred_controller = player;
  default_test_definition(&test, type);
  permanent_type_text = type_text(msg, 100, test.type, 0);
  if ((test.type == 4) || (test.type == 0x40))
  {
    article_suffix = "n";
  }
  else
  {
    article_suffix = "";
  }
  sprintf(test.message, "Select a%s %s to sacrifice.", article_suffix, permanent_type_text);
  instance = get_card_instance(player, card);
  if ((ai_is_speculating == 1) ||
      ((desired_count = -1, player == 1 && (((unsigned char)trace_mode & 2) == 0))))
  {
    count = count_subtype(player, type, -1);
    desired_count = count / 3;
  }
  instance->targets[1].player = 0;
  instance->targets[1].card = 0;
  csvid = get_id(player, card);
  devoured_count = 0;
  do
  {
    last_index_or_result = can_target(&td);
    if ((last_index_or_result == 0) ||
        (sacrificed = new_sacrifice(player, card, player,
                                    SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test),
         sacrificed == 0))
    {
      last_index_or_result = devoured_count + -1;
      desired_count = devoured_count;
      break;
    }
    instance->targets[1].card = instance->targets[1].card + 1;
    if ((csvid == CARD_ID_VORACIOUS_DRAGON) &&
        (last_index_or_result = has_subtype(player, sacrificed >> 0x18, SUBTYPE_GOBLIN),
         last_index_or_result != 0))
    {
      instance->targets[1].player = instance->targets[1].player + 1;
    }
    next_index = devoured_count + 1;
    devoured[devoured_count] = sacrificed >> 0x18;
    last_index_or_result = devoured_count;
    devoured_count = next_index;
  } while (desired_count != next_index);
  if (-1 < last_index_or_result)
  {
    devoured_cursor = devoured + last_index_or_result;
    do
    {
      kill_card(player, *devoured_cursor, KILL_SACRIFICE);
      more_devoured = devoured_cursor != devoured;
      devoured_cursor = devoured_cursor + -1;
    } while (more_devoured);
  }
  return desired_count;
}

/* ManalinkEh 2022: 0x2230120; original cards/modern_horizons_2.c:1409. */
void devour_type(int player, int card, event_t event, int type, int number)
{
  int result;
  card_instance_t *instance;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = can_sacrifice_type_as_cost(player, 1, type);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      instance->state = instance->state | STATE_OUBLIETTED;
      result = devouring_type(player, card, type);
      instance->state = instance->state & 0xff7fffff;
      add_counters(player, card, player, card, COUNTER_P1_P1, number * result);
      if (0 < result)
      {
        instance = get_card_instance(player, card);
        instance->targets[2].player = 0x42;
      }
    }
  }
  return;
}

/* ManalinkEh 2022: 0x22301f0; original cards/modern_horizons_2.c:1430. */
int card_caprichrome(int player, int card, event_t event)
{
  /* Caprichrome	|3|W
   * Artifact Creature - Goat 2/2
   * Flash
   * Vigilance
   * Devour artifact 1 (As this creature enters, you may sacrifice any number of artifacts. It enters with that many +1/+1 counters on it.) */

  int result;

  vigilance(player, card, event);
  devour_type(player, card, event, 0x40, 1);
  result = flash(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2230250; original cards/modern_horizons_2.c:1442. */
int card_captain_ripley_vance(int player, int card, event_t event)
{
  /* Captain Ripley Vance	|2|R
   * Legendary Creature - Human Pirate 3/2
   * Whenever you cast your third spell each turn, put a +1/+1 counter on ~, then it deals damage equal to its power to any target. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((trigger_condition == TRIGGER_SPELL_CAST) && (trigger_cause_controller == player))
  {
    result = get_specific_storm_count(player);
    if (result == 2)
    {
      sp_ability_trigger(player, event, 2, TYPE_ANY, NULL, player, card);
    }
  }
  if (event != EVENT_SPA)
  {
    return 0;
  }
  add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "Select target creature, planeswalker or player.", 0, 0x80);
    if (result != 0)
    {
      result = get_power(player, card);
      damage_target0(player, card, result);
    }
  }
  instance = get_card_instance(player, card);
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x222aef0; original cards/modern_horizons_2.c:1484. */
static void carth_ability(int player, int card)
{
  test_definition_t test;
  new_default_test_definition(&test, 0x100, "Select a planeswalker card.");
  reveal_top_cards_of_library_and_choose(player, card, player, 7, 0, TUTOR_HAND, 1,
                                         TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test);
  return;
}

/* ManalinkEh 2022: 0x222af70; original cards/modern_horizons_2.c:1489. */
static int carth_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (event == EVENT_STATIC_EFFECTS && card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player >= 0 &&
        get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_CARTH_THE_LION)
      kill_card(player, card, KILL_EXILE);
  }
  instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0 &&
      effect_follows_control_of_attachment(player, card, event))
    return 0;
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    instance = get_card_instance(player, card);
    card_instance_t *dying = in_play(affected_card_controller, affected_card);
    if (dying && get_card_instance(affected_card_controller, affected_card)->kill_code != 0)
    {
      if (instance->damage_target_player >= 0 && instance->damage_target_card == affected_card &&
          instance->damage_target_player == affected_card_controller)
      {
        instance->damage_target_card = -1;
        instance->damage_target_player = -1;
        remove_status(player, card, 0x10000);
      }
      count_for_gfp_ability(player, card, 0x77, player, 0x100, NULL);
    }
  }
  unsigned int deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (!check_status(player, card, 0x10000))
  {
    for (deaths &= 0xff; deaths != 0;)
    {
      --deaths;
      carth_ability(player, card);
    }
    kill_card(player, card, KILL_EXILE);
  }
  if (event == EVENT_CLEANUP && !check_status(player, card, 0x10000))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x22303f0; original cards/modern_horizons_2.c:1538. */
int card_carth_the_lion(int player, int card, event_t event)
{
  /* Carth the Lion	|2|B|G
   * Legendary Creature - Human Warrior 3/5
   * Whenever Carth enters or a planeswalker you control dies, look at the top seven cards of your library. You may reveal a planeswalker card from among them and put it into your hand. Put the rest on the bottom of your library in a random order.
   * Planeswalkers' loyalty abilities you activate cost an additional [+1] to activate. */

  int c;
  card_instance_t *instance;
  unsigned int deaths;
  int legacy_card;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    carth_ability(player, card);
  }
  else if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      bool found = false;
      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          instance = in_play(player, c);

          if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
               (instance = get_card_instance(player, c), instance->info_slot_fn == carth_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            found = true;
            break;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }

      if (!found)
      {
        legacy_card = create_targetted_legacy_effect(player, card, carth_effect, player, card);
        instance = get_card_instance(player, legacy_card);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, legacy_card, 0x10000);
      }
    }
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    count_for_gfp_ability(player, card, 0x77, player, 0x100, NULL);
  }
  instance = get_card_instance(player, card);
  if (instance->kill_code == '\0')
  {

    deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    deaths = deaths & 0xff;
    if (deaths != 0)
    {
      do
      {
        deaths = deaths - 1;
        carth_ability(player, card);
      } while (0 < (int)deaths);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22305e0; original cards/modern_horizons_2.c:1596. */
int card_chatterfang_squirrel_general(int player, int card, event_t event)
{
  /* Chatterfang, Squirrel General	|2|G
   * Legendary Creature - Squirrel Warrior 3/3
   * Forestwalk (This creature can't be blocked as long as defending player controls a Forest.)
   * If one or more tokens would be created under your control, those tokens plus that many 1/1 green Squirrel creature tokens are created instead.
   * |B, Sacrifice X Squirrels: Target creature gets +X/-X until end of turn. */

  enum
  {
    SARV_REPORT_COUNT = 8
  };

  int sacrificed_count;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if (!IS_GAA_EVENT(event))
    return 0;

  new_default_test_definition(&test, 0x47, "Select a Squirrel to sacrifice.");
  test.subtype = 0xc0;
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    sacrificed_count = instance->info_slot;
    instance->info_slot = 0;
    if (valid_target(&td) != 0)
    {
      pump_until_eot_merge_previous(player, card, instance->targets[0].player,
                                    instance->targets[0].card, sacrificed_count,
                                    -sacrificed_count);
    }
  }
  return generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_CAN_TARGET, 0, 1, 0, 0, 0, 0, 0, &td, "TARGET_CREATURE", TYPE_NONE,
      &test, SARV_REPORT_CMC | SARV_REPORT_COUNT);
}

/* ManalinkEh 2022: 0x22307c0; original cards/modern_horizons_2.c:1626. */
int card_chatterstorm(int player, int card, event_t event)
{
  /* Chatterstorm	|1|G
   * Sorcery
   * Create a 1/1 green Squirrel creature token.
   * Storm (When you cast this spell, copy it for each spell cast before it this turn.) */

  int result;
  token_generation_t token;

  if (event == EVENT_QUERY)
  {
    if (attacking_card == 4)
    {
      return 1;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    default_token_definition(player, card, CARD_ID_SQUIRREL, &token);
    token.tou = 1;
    token.pow = 1;
    token.color_forced = '\b';
    generate_token(&token);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2230860; original cards/modern_horizons_2.c:1655. */
int card_chitterspitter(int player, int card, event_t event)
{
  /* Chitterspitter	|2|G
   * Artifact
   * At the beginning of your upkeep, you may sacrifice a token. If you do, put an acorn counter on this artifact.
   * Squirrels you control get +1/+1 for each acorn counter on this artifact.
   * |G, |T: Create a 1/1 green Squirrel creature token. */

  int result;
  card_instance_t *instance;
  unsigned int sacrificed;
  test_definition_t test;

  result = duh_mode(player);
  upkeep_trigger_ability_mode(player, card, event, player,
                              (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    new_default_test_definition(&test, 0x47, "Select a token to sacrifice.");
    test.type_flag = 6;
    result = new_can_sacrifice(player, card, player, &test);
    if (result != 0)
    {

      sacrificed = new_sacrifice(player, card, player, SAC_RETURN_CHOICE | SAC_JUST_MARK, &test);
      if (sacrificed != 0)
      {
        add_counters(player, card, player, card, COUNTER_ACORN, 1);
        kill_card(player, sacrificed >> 0x18, KILL_SACRIFICE);
      }
    }
  }
  else if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if ((result == 0) && (affected_card_controller == player))
      {
        result = has_subtype(player, affected_card, SUBTYPE_SQUIRREL);
        if (result != 0)
        {
          result = count_counters(player, card, COUNTER_ACORN);
          event_result = event_result + result;
        }
      }
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    generate_token_by_id(player, card, CARD_ID_SQUIRREL);
  }
  return generic_activated_ability(player, card, event, 1, 0, 0, 0, 1, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x2230a90; original cards/modern_horizons_2.c:1690. */
int card_chrome_courier(int player, int card, event_t event)
{
  /* Chrome Courier	|1|W|U
   * Artifact Creature - Thopter 1/1
   * Flying
   * When this creature enters, reveal the top two cards of your library. Put one of them into your hand and the other into your graveyard. If you put an artifact card into your hand this way, you gain 3 life. */

  int result;
  test_definition_t test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 0xff, "Select a card.");
    result = reveal_top_cards_of_library_and_choose(player, card, player, 2, 1, TUTOR_HAND, 1,
                                                    TUTOR_GRAVE, 0, &test);
    if (-1 < result)
    {
      result = is_what(player, result, 0x40);
      if (result != 0)
      {
        gain_life(player, 3);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2230b70; original cards/modern_horizons_2.c:1709. */
int card_clattering_augur(int player, int card, event_t event)
{
  /* Clattering Augur	|1|B
   * Creature - Skeleton Shaman 1/1
   * This creature can't block.
   * When this creature enters, you draw a card and you lose 1 life.
   * |2|B|B: Return this card from your graveyard to your hand. */

  int result;
  cannot_block(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    draw_a_card(player);
    lose_life(player, 1);
  }
  else if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = has_mana_multi(player, 2, 2, 0, 0, 0, 0);
    if (result != 0)
    {
      return 4;
    }
  }
  else if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    charge_mana_multi(player, 2, 2, 0, 0, 0, 0);
    return (unsigned int)(spell_fizzled != 1);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2230c90; original cards/modern_horizons_2.c:1742. */
int card_combine_chrysalis(int player, int card, event_t event)
{
  /* Combine Chrysalis	|G|U
   * Artifact
   * Creature tokens you control have flying.
   * |2|G|U, |T, Sacrifice a token: Create a 4/4 green Beast creature token. Activate only as a sorcery. */

  int result;
  card_instance_t *instance;
  token_generation_t token;
  test_definition_t test;

  if (event == EVENT_ABILITIES)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if ((result == 0) && (affected_card_controller == player))
      {
        result = is_what(player, affected_card, 2);
        if (result != 0)
        {
          result = is_token(affected_card_controller, affected_card);
          if (result != 0)
          {
            event_result = event_result | 0x20;
          }
        }
      }
    }
  }
  else if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
            (event == EVENT_RESOLVE_ACTIVATION)) ||
           (event == EVENT_CLEANUP))
  {
    new_default_test_definition(&test, 0x47, "Select a token to sacrifice.");
    test.type_flag = 6;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      default_token_definition(player, card, CARD_ID_BEAST, &token);
      token.tou = 4;
      token.pow = 4;
      token.color_forced = '\b';
      generate_token(&token);
    }
    result = generic_activated_ability_with_sacrifice_as_cost(
        player, card, event, GAA_CAN_SORCERY_BE_PLAYED | GAA_UNTAPPED, 2, 0, 1, 1, 0, 0, 0, NULL,
        NULL, TYPE_NONE, &test, SARV_REPORT_CMC);
    return result;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2230fc0; original cards/modern_horizons_2.c:1807. */
int card_crack_open(int player, int card, event_t event)
{
  /* Crack Open	|2|G
   * Sorcery
   * Destroy target artifact or enchantment. Create a Treasure token. (It's an artifact with "|T, Sacrifice this token: Add one mana of any color.") */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x44);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      generate_token_by_id(player, card, CARD_ID_TREASURE);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "DISENCHANT", 1, NULL);
}

/* ManalinkEh 2022: 0x2231120; original cards/modern_horizons_2.c:1832. */
int card_dakkon_shadow_slayer(int player, int card, event_t event)
{
  /* Dakkon, Shadow Slayer	|W|U|B
   * Legendary Planeswalker - Dakkon (0)
   * Dakkon enters with a number of loyalty counters on him equal to the number of lands you control.
   * +1: Surveil 2.
   * -3: Exile target creature.
   * -6: You may put an artifact card from your hand or graveyard onto the battlefield. */

  enum
  {
    CHOICE_SURVEIL = 1,
    CHOICE_EXILE = 2,
    CHOICE_TUTOR_ARTIFACT = 3
  };

  card_instance_t *instance;
  int result;
  target_definition_t td;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = count_permanents_by_type(player, TYPE_LAND);
      enters_the_battlefield_with_counters(player, card, EVENT_CAST_SPELL, COUNTER_LOYALTY, result);
    }
  }
  else if ((event == EVENT_RESOLVE_ACTIVATION) ||
           (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE))
  {

    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    result = can_target(&td);
    result = dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, DLG_MODE, "Surveil 2", 1,
                         10, 1, "Exile target creature", result, 0x14, 0xfffffffd,
                         "Tutor artifact from hand or graveyard", 1, 5, 0xfffffffa, 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (result == 0)
      {
        return 0;
      }
    }
    else if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      if (result == CHOICE_EXILE)
      {
        new_pick_target(&td, "TARGET_CREATURE", 0, 1);
      }
    }
    else if (result == CHOICE_SURVEIL)
    {
      surveil(player, 2);
    }
    else if (result == CHOICE_EXILE)
    {
      result = validate_target(player, card, &td, 0);
      if (result != 0)
      {
        kill_target0(player, card, KILL_EXILE);
      }
    }
    else
    {

      new_default_test_definition(&this_test, 0x40, "Select an artifact card.");
      result = new_global_tutor(player, player, 4, 4, 0, 2, &this_test);
      if (result < 0)
      {
        new_global_tutor(player, player, 2, 4, 0, 2, &this_test);
      }
    }
  }
  result = planeswalker(player, card, event, 0);
  return result;
}

/* ManalinkEh 2022: 0x2231740; original cards/modern_horizons_2.c:1949. */
int etb_tapped_indestructible_land(int player, int card, event_t event)
{
  int result;

  comes_into_play_tapped(player, card, event);
  indestructible(player, card, event);
  result = plain_mana_producer(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x222ba50; original cards/modern_horizons_2.c:1961. */
static int card_exiled_with_void_counter(int player, int card, event_t event)
{

  card_instance_t *instance;
  int counters;
  int iid_or_exiled_count;
  int card_00;
  int matching_legacies;

  if (event == EVENT_SET_LEGACY_EFFECT_NAME)
  {
    instance = get_card_instance(player, card);
    if (-1 < instance->targets[0].card)
    {

      instance = get_card_instance(player, card);
      scnprintf(set_legacy_effect_name_addr, 0x33, "%s",
                cards_ptr[cards_data[instance->targets[0].card].id]->name);
    }
  }
  else
  {
    if (event == EVENT_SET_LEGACY_EFFECT_TEXT)
    {
      scnprintf(set_legacy_effect_name_addr, 0x33, "Exiled with a void counter.");
      return 0;
    }
    if (event == EVENT_STATIC_EFFECTS)
    {

      instance = get_card_instance(player, card);
      matching_legacies = 0;
      iid_or_exiled_count = instance->targets[0].card;
      if (0 < active_cards_count[player])
      {
        card_00 = 0;
        do
        {
          instance = in_play(player, card_00);
          if (instance != NULL)
          {
            if (is_what(player, card_00, 0x80) != 0)
            {

              instance = get_card_instance(player, card_00);
              if (instance->info_slot_fn == card_exiled_with_void_counter)
              {
                counters = count_counters(player, card_00, COUNTER_VOID);
                if ((counters != 0) && (iid_or_exiled_count == instance->targets[0].card))
                {
                  matching_legacies = matching_legacies + 1;
                }
              }
            }
          }
          card_00 = card_00 + 1;
        } while (card_00 < active_cards_count[player]);
      }
      iid_or_exiled_count = count_iid_in_rfg(player, iid_or_exiled_count);
      if (iid_or_exiled_count < matching_legacies)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2232bb0; original cards/modern_horizons_2.c:2121. */
int card_deepwood_denizen(int player, int card, event_t event)
{
  /* Deepwood Denizen	|2|G
   * Creature - Elf Warrior 3/2
   * Vigilance (Attacking doesn't cause this creature to tap.)
   * |5|G, |T: Draw a card. This ability costs |1 less to activate for each +1/+1 counter on creatures you control. */

  int result;
  vigilance(player, card, event);
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    draw_a_card(player);
  }
  result = count_counters_by_counter_and_card_type(player, COUNTER_P1_P1, TYPE_CREATURE);
  return generic_activated_ability(player, card, event, 1,
                                     5U - result & (int)(5U - result ^ 0xffffffff) >> 0x1f, 0, 0, 1,
                                     0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x222b300; original cards/modern_horizons_2.c:2140. */
int dermotaxi_legacy(int player, int card, event_t event)
{
  char attached_player;
  int t_card;
  card_instance_t *effect;
  card_instance_t *attached;
  unsigned char artifact_type;
  uint32_t argument1;
  inst_info_t imprint_record;
  uint32_t card_type;

  if (event != EVENT_CHANGE_TYPE)
  {
    attached_is_additional_subtype(player, card, event, SUBTYPE_VEHICLE);
    if (event != EVENT_CLEANUP)
    {
      return 0;
    }
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  effect = get_card_instance(player, card);
  if (-1 < effect->damage_target_player)
  {

    effect = get_card_instance(player, card);
    attached = get_card_instance((int)effect->damage_target_player, effect->damage_target_card);
    t_card = effect->damage_target_card;
    attached_player = effect->damage_target_player;
    imprint_record = inst_info(effect, 0x59dd);
    argument1 = imprint_record.data;
    if ((((t_card == affected_card) && (affected_card_controller == attached_player)) &&
         (argument1 != 0xffffffff)) &&
        ((((unsigned int)land_can_be_played >> 16 & 0xff) & 2) == 0))
    {

      artifact_type = cards_data[argument1].type | 0x40;
      if (cards_data[argument1].type == artifact_type)
      {
        effect->targets[7].player = argument1;
        effect->dummy3 = argument1;
        attached->regen_status = attached->regen_status | 0x1000000;
        card_type = argument1;
      }
      else if ((effect->targets[7].player != argument1) ||
               (card_type = effect->dummy3, cards_data[effect->dummy3].type != artifact_type))
      {

        card_type = create_a_card_type(argument1);
        if (card_type == 0xffffffff)
        {
          return 0;
        }
        cards_data[card_type].type = artifact_type;
        effect->dummy3 = card_type;
        effect->targets[7].player = argument1;
        attached->regen_status = attached->regen_status | 0x1000000;
      }
      event_result = card_type;
    }
  }
  attached_is_additional_subtype(player, card, EVENT_CHANGE_TYPE, SUBTYPE_VEHICLE);
  return 0;
}

/* ManalinkEh 2022: 0x2232c90; original cards/modern_horizons_2.c:2191. */
int card_dermotaxi(int player, int card, event_t event)
{
  /* Dermotaxi	|2
   * Artifact - Vehicle 0/0
   * Imprint - As this Vehicle enters, exile a creature card from a graveyard.
   * Tap two untapped creatures you control: Until end of turn, this Vehicle becomes a copy of the exiled card, except it's a Vehicle artifact in addition to its other types. */

  target_t *ret_location;
  unsigned short exiled_csvid;
  uint32_t data;
  int value;
  card_instance_t *effect_or_card;
  int opponent_creatures;
  int32_t player_00;
  target_t *target_cursor;
  target_t *next_target;
  inst_info_t imprint_record;
  int second_creature;
  unsigned int selected_count;
  card_instance_t *instance;

  if (event == EVENT_RESOLVE_SPELL)
  {
    test_definition_t test;
    value = count_graveyard_by_type(player, 2);
    player_00 = 1 - player;
    opponent_creatures = count_graveyard_by_type(player_00, 2);
    effect_or_card = get_card_instance(player, card);
    if (opponent_creatures == 0)
    {
      if (value != 0)
      {
        player_00 = player;
      }
    }
    else if (value != 0)
    {
      target_definition_t td;
      default_target_definition(player, card, &td, 0xff);
      td.zone = TARGET_ZONE_PLAYERS;
      td.allow_cancel = 0;
      td.illegal_abilities = 0;
      if (new_pick_target(&td, "Select the owner of the graveyard you want to search.", 0, 0x80) !=
          0)
      {
        player_00 = effect_or_card->targets[0].player;
        effect_or_card->number_of_targets = '\0';
      }
    }
    new_default_test_definition(&test, 2, "Select a creature card to imprint on Dermotaxi.");
    value = new_select_a_card(player, player_00, 2, 1, 2, -1, &test);
    data = graveyard_ptr_mutable[player_00][value];
    rfg_card_from_grave(player_00, value);
    set_inst_info(effect_or_card, 0x59dd, data, (uint16_t)player_00);
  }
  else
  {
    target_definition_t td2;
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      default_target_definition(player, card, &td2, 2);
      td2.illegal_abilities = 0;
      td2.allowed_controller = player;
      td2.preferred_controller = player;
      td2.illegal_state = TARGET_STATE_TAPPED;
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 0, 0, 0, 0, 0,
                                      NULL, NULL) == 0)
        {
          return 0;
        }
        value = target_available(player, card, &td2);
        return (unsigned int)(1 < value);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      {
        return 0;
      }
      instance = get_card_instance(player, card);
      default_target_definition(player, card, &td2, 2);
    }
    td2.allowed_controller = player;
    td2.illegal_state = TARGET_STATE_TAPPED;
    td2.illegal_abilities = 0;
    td2.preferred_controller = td2.allowed_controller;
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      if (charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) != 0)
      {
        ret_location = instance->targets;
        second_creature = -1;
        if (select_target(player, card, &td2, "Select a creature to tap.", ret_location) != 0)
        {
          state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
          value = instance->targets[0].card;
          if (select_target(player, card, &td2, "Select a creature to tap.",
                            instance->targets + 1) == 0)
          {
            selected_count = 1;
          }
          else
          {
            state_untargettable(instance->targets[1].player, instance->targets[1].card, 1);
            second_creature = instance->targets[1].card;
            selected_count = 2;
          }
          target_cursor = ret_location;
          do
          {
            next_target = target_cursor + 1;
            state_untargettable(target_cursor->player, target_cursor->card, 0);
            target_cursor = next_target;
          } while (next_target != ret_location + selected_count);
          if (1 < selected_count)
          {
            instance->number_of_targets = '\0';
            tap_card(player, value);
            tap_card(player, second_creature);
            return 0;
          }
        }
        spell_fizzled = 1;
        return 0;
      }
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) &&
             (imprint_record = inst_info(instance, 0x59dd), imprint_record.data != 0))
    {
      imprint_record = inst_info(instance, 0x59dd);
      exiled_csvid = cards_data[imprint_record.data].id;
      imprint_record = inst_info(instance, 0x59dd);
      if (check_rfg(((uint32_t)imprint_record.id | ((uint32_t)imprint_record.data2 << 16)) >> 0x10,
                    (unsigned int)exiled_csvid) != 0)
      {
        effect_or_card =
            get_card_instance(player, create_targetted_legacy_effect(player, card, dermotaxi_legacy,
                                                                     instance->parent_controller,
                                                                     instance->parent_card));
        imprint_record = inst_info(instance, 0x59dd);
        set_inst_info(effect_or_card, 0x59dd, imprint_record.data, 0);
        return 0;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2233220; original cards/modern_horizons_2.c:2291. */
int card_dihadas_ploy(int player, int card, event_t event)
{
  /* Dihada's Ploy	|1|U|B
   * Instant
   * Draw two cards, then discard a card. You gain life equal to the number of cards you've discarded this turn.
   * Jump-start (You may cast this card from your graveyard by discarding a card in addition to paying its other costs. Then exile this card.) */

  int result;
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      draw_cards(player, 2);
      discard(player, 0, player);
      result = get_trap_condition(player, 0x203);
      gain_life(player, result);
      kill_card(player, card, KILL_DESTROY);
    }
    result = jumpstart(player, card, event, 0x59df);
    return result;
  }
  return basic_spell(player, card, EVENT_CAN_CAST);
}

/* ManalinkEh 2022: 0x22332d0; original cards/modern_horizons_2.c:2317. */
int card_disciple_of_the_sun(int player, int card, event_t event)
{
  /* Disciple of the Sun	|4|W
   * Creature - Human Cleric 3/3
   * Lifelink
   * When this creature enters, return target permanent card with mana value 3 or less from your graveyard to your hand. */

  int result;
  test_definition_t test;
  lifelink(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 0x47,
                                "Select target target permanent card with mana value 3 or less.");
    test.cmc = 4;
    test.cmc_flag = 3;
    result = new_special_count_grave(player, &test);
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

/* ManalinkEh 2022: 0x22333b0; original cards/modern_horizons_2.c:2341. */
int card_dragons_rage_channeler(int player, int card, event_t event)
{
  /* Dragon's Rage Channeler	|R
   * Creature - Human Shaman 1/1
   * Whenever you cast a noncreature spell, surveil 1. (Look at the top card of your library. You may put that card into your graveyard.)
   * Delirium - As long as there are four or more card types among cards in your graveyard, this creature gets +2/+2, has flying, and attacks each combat if able. */

  if (trigger_condition == TRIGGER_SPELL_CAST && trigger_cause_controller == player)
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 2);
    this_test.type_flag = 1;
    sp_ability_trigger(player, event, 2, TYPE_ANY, &this_test, player, card);
  }
  if (event == EVENT_SPA)
    surveil(player, 1);
  else if (((uint32_t)(event - EVENT_POWER) < 3 || event == EVENT_BEGIN_COMBAT) &&
           !is_humiliated(player, card, AL_STATIC) && delirium(player))
  {
    if (affect_me(player, card))
    {
      if (event == EVENT_POWER || event == EVENT_TOUGHNESS)
        event_result += 2;
      else if (event == EVENT_ABILITIES)
        event_result |= 0x20;
    }
    attack_if_able(player, card, event);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2233510; original cards/modern_horizons_2.c:2374. */
int card_dress_down(int player, int card, event_t event)
{
  /* Dress Down	|1|U
   * Enchantment
   * Flash
   * When this enchantment enters, draw a card.
   * Creatures lose all abilities.
   * At the beginning of the end step, sacrifice this enchantment. */

  int result;
  card_instance_t *instance;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    draw_cards(player, 1);
    if (!eot_trigger(player, card, EVENT_ETB))
      return flash(player, card, event);
    kill_card(player, card, KILL_SACRIFICE);
  }
  else if (eot_trigger(player, card, event))
    kill_card(player, card, KILL_SACRIFICE);
  if ((event != EVENT_ABILITIES) && (event != EVENT_CAST_SPELL))
  {
    return flash(player, card, event);
  }
  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = in_play(affected_card_controller, affected_card);
    if (instance != NULL)
    {
      result = is_what(affected_card_controller, affected_card, 2);
      if (result != 0)
      {

        instance = get_card_instance(affected_card_controller, affected_card);
        event_result = event_result & 0x9f080000;
        instance->state = instance->state & 0xffffdfff;
        instance->ext->sp_keywords = instance->ext->sp_keywords & 0x993c00c0;
        humiliate(player, card, affected_card_controller, affected_card, 1);
        return flash(player, card, event);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2233690; original cards/modern_horizons_2.c:2410. */
int card_drey_keeper(int player, int card, event_t event)
{
  /* Drey Keeper	|3|B|G
   * Creature - Elf Druid 2/2
   * When this creature enters, create two 1/1 green Squirrel creature tokens.
   * |3|B: Squirrels you control get +1/+0 and gain menace until end of turn. */

  int result;
  test_definition_t this_test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    generate_tokens_by_id(player, card, CARD_ID_SQUIRREL, 2);
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    default_test_definition(&this_test, 2);
    this_test.subtype = 0xc0;
    pump_creatures_until_eot(player, card, player, 0, 1, 0, 0, 0x10000, &this_test);
  }
  result = generic_activated_ability(player, card, event, 0, 3, 1, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x22337d0; original cards/modern_horizons_2.c:2440. */
int card_duskshell_crawler(int player, int card, event_t event)
{
  /* Duskshell Crawler	|1|G
   * Creature - Insect 0/3
   * When this creature enters, put a +1/+1 counter on target creature.
   * Each creature you control with a +1/+1 counter on it has trample. (It can deal excess combat damage to the player or planeswalker it's attacking.) */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    if (event == EVENT_ABILITIES)
    {
      result = is_what(affected_card_controller, affected_card, 2);
      if (result != 0)
      {
        result = is_humiliated(player, card, AL_STATIC);
        if (result == 0)
        {
          result = count_counters(affected_card_controller, affected_card, COUNTER_P1_P1);
          if (0 < result)
          {
            event_result = event_result | 0x80;
          }
        }
      }
    }
    return 0;
  }

  default_target_definition(player, card, &td, 2);
  td.allow_cancel = 0;
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = pick_target(&td, "TARGET_CREATURE");
    if (result != 0)
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                               COUNTER_P1_P1, 1);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x2233940; original cards/modern_horizons_2.c:2472. */
int card_echoing_return(int player, int card, event_t event)
{
  /* Echoing Return	|B
   * Sorcery
   * Return target creature card and all other cards with the same name as that card from your graveyard to your hand. */

  int count;
  short base_value;
  int *graveyard;
  int iid_or_count;
  int grave_index;
  test_definition_t this_test;
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        new_default_test_definition(&this_test, 2, "Select a creature card.");
        if ((card != affected_card) || (player != affected_card_controller))
          return generic_spell(player, card, event, 8, NULL, NULL, 1, &this_test);
        generic_spell(player, card, EVENT_CAST_SPELL, 8, NULL, NULL, 1, &this_test);
        if ((spell_fizzled != 1) && (ai_is_speculating == 1))
        {
          graveyard = graveyard_ptr_mutable[player];
          iid_or_count = validate_target_from_grave_source(player, card, player, 0);
          this_test.id = (int)cards_data[graveyard[iid_or_count]].id;
          base_value = cards_ptr[graveyard[iid_or_count]]->ai_base_value;
          iid_or_count = new_special_count_grave(player, &this_test);
          ai_modifier = ai_modifier + base_value * iid_or_count;
        }
      }
      return 0;
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      new_default_test_definition(&this_test, 2, "Select a creature card.");
      if (event == EVENT_RESOLVE_SPELL)
      {
        iid_or_count = validate_target_from_grave_source(player, card, player, 0);
        if (iid_or_count != -1)
        {
          iid_or_count = graveyard_ptr_mutable[player][iid_or_count];
          count = count_graveyard(player);
          grave_index = count + -1;
          if (-1 < grave_index)
          {
            do
            {
              if (graveyard_ptr_mutable[player][grave_index] == iid_or_count)
              {
                from_grave_to_hand(player, grave_index, 1);
              }
              grave_index = grave_index + -1;
            } while (grave_index != -1);
          }
        }
        kill_card(player, card, KILL_DESTROY);
      }
      return generic_spell(player, card, event, 8, NULL, NULL, 1, &this_test);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  new_default_test_definition(&this_test, 2, "Select a creature card.");
  return generic_spell(player, card, event, 8, NULL, NULL, 1, &this_test);
}

/* ManalinkEh 2022: 0x2233bd0; original cards/modern_horizons_2.c:2513. */
int card_endurance(int player, int card, event_t event)
{
  /* Endurance	|1|G|G
   * Creature - Elemental Incarnation 3/4
   * Flash
   * Reach
   * When this creature enters, up to one target player puts all the cards from their graveyard on the bottom of their library in a random order.
   * Evoke-Exile a green card from your hand. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  evoke_with_cards_in_hand(player, card, event, 8);
  if (event == EVENT_ETB)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.zone = TARGET_ZONE_PLAYERS;
    card_instance_t *instance = get_card_instance(player, card);
    if (can_target(&td) && pick_target(&td, "TARGET_PLAYER"))
    {
      int t_player = instance->targets[0].player;
      int count = count_graveyard(t_player);
      if (count < 1)
        count = 0;
      else
        for (int position = count - 1; position >= 0; --position)
        {
          int iid = graveyard_ptr_mutable[t_player][position];
          remove_card_from_grave(t_player, position);
          raw_put_iid_on_top_of_deck(t_player, iid);
        }
      put_top_x_on_bottom_in_random_order(t_player, count);
    }
    instance->number_of_targets = 0;
    if (evoked(player, card))
      kill_card(player, card, KILL_SACRIFICE);
  }
  return flash(player, card, event);
}

/* ManalinkEh 2022: 0x2233d30; original cards/modern_horizons_2.c:2553. */
int card_esper_sentinel(int player, int card, event_t event)
{
  /* Esper Sentinel	|W
   * Artifact Creature - Human Soldier 1/1
   * Whenever an opponent casts their first noncreature spell each turn, draw a card unless that player pays |X, where X is this creature's power. */

  int storm_count_or_power;
  int creature_spells_or_payment;
  int opponent;
  test_definition_t test;

  if ((trigger_condition == TRIGGER_SPELL_CAST) && (reason_for_trigger_controller == player))
  {
    opponent = 1 - player;
    storm_count_or_power = get_specific_storm_count(opponent);
    creature_spells_or_payment = get_stormcreature_count(opponent);
    if (storm_count_or_power == creature_spells_or_payment)
    {
      default_test_definition(&test, 2);
      test.type_flag = 1;
      sp_ability_trigger(opponent, event, 2, TYPE_NONE, &test, player, card);
    }
  }
  if (event == EVENT_SPA)
  {

    storm_count_or_power = get_power(player, card);
    creature_spells_or_payment = has_mana(1 - player, 0, storm_count_or_power);
    if ((creature_spells_or_payment != 0) &&
        (storm_count_or_power = charge_mana(1 - player, COLOR_COLORLESS, storm_count_or_power),
         storm_count_or_power != 0))
    {
      return 0;
    }
    draw_cards(player, 1);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2233e60; original cards/modern_horizons_2.c:2581. */
int card_etherium_spinner(int player, int card, event_t event)
{
  /* Etherium Spinner	|2|U
   * Artifact Creature - Human Wizard 2/1
   * Whenever you cast a spell with mana value 4 or greater, create a 1/1 colorless Thopter artifact creature token with flying. */

  test_definition_t test;

  if ((trigger_condition == TRIGGER_SPELL_CAST) && (trigger_cause_controller == player))
  {
    default_test_definition(&test, 0xff);
    test.cmc = 3;
    test.cmc_flag = 2;
    sp_ability_trigger(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event != EVENT_SPA)
  {
    return 0;
  }
  generate_token_by_id(player, card, CARD_ID_THOPTER);
  return 0;
}

/* ManalinkEh 2022: 0x2233f20; original cards/modern_horizons_2.c:2602. */
int card_ethersworn_sphinx(int player, int card, event_t event)
{
  /* Ethersworn Sphinx	|7|W|U
   * Artifact Creature - Sphinx 4/4
   * Affinity for artifacts (This spell costs |1 less to cast for each artifact you control.)
   * Flying
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card with lesser mana value. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.) */

  affinity(player, card, event, 0x40, -1);
  cascade(player, card, event);
  return 0;
}

/* ManalinkEh 2022: 0x2233f70; original cards/modern_horizons_2.c:2614. */
int card_fae_offering(int player, int card, event_t event)
{
  /* Fae Offering	|2|G
   * Enchantment
   * At the beginning of each end step, if you've cast both a creature spell and a noncreature spell this turn, create a Clue token, a Food token, and a Treasure token. */

  int creature_spells;
  int all_spells;
  if (eot_trigger(player, card, event) != 0)
  {
    creature_spells = get_stormcreature_count(player);
    if (creature_spells != 0)
    {
      all_spells = get_specific_storm_count(player);
      if (creature_spells < all_spells)
      {
        generate_token_by_id(player, card, CARD_ID_CLUE);
        generate_token_by_id(player, card, CARD_ID_FOOD);
        generate_token_by_id(player, card, CARD_ID_TREASURE);
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2234010; original cards/modern_horizons_2.c:2632. */
int card_fairgrounds_patrol(int player, int card, event_t event)
{
  /* Fairgrounds Patrol	|1|W
   * Creature - Human Soldier 2/1
   * |1|W, Exile this card from your graveyard: Create a 1/1 colorless Thopter artifact creature token with flying. Activate only as a sorcery. */

  int result;
  token_generation_t token;
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = has_mana_multi(player, 1, 0, 0, 0, 0, 1);
    if ((result != 0) &&
        (result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY), result != 0))
    {
      return 0x20;
    }
  }
  else if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    charge_mana_multi(player, 1, 0, 0, 0, 0, 1);
    if (spell_fizzled != 1)
    {
      return 2;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATED_GRAVEYARD_ABILITY)
  {
    default_token_definition(player, card, CARD_ID_THOPTER, &token);
    token.tou = 1;
    token.pow = 1;
    token.key_plus = 0x20;
    token.action = TOKEN_ACTION_EQUIP;
    generate_token(&token);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2234150; original cards/modern_horizons_2.c:2660. */
int card_faithless_salvaging(int player, int card, event_t event)
{
  /* Faithless Salvaging	|1|R
   * Instant
   * Discard a card, then draw a card.
   * Rebound (If you cast this spell from your hand, exile it as it resolves. At the beginning of your next upkeep, you may cast this card from exile without paying its mana cost.) */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    discard(player, 0, 0);
    draw_a_card(player);
    rebound(player, card);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22341b0; original cards/modern_horizons_2.c:2675. */
int card_fast_furious(int player, int card, event_t event)
{
  /* Fast // Furious	|2|R // |3|R|R
   * Instant // Sorcery
   * Fast:
   * Discard a card, then draw two cards.
   *
   * Furious:
   * Furious deals 3 damage to each creature without flying. */

  int p;
  card_instance_t *instance;
  int eligible;
  unsigned int selected_modes;
  int second_priority;
  int priority_first_half;
  int c;

  instance = get_card_instance(player, card);
  p = current_turn;
  if (event == EVENT_MODIFY_COST)
  {
    generic_split_card(player, card, EVENT_MODIFY_COST, 0x59f0, 1, 10, 0x59ff, 1, 0x14);
    return 0;
  }
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
      int totals[2];
      totals[0] = 0;
      totals[1] = 0;

      priority_first_half = hand_count[player] * -10 + 0x46;
      c = active_cards_count[current_turn] + -1;
      if (-1 < c)
      {
        do
        {
          instance = in_play(p, c);
          if ((((instance != NULL) && (eligible = is_what(p, c, 2), eligible != 0)) &&
               (eligible = get_toughness(p, c), eligible < 4)) &&
              ((eligible = check_for_ability(p, c, 0x4000),
                eligible == 0 && (eligible = check_for_ability(p, c, 0x20), eligible == 0))))
          {
            totals[p] = totals[p] + 1;
          }
          c = c + -1;
        } while (c != -1);
      }
      p = 1 - current_turn;
      c = active_cards_count[p] + -1;
      if (-1 < c)
      {
        do
        {
          instance = in_play(p, c);
          if (((instance != NULL) && (eligible = is_what(p, c, 2), eligible != 0)) &&
              ((eligible = get_toughness(p, c),
                eligible < 4 && ((eligible = check_for_ability(p, c, 0x4000),
                                  eligible == 0 &&
                                      (eligible = check_for_ability(p, c, 0x20), eligible == 0))))))
          {
            totals[p] = totals[p] + 1;
          }
          c = c + -1;
        } while (c != -1);
      }
      second_priority = totals[player] * -10 + 0x14 + totals[1 - player] * 10;
      return generic_split_card(player, card, event, 0x59f0, 1, priority_first_half, 0x59ff, 1,
                                second_priority);
    }
    if (event != EVENT_CAN_CHANGE_TARGET)
    {
      if (event == EVENT_RESOLVE_SPELL)
      {
        selected_modes = instance->info_slot;
        if ((selected_modes & 1) != 0)
        {
          discard(player, 0, 0);
          draw_cards(player, 2);
          selected_modes = instance->info_slot;
        }
        if ((selected_modes & 2) != 0)
        {
          test_definition_t this_test;
          default_test_definition(&this_test, 2);
          this_test.keyword = KEYWORD_FLYING;
          this_test.keyword_flag = DOESNT_MATCH;
          new_damage_all(player, card, 2, 3, 0, &this_test);
        }
        kill_card(player, card, KILL_DESTROY);
      }
      else if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
    }
  }
  second_priority = 0x14;
  priority_first_half = 10;
  return generic_split_card(player, card, event, 0x59f0, 1, priority_first_half, 0x59ff, 1,
                            second_priority);
}

/* ManalinkEh 2022: 0x2234550; original cards/modern_horizons_2.c:2727. */
int card_feast_of_sanity(int player, int card, event_t event)
{
  /* Feast of Sanity	|3|B
   * Enchantment
   * Whenever you discard a card, this enchantment deals 1 damage to any target and you gain 1 life. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  result = discard_trigger(player, card, event, player, 2, 0);
  if (result != 0)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "Select target creature, planeswalker or player.", 0, 0x80);
      if (result != 0)
      {
        damage_target0(player, card, 1);
        gain_life(player, 1);
      }
    }
    instance = get_card_instance(player, card);
    instance->number_of_targets = '\0';
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2234640; original cards/modern_horizons_2.c:2749. */
int card_filigree_attendant(int player, int card, event_t event)
{
  /* Filigree Attendant	|2|U|U
   * Artifact Creature - Homunculus * /3
   * Flying
   * ~'s power is equal to the number of artifacts you control. */

  int newval;

  if (((event == EVENT_POWER) && (card == affected_card)) && (player == affected_card_controller))
  {
    newval = count_permanents_by_type(player, TYPE_ARTIFACT);
    set_base_pt(newval);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2234690; original cards/modern_horizons_2.c:2762. */
int card_flame_blitz(int player, int card, event_t event)
{
  /* Flame Blitz	|R
   * Enchantment
   * At the beginning of your end step, this enchantment deals 5 damage to each planeswalker.
   * Cycling |2 (|2, Discard this card: Draw a card.) */

  int result;
  test_definition_t this_test;
  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    if (current_turn == player)
    {
      result = eot_trigger(player, card, event);
      if (result != 0)
      {
        default_test_definition(&this_test, 0x100);
        new_damage_all(player, card, 2, 5, 0, &this_test);
      }
    }
    result = global_enchantment(player, card, event);
    return result;
  }
  result = cycling(player, card, event, 2, 0, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x2234790; original cards/modern_horizons_2.c:2782. */
int card_flametongue_yearling(int player, int card, event_t event)
{
  /* Flametongue Yearling	|R|R
   * Creature - Kavu 2/1
   * Multikicker |2 (You may pay an additional |2 any number of times as you cast this spell.)
   * This creature enters with a +1/+1 counter on it for each time it was kicked.
   * When this creature enters, it deals damage equal to its power to target creature. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 0;
      result = is_token(player, card);
      if (result == 0)
      {
        result = check_special_flags(player, card, 0x40000);
        if ((result == 0) && (spell_fizzled != 1))
        {
          result = do_multikicker(player, card, EVENT_CAST_SPELL, 2, 0, 0, 0, 0, 0);
          instance->info_slot = result;
        }
      }
    }
    instance = get_card_instance(player, card);
    enters_the_battlefield_with_counters(player, card, EVENT_CAST_SPELL, COUNTER_P1_P1,
                                         instance->info_slot);
    etb_ability(player, card, EVENT_CAST_SPELL, RESOLVE_TRIGGER_MANDATORY);
    return 0;
  }
  instance = get_card_instance(player, card);
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, instance->info_slot);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.allow_cancel = 0;
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "TARGET_CREATURE", 0, 0);
    if (result != 0)
    {
      result = get_power(player, card);
      damage_target0(player, card, result);
    }
  }
  instance = get_card_instance(player, card);
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x22349c0; original cards/modern_horizons_2.c:2819. */
int card_flay_essence(int player, int card, event_t event)
{
  /* Flay Essence	|1|B|B
   * Sorcery
   * Exile target creature or planeswalker. You gain life equal to the number of counters on it. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 3);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      result = count_counters(instance->targets[0].player, instance->targets[0].card,
                              ~COUNTER_P1_P1);
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
      gain_life(player, result);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td, "Select target creature or planeswalker.",
                         1, NULL);
}

/* ManalinkEh 2022: 0x2234b40; original cards/modern_horizons_2.c:2844. */
int card_floodhound(int player, int card, event_t event)
{
  /* Floodhound	|U
   * Creature - Elemental Dog 1/2
   * |3, |T: Investigate. (Create a Clue token. It's an artifact with "|2, Sacrifice this token: Draw a card.") */

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    ai_modifier = ai_modifier + hand_count[player] * -10 + 70;
    investigate(player, card, player);
  }
  return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3),
                                    0, NULL, NULL);
}

/* ManalinkEh 2022: 0x2234c00; original cards/modern_horizons_2.c:2858. */
int card_flourishing_strike(int player, int card, event_t event)
{
  /* Flourishing Strike	|1|G
   * Instant
   * Choose one -
   * * ~ deals 5 damage to target creature with flying.
   * * Target creature gets +3/+3 until end of turn.
   * Entwine |2|G (Choose both if you pay the entwine cost.) */

  int choice;
  unsigned char selected_mode;
  card_instance_t *instance;
  int pump_available_or_valid;
  int entwine_available;
  unsigned int entwine_or_target_index;
  int damage_available_or_index;
  target_definition_t td;
  target_definition_t td2;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.required_abilities = 0x20;
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = player;
    get_card_instance(player, card);
    damage_available_or_index = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    if ((damage_available_or_index != 0) ||
        (damage_available_or_index =
             generic_spell(player, card, EVENT_CAN_CAST, 1, &td2, NULL, 1, NULL),
         damage_available_or_index != 0))
    {
      return 1;
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.required_abilities = 0x20;
      default_target_definition(player, card, &td2, 2);
      td2.preferred_controller = player;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {

        instance->number_of_targets = '\0';
        if (check_special_flags2(player, card, -0x80000000) == 0)
        {

          instance->mode = '\0';
          damage_available_or_index = can_target(&td);
          pump_available_or_valid = can_target(&td2);
          entwine_or_target_index = 0;
          if ((pump_available_or_valid != 0) && (damage_available_or_index != 0))
          {
            entwine_available = has_mana_multi(player, 2, 0, 0, 1, 0, 0);
            entwine_or_target_index = (unsigned int)(entwine_available != 0);
          }
          choice =
              dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_MODE,
                          "5 damage to target creature with flying", damage_available_or_index, 10,
                          "Target creature gets +3/+3 until end of turn", pump_available_or_valid,
                          5, "Entwine", entwine_or_target_index, 0xf, 0, 0, 0, 0);
          if (choice == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
          if ((choice == 3) && (charge_mana_multi(player, 2, 0, 0, 1, 0, 0), spell_fizzled == 1))
          {
            return 0;
          }
        }
        selected_mode = instance->mode;
        if ((selected_mode & 1) == 0)
        {
          entwine_or_target_index = 0;
        }
        else
        {
          damage_available_or_index =
              new_pick_target(&td, "Select target creature with flying.", 0, 0x81);
          entwine_or_target_index = (unsigned int)(damage_available_or_index != 0);
          if (spell_fizzled == 1)
          {
            return 0;
          }
          selected_mode = instance->mode;
        }
        if ((selected_mode & 2) != 0)
        {
          new_pick_target(&td2, "Select target creature to get +3/+3", entwine_or_target_index,
                          0x81);
        }
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.required_abilities = 0x20;
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      selected_mode = instance->mode;
      damage_available_or_index = 0;
      if ((selected_mode & 1) != 0)
      {
        if (validate_target(player, card, &td, 0) != 0)
        {
          damage_target0(player, card, 5);
        }
        selected_mode = instance->mode;
        damage_available_or_index = 1;
      }
      if (((selected_mode & 2) != 0) &&
          (pump_available_or_valid = validate_target(player, card, &td2, damage_available_or_index),
           pump_available_or_valid != 0))
      {
        pump_until_eot(player, card, instance->targets[damage_available_or_index].player,
                       instance->targets[damage_available_or_index].card, 3, 3);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2235190; original cards/modern_horizons_2.c:2942. */
int card_fodder_tosser(int player, int card, event_t event)
{
  /* Fodder Tosser	|3
   * Artifact
   * |T, Discard a card: This artifact deals 2 damage to target player or planeswalker. */

  int result;
  target_definition_t td;
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 0x100);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0x100);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      damage_target0(player, card, 2);
    }
  }
  result = generic_activated_ability(player, card, event, 0x25, 0, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_PLAYER_OR_PLANESWALKER");
  return result;
}

/* ManalinkEh 2022: 0x22352a0; original cards/modern_horizons_2.c:2961. */
int card_foul_watcher(int player, int card, event_t event)
{
  /* Foul Watcher	|1|U
   * Creature - Nightmare Bird 1/2
   * Flying
   * When this creature enters, surveil 1. (Look at the top card of your library. You may put that card into your graveyard.)
   * Delirium - This creature gets +1/+0 as long as there are four or more card types among cards in your graveyard. */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    if (((event == EVENT_POWER) && (card == affected_card)) && (player == affected_card_controller))
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = delirium(player);
        if (result != 0)
        {
          event_result = event_result + 1;
        }
      }
    }
    return 0;
  }
  surveil(player, 1);
  return 0;
}

/* ManalinkEh 2022: 0x2235350; original cards/modern_horizons_2.c:2980. */
int card_foundation_breaker(int player, int card, event_t event)
{
  /* Foundation Breaker	|3|G
   * Creature - Elemental 2/2
   * When this creature enters, you may destroy target artifact or enchantment.
   * Evoke |1|G (You may cast this spell for its evoke cost. If you do, it's sacrificed when it enters.) */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 0x44);
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "DISENCHANT", 0, 0);
      if (result != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
    }
    instance->number_of_targets = '\0';
    result = evoked(player, card);
    if (result != 0)
    {
      kill_card(player, card, KILL_SACRIFICE);
    }
  }
  result = evoke(player, card, event, 1, 0, 0, 1, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22354a0; original cards/modern_horizons_2.c:3005. */
int card_foundry_helix(int player, int card, event_t event)
{
  /* Foundry Helix	|1|R|W
   * Instant
   * As an additional cost to cast this spell, sacrifice a permanent.
   * ~ deals 4 damage to any target. If the sacrificed permanent was an artifact, you gain 4 life. */

  card_instance_t *instance;
  unsigned int sacrificed_or_owner;
  int result;
  unsigned int card_00;
  target_definition_t td;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    new_default_test_definition(&test, 0x47, "Select a permanent to sacrifice.");
    get_card_instance(player, card);
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_ANY", 1, NULL);
    if (result != 0)
    {
      return new_can_sacrifice_as_cost(player, card, &test);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      new_default_test_definition(&test, 0x47, "Select a permanent to sacrifice.");
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        sacrificed_or_owner = new_sacrifice(player, card, player,
                                            SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
        if (sacrificed_or_owner == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_ANY", 1, NULL);
        card_00 = sacrificed_or_owner >> 0x18;
        sacrificed_or_owner = sacrificed_or_owner >> 0x10 & 0xff;
        if (spell_fizzled == 1)
        {
          state_untargettable(sacrificed_or_owner, card_00, 0);
        }
        else
        {
          result = is_what(sacrificed_or_owner, card_00, 0x40);
          instance->info_slot = result;
          kill_card(sacrificed_or_owner, card_00, KILL_SACRIFICE);
        }
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    new_default_test_definition(&test, 0x47, "Select a permanent to sacrifice.");
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = valid_target(&td);
      if ((result != 0) && (damage_target0(player, card, 4), instance->info_slot == 1))
      {
        gain_life(player, 4);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2235800; original cards/modern_horizons_2.c:3055. */
int card_fractured_sanity(int player, int card, event_t event)
{
  /* Fractured Sanity	|U|U|U
   * Sorcery
   * Each opponent mills fourteen cards.
   * Cycling |1|U (|1|U, Discard this card: Draw a card.)
   * When you cycle this card, each opponent mills four cards. */

  int result;
  if (1 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    if (event == EVENT_RESOLVE_ACTIVATION_FROM_HAND)
    {
      if (spell_fizzled != 1)
      {
        mill(1 - player, 4);
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      mill(1 - player, 0xe);
      kill_card(player, card, KILL_DESTROY);
    }
    result = basic_spell(player, card, event);
    return result;
  }
  return cycling(player, card, event, 1, 0, 1, 0, 0, 0);
}

/* ManalinkEh 2022: 0x22358e0; original cards/modern_horizons_2.c:3079. */
int card_funnel_web_recluse(int player, int card, event_t event)
{
  /* Funnel-Web Recluse	|4|G
   * Creature - Spider 3/5
   * Reach
   * Morbid - When this creature enters, if a creature died this turn, investigate. (Create a Clue token. It's an artifact with "|2, Sacrifice this token: Draw a card.") */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if ((event == EVENT_ETB) && (0 < creatures_dead_this_turn))
  {
    investigate(player, card, player);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2235950; original cards/modern_horizons_2.c:3095. */
int card_fury2(int player, int card, event_t event)
{
  /* Fury	|3|R|R
   * Creature - Elemental Incarnation 3/3
   * Double strike
   * When this creature enters, it deals 4 damage divided as you choose among any number of target creatures and/or planeswalkers.
   * Evoke-Exile a red card from your hand. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 0x102);
    td.allow_cancel = 0;
    target_and_divide_damage(player, card, &td, "TARGET_CREATURE_OR_PLANESWALKER", 4);
    instance = get_card_instance(player, card);
    instance->number_of_targets = '\0';
    result = evoked(player, card);
    if (result != 0)
    {
      kill_card(player, card, KILL_SACRIFICE);
    }
  }
  result = evoke_with_cards_in_hand(player, card, event, 0x10);
  return result;
}

/* ManalinkEh 2022: 0x2235a30; original cards/modern_horizons_2.c:3118. */
int card_gaeas_will(int player, int card, event_t event)
{
  /* Gaea's Will	""
   * Sorcery
   * Suspend 4-|G
   * Until end of turn, you may play lands and cast spells from your graveyard.
   * If a card would be put into your graveyard from anywhere this turn, exile that card instead. */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    create_legacy_activate(player, card, effect_yawgmoths_will);
    kill_card(player, card, KILL_DESTROY);
  }
  result = suspend_only(player, card, event, 4, 0, 0, 0, 1, 0, 0);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2235ad0; original cards/modern_horizons_2.c:3134. */
int card_galvanic_relay(int player, int card, event_t event)
{
  /* Galvanic Relay	|2|R
   * Sorcery
   * Exile the top card of your library. During your next turn, you may play that card.
   * Storm (When you cast this spell, copy it for each spell cast before it this turn.) */

  enum
  {
    MPCFE_DURING_YOUR_NEXT_TURN = 1024
  };

  int result;
  card_instance_t *instance;
  bool own_turn;

  if (event == EVENT_QUERY)
  {
    if (attacking_card == 4)
    {
      return 1;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {

    result = exile_top_x_card_you_may_play_them(player, card, 1, NULL, player,
                                                MPCFE_DURING_YOUR_NEXT_TURN |
                                                    MPCFE_UNTIL_END_OF_YOUR_NEXT_TURN);
    own_turn = current_turn == player;
    instance = get_card_instance(player, result);
    instance->targets[1].card = (unsigned int)own_turn;
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int is_artifact_land(int iid, int unused, int unused2, int unused3);
static int is_snow_or_can_produce_colorless(int iid, int val, int player, int card);
static int count_lands_in_play_and_in_hand(int player);
static int carth_effect(int player, int card, event_t event);
static int tide_shaper_effect(int player, int card, event_t event);
int one_or_more_p1_p1_counters_are_putted_on_me(int player, int card, event_t event);
void ward_granted(int player, int card, event_t event, int t_player, int t_card, type_t type,
                  subtype_t subtype, int flags, int cless, int black, int blue, int green, int red,
                  int white, int life_to_pay);
void ward_life(int player, int card, event_t event, int life_to_pay);

/* ManalinkEh 2022: 0x2235b90; original cards/modern_horizons_2.c:3152. */
int card_gargadon(int player, int card, event_t event)
{
  /* Gargadon	|5|R|R
   * Creature - Beast 7/5
   * Trample
   * Suspend 4-|1|R (Rather than cast this card from your hand, you may pay |1|R and exile it with four time counters on it. At the beginning of your upkeep, remove a time counter. When the last is removed, you may cast it without paying its mana cost. It has haste.) */

  int result;
  result = suspend(player, card, event, 4, 1, 0, 0, 0, 1, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2236070; original cards/modern_horizons_2.c:3203. */
int card_general_ferrous_rokiric(int player, int card, event_t event)
{
  /* General Ferrous Rokiric	|1|R|W
   * Legendary Creature - Human Soldier 3/1
   * Hexproof from monocolored
   * Whenever you cast a multicolored spell, create a 4/4 red and white Golem artifact creature token. */

  int result;
  token_generation_t token;

  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      result = count_colors(trigger_cause_controller, trigger_cause);
      if (1 < result)
      {
        sp_ability_trigger(player, event, 2, TYPE_ANY, NULL, player, card);
      }
    }
  }
  if (event != EVENT_SPA)
  {
    misc_hexproof(player, card, event);
    if (((event == EVENT_PROT_MISC) && (card == affected_card)) &&
        (player == affected_card_controller))
    {
      result = count_colors(attacking_card_controller, attacking_card);
      if (result == 1)
      {
        event_result = event_result | 1;
      }
    }
    return 0;
  }
  default_token_definition(player, card, CARD_ID_GOLEM, &token);
  token.tou = 4;
  token.pow = 4;
  token.color_forced = '0';
  generate_token(&token);
  misc_hexproof(player, card, EVENT_SPA);
  return 0;
}

/* ManalinkEh 2022: 0x22361d0; original cards/modern_horizons_2.c:3234. */
int card_geyadrone_dihada(int player, int card, event_t event)
{
  /* Geyadrone Dihada	|1|U|B|R
   * Legendary Planeswalker - Dihada (4)
   * Protection from permanents with corruption counters on them
   * +1: Each opponent loses 2 life and you gain 2 life. Put a corruption counter on up to one other target creature or planeswalker.
   * -3: Gain control of target creature or planeswalker until end of turn. Untap it and put a corruption counter on it. It gains haste until end of turn.
   * -7: Gain control of each permanent with a corruption counter on it. */

  enum
  {
    CHOICE_OPP_LOSES_2_LIFE = 1,
    CHOICE_STEAL_PERMANENT_UNTIL_EOT = 2,
    CHOICE_STEAL_ALL_PERMANENTS = 3
  };


  card_instance_t *instance;
  int value;
  int choice;
  target_definition_t td;

  target_definition_t td2;
  int p;
  int c;
  int marked[151][2];
  check_legend_rule(player, card, event);
  misc_protection(player, card, event);
  if (event == EVENT_PROT_MISC)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      if (((unsigned int)event_result >> 24 & 0xff) == '\x06')
      {
        event_result = event_result | 1;
        return 0;
      }
      get_protection_source('\0', &p, &c);
      if ((((-1 < p) && (-1 < c)) && (value = is_what(p, c, 0x47), value != 0)) &&
          (value = count_counters(p, c, COUNTER_CORRUPTION), value != 0))
      {
        event_result = event_result | 1;
      }
    }
  }
  else if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0x102);
    td.special = TARGET_SPECIAL_NOT_ME;
    default_target_definition(player, card, &td2, 0x102);
    value = can_target(&td2);
    value = dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, DLG_MODE,
                        "Drain 2 and corrupt", 1, 0x14 - life[player], 1, "Borrow target", value,
                        10, 0xfffffffd, "Steal all corrupted", 1, 0x1e, 0xfffffff9, 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (value == 0)
      {
        return 0;
      }
    }
    else
    {
      instance->number_of_targets = '\0';
      if (value == CHOICE_OPP_LOSES_2_LIFE)
      {
        td.allow_cancel = 2;
        new_pick_target(&td, "Select another target creature or planeswalker.", 0, 0x80);
      }
      else if (value == CHOICE_STEAL_PERMANENT_UNTIL_EOT)
      {
        new_pick_target(&td2, "Select target creature or planeswalker.", 0, 0x81);
      }
    }
  }
  else if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0x102);
    td.special = TARGET_SPECIAL_NOT_ME;
    default_target_definition(player, card, &td2, 0x102);
    value = can_target(&td2);
    choice = dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, DLG_MODE,
                         "Drain 2 and corrupt", 1, 0x14 - life[player], 1, "Borrow target", value,
                         10, 0xfffffffd, "Steal all corrupted", 1, 0x1e, 0xfffffff9, 0, 0, 0, 0);
    value = current_turn;
    if (choice == CHOICE_OPP_LOSES_2_LIFE)
    {
      lose_life(1 - player, 2);
      gain_life(player, 2);
      if (validate_target(player, card, &td, 0) != 0)
      {
        add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                     COUNTER_CORRUPTION, 1);
      }
    }
    else if (choice == CHOICE_STEAL_PERMANENT_UNTIL_EOT)
    {
      if (validate_target(player, card, &td2, 0) != 0)
      {
        add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                     COUNTER_CORRUPTION, 1);
        effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
      }
    }
    else if (choice == CHOICE_STEAL_ALL_PERMANENTS)
    {
      int marked_count = 0;
      int scan_player = value;
      for (int scan_card = active_cards_count[current_turn] - 1; scan_card >= 0; --scan_card)
      {
        instance = in_play(scan_player, scan_card);
        if (instance && is_what(scan_player, scan_card, 0x47) &&
            count_counters(scan_player, scan_card, COUNTER_CORRUPTION))
        {
          marked[marked_count][0] = scan_player;
          marked[marked_count][1] = scan_card;
          ++marked_count;
        }
      }
      scan_player = 1 - current_turn;
      for (int scan_card = active_cards_count[scan_player] - 1; scan_card >= 0; --scan_card)
      {
        instance = in_play(scan_player, scan_card);
        if (instance && is_what(scan_player, scan_card, 0x47) &&
            count_counters(scan_player, scan_card, COUNTER_CORRUPTION))
        {
          marked[marked_count][0] = scan_player;
          marked[marked_count][1] = scan_card;
          ++marked_count;
        }
      }
      for (int index = marked_count - 1; index >= 0; --index)
        gain_control_permanently(player, marked[index][0], marked[index][1]);
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x2236880; original cards/modern_horizons_2.c:3331. */
int card_ghost_lit_drifter(int player, int card, event_t event)
{
  /* Ghost-Lit Drifter	|2|U
   * Creature - Spirit 2/2
   * Flying
   * |2|U: Another target creature gains flying until end of turn.
   * Channel - |X|U, Discard this card: X target creatures gain flying until end of turn. */

  unsigned int saved_state;
  card_instance_t *instance;
  int targets_or_result;
  int card_00;
  card_instance_t *activation;
  target_definition_t td;

  if (event == EVENT_CAN_ACTIVATE_FROM_HAND)
    return has_mana_multi(player, 0, 0, 1, 0, 0, 0) != 0;
  if (event == EVENT_ACTIVATE_FROM_HAND)
  {
    charge_mana_multi(player, -1, 0, 1, 0, 0, 0);
    if (spell_fizzled != 1)
    {
      instance = get_card_instance(player, card);
      instance->targets[0].player = x_value;
      discard_card(player, card);
      return 1;
    }
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION_FROM_HAND)
  {

    if (spell_fizzled == 1)
    {
      return 0;
    }
    card_00 = add_card_to_hand(player, get_internal_card_id_from_csv_id(CARD_ID_GHOST_LIT_DRIFTER));
    instance = get_card_instance(player, card_00);
    saved_state = instance->state;
    hand_count[player] = hand_count[player] + -1;
    instance->state = saved_state | 0x20;
    default_target_definition(player, card_00, &td, 2);
    td.preferred_controller = player;
    activation = get_card_instance(player, card);
    targets_or_result = activation->targets[0].player;
    while ((targets_or_result != 0 && (new_pick_target(&td, "TARGET_CREATURE", 0, 0) != 0)))
    {
      pump_ability_until_eot(player, card_00, instance->targets[0].player,
                             instance->targets[0].card, 0, 0, 0x20, 0);
      targets_or_result = targets_or_result + -1;
      instance->number_of_targets = '\0';
    }
    obliterate_card(player, card_00);
    return 0;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  td.special = TARGET_SPECIAL_NOT_ME;
  instance = get_card_instance(player, card);
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (targets_or_result = valid_target(&td), targets_or_result != 0))
  {
    pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0,
                           0, 0x20, 0);
  }
  return generic_activated_ability(player, card, event, 4, 2, 0, 1, 0, 0, 0, 0, &td,
                                   "Select another target creature.");
}

/* ManalinkEh 2022: 0x2236bc0; original cards/modern_horizons_2.c:3398. */
int card_gilt_blade_prowler(int player, int card, event_t event)
{
  /* Gilt-Blade Prowler	|2|B
   * Creature - Human Rogue 2/3
   * |1, |T, Pay 1 life: Draw a card. Activate only if you've discarded a card this turn. */

  int result;
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = get_trap_condition(player, 0x203);
    if (result == 0)
    {
      return 0;
    }
    return generic_activated_ability(player, card, event, 1, 1, 0, 0, 0, 0, 0, 1, NULL, NULL);
  }

  if (event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
    draw_cards(player, 1);
  return generic_activated_ability(player, card, event, 1, 1, 0, 0, 0, 0, 0, 1, NULL, NULL);
}

/* ManalinkEh 2022: 0x2236cb0; original cards/modern_horizons_2.c:3420. */
int card_glimmer_bairn(int player, int card, event_t event)
{
  /* Glimmer Bairn	|G
   * Creature - Ouphe 1/2
   * Sacrifice a token: This creature gets +2/+2 until end of turn. */

  card_instance_t *instance;
  int result;
  test_definition_t test;

  if (event == EVENT_ACTIVATE)
  {
    new_default_test_definition(&test, 0x47, "Select a token to sacrifice.");
    test.type_flag = 6;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    new_default_test_definition(&test, 0x47, "Select a token to sacrifice.");
    test.type_flag = 6;
    if (event == EVENT_CHECK_PUMP)
    {
      result = generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE,
                                                                GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL,
                                                                NULL, TYPE_NONE, &test, SARV_0);
      if (result != 0)
      {
        pumpable_power[player] = pumpable_power[player] + 2;
        pumpable_toughness[player] = pumpable_toughness[player] + 2;
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                    instance->parent_controller, instance->parent_card, 2, 2);
    }
  }
  result = generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test, SARV_0);
  return result;
}

/* ManalinkEh 2022: 0x2236ec0; original cards/modern_horizons_2.c:3449. */
int card_glimpse_of_tomorrow(int player, int card, event_t event)
{
  /* Glimpse of Tomorrow	""
   * Sorcery
   * Suspend 3-|R|R
   * Shuffle all permanents you own into your library, then reveal that many cards from the top of your library. Put all non-Aura permanent cards revealed this way onto the battlefield, then do the same for Aura cards, then put the rest on the bottom of your library in a random order. */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    warp_world_effect(player, card, player, 0xc);
    kill_card(player, card, KILL_DESTROY);
  }
  result = suspend_only(player, card, event, 3, 0, 0, 0, 0, 2, 0);
  return result;
}

/* ManalinkEh 2022: 0x2237020; original cards/modern_horizons_2.c:3481. */
int card_glorious_enforcer(int player, int card, event_t event)
{
  /* Glorious Enforcer	|5|W|W
   * Creature - Angel 5/5
   * Flying, lifelink
   * At the beginning of each combat, if you have more life than an opponent, this creature gains double strike until end of turn. */

  lifelink(player, card, event);
  if (beginning_of_combat(player, card, event, RESOLVE_TRIGGER_MANDATORY) &&
      life[1 - player] < life[player])
    pump_ability_until_eot_no_repeat(player, card, player, card, KEYWORD_DOUBLE_STRIKE, 0);
  return 0;
}

/* ManalinkEh 2022: 0x22370b0; original cards/modern_horizons_2.c:3496. */
int card_goblin_anarchomancer(int player, int card, event_t event)
{
  /* Goblin Anarchomancer	|R|G
   * Creature - Goblin Shaman 2/2
   * Each spell you cast that's red or green costs |1 less to cast. */

  card_instance_t *instance;
  unsigned int spell_color;
  unsigned int sleighted_colors;

  if ((event == EVENT_MODIFY_COST_GLOBAL) && (affected_card_controller == player))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      if (!is_humiliated(player, card, AL_STATIC))
      {
        spell_color = get_color(affected_card_controller, affected_card);
        sleighted_colors = get_sleighted_color_test(player, card, 0x18);
        if ((spell_color & sleighted_colors) != 0)
        {
          COST_COLORLESS = COST_COLORLESS + -1;
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2237150; original cards/modern_horizons_2.c:3510. */
int card_goblin_traprunner(int player, int card, event_t event)
{
  /* Goblin Traprunner	|3|R
   * Creature - Goblin 4/2
   * Whenever this creature attacks, flip three coins. For each flip you win, create a 1/1 red Goblin creature token that's tapped and attacking. */

  int coin_result;
  int i;
  int wins;
  token_generation_t token;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    i = 3;

    wins = 0;
    do
    {
      coin_result = flip_a_coin(player, card);
      wins = wins + coin_result;
      i = i + -1;
    } while (i != 0);
    if (wins != 0)
    {
      default_token_definition(player, card, CARD_ID_GOBLIN, &token);
      token.qty = (unsigned char)wins;
      token.tou = 1;
      token.pow = 1;
      token.color_forced = '\x10';
      token.action = TOKEN_ACTION_ATTACKING;
      generate_token(&token);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2237210; original cards/modern_horizons_2.c:3545. */
int card_gouged_zealot(int player, int card, event_t event)
{
  /* Gouged Zealot	|3|R
   * Creature - Cyclops Berserker 4/3
   * Reach
   * Delirium - Whenever this creature attacks, if there are four or more card types among cards in your graveyard, this creature deals 1 damage to each creature defending player controls. */

  xtrigger_t trigger;

  trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    trigger = xtrigger_impl_value_dont_use_directly;
  }
  if (((event == EVENT_DECLARE_ATTACKERS) || (trigger == XTRIGGER_ATTACKING)) &&
      (delirium(player) != 0))
  {
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  }
  if (event == EVENT_DAT_ABILITY)
  {
    new_damage_all(player, card, 1 - player, 1, 1, NULL);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22372d0; original cards/modern_horizons_2.c:3564. */
int card_graceful_restoration(int player, int card, event_t event)
{
  /* Graceful Restoration	|3|W|B
   * Sorcery
   * Choose one -
   * * Return target creature card from your graveyard to the battlefield with an additional +1/+1 counter on it.
   * * Return up to two target creature cards with power 2 or less from your graveyard to the battlefield. */

  enum
  {
    CHOICE_REANIMATE_P1_P1 = 1,
    CHOICE_REANIMATE_2 = 2
  };

  int i;
  int choice;
  uint8_t selected_mode;
  card_instance_t *instance;
  int grave_index_or_available;
  unsigned int one_available;
  int result_or_available;
  test_definition_t this_test;
  test_definition_t this_test2;

  if (event == EVENT_CAN_CAST)
  {
    new_default_test_definition(&this_test, 2, "Select target creature card.");
    this_test.ai_selection_mode = 2;
    new_default_test_definition(&this_test2, 2,
                                "Select target creature card with power 2 or less.");
    this_test2.power = 3;
    this_test2.power_flag = 3;
    this_test2.ai_selection_mode = 1;
    get_card_instance(player, card);
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      new_default_test_definition(&this_test, 2, "Select target creature card.");
      this_test.ai_selection_mode = 2;
      new_default_test_definition(&this_test2, 2,
                                  "Select target creature card with power 2 or less.");
      this_test2.power = 3;
      this_test2.power_flag = 3;
      this_test2.ai_selection_mode = 1;
      get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {

        result_or_available = new_special_count_grave(player, &this_test2);
        grave_index_or_available = count_graveyard_by_type(player, 2);
        one_available = 0;
        if (grave_index_or_available != 0)
        {
          grave_index_or_available = graveyard_has_shroud(player);
          one_available = (unsigned int)(grave_index_or_available == 0);
        }
        choice = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_MODE, DLG_AUTOCHOOSE_IF_1,
                             "Reanimate one", one_available, 10, "Reanimate two",
                             result_or_available, 0x14, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          result_or_available = 0;
        }
        else if (choice == CHOICE_REANIMATE_P1_P1)
        {
          result_or_available =
              generic_spell(player, card, EVENT_CAST_SPELL, 8, NULL, NULL, 0, &this_test);
        }
        else
        {
          if (choice != CHOICE_REANIMATE_2)
          {
            return 0;
          }
          result_or_available =
              generic_spell(player, card, EVENT_CAST_SPELL, 0x28, NULL, NULL, 2, &this_test2);
        }
        return result_or_available;
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    new_default_test_definition(&this_test, 2, "Select target creature card.");
    this_test.ai_selection_mode = 2;
    new_default_test_definition(&this_test2, 2,
                                "Select target creature card with power 2 or less.");
    this_test2.power = 3;
    this_test2.power_flag = 3;
    this_test2.ai_selection_mode = 1;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      selected_mode = instance->mode;
      if (selected_mode == '\x01')
      {

        result_or_available = validate_target_from_grave_source(player, card, player, 0);
        if (result_or_available != -1)
        {
          reanimate_permanent(player, card, player, result_or_available,
                              REANIMATE_PLUS1_PLUS1_COUNTER);
        }
        selected_mode = instance->mode;
      }
      if (selected_mode == '\x02')
      {

        for (i = 0; instance = get_card_instance(player, card), i < instance->info_slot; i = i + 1)
        {

          grave_index_or_available = validate_target_from_grave(player, card, player, i);
          if (grave_index_or_available != -1)
          {
            reanimate_permanent(player, card, player, grave_index_or_available, REANIMATE_DEFAULT);
          }
        }
      }
      kill_card(player, card, KILL_SACRIFICE);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    new_default_test_definition(&this_test, 2, "Select target creature card.");
    this_test.ai_selection_mode = 2;
    new_default_test_definition(&this_test2, 2,
                                "Select target creature card with power 2 or less.");
    this_test2.power = 3;
    this_test2.power_flag = 3;
    this_test2.ai_selection_mode = 1;
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2237770; original cards/modern_horizons_2.c:3633. */
int card_grief2(int player, int card, event_t event)
{
  /* Grief	|2|B|B
   * Creature - Elemental Incarnation 3/2
   * Menace
   * When this creature enters, target opponent reveals their hand. You choose a nonland card from it. That player discards that card.
   * Evoke-Exile a black card from your hand. */

  int result;
  card_instance_t *instance;
  int selected;
  target_definition_t td;
  test_definition_t test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_PLAYER", 0, 0);
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        result = instance->targets[0].player;
        new_default_test_definition(&test, 1, "Select a nonland card.");
        test.type_flag = 1;
        selected = new_select_a_card(player, result, 4, 1, 1, -1, &test);
        if (selected != -1)
        {
          new_discard_card(result, selected, player, 2);
        }
      }
    }
    result = evoked(player, card);
    if (result != 0)
    {
      kill_card(player, card, KILL_SACRIFICE);
    }
  }
  result = evoke_with_cards_in_hand(player, card, event, 2);
  return result;
}

/* ManalinkEh 2022: 0x22378f0; original cards/modern_horizons_2.c:3666. */
int card_grist_the_hunger_tide(int player, int card, event_t event)
{
  /* Grist, the Hunger Tide	|1|B|G
   * Legendary Planeswalker - Grist (3)
   * As long as Grist isn't on the battlefield, it's a 1/1 Insect creature in addition to its other types.
   * +1: Create a 1/1 black and green Insect creature token, then mill a card. If an Insect card was milled this way, put a loyalty counter on Grist and repeat this process.
   * -2: You may sacrifice a creature. When you do, destroy target creature or planeswalker.
   * -5: Each opponent loses life equal to the number of creature cards in your graveyard. */

  enum
  {
    CHOICE_INSECT = 1,
    CHOICE_SAC_AND_KILL = 2,
    CHOICE_OPP_LOSE_LIFE = 3
  };

  int choice_or_result;
  card_instance_t *instance;
  int grave_creatures;
  unsigned int sacrificed;
  card_instance_t *inst;
  unsigned int player_00;
  int opponent;
  inst_info_t ability_record;
  uint32_t ultimate_priority;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0x102);
    choice_or_result = life[1 - player];
    grave_creatures = count_graveyard_by_type(1 - player, 2);
    if (choice_or_result - grave_creatures < 6)
    {
      choice_or_result = can_target(&td);
      ultimate_priority = 100;
    }
    else
    {
      choice_or_result = can_target(&td);
      ultimate_priority = 10;
    }
    choice_or_result = dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, DLG_MODE,
                                   "Insect and mill a card", 1, 10, 1, "Destroy creature or PW",
                                   choice_or_result, 0x14, 0xfffffffe, "Opponent loses life", 1,
                                   ultimate_priority, 0xfffffffb, 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (choice_or_result == 0)
      {
        return 0;
      }
    }
    else
    {
      instance->number_of_targets = '\0';
      if (choice_or_result == CHOICE_SAC_AND_KILL)
      {
        test_definition_t this_test;
        new_default_test_definition(&this_test, 2, "Select a creature to sacrifice.");
        sacrificed = new_sacrifice(player, card, player,
                                   SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &this_test);
        if (sacrificed != 0)
        {
          choice_or_result =
              new_pick_target(&td, "Select target creature or planeswalker.", 0, 0x81);
          player_00 = (int)sacrificed >> 0x10 & 0xff;
          if (choice_or_result == 0)
          {
            state_untargettable(player_00, sacrificed >> 0x18, 0);
          }
          else
          {
            kill_card(player_00, sacrificed >> 0x18, KILL_SACRIFICE);
          }
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0x102);
    opponent = 1 - player;
    choice_or_result = life[opponent];
    grave_creatures = count_graveyard_by_type(opponent, 2);
    if (choice_or_result - grave_creatures < 6)
    {
      choice_or_result = can_target(&td);
      ultimate_priority = 100;
    }
    else
    {
      choice_or_result = can_target(&td);
      ultimate_priority = 10;
    }
    choice_or_result = dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, DLG_MODE,
                                   "Insect and mill a card", 1, 10, 1, "Destroy creature or PW",
                                   choice_or_result, 0x14, 0xfffffffe, "Opponent loses life", 1,
                                   ultimate_priority, 0xfffffffb, 0, 0, 0, 0);
    if (choice_or_result == CHOICE_INSECT)
    {
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_INSECT, &token);
      token.color_forced = '\n';
      generate_token(&token);
      while (true)
      {
        inst = get_card_instance(instance->parent_controller, instance->parent_card);
        inst->info_slot = 0;
        special_mill(instance->parent_controller, instance->parent_card, 0x5a13, player, 1);
        ability_record = inst_info(inst, 0x5a13);
        if (ability_record.data2 != 0x42)
          break;
        add_counters(player, card, instance->parent_controller, instance->parent_card,
                     COUNTER_LOYALTY, 1);
      }
    }
    else if (choice_or_result == CHOICE_SAC_AND_KILL)
    {
      if (validate_target(player, card, &td, 0) != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
    }
    else if (choice_or_result == CHOICE_OPP_LOSE_LIFE)
    {
      lose_life(opponent, count_graveyard_by_type(player, 2));
    }
  }
  return planeswalker(player, card, event, 3);
}

/* ManalinkEh 2022: 0x2237ea0; original cards/modern_horizons_2.c:3749. */
int card_hard_evidence(int player, int card, event_t event)
{
  /* Hard Evidence	|U
   * Sorcery
   * Create a 0/3 blue Crab creature token.
   * Investigate. (Create a Clue token. It's an artifact with "|2, Sacrifice this token: Draw a card.") */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    generate_token_by_id(player, card, CARD_ID_CRAB);
    investigate(player, card, player);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2237f10; original cards/modern_horizons_2.c:3778. */
int card_hell_mongrel(int player, int card, event_t event)
{
  /* Hell Mongrel	|3|B
   * Creature - Nightmare Dog 4/3
   * Discard a card: This creature gets +1/+1 until end of turn.
   * Madness |2|B (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

  int result;

  if (1 < (uint32_t)(event - EVENT_CAN_PAY_MADNESS_COST))
  {
    result = card_noose_constrictor(player, card, event);
    return result;
  }
  return madness(player, card, event, 2, 1, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x2237f80; original cards/modern_horizons_2.c:3793. */
int card_herd_baloth(int player, int card, event_t event)
{
  /* Herd Baloth	|3|G|G
   * Creature - Beast 4/4
   * Whenever one or more +1/+1 counters are put on this creature, you may create a 4/4 green Beast creature token. */

  int result;
  token_generation_t token;

  result = one_or_more_p1_p1_counters_are_putted_on_me(player, card, event);
  if (result == 0)
  {
    return 0;
  }
  default_token_definition(player, card, CARD_ID_BEAST, &token);
  token.tou = 4;
  token.pow = 4;
  generate_token(&token);
  return 0;
}

/* ManalinkEh 2022: 0x2237ff0; original cards/modern_horizons_2.c:3814. */
int card_inevitable_betrayal(int player, int card, event_t event)
{
  /* Inevitable Betrayal	""
   * Sorcery
   * Suspend 3-|1|U|U (Rather than cast this card from your hand, pay |1|U|U and exile it with three time counters on it. At the beginning of your upkeep, remove a time counter. When the last is removed, you may cast it without paying its mana cost.)
   * Search target opponent's library for a creature card and put that card onto the battlefield under your control. Then that player shuffles. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {

    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    if (instance->info_slot == 1)
    {
      result = basic_spell(player, card, EVENT_CAN_CAST);
      return result;
    }
    result = generic_spell(player, card, EVENT_CAN_CAST, 0x40, &td, NULL, 1, NULL);
    return result;
  }
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
    result = played_for_free(player, card);
    if ((result != 0) || (result = is_token(player, card), result != 0))
    {
      default_target_definition(player, card, &td, 0);
      td.zone = TARGET_ZONE_PLAYERS;
      result = generic_spell(player, card, EVENT_CAST_SPELL, 0x40, &td, NULL, 1, NULL);
      return result;
    }
  }
  else
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = card_bribery(player, card, EVENT_RESOLVE_SPELL);
      return result;
    }
    if (((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET)) &&
        (event != EVENT_MODIFY_COST))
    {
      return 0;
    }
  }
  result = suspend_only(player, card, event, 3, 1, 0, 2, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22381f0; original cards/modern_horizons_2.c:3858. */
int card_jewel_eyed_cobra(int player, int card, event_t event)
{
  /* Jewel-Eyed Cobra	|2|G
   * Creature - Snake 3/1
   * Deathtouch
   * When this creature dies, create a Treasure token. (It's an artifact with "|T, Sacrifice this token: Add one mana of any color.") */

  int result;
  deathtouch(player, card, event);
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  generate_token_by_id(player, card, CARD_ID_TREASURE);
  return 0;
}

/* ManalinkEh 2022: 0x2238260; original cards/modern_horizons_2.c:3874. */
int card_junk_winder(int player, int card, event_t event)
{
  /* Junk Winder	|5|U|U
   * Creature - Serpent 5/6
   * Affinity for tokens (This spell costs |1 less to cast for each token you control.)
   * Whenever a token you control enters, tap target nonland permanent an opponent controls. It doesn't untap during its controller's next untap step. */

  if (event == EVENT_MODIFY_COST)
  {
    if (card == affected_card && player == affected_card_controller)
    {
      test_definition_t test;
      default_test_definition(&test, 0x47);
      test.type_flag = 6;
      int tokens = check_battlefield_for_special_card(player, card, player, 4, &test);
      COST_COLORLESS -= (char)tokens;
    }
    if (trigger_condition != TRIGGER_COMES_INTO_PLAY)
      return 0;
  }
  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && card == affected_card && affected_card_controller == player)
  {
    test_definition_t test;
    default_test_definition(&test, 0x47);
    test.type_flag = 6;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x47);
    td.allowed_controller = 1 - player;
    td.illegal_type = TYPE_LAND;
    card_instance_t *instance = get_card_instance(player, card);
    if (can_target(&td) &&
        new_pick_target(&td, "Select target nonland permanent an opponent controls.", 0, 0x80))
    {
      effect_frost_titan(player, card, instance->targets[0].player, instance->targets[0].card);
      instance->number_of_targets = 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2238460; original cards/modern_horizons_2.c:3910. */
int card_kaldra_compleat(int player, int card, event_t event)
{
  /* Kaldra Compleat	|7
   * Legendary Artifact - Equipment
   * Living weapon
   * Indestructible
   * Equipped creature gets +5/+5 and has first strike, trample, indestructible, haste, and "Whenever this creature deals combat damage to a creature, exile that creature."
   * Equip |7 */

  card_instance_t *instance;
  int equipped_card;
  int target_player;

  check_legend_rule(player, card, event);
  indestructible(player, card, event);
  instance = is_equipping(player, card);
  if (instance != NULL)
  {

    instance = get_card_instance(player, card);
    target_player = (int)instance->damage_target_player;
    equipped_card = instance->damage_target_card;
    rfg_when_damage(target_player, equipped_card, event);
    if (((event == EVENT_CHANGE_TYPE) && (equipped_card == affected_card)) &&
        (target_player == affected_card_controller))
    {
      if (is_humiliated(player, card, AL_TRIG) == 0)
      {
        instance = get_card_instance(target_player, equipped_card);
        instance->destroys_if_blocked = instance->destroys_if_blocked | 0xc;
      }
    }
    return vanilla_equipment(player, card, event, 7, 5, 5, 0x180, 0x804);
  }
  return living_weapon(player, card, event, 7);
}

/* ManalinkEh 2022: 0x2238590; original cards/modern_horizons_2.c:3940. */
int card_kaleidoscorch(int player, int card, event_t event)
{
  /* Kaleidoscorch	|1|R
   * Sorcery
   * Converge - ~ deals X damage to any target, where X is the number of colors of mana spent to cast this spell.
   * Flashback |4|R (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

  card_instance_t *instance;
  target_definition_t td;

  store_num_colors_paid_in_eot_toughness0(player, card, event);

  bool counted_colors = trigger_condition == TRIGGER_SPELL_CAST && reason_for_trigger_controller == player &&
                        trigger_cause_controller == player && trigger_cause == card &&
                        event == EVENT_TRIGGER;
  if (counted_colors)
  {
    unsigned char colors = 0;
    for (int color = 1; color != 6; ++color)
      if (mana_paid[color] > 0)
        ++colors;
    instance = get_card_instance(player, card);
    instance->eot_toughness0 = colors;
  }
  else if (event == EVENT_CAN_CAST || event == EVENT_CHANGE_TARGET ||
           (event == EVENT_CAST_SPELL && card == affected_card &&
            player == affected_card_controller) ||
           event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    if (event == EVENT_RESOLVE_SPELL)
    {
      if (valid_target(&td))
      {
        instance = get_card_instance(player, card);
        damage_target0(player, card, (unsigned int)instance->eot_toughness0);
      }
      int flashback = get_flashback(player, card);
      kill_card(player, card, flashback ? KILL_EXILE : KILL_DESTROY);
    }
    return generic_spell(player, card, event, 1, &td, "TARGET_ANY", 1, NULL);
  }
  return do_flashback(player, card, event, 4, 0, 0, 0, 1, 0);
}

/* ManalinkEh 2022: 0x22387d0; original cards/modern_horizons_2.c:3978. */
int card_kitchen_imp(int player, int card, event_t event)
{
  /* Kitchen Imp	|3|B
   * Creature - Imp 2/2
   * Flying, haste
   * Madness |B (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

  haste(player, card, event);
  if (1 < (uint32_t)(event - EVENT_CAN_PAY_MADNESS_COST))
  {
    return 0;
  }
  return madness(player, card, event, 0, 1, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x2238850; original cards/modern_horizons_2.c:3994. */
int card_knighted_myr(int player, int card, event_t event)
{
  /* Knighted Myr	|2|W
   * Artifact Creature - Myr Knight 2/2
   * |2|W: Adapt 1. (If this creature has no +1/+1 counters on it, put a +1/+1 counter on it.)
   * Whenever one or more +1/+1 counters are put on this creature, it gains double strike until end of turn. */

  int result;

  result = one_or_more_p1_p1_counters_are_putted_on_me(player, card, event);
  if (result != 0)
  {
    pump_ability_until_eot_no_repeat(player, card, player, card, 0x40000000, 0);
  }
  result = adapt(player, card, event, 1, 2, 0, 0, 0, 0, 1);
  return result;
}

/* ManalinkEh 2022: 0x22388f0; original cards/modern_horizons_2.c:4008. */
int card_landscaper_colos(int player, int card, event_t event)
{
  /* Landscaper Colos	|5|W
   * Creature - Goat Beast 4/6
   * When this creature enters, put target card from an opponent's graveyard on the bottom of their library.
   * Basic landcycling |1|W (|1|W, Discard this card: Search your library for a basic land card, reveal it, put it into your hand, then shuffle.) */

  if (IS_ACTIVATING_FROM_HAND(event))
    return basic_landcycling(player, card, event, 1, 0, 0, 0, 0, 1);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB && count_graveyard(1 - player) && !graveyard_has_shroud(player))
  {
    test_definition_t this_test;
    new_default_test_definition(&this_test, 0xff,
                                "Select a card to put on the bottom of opponent\'s library.");
    new_global_tutor(player, 1 - player, 2, 10, 1, 1, &this_test);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2238a10; original cards/modern_horizons_2.c:4030. */
int card_late_to_dinner(int player, int card, event_t event)
{
  /* Late to Dinner	|3|W
   * Sorcery
   * Return target creature card from your graveyard to the battlefield. Create a Food token. (It's an artifact with "|2, |T, Sacrifice this token: You gain 3 life.") */

  int result;
  test_definition_t this_test;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  new_default_test_definition(&this_test, 2, "Select target creature card.");
  this_test.ai_selection_mode = 2;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = validate_target_from_grave_source(player, card, player, 0);
    if (result != -1)
    {
      reanimate_permanent(player, card, player, result, REANIMATE_DEFAULT);
      generate_token_by_id(player, card, CARD_ID_FOOD);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 8, NULL, NULL, 0, &this_test);
}

/* ManalinkEh 2022: 0x2238b80; original cards/modern_horizons_2.c:4055. */
int card_lazotep_chancellor(int player, int card, event_t event)
{
  /* Lazotep Chancellor	|U|B
   * Creature - Zombie Wizard 1/3
   * Whenever you discard a card, you may pay |1. If you do, amass Zombies 2. (Put two +1/+1 counters on an Army you control. It's also a Zombie. If you don't control an Army, create a 0/0 black Zombie Army creature token first.) */

  int result;

  if (trigger_condition != TRIGGER_DISCARD)
  {
    return 0;
  }
  if ((card == affected_card) && (player == affected_card_controller))
  {
    result = duh_mode(player);
    result = discard_trigger(player, card, event, player, (result != 0) + 1, 0);
    if (result != 0)
    {
      result = charge_mana_while_resolving(player, card, event, player, COLOR_COLORLESS, 1);
      if (result != 0)
      {
        amass(player, card, 2);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2238c50; original cards/modern_horizons_2.c:4069. */
int card_legion_vanguard(int player, int card, event_t event)
{
  /* Legion Vanguard	|1|B
   * Creature - Vampire Soldier 2/2
   * |1, Sacrifice another creature: This creature explores. (Reveal the top card of your library. Put that card into your hand if it's a land. Otherwise, put a +1/+1 counter on this creature, then put the card back or put it into your graveyard.) */

  int result;
  test_definition_t test;

  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    explore(player, card, EVENT_RESOLVE_ACTIVATION);
  }
  new_default_test_definition(&test, 2, "Select another creature to sacrifice.");
  test.not_me = 1;
  result = generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_NONE, 1, 0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test, SARV_0);
  return result;
}

/* ManalinkEh 2022: 0x2238d40; original cards/modern_horizons_2.c:4088. */
int card_lens_flare(int player, int card, event_t event)
{
  /* Lens Flare	|4|W
   * Instant
   * Affinity for artifacts (This spell costs |1 less to cast for each artifact you control.)
   * ~ deals 5 damage to target attacking or blocking creature. */

  int result;

  affinity(player, card, event, 0x40, -1);
  result = card_sandblast(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2238d90; original cards/modern_horizons_2.c:4098. */
int card_lightning_spear(int player, int card, event_t event)
{
  /* Lightning Spear	|1|R
   * Artifact - Equipment
   * Equipped creature gets +1/+0 and has trample.
   * |2|R, Sacrifice this Equipment: It deals 3 damage to any target.
   * Equip |1 */

  enum
  {
    CHOICE_EQUIP = 1,
    CHOICE_SAC_DAMAGE = 2
  };

  int choice;
  card_instance_t *instance;
  int equip_available;
  target_definition_t *target;
  target_definition_t td;

  attached_gets_pt(player, card, event, 1, 0);
  attached_gets_kw(player, card, event, KEYWORD_TRAMPLE, SP_KEYWORD_0);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {

      equip_available = basic_equipment(player, card, EVENT_CAN_ACTIVATE, 1);
      if (equip_available == 0)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xc, 2, 0, 0, 0, 1, 0, 0,
                                         &td, NULL);
      }
      return equip_available;
    }
    if (event == EVENT_ACTIVATE)
    {

      equip_available = can_activate_basic_equipment(player, card, EVENT_ACTIVATE, 1);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_OMIT_ILLEGAL, DLG_AUTOCHOOSE_IF_1,
                           DLG_MODE, "Equip", equip_available, 5, "Sac & damage",
                           generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xc, 2, 0, 0,
                                                     0, 1, 0, 0, &td, NULL),
                           10, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice != CHOICE_EQUIP)
      {
        if (choice != CHOICE_SAC_DAMAGE)
        {
          return 0;
        }
        generic_activated_ability(player, card, EVENT_ACTIVATE, 0xc, 2, 0, 0, 0, 1, 0, 0, &td,
                                  "TARGET_ANY");
        return 0;
      }
      return basic_equipment(player, card, EVENT_ACTIVATE, 1);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    instance = get_card_instance(player, card);
  }
  target = &td;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->mode == '\x01')
    {
      return basic_equipment(player, card, EVENT_RESOLVE_ACTIVATION, 1);
    }
    if (instance->mode == '\x02')
    {
      td.illegal_abilities = instance->targets[1].player;
      if (valid_target(target) != 0)
      {
        damage_target0(player, card, 3);
        return 0;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2239140; original cards/modern_horizons_2.c:4165. */
int card_liquimetal_torque(int player, int card, event_t event)
{
  /* Liquimetal Torque	|2
   * Artifact
   * |T: Add |C.
   * |T: Target nonland permanent becomes an artifact in addition to its other types until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      default_target_definition(player, card, &td, 0x47);
      td.illegal_type = TYPE_LAND;
      td.preferred_controller = player;
      instance = get_card_instance(player, card);

      instance->number_of_targets = '\0';
      instance->mode = '\0';
      result = paying_mana();
      if (((result != 0) || (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5,
                                                                2, 0, 0, 0, 0, 0, 0, &td, NULL),
                             result == 0)) ||
          (result = do_dialog(player, player, card, -1, -1,
                              " Produce mana\n Turn nonland permanent into artifact\n Cancel", 1),
           result == 0))
      {
        result = mana_producer(player, card, EVENT_ACTIVATE);
        return result;
      }
      if (result == 1)
      {
        add_state(player, card, 0x10);
        generic_activated_ability(player, card, EVENT_ACTIVATE, 7, 1, 0, 0, 0, 0, 0, 0, &td,
                                  "Select target nonland permanent.");
        if (spell_fizzled != 1)
        {
          result = is_what(instance->targets[0].player, instance->targets[0].card, 0x40);
          if (result != 0)
          {
            ai_modifier = ai_modifier + -100;
          }
          instance->mode = '\x01';
          return 0;
        }
        remove_state(player, card, 0x10);
        return 0;
      }
      if (result != 2)
      {
        return 0;
      }
      spell_fizzled = 1;
      return 0;
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      default_target_definition(player, card, &td, 0x47);
      td.illegal_type = TYPE_LAND;
      td.preferred_controller = player;
      instance = get_card_instance(player, card);
      if ((event == EVENT_RESOLVE_ACTIVATION) &&
          ((instance->mode == '\x01' && (result = valid_target(&td), result != 0))))
      {
        turn_into_artifact(player, card, instance->targets[0].player, instance->targets[0].card, 1);
        return 0;
      }
      return 0;
    }
  }
  result = mana_producer(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2239520; original cards/modern_horizons_2.c:4243. */
int card_lonis_cryptozoologist(int player, int card, event_t event)
{
  /* Lonis, Cryptozoologist	|G|U
   * Legendary Creature - Snake Elf Scout 1/2
   * Whenever another nontoken creature you control enters, investigate.
   * |T, Sacrifice X Clues: Target opponent reveals the top X cards of their library. You may put a nonland permanent card with mana value X or less from among them onto the battlefield under your control. That player puts the rest on the bottom of their library in a random order. */

  enum
  {
    SARV_REPORT_COUNT = 8
  };

  card_instance_t *instance;
  uint32_t num_cards;
  inst_info_t clue_record;
  target_definition_t td;
  char buffer[100];
  test_definition_t test2;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((trigger_cause_controller != player || (trigger_cause != card))))
  {
    default_test_definition(&test, 2);
    test.type_flag = 3;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    investigate(player, card, player);
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      new_default_test_definition(&test2, 0x47, "Select a Clue to sacrifice,");
      test2.subtype = 0x3004;
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        return generic_activated_ability_with_sacrifice_as_cost(
            player, card, EVENT_CAN_ACTIVATE, GAA_CAN_ONLY_TARGET_OPPONENT | GAA_UNTAPPED, 0, 0, 0,
            0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test2, SARV_REPORT_COUNT);
      }
      if (event == EVENT_ACTIVATE)
      {
        generic_activated_ability_with_sacrifice_as_cost(
            player, card, EVENT_ACTIVATE, GAA_CAN_ONLY_TARGET_OPPONENT | GAA_UNTAPPED, 0, 0, 0, 0,
            0, 0, 0, NULL, NULL, TYPE_NONE, &test2, SARV_REPORT_COUNT);
        set_inst_info(instance, 0x5a2b, instance->info_slot, 0);
        instance->info_slot = 0;
        return 0;
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      {
        return 0;
      }
      new_default_test_definition(&test2, 0x47, "Select a Clue to sacrifice,");
      test2.subtype = 0x3004;
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      default_target_definition(player, card, &td, 0);
      td.zone = TARGET_ZONE_PLAYERS;
      td.allowed_controller = 1 - player;
      if (valid_target(&td) != 0)
      {

        clue_record = inst_info(instance, 0x5a2b);
        num_cards = clue_record.data;
        clear_info(instance, 0x5a2b);
        scnprintf(buffer, 100, "Select a nonland permanent with mana value %d or less.", num_cards);
        new_default_test_definition(&test, 0x39, buffer);
        test.cmc = num_cards + 1;
        test.type_flag = 1;
        test.cmc_flag = 3;
        reveal_top_cards_of_library_and_choose(player, card, 1 - player, num_cards, 0, TUTOR_PLAY,
                                               0, TUTOR_BOTTOM_OF_DECK_RANDOM, 1, &test);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2239920; original cards/modern_horizons_2.c:4311. */
int card_lucid_dreams(int player, int card, event_t event)
{
  /* Lucid Dreams	|3|U|U
   * Sorcery
   * Draw X cards, where X is the number of card types among cards in your graveyard. */

  int result;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = count_types_in_grave(player);
    draw_cards(player, result);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x222abc0; original cards/modern_horizons_2.c:4325. */
static int magus_of_the_bridge_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;
  card_instance_t *dying;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (result = get_id((int)instance->damage_target_player, instance->damage_target_card),
           result != 0x5a2e))
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
      dying = in_play(affected_card_controller, affected_card);

      if ((dying != NULL) && (dying = get_card_instance(affected_card_controller, affected_card),
                              dying->kill_code != '\0'))
      {
        test_definition_t this_test;
        default_test_definition(&this_test, 2);
        this_test.type_flag = 3;

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
        }
        count_for_gfp_ability(player, card, 0x77, player, 0, &this_test);
      }
    }
  }
  if (!check_status(player, card, 0x10000))
  {

    result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (result != 0)
    {
      token_generation_t token;

      default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
      token.tou = 2;
      token.pow = 2;
      token.color_forced = '\x02';
      token.qty = (char)result;
      generate_token(&token);
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) && (result = check_status(player, card, 0x10000), result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2239980; original cards/modern_horizons_2.c:4387. */
int card_magus_of_the_bridge(int player, int card, event_t event)
{
  /* Magus of the Bridge	|B|B|B
   * Creature - Human Wizard 4/4
   * Whenever a nontoken creature is put into your graveyard from the battlefield, create a 2/2 black Zombie creature token.
   * When a creature is put into an opponent's graveyard from the battlefield, exile this creature. */

  card_instance_t *instance;
  int deaths_or_legacy;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      bool found = false;
      if (0 < active_cards_count[player])
      {
        deaths_or_legacy = 0;
        do
        {
          instance = in_play(player, deaths_or_legacy);

          if ((((instance != NULL) && (is_what(player, deaths_or_legacy, 0x80) != 0)) &&
               (instance = get_card_instance(player, deaths_or_legacy),
                instance->info_slot_fn == magus_of_the_bridge_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            found = true;
            break;
          }
          deaths_or_legacy = deaths_or_legacy + 1;
        } while (deaths_or_legacy < active_cards_count[player]);
      }

      if (!found)
      {
        deaths_or_legacy =
            create_targetted_legacy_effect(player, card, magus_of_the_bridge_effect, player, card);
        instance = get_card_instance(player, deaths_or_legacy);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, deaths_or_legacy, 0x10000);
      }
    }
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 2);
    this_test.type_flag = 3;
    count_for_gfp_ability_and_store_values(player, card, 0x77, 2, 0, &this_test, 0x200, 0);
  }
  instance = get_card_instance(player, card);

  if ((instance->kill_code == '\0') &&
      (deaths_or_legacy = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY),
       deaths_or_legacy != 0))
  {
    token_generation_t token;

    default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
    token.qty = (char)deaths_or_legacy;
    token.tou = 2;
    token.pow = 2;
    token.color_forced = '\x02';
    generate_token(&token);
    if ((char)((unsigned int)deaths_or_legacy >> 8) != '\0')
    {
      kill_card(player, card, KILL_EXILE);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2239bc0; original cards/modern_horizons_2.c:4452. */
int card_master_of_death(int player, int card, event_t event)
{
  /* Master of Death	|1|U|B
   * Creature - Zombie Wizard 3/1
   * When this creature enters, surveil 2. (Look at the top two cards of your library, then put any number of them into your graveyard and the rest on top of your library in any order.)
   * At the beginning of your upkeep, if this card is in your graveyard, you may pay 1 life. If you do, return it to your hand. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    surveil(player, 2);
    return 0;
  }
  if (event == EVENT_GRAVEYARD_ABILITY_UPKEEP)
  {
    if (can_pay_life_as_cost_for_spells_or_activated_abilities(player, 1))
    {
      int choice = do_dialog(player, player, card, -1, -1,
                             " Return Master of Death to your hand\n decline", life[player] < 7);
      if (choice == 0)
      {
        lose_life(player, 1);
        card_instance_t *instance = get_card_instance(player, card);
        unsigned int saved_state = instance->state;
        ++hand_count[player];
        instance->state = saved_state & 0xffffffdf;
        return -1;
      }
    }
    return -2;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2239cd0; original cards/modern_horizons_2.c:4482. */
int card_mental_journey(int player, int card, event_t event)
{
  /* Mental Journey	|4|U|U
   * Instant
   * Draw three cards.
   * Basic landcycling |1|U (|1|U, Discard this card: Search your library for a basic land card, reveal it, put it into your hand, then shuffle.) */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      draw_cards(player, 3);
      kill_card(player, card, KILL_DESTROY);
    }
    result = basic_spell(player, card, event);
    return result;
  }
  result = basic_landcycling(player, card, event, 1, 0, 1, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x2239d80; original cards/modern_horizons_2.c:4499. */
int card_mine_collapse(int player, int card, event_t event)
{
  /* Mine Collapse	|3|R
   * Instant
   * If it's your turn, you may sacrifice a Mountain rather than pay this spell's mana cost.
   * ~ deals 5 damage to target creature or planeswalker. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t test;

  if (event == EVENT_MODIFY_COST)
  {
    default_test_definition(&test, 0x47);
    test.subtype = 0x5006;
    if ((current_turn == player) &&
        (result = new_can_sacrifice_as_cost(player, card, &test), result != 0))
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      null_casting_cost(player, card);
    }
    else
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 0;
    }
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      default_target_definition(player, card, &td, 0x102);
      get_card_instance(player, card);
      return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    }
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        default_target_definition(player, card, &td, 0x102);
        instance = get_card_instance(player, card);
        if ((card == affected_card) && (player == affected_card_controller))
        {
          result = played_for_free(player, card);
          if (((result == 0) && (result = is_token(player, card), result == 0)) &&
              (instance->info_slot == 1))
          {

            result = has_mana_to_cast_iid(player, EVENT_CAST_SPELL, instance->internal_card_id);
            if (result != 0)
            {
              result = count_subtype(player, 1, -1);
              result = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                                   "Sacrifice a mountain", 1, result * 2, "Cast normally", 1, 5, 0,
                                   0, 0, 0);
              if (result != 1)
              {
                if (result == 2)
                {
                  result = get_id(player, card);
                  charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
                }
                else
                {
                  spell_fizzled = 1;
                }
                return generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                                     "Select target creature or planeswalker.", 1, NULL);
              }
            }
            new_default_test_definition(&test, 0x47, "Select a Mountain to sacrifice.");
            test.subtype = 0x5006;
            new_sacrifice(player, card, player, SAC_AS_COST, &test);
            td.allow_cancel = 0;
          }
          return generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                               "Select target creature or planeswalker.", 1, NULL);
        }
      }
    }
    else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 0x102);
      get_card_instance(player, card);
      if (event == EVENT_RESOLVE_SPELL)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          damage_target0(player, card, 5);
        }
        kill_card(player, card, KILL_DESTROY);
      }
    }
    else if (event == EVENT_CHANGE_TARGET)
    {
      default_target_definition(player, card, &td, 0x102);
      get_card_instance(player, card);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x223a190; original cards/modern_horizons_2.c:4576. */
int card_moderation(int player, int card, event_t event)
{
  /* Moderation	|1|W|U
   * Enchantment
   * You can't cast more than one spell each turn.
   * Whenever you cast a spell, draw a card. */

  int result;

  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((result == 0) && (affected_card_controller == player))
    {
      result = is_what(player, affected_card, 1);
      if (result == 0)
      {
        result = get_specific_storm_count(affected_card_controller);
        if (0 < result)
        {
          infinite_casting_cost();
        }
      }
    }
    sp_ability_trigger(player, EVENT_MODIFY_COST_GLOBAL, 2, TYPE_ANY, NULL, player, card);
    return 0;
  }
  sp_ability_trigger(player, event, 2, TYPE_ANY, NULL, player, card);
  if (event == EVENT_SPA)
  {
    draw_cards(player, 1);
    return 0;
  }
  if (event == EVENT_SHOULD_AI_PLAY)
  {

    if (player == 0)
    {
      ai_modifier = ai_modifier + -8;
    }
    else
    {
      ai_modifier = ai_modifier + (unsigned int)((hand_count[player] < 3) ? 0 : -16) + 8;
    }
    return 0;
  }
  if (event != EVENT_CAN_CAST)
  {
    return 0;
  }
  result = basic_spell(player, card, EVENT_CAN_CAST);
  return result;
}

/* ManalinkEh 2022: 0x223a330; original cards/modern_horizons_2.c:4607. */
int card_monoskelion(int player, int card, event_t event)
{
  /* Monoskelion	|2
   * Artifact Creature - Construct 1/1
   * This creature enters with a +1/+1 counter on it.
   * |1, Remove a +1/+1 counter from this creature: It deals 1 damage to any target. */

  int result;
  result = triskelion_like_creature(player, card, event, 1, 1, 0, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x223a390; original cards/modern_horizons_2.c:4616. */
int card_mount_velus_manticore(int player, int card, event_t event)
{
  /* Mount Velus Manticore	|2|R|R
   * Enchantment Creature - Manticore 3/4
   * At the beginning of combat on your turn, you may discard a card. When you do, this creature deals X damage to any target, where X is the number of card types the discarded card has. */

  if (beginning_of_combat(player, card, event, player))
  {
    test_definition_t test;
    new_default_test_definition(&test, 0xff, "Select a card to discard.");
    int selected = new_select_a_card(player, player, 4, 0, 3, -1, &test);
    if (selected >= 0)
    {
      type_t discarded_type = get_type(player, selected);
      unsigned int amount = num_bits_set(discarded_type & ~TYPE_INTERRUPT);
      amount += is_tribal(player, selected) != 0;
      discard_card(player, selected);
      target_definition_t td;
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td.allow_cancel = 0;
      if (can_target(&td) && new_pick_target(&td, "TARGET_ANY", 0, 0))
      {
        damage_target0(player, card, amount);
        get_card_instance(player, card)->number_of_targets = 0;
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x223a530; original cards/modern_horizons_2.c:4645. */
int card_murktide_regent(int player, int card, event_t event)
{
  /* Murktide Regent	|5|U|U
   * Creature - Dragon 3/3
   * Delve (Each card you exile from your graveyard while casting this spell pays for |1.)
   * Flying
   * This creature enters with a +1/+1 counter on it for each instant and sorcery card exiled with it.
   * Whenever an instant or sorcery card leaves your graveyard, put a +1/+1 counter on this creature. */

  card_instance_t *instance;
  inst_info_t result;

  delve(player, card, event);
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      result = inst_info(instance, 0x5a37);
      enters_the_battlefield_with_counters(player, card, EVENT_CAST_SPELL, COUNTER_P1_P1,
                                           result.data);
      clear_info(instance, 0x5a37);
    }
  }
  else if ((event == EVENT_REMOVED_FROM_GRAVEYARD) && (affected_card_controller == player))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      add_counters(player, card, player, card, COUNTER_P1_P1,
                               (unsigned int)((unsigned int)event_result >> 8 & 0xff));
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x223a620; original cards/modern_horizons_2.c:4671. */
int card_myr_scrapling(int player, int card, event_t event)
{
  /* Myr Scrapling	|1
   * Artifact Creature - Myr 1/1
   * Sacrifice this creature: Put a +1/+1 counter on target creature. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                               COUNTER_P1_P1, 1);
    }
  }
  result = generic_activated_ability(player, card, event, 0xc, 0, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
  return result;
}

/* ManalinkEh 2022: 0x223a780; original cards/modern_horizons_2.c:4695. */
int card_mystic_redaction(int player, int card, event_t event)
{
  /* Mystic Redaction	|2|U
   * Enchantment
   * At the beginning of your upkeep, scry 1.
   * Whenever you discard a card, each opponent mills two cards. (They put the top two cards of their library into their graveyard.) */

  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    scry(player, 1);
  }
  if (discard_trigger(player, card, event, player, 2, 0))
  {
    mill(1 - player, 2);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x223a830; original cards/modern_horizons_2.c:4712. */
int card_necrogoyf(int player, int card, event_t event)
{
  /* Necrogoyf	|3|B|B
   * Creature - Lhurgoyf * /4
   * ~'s power is equal to the number of creature cards in all graveyards.
   * At the beginning of each player's upkeep, that player discards a card.
   * Madness |1|B|B (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

  int own_creatures;
  int opponent_creatures;

  if ((uint32_t)(event - EVENT_CAN_PAY_MADNESS_COST) < 2)
  {
    return madness(player, card, event, 1, 2, 0, 0, 0, 0);
  }
  upkeep_trigger_ability(player, card, event, 2);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    if (((event == EVENT_POWER) && (card == affected_card)) && (player == affected_card_controller))
    {
      own_creatures = count_graveyard_by_type(player, 2);
      opponent_creatures = count_graveyard_by_type(1 - player, 2);
      set_base_pt(own_creatures + opponent_creatures);
    }
    return 0;
  }
  new_multidiscard(current_turn, 1, 2, player);
  return 0;
}

/* ManalinkEh 2022: 0x223a960; original cards/modern_horizons_2.c:4735. */
int card_necromancers_familiar(int player, int card, event_t event)
{
  /* Necromancer's Familiar	|3|B
   * Creature - Bird Spirit 3/1
   * Flying
   * Hellbent - This creature has lifelink as long as you have no cards in hand.
   * |B, Discard a card: This creature gains indestructible until end of turn. Tap it. */

  if (event == EVENT_ABILITIES)
  {
    if (card == affected_card && player == affected_card_controller &&
        !is_humiliated(player, card, AL_STATIC) && hand_count[player] == 0)
      lifelink(player, card, EVENT_ABILITIES);
  }
  else if (IS_GAA_EVENT(event))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      pump_ability_until_eot_no_repeat(player, card, instance->parent_controller,
                                       instance->parent_card, 0, 0x800);
      tap_card(instance->parent_controller, instance->parent_card);
    }
    return generic_activated_ability(player, card, event, 0x20, 0, 1, 0, 0, 0, 0, 0, NULL, NULL);
  }
  return 0;
}

/* ManalinkEh 2022: 0x223aad0; original cards/modern_horizons_2.c:4759. */
int card_nested_shambler(int player, int card, event_t event)
{
  /* Nested Shambler	|B
   * Creature - Zombie 1/1
   * When this creature dies, create X tapped 1/1 green Squirrel creature tokens, where X is this creature's power. */

  int result;
  token_generation_t token;

  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  default_token_definition(player, card, CARD_ID_SQUIRREL, &token);
  token.action = TOKEN_ACTION_TAPPED;
  result = get_power(player, card);
  token.qty = (unsigned char)result;
  generate_token(&token);
  return 0;
}

/* ManalinkEh 2022: 0x223ab50; original cards/modern_horizons_2.c:4775. */
int card_nettlecyst(int player, int card, event_t event)
{
  /* Nettlecyst	|3
   * Artifact - Equipment
   * Living weapon (When this Equipment enters, create a 0/0 black Phyrexian Germ creature token, then attach this to it.)
   * Equipped creature gets +1/+1 for each artifact and/or enchantment you control.
   * Equip |2 */

  int result;
  int power;

  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    result = count_permanents_by_type(player, TYPE_ARTIFACT | TYPE_ENCHANTMENT);
    power = count_permanents_by_type(player, TYPE_ARTIFACT | TYPE_ENCHANTMENT);
    attached_gets_pt(player, card, event, power, result);
  }
  result = living_weapon(player, card, event, 2);
  return result;
}

/* ManalinkEh 2022: 0x223abd0; original cards/modern_horizons_2.c:4789. */
int card_nykthos_paragon(int player, int card, event_t event)
{
  /* Nykthos Paragon	|4|W|W
   * Enchantment Creature - Human Soldier 4/6
   * Whenever you gain life, you may put that many +1/+1 counters on each creature you control. Do this only once each turn. */

  int result;
  actions_t life_gained;
  card_instance_t *instance;

  result = duh_mode(player);
  life_gained =
      trigger_gain_life(player, card, event, player, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (life_gained != 0)
  {
    instance = get_card_instance(player, card);
    if (instance->info_slot != 0x42)
    {
      instance = in_play(player, card);
      if (instance != NULL)
      {
        manipulate_type(player, card, 2, TYPE_CREATURE,
                        life_gained & 0xffff | ACT_ADD_COUNTERS_BASE);
        instance = get_card_instance(player, card);
        set_inst_info(instance, 0x5a3e, 0, 0x42);
      }
    }
  }
  if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(player, card);
    clear_info(instance, 0x5a3e);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x223ace0; original cards/modern_horizons_2.c:4806. */
int card_obsidian_charmaw(int player, int card, event_t event)
{
  /* Obsidian Charmaw	|3|R|R
   * Creature - Dragon 4/4
   * This spell costs |1 less to cast for each land your opponents control that could produce |C.
   * Flying
   * When this creature enters, destroy target nonbasic land an opponent controls. */

  char colorless_lands;
  card_instance_t *instance;
  int opponent_or_result;
  int card_00;
  target_definition_t td1;

  if (event == EVENT_MODIFY_COST)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      opponent_or_result = 1 - player;
      if (active_cards_count[opponent_or_result] < 1)
      {
        colorless_lands = '\0';
      }
      else
      {
        card_00 = 0;
        colorless_lands = '\0';
        do
        {
          instance = in_play(opponent_or_result, card_00);
          if ((instance != NULL) && (is_what(opponent_or_result, card_00, 1) != 0))
          {
            instance = get_card_instance(opponent_or_result, card_00);
            colorless_lands = (colorless_lands + '\x01') - ((instance->card_color & 0x41) == 0);
          }
          card_00 = card_00 + 1;
        } while (card_00 < active_cards_count[opponent_or_result]);
      }
      COST_COLORLESS = COST_COLORLESS - colorless_lands;
    }
    etb_ability(player, card, EVENT_MODIFY_COST, RESOLVE_TRIGGER_MANDATORY);
    return 0;
  }
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td1, 1);
    td1.allowed_controller = 1 - player;
    td1.required_subtype = 0x2000;
    td1.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
    td1.allow_cancel = 0;
    td1.preferred_controller = td1.allowed_controller;
    instance = get_card_instance(player, card);
    opponent_or_result = can_target(&td1);
    if ((opponent_or_result != 0) &&
        (opponent_or_result =
             new_pick_target(&td1, "Select target nonbasic land an opponent controls.", 0, 0x80),
         opponent_or_result != 0))
    {
      action_on_target(player, card, 0, ACT_KILL_DESTROY);
      instance->number_of_targets = '\0';
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x223aeb0; original cards/modern_horizons_2.c:4844. */
int card_orchard_strider(int player, int card, event_t event)
{
  /* Orchard Strider	|4|G|G
   * Creature - Treefolk 6/4
   * When this creature enters, create two Food tokens. (They're artifacts with "|2, |T, Sacrifice this token: You gain 3 life.")
   * Basic landcycling |1|G (|1|G, Discard this card: Search your library for a basic land card, reveal it, put it into your hand, then shuffle.) */

  int result;

  if ((uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND) < 3)
  {
    result = basic_landcycling(player, card, event, 1, 0, 0, 1, 0, 0);
    return result;
  }
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  generate_tokens_by_id(player, card, CARD_ID_FOOD, 2);
  return 0;
}

/* ManalinkEh 2022: 0x223b200; original cards/modern_horizons_2.c:4917. */
int card_persist2(int player, int card, event_t event)
{
  /* Persist	|1|B
   * Sorcery
   * Return target nonlegendary creature card from your graveyard to the battlefield with a -1/-1 counter on it. */

  test_definition_t this_test;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  new_default_test_definition(&this_test, 2, "Select target nonlegendary creature card.");
  this_test.ai_selection_mode = 2;
  this_test.subtype = 0x2001;
  this_test.subtype_flag = 1;
  if (event == EVENT_RESOLVE_SPELL)
  {
    int selected = validate_target_from_grave_source(player, card, player, 0);
    if (selected != -1)
    {
      reanimate_permanent(player, card, player, selected, REANIMATE_MINUS1_MINUS1_COUNTER);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 8, NULL, NULL, 0, &this_test);
}

/* ManalinkEh 2022: 0x223b380; original cards/modern_horizons_2.c:4950. */
int card_piercing_rays(int player, int card, event_t event)
{
  /* Piercing Rays	|1|W
   * Sorcery
   * Exile target tapped creature.
   * Forecast - |2|W, Reveal this card from your hand: Tap target untapped creature. (Activate this ability only during your upkeep and only once each turn.) */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = card_excoriate(player, card, event);
    return result;
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  td.illegal_state = TARGET_STATE_TAPPED;
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE_FROM_HAND)
  {
    result = has_mana_multi(player, 2, 0, 0, 0, 0, 1);
    if ((((result != 0) && (result = can_target(&td), result != 0)) &&
         (instance->info_slot != 1)) &&
        (current_phase == 4))
    {
      return 1;
    }
  }
  else if (event == EVENT_ACTIVATE_FROM_HAND)
  {
    instance->number_of_targets = '\0';
    charge_mana_multi(player, 2, 0, 0, 0, 0, 1);
    if ((spell_fizzled != 1) &&
        (result = new_pick_target(&td, "Select target untapped creature.", 0, 0x80), result != 0))
    {
      return 1;
    }
  }
  else if (((event == EVENT_RESOLVE_ACTIVATION_FROM_HAND) && (spell_fizzled != 1)) &&
           (result = valid_target(&td), result != 0))
  {
    tap_card(instance->targets[0].player, instance->targets[0].card);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x223b560; original cards/modern_horizons_2.c:4991. */
int card_piru_the_volatile(int player, int card, event_t event)
{
  /* Piru, the Volatile	|2|R|R|W|W|B|B
   * Legendary Creature - Elder Dragon 7/7
   * Flying, lifelink
   * At the beginning of your upkeep, sacrifice Piru unless you pay |R|W|B.
   * When Piru dies, it deals 7 damage to each nonlegendary creature. */

  int result;
  test_definition_t this_test;
  check_legend_rule(player, card, event);
  lifelink(player, card, event);
  if ((trigger_condition == TRIGGER_UPKEEP) && (current_turn == player))
  {
    upkeep_trigger_ability(player, card, event, player);
  }
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    charge_mana_multi(player, 0, 1, 0, 0, 1, 1);
    if (spell_fizzled == 1)
    {
      kill_card(player, card, KILL_SACRIFICE);
    }
  }
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  default_test_definition(&this_test, 2);
  this_test.subtype = 0x2001;
  this_test.subtype_flag = 1;
  new_damage_all(player, card, 2, 7, 0, &this_test);
  return 0;
}

/* ManalinkEh 2022: 0x223b6b0; original cards/modern_horizons_2.c:5031. */
int card_priest_of_fell_rites(int player, int card, event_t event)
{
  /* Priest of Fell Rites	|W|B
   * Creature - Human Warlock 2/2
   * |T, Pay 3 life, Sacrifice this creature: Return target creature card from your graveyard to the battlefield. Activate only as a sorcery.
   * Unearth |3|W|B (|3|W|B: Return this card from your graveyard to the battlefield. It gains haste. Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

  int result;
  card_instance_t *instance;
  test_definition_t this_test;

  if (event == EVENT_CAN_ACTIVATE)
  {
    result = count_graveyard_by_type(player, 2);
    if ((0 < result) && (result = graveyard_has_shroud(player), result == 0))
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x19, 0, 0, 0, 0, 0, 0,
                                         3, NULL, NULL);
      return result;
    }
  }
  else
  {
    if (event == EVENT_ACTIVATE)
    {
      new_default_test_definition(&this_test, 2, "Select target creature card.");
      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
      result = select_target_from_grave_source(player, card, player, SFG_0, 2, &this_test, 0);
      if (result != -1)
      {
        result = generic_activated_ability(player, card, EVENT_ACTIVATE, 0x19, 0, 0, 0, 0, 0, 0, 3,
                                           NULL, NULL);
        return result;
      }
      spell_fizzled = 1;
      return 0;
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) &&
        (result = validate_target_from_grave_source(player, card, player, 0), result != -1))
    {
      reanimate_permanent(player, card, player, result, REANIMATE_DEFAULT);
    }
  }
  return unearth(player, event, 3, 1, 0, 0, 0, 1);
}

/* ManalinkEh 2022: 0x223b8f0; original cards/modern_horizons_2.c:5067. */
int card_prismatic_ending(int player, int card, event_t event)
{
  /* Prismatic Ending	|X|W
   * Sorcery
   * Converge - Exile target nonland permanent if its mana value is less than or equal to the number of colors of mana spent to cast this spell. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  store_num_colors_paid_in_eot_toughness0(player, card, event);
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.illegal_type = TYPE_LAND;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      result = get_cmc(instance->targets[0].player, instance->targets[0].card);
      if (result <= (int)(unsigned int)instance->eot_toughness0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td, "Select target nonland permanent.", 1, NULL);
}

/* ManalinkEh 2022: 0x223ba70; original cards/modern_horizons_2.c:5094. */
int card_profane_tutor(int player, int card, event_t event)
{
  /* Profane Tutor	""
   * Sorcery
   * Suspend 2-|1|B (Rather than cast this card from your hand, pay |1|B and exile it with two time counters on it. At the beginning of your upkeep, remove a time counter. When the last is removed, you may cast it without paying its mana cost.)
   * Search your library for a card, put that card into your hand, then shuffle. */

  int result;

  if (event != EVENT_RESOLVE_SPELL)
  {
    result = suspend_only(player, card, event, 2, 1, 1, 0, 0, 0, 0);
    return result;
  }
  result = card_demonic_tutor(player, card, EVENT_RESOLVE_SPELL);
  return result;
}

/* ManalinkEh 2022: 0x223bae0; original cards/modern_horizons_2.c:5108. */
int card_prophetic_titan(int player, int card, event_t event)
{
  /* Prophetic Titan	|4|U|R
   * Creature - Giant Wizard 4/4
   * Delirium - When this creature enters, choose one. If there are four or more card types among cards in your graveyard, choose both instead.
   * * This creature deals 4 damage to any target.
   * * Look at the top four cards of your library. Put one of them into your hand and the rest on the bottom of your library in a random order. */

  card_instance_t *instance;
  int value;
  unsigned int selected_modes;
  target_definition_t td;
  test_definition_t test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    value = delirium(player);
    if ((value == 0) && (value = can_target(&td), value != 0))
    {
      value = hand_count[player];
      selected_modes = dialog_impl(player, card, EVENT_ACTIVATE, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_AUTOCHOOSE_IF_1,
                                   "4 damage to any target", can_target(&td), 3,
                                   "Look at top four of library", 1, 7 - value, 0, 0, 0, 0);
      if (((selected_modes & 1) != 0) &&
          (value = new_pick_target(&td, "TARGET_ANY", 0, 1), value != 0))
      {
        damage_target0(player, card, 4);
        instance->number_of_targets = '\0';
      }
      if ((selected_modes & 2) == 0)
      {
        return 0;
      }
    }
    else
    {
      if (new_pick_target(&td, "TARGET_ANY", 0, 1) != 0)
      {
        damage_target0(player, card, 4);
        instance->number_of_targets = '\0';
      }
    }
    new_default_test_definition(&test, 0xff, "Select a card to add to your hand.");
    reveal_top_cards_of_library_and_choose(player, card, player, 4, 1, TUTOR_HAND, 0,
                                           TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test);
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int magus_of_the_bridge_effect(int player, int card, event_t event);

/* ManalinkEh 2022: 0x223bdf0; original cards/modern_horizons_2.c:5167. */
int card_ragavan_nimble_pilferer(int player, int card, event_t event)
{
  /* Ragavan, Nimble Pilferer	|R
   * Legendary Creature - Monkey Pirate 2/1
   * Whenever Ragavan deals combat damage to a player, create a Treasure token and exile the top card of that player's library. Until end of turn, you may cast that card.
   * Dash |1|R (You may cast this spell for its dash cost. If you do, it gains haste, and it's returned from the battlefield to its owner's hand at the beginning of the next end step.) */

  unsigned int packed_damage_counts;
  int result;
  card_instance_t *instance;
  unsigned int remaining;
  int damaged_player;
  unsigned int *damage_count;
  int times_damaged[2];

  check_legend_rule(player, card, event);
  dash(player, card, event, 1, 0, 0, 0, 1, 0);
  result = damage_dealt_by_me(player, card, event,
                              DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                                  DDBM_MUST_BE_COMBAT_DAMAGE);
  if (result != 0)
  {
    instance = get_card_instance(player, card);
    damaged_player = 0;
    packed_damage_counts = instance->targets[1].player;
    instance->targets[1].player = 0;
    damage_count = (unsigned int *)times_damaged;
    remaining = packed_damage_counts & 0xff;
    times_damaged[1] = packed_damage_counts >> 8 & 0xff;
    times_damaged[0] = remaining;
    while (true)
    {
      while (remaining != 0)
      {
        generate_token_by_id(player, card, CARD_ID_TREASURE);
        exile_top_x_card_you_may_play_them(player, card, 1, NULL, damaged_player, MPCFE_UNTIL_EOT);
        remaining = remaining - 1;
        *damage_count = remaining;
      }
      damaged_player = damaged_player + 1;
      damage_count = damage_count + 1;
      if (damaged_player == 2)
        break;
      remaining = *damage_count;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x223bf20; original cards/modern_horizons_2.c:5201. */
int card_ravenous_squirrel(int player, int card, event_t event)
{
  /* Ravenous Squirrel	|BG
   * Creature - Squirrel 1/1
   * Whenever you sacrifice an artifact or creature, put a +1/+1 counter on this creature.
   * |1|B|G, Sacrifice an artifact or creature: You gain 1 life and draw a card. */

  card_instance_t *instance;
  test_definition_t this_test;

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {

    instance = in_play(affected_card_controller, affected_card);
    if ((((instance != NULL) && (affected_card_controller == player)) &&
         (is_what(player, affected_card, 0x42) != 0)) &&
        ((instance->kill_code == '\x03' &&
          (check_special_flags(affected_card_controller, affected_card, 0x800) == 0))))
    {

      instance = get_card_instance(player, card);
      if (instance->targets[0xb].player < 0)
      {
        instance->targets[0xb].player = 0;
      }
      *(char *)&instance->targets[0xb].player = (char)instance->targets[0xb].player + '\x01';
    }
  }
  instance = get_card_instance(player, card);
  if (instance->kill_code == 0)
  {
    unsigned int sacrifices = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (sacrifices != 0)
      add_counters(player, card, player, card, COUNTER_P1_P1, sacrifices & 0xff);
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
    return 0;
  if (((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE)) &&
      ((event != EVENT_RESOLVE_ACTIVATION && (event != EVENT_CLEANUP))))
  {
    return 0;
  }
  new_default_test_definition(&this_test, 0x42, "Select a creature or an artifact to sacrifice.");
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    draw_cards(player, 1);
    gain_life(player, 1);
  }
  return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_NONE, 1, 1, 0, 1,
                                                          0, 0, 0, NULL, NULL, TYPE_NONE,
                                                          &this_test, SARV_0);
}

/* ManalinkEh 2022: 0x223c150; original cards/modern_horizons_2.c:5244. */
int card_raving_visionary(int player, int card, event_t event)
{
  /* Raving Visionary	|1|U
   * Creature - Merfolk Wizard 1/1
   * |U, |T: Draw a card, then discard a card.
   * Delirium - |2|U, |T: Draw a card. Activate only if there are four or more card types among cards in your graveyard. */

  int c;
  card_instance_t *instance;
  int result_or_cost;
  card_instance_t *permanent;
  unsigned int draw_available;
  int land_count;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result_or_cost = delirium(player);
      if ((result_or_cost != 0) &&
          (result_or_cost = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 2, 0, 1,
                                                      0, 0, 0, 0, NULL, NULL),
           result_or_cost != 0))
      {
        return 1;
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 1, 0, 0, 0, 0,
                                       NULL, NULL);
    }

    result_or_cost = delirium(player);
    draw_available = 0;
    if (result_or_cost != 0)
    {
      result_or_cost = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 2, 0, 1, 0, 0,
                                                 0, 0, NULL, NULL);
      draw_available = (unsigned int)(result_or_cost != 0);
    }
    result_or_cost = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 1, 0, 0,
                                               0, 0, NULL, NULL);
    if (dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_MODE,
                    "Draw & discard", result_or_cost, 5, "Draw a card", draw_available, 10, 0, 0, 0,
                    0) == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    c = 0;
    land_count = 0;
    bool no_active_cards = active_cards_count[player] < 1;
    if (!no_active_cards)
    {
      do
      {
        permanent = in_play(player, c);
        if (permanent != NULL || (permanent = in_hand(player, c), permanent != NULL))
          land_count += is_what(player, c, 1) != 0;
        ++c;
      } while (c < active_cards_count[player]);
    }
    if (no_active_cards || land_count - 3U > 4)
      ai_modifier += 0x30;
    if (instance->mode == '\x01')
    {
      result_or_cost = 0;
    }
    else
    {
      if (instance->mode != '\x02')
        return 0;
      result_or_cost = 2;
    }
    result_or_cost = generic_activated_ability(player, card, EVENT_ACTIVATE, 1, result_or_cost, 0,
                                               1, 0, 0, 0, 0, NULL, NULL);
  }
  else
  {
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      instance = get_card_instance(player, card);
      if ((event == EVENT_RESOLVE_ACTIVATION) && (draw_a_card(player), instance->mode == '\x01'))
      {
        discard(player, 0, player);
      }
    }
    result_or_cost = 0;
  }
  return result_or_cost;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x223c5c0; original cards/modern_horizons_2.c:5303. */
int card_recalibrate(int player, int card, event_t event)
{
  /* Recalibrate	|1|U
   * Instant
   * Return target creature to its owner's hand. If you've discarded a card this turn, draw a card. */

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
      bounce_permanent(instance->targets[0].player, instance->targets[0].card);
      result = get_trap_condition(player, 0x203);
      if (result != 0)
      {
        draw_a_card(player);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x223c720; original cards/modern_horizons_2.c:5328. */
int card_resurgent_belief(int player, int card, event_t event)
{
  /* Resurgent Belief	""
   * Sorcery
   * Suspend 2-|1|W (Rather than cast this card from your hand, pay |1|W and exile it with two time counters on it. At the beginning of your upkeep, remove a time counter. When the last is removed, you may cast it without paying its mana cost.)
   * Return all enchantment cards from your graveyard to the battlefield. (Auras with nothing to enchant remain in your graveyard.) */

  int result;

  if (event != EVENT_RESOLVE_SPELL)
  {
    result = suspend_only(player, card, event, 2, 1, 0, 0, 0, 0, 1);
    return result;
  }
  result = card_replenish(player, card, EVENT_RESOLVE_SPELL);
  return result;
}

/* ManalinkEh 2022: 0x223c790; original cards/modern_horizons_2.c:5341. */
int card_revolutionist(int player, int card, event_t event)
{
  /* Revolutionist	|5|R
   * Creature - Human Wizard 3/3
   * When this creature enters, return target instant or sorcery card from your graveyard to your hand.
   * Madness |3|R (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

  int result;

  if (1 < (uint32_t)(event - EVENT_CAN_PAY_MADNESS_COST))
  {
    result = card_archaeomancer(player, card, event);
    return result;
  }
  return madness(player, card, event, 3, 0, 0, 0, 1, 0);
}

/* ManalinkEh 2022: 0x223c800; original cards/modern_horizons_2.c:5353. */
int card_rift_sower(int player, int card, event_t event)
{
  /* Rift Sower	|2|G
   * Creature - Elf Druid 1/3
   * |T: Add one mana of any color.
   * Suspend 2-|G (Rather than cast this card from your hand, you may pay |G and exile it with two time counters on it. At the beginning of your upkeep, remove a time counter. When the last is removed, you may cast it without paying its mana cost. It has haste.) */

  if (IS_GS_EVENT(player, card, event) || event == EVENT_MODIFY_COST)
    return suspend(player, card, event, 2, 0, 0, 0, 1, 0, 0, NULL, NULL);
  return card_generic_noncombat_1_mana_producing_creature(player, card, event);
}

/* ManalinkEh 2022: 0x223cd70; original cards/modern_horizons_2.c:5434. */
int card_rishadan_dockhand(int player, int card, event_t event)
{
  /* Rishadan Dockhand	|U
   * Creature - Merfolk 1/2
   * Islandwalk (This creature can't be blocked as long as defending player controls an Island.)
   * |1, |T: Tap target land. */

  card_instance_t *instance;
  target_definition_t td;
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 1);
    if (event == EVENT_CAN_ACTIVATE)
      return generic_activated_ability(player, card, event, 5, 1, 0, 0, 0, 0, 0, 0, &td,
                                       "TARGET_LAND");
    generic_activated_ability(player, card, EVENT_ACTIVATE, 5, 1, 0, 0, 0, 0, 0, 0, &td,
                              "TARGET_LAND");
    if (spell_fizzled != 1)
    {
      ai_mod_for_tap_effects(player, card, 0);
    }
  }
  else if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
  {
    default_target_definition(player, card, &td, 1);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (valid_target(&td))
      {
        instance = get_card_instance(player, card);
        tap_card(instance->targets[0].player, instance->targets[0].card);
      }
    }
    return generic_activated_ability(player, card, event, 5, 1, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_LAND");
  }
  return 0;
}

/* ManalinkEh 2022: 0x223cf40; original cards/modern_horizons_2.c:5463. */
int card_road_ruin(int player, int card, event_t event)
{
  /* Road // Ruin	|2|G // |1|R|R
   * Instant // Sorcery
   * Road:
   * Search your library for a basic land card, put it onto the battlefield tapped, then shuffle.
   *
   * Ruin:
   * Aftermath (Cast this spell only from your graveyard. Then exile it.)
   * Ruin deals damage to target creature equal to the number of lands you control. */

  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY);
    if (result == 0)
    {
      return 0;
    }
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    if (result != 0)
    {
      result = aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 1, 0, 0, 0, 2, 0, 0);
      return result;
    }
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      result = basic_spell(player, card, EVENT_CAN_CAST);
      return result;
    }
    if (event == EVENT_PLAY_FIRST_HALF)
    {
      result = basic_spell(player, card, EVENT_CAST_SPELL);
      return result;
    }
    if (event == EVENT_PLAY_SECOND_HALF)
    {
      result = generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
      return result;
    }
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = get_aftermath(player, card);
      if (result == 0)
      {
        result = card_rampant_growth(player, card, EVENT_RESOLVE_SPELL);
        return result;
      }
      result = get_aftermath(player, card);
      if ((result != 0) && (result = valid_target(&td), result != 0))
      {
        result = count_subtype(player, 1, -1);
        damage_target0(player, card, result);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  result = aftermath(player, card, event, 1, 0, 0, 0, 2, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x223d1d0; original cards/modern_horizons_2.c:5515. */
int card_said_done(int player, int card, event_t event)
{
  /* Said // Done	|2|U // |3|U
   * Sorcery // Instant
   * Said:
   * Return target instant or sorcery card from your graveyard to your hand.
   *
   * Done:
   * Tap up to two target creatures. Those creatures don't untap during their controller's next untap step. */

  if (event == EVENT_MODIFY_COST)
  {
    test_definition_t test;
    default_test_definition(&test, 0x38);
    unsigned int first_available = 0;
    if (new_special_count_grave(player, &test) && !graveyard_has_shroud(player))
      first_available = can_sorcery_be_played(player, event) != 0;
    target_definition_t td;
    default_target_definition(player, card, &td, 2);
    int second_available = can_target(&td);
    generic_split_card(player, card, event, 0x5a5d, first_available, 10, 0x59e2, second_available,
                       8);
    return 0;
  }
  if (event == EVENT_PLAY_FIRST_HALF || event == EVENT_PLAY_SECOND_HALF)
  {
    card_instance_t *instance = get_card_instance(player, card);
    instance->number_of_targets = 0;
    if (event == EVENT_PLAY_FIRST_HALF)
      return card_call_to_mind(player, card, EVENT_CAST_SPELL);
    return card_frost_breath(player, card, EVENT_CAST_SPELL);
  }
  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  test_definition_t test;
  default_test_definition(&test, 0x38);
  unsigned int first_available = 0;
  if (new_special_count_grave(player, &test) && !graveyard_has_shroud(player))
    first_available = can_sorcery_be_played(player, event) != 0;
  target_definition_t td;
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (instance->info_slot & 1U)
      return card_call_to_mind(player, card, event);
    if (instance->info_slot & 2U)
      return card_frost_breath(player, card, event);
    kill_card(player, card, KILL_DESTROY);
  }
  int priority = 5;
  if (event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller &&
      current_turn == player)
    priority = current_phase > 0x17 ? 15 : 5;
  int second_available = can_target(&td);
  return generic_split_card(player, card, event, 0x5a5d, first_available, priority, 0x59e2,
                            second_available, 10);
}

/* ManalinkEh 2022: 0x223d690; original cards/modern_horizons_2.c:5576. */
int card_sanctifier_en_vec(int player, int card, event_t event)
{
  /* Sanctifier en-Vec	|W|W
   * Creature - Human Cleric 2/2
   * Protection from black and from red
   * When this creature enters, exile all cards that are black or red from all graveyards.
   * If a black or red permanent, spell, or card not on the battlefield would be put into a graveyard, exile it instead. */

  test_definition_t this_test;
  default_test_definition(&this_test, 0xff);
  this_test.color = get_sleighted_color_test(player, card, 0x12);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    for (int grave_owner = 0; grave_owner < 2; ++grave_owner)
      for (int index = count_graveyard(grave_owner) - 1; index >= 0; --index)
        if (new_make_test(grave_owner, graveyard_ptr_mutable[grave_owner][index], -1, &this_test))
          rfg_card_from_grave(grave_owner, index);
  }
  if (if_a_card_would_be_put_into_graveyard_from_library_do_something_instead(
          player, card, event, 2, RESOLVE_TRIGGER_MANDATORY, &this_test) != 0)
  {
    rfg_top_card_of_deck(trigger_cause_controller);
    replace_milled = 1;
  }
  if (if_a_card_would_be_put_into_graveyard_from_anywhere_but_library_do_something_instead(
          player, card, event, 2, RESOLVE_TRIGGER_MANDATORY, &this_test) != 0)
  {
    add_card_to_rfg(trigger_cause_controller, trigger_cause);
    play_sound_effect(WAV_DESTROY);
    replace_milled = 1;
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x223d840; original cards/modern_horizons_2.c:5617. */
int card_sanctuary_raptor(int player, int card, event_t event)
{
  /* Sanctuary Raptor	|3
   * Artifact Creature - Bird 2/1
   * Flying
   * Whenever this creature attacks, if you control three or more tokens, this creature gets +2/+0 and gains first strike until end of turn. */

  int result;
  test_definition_t this_test;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    default_test_definition(&this_test, 0x47);
    this_test.type_flag = 6;
    result = check_battlefield_for_special_card(player, card, player, 4, &this_test);
    if (2 < result)
    {
      pump_ability_until_eot(player, card, player, card, 2, 0, 0x100, 0);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x223d910; original cards/modern_horizons_2.c:5635. */
int card_sanctum_weaver(int player, int card, event_t event)
{
  /* Sanctum Weaver	|1|G
   * Enchantment Creature - Dryad 0/2
   * |T: Add X mana of any one color, where X is the number of enchantments you control. */

  int result;
  if ((event != EVENT_CAN_CAST) && (event != EVENT_SHOULD_AI_PLAY))
  {
    result = count_permanents_by_type(player, TYPE_ENCHANTMENT);
    result = mana_producing_creature_all_one_color(player, card, event, 0x18,
                                                   COLOR_TEST_ANY_COLORED, result);
    return result;
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x223d990; original cards/modern_horizons_2.c:5647. */
int card_scion_of_draco(int player, int card, event_t event)
{
  /* Scion of Draco	|12
   * Artifact Creature - Dragon 4/4
   * Domain - This spell costs |2 less to cast for each basic land type among lands you control.
   * Flying
   * Each creature you control has vigilance if it's white, hexproof if it's blue, lifelink if it's black, first strike if it's red, and trample if it's green. */

  if (event == EVENT_MODIFY_COST)
  {
    int domain = count_domain(player, card);
    COST_COLORLESS += (char)domain * -2;
  }
  else if (event == EVENT_ABILITIES && affected_card_controller == player &&
           is_what(player, affected_card, TYPE_CREATURE) && !is_humiliated(player, card, AL_STATIC))
  {
    unsigned int clr = get_color(affected_card_controller, affected_card);
    if (get_sleighted_color_test(player, card, 0x20) & clr)
      vigilance(affected_card_controller, affected_card, EVENT_ABILITIES);
    if (get_sleighted_color_test(player, card, 4) & clr)
      hexproof(affected_card_controller, affected_card, EVENT_ABILITIES);
    if (get_sleighted_color_test(player, card, 2) & clr)
      lifelink(affected_card_controller, affected_card, EVENT_ABILITIES);
    if (get_sleighted_color_test(player, card, 0x10) & clr)
      event_result |= 0x100;
    if (get_sleighted_color_test(player, card, 8) & clr)
      event_result |= 0x80;
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int evoke_with_cards_in_hand(int player, int card, event_t event, int color_test);

/* ManalinkEh 2022: 0x223db80; original cards/modern_horizons_2.c:5676. */
int card_scour_the_desert(int player, int card, event_t event)
{
  /* Scour the Desert	|3|W|W
   * Sorcery
   * Exile target creature card from your graveyard. Create X 1/1 white Bird creature tokens with flying, where X is the exiled card's toughness. */

  int grave_index;
  int toughness;
  token_generation_t token;
  test_definition_t this_test;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  new_default_test_definition(&this_test, 2, "Select target creature card.");
  this_test.ai_selection_mode = 2;
  if (event == EVENT_RESOLVE_SPELL)
  {
    grave_index = validate_target_from_grave_source(player, card, player, 0);
    if (grave_index != -1)
    {
      default_token_definition(player, card, CARD_ID_BIRD, &token);
      token.tou = 1;
      token.pow = 1;
      token.color_forced = ' ';
      toughness = get_base_toughness_iid(player, graveyard_ptr_mutable[player][grave_index]);
      token.qty = (unsigned char)toughness;
      rfg_card_from_grave(player, grave_index);
      generate_token(&token);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 8, NULL, NULL, 0, &this_test);
}

/* ManalinkEh 2022: 0x223ddc0; original cards/modern_horizons_2.c:5726. */
int card_scuttletide(int player, int card, event_t event)
{
  /* Scuttletide	|1|U
   * Enchantment
   * |1, Discard a card: Create a 0/3 blue Crab creature token.
   * Delirium - Crabs you control get +1/+1 as long as there are four or more card types among cards in your graveyard. */

  token_generation_t token;

  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if ((((affected_card_controller == player) && (is_humiliated(player, card, AL_STATIC) == 0)) &&
         (has_subtype(affected_card_controller, affected_card, SUBTYPE_CRAB) != 0)) &&
        (delirium(player) != 0))
    {
      event_result = event_result + 1;
    }
    return global_enchantment(player, card, event);
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_CLEANUP)
    return generic_activated_ability(player, card, event, 0x20, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
  if (event != EVENT_RESOLVE_ACTIVATION)
    return global_enchantment(player, card, event);
  default_token_definition(player, card, CARD_ID_CRAB, &token);
  token.pow = 0;
  token.tou = 3;
  token.color_forced = '\x04';
  generate_token(&token);
  return generic_activated_ability(player, card, event, 0x20, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x223df40; original cards/modern_horizons_2.c:5754. */
int card_search_the_premises(int player, int card, event_t event)
{
  /* Search the Premises	|3|W
   * Enchantment
   * Whenever a creature attacks you or a planeswalker you control, investigate. (Create a Clue token. It's an artifact with "|2, Sacrifice this token: Draw a card.") */

  int result;

  store_attackers(player, card, event, 0, 1 - player, -1, NULL);
  dat_ability(player, card, event, DAT_SEPARATE_TRIGGERS | DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
  {
    investigate(player, card, player);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x223e4f0; original cards/modern_horizons_2.c:5884. */
int card_sinister_starfish(int player, int card, event_t event)
{
  /* Sinister Starfish	|1|B
   * Creature - Starfish 0/3
   * |T: Surveil 1. (Look at the top card of your library. You may put it into your graveyard.) */

  int result;

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    surveil(player, 1);
  }
  result = generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x223e590; original cards/modern_horizons_2.c:5896. */
int card_skophos_reaver(int player, int card, event_t event)
{
  /* Skophos Reaver	|2|R
   * Creature - Minotaur Warrior 2/3
   * During your turn, this creature gets +2/+0.
   * Madness |1|R (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

  int result;

  if ((uint32_t)(event - EVENT_CAN_PAY_MADNESS_COST) < 2)
  {
    result = madness(player, card, event, 1, 0, 0, 0, 1, 0);
    return result;
  }
  if (((event == EVENT_POWER) && (card == affected_card)) && (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((result == 0) && (current_turn == player))
    {
      event_result = event_result + 2;
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x223e840; original cards/modern_horizons_2.c:5949. */
int card_slag_strider(int player, int card, event_t event)
{
  /* Slag Strider	|5|R|R
   * Creature - Elemental 3/3
   * Affinity for artifacts (This spell costs |1 less to cast for each artifact you control.)
   * |1, Sacrifice an artifact: This creature deals 1 damage to any target. */

  target_definition_t td_creature_or_player;

  affinity(player, card, event, 0x40, -1);
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td_creature_or_player, 0x102);
    td_creature_or_player.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td_creature_or_player, 0x102);
    td_creature_or_player.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    if ((event == EVENT_RESOLVE_ACTIVATION) &&
        (valid_target(&td_creature_or_player) != 0))
    {
      damage_target0(player, card, 1);
    }
  }
  return generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_CAN_TARGET, 1, 0, 0, 0, 0, 0, 0, &td_creature_or_player,
      "TARGET_ANY", TYPE_ARTIFACT, NULL, SARV_0);
}

/* ManalinkEh 2022: 0x223e9b0; original cards/modern_horizons_2.c:5978. */
int card_smell_fear(int player, int card, event_t event)
{
  /* Smell Fear	|1|G
   * Sorcery
   * Proliferate. (Choose any number of permanents and/or players, then give each another counter of each kind already there.)
   * Target creature you control fights up to one target creature you don't control. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  target_definition_t td1;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    default_target_definition(player, card, &td1, 2);
    td1.allowed_controller = 1 - player;
    td1.allow_cancel = 2;
    td1.preferred_controller = td1.allowed_controller;
    get_card_instance(player, card);
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    if (result != 0)
    {
      result = can_target(&td1);
      return result;
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = player;
      td.preferred_controller = player;
      default_target_definition(player, card, &td1, 2);
      td1.allowed_controller = 1 - player;
      td1.allow_cancel = 2;
      td1.preferred_controller = td1.allowed_controller;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        result = new_pick_target(&td, "Select target creature you control.", 0, 0x81);
        if (result != 0)
        {
          new_pick_target(&td1, "Select target creature opponent controls.", 1, 0x80);
        }
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    default_target_definition(player, card, &td1, 2);
    td1.allowed_controller = 1 - player;
    td1.allow_cancel = 2;
    td1.preferred_controller = td1.allowed_controller;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = validate_target(player, card, &td, 0);
      if (result != 0)
      {
        proliferate(player, card);
        result = validate_target(player, card, &td1, 1);
        if (result != 0)
        {
          fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player,
                instance->targets[1].card);
        }
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x223ecd0; original cards/modern_horizons_2.c:6025. */
int card_so_shiny(int player, int card, event_t event)
{
  /* So Shiny	|2|U
   * Enchantment - Aura
   * Enchant creature
   * When this Aura enters, if you control a token, tap enchanted creature, then scry 2.
   * Enchanted creature doesn't untap during its controller's untap step. */

  int result;
  card_instance_t *instance;
  test_definition_t test;

  if (event == EVENT_UNTAP)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {

        instance = get_card_instance(player, card);
        if ((-1 < instance->damage_target_player) && (-1 < instance->damage_target_card))
        {
          does_not_untap((int)instance->damage_target_player, instance->damage_target_card,
                         EVENT_UNTAP);
        }
      }
    }
    etb_ability(player, card, EVENT_UNTAP, RESOLVE_TRIGGER_MANDATORY);
  }
  else
  {
    etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (event == EVENT_ETB)
    {
      default_test_definition(&test, 0x47);
      test.type_flag = 6;
      result = check_battlefield_for_special_card(player, card, player, 0, &test);
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        if ((-1 < instance->damage_target_player) && (-1 < instance->damage_target_card))
        {
          tap_card((int)instance->damage_target_player, instance->damage_target_card);
        }
        scry(player, 2);
      }
    }
  }
  return disabling_aura(player, card, event);
}

/* ManalinkEh 2022: 0x222b890; original cards/modern_horizons_2.c:6060. */
static int is_artifact_land(int iid, int unused, int unused2, int unused3)
{
  int result;

  result = is_what(-1, iid, 0x40);
  if (result == 0)
  {
    return 0;
  }
  result = is_what(-1, iid, 1);
  return (unsigned int)(result != 0);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x223ee40; original cards/modern_horizons_2.c:6067. */
int card_sojourners_companion(int player, int card, event_t event)
{
  /* Sojourner's Companion	|7
   * Artifact Creature - Salamander 4/4
   * Affinity for artifacts
   * Artifact landcycling |2 (|2, Discard this card: Search your library for an artifact land card, reveal it, put it into your hand, then shuffle.) */

  int result;
  test_definition_t test;

  affinity(player, card, event, 0x40, -1);
  if ((uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND) < 2)
  {
    result = cycling(player, card, event, 2, 0, 0, 0, 0, 0);
    return result;
  }
  if (event != EVENT_RESOLVE_ACTIVATION_FROM_HAND)
  {
    return 0;
  }
  new_default_test_definition(&test, 0x47, "Select an artifact land card.");
  test.special_selection_function = is_artifact_land;
  new_global_tutor(player, player, 1, 1, 0, 1, &test);
  return 0;
}

/* ManalinkEh 2022: 0x223ef60; original cards/modern_horizons_2.c:6089. */
int card_sol_talisman(int player, int card, event_t event)
{
  /* Sol Talisman	""
   * Artifact
   * Suspend 3-|1 (Rather than cast this card from your hand, pay |1 and exile it with three time counters on it. At the beginning of your upkeep, remove a time counter. When the last is removed, you may cast it without paying its mana cost.)
   * |T: Add |C|C. */

  if (IS_GAA_EVENT(event) || event == EVENT_COUNT_MANA)
    return card_sol_ring(player, card, event);
  return suspend_only(player, card, event, 3, 1, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x223f000; original cards/modern_horizons_2.c:6102. */
int card_solitude(int player, int card, event_t event)
{
  /* Solitude	|3|W|W
   * Creature - Elemental Incarnation 3/2
   * Flash
   * Lifelink
   * When this creature enters, exile up to one other target creature. That creature's controller gains life equal to its power.
   * Evoke-Exile a white card from your hand. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event != EVENT_CAN_CAST)
  {
    lifelink(player, card, event);
    etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (event == EVENT_ETB)
    {

      default_target_definition(player, card, &td, 2);
      td.allow_cancel = 2;
      instance = get_card_instance(player, card);
      result = can_target(&td);
      if (result != 0)
      {
        result = new_pick_target(&td, "TARGET_CREATURE", 0, 0);
        if (result != 0)
        {

          result = get_power(instance->targets[0].player, instance->targets[0].card);
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
          gain_life(instance->targets[0].player, result);
        }
      }
      instance->number_of_targets = '\0';
      result = evoked(player, card);
      if (result != 0)
      {
        kill_card(player, card, KILL_SACRIFICE);
      }
    }
    result = evoke_with_cards_in_hand(player, card, event, 0x20);
    return result;
  }
  result = flash(player, card, EVENT_CAN_CAST);
  return result;
}

/* ManalinkEh 2022: 0x223f180; original cards/modern_horizons_2.c:6138. */
int card_soul_of_migration(int player, int card, event_t event)
{
  /* Soul of Migration	|5|W|W
   * Creature - Elemental 2/4
   * Flying
   * When this creature enters, create two 1/1 white Bird creature tokens with flying.
   * Evoke |3|W (You may cast this spell for its evoke cost. If you do, it's sacrificed when it enters.) */

  int result;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    generate_tokens_by_id(player, card, CARD_ID_BIRD, 2);
    result = evoked(player, card);
    if (result != 0)
    {
      kill_card(player, card, KILL_SACRIFICE);
    }
  }
  result = evoke(player, card, event, 3, 0, 0, 0, 0, 1);
  return result;
}

/* ManalinkEh 2022: 0x223f250; original cards/modern_horizons_2.c:6156. */
int card_specimen_collector(int player, int card, event_t event)
{
  /* Specimen Collector	|4|U
   * Creature - Vedalken Wizard 2/1
   * When this creature enters, create a 1/1 green Squirrel creature token and a 0/3 blue Crab creature token.
   * When this creature dies, create a token that's a copy of target token you control. */

  int result;
  target_t tgt;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    generate_token_by_id(player, card, CARD_ID_SQUIRREL);
    generate_token_by_id(player, card, CARD_ID_CRAB);
  }
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    default_target_definition(player, card, &td, 0x1000);
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.allow_cancel = 0;
    result = can_target(&td);
    if (result != 0)
    {
      result = select_target(player, card + -1000, &td, "Select target token you control.", &tgt);
      if (result != 0)
      {
        copy_token(player, card, tgt.player, tgt.card);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x223f360; original cards/modern_horizons_2.c:6188. */
int card_squirrel_sanctuary(int player, int card, event_t event)
{
  /* Squirrel Sanctuary	|G
   * Enchantment
   * When this enchantment enters, create a 1/1 green Squirrel creature token.
   * Whenever a nontoken creature you control dies, you may pay |1. If you do, return this enchantment to its owner's hand. */

  int result;
  unsigned int deaths;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    generate_token_by_id(player, card, CARD_ID_SQUIRREL);
  }
  else if (((event == EVENT_GRAVEYARD_FROM_PLAY) && (affected_card_controller == player)) &&
           (result = is_token(player, affected_card), result == 0))
  {
    count_for_gfp_ability(player, card, 0x77, player, 2, NULL);
  }
  result = duh_mode(player);
  deaths = resolve_gfp_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (deaths != 0)
  {

    deaths = deaths & 0xff;
    while (true)
    {

      if (((deaths == 0) || (result = has_mana(player, 7, 1), result == 0)) ||
          (result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
                                "Return Squirrel Sanctuary to its owner's hand", 1, 10, "Decline",
                                1, 5, 0, 0, 0, 0),
           result != 1))
        return global_enchantment(player, card, event);
      result =
          charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1);
      if (result != 0)
        break;
      deaths = deaths - 1;
    }
    bounce_permanent(player, card);
  }
  return global_enchantment(player, card, event);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x223f570; original cards/modern_horizons_2.c:6228. */
int card_squirrel_sovereign(int player, int card, event_t event)
{
  /* Squirrel Sovereign	|1|G
   * Creature - Squirrel Noble 2/2
   * Other Squirrels you control get +1/+1. */

  boost_creature_type(player, card, event, SUBTYPE_SQUIRREL, 1, 1, 0, BCT_CONTROLLER_ONLY);
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x223f5c0; original cards/modern_horizons_2.c:6239. */
int card_steel_dromedary(int player, int card, event_t event)
{
  /* Steel Dromedary	|3
   * Artifact Creature - Camel 2/2
   * This creature enters tapped with two +1/+1 counters on it.
   * This creature doesn't untap during your untap step if it has a +1/+1 counter on it.
   * At the beginning of combat on your turn, you may move a +1/+1 counter from this creature onto target creature. */

  comes_into_play_tapped(player, card, event);
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);
  if (event == EVENT_UNTAP)
  {
    if (!affect_me(player, card))
      return 0;
  }
  else if (event != EVENT_CHANGE_TYPE && event != EVENT_BEGIN_COMBAT)
    return 0;
  if (count_counters(player, card, COUNTER_P1_P1))
  {
    does_not_untap(player, card, event);
    if (beginning_of_combat(player, card, event, player))
    {
      target_definition_t td;
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      if (can_target(&td) &&
          new_pick_target(&td, "Select target creature which will get a +1/+1 counter.", 0, 0x80))
      {
        card_instance_t *instance = get_card_instance(player, card);
        move_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                      player, card, COUNTER_P1_P1, 1);
        instance->number_of_targets = 0;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x223f720; original cards/modern_horizons_2.c:6269. */
int card_steelfin_whale(int player, int card, event_t event)
{
  /* Steelfin Whale	|5|U
   * Creature - Whale 3/4
   * Affinity for artifacts (This spell costs |1 less to cast for each artifact you control.)
   * Whenever an artifact you control enters, untap this creature. */

  affinity(player, card, event, 0x40, -1);
  etb_ability_global(player, event, 2, TYPE_ARTIFACT, NULL, player, card);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  untap_card(player, card);
  return 0;
}

/* ManalinkEh 2022: 0x223f7b0; original cards/modern_horizons_2.c:6285. */
int card_step_through(int player, int card, event_t event)
{
  /* Step Through	|3|U|U
   * Sorcery
   * Return two target creatures to their owners' hands.
   * Wizardcycling |2 (|2, Discard this card: Search your library for a Wizard card, reveal it, put it into your hand, then shuffle.) */

  int result;
  test_definition_t test;

  if (1 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    if (event == EVENT_RESOLVE_ACTIVATION_FROM_HAND)
    {
      new_default_test_definition(&test, 0xff, "Select a Wizard card.");
      test.subtype = 0xd6;
      new_global_tutor(player, player, 1, 1, 0, 1, &test);
    }
    result = card_undo(player, card, event);
    return result;
  }
  return cycling(player, card, event, 2, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x223f8a0; original cards/modern_horizons_2.c:6304. */
int card_storm_gods_oracle(int player, int card, event_t event)
{
  /* Storm God's Oracle	|1|U|R
   * Enchantment Creature - Human Shaman 1/3
   * |1: This creature gets +1/-1 until end of turn.
   * When this creature dies, it deals 3 damage to any target. */

  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    if (can_target(&td) && new_pick_target(&td, "TARGET_ANY", 0, 0))
      damage_target0(player, card, 3);
  }
  if (!IS_GAA_EVENT(event))
    return global_enchantment(player, card, event);
  return generic_shade(player, card, event, 0, 1, 0, 0, 0, 0, 0, 1, -1, 0, 0);
}

/* ManalinkEh 2022: 0x223fa00; original cards/modern_horizons_2.c:6327. */
int card_strike_it_rich(int player, int card, event_t event)
{
  /* Strike It Rich	|R
   * Sorcery
   * Create a Treasure token. (It's an artifact with "|T, Sacrifice this token: Add one mana of any color.")
   * Flashback |2|R (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

  if (!IS_GS_EVENT(player, card, event))
    return do_flashback(player, card, event, MANACOST_XR(2, 1));
  if (event == EVENT_RESOLVE_SPELL)
  {
    generate_token_by_id(player, card, CARD_ID_TREASURE);
    kill_card(player, card, get_flashback(player, card) ? KILL_EXILE : KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x223fdd0; original cards/modern_horizons_2.c:6406. */
int card_sudden_edict(int player, int card, event_t event)
{
  /* Sudden Edict	|1|B
   * Instant
   * Split second (As long as this spell is on the stack, players can't cast spells or activate abilities that aren't mana abilities.)
   * Target player sacrifices a creature of their choice. */

  int result;
  target_definition_t td;
  if (event == EVENT_CAN_CAST)
  {
    result = card_diabolic_edict(player, card, EVENT_CAN_CAST);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    add_state(player, card, 0x100000);
    default_target_definition(player, card, &td, 0xff);
    td.zone = TARGET_ZONE_PLAYERS;
    result = can_target(&td);
    if ((result != 0) && (result = new_pick_target(&td, "TARGET_PLAYER", 0, 1), result != 0))
    {
      card_diabolic_edict(player, card, EVENT_RESOLVE_SPELL);
    }
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x223feb0; original cards/modern_horizons_2.c:6427. */
int card_suspend(int player, int card, event_t event)
{
  /* Suspend	|U
   * Instant
   * Exile target creature and put two time counters on it. If it doesn't have suspend, it gains suspend. (At the beginning of its owner's upkeep, they remove a time counter. When the last is removed, they may play it without paying its mana cost. If it's a creature, it has haste.) */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      suspend_a_card(player, card, instance->targets[0].player, instance->targets[0].card, 2);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x2240000; original cards/modern_horizons_2.c:6450. */
int card_svyelun_of_sea_and_sky(int player, int card, event_t event)
{
  /* Svyelun of Sea and Sky	|1|U|U
   * Legendary Creature - Merfolk God 3/4
   * Svyelun has indestructible as long as you control at least two other Merfolk.
   * Whenever Svyelun attacks, draw a card.
   * Other Merfolk you control have ward |1. (Whenever another Merfolk you control becomes the target of a spell or ability an opponent controls, counter it unless that player pays |1.) */

  int merfolk_count;
  int is_merfolk;

  check_legend_rule(player, card, event);
  if (event == EVENT_ABILITIES)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      merfolk_count = count_subtype(player, 0x47, 0x74);
      is_merfolk = has_subtype(player, card, SUBTYPE_MERFOLK);
      if ((int)((is_merfolk != 0) + 2) <= merfolk_count)
      {
        if (is_humiliated(player, card, AL_STATIC) == 0)
        {
          indestructible(player, card, EVENT_ABILITIES);
        }
      }
    }
    ward_granted(player, card, EVENT_ABILITIES, player, -1, TYPE_PERMANENT, SUBTYPE_MERFOLK, 2, 1,
                 0, 0, 0, 0, 0, 0);
    dat_ability(player, card, EVENT_ABILITIES, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
    return 0;
  }
  ward_granted(player, card, event, player, -1, TYPE_PERMANENT, SUBTYPE_MERFOLK, 2, 1, 0, 0, 0, 0,
               0, 0);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }
  draw_cards(player, 1);
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2240230; original cards/modern_horizons_2.c:6473. */
int card_sweep_the_skies(int player, int card, event_t event)
{
  /* Sweep the Skies	|X|U|U
   * Sorcery
   * Converge - Create a 1/1 colorless Thopter artifact creature token with flying for each color of mana spent to cast this spell. */

  card_instance_t *instance;

  store_num_colors_paid_in_eot_toughness0(player, card, event);
  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    generate_tokens_by_id(player, card, CARD_ID_THOPTER, (unsigned int)instance->eot_toughness0);
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
}

/* ManalinkEh 2022: 0x222b780; original cards/modern_horizons_2.c:6494. */
static char *target_you_own(int who_chooses, int player, int card, int targeting_player,
                            int targeting_card)
{
  int result;
  char *error;
  result = get_owner(player, card);
  error = NULL;
  if (result != targeting_player)
  {
    error = "owner";
  }
  return error;
}

/* ManalinkEh 2022: 0x2240320; original cards/modern_horizons_2.c:6497. */
int card_sword_of_hearth_and_home(int player, int card, event_t event)
{
  /* Sword of Hearth and Home	|3
   * Artifact - Equipment
   * Equipped creature gets +2/+2 and has protection from green and from white.
   * Whenever equipped creature deals combat damage to a player, exile up to one target creature you own, then search your library for a basic land card. Put both cards onto the battlefield under your control, then shuffle.
   * Equip |2 */

  unsigned int first_protection;
  unsigned int second_protection;
  int result;
  card_instance_t *instance;
  int iid;
  int card_00;
  int damage_triggers;
  target_definition_t td;
  test_definition_t this_test;

  damage_triggers = equipped_creature_deals_damage(
      player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE);
  if (damage_triggers != 0)
  {

    default_target_definition(player, card, &td, 2);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (int32_t)target_you_own;
    td.allow_cancel = 2;
    new_default_test_definition(&this_test, 1, "Select a basic land card.");
    this_test.subtype = 0x2000;
    instance = get_card_instance(player, card);
    if (0 < damage_triggers)
    {
      do
      {
        instance->number_of_targets = '\0';
        result = can_target(&td);
        if ((result == 0) ||
            (result = new_pick_target(&td, "Select up to one target creature you own.", 0, 0x80),
             result == 0))
        {

          result = new_global_tutor(player, player, 1, 1, 0, 5, &this_test);
        }
        else
        {
          iid =
              get_original_internal_card_id(instance->targets[0].player, instance->targets[0].card);
          action_on_target(player, card, 0, ACT_KILL_REMOVE);
          result = new_global_tutor(player, player, 1, 1, 0, 5, &this_test);
          if (-1 < iid)
          {
            card_00 = add_card_to_hand(player, iid);
            remove_card_from_rfg(player, iid);
            set_special_flags(player, card_00, -0x80000000);
            if (-1 < card_00)
            {
              put_into_play(player, card_00);
            }
          }
        }
        if (-1 < result)
        {
          put_into_play(player, result);
        }
        damage_triggers = damage_triggers + -1;
      } while (damage_triggers != 0);
    }
  }
  first_protection = get_sleighted_protection(player, card, 0x8000);
  second_protection = get_sleighted_protection(player, card, 0x2000);
  return vanilla_equipment(player, card, event, 2, 2, 2, first_protection | second_protection, 0);
}

/* ManalinkEh 2022: 0x22405d0; original cards/modern_horizons_2.c:6546. */
int card_sylvan_anthem(int player, int card, event_t event)
{
  /* Sylvan Anthem	|G|G
   * Enchantment
   * Green creatures you control get +1/+1.
   * Whenever a green creature you control enters, scry 1. */

  int result;
  test_definition_t this_test;
  boost_creature_by_color(player, card, event, 8, 1, 1, 0, BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&this_test, 2);
    this_test.color = get_sleighted_color_test(player, card, 8);
    etb_ability_global(player, event, 2, TYPE_NONE, &this_test, player, card);
  }
  if (event == EVENT_ETB)
  {
    scry(player, 1);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x22406e0; original cards/modern_horizons_2.c:6568. */
int card_sythis_harvests_hand(int player, int card, event_t event)
{
  /* Sythis, Harvest's Hand	|G|W
   * Legendary Enchantment Creature - Nymph 1/2
   * Whenever you cast an enchantment spell, you gain 1 life and draw a card. */

  int result;

  check_legend_rule(player, card, event);
  sp_ability_trigger(player, event, 2, TYPE_ENCHANTMENT, NULL, player, card);
  if (event == EVENT_SPA)
  {
    draw_cards(player, 1);
    gain_life(player, 1);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2240780; original cards/modern_horizons_2.c:6593. */
int card_tavern_scoundrel(int player, int card, event_t event)
{
  /* Tavern Scoundrel	|1|R
   * Creature - Human Rogue 1/3
   * Whenever you win a coin flip, create two Treasure tokens. (They're artifacts with "|T, Sacrifice this token: Add one mana of any color.")
   * |1, |T, Sacrifice another permanent: Flip a coin. */

  int result;
  card_instance_t *instance;
  test_definition_t test;

  if (event == EVENT_COIN_FLIP_WON_OR_LOST)
  {
    if (affected_card == 1)
    {
      generate_tokens_by_id(player, card, CARD_ID_TREASURE, 2);
    }
  }
  else if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
            (event == EVENT_RESOLVE_ACTIVATION)) ||
           (event == EVENT_CLEANUP))
  {
    new_default_test_definition(&test, 0x47, "Select another permanent to sacrifice.");
    test.not_me = 1;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      instance = get_card_instance(player, card);
      flip_a_coin(instance->parent_controller, instance->parent_card);
    }
    result = generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED, 1,
                                                              0, 0, 0, 0, 0, 0, NULL, NULL,
                                                              TYPE_NONE, &test, SARV_0);
    return result;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22408e0; original cards/modern_horizons_2.c:6620. */
int card_terminal_agony(int player, int card, event_t event)
{
  /* Terminal Agony	|2|B|R
   * Sorcery
   * Destroy target creature.
   * Madness |B|R (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

  int result;

  if (1 < (uint32_t)(event - EVENT_CAN_PAY_MADNESS_COST))
  {
    result = card_murder2(player, card, event);
    return result;
  }
  return madness(player, card, event, 0, 1, 0, 0, 1, 0);
}

/* ManalinkEh 2022: 0x2240950; original cards/modern_horizons_2.c:6632. */
int card_terramorph2(int player, int card, event_t event)
{
  /* Terramorph	|3|G
   * Sorcery
   * Search your library for a basic land card, put it onto the battlefield, then shuffle.
   * Rebound (If you cast this spell from your hand, exile it as it resolves. At the beginning of your next upkeep, you may cast this card from exile without paying its mana cost.) */

  int result;

  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = count_subtype(player, 1, -1);
      ai_modifier = ai_modifier + (6 - result) * 8;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    tutor_basic_land(player, 1, 0);
    rebound(player, card);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x2240a00; original cards/modern_horizons_2.c:6650. */
int card_territorial_kavu(int player, int card, event_t event)
{
  /* Territorial Kavu	|R|G
   * Creature - Kavu * / *
   * Domain - ~'s power and toughness are each equal to the number of basic land types among lands you control.
   * Whenever this creature attacks, choose one -
   * * Discard a card. If you do, draw a card.
   * * Exile up to one target card from a graveyard. */

  int choice;
  int count_or_result;
  int hand_count_or_grave_index;
  card_instance_t *instance;
  int opponent;
  int32_t targ_player;
  unsigned int grave_available;

  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      set_base_pt(count_domain(player, card));
    }
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
    return 0;
  }
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }

  hand_count_or_grave_index = hand_count[player];
  count_or_result = count_graveyard(player);
  opponent = 1 - player;
  grave_available = 0;
  if (count_or_result != 0 && !graveyard_has_shroud(player))
    grave_available = 1;
  else
  {
    count_or_result = count_graveyard(opponent);
    if (count_or_result != 0)
      grave_available = graveyard_has_shroud(opponent) == 0;
  }
  count_or_result = count_graveyard(opponent);
  choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL,
                       "Discard & draw", (unsigned int)(0 < hand_count_or_grave_index), 10,
                       "Exile a card in a graveyard", grave_available,
                       ((0x13 < count_or_result) ? 15U : 5U), 0, 0, 0, 0);
  if (choice == 1)
  {
    if (0 < hand_count[player])
    {
      discard(player, 0, player);
      draw_cards(player, 1);
    }
  }
  else if (choice == 2)
  {
    test_definition_t test;

    hand_count_or_grave_index = count_graveyard(0);
    grave_available = 0;
    if (hand_count_or_grave_index != 0)
    {
      hand_count_or_grave_index = graveyard_has_shroud(0);
      grave_available = (unsigned int)(hand_count_or_grave_index == 0);
    }
    hand_count_or_grave_index = count_graveyard(1);
    if (((hand_count_or_grave_index == 0) ||
         (hand_count_or_grave_index = graveyard_has_shroud(1), hand_count_or_grave_index != 0)) ||
        (grave_available = grave_available | 2, grave_available != 3))
    {
      targ_player = (int)grave_available >> 1;
    }
    else
    {
      target_definition_t td1;

      default_target_definition(player, card, &td1, 2);
      td1.zone = TARGET_ZONE_PLAYERS;
      instance = get_card_instance(player, card);
      if (new_pick_target(&td1, "Select the owner of the graveyard you want to look.", 0, 0x80) ==
          0)
      {
        return 0;
      }
      targ_player = instance->targets[0].player;
      instance->number_of_targets = '\0';
    }
    new_default_test_definition(&test, 0xff, "Select a card to exile.");
    hand_count_or_grave_index = new_select_a_card(player, targ_player, 2, 0, 1, -1, &test);
    if (hand_count_or_grave_index != -1)
    {
      rfg_card_from_grave(targ_player, hand_count_or_grave_index);
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2240d80; original cards/modern_horizons_2.c:6709. */
int card_the_underworld_cookbook(int player, int card, event_t event)
{
  /* The Underworld Cookbook	|1
   * Artifact - Book
   * |T, Discard a card: Create a Food token. (It's an artifact with "|2, |T, Sacrifice this token: You gain 3 life.")
   * |4, |T, Sacrifice this artifact: Return target creature card from your graveyard to your hand. */

  int choice;
  card_instance_t *instance;
  int food_available_or_grave_index;
  unsigned int food_available;
  unsigned int return_available;
  test_definition_t test;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_test_definition(&test, 2);
    if (event == EVENT_CAN_ACTIVATE)
    {
      food_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x21, 0, 0, 0, 0,
                                                 0, 0, 0, NULL, NULL);
      return_available = generic_activated_ability_with_target_in_grave(
          player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_UNTAPPED, 4, 0, 0, 0, 0, 0, 0,
          NULL, NULL, TYPE_CREATURE, &test, 8);
      return return_available | food_available;
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->mode = '\0';
      food_available_or_grave_index = generic_activated_ability(
          player, card, EVENT_CAN_ACTIVATE, 0x21, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_MODE, DLG_AUTOCHOOSE_IF_1,
                           "Create a Food token", food_available_or_grave_index, 2,
                           "Creature from grave to hand",
                           generic_activated_ability_with_target_in_grave(
                               player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_UNTAPPED, 4,
                               0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_CREATURE, &test, 8),
                           1, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice != 1)
      {
        if (choice != 2)
        {
          return 0;
        }
        generic_activated_ability_with_target_in_grave(
            player, card, EVENT_ACTIVATE, GAA_SACRIFICE_ME | GAA_UNTAPPED, 4, 0, 0, 0, 0, 0, 0,
            NULL, NULL, TYPE_CREATURE, &test, 8);
        return 0;
      }
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0x21, 0, 0, 0, 0, 0, 0, 0, NULL,
                                NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      ai_modifier = ai_modifier + (unsigned int)((life[player] < 6) ? 0 : -14) + 0x18;
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
    default_test_definition(&test, 2);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    if (instance->mode == '\x01')
    {
      generate_token_by_id(player, card, CARD_ID_FOOD);
      return 0;
    }

    if ((instance->mode == '\x02') &&
        (food_available_or_grave_index = validate_target_from_grave_source(player, card, player, 0),
         food_available_or_grave_index != -1))
    {

      add_card_to_hand(player, graveyard_ptr_mutable[player][food_available_or_grave_index]);
      remove_card_from_grave(player, food_available_or_grave_index);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2241280; original cards/modern_horizons_2.c:6774. */
int card_thought_monitor(int player, int card, event_t event)
{
  /* Thought Monitor	|6|U
   * Artifact Creature - Construct 2/2
   * Affinity for artifacts (This spell costs |1 less to cast for each artifact you control.)
   * Flying
   * When this creature enters, draw two cards. */

  affinity(player, card, event, 0x40, -1);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  draw_cards(player, 2);
  return 0;
}

/* ManalinkEh 2022: 0x2241300; original cards/modern_horizons_2.c:6793. */
int card_thraben_watcher(int player, int card, event_t event)
{
  /* Thraben Watcher	|2|W|W
   * Creature - Angel 2/2
   * Flying, vigilance
   * Other nontoken creatures you control get +1/+1 and have vigilance. */

  card_instance_t *instance;
  int result;

  vigilance(player, card, event);
  if ((uint32_t)(event - EVENT_POWER) < 3)
  {
    instance = in_play(player, card);
    if ((instance != NULL) && (affected_card_controller == player))
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = is_token(affected_card_controller, affected_card);
        if (result == 0)
        {
          if ((event == EVENT_POWER) || (event == EVENT_TOUGHNESS))
          {
            event_result = event_result + 1;
          }
          else if (event == EVENT_ABILITIES)
          {
            vigilance(affected_card_controller, affected_card, EVENT_ABILITIES);
          }
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22413d0; original cards/modern_horizons_2.c:6815. */
int card_thrasta_tempests_roar(int player, int card, event_t event)
{
  /* Thrasta, Tempest's Roar	|10|G|G
   * Legendary Creature - Dinosaur 7/7
   * This spell costs |3 less to cast for each other spell cast this turn.
   * Trample, haste
   * Trample over planeswalkers (This creature can deal excess combat damage to the controller of the planeswalker it's attacking.)
   * Thrasta has hexproof as long as it entered this turn. */

  int storm_count;
  card_instance_t *instance;
  inst_info_t entry_record;

  check_legend_rule(player, card, event);
  haste(player, card, event);
  if (event == EVENT_MODIFY_COST)
  {
    storm_count = get_storm_count();
    COST_COLORLESS = COST_COLORLESS + (char)storm_count * -3;
    trample_over_planeswalkers(player, card, EVENT_MODIFY_COST);
    return 0;
  }
  trample_over_planeswalkers(player, card, event);
  if (event != EVENT_RESOLVE_SPELL)
  {
    if (event == EVENT_ABILITIES)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance = get_card_instance(player, card);
        entry_record = inst_info(instance, 0x5a8f);
        if (entry_record.data2 == 0x42)
        {
          hexproof(player, card, EVENT_ABILITIES);
        }
      }
    }
    else if (event == EVENT_CLEANUP)
    {
      instance = get_card_instance(player, card);
      clear_info(instance, 0x5a8f);
      return 0;
    }
    return 0;
  }
  instance = get_card_instance(player, card);
  set_inst_info(instance, 0x5a8f, 0, 0x42);
  return 0;
}

/* ManalinkEh 2022: 0x222bc00; original cards/modern_horizons_2.c:6845. */
static int tide_shaper_effect(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;

  if (event == EVENT_CHANGE_TYPE)
  {

    instance = get_card_instance(player, card);
    if ((instance->damage_target_card == affected_card) &&
        (instance->damage_target_player == affected_card_controller))
    {
      result = get_hacked_subtype(player, card, 0x5002);
      event_result = land_into_new_basic_land_type(event_result, result);
    }
  }
  else if (event == EVENT_STATIC_EFFECTS)
  {

    instance = get_card_instance(player, card);
    result = instance->targets[0].player;
    if (-1 < result)
    {
      instance = in_play(result, instance->targets[0].card);
      if (instance == NULL)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2241530; original cards/modern_horizons_2.c:6863. */
int card_tide_shaper(int player, int card, event_t event)
{
  /* Tide Shaper	|U
   * Creature - Merfolk Wizard 1/1
   * Kicker |1 (You may pay an additional |1 as you cast this spell.)
   * When this creature enters, if it was kicked, target land becomes an Island for as long as this creature remains on the battlefield.
   * This creature gets +1/+1 as long as an opponent controls an Island. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  kicker(player, card, event, 1, 0, 0, 0, 0, 0);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    result = kicked(player, card);
    if (result != 0)
    {
      default_target_definition(player, card, &td, 1);
      if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
      {
        td.required_subtype = get_hacked_subtype(player, card, 0x5002);
        td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
      }
      td.allow_cancel = 0;
      result = can_target(&td);
      if (result != 0)
      {
        result = new_pick_target(&td, "TARGET_LAND", 0, 0);
        if (result != 0)
        {

          instance = get_card_instance(player, card);
          result = create_targetted_legacy_effect(player, card, tide_shaper_effect,
                                                  instance->targets[0].player,
                                                  instance->targets[0].card);
          instance = get_card_instance(player, result);
          instance->targets[0].player = player;
          instance->targets[0].card = card;
          instance->number_of_targets = '\x01';
        }
      }
    }
  }
  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((((result == 0) && ((uint32_t)(event - EVENT_POWER) < 2)) && (card == affected_card)) &&
        (player == affected_card_controller))
    {
      result = check_battlefield_for_subtype(1 - player, 0x47, 0x5002);
      if (result != 0)
      {
        event_result = event_result + 1;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22418c0; original cards/modern_horizons_2.c:6936. */
int card_tireless_provisioner(int player, int card, event_t event)
{
  /* Tireless Provisioner	|2|G
   * Creature - Elf Scout 3/2
   * Landfall - Whenever a land you control enters, create a Food token or a Treasure token. (Food is an artifact with "|2, |T, Sacrifice this token: You gain 3 life." Treasure is an artifact with "|T, Sacrifice this token: Add one mana of any color.") */

  int result;

  landfall(player, card, event);
  if (event == EVENT_ETB)
  {

    result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
                         "Food token", 1, ((life[player] < 6) ? 30U : 10U),
                         "Treasure token", 1, 0x14, 0, 0, 0, 0);
    generate_token_by_id(player, card, ((result == 1) ? 20599U : 18840U));
  }
  return 0;
}

/* ManalinkEh 2022: 0x22419b0; original cards/modern_horizons_2.c:6955. */
int card_tizerus_charger(int player, int card, event_t event)
{
  /* Tizerus Charger	|2|B
   * Creature - Pegasus 3/2
   * Escape-|4|B, Exile five other cards from your graveyard. (You may cast this card from your graveyard for its escape cost.)
   * This creature escapes with your choice of a +1/+1 counter or a flying counter on it. */

  int result;

  if ((((event == EVENT_CAST_SPELL) || (event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE)) &&
       (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = check_special_flags3(player, card, 0x80000);
    if (result != 0)
    {

      result = dialog_impl(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL, "+1/+1 counter",
                           1, 10, "Flying counter", 1, 0x14, 0, 0, 0, 0);
      if (result == 1)
      {
        enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 1);
      }
      else if (result == 2)
      {
        enters_the_battlefield_with_counters(player, card, event, COUNTER_FLYING, 1);
      }
    }
  }
  result = escape(player, card, event, 4, 1, 0, 0, 0, 0, 5);
  return result;
}

/* ManalinkEh 2022: 0x2241b20; original cards/modern_horizons_2.c:6976. */
int card_tormods_cryptkeeper(int player, int card, event_t event)
{
  /* Tormod's Cryptkeeper	|3
   * Artifact Creature - Golem 3/2
   * Vigilance
   * |T, Sacrifice this creature: Exile target player's graveyard. */

  vigilance(player, card, event);
  if (((event != EVENT_ACTIVATE) && (1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION))) &&
      (event != EVENT_CLEANUP))
  {
    return 0;
  }
  return card_tormods_crypt(player, card, event);
}

/* ManalinkEh 2022: 0x2241b80; original cards/modern_horizons_2.c:6989. */
int card_tourachs_canticle(int player, int card, event_t event)
{
  /* Tourach's Canticle	|3|B
   * Sorcery
   * Target opponent reveals their hand. You choose a card from it. That player discards that card, then discards a card at random. */

  card_instance_t *instance;
  ec_definition_t ec;
  target_definition_t td;
  test_definition_t this_test;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.zone = TARGET_ZONE_PLAYERS;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      default_ec_definition(instance->targets[0].player, player, &ec);
      new_default_test_definition(&this_test, 0xff, "Select a card to discard.");
      new_effect_coercion(&ec, &this_test);
      discard(1 - player, 3, player);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x40, &td, NULL, 1, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2241d50; original cards/modern_horizons_2.c:7021. */
int card_tourach_dread_cantor(int player, int card, event_t event)
{
  /* Tourach, Dread Cantor	|1|B
   * Legendary Creature - Human Cleric 2/1
   * Kicker |B|B (You may pay an additional |B|B as you cast this spell.)
   * Protection from white
   * Whenever an opponent discards a card, put a +1/+1 counter on Tourach.
   * When Tourach enters, if it was kicked, target opponent discards two cards at random. */

  int result;

  check_legend_rule(player, card, event);
  kicker(player, card, event, 0, 2, 0, 0, 0, 0);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    result = kicked(player, card);
    if (result != 0)
    {
      result = opponent_is_valid_target(player, card);
      if (result != 0)
      {
        new_multidiscard(1 - player, 2, 3, player);
      }
    }
  }
  if (((trigger_condition == TRIGGER_DISCARD) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = discard_trigger(player, card, event, 1 - player, 2, 0);
    if (result != 0)
    {
      add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2241ea0; original cards/modern_horizons_2.c:7047. */
int card_tragic_fall(int player, int card, event_t event)
{
  /* Tragic Fall	|1|B
   * Instant
   * Target creature gets -3/-3 until end of turn.
   * Hellbent - That creature gets -13/-13 until end of turn instead if you have no cards in hand. */

  int result;
  result = (unsigned int)((hand_count[player] == 0) ? -13 : -3);
  result = vanilla_instant_pump(player, card, event, 2, 1 - player, result, result, 0, 0);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2241f00; original cards/modern_horizons_2.c:7057. */
int card_unbounded_potential(int player, int card, event_t event)
{
  /* Unbounded Potential	|1|W
   * Instant
   * Choose one -
   * * Put a +1/+1 counter on each of up to two target creatures.
   * * Proliferate. (Choose any number of permanents and/or players, then give each another counter of each kind already there.)
   * Entwine |3|W (Choose both if you pay the entwine cost.) */

  int i;
  unsigned char selected_mode;
  card_instance_t *instance;
  int result_or_choice;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    get_card_instance(player, card);
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        result_or_choice = check_special_flags2(player, card, -0x80000000);
        if (result_or_choice == 0)
        {

          instance->mode = '\0';
          result_or_choice = dialog_impl(
              player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_MODE,
              "Put a +1/+1 counter on each of up to two target creatures", 1, 0x14, "Proliferate",
              1, 10, "Entwine", has_mana_multi(player, 3, 0, 0, 0, 0, 1), 0x1e, 0, 0, 0, 0);
          if (result_or_choice == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
          if ((result_or_choice == 3) &&
              (charge_mana_multi(player, 3, 0, 0, 0, 0, 1), spell_fizzled == 1))
          {
            return 0;
          }
        }
        if (((instance->mode & 1) != 0) &&
            (result_or_choice = new_pick_target(&td, "TARGET_CREATURE", 0, 0),
             result_or_choice != 0))
        {
          state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
          new_pick_target(&td, "TARGET_CREATURE", 1, 0);
          state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      selected_mode = instance->mode;

      if (((selected_mode & 1) != 0) && (instance->number_of_targets != '\0'))
      {
        i = 0;
        do
        {
          if (validate_target(player, card, &td, i) == 0)
          {
            i = i + 1;
            continue;
          }
          add_counters(player, card, instance->targets[i].player, instance->targets[i].card,
                       COUNTER_P1_P1, 1);
          i = i + 1;
        } while (i < (int)(unsigned int)instance->number_of_targets);
        selected_mode = instance->mode;
      }
      if ((selected_mode & 2) != 0)
      {
        proliferate(player, card);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    get_card_instance(player, card);
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2242300; original cards/modern_horizons_2.c:7126. */
int card_underworld_hermit(int player, int card, event_t event)
{
  /* Underworld Hermit	|4|B|B
   * Creature - Human Peasant 3/3
   * When this creature enters, create a number of 1/1 green Squirrel creature tokens equal to your devotion to black. (Each |B in the mana costs of permanents you control counts toward your devotion to black.) */

  int howmany;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  howmany = devotion(player, card, 1, 0);
  generate_tokens_by_id(player, card, CARD_ID_SQUIRREL, howmany);
  return 0;
}

/* ManalinkEh 2022: 0x2242380; original cards/modern_horizons_2.c:7138. */
int card_unholy_heat(int player, int card, event_t event)
{
  /* Unholy Heat	|R
   * Instant
   * ~ deals 2 damage to target creature or planeswalker.
   * Delirium - ~ deals 6 damage instead if there are four or more card types among cards in your graveyard. */

  if (!IS_GS_EVENT(player, card, event))
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, 0x102);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      int amount = delirium(player) ? 6 : 2;
      damage_target0(player, card, amount);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE_OR_PLANESWALKER", 1, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
void devour_type(int player, int card, event_t event, int type, int number);

/* ManalinkEh 2022: 0x22424c0; original cards/modern_horizons_2.c:7162. */
int card_unmarked_grave(int player, int card, event_t event)
{
  /* Unmarked Grave	|1|B
   * Sorcery
   * Search your library for a nonlegendary card, put that card into your graveyard, then shuffle. */

  int result;
  test_definition_t this_test;
  if (event == EVENT_CAN_CAST)
  {
    result = basic_spell(player, card, EVENT_CAN_CAST);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      ai_modifier = ai_modifier + 0x60;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    new_default_test_definition(&this_test, 0xff,
                                "Select a nonlegendary card to put into your graveyard.");
    this_test.subtype = 0x2001;
    this_test.subtype_flag = 1;
    new_global_tutor(player, player, 1, 3, 0, 6, &this_test);
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22425d0; original cards/modern_horizons_2.c:7188. */
int card_urban_daggertooth(int player, int card, event_t event)
{
  /* Urban Daggertooth	|2|G|G
   * Creature - Dinosaur 4/3
   * Vigilance
   * Enrage - Whenever this creature is dealt damage, proliferate. (Choose any number of permanents and/or players, then give each another counter of each kind already there.) */

  int result;

  vigilance(player, card, event);
  result = damage_dealt_to_me_arbitrary(player, card, event, DDBM_0, player, card);
  if (result == 0)
  {
    return 0;
  }
  proliferate(player, card);
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2242cb0; original cards/modern_horizons_2.c:7323. */
int card_vedalken_infiltrator(int player, int card, event_t event)
{
  /* Vedalken Infiltrator	|1|U
   * Creature - Vedalken Rogue 1/3
   * This creature can't be blocked.
   * Metalcraft - This creature gets +1/+0 as long as you control three or more artifacts. */

  int result;
  card_instance_t *instance;

  unblockable(player, card, event);
  if (((event == EVENT_POWER) && (card == affected_card)) && (player == affected_card_controller))
  {
    result = metalcraft(player, card);
    if (result != 0)
    {
      instance = in_play(player, card);
      if (instance != NULL)
      {
        event_result = event_result + 1;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22435b0; original cards/modern_horizons_2.c:7497. */
int card_vermin_gorger(int player, int card, event_t event)
{
  /* Vermin Gorger	|1|B
   * Creature - Vampire 2/2
   * |T, Sacrifice another creature: Each opponent loses 2 life and you gain 2 life. */

  int result;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    lose_life(1 - player, 2);
    gain_life(player, 2);
  }
  result = generic_activated_ability(player, card, event, 0x40201, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2243660; original cards/modern_horizons_2.c:7511. */
int card_viashino_lashclaw(int player, int card, event_t event)
{
  /* Viashino Lashclaw	|1|R
   * Creature - Lizard Warrior 2/2
   * |T, Discard a card: Creatures you control gain haste until end of turn. */

  int result;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    pump_creatures_until_eot_no_repeat(player, card, player, 0, 0, 4, NULL);
  }
  result = generic_activated_ability(player, card, event, 0x21, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2243720; original cards/modern_horizons_2.c:7523. */
int card_vile_entomber(int player, int card, event_t event)
{
  /* Vile Entomber	|2|B|B
   * Creature - Zombie Warlock 2/2
   * Deathtouch (Any amount of damage this deals to a creature is enough to destroy it.)
   * When this creature enters, search your library for a card, put that card into your graveyard, then shuffle. */

  test_definition_t this_test;
  deathtouch(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  new_default_test_definition(&this_test, 0xff, "Select a card to put into your graveyard.");
  new_global_tutor(player, player, 1, 3, 0, 6, &this_test);
  return 0;
}

/* ManalinkEh 2022: 0x22437e0; original cards/modern_horizons_2.c:7547. */
int card_wavesifter(int player, int card, event_t event)
{
  /* Wavesifter	|3|G|U
   * Creature - Elemental 3/2
   * Flying
   * When this creature enters, investigate twice. (To investigate, create a Clue token. It's an artifact with "|2, Sacrifice this token: Draw a card.")
   * Evoke |G|U (You may cast this spell for its evoke cost. If you do, it's sacrificed when it enters.) */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    investigate(player, card, player);
    investigate(player, card, player);
    result = evoked(player, card);
    if (result != 0)
    {
      kill_card(player, card, KILL_SACRIFICE);
    }
  }
  result = evoke(player, card, event, 0, 0, 1, 1, 0, 0);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2243980; original cards/modern_horizons_2.c:7581. */
int card_wrens_run_hydra(int player, int card, event_t event)
{
  /* Wren's Run Hydra	|X|G
   * Creature - Hydra 0/0
   * Reach
   * This creature enters with X +1/+1 counters on it.
   * Reinforce X-|X|G|G (|X|G|G, Discard this card: Put X +1/+1 counters on target creature.) */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = card_ivy_elemental(player, card, event);
    return result;
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE_FROM_HAND)
  {
    result = has_mana_multi(player, 0, 0, 0, 2, 0, 0);
    if (result != 0)
    {
      result = can_target(&td);
      return result;
    }
  }
  else if (event == EVENT_ACTIVATE_FROM_HAND)
  {
    charge_mana_multi(player, -1, 0, 0, 2, 0, 0);
    if ((spell_fizzled != 1) && (result = pick_target(&td, "TARGET_CREATURE"), result != 0))
    {
      discard_card(player, card);
      return 1;
    }
  }
  else if ((event == EVENT_RESOLVE_ACTIVATION_FROM_HAND) && (spell_fizzled != 1))
  {
    add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                             COUNTER_P1_P1, x_value);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2243b30; original cards/modern_horizons_2.c:7619. */
int card_yavimaya_cradle_of_growth(int player, int card, event_t event)
{
  /* Yavimaya, Cradle of Growth	""
   * Legendary Land
   * Each land is a Forest in addition to its other land types. */

  int result;

  check_legend_rule(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {
    play_land_sound_effect(player, card);
  }
  all_lands_are_basiclandtype(player, card, event, 2, 3, 0x5001);
  result = plain_mana_producer(player, card, event);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2243bb0; original cards/modern_horizons_2.c:7635. */
int card_young_necromancer(int player, int card, event_t event)
{
  /* Young Necromancer	|4|B
   * Creature - Human Warlock 2/3
   * When this creature enters, you may exile two cards from your graveyard. When you do, return target creature card from your graveyard to the battlefield. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card || player != affected_card_controller)
      return 0;
    if (count_graveyard(player) > 2 && count_graveyard_by_type(player, TYPE_CREATURE) &&
        !graveyard_has_shroud(player))
      return 0;
    ai_modifier -= 0x30;
    return 0;
  }
  if (event != EVENT_ETB)
    return 0;
  int grave_count = count_graveyard(player);
  if (grave_count < 3 && (player == 1 || ai_is_speculating == 1))
    return 0;
  int creatures = count_graveyard_by_type(player, TYPE_CREATURE);
  if (!creatures && (player == 1 || ai_is_speculating == 1))
    return 0;
  int shrouded = graveyard_has_shroud(player);
  if (shrouded && (player == 1 || ai_is_speculating == 1))
    return 0;
  test_definition_t this_test;
  new_default_test_definition(&this_test, 0xff, "Select a card to exile");
  target_t exiled[2];
  select_multiple_cards_from_graveyard_to_array(player, player, -1, 3, &this_test, 2, exiled);
  if (spell_fizzled != 1)
  {
    rfg_card_from_grave(player, exiled[0].player);
    rfg_card_from_grave(player, exiled[1].player);
    if (count_graveyard_by_type(player, TYPE_CREATURE) && !graveyard_has_shroud(player))
    {
      new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
      new_global_tutor(player, player, 2, 4, 1, 1, &this_test);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x222b840; original cards/modern_horizons_2.c:7671. */
int yusri_effect(int player, int card, event_t event)
{
  int result;

  if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }
  result = card_omniscience(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2243df0; original cards/modern_horizons_2.c:7678. */
int card_yusri_fortunes_flame(int player, int card, event_t event)
{
  /* Yusri, Fortune's Flame	|1|U|R
   * Legendary Creature - Efreet 2/3
   * Flying
   * Whenever Yusri attacks, choose a number between 1 and 5. Flip that many coins. For each flip you win, draw a card. For each flip you lose, Yusri deals 2 damage to you. If you won five flips this way, you may cast spells from your hand this turn without paying their mana costs. */

  int result;
  unsigned int remaining;
  int suggested_flips_or_wins;

  check_legend_rule(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {

    if (life[player] < 0x10)
    {
      suggested_flips_or_wins = 4 - (0xfU - life[player] >> 1);
      if (suggested_flips_or_wins < 1)
      {
        suggested_flips_or_wins = 1;
      }
    }
    else
    {
      suggested_flips_or_wins = 5;
    }
    do
    {
      result = choose_a_number(player, "How many coins you\'ll flip?", suggested_flips_or_wins);
      remaining = result - 1;
    } while (4 < remaining);
    suggested_flips_or_wins = 0;
    while (true)
    {
      result = flip_a_coin(player, card);
      if (result == 0)
      {
        damage_player(player, 2, player, card);
      }
      else
      {
        draw_cards(player, 1);
        suggested_flips_or_wins = suggested_flips_or_wins + 1;
      }
      if (remaining == 0)
        break;
      remaining = remaining - 1;
    }
    if (suggested_flips_or_wins == 5)
    {
      create_legacy_activate(player, card, yusri_effect);
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2243f30; original cards/modern_horizons_2.c:7713. */
int card_zabaz_the_glimmerwasp(int player, int card, event_t event)
{
  /* Zabaz, the Glimmerwasp	|1
   * Legendary Artifact Creature - Insect 0/0
   * Modular 1
   * If a modular triggered ability would put one or more +1/+1 counters on a creature you control, that many plus one +1/+1 counters are put on it instead.
   * |R: Destroy target artifact you control.
   * |W: Zabaz gains flying until end of turn. */

  int choice;
  card_instance_t *instance;
  int destroy_available_or_flags;
  int flying_available;
  target_definition_t *target;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x40);
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 0, 0, 0, 0, 1, 0, 0, &td,
                                    NULL) != 0)
      {
        return 1;
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 0, 0, 0, 1, 0,
                                       NULL, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->mode = '\0';
      destroy_available_or_flags = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 0,
                                                             0, 0, 0, 1, 0, 0, &td, NULL);
      flying_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 0, 0,
                                                   0, 1, 0, NULL, NULL);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_MODE,
                           "Destroy your artifact", destroy_available_or_flags, 10,
                           "Zabaz gains flying", flying_available,
                           ((current_phase < 0x18) ? 20U : 5U), 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == 1)
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, 6, 0, 0, 0, 0, 1, 0, 0, &td,
                                  "Select target artifact you control.");
        if (spell_fizzled != 1)
        {
          destroy_available_or_flags =
              check_special_flags(instance->targets[0].player, instance->targets[0].card, 0x8000);
          ai_modifier =
              ai_modifier + (unsigned int)((destroy_available_or_flags == 0) ? -48 : 0) + 0x18;
        }
      }
      else if (choice == 2)
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 0, 0, 0, 0, 0, 1, 0, NULL, NULL);
      }
      return modular(player, card, event, 1);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return modular(player, card, event, 1);
    default_target_definition(player, card, &td, 0x40);
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
  }
  target = &td;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    if (instance->mode == '\x01')
    {
      if (valid_target(target) != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
    }
    else if (instance->mode == '\x02')
    {
      pump_ability_until_eot_no_repeat(player, card, instance->parent_controller,
                                       instance->parent_card, 0x20, 0);
    }
  }
  return modular(player, card, event, 1);
}

/* October 2022 copied local table sources (.data, writable). */
static int garth_card_ids[7] = {-1, 64, 25, 242, 224, 202, 17};
static int serras_emissary_types[9] = {0, 2, 64, 4, 1, 256, 16, 8, 4105};

/* ManalinkEh 2022: 0x222c7a0; original cards/modern_horizons_2.c:326. */
int card_aeve_progenitor_ooze(int player, int card, event_t event)
{
  /* Aeve, Progenitor Ooze	|2|G|G|G
   * Legendary Creature - Ooze 2/2
   * Storm (When you cast this spell, copy it for each spell cast before it this turn. Copies become tokens.)
   * Aeve isn't legendary if it's a token.
   * Aeve enters with a +1/+1 counter on it for each other Ooze you control. */

  int c;
  int token_or_flags;
  card_instance_t *instance;
  int is_ooze;
  int ooze_count;

  if (event == EVENT_QUERY)
  {
    if (attacking_card == 4)
    {
      return 1;
    }
    check_legend_rule(player, card, EVENT_QUERY);
  }
  else
  {
    check_legend_rule(player, card, event);
    if (event == EVENT_CHANGE_TYPE)
    {
      if (((card == affected_card) && (player == affected_card_controller)) &&
          (token_or_flags = is_token(player, card), token_or_flags != 0))
      {
        remove_a_subtype(player, card, SUBTYPE_LEGEND);
      }
    }
    else if ((((event == EVENT_THIS_SPELL_CAST) ||
               (event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE)) &&
              (card == affected_card)) &&
             ((player == affected_card_controller &&
               (token_or_flags = check_special_flags2(player, card, 0x100), token_or_flags == 0))))
    {

      if (active_cards_count[player] < 1)
      {
        ooze_count = 0;
      }
      else
      {
        c = 0;
        ooze_count = 0;
        do
        {
          if ((card != c) && (instance = in_play(player, c), instance != NULL))
          {
            is_ooze = has_subtype(player, c, SUBTYPE_OOZE);
            ooze_count = (ooze_count + 1) - (unsigned int)(is_ooze == 0);
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }
      hack_silent_counters = hack_silent_counters + 1;
      add_counters(player, card, player, card, COUNTER_P1_P1, ooze_count);
      hack_silent_counters = hack_silent_counters + -1;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2235c00; original cards/modern_horizons_2.c:3161. */
int card_garth_one_eye(int player, int card, event_t event)
{
  /* Garth One-Eye	|W|U|B|R|G
   * Legendary Creature - Human Wizard 5/5
   * |T: Choose a card name that hasn't been chosen from among Disenchant, Braingeyser, Terror, Shivan Dragon, Regrowth, and Black Lotus. Create a copy of the card with the chosen name. You may cast the copy. (You still pay its costs.) */

  int choice;
  int csvid_or_count;
  card_instance_t *inst;
  card_instance_t *inst_00;
  unsigned int lotus_available;
  unsigned int used_spells;
  int *card_id_cursor;
  unsigned int regrowth_available;
  int *copy_cursor;
  unsigned int disenchant_available;
  inst_info_t used_record;
  unsigned int terror_available;
  unsigned int braingeyser_available;
  unsigned int dragon_available;
  int csvids[7];
  check_legend_rule(player, card, event);
  if (event != EVENT_RESOLVE_ACTIVATION)
    return generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  inst = get_card_instance(player, card);
  inst_00 = get_card_instance(inst->parent_controller, inst->parent_card);
  used_record = inst_info(inst, 0x5a04);
  used_spells = ((uint32_t)used_record.id | ((uint32_t)used_record.data2 << 16)) >> 0x10;
  if ((used_record.data2 & 0x3f) == 0x3f)
  {
    return 0;
  }
  card_id_cursor = garth_card_ids;
  copy_cursor = csvids;
  for (csvid_or_count = 7; csvid_or_count != 0; csvid_or_count = csvid_or_count + -1)
  {
    *copy_cursor = *card_id_cursor;
    card_id_cursor = card_id_cursor + 1u;
    copy_cursor = copy_cursor + 1u;
  }
  disenchant_available =
      (used_spells & 2) == 0 &&
      (player == 0 || has_mana_to_cast_id(player, EVENT_RESOLVE_ACTIVATION, 0x40));
  braingeyser_available =
      (used_spells & 4) == 0 &&
      (player == 0 || has_mana_to_cast_id(player, EVENT_RESOLVE_ACTIVATION, 0x19));
  terror_available = (used_spells & 8) == 0 &&
                     (player == 0 || has_mana_to_cast_id(player, EVENT_RESOLVE_ACTIVATION, 0xf2));
  dragon_available = (used_spells & 0x10) == 0 &&
                     (player == 0 || has_mana_to_cast_id(player, EVENT_RESOLVE_ACTIVATION, 0xe0));
  regrowth_available = (used_spells & 0x20) == 0 &&
                       (player == 0 || has_mana_to_cast_id(player, EVENT_RESOLVE_ACTIVATION, 0xca));
  lotus_available = (used_spells & 0x40) == 0 &&
                    (player == 0 || has_mana_to_cast_id(player, EVENT_RESOLVE_ACTIVATION, 0x11));
  csvid_or_count = count_graveyard(player);
  choice = dialog_impl(player, card, EVENT_RESOLVE_ACTIVATION, DLG_RANDOM, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL,
                       DLG_NO_CANCEL, "Disenchant", disenchant_available, 5, "Braingeyser",
                       braingeyser_available, hand_count[player] * -10 + 0x46, "Terror",
                       terror_available, 0xf, "Shivan Dragon", dragon_available, 0x14, "Regrowth",
                       regrowth_available, csvid_or_count, "Black Lotus", lotus_available, 0x32, 0,
                       0, 0, 0);
  set_inst_info(inst_00, 0x5a04, 0,
                (unsigned short)(1 << ((unsigned char)choice & 0x1f)) | used_record.data2);
  csvid_or_count = csvids[choice];
  if (charge_mana_from_id(player, -1, EVENT_RESOLVE_ACTIVATION, csvid_or_count) != 0)
  {
    copy_spell(player, csvid_or_count);
  }
  return generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x223dfe0; original cards/modern_horizons_2.c:5774. */
int card_serras_emissary(int player, int card, event_t event)
{
  /* Serra's Emissary	|4|W|W|W
   * Creature - Angel 7/7
   * Flying
   * As this creature enters, choose a card type.
   * You and creatures you control have protection from the chosen card type. */

  int choice;
  card_instance_t *instance;
  int result_or_count;
  uint32_t chosen_type;
  int *type_cursor;
  int *copy_cursor;
  inst_info_t protection_record;
  int type_array[9];
  if (event != EVENT_RESOLVE_SPELL)
  {
    if (event == EVENT_ABILITIES)
    {
      if ((affected_card_controller == player) &&
          (result_or_count = is_what(player, affected_card, 2), result_or_count != 0))
      {
        instance = get_card_instance(player, card);
        protection_record = inst_info(instance, 0x5a67);
        chosen_type = protection_record.data;
        if (chosen_type == 2)
        {
          event_result = event_result | 0x200000;
        }
        else if (chosen_type == 0x40)
        {
          event_result = event_result | 0x10000;
        }
        else if (chosen_type == 1)
        {
          event_result = event_result | 0x400000;
        }
        else if (chosen_type == 0x10)
        {
          event_result = event_result | 0x40000;
        }
        else if (chosen_type == 8)
        {
          event_result = event_result | 0x100000;
        }
        else
        {
          instance = get_card_instance(affected_card_controller, affected_card);
          instance->ext->sp_keywords2 = instance->ext->sp_keywords2 | 2;
        }
      }
    }
    else if (event == EVENT_STATIC_EFFECTS)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        player_bits[player] = player_bits[player] | 0x4000000;
      }
    }
    else
    {
      if ((((event == EVENT_PROT_MISC) && (instance = in_play(player, card), instance != NULL)) &&
           (result_or_count = is_humiliated(player, card, AL_STATIC), result_or_count == 0)) &&
          (affected_card_controller == player))
      {
        protection_record = inst_info(instance, 0x5a67);
        if (((-1 < affected_card) &&
             (result_or_count = is_what(affected_card_controller, affected_card, 2),
              result_or_count != 0)) ||
            (affected_card == -1))
        {
          if (((unsigned int)event_result >> 24 & 0xff) == '\x06')
          {
            event_result = event_result | 1;
          }
          if (protection_record.data == 0x1009)
          {
            result_or_count = is_tribal(attacking_card_controller, attacking_card);
          }
          else
          {
            result_or_count =
                is_what(attacking_card_controller, attacking_card, protection_record.data);
          }
          if (result_or_count != 0)
          {
            event_result = event_result | 1;
          }
        }
      }
    }
    return 0;
  }
  type_cursor = serras_emissary_types;
  copy_cursor = type_array;
  for (result_or_count = 9; result_or_count != 0; result_or_count = result_or_count + -1)
  {
    *copy_cursor = *type_cursor;
    type_cursor = type_cursor + 1;
    copy_cursor = copy_cursor + 1;
  }
  choice =
      dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE, "Creature",
                  1, 10, "Artifact", 1, 10, "Enchantment", 1, 10, "Land", 1, 5, "Planeswalker", 1,
                  10, "Instant", 1, 10, "Sorcery", 1, 10, "Tribal", 1, 1, 0, 0, 0, 0);
  chosen_type = type_array[choice];
  instance = get_card_instance(player, card);
  set_inst_info(instance, 0x5a67, chosen_type, 0);
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x223fb20; original cards/modern_horizons_2.c:6351. */
int card_subtlety(int player, int card, event_t event)
{
  /* Subtlety	|2|U|U
   * Creature - Elemental Incarnation 3/3
   * Flash
   * Flying
   * When this creature enters, choose up to one target creature spell or planeswalker spell. Its owner puts it on their choice of the top or bottom of their library.
   * Evoke-Exile a blue card from your hand. */

  int to_counter_card;
  int spell_player;
  card_instance_t *instance;
  int choice;
  target_definition_t td;

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {

        if (current_turn == player)
        {
          ai_modifier = ai_modifier + -10;
        }
        instance = get_card_instance(player, card);
        spell_player = card_on_stack_controller;
        if (instance->info_slot == 1)
        {
          instance->number_of_targets = '\x01';
          instance->targets[0].player = spell_player;
          instance->targets[0].card = card_on_stack;
        }
      }
      etb_ability(player, card, EVENT_CAST_SPELL, RESOLVE_TRIGGER_MANDATORY);
    }
    else
    {
      etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
      if (event == EVENT_ETB)
      {
        instance = get_card_instance(player, card);
        if (instance->info_slot == 1)
        {

          counterspell_target_definition(player, card, &td, 0x102);
          instance = get_card_instance(player, card);
          if (counterspell_validate(player, card, &td, 0) != 0)
          {

            spell_player = instance->targets[0].player;
            to_counter_card = instance->targets[0].card;
            if (pre_counterspell(player, card, spell_player, to_counter_card) != 0)
            {

              choice = do_dialog(spell_player, spell_player, to_counter_card, -1, -1,
                                 " Put on top of deck\n Put on bottom of deck", 0);
              action_on_card(player, card, spell_player, to_counter_card,
                             (choice != 0) + ACT_PUT_ON_TOP);
            }
          }
          instance->number_of_targets = '\0';
          if (evoked(player, card) != 0)
          {
            kill_card(player, card, KILL_SACRIFICE);
          }
        }
      }
    }
    return evoke_with_cards_in_hand(player, card, event, 4);
  }
  counterspell_target_definition(player, card, &td, 0x102);
  if (counterspell(player, card, EVENT_CAN_CAST, &td, 1) != 0)
  {
    instance = get_card_instance(player, card);
    instance->info_slot = 1;
    return 99;
  }
  instance = get_card_instance(player, card);
  instance->info_slot = 0;
  return flash(player, card, EVENT_CAN_CAST);
}

/* ManalinkEh 2022: 0x2241730; original cards/modern_horizons_2.c:6903. */
int card_timeless_dragon(int player, int card, event_t event)
{
  /* Timeless Dragon	|3|W|W
   * Creature - Dragon 5/5
   * Flying
   * Plainscycling |2 (|2, Discard this card: Search your library for a Plains card, reveal it, put it into your hand, then shuffle.)
   * Eternalize |2|W|W (|2|W|W, Exile this card from your graveyard: Create a token that's a copy of it, except it's a 4/4 black Zombie Dragon with no mana cost. Eternalize only as a sorcery.) */

  enum
  {
    CARD_ID_TIMELESS_DRAGON = 23185
  };

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = eternalize(player, card, event, CARD_ID_TIMELESS_DRAGON, 2, 0, 0, 0, 0, 2);
    return result;
  }
  return landcycling(player, card, event, 2, 0x5007, -1);
}

/* ManalinkEh 2022: 0x22417d0; original cards/modern_horizons_2.c:6917. */
int card_timeless_witness(int player, int card, event_t event)
{
  /* Timeless Witness	|2|G|G
   * Creature - Human Shaman 2/1
   * When this creature enters, return target card from your graveyard to your hand.
   * Eternalize |5|G|G (|5|G|G, Exile this card from your graveyard: Create a token that's a copy of it, except it's a 4/4 black Zombie Human Shaman with no mana cost. Eternalize only as a sorcery.) */

  enum
  {
    CARD_ID_TIMELESS_WITNESS = 23186
  };

  int result;
  test_definition_t test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 0, "Select target card.");
    new_global_tutor(player, player, 2, 1, 1, 1, &test);
  }
  result = eternalize(player, card, event, CARD_ID_TIMELESS_WITNESS, 5, 0, 0, 2, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x222b7b0; original cards/modern_horizons_2.c:7203. */
static int has_mana_cost_0_or_1(int iid, int val, int player, int card)
{
  int id = cards_data[iid].id;
  if (has_no_mana_cost(-1, iid) || is_what(-1, iid, TYPE_LAND))
    return 0;

  card_ptr_t *printed_card = cards_ptr[id];
  return printed_card->req.req_colorless < 2 &&
         printed_card->req.req_black == 0 && printed_card->req.req_blue == 0 &&
         printed_card->req.req_green == 0 && printed_card->req.req_red == 0 &&
         printed_card->req.req_white == 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2242640; original cards/modern_horizons_2.c:7224. */
int card_urzas_saga(int player, int card, event_t event)
{
  /* Urza's Saga	""
   * Enchantment Land - Urza's Saga
   * (As this Saga enters and after your draw step, add a lore counter. Sacrifice after III.)
   * I - This Saga gains "|T: Add |C."
   * II - This Saga gains "|2, |T: Create a 0/0 colorless Construct artifact creature token with 'This token gets +1/+1 for each artifact you control.'"
   * III - Search your library for an artifact card with mana cost |0 or |1, put it onto the battlefield, then shuffle. */

  card_instance_t *instance;
  inst_info_t chapter_record;

  if (event == EVENT_CHAPTER1 || event == EVENT_CHAPTER2)
  {
    instance = get_card_instance(player, card);
    chapter_record = inst_info(instance, CARD_ID_URZAS_SAGA);
    set_inst_info(instance, CARD_ID_URZAS_SAGA, 0,
                  chapter_record.data2 | (event == EVENT_CHAPTER1 ? 1 : 2));
    if (event == EVENT_CHAPTER2)
      return saga(player, card, event, 3);
  }
  else if (event == EVENT_CHAPTER3)
  {
    test_definition_t test;
    new_default_test_definition(&test, TYPE_ARTIFACT,
                                 "Select an artifact card with mana cost |0 or |1.");
    test.special_selection_function = has_mana_cost_0_or_1;
    new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
    return saga(player, card, event, 3);
  }
  else if (event == EVENT_CAN_ACTIVATE || event == EVENT_COUNT_MANA)
  {
    if (card == affected_card && player == affected_card_controller &&
        (instance = in_play(player, card), instance != NULL))
    {
      chapter_record = inst_info(instance, CARD_ID_URZAS_SAGA);
      if (chapter_record.data2 & 1)
        return mana_producer(player, card, event);
    }
  }

  if (event == EVENT_ACTIVATE)
  {
    instance = in_play(player, card);
    instance->mode = 0;
    chapter_record = inst_info(instance, CARD_ID_URZAS_SAGA);
    unsigned int mana_available = 0;
    if (chapter_record.data2 & 1)
      mana_available = mana_producer(player, card, EVENT_CAN_ACTIVATE) != 0;
    unsigned int token_available = 0;
    if (!paying_mana() && (chapter_record.data2 & 2))
      token_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE,
                                                  GAA_UNTAPPED, 3, 0, 0, 0, 0, 0, 0,
                                                  NULL, NULL) != 0;
    int choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
                              DLG_MODE, "Produce mana", mana_available, 5,
                              "Create a token", token_available, 10, 0, 0, 0, 0);
    if (!choice)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (instance->mode == 1)
      return mana_producer(player, card, EVENT_ACTIVATE);
    if (instance->mode == 2)
    {
      add_state(player, card, STATE_TAPPED);
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 2, 0, 0, 0, 0, 0, 0,
                                 NULL, NULL);
      if (spell_fizzled == 1)
        remove_state(player, card, STATE_TAPPED);
      else
      {
        int activation_priority = -48;
        if (current_turn == player)
          activation_priority = current_phase <= PHASE_MAIN1 ? 48 : -48;
        else if (current_turn == 1 - player && current_phase < PHASE_DECLARE_BLOCKERS)
          activation_priority = 48;
        ai_modifier += activation_priority;
      }
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION &&
           (instance = get_card_instance(player, card), instance->mode == 2))
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_CONSTRUCT, &token);
    token.tou = 0;
    token.pow = 0;
    token.special_infos = 66;
    generate_token(&token);
  }
  return saga(player, card, event, 3);
}

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

static void ward_action(int player, int card, event_t event, int tcp, int tcc, int mode, int flags, int cless, int black, int blue, int green, int red, int white, int life_to_pay)
{
  int result;
  unsigned int must_counter;
  int saved_fizzle = spell_fizzled;
  char buffer[100];
  (void)mode;
  must_counter = flags & 1;
  if (must_counter == 0)
  {
    must_counter = 1;
  }
  else if (-1 < hand_count[trigger_cause_controller])
  {

    discard(trigger_cause_controller, 6, player);
    must_counter = (unsigned int)(spell_fizzled == 1);
  }
  spell_fizzled = saved_fizzle;
  if ((life_to_pay != 0) && (result = can_pay_life(tcp, life_to_pay), result != 0))
  {

    scnprintf(buffer, 100, "Pay %d life", life_to_pay);
    result = dialog_impl(player, card, event, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
                        DLG_WHO_CHOOSES(tcp), buffer, 1,
                        (5 < life[tcp] - life_to_pay ? 20U : 5U), "Decline", 1,
                        10, 0, 0, 0, 0);
    if (result == 1)
    {
      must_counter = 0;
      lose_life(tcp, life_to_pay);
    }
  }
  if (((((((cless < 1) && (black < 1)) && (blue < 1)) && ((green < 1 && (red < 1)))) &&
        (white < 1)) ||
       ((result = has_mana_multi(tcp, cless, black, blue, green, red, white),
         result == 0 || (result = charge_mana_multi(tcp, cless, black, blue, green, red, white),
                        result == 0)))) &&
      (must_counter != 0))
  {
    real_counter_a_spell(player, card, tcp, tcc);
  }
}

static int check_targets_for_ward(int player, int card, int ctc_player, int ctc_card, int t_player, int t_card, type_t type, subtype_t subtype, int flags)
{
  card_instance_t *instance = get_card_instance(ctc_player, ctc_card);
  for (int i = 0; i < (int)(unsigned int)instance->number_of_targets; ++i)
  {
    int p = instance->targets[i].player;
    int c = instance->targets[i].card;
    if (p == t_player && c != -1 && (t_card == c || t_card == -1) &&
        (player != p || card != c || !(flags & 2)) &&
        (type == TYPE_NONE || is_what(p, c, type)) &&
        (subtype == SUBTYPE_INVALID || has_subtype(p, c, subtype)))
      return 2;
  }
  return 0;
}

static void archon_of_cruelty_effect(int player, int card)
{
  if (opponent_is_valid_target(player, card))
  {
    int opponent = 1 - player;
    player_sacrifices_a_permanent(player, card, opponent, 0x102, SAC_CAUSED);
    new_multidiscard(opponent, 1, player, 2);
    lose_life(opponent, 3);
    draw_cards(player, 1);
    gain_life(player, 3);
  }
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

static int can_produce_colorless(int player, int card)
{
  return get_card_instance(player, card)->card_color & 0x41;
}

/* ManalinkEh 2022: 0x222f0a0; original cards/modern_horizons_2.c:1089. */
int card_break_the_ice(int player, int card, event_t event)
{
  /* Break the Ice	|B|B
   * Sorcery
   * Destroy target land that is snow or could produce |C.
   * Overload |4|B|B (You may cast this spell for its overload cost. If you do, change "target" in its text to "each.") */

  card_instance_t *instance;
  int result;
  unsigned int overload_result;
  target_definition_t td;
  test_definition_t this_test;
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 1);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (int32_t)target_is_snow_or_can_produce_colorless;
    instance = get_card_instance(player, card);
    result = basic_spell(player, card, EVENT_CAN_CAST);
    if (result != 0)
    {
      if (instance->info_slot == 1)
      {
        return 1;
      }
      return can_target(&td);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 1);
      td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      td.extra = (int32_t)target_is_snow_or_can_produce_colorless;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        result = check_special_flags2(player, card, -0x80000000);
        if ((result != 0) ||
            (overload_result = pay_overload(player, card, 4, 2, 0, 0, 0, 0), overload_result == 1))
        {
          new_pick_target(&td, "Select land that is snow or could produce |C.", 0, 0x81);
          if (spell_fizzled == 1)
          {
            return 0;
          }
          overload_result = 1;
        }
        instance->info_slot = instance->info_slot | overload_result;
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 1);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (int32_t)target_is_snow_or_can_produce_colorless;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      if (((unsigned int)instance->info_slot & 2) == 0)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        }
      }
      else
      {
        default_test_definition(&this_test, 1);
        this_test.special_selection_function = is_snow_or_can_produce_colorless;
        new_manipulate_all(player, card, 2, &this_test, ACT_KILL_DESTROY);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  return overload(player, card, event, 4, 2, 0, 0, 0, 0);
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2230390; original cards/modern_horizons_2.c:1468. */
int card_captured_by_lagacs(int player, int card, event_t event)
{
  /* Captured by Lagacs	|1|G|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature can't attack or block.
   * When this Aura enters, support 2. (Put a +1/+1 counter on each of up to two target creatures.) */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    support(player, card, 2);
  }
  result = card_pacifism(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2230e60; original cards/modern_horizons_2.c:1773. */
int card_constable_of_the_realm(int player, int card, event_t event)
{
  /* Constable of the Realm	|4|W
   * Creature - Giant Soldier 3/3
   * Renown 2 (When this creature deals combat damage to a player, if it isn't renowned, put two +1/+1 counters on it and it becomes renowned.)
   * Whenever one or more +1/+1 counters are put on this creature, exile up to one other target nonland permanent until this creature leaves the battlefield. */

  xtrigger_t trigger;
  int result;
  card_instance_t *instance;
  target_definition_t td;

  return_from_oblivion(player, card, event);
  renown(player, card, event, 2);
  enable_xtrigger_flags = enable_xtrigger_flags | ENABLE_XTRIGGER_1_1_COUNTERS;
  trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    trigger = xtrigger_impl_value_dont_use_directly;
  }
  if ((((trigger == XTRIGGER_1_1_COUNTERS) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (((reason_for_trigger_controller == player && (trigger_cause_controller == player)) &&
        (trigger_cause == card))))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      if (event == EVENT_TRIGGER)
      {
        result = duh_mode(player);
        event_result = event_result | (result != 0) + 1;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {
        default_target_definition(player, card, &td, 0x47);
        td.illegal_type = TYPE_LAND;
        result = new_pick_target(&td, "TARGET_NONLAND_PERMANENT", 0, 0);
        if (result != 0)
        {

          instance = get_card_instance(player, card);
          obliviation(player, card, instance->targets[0].player, instance->targets[0].card);
          instance->number_of_targets = '\0';
        }
      }
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2231430; original cards/modern_horizons_2.c:1895. */
int card_damn2(int player, int card, event_t event)
{
  /* Damn	|B|B
   * Sorcery
   * Destroy target creature. A creature destroyed this way can't be regenerated.
   * Overload |2|W|W (You may cast this spell for its overload cost. If you do, change "target" in its text to "each.") */

  card_instance_t *instance;
  int result;
  unsigned int overload_result;
  target_definition_t td;
  test_definition_t this_test;
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    result = basic_spell(player, card, EVENT_CAN_CAST);
    if (result != 0)
    {
      if (instance->info_slot == 1)
      {
        return 1;
      }
      return can_target(&td);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        result = check_special_flags2(player, card, -0x80000000);
        if ((result != 0) ||
            (overload_result = pay_overload(player, card, 2, 0, 0, 0, 0, 2), overload_result == 1))
        {
          new_pick_target(&td, "TARGET_CREATURE", 0, 1);
          if (spell_fizzled == 1)
          {
            return 0;
          }
          overload_result = 1;
        }
        instance->info_slot = instance->info_slot | overload_result;
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      if (((unsigned int)instance->info_slot & 2) == 0)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
        }
      }
      else
      {
        default_test_definition(&this_test, 2);
        new_manipulate_all(player, card, 2, &this_test, ACT_KILL_BURY);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  return overload(player, card, event, 2, 0, 0, 0, 0, 2);
}

/* ManalinkEh 2022: 0x222b230; original cards/modern_horizons_2.c:1987. */
static int create_dauthi_voidwalker_legacy(int player, int card, int iid)
{
  int legacy =
      create_legacy_effect_for_opponent(player, card, card_exiled_with_void_counter, -1, -1);
  if (legacy != -1)
  {
    int owner = 1 - player;
    add_counters(player, card, owner, legacy, COUNTER_VOID, 1);
    card_instance_t *leg = get_card_instance(owner, legacy);
    int source_card = leg->damage_source_card;
    unsigned csvid = (uint16_t)cards_data[iid].id;
    leg->display_pic_csv_id = csvid;
    leg->targets[0].card = iid;
    leg->targets[0].player = owner;
    leg->eot_toughness = 0x300;
    leg->display_pic_num =
        (uint16_t)get_card_image_number(csvid, leg->damage_source_player, source_card);
  }
  return legacy;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2231790; original cards/modern_horizons_2.c:2001. */
int card_dauthi_voidwalker(int player, int card, event_t event)
{
  /* Dauthi Voidwalker	|B|B
   * Creature - Dauthi Rogue 3/2
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * If a card would be put into an opponent's graveyard from anywhere, instead exile it with a void counter on it.
   * |T, Sacrifice this creature: Choose an exiled card an opponent owns with a void counter on it. You may play it this turn without paying its mana cost. */

  shadow(player, card, event);
  int opponent = 1 - player;
  if (if_a_card_would_be_put_into_graveyard_from_library_do_something_instead(
          player, card, event, opponent, RESOLVE_TRIGGER_MANDATORY, NULL))
  {
    create_dauthi_voidwalker_legacy(player, card,
                                    deck_ptr[trigger_cause_controller][trigger_cause]);
    rfg_card_in_deck(trigger_cause_controller, trigger_cause);
    replace_milled = 1;
  }
  if (if_a_card_would_be_put_into_graveyard_from_anywhere_but_library_do_something_instead(
          player, card, event, opponent, RESOLVE_TRIGGER_MANDATORY, NULL))
  {
    create_dauthi_voidwalker_legacy(player, card, trigger_cause);
    add_card_to_rfg(trigger_cause_controller, trigger_cause);
    play_sound_effect(WAV_DESTROY);
    replace_milled = 1;
  }
  if (event == EVENT_ACTIVATE)
  {
    int count = 0;
    for (int c = 0; c < active_cards_count[opponent]; ++c)
      if (in_play(opponent, c) && is_what(opponent, c, 0x80) &&
          count_counters(opponent, c, COUNTER_VOID))
        ++count;
    ai_modifier += count * 10;
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    int exiled[31][3] = {{0}};
    if (active_cards_count[opponent] > 0)
    {
      int best_index = -1, max_value = -99, count = 0;
      for (int c = 0; c < active_cards_count[opponent] && count < 31; ++c)
      {
        if (in_play(opponent, c) && is_what(opponent, c, 0x80) &&
            count_counters(opponent, c, COUNTER_VOID))
        {
          card_instance_t *leg = get_card_instance(opponent, c);
          int iid = leg->targets[0].card, owner = leg->targets[0].player;
          if (check_rfg(owner, (uint16_t)cards_data[iid].id))
          {
            int found = 0;
            for (int d = 0; d <= count; ++d)
              if (exiled[d][1] == iid)
              {
                found = 1;
                break;
              }
            if (!found)
            {
              ++count;
              exiled[count][0] = owner;
              exiled[count][1] = iid;
              exiled[count][2] = c;
              int value = my_base_value_by_id((uint16_t)cards_data[iid].id);
              if (value > max_value)
              {
                max_value = value;
                best_index = count;
              }
            }
          }
        }
      }
      if (count != 0)
      {
        const char *labels[15];
        int weights[15];
        for (int i = 14; i >= 0; --i)
          labels[i] = count > i ? cards_data[exiled[i + 1][1]].name : " ";
        int choice;
        for (;;)
        {
          for (int i = 14; i >= 0; --i)
            weights[i] = my_base_value_by_id((uint16_t)cards_data[exiled[i + 1][1]].id);
          choice = dialog_impl(
              player, card, EVENT_RESOLVE_ACTIVATION, DLG_NO_STORAGE, DLG_RANDOM, DLG_OMIT_ILLEGAL,
              DLG_NO_CANCEL, labels[0], count > 0, weights[0], labels[1], count > 1, weights[1],
              labels[2], count > 2, weights[2], labels[3], count > 3, weights[3], labels[4],
              count > 4, weights[4], labels[5], count > 5, weights[5], labels[6], count > 6,
              weights[6], labels[7], count > 7, weights[7], labels[8], count > 8, weights[8],
              labels[9], count > 9, weights[9], labels[10], count > 10, weights[10], labels[11],
              count > 11, weights[11], labels[12], count > 12, weights[12], labels[13], count > 13,
              weights[13], labels[14], count > 14, weights[14], "Continue >>", count > 15,
              (best_index < 16 ? 0 : 999), 0, 0, 0, 0);
          if (choice != 16)
            break;
          int allow_back =
              ai_is_speculating != 1 && (player != 1 || ((uint32_t)trace_mode >> 1 & 1));
          const char *saved_labels[15];
          for (int i = 0; i < 15; ++i)
            saved_labels[i] = labels[i];
          for (int i = 14; i >= 0; --i)
          {
            int iid = exiled[i + 16][1];
            weights[i] = my_base_value_by_id((uint16_t)cards_data[iid].id);
            labels[i] = count > i + 15 ? cards_data[iid].name : " ";
          }
          int second = dialog_impl(
              player, card, EVENT_RESOLVE_ACTIVATION, DLG_NO_STORAGE, DLG_RANDOM, DLG_OMIT_ILLEGAL,
              DLG_NO_CANCEL, labels[0], count > 15, weights[0], labels[1], count > 16, weights[1],
              labels[2], count > 17, weights[2], labels[3], count > 18, weights[3], labels[4],
              count > 19, weights[4], labels[5], count > 20, weights[5], labels[6], count > 21,
              weights[6], labels[7], count > 22, weights[7], labels[8], count > 23, weights[8],
              labels[9], count > 24, weights[9], labels[10], count > 25, weights[10], labels[11],
              count > 26, weights[11], labels[12], count > 27, weights[12], labels[13], count > 28,
              weights[13], labels[14], count > 29, weights[14], "<<Back", allow_back, -999, 0, 0, 0,
              0);
          for (int i = 0; i < 15; ++i)
            labels[i] = saved_labels[i];
          if (second != 16)
          {
            choice = second + 15;
            break;
          }
        }
        create_may_play_card_from_exile_effect(player, card, exiled[choice][0],
                                                    (uint16_t)cards_data[exiled[choice][1]].id,
                                                    MPCFE_UNTIL_EOT | MPCFE_FOR_FREE);
      }
    }
  }
  return generic_activated_ability(player, card, event, 9, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2236f60; original cards/modern_horizons_2.c:3463. */
int card_glinting_creeper(int player, int card, event_t event)
{
  /* Glinting Creeper	|4|G
   * Creature - Plant 0/0
   * Converge - This creature enters with two +1/+1 counters on it for each color of mana spent to cast it.
   * This creature can't be blocked by creatures with power 2 or less. */

  card_instance_t *instance;
  int result;

  converge(player, card, event);
  instance = get_card_instance(player, card);
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, instance->info_slot);
  if (((event == EVENT_BLOCK_LEGALITY) && (current_turn == player)) && (attacking_card == card))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      result = get_power(affected_card_controller, affected_card);
      if (result < 3)
      {
        event_result = event_result | 1;
      }
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2239450; original cards/modern_horizons_2.c:4218. */
int card_loathsome_curator(int player, int card, event_t event)
{
  /* Loathsome Curator	|4|B
   * Creature - Gorgon Wizard 5/4
   * Exploit (When this creature enters, you may sacrifice a creature.)
   * Menace
   * When this creature exploits a creature, destroy target creature you don't control with mana value 3 or less. */

  int result;
  target_definition_t td;

  menace(player, card, event);
  result = exploit(player, card, event);
  if (result != 0)
  {
    default_target_definition(player, card, &td, 0x47);
    td.allowed_controller = 1 - player;
    td.extra = 3;
    td.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
    td.allow_cancel = 0;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(
          &td, "Select target creature you don\'t control with mana value 3 or less.", 0, 0x80);
      if (result != 0)
      {
        kill_target0(player, card, KILL_DESTROY);
      }
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x223af70; original cards/modern_horizons_2.c:4868. */
int card_out_of_time(int player, int card, event_t event)
{
  /* Out of Time	|1|W|W
   * Enchantment
   * When this enchantment enters, untap all creatures, then those creatures phase out until this enchantment leaves the battlefield. Put a time counter on this enchantment for each creature that phased out this way.
   * Vanishing (At the beginning of your upkeep, remove a time counter from this enchantment. When the last is removed, sacrifice it.) */

  int p;
  card_instance_t *instance;
  int creature_count_local;
  int marked[151][2];

  if (event == EVENT_CAN_CAST)
  {
    return 1;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      creature_count_local = count_subtype(1 - player, 2, -1);
      ai_modifier = ai_modifier + creature_count_local * 5;
      creature_count_local = count_subtype(player, 2, -1);
      ai_modifier = ai_modifier + creature_count_local * -5;
    }
    etb_ability(player, card, EVENT_CAST_SPELL, RESOLVE_TRIGGER_MANDATORY);
    vanishing(player, card, event, 0);
    return 0;
  }
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  p = current_turn;
  if (event == EVENT_ETB)
  {
    int marked_count = 0;
    for (int scan_card = active_cards_count[current_turn] - 1; scan_card >= 0; --scan_card)
    {
      instance = in_play(p, scan_card);
      if (instance && is_what(p, scan_card, 2))
      {
        marked[marked_count][0] = p;
        marked[marked_count][1] = scan_card;
        ++marked_count;
      }
    }
    int other_player = 1 - current_turn;
    for (int scan_card = active_cards_count[other_player] - 1; scan_card >= 0; --scan_card)
    {
      instance = in_play(other_player, scan_card);
      if (instance && is_what(other_player, scan_card, 2))
      {
        marked[marked_count][1] = scan_card;
        marked[marked_count][0] = other_player;
        ++marked_count;
      }
    }
    for (int index = marked_count - 1; index >= 0; --index)
    {
      int target_card = marked[index][1];
      int target_player = marked[index][0];
      untap_card(target_player, target_card);
      phase_out_target_until_source_leaves_play(player, card, target_player, target_card);
    }
    add_counters(player, card, player, card, COUNTER_TIME, marked_count);
  }
  vanishing(player, card, event, 0);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x223bd60; original cards/modern_horizons_2.c:5148. */
int card_radiant_epicure(int player, int card, event_t event)
{
  /* Radiant Epicure	|4|B
   * Creature - Vampire Wizard 5/5
   * Converge - When this creature enters, each opponent loses X life and you gain X life, where X is the number of colors of mana spent to cast this spell. */

  int amount;
  card_instance_t *instance;

  converge(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  amount = instance->info_slot;
  lose_life(1 - player, amount);
  gain_life(player, amount);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x223c8c0; original cards/modern_horizons_2.c:5364. */
int card_rise_and_shine(int player, int card, event_t event)
{
  /* Rise and Shine	|1|U
   * Sorcery
   * Target noncreature artifact you control becomes a 0/0 artifact creature. Put four +1/+1 counters on each artifact that became a creature this way.
   * Overload |4|U|U (You may cast this spell for its overload cost. If you do, change "target" in its text to "each.") */

  int c;
  card_instance_t *instance;
  unsigned int overload_result;
  int result_or_artifact;
  int *marked_cursor;
  bool more_marked;
  int marked_count;
  target_definition_t td;
  int marked[151];
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x40);
    td.illegal_type = TYPE_CREATURE;
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
    if (basic_spell(player, card, EVENT_CAN_CAST) != 0)
    {
      if (instance->info_slot == 1)
      {
        return 1;
      }
      return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x40);
      td.illegal_type = TYPE_CREATURE;
      td.allowed_controller = player;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        result_or_artifact = check_special_flags2(player, card, -0x80000000);
        if ((result_or_artifact != 0) ||
            (overload_result = pay_overload(player, card, 4, 0, 2, 0, 0, 0), overload_result == 1))
        {
          new_pick_target(&td, "Select targer noncreature artifact", 0, 0x81);
          if (spell_fizzled == 1)
          {
            return 0;
          }
          overload_result = 1;
        }
        instance->info_slot = instance->info_slot | overload_result;
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x40);
    td.illegal_type = TYPE_CREATURE;
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      if (((unsigned int)instance->info_slot & 2) == 0)
      {
        if (valid_target(&td) != 0)
        {
          add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                       COUNTER_P1_P1, 4);
          artifact_animation(player, card, instance->targets[0].player, instance->targets[0].card,
                             0, 0, 0, 0, 0);
        }
      }
      else
      {
        if (0 < active_cards_count[player])
        {
          marked_count = 0;
          for (c = 0; c < active_cards_count[player]; ++c)
            if ((instance = in_play(player, c), instance != NULL) && is_what(player, c, 0x40) &&
                !is_what(player, c, 2))
              marked[marked_count++] = c;
          if (0 < marked_count)
          {
            marked_cursor = marked + marked_count + -1;
            do
            {
              result_or_artifact = *marked_cursor;
              add_counters(player, card, player, result_or_artifact, COUNTER_P1_P1, 4);
              artifact_animation(player, card, player, result_or_artifact, 0, 0, 0, 0, 0);
              more_marked = marked != marked_cursor;
              marked_cursor = marked_cursor + -1;
            } while (more_marked);
          }
        }
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  return overload(player, card, event, 4, 0, 2, 0, 0, 0);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x223dd40; original cards/modern_horizons_2.c:5706. */
int card_scurry_oak(int player, int card, event_t event)
{
  /* Scurry Oak	|2|G
   * Creature - Treefolk 1/2
   * Evolve (Whenever a creature you control enters, if that creature has greater power or toughness than this creature, put a +1/+1 counter on this creature.)
   * Whenever one or more +1/+1 counters are put on this creature, you may create a 1/1 green Squirrel creature token. */

  int result;
  token_generation_t token;

  evolve(player, card, event);
  result = one_or_more_p1_p1_counters_are_putted_on_me(player, card, event);
  if (result == 0)
  {
    return 0;
  }
  default_token_definition(player, card, CARD_ID_SQUIRREL, &token);
  token.tou = 1;
  token.pow = 1;
  token.color_forced = '\b';
  generate_token(&token);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x223e3d0; original cards/modern_horizons_2.c:5853. */
int card_shattered_ego(int player, int card, event_t event)
{
  /* Shattered Ego	|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets -3/-0.
   * |3|U|U: Put enchanted creature into its owner's library third from the top. */

  int result;
  card_instance_t *instance;

  attached_gets_pt(player, card, event, -3, 0);
  if ((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE))
  {
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      instance = get_card_instance(player, card);
      put_permanent_under_the_first_x_card_of_its_owners_library(
          (int)instance->damage_target_player, instance->damage_target_card, 3);
    }
    result = aura(player, card, event, NULL, NULL);
    disabling_aura_ai(player, card, event);
    return result;
  }
  result = generic_activated_ability(player, card, event, 0, 3, 0, 2, 0, 0, 0, 0, NULL, NULL);
  return result;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline-only helpers; native code remains in their callers. */
/* ManalinkEh 2022: 0x223e650; original cards/modern_horizons_2.c:5913. */
int card_skyblades_boon(int player, int card, event_t event)
{
  /* Skyblade's Boon	|1|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+1 and has flying.
   * |2|W: Return ~ to its owner's hand. Activate only if ~ is on the battlefield or in your graveyard. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = has_mana_multi(player, 2, 0, 0, 0, 0, 1);
    if (result != 0)
    {
      return 4;
    }
  }
  else
  {
    if (event == EVENT_PAY_FLASHBACK_COSTS)
    {
      charge_mana_multi(player, 2, 0, 0, 0, 0, 1);
      if (spell_fizzled != 1)
      {
        return 1;
      }
    }
    else if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 1, 0, NULL, NULL);
    }
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    bounce_permanent(instance->parent_controller, instance->parent_card);
  }
  attached_gets_pt(player, card, event, 1, 1);
  attached_gets_kw(player, card, event, KEYWORD_FLYING, SP_KEYWORD_0);
  return aura(player, card, event, NULL, NULL);
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline helpers; existing native callers contain the expansions. */
/* ManalinkEh 2022: 0x2242b50; original cards/modern_horizons_2.c:7296. */
int card_vectis_gloves(int player, int card, event_t event)
{
  /* Vectis Gloves	|2
   * Artifact - Equipment
   * Equipped creature gets +2/+0 and has artifact landwalk. (It can't be blocked as long as defending player controls an artifact land.)
   * Equip |2 */

  int p;
  int humiliated;
  card_instance_t *instance;
  unsigned int nonbasic_land;
  int card_00;

  if (((((event == EVENT_BLOCK_LEGALITY) &&
         (instance = is_equipping(player, card), instance != NULL)) &&
        (humiliated = is_humiliated(player, card, AL_STATIC), humiliated == 0)) &&
       (((player_bits[player] & PB_NONSTANDARD_LANDWALK_DISABLED) == 0 &&
         (instance = get_card_instance(player, card),
          attacking_card_controller == instance->damage_target_player)))) &&
      (attacking_card == instance->damage_target_card))
  {
    p = 1 - player;
    nonbasic_land = 0;
    if (0 < active_cards_count[p])
    {
      card_00 = 0;
      do
      {
        instance = in_play(p, card_00);
        if (((instance != NULL) && (is_what(p, card_00, 1) != 0)) &&
            (is_what(p, card_00, 0x40) != 0))
        {
          nonbasic_land = 1;
          break;
        }
        card_00 = card_00 + 1;
      } while (card_00 < active_cards_count[p]);
    }
    event_result = event_result | nonbasic_land;
  }
  return vanilla_equipment(player, card, event, 2, 2, 0, 0, 0);
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 private inline definitions. See per-helper limits in the recovery evidence. */
/* ManalinkEh 2022: 0x2242d30; original cards/modern_horizons_2.c:7339. */
int card_verdant_command(int player, int card, event_t event)
{
  /* Verdant Command	|1|G
   * Instant
   * Choose two -
   * * Target player creates two tapped 1/1 green Squirrel creature tokens.
   * * Counter target loyalty ability of a planeswalker.
   * * Exile target card from a graveyard.
   * * Target player gains 3 life. */

  typedef enum
  {
    CHOICE_INVALID_TARGET = 0,
    CHOICE_SQUIRREL = 1,
    CHOICE_COUNTER_PW_ABIL = 2,
    CHOICE_EXILE_CARD = 3,
    CHOICE_3_LIFE = 4
  } CommandChoices;
  CommandChoices choice;
  int result;
  int opponent;
  const char *prompt;
  card_instance_t *instance;
  test_definition_t *grave_test;
  target_definition_t *player_target;
  int counter_available;
  int priority3;
  CommandChoices choices[2];
  int abils[5];
  // Native 0x022430df-0x0224311f reads these without initialization on the human path.
  int priority[5];
  token_generation_t token;
  target_definition_t td_spell;
  target_definition_t td_player;
  test_definition_t test;

  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  counter_activated_target_definition(player, card, &td_spell, 0x100);
  counter_available = can_counter_activated_ability(player, card, event, &td_spell);
  default_target_definition(player, card, &td_player, 0);
  td_player.zone = TARGET_ZONE_PLAYERS;
  new_default_test_definition(&test, 0xff, "Select target card to exile.");
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    // Native byte-fill gives each 32-bit int the value 0x01010101.
    memset(abils, 1, sizeof(abils));
    choices[0] =
        dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS,
                    DLG_RANDOM, "2 tapped Squirrels", 0x1010101, 0x1010101,
                    "Counter PW loyalty ability", 0x1010101, 0x1010101, "Exile card in grave",
                    0x1010101, 0x1010101, "Gain 3 life", 0x1010101, 0x1010101, 0, 0, 0, 0);

    int target_index = 0;
    for (int choice_index = 0; choice_index < 2; ++choice_index)
    {
      switch (choices[choice_index])
      {
      case CHOICE_INVALID_TARGET:
        ++target_index;
        break;
      case CHOICE_SQUIRREL:
        if (validate_target(player, card, &td_player, target_index))
        {
          default_token_definition(player, card, CARD_ID_SQUIRREL, &token);
          token.t_player = instance->targets[target_index].player;
          token.tou = 1;
          token.pow = 1;
          token.color_forced = 8;
          token.qty = 2;
          token.action = TOKEN_ACTION_TAPPED;
          generate_token(&token);
        }
        ++target_index;
        break;
      case CHOICE_COUNTER_PW_ABIL:
        resolve_counter_activated_ability(player, card, &td_spell, target_index);
        ++target_index;
        break;
      case CHOICE_EXILE_CARD:
      {
        int grave_index = validate_target_from_grave_source(
            player, card, instance->targets[target_index].player, target_index + 1);
        if (grave_index != -1)
          rfg_card_from_grave(instance->targets[target_index].player, grave_index);
        target_index += 2;
        break;
      }
      case CHOICE_3_LIFE:
        if (validate_target(player, card, &td_player, target_index))
          gain_life(instance->targets[target_index].player, 3);
        // The native fourth mode does not advance the target index.
        break;
      }
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  grave_test = &test;
  player_target = &td_player;
  abils[1] = generic_spell(player, card, EVENT_CAN_CAST, 1, player_target, NULL, 1, NULL);
  abils[2] = (int)(counter_available != 0);
  abils[3] = generic_spell(player, card, EVENT_CAN_CAST, 0x1000, NULL, NULL, 1, grave_test);
  abils[4] = abils[1];
  if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
  {
    opponent = 1 - player;
    result = count_subtype(opponent, 2, -1);
    priority[1] = result * 5;
    priority[2] = ((abils[2] == 0) ? 1U : 1000000U);
    result = graveyard_has_shroud(opponent);
    priority3 = 5;
    if (result == 0)
    {
      priority3 = count_graveyard(opponent);
    }
    priority[4] = ((life[player] < 6) ? 50U : 10U);
  }
  else
  {
    priority3 = priority[3];
  }
  choice = dialog_impl(player, card, event, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS, DLG_RANDOM,
                       "2 tapped Squirrels", abils[1], priority[1], "Counter PW loyalty ability",
                       abils[2], priority[2], "Exile card in grave", abils[3], priority3,
                       "Gain 3 life", abils[4], priority[4], 0, 0, 0, 0);
  if (event == EVENT_CAN_CAST)
  {
    if (counter_available != 0)
    {
      choice = 99;
    }
    return choice;
  }
  choices[0] = choice;

  if (event != EVENT_CAST_SPELL || card != affected_card || player != affected_card_controller)
    return 0;
  instance->number_of_targets = 0;
  int target_index = 0;
  for (int choice_index = 0; choice_index < 2; ++choice_index)
    switch (choices[choice_index])
    {
    case CHOICE_INVALID_TARGET:
      spell_fizzled = 1;
      break;
    case CHOICE_SQUIRREL:
    case CHOICE_3_LIFE:
      prompt = choices[choice_index] == CHOICE_SQUIRREL ? "Select target player to get squirrels."
                                                        : "Select target player to gain life.";
      td_player.preferred_controller = player;
      if (!new_pick_target(player_target, prompt, target_index, 0x81))
        return 0;
      ++target_index;
      break;
    case CHOICE_COUNTER_PW_ABIL:
      cast_counter_activated_ability(player, card, target_index);
      ++target_index;
      break;
    case CHOICE_EXILE_CARD:
      if (select_target_from_either_grave(player, card, SFG_0, 3, 1, grave_test, target_index,
                                          target_index + 1) == -1)
      {
        spell_fizzled = 1;
        return 0;
      }
      target_index += 2;
      break;
    }
  return 0;
}

/* ManalinkEh 2022: 0x22438b0; original cards/modern_horizons_2.c:7566. */
int card_world_weary(int player, int card, event_t event)
{
  /* World-Weary	|3|B|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets -4/-4.
   * Basic landcycling |1|B (|1|B, Discard this card: Search your library for a basic land card, reveal it, put it into your hand, then shuffle.) */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    attached_gets_pt(player, card, event, -4, -4);
    result = aura(player, card, event, NULL, NULL);
    disabling_aura_ai(player, card, event);
    return result;
  }
  result = basic_landcycling(player, card, event, 1, 0, 0, 0, 1, 0);
  return result;
}
#endif
