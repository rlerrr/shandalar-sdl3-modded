#include "manalink.h"

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21c7f50; original cards/war_of_the_spark.c:2. */
void amass(int player, int card, int amount)
{
  int c;
  int count;
  int result_or_army;
  card_instance_t *instance;

  count = count_subtype(player, -1, 0xfe);
  if (count == 1)
  {
    c = 0;
    if (0 < active_cards_count[player])
    {
      do
      {
        instance = in_play(player, c);
        if (((instance != NULL) && (is_what(player, c, 2) != 0)) &&
            (has_subtype(player, c, SUBTYPE_ARMY) != 0))
        {
          add_counters(player, card, player, c, COUNTER_P1_P1, amount);
        }
        c = c + 1;
      } while (c < active_cards_count[player]);
    }
  }
  else
  {
    if (count < 2)
    {
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_ZOMBIE_ARMY, &token);
      token.action = TOKEN_ACTION_ADD_COUNTERS;
      token.action_argument = amount;
      generate_token(&token);
      return;
    }
    {
      target_definition_t td;
      default_target_definition(player, card, &td, 2);
      td.required_subtype = 0xfe;
      td.preferred_controller = player;
      td.allowed_controller = player;
      td.allow_cancel = 0;
      result_or_army = can_target(&td);
      if ((result_or_army != 0) &&
          (result_or_army = new_pick_target(&td, "Select target Army you control.", 0, 0x80),
           result_or_army != 0))
      {
        instance = get_card_instance(player, card);
        result_or_army = instance->targets[0].card;
        instance = get_card_instance(player, card);
        add_counters(player, card, instance->targets[0].player, result_or_army, COUNTER_P1_P1,
                     amount);
      }
    }
  }
  return;
}

/* ManalinkEh 2022: 0x21c6fc0; original cards/war_of_the_spark.c:29. */
static int war_god_eternal(int player, int card, event_t event)
{
  leaves_play(player, card, event, 1, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    card_instance_t *inst = get_card_instance(player, card);
    unsigned char kc = inst->kill_code;
    int owner, position;
    int iid;
    if (kc != 0 && kc < 4)
    {
      if (!find_in_owners_graveyard(player, card, &owner, &position))
        return 0;
      if (do_dialog(player, player, card, -1, -1, " Put into library\n Pass", 0) != 0)
        return 0;
      iid = graveyard_ptr_mutable[owner][position];
      remove_card_from_grave(owner, position);
    }
    else if (kc == 4)
    {
      iid = inst->original_internal_card_id;
      owner = get_owner(player, card);
      position = find_iid_in_rfg(owner, iid);
      if (position < 0)
        return 0;
      if (do_dialog(player, player, card, -1, -1, " Put into library\n Pass", 0) != 0)
        return 0;
      remove_card_from_rfg_by_position(owner, position);
    }
    else
      return 0;
    put_iid_under_the_first_x_cards_of_library(owner, iid, 2);
  }
  return 0;
}

/* ManalinkEh 2022: 0x21c8110; original cards/war_of_the_spark.c:66. */
int card_ajani_the_greathearted(int player, int card, event_t event)
{
  /* Ajani, the Greathearted	|2|G|W
   * Legendary Planeswalker - Ajani (5)
   * Creatures you control have vigilance.
   * +1: You gain 3 life.
   * -2: Put a +1/+1 counter on each creature you control and a loyalty counter on each other planeswalker you control. */

  enum
  {
    CHOICE_LIFE = 1,
    CHOICE_COUNTERS_TO_ALL = 2
  };

  int result;
  card_instance_t *permanent;
  card_instance_t *instance;
  int permanent_index;

  check_legend_rule(player, card, event);
  boost_subtype(player, card, event, ~SUBTYPE_INVALID, 0, 0, 0, 8,
                BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Gain 3 life", 1,
                           5, 1, "Add counters", 1, 10, 0xfffffffe, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    if (instance->info_slot == CHOICE_LIFE)
    {
      gain_life(player, 3);
    }
    else
    {

      if ((instance->info_slot == CHOICE_COUNTERS_TO_ALL) &&
          (permanent_index = active_cards_count[player] + -1, -1 < permanent_index))
      {
        do
        {
          permanent = in_play(player, permanent_index);
          if (permanent != NULL)
          {
            result = is_what(player, permanent_index, 2);
            if (result != 0)
            {
              add_counters(player, card, player, permanent_index, COUNTER_P1_P1, 1);
            }
            result = is_planeswalker(player, permanent_index);
            if ((result != 0) && (instance->parent_card != permanent_index))
            {
              add_counters(player, card, player, permanent_index, COUNTER_LOYALTY, 1);
            }
          }
          permanent_index = permanent_index + -1;
        } while (permanent_index != -1);
      }
    }
  }
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x21c8380; original cards/war_of_the_spark.c:128. */
int card_angraths_rampage(int player, int card, event_t event)
{
  /* Angrath's Rampage	|B|R
   * Sorcery
   * Choose one -
   * * Target player sacrifices an artifact of their choice.
   * * Target player sacrifices a creature of their choice.
   * * Target player sacrifices a planeswalker of their choice. */

  enum
  {
    CHOICE_ARTIFACT = 1,
    CHOICE_CREATURE = 2,
    CHOICE_PLANESWALKER = 3
  };

  card_instance_t *instance;
  int result;
  type_t type;
  const char *type_name;
  target_definition_t td;
  char typetext[100];
  char prompt[200];
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_PLAYER", 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.zone = TARGET_ZONE_PLAYERS;
      get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        result = check_special_flags2(player, card, -0x80000000);

        if ((result == 0) &&
            (result =
                 dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM,
                             "Target sacrifices an artifact", 1, 5, "Target sacrifices a creature",
                             1, 5, "Target sacrifices a planeswalker", 1, 5, 0, 0, 0, 0),
             result == 0))
        {
          spell_fizzled = 1;
        }
        else
        {
          generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_PLAYER", 1, NULL);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = valid_target(&td);
      if (result != 0)
      {

        if (instance->info_slot == CHOICE_ARTIFACT)
        {
          result = 0x40;
          type = TYPE_ARTIFACT;
        }
        else if (instance->info_slot == CHOICE_CREATURE)
        {
          type = TYPE_CREATURE;
          result = 2;
        }
        else
        {
          result = 0x100;
          type = TARGET_TYPE_PLANESWALKER;
        }
        if (ai_is_speculating == 1)
        {
          prompt[0] = '\0';
        }
        else
        {
          type_name = type_text(typetext, 100, type, TYPETEXT_0);
          scnprintf(prompt, 200, "Select a %s to_sacrifice.", type_name);
        }
        new_default_test_definition(&test, result, prompt);
        new_sacrifice(player, card, instance->targets[0].player, SAC_CAUSED | SAC_NO_CANCEL, &test);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x21c8760; original cards/war_of_the_spark.c:199. */
int card_angrath_captain_of_chaos(int player, int card, event_t event)
{
  /* Angrath, Captain of Chaos	|2|BR|BR
   * Legendary Planeswalker - Angrath (5)
   * Creatures you control have menace.
   * -2: Amass Zombies 2. (Put two +1/+1 counters on an Army you control. It's also a Zombie. If you don't control an Army, create a 0/0 black Zombie Army creature token first.) */

  check_legend_rule(player, card, event);
  hybrid(player, card, event);
  boost_subtype(player, card, event, ~SUBTYPE_INVALID, 0, 0, 0, SP_KEYWORD_MENACE,
                BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  if (!IS_GAA_EVENT(event))
    return planeswalker(player, card, event, 5);

  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE && count_counters(player, card, COUNTER_LOYALTY) < 2)
    return 0;
  if (event == EVENT_ACTIVATE)
  {
    instance->number_of_targets = 0;
    instance->info_slot = 0;
    int choice = dialog_impl(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL,
                              DLG_NO_STORAGE, DLG_PLANESWALKER,
                              "Amass 2", 1, 5, -2, 0, 0, 0, 0);
    if (!choice)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
    amass(player, card, 2);
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x21c8920; original cards/war_of_the_spark.c:239. */
int card_arboreal_grazer(int player, int card, event_t event)
{
  /* Arboreal Grazer	|G
   * Creature - Sloth Beast 0/3
   * Reach
   * When this creature enters, you may put a land card from your hand onto the battlefield tapped. */

  int result;
  test_definition_t test;
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 1, "Select a land card.");
    test.zone = 0x100;
    result = check_battlefield_for_special_card(player, card, player, 0, &test);
    if (result != 0)
    {
      new_global_tutor(player, player, 4, 5, 0, 1, &test);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21c8a00; original cards/war_of_the_spark.c:268. */
int card_arlinn_voice_of_the_pack(int player, int card, event_t event)
{
  /* Arlinn, Voice of the Pack	|4|G|G
   * Legendary Planeswalker - Arlinn (7)
   * Each creature you control that's a Wolf or a Werewolf enters with an additional +1/+1 counter on it.
   * -2: Create a 2/2 green Wolf creature token. */

  check_legend_rule(player, card, event);
  if (in_play(player, card) && !is_humiliated(player, card, AL_STATIC) &&
      (event == EVENT_CAST_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE) &&
      affected_card_controller == player)
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_CREATURE);
    test.subtype = SUBTYPE_WOLF;
    test.sub2 = SUBTYPE_WEREWOLF;
    test.subtype_flag = F2_MULTISUBTYPE;
    if (new_make_test_in_play(affected_card_controller, affected_card, -1, &test))
      enters_the_battlefield_with_counters(affected_card_controller, affected_card, event,
                                           COUNTER_P1_P1, 1);
  }

  if (!IS_GAA_EVENT(event))
    return planeswalker(player, card, event, 7);
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE && count_counters(player, card, COUNTER_LOYALTY) < 2)
    return 0;
  if (event == EVENT_ACTIVATE)
  {
    instance->number_of_targets = 0;
    instance->info_slot = 0;
    int choice = dialog_impl(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL,
                              DLG_NO_STORAGE, DLG_PLANESWALKER,
                              "Create a Wolf", 1, 5, -2, 0, 0, 0, 0);
    if (!choice)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
    generate_token_by_id(player, card, CARD_ID_WOLF);
  return planeswalker(player, card, event, 7);
}

/* ManalinkEh 2022: 0x21c8c50; original cards/war_of_the_spark.c:323. */
int card_ashioks_skulker(int player, int card, event_t event)
{
  /* Ashiok's Skulker	|4|U
   * Creature - Nightmare 3/5
   * |3|U: This creature can't be blocked this turn. */

  int result;
  result = generic_shade(player, card, event, 0, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0x10);
  return result;
}

/* ManalinkEh 2022: 0x21c8cd0; original cards/war_of_the_spark.c:333. */
int card_ashiok_dream_render(int player, int card, event_t event)
{
  /* Ashiok, Dream Render	|1|UB|UB
   * Legendary Planeswalker - Ashiok (5)
   * Spells and abilities your opponents control can't cause their controller to search their library.
   * -1: Target player mills four cards. Then exile each opponent's graveyard. */

  check_legend_rule(player, card, event);
  hybrid(player, card, event);
  if (!IS_GAA_EVENT(event))
    return planeswalker(player, card, event, 5);

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.zone = TARGET_ZONE_PLAYERS;
  td.allow_cancel = 1;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE && !can_target(&td))
    return 0;
  if (event == EVENT_ACTIVATE)
  {
    instance->number_of_targets = 0;
    instance->info_slot = 0;
    int choice = dialog_impl(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL,
                              DLG_NO_STORAGE, DLG_PLANESWALKER,
                              "Mill 4, then exile opp's graveyard", can_target(&td), 5, -1,
                              0, 0, 0, 0);
    if (!choice)
    {
      spell_fizzled = 1;
      return 0;
    }
    new_pick_target(&td, "TARGET_PLAYER", 0, 1);
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    mill(instance->targets[0].player, 4);
    rfg_whole_graveyard(1-player);
  }
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x21c8ed0; original cards/war_of_the_spark.c:411. */
int card_battlefield_promotion(int player, int card, event_t event)
{
  /* Battlefield Promotion	|1|W
   * Instant
   * Put a +1/+1 counter on target creature. That creature gains first strike until end of turn. You gain 2 life. (A creature with first strike deals combat damage before creatures without first strike.) */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {

      instance = get_card_instance(player, card);
      pump_ability_until_eot(player, card, instance->targets[0].player,
                             instance->targets[0].card, 0, 0, 0x100, 0);
      add_counters(player, card, instance->targets[0].player,
                               instance->targets[0].card, COUNTER_P1_P1, 1);
      gain_life(player, 2);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x21c9080; original cards/war_of_the_spark.c:449. */
int card_blast_zone(int player, int card, event_t event)
{
  /* Blast Zone	""
   * Land
   * This land enters with a charge counter on it.
   * |T: Add |C.
   * |X|X, |T: Put X charge counters on this land.
   * |3, |T, Sacrifice this land: Destroy each nonland permanent with mana value equal to the number of charge counters on this land. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_COUNTERS = 2,
    CHOICE_DETONATE = 3
  };

  int choice;
  int value;
  int opponent_cmc;
  card_instance_t *instance;
  unsigned int argument3;
  int charge_counters;
  test_definition_t test;

  enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, 1);
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      instance = get_card_instance(player, card);

      instance->info_slot = 0;
      if (paying_mana() == 0)
      {

        value = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0,
                                          NULL, NULL);
        choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, "Produce mana",
                             1, 8, "Add charge counter", value, 1, "Detonate",
                             generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9, 4, 0, 0,
                                                       0, 0, 0, 0, NULL, NULL),
                             1, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        instance->info_slot = choice;
        if (choice != CHOICE_MANA)
        {
          if (choice == CHOICE_COUNTERS)
          {

            value = has_mana(player, 7, 1);
            argument3 = round_down_value(value - 1);
            value = choose_a_number(player, "Add how many counters?", argument3);
            if (value < 0)
            {
              spell_fizzled = 1;
            }
            else
            {
              add_state(player, card, 0x10);
              tapped_for_mana_color = -1;
            }
            if (charge_mana_for_activated_ability(player, card, value * 2, 0, 0, 0, 0, 0) == 0)
            {
              remove_state(player, card, 0x10);
              spell_fizzled = 1;
            }
            else
            {
              add_state(player, card, 0x10);
              tapped_for_mana_color = -1;
              instance->targets[9].card = value;
            }
            if (ai_is_speculating == 1)
            {

              opponent_cmc = get_most_common_cmc_nonland(1 - player);
              charge_counters = count_counters(player, card, COUNTER_CHARGE);
              value = 1;
              if (player != 1)
              {
                value = -1;
              }
              ai_modifier =
                  ai_modifier + (((charge_counters <= opponent_cmc) ? 32U : 4U)) *
                                    value * charge_counters;
            }
            tapped_for_mana_color = -1;
            return 0;
          }
          if (choice != CHOICE_DETONATE)
          {
            return 0;
          }
          add_state(player, card, 0x10);
          if (generic_activated_ability(player, card, EVENT_ACTIVATE, 8, 3, 0, 0, 0, 0, 0, 0, NULL,
                                        NULL) != 0)
          {
            return 0;
          }
          remove_state(player, card, 0x10);
          return 0;
        }
      }
      else
      {
        instance->info_slot = 1;
      }
      return mana_producer(player, card, EVENT_ACTIVATE);
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_ACTIVATION)
      {
        value = instance->info_slot;
        if (value == 2)
        {
          add_counters(player, card, instance->parent_controller, instance->parent_card,
                       COUNTER_CHARGE, instance->targets[9].card);
          value = instance->info_slot;
        }
        if (value == 3)
        {
          default_test_definition(&test, 1);
          test.type_flag = 1;
          test.cmc = count_counters(player, card, COUNTER_CHARGE);
          new_manipulate_all(player, card, 2, &test, ACT_KILL_DESTROY);
          return 0;
        }
      }
      return 0;
    }
  }
  return mana_producer(player, card, event);
}

/* ManalinkEh 2022: 0x21c95b0; original cards/war_of_the_spark.c:565. */
int card_bolass_citadel(int player, int card, event_t event)
{
  /* Bolas's Citadel	|3|B|B|B
   * Legendary Artifact
   * You may look at the top card of your library any time.
   * You may play lands and cast spells from the top of your library. If you cast a spell this way, pay life equal to its mana value rather than pay its mana cost.
   * |T, Sacrifice ten nonland permanents: Each opponent loses 10 life. */

  enum
  {
    CHOICE_REVEAL = 1,
    CHOICE_PLAY = 2,
    CHOICE_SACLIFE = 3
  };

  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  test_definition_t test;
  new_default_test_definition(&test, 1, "Select an nonland permanent to sacrifice.");
  test.qty = 10;
  test.type_flag = 1;
  if (event == EVENT_RESOLVE_ACTIVATION || event == EVENT_CLEANUP)
  {
    if (event == EVENT_RESOLVE_ACTIVATION && get_card_instance(player, card)->info_slot == 3)
      lose_life(1 - player, 10);
    return 0;
  }
  int top_card = *deck_ptr[player];
  bool human = !(ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0));
  if (event == EVENT_CAN_ACTIVATE)
  {
    bool can_play = top_card != -1;
    if (can_play && human && can_legally_play_iid_now(player, top_card, EVENT_CAN_ACTIVATE) < 99)
      return 1;
    if (can_play && !is_what(-1, top_card, 1))
    {
      int cmc = get_cmc_by_internal_id(top_card);
      if (!can_pay_life_as_cost_for_spells_or_activated_abilities(player, cmc))
        can_play = false;
    }
    if (can_play)
      return can_legally_play_iid_now(player, top_card, EVENT_CAN_ACTIVATE);
    if (!generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL,
                                   NULL))
      return 0;
    return (unsigned int)(new_can_sacrifice_as_cost(player, card, &test) != 0);
  }
  uint32_t can_look = human && top_card != -1;
  unsigned int can_play = 0;
  if (top_card != -1)
  {
    bool can_pay = true;
    if (!is_what(-1, top_card, 1))
    {
      int cmc = get_cmc_by_internal_id(top_card);
      can_pay = can_pay_life_as_cost_for_spells_or_activated_abilities(player, cmc) != 0;
    }
    if (can_pay)
      can_play = can_legally_play_iid_now(player, *deck_ptr[player], EVENT_ACTIVATE) != 0;
  }
  int can_sacrifice = 0;
  if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL,
                                NULL))
    can_sacrifice = new_can_sacrifice_as_cost(player, card, &test) != 0;
  int choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
                           "Check top card of deck", can_look, 0, "Play top card", can_play, 10,
                           "Sac 10 nonland permanents", can_sacrifice, 10, 0, 0, 0, 0);
  if (!choice)
  {
    spell_fizzled = 1;
    return 0;
  }
  if (choice == CHOICE_REVEAL)
  {
    show_deck(player, deck_ptr[player], 1, "Here's the top card of your deck", 0,
              gs_done_008b40e0);
    spell_fizzled = 1;
    return 0;
  }
  if (choice == CHOICE_PLAY)
  {
    lose_life(player, get_cmc_by_internal_id(top_card));
    play_card_in_deck_for_free(player, player, 0);
    spell_fizzled = 1;
    cant_be_responded_to = 1;
    return 0;
  }
  if (choice != CHOICE_SACLIFE ||
      !generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL))
    return 0;
  test.qty = 1;
  int sacrifices[10] = {0};
  bool complete = true;
  for (int index = 0; index < 10; ++index)
  {
    int selected =
        new_sacrifice(player, card, player, SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
    sacrifices[index] = selected;
    if (!selected)
    {
      remove_state(player, card, 0x10);
      spell_fizzled = 1;
      complete = false;
      break;
    }
  }
  for (int index = 0; index < 10; ++index)
  {
    unsigned int selected = sacrifices[index];
    if (selected)
    {
      spell_fizzled = 1;
      state_untargettable((int)selected >> 16 & 0xff, selected >> 24, 0);
    }
  }
  if (!complete)
    return 0;
  for (int index = 0; index < 10; ++index)
  {
    unsigned int selected = sacrifices[index];
    if (selected)
      kill_card((int)selected >> 16 & 0xff, selected >> 24, KILL_SACRIFICE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x21c9c50; original cards/war_of_the_spark.c:727. */
int card_casualties_of_war(int player, int card, event_t event)
{
  /* Casualties of War	|2|B|B|G|G
   * Sorcery
   * Choose one or more -
   * * Destroy target artifact.
   * * Destroy target creature.
   * * Destroy target enchantment.
   * * Destroy target land.
   * * Destroy target planeswalker. */

  int i;
  card_instance_t *instance;
  target_t *slot;
  int result;
  target_t *selected;
  target_definition_t td;
  target_definition_t td1;
  target_definition_t td2;
  target_definition_t td3;
  target_definition_t td4;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x40);
    default_target_definition(player, card, &td1, 2);
    default_target_definition(player, card, &td2, 4);
    default_target_definition(player, card, &td3, 1);
    default_target_definition(player, card, &td4, 0x100);
    get_card_instance(player, card);
    result = can_target(&td);
    if (((result != 0) || (result = can_target(&td1), result != 0)) ||
        ((result = can_target(&td2),
          result != 0 || ((result = can_target(&td3),
                           result != 0 || (result = can_target(&td4), result != 0))))))
    {
      return basic_spell(player, card, EVENT_CAN_CAST);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x40);
      default_target_definition(player, card, &td1, 2);
      default_target_definition(player, card, &td2, 4);
      default_target_definition(player, card, &td3, 1);
      default_target_definition(player, card, &td4, 0x100);
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {

        selected = instance->targets;
        instance->number_of_targets = '\0';
        slot = selected;
        do
        {
          slot->card = -1;
          slot->player = -1;
          slot = slot + 1;
        } while (instance->targets + 10 != slot);
        result = can_target(&td);
        if ((result != 0) && (result = new_pick_target(&td, "TARGET_ARTIFACT", 0, 0), result != 0))
        {

          state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
          instance->targets[instance->number_of_targets + 4].player = 1;
        }
        result = can_target(&td1);
        if ((result != 0) &&
            (result = new_pick_target(&td1, "TARGET_CREATURE", -1, 0), result != 0))
        {

          state_untargettable(instance->targets[instance->number_of_targets - 1].player,
                              instance->targets[instance->number_of_targets - 1].card, 1);
          instance->targets[instance->number_of_targets + 4].player = 2;
        }
        result = can_target(&td2);
        if ((result != 0) &&
            (result = new_pick_target(&td2, "TARGET_ENCHANTMENT", -1, 0), result != 0))
        {

          instance->targets[instance->number_of_targets + 4].player = 4;
        }
        result = can_target(&td3);
        if ((result != 0) && (result = new_pick_target(&td3, "TARGET_LAND", -1, 0), result != 0))
        {

          instance->targets[instance->number_of_targets + 4].player = 8;
        }
        result = can_target(&td4);
        if ((result != 0) &&
            (result = new_pick_target(&td4, "TARGET_PLANESWALKER", -1, 0), result != 0))
        {

          instance->targets[instance->number_of_targets + 4].player = 0x10;
        }
        do
        {
          if ((selected->player != -1) && (selected->card != -1))
          {
            state_untargettable(selected->player, selected->card, 0);
          }
          selected = selected + 1;
        } while (instance->targets + 5 != selected);
        if (instance->number_of_targets == '\0')
        {
          spell_fizzled = 1;
        }
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x40);
    default_target_definition(player, card, &td1, 2);
    default_target_definition(player, card, &td2, 4);
    default_target_definition(player, card, &td3, 1);
    default_target_definition(player, card, &td4, 0x100);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      target_definition_t *definitions[5] = {&td, &td1, &td2, &td3, &td4};
      for (i = 0; i < (int)(unsigned int)instance->number_of_targets; ++i)
      {
        unsigned int flags = instance->targets[i + 5].player;
        if ((int)flags < 0)
          continue;
        bool valid = false;
        for (int type = 0; type < 5; ++type)
          if (flags & (1U << type))
          {
            if (validate_target(player, card, definitions[type], i))
              valid = true;
            flags = instance->targets[i + 5].player;
          }
        if (valid)
          kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21ca3e0; original cards/war_of_the_spark.c:873. */
int card_chandra_fire_artisan(int player, int card, event_t event)
{
  /* Chandra, Fire Artisan	|2|R|R
   * Legendary Planeswalker - Chandra (4)
   * Whenever one or more loyalty counters are removed from Chandra, she deals that much damage to target opponent or planeswalker.
   * +1: Exile the top card of your library. You may play it this turn.
   * -7: Exile the top seven cards of your library. You may play them this turn. */

  enum
  {
    CHOICE_ONE = 1,
    CHOICE_SEVEN = 2
  };

  int i;
  int top_card;
  card_instance_t *instance;
  int value;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (event == EVENT_COUNTERS)
  {
    if (((card == affected_card) && (player == affected_card_controller)) && (counters_added < 0))
    {
      opponent_or_pw_target_definition(player, card, &td);
      value = can_target(&td);
      if ((value != 0) &&
          (value = new_pick_target(&td, "TARGET_OPPONENT_OR_PLANESWALKER", 0, 1), value != 0))
      {
        damage_target0(player, card, -counters_added);
      }
    }
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
        if (dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Exile one card", 1,
                        5, 1, "Exile seven cards", 1, 10, 0xfffffff9, 0, 0, 0, 0) == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        return planeswalker(player, card, event, 4);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return planeswalker(player, card, event, 4);
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (instance->info_slot == CHOICE_ONE)
      {

        value = *deck_ptr[player];
        if (value != -1)
        {
          rfg_top_card_of_deck(player);
          create_may_play_card_from_exile_effect(
              player, card, player, (unsigned int)cards_data[value].id, MPCFE_UNTIL_EOT);
        }
      }
      else if (instance->info_slot == CHOICE_SEVEN)
      {
        i = 7;
        do
        {

          top_card = *deck_ptr[player];
          if (top_card != -1)
          {
            rfg_top_card_of_deck(player);
            create_may_play_card_from_exile_effect(
                player, card, player, (unsigned int)cards_data[top_card].id, MPCFE_UNTIL_EOT);
          }
          i = i + -1;
        } while (i != 0);
      }
    }
  }

  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x21c63f0; original cards/war_of_the_spark.c:1035. */
static int cruel_celebrant_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (event == EVENT_STATIC_EFFECTS && card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player >= 0 &&
        get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_CRUEL_CELEBRANT)
      kill_card(player, card, KILL_EXILE);
  }
  instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0 &&
      effect_follows_control_of_attachment(player, card, event))
    return 0;
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    instance = get_card_instance(player, card);
    if (in_play(affected_card_controller, affected_card) &&
        is_this_dying(affected_card_controller, affected_card))
    {
      if (instance->damage_target_card == affected_card &&
          instance->damage_target_player == affected_card_controller)
      {
        if (!is_what(instance->damage_target_player, instance->damage_target_card, 2))
          count_for_gfp_ability(player, card, EVENT_GRAVEYARD_FROM_PLAY, player, 0x47, NULL);
        instance->damage_target_card = -1;
        instance->damage_target_player = -1;
        remove_status(player, card, 0x10000);
        if (instance->targets[11].player < 0)
          instance->targets[11].player = 0;
        *((uint8_t *)&instance->targets[11].player + 2) = 1;
      }
      count_for_gfp_ability(player, card, EVENT_GRAVEYARD_FROM_PLAY, player, 2, NULL);
    }
  }
  unsigned int deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (deaths && !check_status(player, card, 0x10000))
  {
    for (deaths &= 0xff; deaths; --deaths)
    {
      lose_life(1 - player, 1);
      gain_life(player, 1);
    }
    kill_card(player, card, KILL_EXILE);
  }
  if (event == EVENT_CLEANUP && !check_status(player, card, 0x10000))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x21caa00; original cards/war_of_the_spark.c:1090. */
int card_cruel_celebrant(int player, int card, event_t event)
{
  /* Cruel Celebrant	|W|B
   * Creature - Vampire 1/2
   * Whenever this creature or another creature or planeswalker you control dies, each opponent loses 1 life and you gain 1 life. */

  int c;
  int legacy_or_result;
  card_instance_t *instance;
  unsigned int deaths;

  if (is_humiliated(player, card, AL_TRIG) == 0)
  {
    if (event == EVENT_STATIC_EFFECTS)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {

        c = 0;
        bool found = false;
        if (0 < active_cards_count[player])
        {
          do
          {
            instance = in_play(player, c);

            if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
                 (instance = get_card_instance(player, c),
                  instance->info_slot_fn == cruel_celebrant_effect)) &&
                (((instance->damage_target_player == player &&
                   (instance->damage_target_card == card)) ||
                  ((instance->targets[0].player == player &&
                    (instance->targets[0].card == card))))))
            {
              found = true;
              break;
            }
            c = c + 1;
          } while (c < active_cards_count[player]);
        }

        if (!found)
        {
          legacy_or_result =
              create_targetted_legacy_effect(player, card, cruel_celebrant_effect, player, card);
          instance = get_card_instance(player, legacy_or_result);
          instance->targets[0].player = player;
          instance->targets[0].card = card;
          instance->number_of_targets = '\x01';
          add_status(player, legacy_or_result, 0x10000);
        }
      }
    }
    else if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {
      count_for_gfp_ability(player, card, 0x77, player, 2, NULL);
    }
    instance = get_card_instance(player, card);

    if (((instance->kill_code == '\0') &&
         (deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY),
          deaths != 0)) &&
        (deaths = deaths & 0xff, deaths != 0))
    {
      do
      {
        lose_life(1 - player, 1);
        deaths = deaths - 1;
        gain_life(player, 1);
      } while (0 < (int)deaths);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21cabd0; original cards/war_of_the_spark.c:1167. */
int card_davriel_rogue_shadowmage(int player, int card, event_t event)
{
  /* Davriel, Rogue Shadowmage	|2|B
   * Legendary Planeswalker - Davriel (3)
   * At the beginning of each opponent's upkeep, if that player has one or fewer cards in hand, Davriel deals 2 damage to them.
   * -1: Target player discards a card. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    instance->targets[1].player = 1 - player;
  }
  instance = get_card_instance(player, card);
  if (-1 < instance->targets[1].player)
  {
    instance = get_card_instance(player, card);
    upkeep_trigger_ability(player, card, event, instance->targets[0].player);
    if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    {

      instance = get_card_instance(player, card);
      result = instance->targets[1].player;
      if (hand_count[result] < 2)
      {
        damage_player(result, 2, player, card);
      }
      return planeswalker(player, card, event, 3);
    }
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    td.allow_cancel = 1;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = can_target(&td);
      if (result == 0)
      {
        return 0;
      }
      return planeswalker(player, card, event, 3);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 3);
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    td.allow_cancel = 1;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_ACTIVATE)
  {

    instance->number_of_targets = '\0';
    instance->info_slot = 0;
    result = can_target(&td);
    result =
        dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL, DLG_NO_STORAGE,
                    DLG_PLANESWALKER, "Target player discards", result, 5, 0xffffffff, 0, 0, 0, 0);
    if (result == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    new_pick_target(&td, "TARGET_PLAYER", 0, 1);
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    discard(instance->targets[0].player, 2, player);
  }
  return planeswalker(player, card, event, 3);
}

/* ManalinkEh 2022: 0x21cae80; original cards/war_of_the_spark.c:1229. */
int card_deathsprout(int player, int card, event_t event)
{
  /* Deathsprout	|1|B|B|G
   * Instant
   * Destroy target creature. Search your library for a basic land card, put it onto the battlefield tapped, then shuffle. */

  card_instance_t *instance;
  target_definition_t td1;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td1, 2);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td1))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      tutor_basic_land(player, 1, 1);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td1, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x21cafe0; original cards/war_of_the_spark.c:1264. */
int card_despark(int player, int card, event_t event)
{
  /* Despark	|W|B
   * Instant
   * Exile target permanent with mana value 4 or greater. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.special = TARGET_SPECIAL_CMC_GREATER_OR_EQUAL;
  td.extra = 4;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td,
                         "Select target permanent with converted mana cost 4 or greater.", 1, NULL);
}

/* ManalinkEh 2022: 0x21cb150; original cards/war_of_the_spark.c:1321. */
int card_domris_ambush(int player, int card, event_t event)
{
  /* Domri's Ambush	|R|G
   * Sorcery
   * Put a +1/+1 counter on target creature you control. Then that creature deals damage equal to its power to target creature or planeswalker you don't control. */

  int target_player;
  card_instance_t *instance;
  int result;
  int amt;
  target_definition_t td_control;
  target_definition_t td_dont;

  if (event == EVENT_CHECK_PUMP)
  {
    instance = get_card_instance(player, card);
    result = can_play_iid(player, 0x3b, instance->internal_card_id);
    if (result != 0)
    {
      pumpable_power[player] = pumpable_power[player] + 1;
      pumpable_toughness[player] = pumpable_toughness[player] + 1;
    }
  }
  else if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td_control, 2);
    td_control.preferred_controller = player;
    td_control.allowed_controller = player;
    default_target_definition(player, card, &td_dont, 0x102);
    td_dont.allowed_controller = 1 - player;
    td_dont.preferred_controller = td_dont.allowed_controller;
    result = can_target(&td_control);
    if (result != 0)
    {
      result = can_target(&td_dont);
      return (unsigned int)(result != 0);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td_control, 2);
      td_control.preferred_controller = player;
      td_control.allowed_controller = player;
      default_target_definition(player, card, &td_dont, 0x102);
      td_dont.allowed_controller = 1 - player;
      td_dont.preferred_controller = td_dont.allowed_controller;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        result = new_pick_target(&td_control, "ASHNODS_BATTLEGEAR", 0, 1);
        if (result != 0)
        {
          new_pick_target(&td_dont, "Select target creature or planeswalker you don\'t control.", 1,
                          0x81);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td_control, 2);
    td_control.preferred_controller = player;
    td_control.allowed_controller = player;
    default_target_definition(player, card, &td_dont, 0x102);
    td_dont.allowed_controller = 1 - player;
    td_dont.preferred_controller = td_dont.allowed_controller;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = validate_target(player, card, &td_control, 0);
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_P1_P1, 1);
        result = validate_target(player, card, &td_dont, 1);
        if (result != 0)
        {
          result = instance->targets[0].card;
          target_player = instance->targets[0].player;
          amt = get_power(target_player, result);
          damage_creature(instance->targets[1].player, instance->targets[1].card, amt, target_player,
                          result);
        }
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21c6700; original cards/war_of_the_spark.c:1373. */
int insist_until_eot(int player, int card, event_t event)
{
  if (event == EVENT_CAST_SPELL && card != affected_card && player == affected_card_controller &&
      is_what(player, affected_card, 2))
    state_untargettable(affected_card_controller, affected_card, 1);
  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && card == affected_card && player == affected_card_controller &&
      reason_for_trigger_controller == affected_card_controller &&
      is_what(trigger_cause_controller, trigger_cause, 2) &&
      check_state(trigger_cause_controller, trigger_cause, 0x100000))
  {
    if (event == EVENT_TRIGGER)
      event_result |= 2;
    else if (event == EVENT_RESOLVE_TRIGGER)
      state_untargettable(trigger_cause_controller, trigger_cause, 0);
  }
  if (eot_trigger(player, card, event))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x21cb4d0; original cards/war_of_the_spark.c:1397. */
int card_domri_anarch_of_bolas(int player, int card, event_t event)
{
  /* Domri, Anarch of Bolas	|1|R|G
   * Legendary Planeswalker - Domri (3)
   * Creatures you control get +1/+0.
   * +1: Add |R or |G. Creature spells you cast this turn can't be countered.
   * -2: Target creature you control fights target creature you don't control. */

  enum
  {
    CHOICE_MANA_INSIST = 1,
    CHOICE_FIGHT = 2
  };

  card_instance_t *instance;
  int result;
  unsigned int can_fight;
  target_definition_t *opponent_target;
  target_definition_t *own_target;
  target_definition_t td;
  target_definition_t td1;

  check_legend_rule(player, card, event);
  boost_subtype(player, card, event, ~SUBTYPE_INVALID, 1, 0, 0, 0,
                BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  instance = get_card_instance(player, card);
  if (event == EVENT_COUNT_MANA)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (0 < instance->targets[9].card))
    {
      declare_mana_available_hex(player, 0x18, 1);
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = player;
      td.preferred_controller = player;
      default_target_definition(player, card, &td1, 2);
      td1.allowed_controller = 1 - player;
      td1.preferred_controller = td1.allowed_controller;
      if (event == EVENT_ACTIVATE)
      {

        instance->number_of_targets = '\0';
        instance->info_slot = 0;
        result = can_target(&td);
        can_fight = 0;
        if (result != 0)
        {
          result = can_target(&td1);
          can_fight = (unsigned int)(result != 0);
        }
        result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                             "Mana & uncounterable", 1, 5, 1, "Fight!", can_fight, 7, 0xfffffffe, 0,
                             0, 0, 0);
        if (result == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (result == CHOICE_MANA_INSIST)
        {
          produce_mana_any_combination_of_colors(player, COLOR_TEST_RED | COLOR_TEST_GREEN, 1,
                                                 NULL);
          tapped_for_mana_color = -2;
          create_legacy_effect(player, card, insist_until_eot);
        }
        else if (result == CHOICE_FIGHT)
        {
          instance->number_of_targets = '\0';
          result = new_pick_target(&td, "Select target creature you control.", 0, 0x81);
          if (result != 0)
          {
            new_pick_target(&td1, "Select target creature opponent controls.", 1, 0x81);
          }
        }
        return planeswalker(player, card, event, 3);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return planeswalker(player, card, event, 3);
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = player;
      td.preferred_controller = player;
      default_target_definition(player, card, &td1, 2);
      td1.preferred_controller = 1 - player;
    }
    own_target = &td;
    opponent_target = &td1;
    td1.allowed_controller = td1.preferred_controller;

    if ((((event == EVENT_RESOLVE_ACTIVATION) && (instance->info_slot == CHOICE_FIGHT)) &&
         (result = validate_target(player, card, own_target, 0), result != 0)) &&
        (result = validate_target(player, card, opponent_target, 1), result != 0))
    {
      fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player,
            instance->targets[1].card);
    }
  }
  return planeswalker(player, card, event, 3);
}

/* ManalinkEh 2022: 0x21cb8e0; original cards/war_of_the_spark.c:1471. */
int card_dovins_veto(int player, int card, event_t event)
{
  /* Dovin's Veto	|W|U
   * Instant
   * This spell can't be countered.
   * Counter target noncreature spell. */

  int result;

  cannot_be_countered(player, card, event);
  result = card_negate(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x21cbca0; original cards/war_of_the_spark.c:1559. */
int card_dreadhorde_arcanist(int player, int card, event_t event)
{
  /* Dreadhorde Arcanist	|1|R
   * Creature - Zombie Wizard 1/3
   * Trample
   * Whenever this creature attacks, you may cast target instant or sorcery card with mana value less than or equal to this creature's power from your graveyard without paying its mana cost. If that spell would be put into your graveyard, exile it instead. */

  int result;
  char buffer[75];
  test_definition_t test;

  result = duh_mode(player);
  dat_ability(player, card, event, (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    result = get_power(player, card);
    scnprintf(buffer, 0x4b,
              " Select an instant or sorcery card with converted mana cost %d or less.",
              result + 1);
    new_default_test_definition(&test, 0x38, buffer);
    result = get_power(player, card);
    test.cmc = result + 1;
    test.cmc_flag = 3;
    result = new_special_count_grave(player, &test);
    if (result != 0)
    {

      result = new_select_a_card(player, player, 2, 0, 1, -1, &test);
      if (result != -1)
      {
        play_card_in_grave_for_free_and_exile_it(player, player, result);
      }
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21cbdd0; original cards/war_of_the_spark.c:1588. */
int card_dreadhorde_butcher(int player, int card, event_t event)
{
  /* Dreadhorde Butcher	|B|R
   * Creature - Zombie Warrior 1/1
   * Haste
   * Whenever this creature deals combat damage to a player or planeswalker, put a +1/+1 counter on this creature.
   * When this creature dies, it deals damage equal to its power to any target. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  haste(player, card, event);
  result = damage_dealt_by_me(player, card, event,
                              DDBM_MAY_DAMAGE_PLANESWALKER | DDBM_MUST_DAMAGE_PLAYER);
  if (result != 0)
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  }
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {

    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_ANY");
      if (result != 0)
      {
        result = get_power(player, card);
        damage_target_creature_or_player(instance->targets[0].player, instance->targets[0].card,
                                         result, player, card);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21cbef0; original cards/war_of_the_spark.c:1618. */
int card_dreadhorde_invasion(int player, int card, event_t event)
{
  /* Dreadhorde Invasion	|1|B
   * Enchantment
   * At the beginning of your upkeep, you lose 1 life and amass Zombies 1. (Put a +1/+1 counter on an Army you control. It's also a Zombie. If you don't control an Army, create a 0/0 black Zombie Army creature token first.)
   * Whenever a Zombie token you control with power 6 or greater attacks, it gains lifelink until end of turn. */

  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    lose_life(player, 1);
    amass(player, card, 1);
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  }
  else if (event == EVENT_DECLARE_ATTACKERS)
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_CREATURE);
    test.subtype = SUBTYPE_ZOMBIE;
    test.type_flag = 6;
    test.power = 5;
    test.power_flag = F5_POWER_GREATER_THAN_VALUE;
    store_attackers(player, card, event, DAT_TRACK | DAT_RESOLVE_TRIGGER_MANDATORY, player, -1, &test);
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  }
  else
  {
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      card_instance_t *instance = get_card_instance(player, card);
      const uint8_t *attackers = (const uint8_t *)&instance->targets + 2 * sizeof(target_t);
      for (int index = instance->info_slot - 1; index >= 0; --index)
        if (in_play(current_turn, attackers[index]))
          pump_ability_until_eot(player, card, current_turn, attackers[index], 0, 0, 0,
                                  SP_KEYWORD_LIFELINK);
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x21cc0f0; original cards/war_of_the_spark.c:1682. */
int card_elite_guardmage(int player, int card, event_t event)
{
  /* Elite Guardmage	|2|W|U
   * Creature - Human Wizard 2/3
   * Flying
   * When this creature enters, you gain 3 life and draw a card. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  gain_life(player, 3);
  draw_a_card(player);
  return 0;
}

/* ManalinkEh 2022: 0x21cc520; original cards/war_of_the_spark.c:1798. */
int card_enter_the_god_eternals(int player, int card, event_t event)
{
  /* Enter the God-Eternals	|2|U|U|B
   * Sorcery
   * ~ deals 4 damage to target creature and you gain life equal to the damage dealt this way. Target player mills four cards. Amass Zombies 4. (Put four +1/+1 counters on an Army you control. It's also a Zombie. If you don't control an Army, create a 0/0 black Zombie Army creature token first.) */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  target_definition_t td1;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    default_target_definition(player, card, &td1, 0x100);
    td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    get_card_instance(player, card);
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    if (result == 0)
    {
      return 0;
    }
    return can_target(&td1);
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
    default_target_definition(player, card, &td, 2);
    default_target_definition(player, card, &td1, 0x100);
    td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    instance = get_card_instance(player, card);
    if (card != affected_card)
    {
      return 0;
    }
    if (player != affected_card_controller)
    {
      return 0;
    }
    instance->number_of_targets = '\0';
    result = pick_target(&td, "TARGET_CREATURE");
    if (result == 0)
    {
      return 0;
    }
    new_pick_target(&td1, "TARGET_PLAYER_OR_PLANESWALKER", 1, 1);
    return 0;
  }
  if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
      (event != EVENT_CHANGE_TARGET))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  default_target_definition(player, card, &td1, 0x100);
  td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  instance = get_card_instance(player, card);
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }

  bool creature_valid = valid_target(&td) != 0;
  if (creature_valid)
  {
    damage_target0(player, card, 4);
    gain_life(player, 4);
  }
  bool player_valid = validate_target(player, card, &td1, 1) != 0;
  if (player_valid)
    mill(instance->targets[1].player, 4);
  if (creature_valid || player_valid)
    amass(player, card, 4);
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

/* ManalinkEh 2022: 0x21cc800; original cards/war_of_the_spark.c:1851. */
int card_erratic_visionary(int player, int card, event_t event)
{
  /* Erratic Visionary	|1|U
   * Creature - Human Wizard 1/3
   * |1|U, |T: Draw a card, then discard a card. */

  int result;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    draw_cards(player, 1);
    discard(player, 0, player);
  }
  result = generic_activated_ability(player, card, event, 1, 1, 0, 1, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x21cc8b0; original cards/war_of_the_spark.c:1882. */
int card_evolution_sage(int player, int card, event_t event)
{
  /* Evolution Sage	|2|G
   * Creature - Elf Druid 3/2
   * Landfall - Whenever a land you control enters, proliferate. (Choose any number of permanents and/or players, then give each another counter of each kind already there.) */

  landfall(player, card, event);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  proliferate(player, card);
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21cc900; original cards/war_of_the_spark.c:1896. */
int card_fblthp_the_lost(int player, int card, event_t event)
{
  /* Fblthp, the Lost	|1|U
   * Legendary Creature - Homunculus 1/1
   * When Fblthp enters, draw a card. If it entered from your library or was cast from your library, draw two cards instead.
   * When Fblthp becomes the target of a spell, shuffle Fblthp into its owner's library. */

  const target_t *spell_target;
  card_instance_t *instance;
  int result;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    result = check_special_flags(player, card, 0x10000);
    draw_cards(player, (result != 0) + 1);
  }
  spell_target =
      becomes_target_of_spell(player, card, event, player, card, 2, RESOLVE_TRIGGER_MANDATORY);
  if (spell_target != NULL)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_TRIG);
      if (result == 0)
      {
        shuffle_into_library(player, card);
      }
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21cc9e0; original cards/war_of_the_spark.c:1923. */
int card_finale_of_devastation(int player, int card, event_t event)
{
  /* Finale of Devastation	|X|G|G
   * Sorcery
   * Search your library and/or graveyard for a creature card with mana value X or less and put it onto the battlefield. If you search your library this way, shuffle. If X is 10 or more, creatures you control get +X/+X and gain haste until end of turn. */

  card_instance_t *instance;
  int x;
  int grave_result;
  char buffer[100];
  test_definition_t test;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    x = instance->info_slot;
    scnprintf(buffer, 100, "Select a creature card with CMC %d or less", x);
    new_default_test_definition(&test, 2, buffer);
    test.cmc = x + 1;
    test.cmc_flag = 3;
    grave_result = new_special_count_grave(player, &test);
    if ((grave_result == 0) ||
        (grave_result = new_global_tutor(player, player, 2, 4, 0, 2, &test), grave_result < 0))
    {
      new_global_tutor(player, player, 1, 4, 0, 2, &test);
    }
    if (9 < x)
    {
      pump_subtype_until_eot(player, card, player, -1, x, x, 0, 4);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int massacre_girl_legacy(int player, int card, event_t event);
static int colors_to_guilds(int iid);
int card_return_to_nature(int player, int card, event_t event);

/* ManalinkEh 2022: 0x21ccbf0; original cards/war_of_the_spark.c:1960. */
int card_finale_of_eternity(int player, int card, event_t event)
{
  /* Finale of Eternity	|X|B|B
   * Sorcery
   * Destroy up to three target creatures with toughness X or less. If X is 10 or more, return all creature cards from your graveyard to the battlefield. */

  target_t *target;
  target_t *selected_target;
  card_instance_t *instance;
  unsigned int x;
  int i;
  target_definition_t td;
  test_definition_t test;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 2);
  td.toughness_requirement = x_value | 0x2000;
  td.allow_cancel = 2;
  if (event == EVENT_RESOLVE_SPELL)
  {
    x = instance->info_slot;
    td.toughness_requirement = x | 0x2000;
    if (instance->number_of_targets != '\0')
    {
      i = 0;
      do
      {
        if (validate_target(player, card, &td, i) == 0)
        {
          i = i + 1;
          continue;
        }
        selected_target = instance->targets + i;
        target = instance->targets + i;
        i = i + 1;
        kill_card(target->player, selected_target->card, KILL_DESTROY);
      } while (i < (int)(unsigned int)instance->number_of_targets);
      x = instance->info_slot;
    }
    if (9 < (int)x)
    {
      default_test_definition(&test, 2);
      new_reanimate_all(player, card, player, &test, REANIMATE_DEFAULT);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x24, &td, "TARGET_CREATURE", 3, NULL);
}

/* ManalinkEh 2022: 0x21cce20; original cards/war_of_the_spark.c:1997. */
int card_finale_of_glory(int player, int card, event_t event)
{
  /* Finale of Glory	|X|W|W
   * Sorcery
   * Create X 2/2 white Soldier creature tokens with vigilance. If X is 10 or more, also create X 4/4 white Angel creature tokens with flying and vigilance. */

  card_instance_t *instance;
  token_generation_t token;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_token_definition(player, card, CARD_ID_SOLDIER, &token);
    token.qty = (unsigned char)instance->info_slot;
    token.s_key_plus = 8;
    generate_token(&token);
    if (9 < instance->info_slot)
    {
      default_token_definition(player, card, CARD_ID_ANGEL, &token);
      token.s_key_plus = 8;
      token.qty = (unsigned char)instance->info_slot;
      generate_token(&token);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
}

/* ManalinkEh 2022: 0x21ccf80; original cards/war_of_the_spark.c:2028. */
int card_finale_of_promise(int player, int card, event_t event)
{
  /* Finale of Promise	|X|R|R
   * Sorcery
   * You may cast up to one target instant card and/or up to one target sorcery card from your graveyard each with mana value X or less without paying their mana costs. If a spell cast this way would be put into your graveyard, exile it instead. If X is 10 or more, copy each of those spells twice. You may choose new targets for the copies. */

  card_instance_t *instance;
  int value;
  int hand_card;
  int mode_or_target;
  char buffer_i[100];
  char buffer_s[100];
  test_definition_t test_i;
  test_definition_t test_s;

  if (event == EVENT_CAN_CAST)
  {
    get_card_instance(player, card);
    scnprintf(buffer_i, 100, "Select an instant card with cmc %d or less", x_value);
    new_default_test_definition(&test_i, 0x30, buffer_i);
    scnprintf(buffer_s, 100, "Select a sorcery card with cmc %d or less", x_value);
    new_default_test_definition(&test_s, 8, buffer_s);
    mode_or_target = generic_spell(player, card, EVENT_CAN_CAST, 0xc, NULL, NULL, 1, &test_i);
    value = 1;
    if (mode_or_target == 0)
    {
      value = generic_spell(player, card, EVENT_CAN_CAST, 0xc, NULL, NULL, 1, &test_s);
    }
  }
  else
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance = get_card_instance(player, card);
        scnprintf(buffer_i, 100, "Select an instant card with cmc %d or less", x_value);
        new_default_test_definition(&test_i, 0x30, buffer_i);
        scnprintf(buffer_s, 100, "Select a sorcery card with cmc %d or less", x_value);
        new_default_test_definition(&test_s, 8, buffer_s);
        if ((card == affected_card) && (player == affected_card_controller))
        {
          generic_spell(player, card, EVENT_CAST_SPELL, 4, NULL, NULL, 1, NULL);
          test_i.cmc = x_value + 1;
          test_i.cmc_flag = 3;
          test_s.cmc_flag = 3;
          test_s.cmc = test_i.cmc;
          if (new_special_count_grave(player, &test_i) != 0)
          {
            value = new_select_a_card(player, player, 2, 0, 2, -1, &test_i);
            if (value != -1)
            {
              instance->targets[0].player = value;
              value = turn_card_in_grave_face_down(player, value);
              instance->targets[0].card = value;
            }
          }
          if (new_special_count_grave(player, &test_s) != 0)
          {
            value = new_select_a_card(player, player, 2, 0, 2, -1, &test_s);
            if (value != -1)
            {
              instance->targets[1].player = value;
              instance->targets[1].card = graveyard_ptr_mutable[player][value];
            }
          }
          value = instance->targets[0].player;
          if (value != -1)
          {
            turn_card_in_grave_face_up(player, value, instance->targets[0].card);
          }
        }
      }
    }
    else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      instance = get_card_instance(player, card);
      scnprintf(buffer_i, 100, "Select an instant card with cmc %d or less", x_value);
      new_default_test_definition(&test_i, 0x30, buffer_i);
      scnprintf(buffer_s, 100, "Select a sorcery card with cmc %d or less", x_value);
      new_default_test_definition(&test_s, 8, buffer_s);
      if (event == EVENT_RESOLVE_SPELL)
      {
        value = instance->targets[0].card;
        mode_or_target = instance->targets[1].card;
        if (value != -1)
        {
          if (can_legally_play_iid(player, value) != 0)
          {
            remove_card_from_grave(player, instance->targets[0].player);
            hand_card = add_card_to_hand(player, value);
            if (real_play_spell_for_free(player, hand_card, 0x10100000) == 0)
            {
              obliterate_card(player, hand_card);
            }
            else if (9 < instance->info_slot)
            {
              copy_spell(player, (unsigned int)cards_data[value].id);
              copy_spell(player, (unsigned int)cards_data[value].id);
            }
          }
        }
        if (mode_or_target != -1)
        {
          if (can_legally_play_iid(player, mode_or_target) != 0)
          {
            remove_card_from_grave(player, instance->targets[1].player);
            value = add_card_to_hand(player, mode_or_target);
            if (real_play_spell_for_free(player, value, 0x10100000) == 0)
            {
              obliterate_card(player, value);
            }
            else if (9 < instance->info_slot)
            {
              copy_spell(player, (unsigned int)cards_data[mode_or_target].id);
              copy_spell(player, (unsigned int)cards_data[mode_or_target].id);
            }
          }
        }
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
    }
    else if (event == EVENT_CHANGE_TARGET)
    {
      get_card_instance(player, card);
      scnprintf(buffer_i, 100, "Select an instant card with cmc %d or less", x_value);
      new_default_test_definition(&test_i, 0x30, buffer_i);
      scnprintf(buffer_s, 100, "Select a sorcery card with cmc %d or less", x_value);
      new_default_test_definition(&test_s, 8, buffer_s);
    }
    value = 0;
  }
  return value;
}

/* ManalinkEh 2022: 0x21cd630; original cards/war_of_the_spark.c:2121. */
int card_finale_of_revelation(int player, int card, event_t event)
{
  /* Finale of Revelation	|X|U|U
   * Sorcery
   * Draw X cards. If X is 10 or more, instead shuffle your graveyard into your library, draw X cards, untap up to five lands, and you have no maximum hand size for the rest of the game.
   * Exile ~. */

  card_instance_t *instance;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (instance->info_slot < 10)
    {
      draw_cards(player, instance->info_slot);
    }
    else
    {
      reshuffle_grave_into_deck(player, 0);
      draw_cards(player, instance->info_slot);
      untap_lands(player, card, 5);
      create_legacy_effect(player, card, card_spellbook);
    }
    kill_card(player, card, KILL_EXILE);
  }
  return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
}

/* ManalinkEh 2022: 0x21cd7f0; original cards/war_of_the_spark.c:2162. */
int card_flux_channeler(int player, int card, event_t event)
{
  /* Flux Channeler	|2|U
   * Creature - Human Wizard 2/2
   * Whenever you cast a noncreature spell, proliferate. (Choose any number of permanents and/or players, then give each another counter of each kind already there.) */

  prowess_mode(player, card, event, 2);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  proliferate(player, card);
  return 0;
}

/* ManalinkEh 2022: 0x21cd850; original cards/war_of_the_spark.c:2184. */
int card_gideon_blackblade(int player, int card, event_t event)
{
  /* Gideon Blackblade	|1|W|W
   * Legendary Planeswalker - Gideon (4)
   * During your turn, ~ is a 4/4 Human Soldier creature with indestructible that's still a planeswalker.
   * Prevent all damage that would be dealt to ~ during your turn.
   * +1: Up to one other target creature you control gains your choice of vigilance, lifelink, or indestructible until end of turn.
   * -6: Exile target nonland permanent. */

  enum
  {
    CHOICE_ABILITY = 1,
    CHOICE_EXILE = 2
  };

  card_instance_t *instance;
  int result;
  uint32_t vigilance_priority;
  target_definition_t *creature_target;
  target_definition_t *permanent_target;
  target_definition_t td1;
  target_definition_t td2;

  check_legend_rule(player, card, event);
  if ((((event == EVENT_CAN_SKIP_TURN) || (event == EVENT_RESOLVE_SPELL)) &&
       (current_turn == player)) &&
      (instance = in_play(player, card), instance != NULL))
  {
    result = prevent_all_damage_to_target(player, card, player, card, 1);
    instance = get_card_instance(player, result);
    instance->token_status = instance->token_status | 0x10000;
    animate_self(player, card, 4, 4, KEYWORD_0, SP_KEYWORD_INDESTRUCTIBLE, COLOR_TEST_0, 0);
  }
  has_subtype_if_animated_self(player, card, event, SUBTYPE_HUMAN);
  has_subtype_if_animated_self(player, card, event, SUBTYPE_SOLDIER);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td1, 2);
    td1.preferred_controller = player;
    td1.allowed_controller = player;
    td1.allow_cancel = 1;
    default_target_definition(player, card, &td2, 0x47);
    td2.illegal_type = TYPE_LAND;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = can_target(&td1);
      if (result == 0)
      {
        result = count_counters(player, card, COUNTER_LOYALTY);
        if (result < 6)
        {
          return 0;
        }
        result = can_target(&td2);
        if (result == 0)
        {
          return 0;
        }
      }
      return planeswalker(player, card, event, 4);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = can_target(&td2);
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                           "Pump a creature you control", 1, 5, 1, "Exile target nonland permanent",
                           result, 10, 0xfffffffa, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_ABILITY)
      {
        result = can_target(&td1);
        if (result != 0)
        {
          new_pick_target(&td1, "TARGET_CREATURE_YOU_CONTROL", 0, 0);
        }
      }
      else if ((result == CHOICE_EXILE) && (result = can_target(&td2), result != 0))
      {
        new_pick_target(&td2, "TARGET_NONLAND_PERMANENT", 0, 1);
      }
      return planeswalker(player, card, event, 4);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 4);
    default_target_definition(player, card, &td1, 2);
    td1.preferred_controller = player;
    td1.allowed_controller = player;
    td1.allow_cancel = 1;
    default_target_definition(player, card, &td2, 0x47);
    td2.illegal_type = TYPE_LAND;
    instance = get_card_instance(player, card);
  }
  permanent_target = &td2;
  creature_target = &td1;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    if (instance->info_slot == CHOICE_ABILITY)
    {
      result = valid_target(creature_target);
      if (result != 0)
      {

        result = is_tapped(instance->targets[0].player, instance->targets[0].card);
        if (result == 0)
        {
          vigilance_priority = 0;
          if (current_phase < 0x15)
          {
            vigilance_priority = 5;
          }
        }
        else
        {
          vigilance_priority = 0;
        }
        result = dialog_impl(player, card, EVENT_RESOLVE_ACTIVATION, DLG_RANDOM, DLG_NO_STORAGE,
                             "Vigilance", 1, vigilance_priority, "Lifelink", 5, 1, "Indestructible",
                             5, 1, 0, 0, 0, 0);
        if (result == 1)
        {
          result = 8;
        }
        else if (result != 2)
        {
          result = 0x800;
        }
        pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                               0, 0, 0, result);
      }
    }
    else if ((instance->info_slot == CHOICE_EXILE) && (result = valid_target(permanent_target), result != 0))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x21cdde0; original cards/war_of_the_spark.c:2314. */
int card_gleaming_overseer(int player, int card, event_t event)
{
  /* Gleaming Overseer	|1|U|B
   * Creature - Zombie Wizard 1/4
   * When this creature enters, amass Zombies 1. (Put a +1/+1 counter on an Army you control. It's also a Zombie. If you don't control an Army, create a 0/0 black Zombie Army creature token first.)
   * Zombie tokens you control have hexproof and menace. */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    if ((event == EVENT_ABILITIES) && (affected_card_controller == player))
    {
      result = has_subtype(player, affected_card, SUBTYPE_ZOMBIE);
      if (result != 0)
      {
        result = is_token(affected_card_controller, affected_card);
        if (result != 0)
        {
          hexproof(affected_card_controller, affected_card, EVENT_ABILITIES);
          menace(affected_card_controller, affected_card, EVENT_ABILITIES);
        }
      }
    }
    return 0;
  }
  amass(player, card, 1);
  return 0;
}

/* ManalinkEh 2022: 0x21cded0; original cards/war_of_the_spark.c:2348. */
int card_god_eternal_bontu(int player, int card, event_t event)
{
  /* God-Eternal Bontu	|3|B|B
   * Legendary Creature - Zombie God 5/6
   * Menace
   * When ~ enters, sacrifice any number of other permanents, then draw that many cards.
   * When ~ dies or is put into exile from the battlefield, you may put her into her owner's library third from the top. */

  int result;
  card_instance_t *instance;
  int sacrificed_count;
  test_definition_t test;
  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 0x47, "");
    test.not_me = 1;
    instance = get_card_instance(player, card);
    sacrificed_count = 0;
    instance->state = instance->state | STATE_OUBLIETTED;
    while (true)
    {
      result = new_sacrifice(player, card, player, SAC_DONE, &test);
      if (result == 0)
        break;
      sacrificed_count = sacrificed_count + 1;
    }
    instance->state = instance->state & 0xff7fffff;
    draw_cards(player, sacrificed_count);
  }
  return war_god_eternal(player, card, event);
}

/* ManalinkEh 2022: 0x21cdfe0; original cards/war_of_the_spark.c:2421. */
int charge_mana_from_id_minus_two(int player, int card, event_t event, int csvid)
{
  if (csvid < 0)
    csvid = get_id(player, card);
  unsigned int iid;
  if (card == -1)
    iid = get_internal_card_id_from_csv_id(csvid);
  else
  {
    card_instance_t *instance = get_card_instance(player, card);
    iid = instance->internal_card_id;
    if ((unsigned int)cards_data[iid].id != csvid)
      backtrace_and_abort("cards/war_of_the_spark.c:2435\ncards_data[iid].id == csvid");
  }
  if (csvid == CARD_ID_REAPER_KING)
  {
    if (card >= 0)
      return casting_reaper_king(player, card, event);
    int temporary = add_card_to_hand(player, iid);
    int paid = casting_reaper_king(player, temporary, event);
    obliterate_card(player, temporary);
    return paid;
  }
  card_ptr_t *printed_card = cards_ptr[csvid];
  unsigned int printed_generic = (unsigned int)printed_card->req.req_colorless;
  if (printed_generic == 0x28)
    printed_generic = 0;
  unsigned int artifact = cards_data[iid].type & 0x40;
  int temporary = -1;
  int paid;
  if (!is_phyrexian(csvid))
  {
    int updated = card < 0 ? get_updated_casting_cost(player, -1, iid, event, printed_generic)
                           : get_updated_casting_cost(player, card, -1, event, printed_generic);
    unsigned int generic = (unsigned int)updated - 2U;
    if ((int32_t)generic < 0)
      generic = 0;
    bool normal_hybrid = is_normal_hybrid(printed_card) != 0;
    bool monocolor_hybrid = false;
    if (!normal_hybrid)
      monocolor_hybrid = is_monocolor_hybrid(printed_card) != 0;
    if (!normal_hybrid && !monocolor_hybrid)
    {
      if (csvid == CARD_ID_KARADOR_GHOST_CHIEFTAIN)
      {
        int creatures = count_graveyard_by_type(player, 2);
        generic -= creatures;
        if ((int32_t)generic < 0)
          generic = 0;
      }
      bool split_x = false;
      if (cards_data[iid].cc[1] == 0xff)
      {
        if (generic != 0)
          split_x = true;
        else
          generic = 0xffffffff;
      }
      unsigned int red = (unsigned int)printed_card->req.req_red;
      unsigned int blue = (unsigned int)printed_card->req.req_blue;
      unsigned int white = (unsigned int)printed_card->req.req_white;
      unsigned int green = (unsigned int)printed_card->req.req_green;
      unsigned int black = (unsigned int)printed_card->req.req_black;
      paid = charge_mana_multi_a(player, artifact ? 0 : generic, black, blue, green, red, white,
                                 artifact ? generic : 0);
      if (!paid)
        return 0;
      if (split_x)
        return charge_mana_multi_a(player, -(unsigned int)(artifact == 0), 0, 0, 0, 0, 0,
                                   -(unsigned int)(artifact != 0));
      return paid;
    }
    if (card < 0)
    {
      card = add_card_to_hand(player, iid);
      card_instance_t *instance = get_card_instance(player, card);
      if (normal_hybrid)
      {
        instance->state |= 0x20;
        --hand_count[player];
      }
      else
      {
        unsigned int state = instance->state;
        --hand_count[player];
        instance->state = state | 0x20;
      }
      temporary = card;
    }
    if (normal_hybrid)
    {
      int first_color = get_colors_for_hybrid(player, card, 0, 0);
      int second_color = get_colors_for_hybrid(player, card, first_color, 0);
      int symbols = get_number_of_hybrid_mana(player, card, 0, 0);
      paid = charge_mana_hybrid(player, card, symbols, first_color, second_color, generic);
    }
    else
    {
      int color = get_color_for_monocolored_hybrid(player, card);
      int symbols = get_number_of_monocolored_hybrid_mana(player, card, 0);
      paid = charge_mana_for_monocolor_hybrid(player, card, symbols, color, generic);
    }
  }
  else
  {
    if (card < 0)
    {
      card = add_card_to_hand(player, iid);
      card_instance_t *instance = get_card_instance(player, card);
      unsigned int state = instance->state;
      --hand_count[player];
      instance->state = state | 0x20;
      temporary = card;
    }
    unsigned int generic = printed_generic - 2U;
    if ((int32_t)generic < 0)
      generic = 0;
    paid = charge_phyrexian_mana(
        player, card, event, generic, (unsigned int)printed_card->req.req_black,
        (unsigned int)printed_card->req.req_blue, (unsigned int)printed_card->req.req_green,
        (unsigned int)printed_card->req.req_red, (unsigned int)printed_card->req.req_white, artifact);
  }
  if (temporary >= 0)
    obliterate_card(player, temporary);
  return paid;
}

/* ManalinkEh 2022: 0x21cebb0; original cards/war_of_the_spark.c:2568. */
int card_god_eternal_oketra(int player, int card, event_t event)
{
  /* God-Eternal Oketra	|3|W|W
   * Legendary Creature - Zombie God 3/6
   * Double strike
   * Whenever you cast a creature spell, create a 4/4 black Zombie Warrior creature token with vigilance.
   * When ~ dies or is put into exile from the battlefield, you may put it into its owner's library third from the top. */

  int result;
  token_generation_t token;

  check_legend_rule(player, card, event);
  sp_ability_trigger(player, event, 2, TYPE_CREATURE, NULL, player, card);
  if (event == EVENT_SPA)
  {
    default_token_definition(player, card, CARD_ID_ZOMBIE_WARRIOR, &token);
    token.tou = 4;
    token.pow = 4;
    token.s_key_plus = 8;
    generate_token(&token);
  }
  result = war_god_eternal(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x21cec60; original cards/war_of_the_spark.c:2592. */
int card_god_eternal_rhonas(int player, int card, event_t event)
{
  /* God-Eternal Rhonas	|3|G|G
   * Legendary Creature - Zombie God 5/5
   * Deathtouch
   * When ~ enters, double the power of each other creature you control until end of turn. Those creatures gain vigilance until end of turn.
   * When ~ dies or is put into exile from the battlefield, you may put it into its owner's library third from the top. */

  int c;
  card_instance_t *instance;
  int type_or_power;

  check_legend_rule(player, card, event);
  deathtouch(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

  if ((event == EVENT_ETB) && (0 < active_cards_count[player]))
  {
    c = 0;
    do
    {
      instance = in_play(player, c);
      if (((instance != NULL) && (type_or_power = is_what(player, c, 2), card != c)) &&
          (type_or_power != 0))
      {
        type_or_power = get_power(player, c);
        pump_ability_until_eot(player, card, player, c, type_or_power, 0, 0, 8);
      }
      c = c + 1;
    } while (c < active_cards_count[player]);
  }
  return war_god_eternal(player, card, event);
}

/* ManalinkEh 2022: 0x21ced60; original cards/war_of_the_spark.c:2637. */
int card_guild_globe(int player, int card, event_t event)
{
  /* Guild Globe	|2
   * Artifact
   * When this artifact enters, draw a card.
   * |2, |T, Sacrifice this artifact: Add two mana of different colors. */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    draw_a_card(player);
  }
  else if (event == EVENT_COUNT_MANA)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = mana_producer(player, card, EVENT_CAN_ACTIVATE), result != 0)) &&
        ((result = can_sacrifice_this_as_cost(player, card),
          result != 0 && (result = has_mana(player, 7, 2), result != 0))))
    {
      declare_mana_available_any_combination_of_colors(player, COLOR_TEST_ANY_COLORED, 2);
    }
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    result = mana_producer(player, card, EVENT_CAN_ACTIVATE);
    if (((result != 0) && (result = can_sacrifice_this_as_cost(player, card), result != 0)) &&
        (result = has_mana(player, 7, 2), result != 0))
    {
      return 1;
    }
  }
  else if ((event == EVENT_ACTIVATE) &&
           (result = charge_mana_for_activated_ability(player, card, 2, 0, 0, 0, 0, 0),
            result != 0))
  {
    produce_mana_tapped_all_different_colors(player, card, COLOR_TEST_ANY_COLORED, 2, NULL);
    kill_card(player, card, KILL_SACRIFICE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x21cef20; original cards/war_of_the_spark.c:2692. */
int card_heartwarming_redemption(int player, int card, event_t event)
{
  /* Heartwarming Redemption	|2|R|W
   * Instant
   * Discard all the cards in your hand, then draw that many cards plus one. You gain life equal to the number of cards in your hand. */

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
      result = hand_count[player] + 1;
      discard_all(player);
      draw_cards(player, result);
      gain_life(player, hand_count[player]);
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x21ceff0; original cards/war_of_the_spark.c:2726. */
int card_huatlis_raptor(int player, int card, event_t event)
{
  /* Huatli's Raptor	|G|W
   * Creature - Dinosaur 2/3
   * Vigilance
   * When this creature enters, proliferate. (Choose any number of permanents and/or players, then give each another counter of each kind already there.) */

  vigilance(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  proliferate(player, card);
  return 0;
}

/* ManalinkEh 2022: 0x21cf060; original cards/war_of_the_spark.c:2742. */
int card_huatli_the_suns_heart(int player, int card, event_t event)
{
  /* Huatli, the Sun's Heart	|2|GW
   * Legendary Planeswalker - Huatli (7)
   * Each creature you control assigns combat damage equal to its toughness rather than its power.
   * -3: You gain life equal to the greatest toughness among creatures you control. */

  int result;
  card_instance_t *instance;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  hybrid(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = count_counters(player, card, COUNTER_LOYALTY);
      if (result < 3)
      {
        return 0;
      }
      return planeswalker(player, card, event, 7);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL,
                           DLG_NO_STORAGE, DLG_PLANESWALKER, "Gain life", 1, 5, 0xfffffffd, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      return planeswalker(player, card, event, 7);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 7);
    get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    default_test_definition(&this_test, 2);
    result = check_battlefield_for_special_card(player, card, player, 0x100, &this_test);
    gain_life(player, result);
  }
  return planeswalker(player, card, event, 7);
}

/* ManalinkEh 2022: 0x21cf210; original cards/war_of_the_spark.c:2784. */
int card_ignite_the_beacon(int player, int card, event_t event)
{
  /* Ignite the Beacon	|4|W
   * Instant
   * Search your library for up to two planeswalker cards, reveal them, put them into your hand, then shuffle. */

  test_definition_t this_test;
  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    new_default_test_definition(&this_test, 0x100, "Select a planeswalker card.");
    this_test.qty = 2;
    new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x21cf450; original cards/war_of_the_spark.c:2951. */
int card_jace_wielder_of_mysteries(int player, int card, event_t event)
{
  /* Jace, Wielder of Mysteries	|1|U|U|U
   * Legendary Planeswalker - Jace (4)
   * If you would draw a card while your library has no cards in it, you win the game instead.
   * +1: Target player mills two cards. Draw a card.
   * -8: Draw seven cards. Then if your library has no cards in it, you win the game. */

  enum
  {
    CHOICE_MILL_DRAW = 1,
    CHOICE_DRAW_SEVEN = 2
  };

  card_instance_t *instance;
  int result;
  target_definition_t td;

  check_legend_rule(player, card, event);

  if (((((trigger_condition == TRIGGER_REPLACE_CARD_DRAW) && (card == affected_card)) &&
        (player == affected_card_controller)) &&
       ((reason_for_trigger_controller == player && (suppress_draw == 0)))) &&
      ((result = is_humiliated(player, card, AL_STATIC),
        result == 0 && (*deck_ptr[player] == -1))))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
      return planeswalker(player, card, event, 4);
    }
    if (event == EVENT_RESOLVE_TRIGGER)
    {
      lose_the_game(1 - player);
      suppress_draw = 1;
      return planeswalker(player, card, event, 4);
    }
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    td.allow_cancel = 1;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Mill 3 and draw",
                           1, 5, 1, "Draw 7", 1, 0x14, 0xfffffff8, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      new_pick_target(&td, "TARGET_PLAYER", 0, 1);
      return planeswalker(player, card, event, 4);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 4);
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    td.allow_cancel = 1;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    if (instance->info_slot == CHOICE_MILL_DRAW)
    {
      mill(instance->targets[0].player, 2);
      draw_a_card(player);
    }
    else if ((instance->info_slot == CHOICE_DRAW_SEVEN) && (draw_cards(player, 7), *deck_ptr[player] == -1))
    {
      lose_the_game(1 - player);
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x21cf710; original cards/war_of_the_spark.c:3036. */
int card_jaya_venerated_firemage(int player, int card, event_t event)
{
  /* Jaya, Venerated Firemage	|4|R
   * Legendary Planeswalker - Jaya (5)
   * If another red source you control would deal damage to a permanent or player, it deals that much damage plus 1 to that permanent or player instead.
   * -2: Jaya deals 2 damage to any target. */

  char source_color;
  card_instance_t *instance;
  int result;
  unsigned int red_color;
  target_definition_t td;

  instance = damage_being_dealt(event);
  if ((((instance != NULL) && (instance->damage_source_player == player)) &&
       (instance->damage_source_card != card)) &&
      ((source_color = instance->initial_color,
        red_color = get_sleighted_color_test(player, card, 0x10),
        ((int)source_color & red_color) != 0 &&
            (result = is_humiliated(player, card, AL_STATIC), result == 0))))
  {
    instance->info_slot = instance->info_slot + 1;
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 1;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = can_target(&td);
      if (result == 0)
      {
        return 0;
      }
      result = count_counters(player, card, COUNTER_LOYALTY);
      if (result < 2)
      {
        return 0;
      }
      return planeswalker(player, card, event, 5);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = can_target(&td);
      result =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL, DLG_NO_STORAGE,
                      DLG_PLANESWALKER, "Deal 2 damage", result, 5, 0xfffffffe, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      new_pick_target(&td, "TARGET_ANY", 0, 1);
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 1;
    get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    damage_target0(player, card, 2);
  }
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x21c7d20; original cards/war_of_the_spark.c:3090. */
static char *has_a_counter_and_not_mana_src(int who_chooses, int player, int card)
{
  int result;

  result = count_counters(player, card, ~COUNTER_P1_P1);
  if (result != 0)
  {
    result = get_id(player, card);
    result = get_internal_card_id_from_csv_id(result);
    if (((cards_data[result].extra_ability & 0x1000) == 0) &&
        (result = is_humiliated(player, card, AL_MANA), result == 0))
    {
      return NULL;
    }
  }
  return "Must have a counter.";
}

/* ManalinkEh 2022: 0x21c71e0; original cards/war_of_the_spark.c:3096. */
static int effect_yanggu_mana(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.allow_cancel = 2;
  td.illegal_state = TARGET_STATE_TAPPED | TARGET_STATE_SUMMONING_SICK;
  td.illegal_abilities = 0;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (intptr_t)has_a_counter_and_not_mana_src;
  if (event == EVENT_COUNT_MANA)
  {

    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
        (result = target_available(player, card, &td), 0 < result))
    {
      declare_mana_available_hex(player, 0x3e, result);
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        result = is_humiliated((int)instance->damage_target_player,
                                           instance->damage_target_card, AL_STATIC);
        if (result != 0)
        {
          return 0;
        }
        result = can_target(&td);
        return result;
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      {
        return 0;
      }
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_ACTIVATE)
    {
      result = pick_target(&td, "TARGET_CREATURE");
      if (result != 0)
      {
        instance->number_of_targets = '\x01';
        produce_mana_tapped_any_combination_of_colors_impl(instance->targets[0].player,
                                                           instance->targets[0].card,
                                                           COLOR_TEST_ANY_COLORED, 1, NULL, 0);
      }
      spell_fizzled = 1;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21cf9b0; original cards/war_of_the_spark.c:3134. */
int card_jiang_yanggu_wildcrafter(int player, int card, event_t event)
{
  /* Jiang Yanggu, Wildcrafter	|2|G
   * Legendary Planeswalker - Yanggu (3)
   * Each creature you control with a +1/+1 counter on it has "|T: Add one mana of any color."
   * -1: Put a +1/+1 counter on target creature. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {
    create_targetted_legacy_activate(player, card, effect_yanggu_mana, player, card);
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_target_definition(player, card, &td, 2);
      td.allow_cancel = 1;
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        result = can_target(&td);
        if (result == 0)
        {
          return 0;
        }
        return planeswalker(player, card, event, 3);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return planeswalker(player, card, event, 3);
      default_target_definition(player, card, &td, 2);
      td.allow_cancel = 1;
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = can_target(&td);
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_STORAGE,
                           DLG_PLANESWALKER, "Add a +1/+1 counter", result, 5, 0xffffffff, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      new_pick_target(&td, "TARGET_CREATURE", 0, 1);
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                               COUNTER_P1_P1, 1);
    }
  }
  return planeswalker(player, card, event, 3);
}

/* ManalinkEh 2022: 0x21cfbf0; original cards/war_of_the_spark.c:3187. */
int card_karns_bastion(int player, int card, event_t event)
{
  /* Karn's Bastion	""
   * Land
   * |T: Add |C.
   * |4, |T: Proliferate. (Choose any number of permanents and/or players, then give each another counter of each kind already there.) */

  int result;
  card_instance_t *instance;

  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 0;
      result = paying_mana();
      if (((result == 0) && (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1,
                                                                4, 0, 0, 0, 0, 0, 0, NULL, NULL),
                             result != 0)) &&
          (result =
               do_dialog(player, player, card, -1, -1, " Generate mana\n Proliferate\n Cancel", 1),
           result != 0))
      {
        if (result != 1)
        {
          return 0;
        }
        add_state(player, card, 0x10);
        result = charge_mana_for_activated_ability(player, card, 4, 0, 0, 0, 0, 0);
        if (result == 0)
        {
          remove_state(player, card, 0x10);
        }
        else
        {
          instance->info_slot = 1;
        }
        tapped_for_mana_color = -1;
        return 0;
      }
      event = EVENT_ACTIVATE;
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      instance = get_card_instance(player, card);
      if ((event == EVENT_RESOLVE_ACTIVATION) && (0 < instance->info_slot))
      {
        proliferate(player, card);
      }
      return 0;
    }
  }

  result = mana_producer(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x21c7380; original cards/war_of_the_spark.c:3345. */
static int kayas_ghostform_effect(int player, int card, event_t event)
{
  card_instance_t *instance;

  instance = get_card_instance(player, card);
  if (!resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    return 0;
  }
  seek_grave_for_id_to_reanimate(player, card, instance->targets[0].player,
                                 (unsigned int)cards_data[instance->targets[0].card].id,
                                 REANIMATE_DEFAULT);
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x21d0210; original cards/war_of_the_spark.c:3356. */
int card_kayas_ghostform(int player, int card, event_t event)
{
  /* Kaya's Ghostform	|B
   * Enchantment - Aura
   * Enchant creature or planeswalker you control
   * When enchanted permanent dies or is put into exile, return that card to the battlefield under your control. */

  card_instance_t *instance = get_card_instance(player, card);
  card_instance_t *aura = in_play(player, card);
  if (aura)
  {
    char attached_player = instance->damage_target_player;
    if (event == EVENT_GRAVEYARD_FROM_PLAY && attached_player != -1 &&
        instance->damage_target_card == affected_card &&
        attached_player == affected_card_controller &&
        !check_for_death_trigger_removal(attached_player, affected_card))
    {
      attached_player = instance->damage_target_player;
      int attached_card = instance->damage_target_card;
      card_instance_t *dying = in_play(attached_player, attached_card);
      if (dying && (dying = get_card_instance(attached_player, attached_card),
                    (unsigned char)(dying->kill_code - 1) < 4))
      {
        int legacy_card = create_legacy_effect(player, card, kayas_ghostform_effect);
        card_instance_t *legacy = get_card_instance(player, legacy_card);
        int owner = get_owner(instance->damage_target_player, instance->damage_target_card);
        legacy->targets[0].player = owner;
        int iid = get_original_internal_card_id(instance->damage_target_player,
                                                instance->damage_target_card);
        legacy->targets[11].player = 1;
        legacy->targets[0].card = iid;
      }
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

  target_definition_t td;
  default_target_definition(player, card, &td, 0x102);
  td.allowed_controller = player;
  return targeted_aura_custom_prompt(player, card, event, &td,
                                     "Select target creature or planeswalker you control.");
}

/* ManalinkEh 2022: 0x21d0410; original cards/war_of_the_spark.c:3392. */
int card_kaya_bane_of_the_dead(int player, int card, event_t event)
{
  /* Kaya, Bane of the Dead	|3|WB|WB|WB
   * Legendary Planeswalker - Kaya (7)
   * Your opponents and permanents your opponents control with hexproof can be the targets of spells and abilities you control as though they didn't have hexproof.
   * -3: Exile target creature. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  check_legend_rule(player, card, event);
  hybrid(player, card, event);
  if (event == EVENT_ABILITIES)
  {
    if (((1 - player == affected_card_controller) &&
         (result = is_what(1 - player, affected_card, 0x47), result != 0)) &&
        (result = is_humiliated(player, card, AL_STATIC), result == 0))
    {
      set_special_flags(affected_card_controller, affected_card, 0x20000);
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_target_definition(player, card, &td, 2);
      td.allow_cancel = 1;
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        result = can_target(&td);
        if (result == 0)
        {
          return 0;
        }
        result = count_counters(player, card, COUNTER_LOYALTY);
        if (result < 3)
        {
          return 0;
        }
        return planeswalker(player, card, event, 7);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return planeswalker(player, card, event, 7);
      default_target_definition(player, card, &td, 2);
      td.allow_cancel = 1;
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_ACTIVATE)
    {

      result = can_target(&td);
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL,
                           DLG_NO_DISPLAY_FOR_AI, DLG_NO_STORAGE, DLG_PLANESWALKER, "Exile target creature.", result, 5,
                           0xfffffffd, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      new_pick_target(&td, "TARGET_CREATURE", 0, 1);
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
    }
  }
  return planeswalker(player, card, event, 7);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int cruel_celebrant_effect(int player, int card, event_t event);
static int mayhem_devil_effect(int player, int card, event_t event);
static int liliana_dreadhorde_general_effect(int player, int card, event_t event);

/* ManalinkEh 2022: 0x21d06d0; original cards/war_of_the_spark.c:3452. */
int card_kiora_behemoth_beckoner(int player, int card, event_t event)
{
  /* Kiora, Behemoth Beckoner	|2|GU
   * Legendary Planeswalker - Kiora (7)
   * Whenever a creature you control with power 4 or greater enters, draw a card.
   * -1: Untap target permanent. */

  int result;
  card_instance_t *instance;

  check_legend_rule(player, card, event);
  hybrid(player, card, event);
  if ((((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 2);
    this_test.power = 3;
    this_test.power_flag = F5_POWER_GREATER_THAN_VALUE;
    etb_ability_global(player, event, 2, TYPE_NONE, &this_test, player, card);
  }
  if (event == EVENT_ETB)
  {
    result = is_what(trigger_cause_controller, trigger_cause, 2);
    if (result != 0)
    {
      draw_cards(player, 1);
    }
  }
  else
  {
    target_definition_t td;
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      default_target_definition(player, card, &td, 0x47);
      td.allow_cancel = 1;
      if (event == EVENT_CAN_ACTIVATE)
      {
        result = can_target(&td);
        if (result == 0)
        {
          return 0;
        }
        return planeswalker(player, card, event, 7);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return planeswalker(player, card, event, 7);
      instance = get_card_instance(player, card);
      default_target_definition(player, card, &td, 0x47);
    }
    td.allow_cancel = 1;
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = can_target(&td);
      result =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL, DLG_NO_STORAGE,
                      DLG_PLANESWALKER, "Untap a permanent", result, 5, 0xffffffff, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      new_pick_target(&td, "TARGET_PERMANENT", 0, 1);
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) &&
             (result = valid_target(&td), result != 0))
    {
      untap_card(instance->targets[0].player, instance->targets[0].card);
    }
  }
  return planeswalker(player, card, event, 7);
}

/* ManalinkEh 2022: 0x21d0a00; original cards/war_of_the_spark.c:3520. */
int card_krenko_tin_street_kingpin(int player, int card, event_t event)
{
  /* Krenko, Tin Street Kingpin	|2|R
   * Legendary Creature - Goblin 1/2
   * Whenever Krenko attacks, put a +1/+1 counter on it, then create a number of 1/1 red Goblin creature tokens equal to Krenko's power. */

  int howmany;

  check_legend_rule(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }
  add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  howmany = get_power(player, card);
  generate_tokens_by_id(player, card, CARD_ID_GOBLIN, howmany);
  return 0;
}

/* ManalinkEh 2022: 0x21d0ab0; original cards/war_of_the_spark.c:3545. */
int card_law_rune_enforcer(int player, int card, event_t event)
{
  /* Law-Rune Enforcer	|W
   * Creature - Human Soldier 1/2
   * |1, |T: Tap target creature with mana value 2 or greater. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 2);
  td.special = TARGET_SPECIAL_CMC_GREATER_OR_EQUAL;
  td.extra = 2;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = instance->targets[0].card;
    instance = get_card_instance(player, card);
    tap_card(instance->targets[0].player, result);
  }
  result = generic_activated_ability(player, card, event, 5, 1, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
  return result;
}

/* ManalinkEh 2022: 0x21c68a0; original cards/war_of_the_spark.c:3572. */
static int lazotep_plating_effect(int player, int card, event_t event)
{
  if (event == EVENT_STATIC_EFFECTS)
  {
    player_bits[player] =
        player_bits[player] | 0x20000;
  }
  if (!eot_trigger(player, card, event))
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x21d0bf0; original cards/war_of_the_spark.c:3581. */
int card_lazotep_plating(int player, int card, event_t event)
{
  /* Lazotep Plating	|1|U
   * Instant
   * Amass Zombies 1. (Put a +1/+1 counter on an Army you control. It's also a Zombie. If you don't control an Army, create a 0/0 black Zombie Army creature token first.)
   * You and permanents you control gain hexproof until end of turn. (You and they can't be the targets of spells or abilities your opponents control.) */

  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    amass(player, card, 1);
    for (int permanent = 0; permanent < active_cards_count[player]; ++permanent)
      if (in_play(player, permanent) && is_what(player, permanent, 0x47))
        pump_ability_until_eot(player, card, player, permanent, 0, 0, 0, SP_KEYWORD_HEXPROOF);
    create_legacy_effect(player, card, lazotep_plating_effect);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x21d0d50; original cards/war_of_the_spark.c:3606. */
int card_lazotep_reaver(int player, int card, event_t event)
{
  /* Lazotep Reaver	|1|B
   * Creature - Zombie Beast 1/2
   * When this creature enters, amass Zombies 1. (Put a +1/+1 counter on an Army you control. It's also a Zombie. If you don't control an Army, create a 0/0 black Zombie Army creature token first.) */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  amass(player, card, 1);
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21d0db0; original cards/war_of_the_spark.c:3620. */
int card_leyline_prowler(int player, int card, event_t event)
{
  /* Leyline Prowler	|1|B|G
   * Creature - Nightmare Beast 2/3
   * Deathtouch, lifelink
   * |T: Add one mana of any color. */

  int result;
  deathtouch(player, card, event);
  lifelink(player, card, event);
  result = mana_producing_creature_all_one_color(player, card, event, 0, COLOR_TEST_ANY_COLORED, 1);
  return result;
}

/* ManalinkEh 2022: 0x21c6d10; original cards/war_of_the_spark.c:3641. */
static int liliana_dreadhorde_general_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (event == EVENT_STATIC_EFFECTS && card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player >= 0 &&
        get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_LILIANA_DREADHORDE_GENERAL)
      kill_card(player, card, KILL_EXILE);
  }
  instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0 &&
      effect_follows_control_of_attachment(player, card, event))
    return 0;
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    instance = get_card_instance(player, card);
    if (in_play(affected_card_controller, affected_card))
    {
      card_instance_t *dying = get_card_instance(affected_card_controller, affected_card);
      if (dying->kill_code)
      {
        if (instance->damage_target_player >= 0 && instance->damage_target_card == affected_card &&
            instance->damage_target_player == affected_card_controller)
        {
          instance->damage_target_card = -1;
          instance->damage_target_player = -1;
          remove_status(player, card, 0x10000);
          if (instance->targets[11].player < 0)
            instance->targets[11].player = 0;
          *((uint8_t *)&instance->targets[11].player + 1) = 1;
        }
        count_for_gfp_ability(player, card, EVENT_GRAVEYARD_FROM_PLAY, player, 2, NULL);
      }
    }
  }
  unsigned int deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (deaths && !check_status(player, card, 0x10000))
  {
    draw_cards(player, deaths & 0xff);
    kill_card(player, card, KILL_EXILE);
  }
  if (event == EVENT_CLEANUP && !check_status(player, card, 0x10000))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x21d0e20; original cards/war_of_the_spark.c:3692. */
int card_liliana_dreadhorde_general(int player, int card, event_t event)
{
  /* Liliana, Dreadhorde General	|4|B|B
   * Legendary Planeswalker - Liliana (6)
   * Whenever a creature you control dies, draw a card.
   * +1: Create a 2/2 black Zombie creature token.
   * -4: Each player sacrifices two creatures of their choice.
   * -9: Each opponent chooses a permanent they control of each permanent type and sacrifices the rest. */

  enum
  {
    CHOICE_TOKEN = 1,
    CHOICE_SAC = 2,
    CHOICE_CATACLYSM = 3
  };

  card_instance_t *instance;
  int p;
  int i;
  int c;
  card_instance_t *permanent;
  int value;
  int result;
  card_instance_t *chosen;
  unsigned int deaths_or_state;
  int type_or_card_index;
  type_t types[5];
  char *prompts[5];
  target_definition_t td;
  test_definition_t this_test;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      i = 0;
      bool found = false;
      if (0 < active_cards_count[player])
      {
        do
        {
          permanent = in_play(player, i);

          if ((((permanent != NULL) &&
                (type_or_card_index = is_what(player, i, 0x80), type_or_card_index != 0)) &&
               (permanent = get_card_instance(player, i),
                permanent->info_slot_fn == liliana_dreadhorde_general_effect)) &&
              (((permanent->damage_target_player == player &&
                 (permanent->damage_target_card == card)) ||
                ((permanent->targets[0].player == player &&
                  (permanent->targets[0].card == card))))))
          {
            found = true;
            break;
          }
          i = i + 1;
        } while (i < active_cards_count[player]);
      }

      if (!found)
      {
        value = create_targetted_legacy_effect(player, card, liliana_dreadhorde_general_effect,
                                               player, card);
        instance = get_card_instance(player, value);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, value, 0x10000);
      }
    }
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
    count_for_gfp_ability(player, card, EVENT_GRAVEYARD_FROM_PLAY, player, 2, NULL);
  instance = get_card_instance(player, card);
  if (instance->kill_code == 0)
  {
    deaths_or_state = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (deaths_or_state)
      draw_cards(player, deaths_or_state & 0xff);
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return planeswalker(player, card, event, 6);
  instance = get_card_instance(player, card);
  if (event == EVENT_ACTIVATE)
  {

    instance->number_of_targets = '\0';
    instance->info_slot = 0;
    value = count_permanents_by_type(1 - player, TYPE_PERMANENT);
    if (dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Create a Zombie", 1, 5,
                    1, "Each player sacs", 1, 5, 0xfffffffc, "Opponent chooses & sacs", 1, value,
                    0xfffffff7, 0, 0, 0, 0) == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    return planeswalker(player, card, event, 6);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    value = instance->info_slot;
    if (value == CHOICE_TOKEN)
    {
      generate_token_by_id(player, card, CARD_ID_ZOMBIE);
    }
    else if (value == CHOICE_SAC)
    {
      new_default_test_definition(&test, 2, "Select a creature to sacrifice.");

      test.qty = 2;
      APNAP(p, {
        new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test);
      });
    }
    else if (value == CHOICE_CATACLYSM)
    {
      char marked[2][151];
      memset(marked, 0, sizeof(marked));
      type_or_card_index = 0;
      default_target_definition(player, card, &td, 0);
      p = 1 - player;

      td.allow_cancel = 0;
      td.illegal_abilities = 0;
      types[0] = TYPE_ARTIFACT;
      types[1] = TYPE_CREATURE;
      types[2] = TYPE_ENCHANTMENT;
      types[3] = TYPE_LAND;
      types[4] = TARGET_TYPE_PLANESWALKER;
      prompts[0] = "TARGET_ARTIFACT";
      prompts[1] = "TARGET_CREATURE";
      prompts[2] = "TARGET_ENCHANTMENT";
      prompts[3] = "TARGET_LAND";
      prompts[4] = "TARGET_PLANESWALKER";
      td.required_type = TYPE_ARTIFACT;
      td.who_chooses = p;
      td.allowed_controller = p;
      while (true)
      {
        result = can_target(&td);
        if ((result != 0) &&
            (result = new_pick_target(&td, prompts[type_or_card_index], 0, 0), result != 0))
        {
          result = instance->targets[0].card;
          marked[p][result] = 1;
          chosen = get_card_instance(p, result);
          deaths_or_state = chosen->state;
          chosen->state = deaths_or_state | 0x200000;
          if ((player == 0) || (ai_is_speculating == 1))
          {
            chosen->state = deaths_or_state | 0x300000;
          }
        }
        type_or_card_index = type_or_card_index + 1;
        if (type_or_card_index == 5)
          break;
        td.required_type = types[type_or_card_index];
      }
      td.required_type = TYPE_PERMANENT;
      td.required_subtype = 0x1009;
      type_or_card_index = can_target(&td);
      if ((type_or_card_index != 0) &&
          (type_or_card_index = new_pick_target(&td, "Select a tribal card", 0, 0x80),
           type_or_card_index != 0))
      {

        type_or_card_index = instance->targets[0].card;
        marked[p][type_or_card_index] = 1;
        instance = get_card_instance(p, type_or_card_index);
        deaths_or_state = instance->state;
        instance->state = deaths_or_state | 0x200000;
        if ((player == 0) || (ai_is_speculating == 1))
        {
          instance->state = deaths_or_state | 0x300000;
        }
      }
      if (0 < active_cards_count[p])
      {
        c = 0;
        do
        {
          if (marked[p][c] != '\0')
          {
            instance = get_card_instance(p, c);
            instance->state = instance->state & 0xffcfffff;
          }
          c = c + 1;
        } while (c < active_cards_count[p]);
      }
      if (can_sacrifice(player, p, 1, TYPE_PERMANENT, SUBTYPE_INVALID) != 0)
      {
        default_test_definition(&this_test, 0x47);
        this_test.value_for_special_selection_function = (int)marked;
        this_test.special_selection_function = not_marked;
        new_manipulate_all(player, card, p, &this_test, ACT_KILL_SACRIFICE);
      }
    }
  }
  return planeswalker(player, card, event, 6);
}

/* ManalinkEh 2022: 0x21d1560; original cards/war_of_the_spark.c:3874. */
int card_mana_geode(int player, int card, event_t event)
{
  /* Mana Geode	|3
   * Artifact
   * When this artifact enters, scry 1.
   * |T: Add one mana of any color. */

  int result;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    scry(player, 1);
  }
  result = artifact_mana_all_one_color(player, card, event, 1, 0);
  return result;
}

/* ManalinkEh 2022: 0x21c7420; original cards/war_of_the_spark.c:3895. */
static int massacre_girl_legacy(int player, int card, event_t event)
{
  unsigned int player_00;
  int card_00;
  int deaths_or_penalty;
  card_instance_t *instance;
  unsigned int player_01;
  unsigned int pass;
  int card_01;

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    count_for_gfp_ability(player, card, 0x77, 2, 2, NULL);
  }
  deaths_or_penalty = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (deaths_or_penalty != 0)
  {

    pass = 0;
    instance = get_card_instance(player, card);
    deaths_or_penalty = -deaths_or_penalty;
    player_00 = instance->targets[1].player;
    card_00 = instance->targets[1].card;
    do
    {
      card_01 = 0;
      player_01 = current_turn ^ pass;
      if (0 < active_cards_count[player_01])
      {
        do
        {
          instance = in_play(player_01, card_01);
          if ((((instance != NULL) && (is_this_dying(player_01, card_01) == 0)) &&
               (is_what(player_01, card_01, 2) != 0)) &&
              ((player_00 != player_01 || (card_00 != card_01))))
          {
            if ((player_00 == 0xffffffff) ||
                ((card_00 == -1 || (instance = in_play(player_00, card_00), instance == NULL))))
            {
              pump_until_eot_merge_previous(player, card, player_01, card_01, deaths_or_penalty,
                                            deaths_or_penalty);
            }
            else
            {
              pump_until_eot_merge_previous(player_00, card_00, player_01, card_01,
                                            deaths_or_penalty, deaths_or_penalty);
            }
          }
          card_01 = card_01 + 1;
        } while (card_01 < active_cards_count[player_01]);
      }
      pass = pass + 1;
    } while (pass != 2);
  }

  if (eot_trigger(player, card, event) != 0)
  {
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x21d15e0; original cards/war_of_the_spark.c:3920. */
int card_massacre_girl(int player, int card, event_t event)
{
  /* Massacre Girl	|3|B|B
   * Legendary Creature - Human Assassin 4/4
   * Menace
   * When ~ enters, each other creature gets -1/-1 until end of turn. Whenever a creature dies this turn, each creature other than ~ gets -1/-1 until end of turn. */

  int c;
  int legacy;
  card_instance_t *instance;
  unsigned int player_00;
  unsigned int pass;

  check_legend_rule(player, card, event);
  menace(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    legacy = create_legacy_effect(player, card, massacre_girl_legacy);
    if (legacy == -1)
    {
      backtrace_and_abort("cards/war_of_the_spark.c:3934\nlegacy != -1");
    }
    instance = get_card_instance(player, legacy);
    instance->targets[1].player = player;
    instance = get_card_instance(player, legacy);
    instance->targets[1].card = card;
    add_status(player, legacy, 0x10000);

    pass = 0;
    do
    {
      player_00 = pass ^ current_turn;
      if (0 < active_cards_count[player_00])
      {
        c = 0;
        do
        {
          instance = in_play(player_00, c);
          if (((instance != NULL) && (is_what(player_00, c, 2) != 0)) &&
              ((player != player_00 || (card != c))))
          {
            pump_until_eot_merge_previous(player, card, player_00, c, -1, -1);
          }
          c = c + 1;
        } while (c < active_cards_count[player_00]);
      }
      pass = pass + 1;
    } while (pass != 2);
    return 0;
  }

  return 0;
}

/* ManalinkEh 2022: 0x21c69c0; original cards/war_of_the_spark.c:3950. */
static int mayhem_devil_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (event == EVENT_STATIC_EFFECTS && card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player >= 0 &&
        get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_MAYHEM_DEVIL)
      kill_card(player, card, KILL_EXILE);
  }
  instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0 &&
      effect_follows_control_of_attachment(player, card, event))
    return 0;
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    instance = get_card_instance(player, card);
    if (in_play(affected_card_controller, affected_card))
    {
      card_instance_t *dying = get_card_instance(affected_card_controller, affected_card);
      if (dying->kill_code)
      {
        if (instance->damage_target_player >= 0 && instance->damage_target_card == affected_card &&
            instance->damage_target_player == affected_card_controller)
        {
          instance->damage_target_card = -1;
          instance->damage_target_player = -1;
          remove_status(player, card, 0x10000);
          if (instance->targets[11].player < 0)
            instance->targets[11].player = 0;
          *((uint8_t *)&instance->targets[11].player + 1) = 1;
          return 0;
        }
        if (dying->kill_code == 3)
          count_for_gfp_ability(player, card, EVENT_GRAVEYARD_FROM_PLAY, 2, 0x47, NULL);
      }
    }
  }
  unsigned int deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (deaths && !check_status(player, card, 0x10000))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    for (deaths &= 0xff; deaths; --deaths)
    {
      if (can_target(&td) && pick_target(&td, "TARGET_ANY"))
        damage_target0(player, card, 1);
      instance = get_card_instance(player, card);
      instance->number_of_targets = 0;
    }
    kill_card(player, card, KILL_EXILE);
  }
  if (event == EVENT_CLEANUP && !check_status(player, card, 0x10000))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x21d1770; original cards/war_of_the_spark.c:4014. */
int card_mayhem_devil(int player, int card, event_t event)
{
  /* Mayhem Devil	|1|B|R
   * Creature - Devil 3/3
   * Whenever a player sacrifices a permanent, this creature deals 1 damage to any target. */

  int c;
  card_instance_t *instance;
  unsigned int deaths;
  int legacy_or_result;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
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
               (instance = get_card_instance(player, c),
                instance->info_slot_fn == mayhem_devil_effect)) &&
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
        legacy_or_result =
            create_targetted_legacy_effect(player, card, mayhem_devil_effect, player, card);
        instance = get_card_instance(player, legacy_or_result);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, legacy_or_result, 0x10000);
      }
    }
  }
  else if ((event == EVENT_GRAVEYARD_FROM_PLAY) &&
           (instance = get_card_instance(affected_card_controller, affected_card),
            instance->kill_code == '\x03'))
  {
    count_for_gfp_ability(player, card, 0x77, 2, 0x47, NULL);
  }
  instance = get_card_instance(player, card);

  if ((instance->kill_code == '\0') &&
      (deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY), deaths != 0))
  {

    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    for (deaths = deaths & 0xff; deaths != 0; deaths = deaths - 1)
    {
      legacy_or_result = can_target(&td);
      if ((legacy_or_result != 0) &&
          (legacy_or_result = pick_target(&td, "TARGET_ANY"), legacy_or_result != 0))
      {
        damage_target0(player, card, 1);
      }
      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21d19b0; original cards/war_of_the_spark.c:4075. */
int card_merfolk_skydiver(int player, int card, event_t event)
{
  /* Merfolk Skydiver	|G|U
   * Creature - Merfolk Mutant 1/1
   * Flying
   * When this creature enters, put a +1/+1 counter on target creature you control.
   * |3|G|U: Proliferate. (Choose any number of permanents and/or players, then give each another counter of each kind already there.) */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if ((result != 0) && (result = pick_target(&td, "TARGET_CREATURE"), result != 0))
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                   COUNTER_P1_P1, 1);
    }
    instance->number_of_targets = '\0';
    return 0;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
    proliferate(player, card);
  return generic_activated_ability(player, card, event, 0, 3, 0, 1, 1, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x21d1b30; original cards/war_of_the_spark.c:4147. */
int card_nahiri_storm_of_stone(int player, int card, event_t event)
{
  /* Nahiri, Storm of Stone	|2|RW|RW
   * Legendary Planeswalker - Nahiri (6)
   * During your turn, creatures you control have first strike and equip abilities you activate cost |1 less to activate.
   * -X: Nahiri deals X damage to target tapped creature. */

  int value;
  unsigned int loyalty;
  int toughness_or_loyalty;
  card_instance_t *instance;
  target_definition_t *creature_target;
  target_definition_t td;

  check_legend_rule(player, card, event);
  hybrid(player, card, event);
  if (current_turn == player)
  {
    boost_subtype(player, card, event, ~SUBTYPE_INVALID, 0, 0, 0x100, 0,
                  BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  }
  if (event == EVENT_ETB)
  {
    draw_cards(player, 1);
    return planeswalker(player, card, event, 6);
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return planeswalker(player, card, event, 6);
  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 2);
  td.required_state = TARGET_STATE_TAPPED;
  if (player == 1)
  {
    td.required_state = TARGET_STATE_TAPPED;
    loyalty = count_counters(1, card, COUNTER_LOYALTY);
    td.toughness_requirement = loyalty | 0x2000;
  }
  creature_target = &td;
  td.allow_cancel = 1;
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (!can_target(creature_target))
      return 0;
    return planeswalker(player, card, event, 6);
  }
  if (event != EVENT_ACTIVATE)
  {
    if ((event == EVENT_RESOLVE_ACTIVATION) && (value = valid_target(creature_target), value != 0))
    {
      damage_target0(player, card, instance->targets[1].player);
    }
    return planeswalker(player, card, event, 6);
  }

  instance->number_of_targets = '\0';
  instance->info_slot = 0;
  value = can_target(creature_target);
  if (dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL, DLG_NO_STORAGE,
                  DLG_PLANESWALKER, "Damage a creature", value, 5, 0, 0, 0, 0, 0) == 0)
  {
    spell_fizzled = 1;
    return 0;
  }

  if (pick_target(creature_target, "ROYAL_ASSASSIN") == 0)
  {
    if (spell_fizzled == 1)
      return planeswalker(player, card, event, 6);
  }
  else
  {

    value = count_counters(player, card, COUNTER_LOYALTY);
    toughness_or_loyalty = get_toughness(instance->targets[0].player, instance->targets[0].card);
    if (toughness_or_loyalty < value)
    {
      value = toughness_or_loyalty;
    }
    if (player == 0)
    {
      value = choose_a_number(0, "Pay how much Loyalty?", value);
    }
    if (spell_fizzled == 1)
      return planeswalker(player, card, event, 6);
    if ((0 < value) && (toughness_or_loyalty = count_counters(player, card, COUNTER_LOYALTY),
                        value <= toughness_or_loyalty))
    {
      remove_counters(player, card, player, card, COUNTER_LOYALTY, value);
      instance->targets[1].player = value;
      return planeswalker(player, card, event, 6);
    }
  }
  spell_fizzled = 1;
  return planeswalker(player, card, event, 6);
}

/* ManalinkEh 2022: 0x21d1ee0; original cards/war_of_the_spark.c:4222. */
int card_narsets_reversal(int player, int card, event_t event)
{
  /* Narset's Reversal	|U|U
   * Instant
   * Copy target instant or sorcery spell, then return it to its owner's hand. You may choose new targets for the copy. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_RESOLVE_SPELL)
  {
    result = counterspell_validate(player, card, NULL, 0);
    if (result != 0)
    {

      instance = get_card_instance(player, card);
      result =
          pre_counterspell(player, card, instance->targets[0].player, instance->targets[0].card);
      if (result == 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
      }
      else
      {
        copy_spell_from_stack(player, instance->targets[0].player, instance->targets[0].card);
        bounce_permanent(instance->targets[0].player, instance->targets[0].card);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 2, NULL, NULL, 1, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21d2250; original cards/war_of_the_spark.c:4294. */
int card_neheb_dreadhorde_champion(int player, int card, event_t event)
{
  /* Neheb, Dreadhorde Champion	|2|R|R
   * Legendary Creature - Zombie Minotaur Warrior 5/4
   * Trample
   * Whenever Neheb deals combat damage to a player or planeswalker, you may discard any number of cards. If you do, draw that many cards and add that much |R. Until end of turn, you don't lose this mana as steps and phases end. */

  int result;
  int selected;
  int amount;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  result = damage_dealt_by_me(player, card, event,
                              DDBM_MAY_DAMAGE_PLANESWALKER | DDBM_MUST_DAMAGE_PLAYER);
  if (result == 0)
  {
    return 0;
  }

  new_default_test_definition(&this_test, 0xff, "Select a card to discard.");
  result = hand_count[player];
  amount = 0;
  this_test.zone = 0x100;
  if (0 < result)
  {
    do
    {
      selected = new_select_a_card(player, player, 4, 0, 3, -1, &this_test);
      if (selected == -1)
        break;

      amount = amount + 1;
      discard_card(player, selected);
    } while (result != amount);
  }
  draw_cards(player, amount);
  produce_mana(player, 4, amount);
  mana_doesnt_drain_until_eot(player, COLOR_RED, amount);
  return 0;
}

/* ManalinkEh 2022: 0x21d2370; original cards/war_of_the_spark.c:4328. */
int card_neoform(int player, int card, event_t event)
{
  /* Neoform	|G|U
   * Sorcery
   * As an additional cost to cast this spell, sacrifice a creature.
   * Search your library for a creature card with mana value equal to 1 plus the sacrificed creature's mana value, put that card onto the battlefield with an additional +1/+1 counter on it, then shuffle. */

  unsigned int sacrificed;
  card_instance_t *instance;
  int result;
  unsigned int player_00;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, EVENT_CAN_CAST, 0x400, NULL, NULL, 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (result = check_special_flags2(player, card, -0x80000000), result == 0))
    {

      new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
      sacrificed = new_sacrifice(player, card, player,
                                 SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
      if (sacrificed == 0)
      {
        spell_fizzled = 1;
      }
      else
      {
        player_00 = sacrificed >> 0x10 & 0xff;
        result = get_cmc(player_00, sacrificed >> 0x18);
        instance = get_card_instance(player, card);
        instance->info_slot = (result + 1);
        kill_card(player_00, sacrificed >> 0x18, KILL_SACRIFICE);
      }
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {

    default_test_definition(&test, 2);
    instance = get_card_instance(player, card);
    test.cmc = instance->info_slot;
    instance = get_card_instance(player, card);
    scnprintf(test.message, 100, "Select a creature card with CMC %d.", instance->info_slot);
    result = new_global_tutor(player, player, 1, 4, 0, 2, &test);
    if (-1 < result)
    {
      hack_silent_counters = hack_silent_counters + 1;
      add_counters(player, card, player, result, COUNTER_P1_P1, 1);
      hack_silent_counters = hack_silent_counters + -1;
    }
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x21d3290; original cards/war_of_the_spark.c:4576. */
int card_nissa_who_shakes_the_world(int player, int card, event_t event)
{
  /* Nissa, Who Shakes the World	|3|G|G
   * Legendary Planeswalker - Nissa (5)
   * Whenever you tap a Forest for mana, add an additional |G.
   * +1: Put three +1/+1 counters on up to one target noncreature land you control. Untap it. It becomes a 0/0 Elemental creature with vigilance and haste that's still a land.
   * -8: You get an emblem with "Lands you control have indestructible." Search your library for any number of Forest cards, put them onto the battlefield tapped, then shuffle. */

  enum
  {
    CHOICE_AWAKEN = 1,
    CHOICE_FORESTS = 2
  };

  card_instance_t *instance;
  int result;
  const char *msg;
  subtype_t subtype;
  target_definition_t *land_target;
  target_definition_t td_land;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  if (((((uint32_t)(event - EVENT_COUNT_MANA) & 0xfffffffd) == 0) &&
       (result = is_what(affected_card_controller, affected_card, 1), result != 0)) &&
      (affected_card_controller == player))
  {
    subtype = get_hacked_subtype(player, card, 0x5001);
    result = has_subtype(affected_card_controller, affected_card, subtype);
    if ((result != 0) && (result = is_humiliated(player, card, AL_TRIG), result == 0))
    {
      instance = in_play(player, card);
      if (instance == NULL)
      {
        return 0;
      }
      if (event == EVENT_COUNT_MANA)
      {
        result = is_tapped(affected_card_controller, affected_card);
        if (result != 0)
        {
          return 0;
        }
        result = is_animated_and_sick(affected_card_controller, affected_card);
        if (result != 0)
        {
          return 0;
        }
        result = can_produce_mana(affected_card_controller, affected_card);
        if (result == 0)
        {
          return 0;
        }
        declare_mana_available(affected_card_controller, 3, 1);
        return planeswalker(player, card, event, 5);
      }
      if (-1 < tapped_for_mana_color)
      {
        produce_mana(affected_card_controller, 3, 1);
      }
    }
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td_land, 1);
    td_land.illegal_type = TYPE_CREATURE;
    td_land.preferred_controller = player;
    td_land.allowed_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Awaken", 1, 5, 1,
                           "Indestructible forests", 1, 10, 0xfffffff8, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if ((result == CHOICE_AWAKEN) && (result = can_target(&td_land), result != 0))
      {
        new_pick_target(&td_land, "Select target noncreature land.", 0, 0x80);
      }
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    default_target_definition(player, card, &td_land, 1);
    td_land.illegal_type = TYPE_CREATURE;
    td_land.preferred_controller = player;
    td_land.allowed_controller = player;
    instance = get_card_instance(player, card);
  }
  land_target = &td_land;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    if (instance->info_slot == 1)
    {
      result = validate_target(player, card, land_target, 0);
      if (result != 0)
      {

        add_counters(instance->parent_controller, instance->parent_card,
                     instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 3);
        untap_card(instance->targets[0].player, instance->targets[0].card);
        result = animate_other(instance->parent_controller, instance->parent_card,
                               instance->targets[0].player, instance->targets[0].card, 0, 0,
                               KEYWORD_0, SP_KEYWORD_VIGILANCE | SP_KEYWORD_HASTE, COLOR_TEST_0, 1);
        instance = get_card_instance(player, result);
        instance->targets[9].card = 0x3a;
      }
    }
    else if (instance->info_slot == 2)
    {
      generate_token_by_id(player, card, CARD_ID_NISSA_WHO_SHAKES_THE_WORLD_EMBLEM);
      msg = get_hacked_land_text(player, card, "Select %a card.", 0x5001);
      new_default_test_definition(&this_test, 1, msg);
      this_test.subtype = get_hacked_subtype(player, card, 0x5001);
      result = count_deck_by_type(player, 1);
      this_test.qty = (unsigned short)result;
      new_global_tutor(player, player, 1, 5, 0, 1, &this_test);
    }
  }
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x21d37d0; original cards/war_of_the_spark.c:4667. */
int card_nissa_who_shakes_the_world_emblem(int player, int card, event_t event)
{
  /* Nissa, Who Shakes the World Emblem	""
   * Emblem - Nissa
   * Lands you control have indestructible. */

  int result;
  if ((event == EVENT_ABILITIES) && (affected_card_controller == player))
  {
    result = is_what(affected_card_controller, affected_card, 1);
    if (result != 0)
    {
      indestructible(affected_card_controller, affected_card, EVENT_ABILITIES);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21c7620; original cards/war_of_the_spark.c:4680. */
static int colors_to_guilds(int iid)
{
  unsigned int color_count;
  int result;
  unsigned int v;

  v = (unsigned int)cards_data[iid].color;
  color_count = num_bits_set(v);
  if (color_count == 2)
  {
    if (v == 0x24)
    {
      return 1;
    }
    if (v == 6)
    {
      result = 2;
    }
    else if (v == 0x12)
    {
      result = 4;
    }
    else if (v == 0x18)
    {
      result = 8;
    }
    else if (v == 0x28)
    {
      result = 0x10;
    }
    else if (v == 0x22)
    {
      result = 0x20;
    }
    else if (v == 10)
    {
      result = 0x40;
    }
    else if (v == 0xc)
    {
      result = 0x80;
    }
    else
    {
      if (v != 0x14)
      {
        return (unsigned int)(v == 0x30) << 9;
      }
      result = 0x100;
    }
  }
  else
  {
    result = 0;
  }
  return result;
}

/* ManalinkEh 2022: 0x21c7f20; original cards/war_of_the_spark.c:4706. */
static int not_selected(int iid, int chosen, int unused1, int unused2)
{
  int result;
  unsigned int guilds;

  result = colors_to_guilds(iid);
  if (result < 1)
  {
    return 0;
  }
  guilds = colors_to_guilds(iid);
  return (unsigned int)((chosen & guilds) == 0);
}

/* ManalinkEh 2022: 0x21d3ae0; original cards/war_of_the_spark.c:4769. */
int card_oath_of_kaya(int player, int card, event_t event)
{
  /* Oath of Kaya	|1|W|B
   * Legendary Enchantment
   * When ~ enters, it deals 3 damage to any target and you gain 3 life.
   * Whenever an opponent attacks a planeswalker you control with one or more creatures, ~ deals 2 damage to that player and you gain 2 life. */

  int opponent;
  target_definition_t td;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  opponent = 1 - player;
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    if (can_target(&td) != 0)
    {
      if (pick_target(&td, "TARGET_ANY") != 0)
      {
        damage_target0(player, card, 3);
      }
    }
    gain_life(player, 3);
    store_attackers(player, card, EVENT_ETB, DAT_ATTACKS_PLANESWALKER, opponent, -1, NULL);
    dat_ability(player, card, EVENT_ETB, DAT_ATTACKS_PLANESWALKER | DAT_RESOLVE_TRIGGER_MANDATORY,
                0);
  }
  else
  {
    store_attackers(player, card, event, DAT_ATTACKS_PLANESWALKER, opponent, -1, NULL);
    dat_ability(player, card, event, DAT_ATTACKS_PLANESWALKER | DAT_RESOLVE_TRIGGER_MANDATORY, 0);
    if (event == EVENT_DAT_ABILITY)
    {
      damage_player(opponent, 2, player, card);
      gain_life(player, 2);
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x21d3cb0; original cards/war_of_the_spark.c:4810. */
int card_ob_nixilis_the_hate_twisted(int player, int card, event_t event)
{
  /* Ob Nixilis, the Hate-Twisted	|3|B|B
   * Legendary Planeswalker - Nixilis (5)
   * Whenever an opponent draws a card, Ob Nixilis deals 1 damage to that player.
   * -2: Destroy target creature. Its controller draws two cards. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  check_legend_rule(player, card, event);
  result = card_drawn_trigger(player, card, event, 1 - player, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    damage_player(1 - player, 1, player, card);
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 1;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = can_target(&td);
      if (result == 0)
      {
        return 0;
      }
      result = count_counters(player, card, COUNTER_LOYALTY);
      if (result < 2)
      {
        return 0;
      }
      return planeswalker(player, card, event, 5);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = can_target(&td);
      result =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL, DLG_NO_STORAGE,
                      DLG_PLANESWALKER, "Destroy a creature", result, 5, 0xfffffffe, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      new_pick_target(&td, "TARGET_CREATURE", 0, 1);
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 1;
    instance = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    draw_cards(instance->targets[0].player, 2);
  }
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x21d3f40; original cards/war_of_the_spark.c:4861. */
int card_paradise_druid(int player, int card, event_t event)
{
  /* Paradise Druid	|1|G
   * Creature - Elf Druid 2/1
   * This creature has hexproof as long as it's untapped. (It can't be the target of spells or abilities your opponents control.)
   * |T: Add one mana of any color. */

  int result;

  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_tapped(player, card);
    if (result == 0)
    {
      hexproof(player, card, EVENT_ABILITIES);
    }
  }
  result = mana_producing_creature_all_one_color(player, card, event, 5, COLOR_TEST_ANY_COLORED, 1);
  return result;
}

/* ManalinkEh 2022: 0x21d3fc0; original cards/war_of_the_spark.c:4876. */
int card_parhelion_ii(int player, int card, event_t event)
{
  /* Parhelion II	|6|W|W
   * Legendary Artifact - Vehicle 5/5
   * Flying, first strike, vigilance
   * Whenever ~ attacks, create two 4/4 white Angel creature tokens with flying and vigilance that are attacking.
   * Crew 4 (Tap any number of creatures you control with total power 4 or more: This Vehicle becomes an artifact creature until end of turn.) */

  int result;
  token_generation_t token;

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    default_token_definition(player, card, CARD_ID_ANGEL, &token);
    token.qty = '\x02';
    token.s_key_plus = 8;
    token.action = TOKEN_ACTION_ATTACKING_UNTAPPED;
    generate_token(&token);
  }
  result = vehicle(player, card, event, 4);
  return result;
}

/* ManalinkEh 2022: 0x21d4760; original cards/war_of_the_spark.c:5112. */
int card_prismite(int player, int card, event_t event)
{
  /* Prismite	|2
   * Artifact Creature - Golem 2/1
   * |2: Add one mana of any color. */

  int result;

  if (event == EVENT_COUNT_MANA)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = can_produce_mana(player, card), result != 0)) &&
        (result = has_mana_for_activated_ability(player, card, 2, 0, 0, 0, 0, 0), result != 0))
    {
      result = has_mana_for_activated_ability(player, card, 4, 0, 0, 0, 0, 0);
      if (result == 0)
      {
        declare_mana_available_hex(player, 0x3e, 1);
        declare_mana_available(player, 0, -1);
      }
      else
      {
        result = has_mana_for_activated_ability(player, card, 6, 0, 0, 0, 0, 0);
        if (result == 0)
        {
          declare_mana_available_hex(player, 0x3e, 2);
          declare_mana_available(player, 0, -2);
        }
        else
        {
          declare_mana_available_hex(player, 0x3e, 3);
          declare_mana_available(player, 0, -3);
        }
      }
    }
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    result = has_mana(player, 0, 2);
    if (result != 0)
    {
      result = can_produce_mana(player, card);
      return result;
    }
  }
  else if ((event == EVENT_ACTIVATE) &&
           (result = charge_mana_for_activated_ability(player, card, 2, 0, 0, 0, 0, 0),
            result != 0))
  {
    do
    {
      spell_fizzled = 0;
      produce_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1);
    } while (spell_fizzled == 1);
    tapped_for_mana_color = -2;
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21d49e0; original cards/war_of_the_spark.c:5156. */
int card_prison_realm(int player, int card, event_t event)
{
  /* Prison Realm	|2|W
   * Enchantment
   * When this enchantment enters, exile target creature or planeswalker an opponent controls until this enchantment leaves the battlefield.
   * When this enchantment enters, scry 1. */

  if (event == EVENT_CAN_CAST)
    return basic_spell(player, card, EVENT_CAN_CAST);
  return_from_oblivion(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, 0x102);
  td.allowed_controller = 1 - player;
  td.allow_cancel = 0;
  if (can_target(&td) && new_pick_target(&td, "TARGET_CREATURE_OR_PLANESWALKER", 0, 0))
  {
    card_instance_t *instance = get_card_instance(player, card);
    obliviation(player, card, instance->targets[0].player, instance->targets[0].card);
  }
  else if (player == 1)
    ai_modifier -= 0x30;
  scry(player, 1);
  return 0;
}

/* ManalinkEh 2022: 0x21d4da0; original cards/war_of_the_spark.c:5302. */
int card_return_to_nature(int player, int card, event_t event)
{
  /* Return to Nature	|1|G
   * Instant
   * Choose one -
   * * Destroy target artifact.
   * * Destroy target enchantment.
   * * Exile target card from a graveyard. */

  enum
  {
    CHOICE_ARTIFACT = 1,
    CHOICE_ENCHANTMENT = 2,
    CHOICE_GRAVE = 3
  };

  card_instance_t *instance;
  int mode_or_available;
  int can_enchantment;
  int can_exile;
  target_definition_t *td_00;
  const char *prompt;
  target_definition_t td;
  target_definition_t td2;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x40);
    default_target_definition(player, card, &td2, 4);
    new_default_test_definition(&test, 0xff, "Select target card to exile.");
    get_card_instance(player, card);
    if (generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL) != 0)
    {
      return 1;
    }
    if (generic_spell(player, card, EVENT_CAN_CAST, 1, &td2, NULL, 1, NULL) == 0)
    {
      return generic_spell(player, card, EVENT_CAN_CAST, 0x1000, NULL, NULL, 1, &test);
    }
    return 1;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      td_00 = &td2;
      default_target_definition(player, card, &td, 0x40);
      default_target_definition(player, card, td_00, 4);
      new_default_test_definition(&test, 0xff, "Select target card to exile.");
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        if (check_special_flags2(player, card, -0x80000000) == 0)
        {

          mode_or_available = can_target(&td);
          can_enchantment = can_target(td_00);
          can_exile = generic_spell(player, card, EVENT_CAN_CAST, 0x1000, NULL, NULL, 1, &test);
          if (dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, "Destroy target artifact",
                          mode_or_available, 10, "Destroy target enchantment", can_enchantment, 10,
                          "Exile target card from a graveyard", can_exile, 3, 0, 0, 0, 0) == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
        }
        mode_or_available = instance->info_slot;
        if (mode_or_available == CHOICE_ARTIFACT)
        {
          td_00 = &td;
          prompt = "TARGET_ARTIFACT";
        }
        else
        {
          if (mode_or_available != CHOICE_ENCHANTMENT)
          {
            if (mode_or_available != CHOICE_GRAVE)
            {
              return 0;
            }
            return generic_spell(player, card, EVENT_CAST_SPELL, 0x1000, NULL, NULL, 1, &test);
          }
          prompt = "TARGET_ENCHANTMENT";
        }
        return generic_spell(player, card, EVENT_CAST_SPELL, 1, td_00, prompt, 1, NULL);
      }
    }
    return 0;
  }
  if ((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET))
  {
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0x40);
    default_target_definition(player, card, &td2, 4);
    new_default_test_definition(&test, 0xff, "Select target card to exile.");
    get_card_instance(player, card);
    return 0;
  }
  default_target_definition(player, card, &td, 0x40);
  default_target_definition(player, card, &td2, 4);
  new_default_test_definition(&test, 0xff, "Select target card to exile.");
  instance = get_card_instance(player, card);
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }
  int mode = instance->info_slot;
  bool destroy = false;
  if (mode == CHOICE_ARTIFACT)
  {
    destroy = valid_target(&td) != 0;
    if (!destroy)
      mode = instance->info_slot;
  }
  if (!destroy && mode == CHOICE_ENCHANTMENT)
    destroy = valid_target(&td2) != 0;
  if (destroy)
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
  mode = instance->info_slot;
  if (mode == CHOICE_GRAVE)
  {
    int index = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
    if (index != -1)
      rfg_card_from_grave(instance->targets[0].player, index);
  }
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21d5390; original cards/war_of_the_spark.c:5389. */
int card_roalesk_apex_hybrid(int player, int card, event_t event)
{
  /* Roalesk, Apex Hybrid	|2|G|G|U
   * Legendary Creature - Human Mutant 4/5
   * Flying, trample
   * When Roalesk enters, put two +1/+1 counters on another target creature you control.
   * When Roalesk dies, proliferate, then proliferate again. (Choose any number of permanents and/or players, then give each another counter of each kind already there. Then do it again.) */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.special = TARGET_SPECIAL_NOT_ME;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_CREATURE");
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_P1_P1, 2);
      }
    }
    instance->number_of_targets = '\0';
  }
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  proliferate(player, card);
  proliferate(player, card);
  return 0;
}

/* ManalinkEh 2022: 0x21d54d0; original cards/war_of_the_spark.c:5447. */
int card_saheeli_sublime_artificer(int player, int card, event_t event)
{
  /* Saheeli, Sublime Artificer	|1|UR|UR
   * Legendary Planeswalker - Saheeli (5)
   * Whenever you cast a noncreature spell, create a 1/1 colorless Servo artifact creature token.
   * -2: Target artifact you control becomes a copy of another target artifact or creature you control until end of turn, except it's an artifact in addition to its other types. */

  card_instance_t *instance;
  int result;
  unsigned int can_copy;
  target_definition_t *copy_target;
  target_definition_t *artifact_target;
  target_definition_t td;
  target_definition_t td2;

  check_legend_rule(player, card, event);
  hybrid(player, card, event);
  prowess_mode(player, card, event, 2);
  if (event == EVENT_SPA)
  {
    generate_token_by_id(player, card, CARD_ID_SERVO);
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_target_definition(player, card, &td, 0x40);
      td.allowed_controller = player;
      td.allow_cancel = 1;
      default_target_definition(player, card, &td2, 0x42);
      td2.allowed_controller = player;
      td2.allow_cancel = 1;
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        result = can_target(&td);
        if (((result == 0) || (result = can_target(&td2), result == 0)) ||
            (result = count_counters(player, card, COUNTER_LOYALTY), result < 2))
        {
          return 0;
        }
        return planeswalker(player, card, event, 5);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return planeswalker(player, card, event, 5);
      default_target_definition(player, card, &td, 0x40);
      td.allowed_controller = player;
      td.allow_cancel = 1;
      default_target_definition(player, card, &td2, 0x42);
      td2.allowed_controller = player;
      td2.allow_cancel = 1;
      instance = get_card_instance(player, card);
    }
    artifact_target = &td;
    copy_target = &td2;
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = can_target(artifact_target);
      can_copy = 0;
      if (result != 0)
      {
        result = can_target(copy_target);
        can_copy = (unsigned int)(result != 0);
      }
      result =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL, DLG_NO_STORAGE,
                      DLG_PLANESWALKER, "Copy artifact or creature", can_copy, 5, 0xfffffffe, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      result = new_pick_target(artifact_target, "TARGET_ARTIFACT", 0, 1);
      if (result != 0)
      {
        new_pick_target(copy_target, "PUTREFY", 1, 1);
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      shapeshift_target(player, card, instance->targets[0].player, instance->targets[0].card,
                        instance->targets[1].player, instance->targets[1].card, 5);
    }
  }
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x21d5850; original cards/war_of_the_spark.c:5513. */
int card_samut_tyrant_smasher(int player, int card, event_t event)
{
  /* Samut, Tyrant Smasher	|2|RG|RG
   * Legendary Planeswalker - Samut (5)
   * Creatures you control have haste.
   * -1: Target creature gets +2/+1 and gains haste until end of turn. Scry 1. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  check_legend_rule(player, card, event);
  hybrid(player, card, event);
  boost_subtype(player, card, event, ~SUBTYPE_INVALID, 0, 0, 0, 4,
                BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allow_cancel = 1;
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = can_target(&td);
      if (result == 0)
      {
        return 0;
      }
      return planeswalker(player, card, event, 5);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = can_target(&td);
      result =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL, DLG_NO_STORAGE,
                      DLG_PLANESWALKER, "Boost a creature", result, 5, 0xffffffff, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      new_pick_target(&td, "TARGET_CREATURE", 0, 1);
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
  }
  td.preferred_controller = player;
  td.allow_cancel = 1;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2,
                           1, 0, 4);
    scry(player, 1);
  }
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x21d5ac0; original cards/war_of_the_spark.c:5564. */
int card_sarkhan_the_masterless(int player, int card, event_t event)
{
  /* Sarkhan the Masterless	|3|R|R
   * Legendary Planeswalker - Sarkhan (5)
   * Whenever a creature attacks you or a planeswalker you control, each Dragon you control deals 1 damage to that creature.
   * +1: Until end of turn, each planeswalker you control becomes a 4/4 red Dragon creature and gains flying.
   * -3: Create a 4/4 red Dragon creature token with flying. */

  enum
  {
    CHOICE_MUTATE = 1,
    CHOICE_TOKEN = 2
  };

  card_instance_t *instance;
  int index_or_value;
  int c;
  token_generation_t token;

  check_legend_rule(player, card, event);
  c = 1 - player;
  if (event == EVENT_CHECK_PUMP && current_turn == c && current_phase < 0x16)
  {
    index_or_value = count_subtype(player, 2, 0x30);
    pumpable_toughness[current_turn] -= index_or_value;
  }
  store_attackers(player, card, event, DAT_TRACK, c, -1, NULL);
  dat_ability(player, card, event, DAT_TRACK | DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
  {

    instance = get_card_instance(player, card);
    const uint8_t *attackers = (const uint8_t *)&instance->targets + 2 * sizeof(target_t);
    for (int attacker_index = instance->info_slot - 1; attacker_index >= 0; --attacker_index)
      if (in_play(current_turn, attackers[attacker_index]))
        for (int dragon = active_cards_count[player] - 1; dragon >= 0; --dragon)
          if (in_play(player, dragon) && is_what(player, dragon, TYPE_CREATURE) &&
              has_subtype(player, dragon, SUBTYPE_DRAGON))
            damage_creature(current_turn, attackers[attacker_index], 1, player, dragon);
    return planeswalker(player, card, event, 5);
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return planeswalker(player, card, event, 5);
  instance = get_card_instance(player, card);
  if (event == EVENT_ACTIVATE)
  {

    instance->number_of_targets = '\0';
    instance->info_slot = 0;
    if (dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "PWs become dragons", 1,
                    5, 1, "Create a dragon token", 1, 5, 0xfffffffd, 0, 0, 0, 0) == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    return planeswalker(player, card, event, 5);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    if (instance->info_slot == CHOICE_MUTATE)
    {

      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          instance = in_play(player, c);
          if ((instance != NULL) &&
              (index_or_value = is_what(player, c, 0x100), index_or_value != 0))
          {

            index_or_value = animate_other(player, card, player, c, 4, 4, KEYWORD_FLYING,
                                           SP_KEYWORD_0, COLOR_TEST_RED, 0);
            instance = get_card_instance(player, index_or_value);
            instance->targets[9].card = 0x30;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }
    }
    else if (instance->info_slot == CHOICE_TOKEN)
    {
      default_token_definition(player, card, CARD_ID_DRAGON, &token);
      token.tou = 4;
      token.pow = 4;
      generate_token(&token);
    }
  }
  return planeswalker(player, card, event, 5);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int war_god_eternal(int player, int card, event_t event);

/* ManalinkEh 2022: 0x21d5ef0; original cards/war_of_the_spark.c:5670. */
int card_single_combat(int player, int card, event_t event)
{
  /* Single Combat	|3|W|W
   * Sorcery
   * Each player chooses a creature or planeswalker they control, then sacrifices the rest. Players can't cast creature or planeswalker spells until the end of your next turn. */

  int target_card;
  int value;
  card_instance_t *instance;
  card_instance_t *legacy;
  unsigned int pass;
  unsigned int t_player;
  bool own_turn;

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
      test_definition_t this_test;
      instance = get_card_instance(player, card);
      pass = 0;
      do
      {
        t_player = current_turn ^ pass;
        if (can_sacrifice(player, t_player, 1, TARGET_TYPE_PLANESWALKER | TYPE_CREATURE,
                          SUBTYPE_INVALID) != 0)
        {
          target_definition_t td;
          default_target_definition(player, card, &td, 0x102);
          td.illegal_abilities = 0;
          td.who_chooses = (int32_t)(t_player);
          td.allowed_controller = (int32_t)(t_player);
          value = can_target(&td);
          if ((value != 0) &&
              (value = new_pick_target(&td, "Select a creature or planeswalker to keep.", t_player,
                                       0x80),
               value != 0))
          {
            state_untargettable(instance->targets[t_player].player,
                                instance->targets[t_player].card, 1);
          }
        }
        value = target_player_cant_cast_type(player, card, t_player, 0x102);
        own_turn = current_turn == player;
        legacy = get_card_instance(player, value);
        pass = pass + 1;
        legacy->targets[2].player = own_turn + 1;
      } while (pass != 2);
      default_test_definition(&this_test, 0x102);
      new_manipulate_all(player, card, 2, &this_test, ACT_KILL_SACRIFICE);
      value = instance->targets[0].player;
      if ((value != -1) && (target_card = instance->targets[0].card, target_card != -1))
      {
        state_untargettable(value, target_card, 0);
      }
      value = instance->targets[1].player;
      if ((value != -1) && (target_card = instance->targets[1].card, target_card != -1))
      {
        state_untargettable(value, target_card, 0);
      }
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x21d6150; original cards/war_of_the_spark.c:5736. */
int card_sorin_vengeful_bloodlord(int player, int card, event_t event)
{
  /* Sorin, Vengeful Bloodlord	|2|W|B
   * Legendary Planeswalker - Sorin (4)
   * During your turn, creatures and planeswalkers you control have lifelink.
   * +2: Sorin deals 1 damage to target player or planeswalker.
   * -X: Return target creature card with mana value X from your graveyard to the battlefield. That creature is a Vampire in addition to its other types. */

  enum
  {
    CHOICE_DAMAGE = 1,
    CHOICE_VAMPIRE = 2
  };

  int i;
  int choice;
  int value;
  card_instance_t *instance;
  int current_loyalty;
  int matches_or_loyalty;
  unsigned int can_reanimate;
  int loyalty_to_remove;
  target_definition_t td;
  test_definition_t test_resurrect;

  check_legend_rule(player, card, event);
  lifelink(player, card, event);
  if (event == EVENT_ABILITIES)
  {
    if (((((current_turn == player) && (instance = in_play(player, card), instance != NULL)) &&
          (affected_card_controller == player)) &&
         (instance = in_play(player, affected_card), instance != NULL)) &&
        (((value = is_what(affected_card_controller, affected_card, 2),
           value != 0 ||
               (value = is_planeswalker(affected_card_controller, affected_card), value != 0)) &&
          ((value = is_humiliated(player, card, AL_STATIC),
            value == 0 && (value = is_humiliated(affected_card_controller, affected_card, AL_KEY),
                           value == 0))))))
    {

      instance = get_card_instance(affected_card_controller, affected_card);
      instance->ext->sp_keywords = instance->ext->sp_keywords | 2;
    }
    return planeswalker(player, card, event, 4);
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x100);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    default_test_definition(&test_resurrect, 2);
    value = count_counters(player, card, COUNTER_LOYALTY);
    test_resurrect.cmc = value + 1;
    test_resurrect.cmc_flag = 3;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      value = new_special_count_grave(player, &test_resurrect);
      can_reanimate = 0;
      if (0 < value)
      {
        value = graveyard_has_shroud(player);
        can_reanimate = (unsigned int)(value == 0);
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Deal 1 damage", 1,
                           5, 2, "Reanimate", can_reanimate, 10, 0, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_DAMAGE)
      {
        pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER");
        return planeswalker(player, card, event, 4);
      }
      if (choice != CHOICE_VAMPIRE)
        return planeswalker(player, card, event, 4);

      if (player == 1)
      {
        loyalty_to_remove = 0;
        value = 0;
        for (i = 0;
             matches_or_loyalty = count_counters(1, card, COUNTER_LOYALTY), i <= matches_or_loyalty;
             i = i + 1)
        {

          test_resurrect.cmc = i;
          matches_or_loyalty = new_special_count_grave(1, &test_resurrect);
          if (value < matches_or_loyalty)
          {
            value = matches_or_loyalty;
            loyalty_to_remove = i;
          }
        }
      }
      else if (player == 0)
        loyalty_to_remove = choose_a_number(0, "Remove how much Loyalty?",
                                            count_counters(0, card, COUNTER_LOYALTY));
      else
        loyalty_to_remove = 0;
      value = loyalty_to_remove + 1;
      if (loyalty_to_remove < 0)
        spell_fizzled = 1;
      else
      {
        current_loyalty = count_counters(player, card, COUNTER_LOYALTY);
        if (current_loyalty < loyalty_to_remove)
          spell_fizzled = 1;
        else
        {
          remove_counters(player, card, player, card, COUNTER_LOYALTY, loyalty_to_remove);
          instance->targets[1].player = loyalty_to_remove;
        }
      }
      test_resurrect.cmc = value;
      select_target_from_grave_source(player, card, player, SFG_0, 1, &test_resurrect, 0);
      return planeswalker(player, card, event, 4);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 4);
    default_target_definition(player, card, &td, 0x100);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    default_test_definition(&test_resurrect, 2);
    value = count_counters(player, card, COUNTER_LOYALTY);
    test_resurrect.cmc = value + 1;
    test_resurrect.cmc_flag = 3;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    if (instance->info_slot == 1)
    {
      if (valid_target(&td) != 0)
      {
        damage_target0(player, card, 1);
      }
    }
    else
    {

      if ((instance->info_slot == 2) &&
          (value = validate_target_from_grave_source(player, card, player, 0), -1 < value))
      {

        add_a_subtype_permanently(
            player, card, player,
            reanimate_permanent(player, card, player, value, REANIMATE_DEFAULT), SUBTYPE_VAMPIRE);
      }
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x21d6710; original cards/war_of_the_spark.c:5847. */
int card_spark_double(int player, int card, event_t event)
{
  /* Spark Double	|3|U
   * Creature - Illusion 0/0
   * You may have this creature enter as a copy of a creature or planeswalker you control, except it enters with an additional +1/+1 counter on it if it's a creature, it enters with an additional loyalty counter on it if it's a planeswalker, and it isn't legendary. */

  int p;
  int c;
  card_instance_t *instance;
  int creature;
  counter_t type;
  int planeswalker;
  target_definition_t td;

  base_target_definition(player, card, &td, 0x102);
  enters_the_battlefield_as_copy_of_any(player, card, event, &td,
                                        "TARGET_CREATURE_OR_PLANESWALKER");
  if (event == EVENT_RESOLVE_SPELL)
  {

    instance = get_card_instance(player, card);
    p = instance->targets[0].player;
    instance = get_card_instance(player, card);
    c = instance->targets[0].card;
    if ((p != -1) && (c != -1))
    {
      hack_silent_counters = hack_silent_counters + 1;
      creature = is_what(p, c, 2);
      type = COUNTER_P1_P1;
      if (creature == 0)
      {
        planeswalker = is_what(p, c, 0x100);
        type = ((planeswalker == 0) ? 189U : 0U) + COUNTER_LOYALTY;
      }
      add_counters(player, card, player, card, type, 1);
      hack_silent_counters = hack_silent_counters + -1;
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static void ugins_token_processing(token_generation_t *token, int beast, int number);
static int ugin_the_ineffable_effect(int player, int card, event_t event);

/* ManalinkEh 2022: 0x21c7c40; original cards/war_of_the_spark.c:6064. */
static int teferi_flash_effect(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event == EVENT_CAN_SKIP_TURN)
  {
    instance = get_card_instance(player, card);
    if (instance->targets[9].card < 1)
    {
      kill_card(player, card, KILL_EXILE);
    }
    else
    {
      instance = get_card_instance(player, card);
      instance->targets[9].card = instance->targets[9].card + -1;
    }
  }
  else if (((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2) || (event == EVENT_ACTIVATE))
  {
    base_target_definition(player, card, &td, 8);
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.zone = TARGET_ZONE_HAND;
    result = can_play_cards_as_though_they_had_flash(player, card, event, &td,
                                                     "Select a sorcery card", 1);
    return result;
  }
  return 0;
}

/* ManalinkEh 2022: 0x21d6dc0; original cards/war_of_the_spark.c:6083. */
int card_teferi_time_raveler(int player, int card, event_t event)
{
  /* Teferi, Time Raveler	|1|W|U
   * Legendary Planeswalker - Teferi (4)
   * Each opponent can cast spells only any time they could cast a sorcery.
   * +1: Until your next turn, you may cast sorcery spells as though they had flash.
   * -3: Return up to one target artifact, creature, or enchantment to its owner's hand. Draw a card. */

  enum
  {
    CHOICE_FLASH = 1,
    CHOICE_BOUNCE = 2
  };

  card_instance_t *instance;
  int result;
  bool own_turn;
  target_definition_t *permanent_target;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if (((affected_card_controller != player) &&
         (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
        (result = can_sorcery_be_played(affected_card_controller, EVENT_MODIFY_COST_GLOBAL),
         result == 0))
    {
      infinite_casting_cost();
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_target_definition(player, card, &td, 0x46);
      td.allow_cancel = 1;
      instance = get_card_instance(player, card);
      if (event == EVENT_ACTIVATE)
      {

        instance->number_of_targets = '\0';
        instance->info_slot = 0;
        result = can_target(&td);
        result =
            dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                        "Cast sorceries with flash", 1, 5, 1, "Bounce and draw", 1,
                        ((result == 0) ? 3U : 10U), 0xfffffffd, 0, 0, 0, 0);
        if (result == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if ((result == CHOICE_BOUNCE) && (result = can_target(&td), result != 0))
        {
          new_pick_target(&td, "Select target artifact, creature or enchantment.", 0, 0x80);
        }
        return planeswalker(player, card, event, 4);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return planeswalker(player, card, event, 4);
      default_target_definition(player, card, &td, 0x46);
      td.allow_cancel = 1;
      instance = get_card_instance(player, card);
    }
    permanent_target = &td;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      if (instance->info_slot == 1)
      {
        own_turn = current_turn == player;
        result = create_legacy_activate(player, card, teferi_flash_effect);
        instance = get_card_instance(player, result);
        instance->targets[9].card = (unsigned int)own_turn;
      }
      else if (instance->info_slot == 2)
      {
        result = valid_target(permanent_target);
        if (result != 0)
        {
          bounce_permanent(instance->targets[0].player, instance->targets[0].card);
        }
        draw_a_card(player);
      }
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x21d7260; original cards/war_of_the_spark.c:6212. */
int card_tezzeret_master_of_the_bridge(int player, int card, event_t event)
{
  /* Tezzeret, Master of the Bridge	|4|U|B
   * Legendary Planeswalker - Tezzeret (5)
   * Creature and planeswalker spells you cast have affinity for artifacts. (They cost |1 less to cast for each artifact you control.)
   * +2: Tezzeret deals X damage to each opponent, where X is the number of artifacts you control. You gain X life.
   * -3: Return target artifact card from your graveyard to your hand.
   * -8: Exile the top ten cards of your library. Put all artifact cards from among them onto the battlefield. */

  enum
  {
    CHOICE_DAMAGE_LIFE = 1,
    CHOICE_ARCHAEOLOGY = 2,
    CHOICE_DIG = 3
  };

  int i;
  int count;
  int *end;
  card_instance_t *instance;
  int value;
  int hand_card;
  unsigned int can_recur;
  int *iid_cursor;
  int artifacts_found;
  int revealed;
  int iids[10];
  test_definition_t test;

  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if ((affected_card_controller == player) &&
        (((value = is_what(player, affected_card, 2),
           value != 0 ||
               (value = is_what(affected_card_controller, affected_card, 0x100), value != 0)) &&
          (value = is_humiliated(player, card, AL_STATIC), value == 0))))
    {
      count = count_subtype(player, 0x40, -1);
      COST_COLORLESS = COST_COLORLESS - (char)count;
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_test_definition(&test, 0x40);
      instance = get_card_instance(player, card);
      if (event == EVENT_ACTIVATE)
      {

        instance->number_of_targets = '\0';
        instance->info_slot = 0;
        value = new_special_count_grave(player, &test);
        can_recur = 0;
        if (0 < value)
        {
          value = graveyard_has_shroud(player);
          can_recur = (unsigned int)(value == 0);
        }
        value = dialog_impl(
            player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Damage opponent & gain life", 1,
            10, 2, "Recur an artifact from your graveyard", can_recur, 10, 0xfffffffd,
            "Put into play artifacts from your library", 1, 0x14, 0xfffffff8, 0, 0, 0, 0);
        if (value == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if ((value == CHOICE_ARCHAEOLOGY) &&
            (value = select_target_from_grave_source(player, card, player, SFG_0, 1, &test, 0),
             value == -1))
        {
          spell_fizzled = 1;
          return 0;
        }
        return planeswalker(player, card, event, 5);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return planeswalker(player, card, event, 5);
      default_test_definition(&test, 0x40);
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      value = instance->info_slot;
      if (value == CHOICE_DAMAGE_LIFE)
      {

        count = count_permanents_by_type(player, TYPE_ARTIFACT);
        damage_player(1 - player, count, player, card);
        gain_life(player, count);
      }
      else if (value == CHOICE_ARCHAEOLOGY)
      {

        value = validate_target_from_grave_source(player, card, player, 0);
        if (value != -1)
        {
          from_grave_to_hand(player, value, 1);
        }
      }
      else
      {

        if ((value == CHOICE_DIG) && (value = count_deck(player), value != 0))
        {
          revealed = value;
          if (10 < value)
          {
            revealed = 10;
          }

          show_deck(current_turn, deck_ptr[player], revealed,
                    "Cards revealed by Tezzeret, Master of the Bridge", 0, gs_done_008b40e0);
          show_deck(current_turn ^ 1, deck_ptr[player], revealed,
                    "Cards revealed by Tezzeret, Master of the Bridge", 0, gs_done_008b40e0);
          iid_cursor = iids;
          for (i = 10; i != 0; i = i + -1)
          {
            *iid_cursor = -1;
            iid_cursor = iid_cursor + 1u;
          }
          if (value < 1)
          {
            rfg_top_n_cards_of_deck(player, revealed);
          }
          else
          {
            artifacts_found = 0;
            value = 0;
            do
            {
              if (is_what(-1, deck_ptr[player][value], 0x40) != 0)
              {
                iids[artifacts_found] = deck_ptr[player][value];
                artifacts_found = artifacts_found + 1;
              }
              value = value + 1;
            } while (value < revealed);
            rfg_top_n_cards_of_deck(player, revealed);
            if (0 < artifacts_found)
            {
              iid_cursor = iids;
              end = iid_cursor + artifacts_found;
              do
              {

                value = find_iid_in_rfg(player, *iid_cursor);
                if (-1 < value)
                {

                  hand_card = add_card_to_hand(player, *iid_cursor);
                  remove_card_from_rfg_by_position(player, value);
                  put_into_play(player, hand_card);
                }
                iid_cursor = iid_cursor + 1;
              } while (end != iid_cursor);
            }
          }
        }
      }
    }
  }
  return planeswalker(player, card, event, 5);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21d7790; original cards/war_of_the_spark.c:6313. */
int card_the_elderspell(int player, int card, event_t event)
{
  /* The Elderspell	|B|B
   * Sorcery
   * Destroy any number of target planeswalkers. Choose a planeswalker you control. Put two loyalty counters on it for each planeswalker destroyed this way. */

  target_t *target;
  unsigned int packed_counts;
  target_t *selected_target;
  int i;
  card_instance_t *instance;
  card_instance_t *accumulator;
  target_definition_t td;

  if (event == EVENT_RESOLVE_GENERAL_EFFECT)
  {

    instance = get_card_instance(player, card);
    packed_counts = instance->targets[10].player;
    i = (packed_counts >> 0x10) + (packed_counts & 0xffff);
    default_target_definition(player, card, &td, 0x100);
    td.allowed_controller = player;
    td.allow_cancel = 0;
    if (((i != 0) && (can_target(&td) != 0)) &&
        (new_pick_target(&td, "TARGET_PLANESWALKER", 0, 0) != 0))
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                   COUNTER_LOYALTY, i * 2);
    }
    return 0;
  }
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x100);
  td.preferred_controller = 2;
  td.allow_cancel = 3;
  if (event == EVENT_RESOLVE_SPELL)
  {

    instance = get_card_instance(player, card);
    if (instance->number_of_targets != '\0')
    {
      i = 0;
      accumulator = NULL;
      do
      {
        if (validate_target(player, card, &td, i) == 0)
        {
          i = i + 1;
          continue;
        }
        accumulator = legacy_permanents_destroyed_this_way_accumulate(
            player, card, accumulator, instance->targets[i].player, instance->targets[i].card);
        selected_target = instance->targets + i;
        target = instance->targets + i;
        i = i + 1;
        kill_card(target->player, selected_target->card, KILL_DESTROY);
      } while (i < (int)(unsigned int)instance->number_of_targets);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x20, &td, "TARGET_PLANESWALKER", 0xb, NULL);
}

/* ManalinkEh 2022: 0x21d7a50; original cards/war_of_the_spark.c:6364. */
int card_the_wanderer(int player, int card, event_t event)
{
  /* The Wanderer	|3|W
   * Legendary Planeswalker (5)
   * Prevent all noncombat damage that would be dealt to you and other permanents you control.
   * -2: Exile target creature with power 4 or greater. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  check_legend_rule(player, card, event);
  instance = noncombat_damage_being_prevented(event);
  if ((instance != NULL) && (instance->damage_target_player == player))
  {

    prevent_damage(affected_card_controller, affected_card, instance->info_slot);
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.power_requirement = 0x1004;
    td.allow_cancel = 1;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = can_target(&td);
      if (result == 0)
      {
        return 0;
      }
      result = count_counters(player, card, COUNTER_LOYALTY);
      if (result < 2)
      {
        return 0;
      }
      return planeswalker(player, card, event, 5);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = can_target(&td);
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL,
                           DLG_NO_STORAGE, DLG_PLANESWALKER, "Exile target creature with power 4 or greater.",
                           result, 5, 0xfffffffe, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      new_pick_target(&td, "Select target creature with power 4 or greater.", 0, 0x81);
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    default_target_definition(player, card, &td, 2);
    td.power_requirement = 0x1004;
    td.allow_cancel = 1;
    instance = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
  }
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x21d7cc0; original cards/war_of_the_spark.c:6443. */
int card_tibalt_rakish_instigator(int player, int card, event_t event)
{
  /* Tibalt, Rakish Instigator	|2|R
   * Legendary Planeswalker - Tibalt (5)
   * Your opponents can't gain life.
   * -2: Create a 1/1 red Devil creature token with "When this token dies, it deals 1 damage to any target." */

  card_instance_t *instance;
  int result;
  token_generation_t token;

  check_legend_rule(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    player_bits[(1 - player)] =
        player_bits[(1 - player)] | 0x200000;
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        result = count_counters(player, card, COUNTER_LOYALTY);
        if (result < 2)
        {
          return 0;
        }
        return planeswalker(player, card, event, 5);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return planeswalker(player, card, event, 5);
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL,
                           DLG_NO_STORAGE, DLG_PLANESWALKER, "Create a Devil", 1, 5, 0xfffffffe, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      default_token_definition(player, card, CARD_ID_DEVIL, &token);
      token.special_infos = 0x42;
      generate_token(&token);
    }
  }
  return planeswalker(player, card, event, 5);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21d7e70; original cards/war_of_the_spark.c:6489. */
int card_time_wipe(int player, int card, event_t event)
{
  /* Time Wipe	|2|W|W|U
   * Sorcery
   * Return a creature you control to its owner's hand, then destroy all creatures. */

  int result;
  target_definition_t td;
  test_definition_t this_test;
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
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = player;
      td.preferred_controller = player;
      td.illegal_abilities = 0;
      td.allow_cancel = 0;
      if ((card == affected_card) && (player == affected_card_controller))
      {
        result = can_target(&td);
        if (result != 0)
        {
          pick_target(&td, "SELECT_A_CREATURE_YOU_CONTROL");
        }
        return basic_spell(player, card, EVENT_CAST_SPELL);
      }
      return basic_spell(player, card, event);
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = player;
      td.preferred_controller = player;
      td.illegal_abilities = 0;
      td.allow_cancel = 0;
      if (event == EVENT_RESOLVE_SPELL)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          action_on_target(player, card, 0, ACT_BOUNCE);
        }
        default_test_definition(&this_test, 2);
        new_manipulate_all(player, card, 2, &this_test, ACT_KILL_DESTROY);
        kill_card(player, card, KILL_DESTROY);
      }
      return basic_spell(player, card, event);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.illegal_abilities = 0;
  td.allow_cancel = 0;
  return basic_spell(player, card, event);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21d80a0; original cards/war_of_the_spark.c:6542. */
int card_tolsimir_friend_to_wolves(int player, int card, event_t event)
{
  /* Tolsimir, Friend to Wolves	|2|G|G|W
   * Legendary Creature - Elf Scout 3/3
   * When Tolsimir enters, create Voja, Friend to Elves, a legendary 3/3 green and white Wolf creature token.
   * Whenever a Wolf you control enters, you gain 3 life and that creature fights up to one target creature you don't control. */

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB && trigger_cause_controller == player && trigger_cause == card)
    generate_token_by_id(player, card, CARD_ID_VOJA_FRIEND_TO_ELVES);
  int humiliated = is_humiliated(player, card, AL_TRIG);
  if (!humiliated && trigger_condition == TRIGGER_COMES_INTO_PLAY && card == affected_card &&
      player == affected_card_controller)
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.subtype = 0xd7;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB && trigger_cause != card)
  {
    gain_life(player, 3);
    target_definition_t td;
    default_target_definition(trigger_cause_controller, trigger_cause, &td, 2);
    td.allowed_controller = 1 - player;
    td.allow_cancel = 1;
    td.who_chooses = player;
    td.preferred_controller = td.allowed_controller;
    if (can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0))
    {
      card_instance_t *instance = get_card_instance(trigger_cause_controller, trigger_cause);
      fight(trigger_cause_controller, trigger_cause, instance->targets[0].player,
            instance->targets[0].card);
      instance->number_of_targets = 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21d82d0; original cards/war_of_the_spark.c:6614. */
int card_tyrants_scorn(int player, int card, event_t event)
{
  /* Tyrant's Scorn	|U|B
   * Instant
   * Choose one -
   * * Destroy target creature with mana value 3 or less.
   * * Return target creature to its owner's hand. */

  enum
  {
    CHOICE_KILL = 1,
    CHOICE_BOUNCE = 2
  };

  int choice;
  card_instance_t *instance;
  int can_bounce_or_valid;
  target_definition_t td1;
  target_definition_t td2;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td1, 2);
    td1.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
    td1.extra = 3;
    default_target_definition(player, card, &td2, 2);
    get_card_instance(player, card);
    if (generic_spell(player, card, EVENT_CAN_CAST, 1, &td1, NULL, 1, NULL) != 0)
    {
      return 1;
    }
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td2, NULL, 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td1, 2);
      td1.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
      td1.extra = 3;
      default_target_definition(player, card, &td2, 2);
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        if (check_special_flags2(player, card, -0x80000000) == 0)
        {

          can_bounce_or_valid = can_target(&td2);
          choice = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM,
                               "Destroy target creature with CMC 3 or less", can_target(&td1), 5,
                               "Bounce target creature", can_bounce_or_valid, 5, 0, 0, 0, 0);
          if (choice == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
          if (choice == CHOICE_KILL)
          {
            return generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td1,
                                 "Select target creature with CMC 3 or less.", 1, NULL);
          }
          if (choice == CHOICE_BOUNCE)
          {
            return generic_spell(player, card, EVENT_CAST_SPELL, 1, &td2, "TARGET_CREATURE", 1,
                                 NULL);
          }
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td1, 2);
    td1.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
    td1.extra = 3;
    default_target_definition(player, card, &td2, 2);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      if (instance->info_slot == CHOICE_KILL)
      {
        if (valid_target(&td1) != 0)
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        }
      }
      else if ((instance->info_slot == CHOICE_BOUNCE) &&
               (can_bounce_or_valid = valid_target(&td2), can_bounce_or_valid != 0))
      {
        bounce_permanent(instance->targets[0].player, instance->targets[0].card);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td1, 2);
    td1.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
    td1.extra = 3;
    default_target_definition(player, card, &td2, 2);
    get_card_instance(player, card);
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21c76e0; original cards/war_of_the_spark.c:6686. */
static int ugin_the_ineffable_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;
  int iid;

  instance = get_card_instance(player, card);
  other_leaves_play(player, card, (int)instance->damage_target_player,
                         instance->damage_target_card, event, 1, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    result = instance->targets[9].card;
    if (result != -1)
    {

      add_card_to_hand(player, result);
    }
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (ai_is_speculating == 1)
    {
      return 0;
    }
    if ((player == 1) && (((unsigned char)trace_mode & 2) == 0))
    {
      return 0;
    }
    if (instance->targets[9].card != -1)
    {
      return 0;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    result = instance->targets[9].card;
    if (result != -1)
    {
      iid = result;
      show_deck(player, &iid, 1, "Card exiled by Ugin, the Ineffable", 0, gs_ok_00924800);
      spell_fizzled = 1;
    }
  }
  else if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
  {
    return 0;
  }
  result = generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x21c7140; original cards/war_of_the_spark.c:6711. */
static void ugins_token_processing(token_generation_t *token, int beast, int number)
{
  int *library;
  int card;
  card_instance_t *instance;

  if (*deck_ptr[token->s_player] == -1)
  {
    return;
  }

  card = create_targetted_legacy_activate(token->s_player, token->s_card, ugin_the_ineffable_effect,
                                          token->t_player, beast);
  if (card == -1)
  {
    backtrace_and_abort("cards/war_of_the_spark.c:6714\nlegacy != -1");
  }
  library = deck_ptr[token->s_player];
  instance = get_card_instance(token->t_player, card);
  instance->targets[9].card = *library;
  return;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21d8740; original cards/war_of_the_spark.c:6719. */
int card_ugin_the_ineffable(int player, int card, event_t event)
{
  /* Ugin, the Ineffable	|6
   * Legendary Planeswalker - Ugin (4)
   * Colorless spells you cast cost |2 less to cast.
   * +1: Exile the top card of your library face down and look at it. Create a 2/2 colorless Spirit creature token. When that token leaves the battlefield, put the exiled card into your hand.
   * -3: Destroy target permanent that's one or more colors. */

  enum
  {
    CHOICE_MANIFEST = 1,
    CHOICE_DESTROY = 2
  };

  card_instance_t *instance;
  int result;
  unsigned int colors;
  target_definition_t *permanent_target;
  token_generation_t token;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    colors = get_color_real_or_iid(event_result, affected_card_controller, affected_card);
    if (((colors & 0x3e) == 0) && (result = is_humiliated(player, card, AL_STATIC), result == 0))
    {
      COST_COLORLESS = COST_COLORLESS + -2;
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_target_definition(player, card, &td, 0x47);
      td.required_color = 0x3e;
      instance = get_card_instance(player, card);
      if (event == EVENT_ACTIVATE)
      {

        instance->number_of_targets = '\0';
        instance->info_slot = 0;
        result =
            dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Create a Spirit", 1,
                        5, 1, "Destroy a colored permanent", 1, 10, 0xfffffffd, 0, 0, 0, 0);
        if (result == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (result == CHOICE_DESTROY)
        {
          new_pick_target(&td, "Select target permanent that\'s one or more colors.", 0, 0x81);
        }
        return planeswalker(player, card, event, 4);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return planeswalker(player, card, event, 4);
      default_target_definition(player, card, &td, 0x47);
      td.required_color = 0x3e;
      instance = get_card_instance(player, card);
    }
    permanent_target = &td;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      if (instance->info_slot == CHOICE_MANIFEST)
      {

        if (*deck_ptr[player] == -1)
        {
          default_token_definition(player, card, CARD_ID_SPIRIT, &token);
          token.tou = 2;
          token.pow = 2;
          token.color_forced = '\x01';
          token.no_sleight = '\x01';
          generate_token(&token);
        }
        else
        {
          show_deck(player, deck_ptr[player], 1, "Here\'s the top card of your deck", 0,
                    gs_done_008b40e0);
          default_token_definition(player, card, CARD_ID_SPIRIT, &token);
          token.tou = 2;
          token.pow = 2;
          token.color_forced = '\x01';
          token.no_sleight = '\x01';
          token.special_code_on_generation = ugins_token_processing;
          generate_token(&token);
          obliterate_top_card_of_deck(player);
          play_sound_effect(WAV_DESTROY);
        }
      }
      else if ((instance->info_slot == CHOICE_DESTROY) && (result = valid_target(permanent_target), result != 0))
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
    }
  }
  return planeswalker(player, card, event, 4);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21d8ac0; original cards/war_of_the_spark.c:6796. */
int card_unlikely_aid(int player, int card, event_t event)
{
  /* Unlikely Aid	|1|B
   * Instant
   * Target creature gets +2/+0 and gains indestructible until end of turn. (Damage and effects that say "destroy" don't destroy it.) */

  int result;
  result = vanilla_instant_pump(player, card, event, 2, player, 2, 0, 0, 0x800);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21d8b10; original cards/war_of_the_spark.c:6820. */
int card_viviens_arkbow(int player, int card, event_t event)
{
  /* Vivien's Arkbow	|1|G
   * Legendary Artifact
   * |X, |T, Discard a card: Look at the top X cards of your library. You may put a creature card with mana value X or less from among them onto the battlefield. Put the rest on the bottom of your library in a random order. */

  card_instance_t *instance;
  card_instance_t *activation;
  char buffer[75];
  test_definition_t test;

  check_legend_rule(player, card, event);
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    activation = get_card_instance(player, card);
    scnprintf(buffer, 0x4b, " Select a creature card with converted mana cost %d or less.",
              activation->info_slot);
    new_default_test_definition(&test, 2, buffer);
    test.cmc = instance->info_slot + 1;
    test.cmc_flag = 3;
    reveal_top_cards_of_library_and_choose(instance->parent_controller, instance->parent_card,
                                           player, instance->info_slot, 1, TUTOR_PLAY, 0,
                                           TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test);
  }
  return generic_activated_ability(player, card, event, 0x21, -1, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21c7da0; original cards/war_of_the_spark.c:6856. */
static int fx_vigilance_reach_until_your_next_turn(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (event == EVENT_ABILITIES)
  {
    instance = get_card_instance(player, card);
    if ((instance->damage_target_card == affected_card) &&
        (instance->damage_target_player == affected_card_controller))
    {
      vigilance((int)instance->damage_target_player, instance->damage_target_card, EVENT_ABILITIES);
      event_result = event_result | 0x400;
    }
  }
  else if ((event == EVENT_BEGIN_TURN) && (player == current_turn))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int effect_yanggu_mana(int player, int card, event_t event);

/* ManalinkEh 2022: 0x21c7ac0; original cards/war_of_the_spark.c:6872. */
static int look_at_card(int player, int card, event_t event)
{
  card_instance_t *instance;
  unsigned int visible;
  int iid;

  instance = get_card_instance(player, card);
  iid = instance->targets[0].card;
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (ai_is_speculating == 1)
    {
      visible = 0;
    }
    else
    {
      visible = 1;
      if (player == 1)
      {
        visible = (unsigned int)trace_mode >> 1 & 1;
      }
    }
  }
  else
  {

    visible = 0;
    if (event == EVENT_ACTIVATE)
    {
      show_deck(player, &iid, 1, "Face-down card in exile", 0, gs_ok_00924800);
      spell_fizzled = 1;
    }
  }
  return visible;
}

/* ManalinkEh 2022: 0x21c7a90; original cards/war_of_the_spark.c:7035. */
static int has_deathtouch(int unused, int unused2, int player, int card)
{
  int result;
  result = check_for_special_ability(player, card, 0x200);
  return (unsigned int)(result != 0);
}

/* ManalinkEh 2022: 0x21d9130; original cards/war_of_the_spark.c:7043. */
int card_vraska_swarms_eminence(int player, int card, event_t event)
{
  /* Vraska, Swarm's Eminence	|2|BG|BG
   * Legendary Planeswalker - Vraska (5)
   * Whenever a creature you control with deathtouch deals damage to a player or planeswalker, put a +1/+1 counter on that creature.
   * -2: Create a 1/1 black Assassin creature token with deathtouch and "Whenever this token deals damage to a planeswalker, destroy that planeswalker." */

  card_instance_t *instance;
  int result;
  target_t *target;
  target_t *next_target;
  token_generation_t token;
  test_definition_t test;

  check_legend_rule(player, card, event);
  hybrid(player, card, event);
  default_test_definition(&test, 2);
  test.special_selection_function = has_deathtouch;
  instance = get_card_instance(player, card);
  check_damage_test(player, -1, event,
                    DDBM_TRACE_DAMAGING_SOURCES | DDBM_MAY_DAMAGE_PLANESWALKER |
                        DDBM_MUST_DAMAGE_PLAYER,
                    player, card, 0, &test);
  result = resolve_damage_trigger(player, card, event, DDBM_TRACE_DAMAGING_SOURCES, player, card);

  if (0 < result)
  {
    target = instance->targets;
    do
    {
      next_target = target + 1;
      add_counters(player, card, target->player, target->card, COUNTER_P1_P1, 1);
      target = next_target;
    } while (next_target != instance->targets + result);
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = count_counters(player, card, COUNTER_LOYALTY);
    if (result < 2)
    {
      return 0;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {

    instance->number_of_targets = '\0';
    instance->info_slot = 0;
    result =
        dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL, DLG_NO_STORAGE,
                    DLG_PLANESWALKER, "Create an assassin", 1, 5, 0xfffffffe, 0, 0, 0, 0);
    if (result == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    default_token_definition(player, card, CARD_ID_ASSASSIN, &token);
    token.s_key_plus = 0x200;
    token.special_infos = 0x43;
    generate_token(&token);
  }
  return planeswalker(player, card, event, 5);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21d93c0; original cards/war_of_the_spark.c:7101. */
int card_wall_of_runes(int player, int card, event_t event)
{
  /* Wall of Runes	|U
   * Creature - Wall 0/4
   * Defender (This creature can't attack.)
   * When this creature enters, scry 1. (Look at the top card of your library. You may put that card on the bottom.) */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  scry(player, 1);
  return 0;
}

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21ca670; original cards/war_of_the_spark.c:956. */
int card_command_the_dreadhorde(int player, int card, event_t event)
{
  /* Command the Dreadhorde	|4|B|B
   * Sorcery
   * Choose any number of target creature and/or planeswalker cards in graveyards. ~ deals damage to you equal to the total mana value of those cards. Put them onto the battlefield under your control. */

  int value;
  card_instance_t *instance;
  int end_index;
  int cmc;
  unsigned int pass;
  unsigned int grave_player;
  target_t *ret_location;
  uint8_t selection_count_byte;
  int limit;
  int selected_count;
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
      instance = get_card_instance(player, card);
      if ((((card == affected_card) && (player == affected_card_controller)) &&
           ((value = count_graveyard_by_type(player, 0x102),
             value != 0 || (value = count_graveyard_by_type(1 - player, 0x102), value != 0)))) &&
          ((value = graveyard_has_shroud(player),
            value == 0 || (value = graveyard_has_shroud(1 - player), value == 0))))
      {
        instance->info_slot = 0;
        new_default_test_definition(&test, 0x102,
                                    "Select any number of creature and/or planeswalker cards.");
        limit = count_graveyard_by_type(2, 0x102);
        if (10 < limit)
        {
          limit = 10;
        }
        selected_count = 0;
        pass = 0;
        do
        {
          grave_player = current_turn ^ pass;
          if (count_graveyard_by_type(grave_player, 0x102) != 0)
          {
            ret_location = instance->targets + selected_count;
            value = select_multiple_cards_from_graveyard(player, grave_player, 0, 1, &test,
                                                         limit - selected_count, ret_location);
            end_index = value + selected_count;
            if (selected_count < end_index)
            {
              do
              {
                if (-1 < ret_location->player)
                {
                  cmc = get_cmc_by_internal_id(ret_location->card);
                  instance->info_slot = instance->info_slot + cmc;
                }
                ret_location = ret_location + 1;
              } while (instance->targets + end_index != ret_location);
            }
            selected_count = end_index;
            if (current_turn == grave_player)
            {
              selection_count_byte = (uint8_t)value;
              ((unsigned char *)&instance->info_slot)[1] = selection_count_byte;
            }
          }
          pass = pass + 1;
        } while (pass != 2);
      }
      return basic_spell(player, card, event);
    }
    if ((event == EVENT_CAN_CHANGE_TARGET) || (event == EVENT_RESOLVE_SPELL))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_SPELL)
      {
        damage_player(player, (unsigned int)((unsigned char *)&instance->info_slot)[0], player,
                      card);
        pass = (unsigned int)((unsigned char *)&instance->info_slot)[1];
        if (pass != 0)
        {
          grave_player = 0;
          do
          {
            value = validate_target_from_grave(player, card, current_turn, grave_player);
            if (value != -1)
            {
              reanimate_permanent(player, card, current_turn, value, REANIMATE_DEFAULT);
            }
            grave_player = grave_player + 1;
          } while (pass != grave_player);
        }
        if (pass < 9)
        {
          do
          {
            value = validate_target_from_grave(player, card, 1 - current_turn, pass);
            if (value != -1)
            {
              reanimate_permanent(player, card, 1 - current_turn, value, REANIMATE_DEFAULT);
            }
            pass = pass + 1;
          } while (pass != 9);
        }
        kill_card(player, card, KILL_DESTROY);
      }
      return basic_spell(player, card, event);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  get_card_instance(player, card);
  return basic_spell(player, card, event);
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int effect_prevent_damage_to_or_from_until_my_next_turn(int player, int card, event_t event);

/* ManalinkEh 2022: 0x21c6910; original cards/war_of_the_spark.c:1484. */
static int effect_prevent_damage_to_or_from_until_my_next_turn(int player, int card, event_t event)
{
  card_instance_t *damage;
  card_instance_t *effect;

  damage = damage_being_prevented(event);
  if (((damage != NULL) && (effect = get_card_instance(player, card), effect != NULL)) &&
      (((damage->damage_source_card == effect->damage_target_card &&
         (damage->damage_source_player == effect->damage_target_player)) ||
        ((effect->damage_target_card == damage->damage_target_card &&
          (damage->damage_target_player == effect->damage_target_player))))))
  {

    prevent_damage(affected_card_controller, affected_card, damage->info_slot);
  }
  if ((event == EVENT_BEGIN_TURN) && (current_turn == player))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x21cb920; original cards/war_of_the_spark.c:1499. */
int card_dovin_hand_of_control(int player, int card, event_t event)
{
  /* Dovin, Hand of Control	|2|WU
   * Legendary Planeswalker - Dovin (5)
   * Artifact, instant, and sorcery spells your opponents cast cost |1 more to cast.
   * -1: Until your next turn, prevent all damage that would be dealt to and dealt by target permanent an opponent controls. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  check_legend_rule(player, card, event);
  hybrid(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    instance = in_play(player, card);
    if ((((instance != NULL) &&
          (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
         (1 - player == affected_card_controller)) &&
        ((result = is_what(1 - player, affected_card, 0x78),
          result != 0 &&
              (result = is_what(affected_card_controller, affected_card, 1), result == 0))))
    {
      COST_COLORLESS = COST_COLORLESS + '\x01';
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_target_definition(player, card, &td, 0x47);
      td.allowed_controller = 1 - player;
      td.allow_cancel = 1;
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        result = can_target(&td);
        if (result == 0)
        {
          return 0;
        }
        return planeswalker(player, card, event, 5);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return planeswalker(player, card, event, 5);
      default_target_definition(player, card, &td, 0x47);
      td.allowed_controller = 1 - player;
      td.allow_cancel = 1;
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = can_target(&td);
      result =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL, DLG_NO_STORAGE,
                      DLG_PLANESWALKER, "Prevent damage", result, 5, 0xffffffff, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      new_pick_target(&td, "Select target permanent an opponent controls.", 0, 0x81);
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      result = create_targetted_legacy_effect(
          player, card, effect_prevent_damage_to_or_from_until_my_next_turn,
          instance->targets[0].player, instance->targets[0].card);
      alternate_legacy_text(1, instance->parent_controller, result);
      if (((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
          (result = is_what(instance->targets[0].player, instance->targets[0].card, 2),
           result != 0))
      {
        result = get_power(instance->targets[0].player, instance->targets[0].card);
        ai_modifier = ai_modifier + result * 0xc;
      }
    }
  }
  return planeswalker(player, card, event, 5);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21c7e40; original cards/war_of_the_spark.c:2806. */
int bounce_at_eot(int player, int card, event_t event)
{
  char attached_player;
  int delay_or_card;
  card_instance_t *instance;
  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {
    instance = get_card_instance(player, card);
    if ((((trigger_condition == TRIGGER_EOT) && (card == affected_card)) &&
         (player == affected_card_controller)) &&
        (player == reason_for_trigger_controller))
    {
      delay_or_card = instance->targets[1].player;
      if (delay_or_card < 1)
      {
        if (event == EVENT_TRIGGER)
        {
          event_result = event_result | 2;
        }
        else if (event == EVENT_RESOLVE_TRIGGER)
        {
          attached_player = instance->damage_target_player;
          delay_or_card = instance->damage_target_card;
          if (check_state((int)attached_player, delay_or_card, STATE_OUBLIETTED) == 0)
          {
            bounce_permanent((int)attached_player, delay_or_card);
          }
          else
          {
            kill_card(player, card, KILL_EXILE);
          }
        }
      }
      else
      {
        instance->targets[1].player = delay_or_card + -1;
      }
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21cf300; original cards/war_of_the_spark.c:2835. */
int card_ilharg_the_raze_boar(int player, int card, event_t event)
{
  /* Ilharg, the Raze-Boar	|3|R|R
   * Legendary Creature - Boar God 6/6
   * Trample
   * Whenever Ilharg attacks, you may put a creature card from your hand onto the battlefield tapped and attacking. Return that creature to your hand at the beginning of the next end step.
   * When Ilharg dies or is put into exile from the battlefield, you may put it into its owner's library third from the top. */

  int result;
  int legacy;
  card_instance_t *instance;
  test_definition_t test;

  result = duh_mode(player);
  dat_ability(player, card, event, (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    new_default_test_definition(&test, 2, "Select a creature card");
    test.zone = 0x100;
    result = check_battlefield_for_special_card(player, card, player, 0, &test);
    if (result != 0)
    {

      result = new_select_a_card(player, player, 4, 0, 2, -1, &test);
      if (result != 0)
      {

        add_state(player, result, 0x14);
        legacy = create_targetted_legacy_effect(player, card, bounce_at_eot, player, result);
        if ((current_phase == 0x1f) && (-1 < legacy))
        {
          instance = get_card_instance(player, legacy);
          instance->targets[1].player = 1;
        }
        put_into_play(player, result);
      }
    }
  }
  result = war_god_eternal(player, card, event);
  return result;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21cfde0; original cards/war_of_the_spark.c:3231. */
int card_karn_the_great_creator(int player, int card, event_t event)
{
  /* Karn, the Great Creator	|4
   * Legendary Planeswalker - Karn (5)
   * Activated abilities of artifacts your opponents control can't be activated.
   * +1: Until your next turn, up to one target noncreature artifact becomes an artifact creature with power and toughness each equal to its mana value.
   * -2: You may reveal an artifact card you own from outside the game or choose a face-up artifact card you own in exile. Put that card into your hand. */

  enum
  {
    CHOICE_ANIMATE = 1,
    CHOICE_WISH = 2
  };

  card_instance_t *instance;
  int result;
  card_instance_t *legacy;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  instance = in_play(player, card);
  if ((instance != NULL) && (result = is_humiliated(player, card, AL_STATIC), result == 0))
  {
    default_test_definition(&test, 0x40);
    remove_abilities_by_test(1 - player, event, &test, AL_ALL_ACT);
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x40);
    td.preferred_controller = player;
    td.illegal_type = TYPE_CREATURE;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Animate an artifact",
                      1, 5, 1, "Wish for an artifact", 1, 10, 0xfffffffe, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if ((result == CHOICE_ANIMATE) && (result = can_target(&td), result != 0))
      {
        new_pick_target(&td, "Select target noncreature artifact.", 0, 0x80);
      }
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    default_target_definition(player, card, &td, 0x40);
    td.preferred_controller = player;
    td.illegal_type = TYPE_CREATURE;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    if (instance->info_slot == CHOICE_ANIMATE)
    {

      result = get_cmc(instance->targets[0].player, instance->targets[0].card);
      result = turn_into_creature(instance->parent_controller, instance->parent_card,
                                  instance->targets[0].player, instance->targets[0].card, 8, result,
                                  result);
      legacy = get_card_instance(player, result);
      legacy->targets[4].player = instance->parent_controller;
    }
    else if (instance->info_slot == CHOICE_WISH)
    {

      result = count_rfg_by_type(player, 0x40);
      if (result != 0)
      {
        default_test_definition(&test, 0x40);
        result = new_global_tutor(player, player, 3, 1, 0, 1, &test);
        if (-1 < result)
          return planeswalker(player, card, event, 5);
      }
      default_test_definition(&test, 0x40);
      generic_wish_effect(player, card, &test, 0x1ac2);
    }
  }
  return planeswalker(player, card, event, 5);
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 native scalar-word handlers. */
int card_narset_parter_of_veils(int player, int card, event_t event);
int card_teyo_the_shieldmage(int player, int card, event_t event);

/* ManalinkEh 2022: 0x21d0180; original cards/war_of_the_spark.c:3311. */
int card_kasminas_transmutation(int player, int card, event_t event)
{
  /* Kasmina's Transmutation	|1|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature loses all abilities and has base power and toughness 1/1. */

  int result;

  attached_set_pt(player, card, event, 1, 1);
  disable_attached(player, card, event, 0x7e);
  result = aura(player, card, event, NULL, NULL);
  disabling_aura_ai(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x21d1ff0; original cards/war_of_the_spark.c:4246. */
int card_narset_parter_of_veils(int player, int card, event_t event)
{
  /* Narset, Parter of Veils	|1|U|U
   * Legendary Planeswalker - Narset (5)
   * Each opponent can't draw more than one card each turn.
   * -2: Look at the top four cards of your library. You may reveal a noncreature, nonland card from among them and put it into your hand. Put the rest on the bottom of your library in a random order. */

  int result;
  card_instance_t *instance;
  test_definition_t test;

  check_legend_rule(player, card, event);
  result = -player + 1;
  if ((((cards_drawn_this_turn[result] != 0) && (trigger_condition == TRIGGER_REPLACE_CARD_DRAW)) &&
       (card == affected_card)) &&
      ((player == affected_card_controller && (result == reason_for_trigger_controller))))
  {
    suppress_draw = 1;
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    new_default_test_definition(&test, 3, "Select a nonland, noncreature card.");
    test.type_flag = 1;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = count_counters(player, card, COUNTER_LOYALTY);
      if (result < 2)
      {
        return 0;
      }
      return planeswalker(player, card, event, 5);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL,
                           DLG_NO_STORAGE, DLG_PLANESWALKER, "Look at top 4", 1, 5, 0xfffffffe, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    new_default_test_definition(&test, 3, "Select a nonland, noncreature card.");
    test.type_flag = 1;
    get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    reveal_top_cards_of_library_and_choose(player, card, player, 4, 0, TUTOR_HAND, 1,
                                                TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test);
  }
  return planeswalker(player, card, event, 5);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21d7080; original cards/war_of_the_spark.c:6163. */
int card_teyo_the_shieldmage(int player, int card, event_t event)
{
  /* Teyo, the Shieldmage	|2|W
   * Legendary Planeswalker - Teyo (5)
   * You have hexproof. (You can't be the target of spells or abilities your opponents control.)
   * -2: Create a 0/3 white Wall creature token with defender. */

  card_instance_t *instance;
  int result;
  token_generation_t token;

  check_legend_rule(player, card, event);
  instance = in_play(player, card);
  if ((instance != NULL) &&
      (result = is_humiliated(player, card, AL_STATIC), result == 0))
  {
    give_hexproof_to_player(player, card, event);
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = count_counters(player, card, COUNTER_LOYALTY);
      if (result < 2)
      {
        return 0;
      }
      return planeswalker(player, card, event, 5);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_CANCEL,
                           DLG_NO_STORAGE, DLG_PLANESWALKER, "Create a wall", 1, 5, 0xfffffffe, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    default_token_definition(player, card, CARD_ID_WALL, &token);
    token.pow = 0;
    token.tou = 3;
    token.color_forced = ' ';
    token.key_plus = KEYWORD_DEFENDER;
    generate_token(&token);
  }
  return planeswalker(player, card, event, 5);
}

#endif

#ifndef SHANDALAR
/* October 2022 private event context at 0x2774010. */
static event_t hack_current_event;
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21c7b60; original cards/war_of_the_spark.c:1698. */
static int vivien_enable_flash_land(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  if ((1U < (unsigned int)event - (unsigned int)EVENT_RESOLVE_ACTIVATION) &&
      (event != EVENT_ACTIVATE))
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  base_target_definition(player, card, &td, instance->targets[2].player);
  td.illegal_type = instance->targets[2].card;
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.zone = TARGET_ZONE_HAND;
  result = can_play_cards_as_though_they_had_flash(
      player, card, event, &td, (char *)instance->targets[3].player, instance->targets[3].card);
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21cc160; original cards/war_of_the_spark.c:1721. */
int card_emergence_zone(int player, int card, event_t event)
{
  /* Emergence Zone	""
   * Land
   * |T: Add |C.
   * |1, |T, Sacrifice this land: You may cast spells this turn as though they had flash. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_FLASH = 2
  };

  int mode_or_result;
  int can_flash;
  int tap_check;
  card_instance_t *instance;
  unsigned int can_mana_or_result;
  bool not_can_activate;
  bool not_activate;
  target_definition_t td;
  not_can_activate = event != EVENT_CAN_ACTIVATE;
  not_activate = event != EVENT_ACTIVATE;
  if ((event != EVENT_RESOLVE_ACTIVATION) && (not_activate && not_can_activate))
  {
    return mana_producer(player, card, event);
  }
  base_target_definition(player, card, &td, 0xff);
  td.illegal_type = TYPE_LAND;
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.zone = TARGET_ZONE_HAND;
  if (paying_mana() == 0)
  {
    mode_or_result = can_target(&td);
    can_flash = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9, 3, 0, 0, 0, 0, 0, 0,
                                          NULL, NULL);
    tap_check = is_tapped(player, card);
    if ((tap_check == 0) && (tap_check = is_animated_and_sick(player, card), tap_check == 0))
    {
      tap_check = can_produce_mana(player, card);
      can_mana_or_result = (unsigned int)(tap_check != 0);
    }
    else
    {
      can_mana_or_result = 0;
    }
    mode_or_result =
        dialog_impl(player, card, event, "Add 1", can_mana_or_result, 0xffffffff, "Flash",
                    can_flash, -(unsigned int)(mode_or_result == 0) | 1, 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return mode_or_result;
    }
    if (not_activate && not_can_activate)
    {
      if (mode_or_result != CHOICE_FLASH)
      {
        return 0;
      }
      mode_or_result = create_legacy_activate(player, card, vivien_enable_flash_land);
      if (mode_or_result == -1)
      {
        return 0;
      }
      instance = get_card_instance(player, mode_or_result);
      instance->targets[2].card = 1;
      instance->targets[3].player = (int32_t)"SELECT_CARD";
      instance->targets[3].card = 0;
      if (stack_size < 1)
      {
        return 0;
      }
      allow_response_to_activation(player, card);
      return 0;
    }
    if (mode_or_result != CHOICE_MANA)
    {
      if (mode_or_result != CHOICE_FLASH)
      {
        return 0;
      }
      add_state(player, card, 0x10);
      generic_activated_ability(player, card, event, 8, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        remove_state(player, card, 0x10);
      }
      tapped_for_mana_color = -1;
      return 0;
    }
  }
  else
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      mode_or_result = is_tapped(player, card);
      if ((mode_or_result == 0) &&
          (mode_or_result = is_animated_and_sick(player, card), mode_or_result == 0))
      {
        mode_or_result = can_produce_mana(player, card);
        can_mana_or_result = (unsigned int)(mode_or_result != 0);
      }
      else
      {
        can_mana_or_result = 0;
      }
      return can_mana_or_result;
    }
    if (not_activate && not_can_activate)
    {
      return 0;
    }
  }
  mana_producer(player, card, event);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21cd780; original cards/war_of_the_spark.c:2149. */
int card_firemind_vessel(int player, int card, event_t event)
{
  /* Firemind Vessel	|4
   * Artifact
   * This artifact enters tapped.
   * |T: Add two mana of different colors. */

  int result;
  result = is_humiliated(player, card, AL_TRIG);
  if (result == 0)
  {
    comes_into_play_tapped(player, card, event);
  }
  result = artifact_mana_all_different_colors(player, card, event, 2, 0);
  return result;
}

#endif

#ifndef SHANDALAR
static int has_mana_to_cast_iid_minus_two(int player, event_t event, int iid)
{
  uint32_t id = (uint16_t)cards_data[iid].id;
  int available;
  if (id == 0x25ad)
    available = can_cast_reaper_king(player, -1, event);
  else
  {
    card_ptr_t *cp = cards_ptr[id];
    uint32_t original_cless = cp->req.req_colorless;
    if (original_cless == 0x28)
      original_cless = 0;
    int cless =
        get_updated_casting_cost(player, -1, iid, event, original_cless) - 2;
    if (cless < 0)
      cless = 0;
    if (id == 0x2ded)
    {
      cless -= count_graveyard_by_type(player, 2);
      if (cless < 0)
        cless = 0;
    }
    uint32_t artifact = cards_data[iid].type & 0x40;
    if (is_phyrexian(id))
      available = has_phyrexian_mana(player, cless, cp->req.req_black, cp->req.req_blue,
                                          cp->req.req_green, cp->req.req_red, cp->req.req_white, artifact);
    else if (is_normal_hybrid(cp))
    {
      int c1 = get_colors_for_hybrid(-1, iid, 0, 0);
      int c2 = get_colors_for_hybrid(-1, iid, c1, 0);
      int number = get_number_of_hybrid_mana(-1, iid, 0, 0);
      available = has_mana_hybrid(player, number, c1, c2, cless);
    }
    else if (is_monocolor_hybrid(cp))
    {
      int c1 = get_color_for_monocolored_hybrid(-1, iid);
      int number = get_number_of_monocolored_hybrid_mana(-1, iid, 0);
      available = has_mana_for_monocolor_hybrid(player, number, c1, 0);
    }
    else if (artifact)
      available = has_mana_multi_a(player, cless, cp->req.req_black, cp->req.req_blue, cp->req.req_green,
                                   cp->req.req_red, cp->req.req_white);
    else
      available = has_mana_multi(player, cless, cp->req.req_black, cp->req.req_blue, cp->req.req_green,
                                 cp->req.req_red, cp->req.req_white);
  }
  return available;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21ce5c0; original cards/war_of_the_spark.c:2522. */
int card_god_eternal_kefnet(int player, int card, event_t event)
{
  /* God-Eternal Kefnet	|2|U|U
   * Legendary Creature - Zombie God 4/5
   * Flying
   * You may reveal the first card you draw each turn as you draw it. Whenever you reveal an instant or sorcery card this way, copy that card and you may cast the copy. That copy costs |2 less to cast.
   * When ~ dies or is put into exile from the battlefield, you may put it into its owner's library third from the top. */

  check_legend_rule(player, card, event);
  if (trigger_condition == TRIGGER_REPLACE_CARD_DRAW && cards_drawn_this_turn[player] == 0 && affect_me(player, card) &&
      reason_for_trigger_controller == player && trigger_cause_controller == player &&
      suppress_draw == 0 && deck_ptr[player][0] != -1 &&
      !is_humiliated(player, card, AL_TRIG))
  {
    if (event == EVENT_TRIGGER)
      event_result |= (duh_mode(player) != 0) + 1;
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      show_deck(current_turn, deck_ptr[player], 1, "God-Eternal Kefnet reveals:", 0,
                     gs_done_008b40e0);
      show_deck(current_turn ^ 1, deck_ptr[player], 1, "God-Eternal Kefnet reveals:", 0,
                     gs_done_008b40e0);
      int *deck = deck_ptr[player];
      card_instance_t *instance = get_card_instance(player, card);
      instance->info_slot = *deck;
    }
  }
  if (trigger_condition == TRIGGER_CARD_DRAWN && cards_drawn_this_turn[player] == 1 &&
      (cards_data[get_card_instance(player, card)->info_slot].type & 0x38) != 0 &&
      (cards_data[get_card_instance(player, card)->info_slot].type & 2) == 0 &&
      affect_me(player, card) && reason_for_trigger_controller == player &&
      trigger_cause_controller == player && suppress_draw == 0 &&
      !is_humiliated(player, card, AL_TRIG))
  {
    if (event == EVENT_TRIGGER)
      event_result |= 2;
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      int iid = get_card_instance(player, card)->info_slot;
      uint32_t id = (uint16_t)cards_data[iid].id;
      int available;
      if (id == 0x25ad)
        available = can_cast_reaper_king(player, -1, EVENT_RESOLVE_TRIGGER);
      else
      {
        card_ptr_t *cp = cards_ptr[id];
        uint32_t original_cless = cp->req.req_colorless;
        if (original_cless == 0x28)
          original_cless = 0;
        int cless =
            get_updated_casting_cost(player, -1, iid, EVENT_RESOLVE_TRIGGER, original_cless) - 2;
        if (cless < 0)
          cless = 0;
        if (id == 0x2ded)
        {
          cless -= count_graveyard_by_type(player, 2);
          if (cless < 0)
            cless = 0;
        }
        uint32_t artifact = cards_data[iid].type & 0x40;
        if (is_phyrexian(id))
          available = has_phyrexian_mana(player, cless, cp->req.req_black, cp->req.req_blue,
                                              cp->req.req_green, cp->req.req_red, cp->req.req_white, artifact);
        else if (is_normal_hybrid(cp))
        {
          int c1 = get_colors_for_hybrid(-1, iid, 0, 0);
          int c2 = get_colors_for_hybrid(-1, iid, c1, 0);
          int number = get_number_of_hybrid_mana(-1, iid, 0, 0);
          available = has_mana_hybrid(player, number, c1, c2, cless);
        }
        else if (is_monocolor_hybrid(cp))
        {
          int c1 = get_color_for_monocolored_hybrid(-1, iid);
          int number = get_number_of_monocolored_hybrid_mana(-1, iid, 0);
          available = has_mana_for_monocolor_hybrid(player, number, c1, 0);
        }
        else if (artifact)
          available = has_mana_multi_a(player, cless, cp->req.req_black, cp->req.req_blue, cp->req.req_green,
                                       cp->req.req_red, cp->req.req_white);
        else
          available = has_mana_multi(player, cless, cp->req.req_black, cp->req.req_blue, cp->req.req_green,
                                     cp->req.req_red, cp->req.req_white);
      }
      if (available && dialog_impl(player, card, EVENT_RESOLVE_ACTIVATION, DLG_RANDOM, DLG_NO_STORAGE,
                                   DLG_SMALLCARD_ID(iid), "Copy top card", 1, 10, 0, 0, 0, 0) == 1)
      {
        if (charge_mana_from_id_minus_two(player, -1, EVENT_RESOLVE_TRIGGER,
                                          (uint16_t)cards_data[iid].id))
          copy_spell(player, (uint16_t)cards_data[iid].id);
      }
      get_card_instance(player, card)->info_slot = 0;
    }
    else if (event == EVENT_CLEANUP)
      get_card_instance(player, card)->info_slot = 0;
  }
  else if (event == EVENT_CLEANUP)
    get_card_instance(player, card)->info_slot = 0;
  return war_god_eternal(player, card, event);
}

#endif

#ifndef SHANDALAR
static int is_this_dying_or_being_exiled(int player, int card)
{
  return in_play(player, card) &&
         (unsigned char)(get_card_instance(player, card)->kill_code - 1) <= 3;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21d25c0; original cards/war_of_the_spark.c:4372. */
int card_nicol_bolas_dragon_god(int player, int card, event_t event)
{
  /* Nicol Bolas, Dragon-God	|U|B|B|B|R
   * Legendary Planeswalker - Bolas (4)
   * Nicol Bolas has all loyalty abilities of all other planeswalkers on the battlefield.
   * +1: You draw a card. Each opponent exiles a card from their hand or a permanent they control.
   * -3: Destroy target creature or planeswalker.
   * -8: Each opponent who doesn't control a legendary creature or planeswalker loses the game. */

  enum
  {
    CHOICE_IMITATE = 1,
    CHOICE_DRAW_EXILE = 2,
    CHOICE_DESTROY = 3,
    CHOICE_ULTIMATE = 4
  };

  check_legend_rule(player, card, event);
  if (event == EVENT_CAN_ACTIVATE)
  {
    for (int p = 0; p != 2; ++p)
    {
      for (int c = 0; c < active_cards_count[p]; ++c)
      {
        if (in_play(p, c) && is_what(p, c, 0x100) && get_id(p, c) != CARD_ID_NICOL_BOLAS_DRAGON_GOD)
        {
          int iid = get_card_instance(p, c)->internal_card_id;
          if (cards_data[iid].code_pointer(player, card, EVENT_CAN_ACTIVATE) == 99)
            return 99;
        }
      }
    }
    return planeswalker(player, card, EVENT_CAN_ACTIVATE, 4);
  }
  if (event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION || event == EVENT_CLEANUP)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x102);
    card_instance_t *instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = 0;
      instance->info_slot = 0;
      player_bits[0] |= 0x40000;
      player_bits[1] |= 0x40000;
      int ab1 = 0;
      for (int p = 0; p != 2; ++p)
      {
        for (int c = 0; c < active_cards_count[p]; ++c)
        {
          if (in_play(p, c) && is_planeswalker(p, c) && get_id(p, c) != CARD_ID_NICOL_BOLAS_DRAGON_GOD)
          {
            int iid = get_card_instance(p, c)->internal_card_id;
            int act = cards_data[iid].code_pointer(player, card, EVENT_CAN_ACTIVATE);
            if (act != 0)
            {
              ab1 = act;
              break;
            }
          }
        }
      }
      player_bits[0] &= 0xfffbffff;
      player_bits[1] &= 0xfffbffff;
      int destroy_available = can_target(&td);
      int choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, DLG_NO_STORAGE,
                               "Use another pw's ability", ab1, 10, 0, "Draw and opp chooses", 1, 5,
                               1, "Destroy", destroy_available, 10, -3, "Opp needs a hero", 1, 20,
                               -8, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_IMITATE)
      {
        instance->number_of_targets = 0;
        uint32_t oldpb0 = player_bits[0], oldpb1 = player_bits[1];
        int activable[14] = {0};
        player_bits[0] |= 0x40000;
        player_bits[1] |= 0x40000;
        int ac = 0;
        for (int p = 0; p != 2; ++p)
        {
          for (int c = 0; c < active_cards_count[p]; ++c)
          {
            if (ac > 14)
              break;
            if (in_play(p, c) && is_planeswalker(p, c) && get_id(p, c) != CARD_ID_NICOL_BOLAS_DRAGON_GOD)
            {
              int iid = get_card_instance(p, c)->internal_card_id;
              if (cards_data[iid].code_pointer(player, card, EVENT_CAN_ACTIVATE))
                activable[ac++] = iid;
            }
          }
        }
        if (ac == 0)
        {
          spell_fizzled = 1;
          player_bits[0] = oldpb0;
          player_bits[1] = oldpb1;
          return 0;
        }
        player_bits[0] = oldpb0;
        player_bits[1] = oldpb1;
        const char *names[14];
        for (int i = 13; i >= 0; --i)
          names[i] = get_card_name_by_id((uint16_t)cards_data[activable[i]].id);
        int choice2 = dialog_impl(
            player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL, DLG_NO_DISPLAY_FOR_AI, names[0],
            activable[0], 1, names[1], activable[1], 1, names[2], activable[2], 1, names[3],
            activable[3], 1, names[4], activable[4], 1, names[5], activable[5], 1, names[6],
            activable[6], 1, names[7], activable[7], 1, names[8], activable[8], 1, names[9],
            activable[9], 1, names[10], activable[10], 1, names[11], activable[11], 1, names[12],
            activable[12], 1, names[13], activable[13], 1, 0, 0, 0, 0);
        if (choice2 == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        uint32_t iid = activable[choice2 - 1];
        cards_data[iid].code_pointer(player, card, EVENT_ACTIVATE);
        if (spell_fizzled != 1)
        {
          get_card_instance(player, card)->eot_toughness = iid;
          ++instance->targets[9].card;
        }
      }
      else if (choice == CHOICE_DESTROY)
        new_pick_target(&td, "TARGET_CREATURE_OR_PLANESWALKER", 0, 1);
      instance->unk52 = (uint8_t)choice;
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      unsigned choice = instance->unk52;
      if (choice == CHOICE_IMITATE)
      {
        uint32_t iid = get_card_instance(player, card)->eot_toughness;
        cards_data[iid].code_pointer(player, card, EVENT_RESOLVE_ACTIVATION);
      }
      else if (choice == CHOICE_DRAW_EXILE)
      {
        int opponent = 1 - player;
        draw_a_card(player);
        unsigned nothing_available = 0;
        if (hand_count[opponent] == 0)
          nothing_available =
              can_sacrifice(player, opponent, 1, TYPE_PERMANENT, SUBTYPE_INVALID) == 0;
        int sacrifice_available =
            can_sacrifice(player, opponent, 1, TYPE_PERMANENT, SUBTYPE_INVALID);
        int choice2 =
            dialog_impl(player, card, EVENT_RESOLVE_ACTIVATION, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_WHO_CHOOSES(
                        opponent), DLG_NO_DISPLAY_FOR_AI, DLG_AUTOCHOOSE_IF_1, DLG_OMIT_ILLEGAL, "Discard a card",
                        hand_count[opponent], 10, "Sac a permanent", sacrifice_available, 5,
                        "Do nothing", nothing_available, 0, 0, 0, 0, 0);
        if (choice2 == 1)
          discard(opponent, 0, player);
        else if (choice2 == 2)
          impose_sacrifice(player, card, opponent, 1, 0x47, 0, 0, 0, 0, 0, 0, 0, -1, 0);
      }
      else if (choice == CHOICE_DESTROY)
      {
        if (valid_target(&td))
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
      else if (choice == CHOICE_ULTIMATE)
      {
        int opponent = 1 - player;
        for (int c = active_cards_count[opponent] - 1; c >= 0; --c)
        {
          if (in_play(opponent, c) && (is_what(opponent, c, 0x100) ||
                                       (is_what(opponent, c, 2) && is_legendary(opponent, c))))
            return planeswalker(player, card, event, 4);
        }
        lose_the_game(opponent);
      }
    }
  }
  return planeswalker(player, card, event, 4);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21d3830; original cards/war_of_the_spark.c:4709. */
int card_niv_mizzet_reborn(int player, int card, event_t event)
{
  /* Niv-Mizzet Reborn	|W|U|B|R|G
   * Legendary Creature - Dragon Avatar 6/6
   * Flying
   * When Niv-Mizzet enters, reveal the top ten cards of your library. For each color pair, choose a card that's exactly those colors from among them. Put the chosen cards into your hand and the rest on the bottom of your library in a random order. */

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    int num_cards = count_deck(player);
    if (num_cards != 0)
    {
      if (num_cards > 10)
        num_cards = 10;
      show_deck(current_turn, deck_ptr[player], num_cards,
                     "Cards revealed by Niv-Mizzet Reborn", 0, gs_done_008b40e0);
      show_deck(current_turn ^ 1, deck_ptr[player], num_cards,
                     "Cards revealed by Niv-Mizzet Reborn", 0, gs_done_008b40e0);
      int deck_copy[num_cards];
      for (int i = 0; i < num_cards; ++i)
      {
        deck_copy[i] = deck_ptr[player][0];
        remove_card_from_deck(player, 0);
      }
      char msg[100];
      const char *prompt = "Select a card from a Guild you haven't chosen";
      scnprintf(msg, 100, prompt, num_cards);
      test_definition_t test;
      new_default_test_definition(&test, TYPE_ANY, msg);
      int chosen = 0;
      int remaining = num_cards;
      do
      {
        test.special_selection_function = not_selected;
        test.value_for_special_selection_function = chosen;
        int selected =
            select_card_from_zone(player, player, deck_copy, remaining, 0, 1, -1, &test);
        if (selected == -1)
        {
          if (remaining == 0)
            return 0;
          for (int i = 0; i < remaining; ++i)
            raw_put_iid_on_top_of_deck(player, deck_copy[i]);
          put_top_x_on_bottom_in_random_order(player, remaining);
          return 0;
        }
        --remaining;
        add_card_to_hand(player, deck_copy[selected]);
        chosen |= colors_to_guilds(deck_copy[selected]);
        for (int i = selected; i < remaining; ++i)
          deck_copy[i] = deck_copy[i + 1];
      } while (remaining != 0);
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21d4080; original cards/war_of_the_spark.c:4902. */
int card_planewide_celebration(int player, int card, event_t event)
{
  /* Planewide Celebration	|5|G|G
   * Sorcery
   * Choose four. You may choose the same mode more than once.
   * * Create a 2/2 Citizen creature token that's all colors.
   * * Return target permanent card from your graveyard to your hand.
   * * Proliferate.
   * * You gain 4 life. */

  enum
  {
    CHOICE_TOKEN = 1,
    CHOICE_RETURN = 2,
    CHOICE_PROLIFERATE = 3,
    CHOICE_GAIN_LIFE = 4
  };

  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  test_definition_t test;
  new_default_test_definition(&test, 0x47, "Select a card to return to your hand.");
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAST_SPELL)
  {
    if (card == affected_card && player == affected_card_controller)
    {
      instance->number_of_targets = 0;
      int available = count_graveyard_by_type(player, 0x47);
      if (!check_special_flags2(player, card, -0x80000000))
      {
        int choices[4] = {0};
        instance->info_slot = 0;
        int grave_targets = 0;
        for (int index = 0; index < 4; ++index)
        {
          int life_priority = 0x14 - life[player];
          if (choices[0] == 4)
            life_priority = 0xf - life[player];
          if (choices[1] == 4)
            life_priority -= 5;
          if (choices[2] == 4)
            life_priority -= 5;
          int can_return = generic_spell(player, card, EVENT_CAN_CAST, 8, NULL, NULL, 1, &test);
          int choice =
              dialog_impl(player, card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_RANDOM,
                          "Generate a Citizen", 1, 5, "Return a permanent card from grave to hand",
                          (unsigned int)(can_return != 0 && available != 0), 10, "Proliferate", 1,
                          10, "Gain 4 life", 1, life_priority, 0, 0, 0, 0);
          if (!choice)
          {
            spell_fizzled = 1;
            return 0;
          }
          choices[index] = choice;
          if (choice == CHOICE_RETURN)
          {
            --available;
            ++grave_targets;
          }
        }
        unsigned char *modes = (unsigned char *)&instance->info_slot;
        for (int index = 0; index < 4; ++index)
          modes[index] += (char)choices[index];
        if (grave_targets)
          select_multiple_cards_from_graveyard(player, player, 1, 1, &test, grave_targets,
                                               instance->targets);
      }
    }
    return basic_spell(player, card, event);
  }
  if (event != EVENT_RESOLVE_SPELL)
    return basic_spell(player, card, event);
  unsigned int modes = instance->info_slot;
  int grave_target = 0;
  for (int index = 0; index < 4; ++index)
    if (((modes >> (index * 8)) & 0xff) == 2)
    {
      if (validate_target_from_grave(player, card, player, grave_target) == -1)
      {
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
      ++grave_target;
      modes = instance->info_slot;
    }
  for (int index = 0; index < 4; ++index)
    if (((modes >> (index * 8)) & 0xff) == 1)
    {
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_CITIZEN, &token);
      token.pow = 2;
      token.tou = 2;
      token.color_forced = 62;
      token.no_sleight = 1;
      generate_token(&token);
      modes = instance->info_slot;
    }
  grave_target = 0;
  for (int index = 0; index < 4; ++index)
    if (((modes >> (index * 8)) & 0xff) == 2)
    {
      int grave_index = validate_target_from_grave(player, card, player, grave_target);
      if (grave_index != -1)
        from_grave_to_hand(player, grave_index, 1);
      ++grave_target;
      modes = instance->info_slot;
    }
  for (int index = 0; index < 4; ++index)
    if (((modes >> (index * 8)) & 0xff) == 3)
    {
      proliferate(player, card);
      modes = instance->info_slot;
    }
  for (int index = 0; index < 4; ++index)
  {
    if (index)
      modes = instance->info_slot;
    if (((modes >> (index * 8)) & 0xff) == 4)
      gain_life(player, 4);
  }
  kill_card(player, card, KILL_DESTROY);
  return basic_spell(player, card, event);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21d4b10; original cards/war_of_the_spark.c:5205. */
int count_type_in_hand(int player, type_t type)
{
  int count = 0;
  for (int i = 0; i < active_cards_count[player]; ++i)
    if (in_hand(player, i) && is_what(player, i, type))
      ++count;
  return count;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21d4b70; original cards/war_of_the_spark.c:5213. */
int card_ral_storm_conduit(int player, int card, event_t event)
{
  /* Ral, Storm Conduit	|2|U|R
   * Legendary Planeswalker - Ral (4)
   * Whenever you cast or copy an instant or sorcery spell, Ral deals 1 damage to target opponent or planeswalker.
   * +2: Scry 1.
   * -2: When you next cast an instant or sorcery spell this turn, copy that spell. You may choose new targets for the copy. */

  enum { CHOICE_SCRY = 1, CHOICE_COPY = 2 };
  check_legend_rule(player, card, event);
  magecraft_trigger(player, event, 2, TYPE_ANY, NULL, player, card);
  if (event == EVENT_SPA)
  {
    target_definition_t td;
    opponent_or_pw_target_definition(player, card, &td);
    if (can_target(&td) && new_pick_target(&td, "TARGET_OPPONENT_OR_PLANESWALKER", 0, 1))
      damage_target0(player, card, 1);
    get_card_instance(player, card)->number_of_targets = 0;
  }
  else if (IS_GAA_EVENT(event))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = 0;
      instance->info_slot = 0;
      int spells_in_hand = count_type_in_hand(player, TYPE_SPELL);
      int choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                               "Scry 1", 1, 5, 2,
                               "Copy your next spell", 1, spells_in_hand * 10, 0xfffffffe,
                               0, 0, 0, 0);
      if (!choice)
      {
        spell_fizzled = 1;
        return 0;
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (instance->info_slot == CHOICE_SCRY)
        scry(player, 1);
      else if (instance->info_slot == CHOICE_COPY)
        create_legacy_effect(player, card, doublecast_legacy);
    }
  }
  return planeswalker(player, card, event, 4);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21d6820; original cards/war_of_the_spark.c:5943. */
int card_tamiyo_collector_of_tales(int player, int card, event_t event)
{
  /* Tamiyo, Collector of Tales	|2|G|U
   * Legendary Planeswalker - Tamiyo (5)
   * Spells and abilities your opponents control can't cause you to discard cards or sacrifice permanents.
   * +1: Choose a nonland card name, then reveal the top four cards of your library. Put all cards with the chosen name from among them into your hand and the rest into your graveyard.
   * -3: Return target card from your graveyard to your hand. */

  enum
  {
    CHOICE_DIG = 1,
    CHOICE_REGROWTH = 2
  };

  check_legend_rule(player, card, event);
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION ||
      event == EVENT_CLEANUP)
  {
    test_definition_t test;
    default_test_definition(&test, 0xff);
    test.ai_selection_mode = 1;
    card_instance_t *instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = 0;
      instance->info_slot = 0;
      int choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Reveal top 4",
                               1, 5, 1, "Return a card from your graveyard to your hand",
                               get_grave(player)[0] != -1, 10, -3, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_REGROWTH)
        generic_activated_ability_with_target_in_grave(player, card, EVENT_ACTIVATE, GAA_NONE,
                                                            0, 0, 0, 0, 0, 0, 0, NULL, NULL,
                                                            TYPE_NONE, &test, 8);
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (instance->info_slot == CHOICE_REGROWTH)
      {
        int selected = validate_target_from_grave_source(player, card, player, 0);
        if (selected != -1)
          from_grave_to_hand(player, selected, 1);
      }
      else if (instance->info_slot == CHOICE_DIG)
      {
        int on_top = deck_ptr[player][0];
        int chosen;
        if (ai_is_speculating == 1 || (player == 1 && (trace_mode & 2) == 0))
        {
          chosen = get_internal_card_id_from_csv_id(CARD_ID_LABORATORY_MANIAC);
          if (on_top != -1)
          {
            target_t counts[501];
            for (int i = 0; i != 501; ++i)
            {
              counts[i].player = -1;
              counts[i].card = -1;
            }
            int highest_count = 1;
            for (int i = 0; deck_ptr[player][i] != -1; ++i)
            {
              if (!is_what(-1, deck_ptr[player][i], 1))
              {
                int iid = deck_ptr[player][i];
                int j = 0;
                while (counts[j].card != iid && counts[j].card != -1)
                  ++j;
                if (counts[j].card == -1)
                {
                  counts[j].card = iid;
                  counts[j].player = 1;
                }
                else
                {
                  ++counts[j].player;
                  if (counts[j].player > highest_count)
                    highest_count = counts[j].player;
                }
              }
            }
            for (int i = 0; i != 501; ++i)
              if (counts[i].player == highest_count)
              {
                chosen = counts[i].card;
                break;
              }
          }
        }
        else
        {
          test_definition_t test2;
          new_default_test_definition(&test2, 1, "");
          test2.type_flag = 1;
          int csvid = choose_a_card_test_ct("Choose a nonland card name.", &test, (color_test_t)-1,
                                            (type_t)~TYPE_LAND);
          chosen = get_internal_card_id_from_csv_id(csvid);
        }
        if (player == 1 || (trace_mode & 2) != 0)
          look_at_iid(player, card, chosen, "chooses:");
        int qty = count_deck(player);
        if (qty != 0)
        {
          int amount = qty > 4 ? 4 : qty;
          show_deck(current_turn, deck_ptr[player], amount,
                         "Cards revealed by Tamiyo, Teller of Tales", 0, gs_done_008b40e0);
          show_deck(current_turn ^ 1, deck_ptr[player], amount,
                         "Cards revealed by Tamiyo, Teller of Tales", 0, gs_done_008b40e0);
          int remaining = amount;
          if (qty > 0)
          {
            for (int i = 0; i < amount; ++i)
            {
              if (deck_ptr[player][i] == chosen)
              {
                add_card_to_hand(player, chosen);
                remove_card_from_deck(player, i);
                --remaining;
              }
            }
          }
          if (remaining != 0)
            mill(player, remaining);
        }
      }
    }
  }
  return planeswalker(player, card, event, 5);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21c7a50; original cards/war_of_the_spark.c:6853. */
static char *vivien_has_mana_to_cast_target(int who_chooses, int player, int card,
                                            int targeting_player, int targeting_card)
{
  int result;
  char *error;
  result = get_id(player, card);
  result = has_mana_to_cast_id(who_chooses, hack_current_event, result);
  error = NULL;
  if (result == 0)
  {
    error = "not enough mana";
  }
  return error;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline helpers; existing native callers contain the expansions. */
/* ManalinkEh 2022: 0x21c78a0; original cards/war_of_the_spark.c:6883. */
static int vivien_flash_ability(int player, int card, event_t event)
{
  hack_current_event = event;
  target_definition_t td;
  base_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.zone = TARGET_ZONE_HAND;
  bool ai = ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0);
  if (td.extra == -1 && (ai || event == EVENT_CAN_ACTIVATE))
  {
    td.extra = (intptr_t)vivien_has_mana_to_cast_target;
    td.special |= TARGET_SPECIAL_EXTRA_FUNCTION;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (is_humiliated(instance->damage_target_player, instance->damage_target_card, AL_STATIC))
      return 0;
    return can_target(&td);
  }
  if (event != EVENT_ACTIVATE)
    return 0;
  instance->number_of_targets = 0;
  if (pick_next_target_noload(&td, "Select a creature card") &&
      charge_mana_from_id(player, instance->targets[0].card, EVENT_ACTIVATE, -1))
  {
    int hand_card = instance->targets[0].card;
    instance->number_of_targets = 0;
    play_card_in_hand_for_free(instance->targets[0].player, hand_card);
    cant_be_responded_to = 1;
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 private inline definitions. See per-helper limits in the recovery evidence. */
/* ManalinkEh 2022: 0x21d8c90; original cards/war_of_the_spark.c:6915. */
int card_vivien_champion_of_the_wilds(int player, int card, event_t event)
{
  /* Vivien, Champion of the Wilds	|2|G
   * Legendary Planeswalker - Vivien (4)
   * You may cast creature spells as though they had flash.
   * +1: Until your next turn, up to one target creature gains reach and vigilance.
   * -2: Look at the top three cards of your library. Exile one face down and put the rest on the bottom of your library in any order. For as long as it remains exiled, you may cast it if it's a creature spell. */

  enum
  {
    CHOICE_PUMP = 1,
    CHOICE_DIG = 2
  };

  int choice;
  int iid;
  card_instance_t *instance;
  int selected;
  int amount;
  target_definition_t *creature_target;
  target_definition_t td_creature;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {
    alternate_legacy_text(
        1, player,
        create_targetted_legacy_activate(player, card, vivien_flash_ability, player, card));
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_target_definition(player, card, &td_creature, 2);
      td_creature.preferred_controller = player;
      instance = get_card_instance(player, card);
      if (event == EVENT_ACTIVATE)
      {
        instance->number_of_targets = '\0';
        instance->info_slot = 0;
        choice =
            dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Pump a creature", 1,
                        5, 1, "Look at the top three cards", 1, 10, 0xfffffffe, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (choice == CHOICE_PUMP)
        {
          instance->number_of_targets = '\0';
          if (can_target(&td_creature) != 0)
          {
            pick_target(&td_creature, "TARGET_CREATURE");
          }
        }
        return planeswalker(player, card, event, 4);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return planeswalker(player, card, event, 4);
      default_target_definition(player, card, &td_creature, 2);
      td_creature.preferred_controller = player;
      instance = get_card_instance(player, card);
    }
    creature_target = &td_creature;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (instance->info_slot == 1)
      {
        if (valid_target(creature_target) != 0)
        {
          alternate_legacy_text(1, player,
                                create_targetted_legacy_effect(
                                    player, card, fx_vigilance_reach_until_your_next_turn,
                                    instance->targets[0].player, instance->targets[0].card));
        }
      }
      else
      {
        if ((instance->info_slot == 2) && (amount = count_deck(player), amount != 0))
        {
          new_default_test_definition(&test, 0xff, "Select a card to exile.");
          if (player == 1)
          {
            test.type = 2;
          }
          if (3 < amount)
          {
            amount = 3;
          }
          test.create_minideck = (unsigned short)amount;
          test.no_shuffle = 1;
          selected =
              new_select_a_card(player, player, 1, (unsigned int)(player == 0), 1, -1, &test);
          if (selected == -1)
          {
            new_default_test_definition(&test, 0xff, "Select a card to exile.");
            selected = new_select_a_card(player, player, 1, 1, 1, -1, &test);
          }
          iid = deck_ptr[player][selected];
          rfg_card_in_deck(player, selected);
          remove_card_from_rfg(player, (unsigned int)cards_data[iid].id);
          if ((cards_data[iid].type & 2) == 0)
          {
            selected = alternate_legacy_text(2, player,
                                             create_legacy_activate(player, card, look_at_card));
            if (selected != -1)
            {
              instance = get_card_instance(player, selected);
              instance->targets[0].player = player;
              instance->targets[0].card = iid;
            }
          }
          else
          {
            alternate_legacy_text(
                2, player,
                create_may_play_card_from_exile_effect(
                    player, card, player, (unsigned int)cards_data[iid].id, MPCFE_FACE_DOWN));
          }
          if (amount + -1 != 0)
          {
            put_top_x_on_bottom_in_random_order(player, amount + -1);
          }
        }
      }
    }
  }
  return planeswalker(player, card, event, 4);
}
#endif
