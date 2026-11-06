#include "manalink.h"

static int moldervine_reclamation_effect(int player, int card, event_t event);

static int hexpr_ub_legacy(int player, int card, event_t event);

/* ManalinkEh 2022: 0x2394930; original cards/m20.c:3. */
int card_aerial_assault(int player, int card, event_t event)
{
  /* Aerial Assault	|2|W
   * Sorcery
   * Destroy target tapped creature. You gain 1 life for each creature you control with flying. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t test;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.required_state = TARGET_STATE_TAPPED;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      new_default_test_definition(&test, 2, "");
      test.keyword = 0x20;
      result = check_battlefield_for_special_card(player, card, player, 4, &test);
      gain_life(player, result);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "ROYAL_ASSASSIN", 1, NULL);
}

/* ManalinkEh 2022: 0x2394af0; original cards/m20.c:42. */
int card_agent_of_treachery(int player, int card, event_t event)
{
  /* Agent of Treachery	|5|U|U
   * Creature - Human Rogue 2/3
   * When this creature enters, gain control of target permanent.
   * At the beginning of your end step, if you control three or more permanents you don't own, draw three cards. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_PERMANENT);
    td.preferred_controller = 1 - player;
    td.allow_cancel = 0;
    if (can_target(&td) && new_pick_target(&td, "TARGET_PERMANENT", 0, 0))
    {
      int target_card = get_card_instance(player, card)->targets[0].card;
      int target_player = get_card_instance(player, card)->targets[0].player;
      gain_control_permanently(player, target_player, target_card);
    }
  }
  if (eot_trigger(player, card, event) && current_turn == player)
  {
    int amount = 0;
    for (int count = active_cards_count[player] - 1; count >= 0 && amount < 3; --count)
    {
      if (in_play(player, count) && is_what(player, count, TYPE_PERMANENT))
        amount += is_stolen(player, count) != 0;
    }
    if (amount >= 3)
      draw_cards(player, 3);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2394c80; original cards/m20.c:95. */
int card_ajani_strength_of_the_pride(int player, int card, event_t event)
{
  /* Ajani, Strength of the Pride	|2|W|W
   * Legendary Planeswalker - Ajani (5)
   * +1: You gain life equal to the number of creatures you control plus the number of planeswalkers you control.
   * -2: Create a 2/2 white Cat Soldier creature token named Ajani's Pridemate with "Whenever you gain life, put a +1/+1 counter on this token."
   * 0: If you have at least 15 life more than your starting life total, exile Ajani and each artifact and creature your opponents control. */

  enum
  {
    CHOICE_LIFE = 1,
    CHOICE_TOKEN = 2,
    CHOICE_ULTIMATE = 3
  };

  card_instance_t *instance;
  int life_or_mode;
  int planeswalker_count;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      instance->targets[0].card = -1;
      instance->targets[0].player = -1;
      life_or_mode = get_starting_life_total(player);
      if (dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Gain life", 1, 10, 1,
                      "Pridemate token", 1, 10, 0xfffffffe, "Mega-exile", 1,
                      (unsigned int)((life_or_mode + 0xe < life[player]) ? 25 : -25), 0, 0, 0, 0,
                      0) == 0)
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

    life_or_mode = instance->info_slot;
    if (life_or_mode == CHOICE_LIFE)
    {
      life_or_mode = count_permanents_by_type(player, TYPE_CREATURE);
      planeswalker_count = count_permanents_by_type(player, TARGET_TYPE_PLANESWALKER);
      gain_life(player, planeswalker_count + life_or_mode);
    }
    else if (life_or_mode == CHOICE_TOKEN)
    {
      generate_token_by_id(player, card, CARD_ID_AJANIS_PRIDEMATE);
    }
    else if ((life_or_mode == CHOICE_ULTIMATE) &&
             (life_or_mode = get_starting_life_total(player), life_or_mode + 0xe < life[player]))
    {
      instance = get_card_instance(player, card);
      life_or_mode = instance->parent_card;
      instance = get_card_instance(player, card);
      kill_card(instance->parent_controller, life_or_mode, KILL_EXILE);
      default_test_definition(&this_test, 0x42);
      new_manipulate_all(player, card, 1 - player, &this_test, ACT_KILL_REMOVE);
    }
  }
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x2394f20; original cards/m20.c:247. */
int card_blood_for_bones(int player, int card, event_t event)
{
  /* Blood for Bones	|3|B
   * Sorcery
   * As an additional cost to cast this spell, sacrifice a creature.
   * Return a creature card from your graveyard to the battlefield, then return another creature card from your graveyard to your hand. */

  int reanimate_index;
  int iid;
  int return_index;
  unsigned int reanimated_card;
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

      new_default_test_definition(&test, 2, "Select a creature to return to the battlefield.");
      reanimate_index = new_select_a_card(player, player, 2, 0, 2, -1, &test);
      if (reanimate_index == -1)
      {
        new_default_test_definition(&test, 2, "Select a creature to return to your hand.");
        test.id = 0x388;
        test.id_flag = 1;
        return_index = new_select_a_card(player, player, 2, 0, 1, -1, &test);
        reanimated_card = 0xffffffff;
      }
      else
      {
        iid = turn_card_in_grave_face_down(player, reanimate_index);
        new_default_test_definition(&test, 2, "Select a creature to return to your hand.");
        test.id = 0x388;
        test.id_flag = 1;
        return_index = new_select_a_card(player, player, 2, 0, 1, -1, &test);
        if (iid != -1)
        {
          turn_card_in_grave_face_up(player, reanimate_index, iid);
        }
        reanimated_card =
            reanimate_permanent(player, card, player, reanimate_index, REANIMATE_DEFAULT);
      }
      if (return_index != -1)
      {
        from_grave_to_hand(player,
                           return_index - ((reanimated_card ^ 0xffffffff) >> 0x1f &
                                           (unsigned int)(reanimate_index < return_index)),
                           1);
      }
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return generic_spell(player, card, event, 0x400, NULL, NULL, 0, NULL);
}

/* ManalinkEh 2022: 0x2395180; original cards/m20.c:348. */
int card_cavalier_of_dawn(int player, int card, event_t event)
{
  /* Cavalier of Dawn	|2|W|W|W
   * Creature - Elemental Knight 4/6
   * Vigilance
   * When this creature enters, destroy up to one target nonland permanent. Its controller creates a 3/3 colorless Golem artifact creature token.
   * When this creature dies, return target artifact or enchantment card from your graveyard to your hand. */

  int result;
  card_instance_t *instance;
  token_generation_t token;


  vigilance(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x47);
    td.illegal_type = TYPE_LAND;
    td.allow_cancel = 1;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_NONLAND_PERMANENT", 0, 0);
      if (result != 0)
      {
        instance = get_card_instance(player, card);
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        default_token_definition(player, card, CARD_ID_GOLEM, &token);
        token.t_player = instance->targets[0].player;
        generate_token(&token);
      }
    }
  }
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    result = count_graveyard_by_type(player, 0x44);
    if (result != 0)
    {
      result = graveyard_has_shroud(player);
      if (result == 0)
      {
        test_definition_t this_test;
        new_default_test_definition(&this_test, 0x44,
                                    "Select an Artifact or Enchantment card.");
        new_global_tutor(player, player, 2, 1, 0, 1, &this_test);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2395360; original cards/m20.c:385. */
int card_cavalier_of_flame(int player, int card, event_t event)
{
  /* Cavalier of Flame	|2|R|R|R
   * Creature - Elemental Knight 6/5
   * |1|R: Creatures you control get +1/+0 and gain haste until end of turn.
   * When this creature enters, discard any number of cards, then draw that many cards.
   * When this creature dies, it deals X damage to each opponent and each planeswalker they control, where X is the number of land cards in your graveyard. */

  int result;
  int card_00;
  test_definition_t test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 0xff, "Select a card to discard.");
    result = 0;
    if (hand_count[player] != 0)
    {
      result = 0;
      while (card_00 = new_select_a_card(player, player, 4, 0, 3, -1, &test), -1 < card_00)
      {
        result = result + 1;
        discard_card(player, card_00);
        if (((result == 3) && ((ai_is_speculating == 1 ||
                                ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))))) ||
            (hand_count[player] == 0))
          break;
      }
    }
    draw_cards(player, result);
  }
  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    default_test_definition(&test, 0x100);
    result = count_graveyard_by_type(player, 1);
    new_damage_all(player, card, 1 - player, result, 0x22, &test);
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event != EVENT_RESOLVE_ACTIVATION)
    return generic_activated_ability(player, card, event, 0, 1, 0, 0, 0, 1, 0, 0, NULL, NULL);
  pump_creatures_until_eot(player, card, player, 0, 1, 0, 0, 4, NULL);
  return generic_activated_ability(player, card, event, 0, 1, 0, 0, 0, 1, 0, 0, NULL, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23955e0; original cards/m20.c:431. */
int card_cavalier_of_gales(int player, int card, event_t event)
{
  /* Cavalier of Gales	|2|U|U|U
   * Creature - Elemental Knight 5/5
   * Flying
   * When this creature enters, draw three cards, then put two cards from your hand on top of your library in any order.
   * When this creature dies, shuffle it into its owner's library, then scry 2. (Look at the top two cards of your library, then put any number of them on the bottom and the rest on top in any order.) */

  int selected;
  int owner;
  int position;
  test_definition_t this_test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    draw_cards(player, 3);
    new_default_test_definition(&this_test, 0xff, "Select a card to put on top.");
    int tpb = hand_count[player];
    if (2 < tpb)
    {
      tpb = 2;
    }
    while (0 < tpb)
    {
      selected = new_select_a_card(player, player, 4, 1, 3, -1, &this_test);
      if (selected != -1)
      {
        put_on_top_of_deck(player, selected);
        tpb = tpb + -1;
      }
    }
  }
  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) &&
      find_in_owners_graveyard(player, card, &owner, &position))
  {
    from_graveyard_to_deck(owner, position, 3);
    scry(player, 2);
    return 0;
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x2395750; original cards/m20.c:468. */
int card_cavalier_of_night(int player, int card, event_t event)
{
  /* Cavalier of Night	|2|B|B|B
   * Creature - Elemental Knight 4/5
   * Lifelink
   * When this creature enters, you may sacrifice another creature. When you do, destroy target creature an opponent controls.
   * When this creature dies, return target creature card with mana value 3 or less from your graveyard to the battlefield. */

  int result;
  unsigned int chosen_sacrifice;
  card_instance_t *instance;
  target_definition_t td2;
  test_definition_t test;
  lifelink(player, card, event);
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 2, "Select another creature to sacrifice.");
    test.not_me = 1;
    default_target_definition(player, card, &td2, 2);
    td2.allowed_controller = 1 - player;
    chosen_sacrifice = new_sacrifice(player, card, player,
                                     SAC_RETURN_CHOICE | SAC_JUST_MARK | SAC_NO_CANCEL, &test);
    if (chosen_sacrifice != 0)
    {
      result = can_target(&td2);
      if (result != 0)
      {
        result = new_pick_target(&td2, "TARGET_CREATURE_OPPONENT_CONTROLS", 0, 0);
        if (result != 0)
        {
          instance = get_card_instance(player, card);
          result = instance->targets[0].card;
          instance = get_card_instance(player, card);
          kill_card(instance->targets[0].player, result, KILL_DESTROY);
        }
      }
      kill_card((int)chosen_sacrifice >> 0x10 & 0xff, chosen_sacrifice >> 0x18, KILL_SACRIFICE);
    }
  }
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    new_default_test_definition(&test, 2, "Select a creature card with CMC 3 or less.");
    test.cmc = 4;
    test.cmc_flag = 3;
    result = new_special_count_grave(player, &test);
    if (0 < result)
    {
      result = graveyard_has_shroud(player);
      if (result == 0)
      {
        new_global_tutor(player, player, 2, 4, 1, 1, &test);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23959a0; original cards/m20.c:514. */
int card_cavalier_of_thorns(int player, int card, event_t event)
{
  /* Cavalier of Thorns	|2|G|G|G
   * Creature - Elemental Knight 5/6
   * Reach
   * When this creature enters, reveal the top five cards of your library. Put a land card from among them onto the battlefield and the rest into your graveyard.
   * When this creature dies, you may exile it. If you do, put another target card from your graveyard on top of your library. */

  int result;
  test_definition_t test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    reveal_top_cards_of_library_and_choose_type(player, card, player, 5, 1, TUTOR_PLAY_TAPPED, 1,
                                                TUTOR_GRAVE, 1, TYPE_LAND);
  }
  result = duh_mode(player);
  result = this_dies_trigger(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (result != 0)
  {
    result = exile_from_owners_graveyard(player, card);
    if (result != 0)
    {
      default_test_definition(&test, 0xff);
      new_global_tutor(player, player, 2, 2, 0, 1, &test);
      return 0;
    }
  }
  return 0;
}

#ifndef SHANDALAR
static int can_cast_caf(int player, int card)
{
  int count = count_graveyard(player);
  const int *grave = get_grave(player);
  for (int pos = count; pos >= 0; --pos)
    if (is_what(-1, grave[pos], TYPE_SPELL) && get_cmc_by_internal_id(grave[pos]) <= 3 &&
        has_mana_to_cast_id(player, EVENT_CAN_CAST, cards_data[grave[pos]].id))
      return 1;
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2394370; original cards/m20.c:591. */
static int can_cast_this(int iid, int unused, int player, int card)
{
  int result;

  result = has_mana_to_cast_iid(player, EVENT_CAN_CAST, iid);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2395ad0; original cards/m20.c:594. */
int card_chandra_acolyte_of_flame(int player, int card, event_t event)
{
  /* Chandra, Acolyte of Flame	|1|R|R
   * Legendary Planeswalker - Chandra (4)
   * 0: Put a loyalty counter on each red planeswalker you control.
   * 0: Create two 1/1 red Elemental creature tokens. They gain haste. Sacrifice them at the beginning of the next end step.
   * -2: You may cast target instant or sorcery card with mana value 3 or less from your graveyard. If that spell would be put into your graveyard, exile it instead. */

  enum
  {
    CHOICE_COUNTERS = 1,
    CHOICE_TOKENS = 2,
    CHOICE_FLASHBACK = 3
  };

  int *graveyard;
  card_instance_t *instance;
  int grave_index_or_mode;
  int eligible;
  card_instance_t *planeswalker_record;
  unsigned int card_colors;
  unsigned int required_color;
  uint32_t grave_cast_priority;
  token_generation_t token;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    new_default_test_definition(
        &test, 0x38, "Select an instant or sorcery card with converted mana cost 3 or less.");
    test.cmc = 4;
    test.cmc_flag = 3;
    test.type_flag = 2;
    test.special_selection_function = can_cast_this;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;

      grave_index_or_mode = count_graveyard(player);
      graveyard = graveyard_ptr_mutable[player];
      grave_cast_priority = 0xffffffec;
      if (-1 < grave_index_or_mode)
      {
        do
        {
          eligible = is_what(-1, graveyard[grave_index_or_mode], 0x38);
          if (((eligible != 0) &&
               (eligible = get_cmc_by_internal_id(graveyard[grave_index_or_mode]), eligible < 4)) &&
              (eligible =
                   has_mana_to_cast_id(player, EVENT_CAN_CAST,
                                       (unsigned int)cards_data[graveyard[grave_index_or_mode]].id),
               eligible != 0))
          {
            grave_cast_priority = 0x14;
            break;
          }
          grave_index_or_mode = grave_index_or_mode + -1;
        } while (grave_index_or_mode != -1);
      }
      grave_index_or_mode = dialog_impl(
          player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Add loyalty", 1, 5, 0,
          "Elemental tokens", 1, 10, 0, "Cast from graveyard",
          new_special_count_grave(player, &test), grave_cast_priority, 0xfffffffe, 0, 0, 0, 0);
      if ((grave_index_or_mode == 0) ||
          ((grave_index_or_mode == CHOICE_FLASHBACK && (grave_index_or_mode = select_target_from_grave_source(
                                             player, card, player, SFG_0, 2, &test, 1),
                                         grave_index_or_mode == -1))))
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
    new_default_test_definition(
        &test, 0x38, "Select an instant or sorcery card with converted mana cost 3 or less.");
    test.cmc = 4;
    test.cmc_flag = 3;
    test.type_flag = 2;
    test.special_selection_function = can_cast_this;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    grave_index_or_mode = instance->info_slot;
    if (grave_index_or_mode == CHOICE_COUNTERS)
    {

      grave_index_or_mode = active_cards_count[player] + -1;
      if (-1 < grave_index_or_mode)
      {
        do
        {
          planeswalker_record = in_play(player, grave_index_or_mode);
          if ((planeswalker_record != NULL) &&
              (eligible = is_planeswalker(player, grave_index_or_mode), eligible != 0))
          {
            card_colors = get_color(player, grave_index_or_mode);
            required_color =
                get_sleighted_color_test(instance->parent_controller, instance->parent_card, 0x10);
            if ((card_colors & required_color) != 0)
            {
              add_counters(player, card, player, grave_index_or_mode, COUNTER_LOYALTY, 1);
            }
          }
          grave_index_or_mode = grave_index_or_mode + -1;
        } while (grave_index_or_mode != -1);
      }
    }
    else if (grave_index_or_mode == CHOICE_TOKENS)
    {
      default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
      token.tou = 1;
      token.pow = 1;
      token.qty = '\x02';
      token.special_code_on_generation = haste_and_sacrifice_token_at_end_of_next_turn;
      generate_token(&token);
    }
    else
    {

      if (((grave_index_or_mode == CHOICE_FLASHBACK) &&
           (grave_index_or_mode = validate_target_from_grave_source(player, card, player, 1),
            grave_index_or_mode != -1)) &&
          (eligible = charge_mana_from_id(
               player, -1, EVENT_RESOLVE_ACTIVATION,
               (unsigned int)cards_data[graveyard_ptr_mutable[player][grave_index_or_mode]].id),
           eligible != 0))
      {
        play_card_in_grave_for_free_and_exile_it(player, player, grave_index_or_mode);
      }
    }
  }
  return planeswalker(player, card, event, 4);
}
#endif

/* ManalinkEh 2022: 0x2395f20; original cards/m20.c:683. */
int card_chandra_awakened_inferno(int player, int card, event_t event)
{
  /* Chandra, Awakened Inferno	|4|R|R
   * Legendary Planeswalker - Chandra (6)
   * This spell can't be countered.
   * +2: Each opponent gets an emblem with "At the beginning of your upkeep, this emblem deals 1 damage to you."
   * -3: Chandra deals 3 damage to each non-Elemental creature.
   * -X: Chandra deals X damage to target creature or planeswalker. If a permanent dealt damage this way would die this turn, exile it instead. */

  enum
  {
    CHOICE_EMBLEM = 1,
    CHOICE_CREATURES = 2,
    CHOICE_TARGET = 3
  };

  int p;
  int c;
  int choice;
  int mode_or_available;
  card_instance_t *creature_or_damage;
  unsigned int loyalty_requirement;
  int toughness_or_state;
  int loyalty_or_damage;
  int sweep_priority;
  target_definition_t *damage_target;
  card_instance_t *instance;
  token_generation_t token;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  cannot_be_countered(player, card, event);
  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 6);
    default_test_definition(&test, 2);
    test.subtype = 0x3a;
    test.subtype_flag = 1;
    default_target_definition(player, card, &td, 0x102);
    if (player == 1)
    {
      loyalty_requirement = count_counters(1, card, COUNTER_LOYALTY);
      td.toughness_requirement = loyalty_requirement | 0x2000;
      instance = get_card_instance(1, card);
    }
    else
    {
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      mode_or_available = instance->info_slot;
      if (mode_or_available == 1)
      {
        default_token_definition(player, card, CARD_ID_CHANDRA_AWAKENED_INFERNO_EMBLEM, &token);
        token.t_player = 1 - player;
        generate_token(&token);
      }
      else if (mode_or_available == 2)
      {
        new_damage_all(player, card, 2, 3, 0, &test);
      }
      else if ((mode_or_available == 3) &&
               (mode_or_available = valid_target(&td), mode_or_available != 0))
      {

        creature_or_damage =
            get_card_instance(player, damage_target0(player, card, instance->targets[1].player));
        creature_or_damage->targets[3].card = creature_or_damage->targets[3].card | 0x80;
      }
    }
    return planeswalker(player, card, event, 6);
  }
  default_test_definition(&test, 2);
  test.subtype = 0x3a;
  test.subtype_flag = 1;
  default_target_definition(player, card, &td, 0x102);
  if (player == 1)
  {
    loyalty_requirement = count_counters(1, card, COUNTER_LOYALTY);
    td.toughness_requirement = loyalty_requirement | 0x2000;
    instance = get_card_instance(1, card);
    instance->number_of_targets = '\0';
    instance->info_slot = 0;
    loyalty_or_damage = count_counters(1, card, COUNTER_LOYALTY);
    p = current_turn;

    sweep_priority = 0;
    if (loyalty_or_damage >= 2)
    {
      c = active_cards_count[current_turn] + -1;
      if (-1 < c)
      {
        do
        {
          creature_or_damage = in_play(p, c);
          if (((creature_or_damage != NULL) &&
               (toughness_or_state = has_subtype(p, c, SUBTYPE_ELEMENTAL),
                toughness_or_state == 0)) &&
              (toughness_or_state = is_what(p, c, 2), toughness_or_state != 0))
          {
            toughness_or_state = get_toughness(p, c);
            creature_or_damage = get_card_instance(p, c);
            if (((int)(toughness_or_state - (unsigned int)creature_or_damage->damage_on_card) <
                 4) &&
                (toughness_or_state = check_for_special_ability(p, c, 0x800),
                 toughness_or_state == 0))
            {
              if (p == 1)
              {
                sweep_priority = sweep_priority + -5;
              }
              else
              {
                sweep_priority = sweep_priority + 5;
              }
            }
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
          creature_or_damage = in_play(p, c);
          if (((creature_or_damage != NULL) &&
               (toughness_or_state = has_subtype(p, c, SUBTYPE_ELEMENTAL),
                toughness_or_state == 0)) &&
              (toughness_or_state = is_what(p, c, 2), toughness_or_state != 0))
          {
            toughness_or_state = get_toughness(p, c);
            creature_or_damage = get_card_instance(p, c);
            if (((int)(toughness_or_state - (unsigned int)creature_or_damage->damage_on_card) <
                 4) &&
                (toughness_or_state = check_for_special_ability(p, c, 0x800),
                 toughness_or_state == 0))
            {
              if (p == 1)
              {
                sweep_priority = sweep_priority + -5;
              }
              else
              {
                sweep_priority = sweep_priority + 5;
              }
            }
          }
          c = c + -1;
        } while (c != -1);
      }
    }
  }
  else
  {
    instance = get_card_instance(player, card);

    instance->number_of_targets = '\0';
    instance->info_slot = 0;
    sweep_priority = 0;
  }
  damage_target = &td;
  mode_or_available = can_target(damage_target);
  choice =
      dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Emblem", 1,
                  0xf - life[1 - player], 2, "Damage non-Elementals", 1, sweep_priority, 0xfffffffd,
                  "Damage creature or planeswalker", mode_or_available, 0xf, 0, 0, 0, 0, 0);
  if (choice == 0)
  {
    spell_fizzled = 1;
    return 0;
  }
  if (choice != CHOICE_TARGET)
    return planeswalker(player, card, event, 6);

  if (pick_target(damage_target, "TARGET_CREATURE_OR_PLANESWALKER") == 0)
  {
    if (spell_fizzled == 1)
      return planeswalker(player, card, event, 6);
  }
  else
  {

    mode_or_available = count_counters(player, card, COUNTER_LOYALTY);
    if (is_planeswalker(instance->targets[0].player, instance->targets[0].card) == 0)
    {
      loyalty_or_damage = get_toughness(instance->targets[0].player, instance->targets[0].card);
    }
    else
    {
      loyalty_or_damage =
          count_counters(instance->targets[0].player, instance->targets[0].card, COUNTER_LOYALTY);
    }
    if (mode_or_available < loyalty_or_damage)
    {
      loyalty_or_damage = mode_or_available;
    }
    if (player == 0)
    {
      loyalty_or_damage = choose_a_number(0, "Pay how much Loyalty ?", loyalty_or_damage);
    }
    if (spell_fizzled == 1)
      return planeswalker(player, card, event, 6);
    if ((0 < loyalty_or_damage) &&
        (mode_or_available = count_counters(player, card, COUNTER_LOYALTY),
         loyalty_or_damage <= mode_or_available))
    {
      remove_counters(player, card, player, card, COUNTER_LOYALTY, loyalty_or_damage);
      instance->targets[1].player = loyalty_or_damage;
      return planeswalker(player, card, event, 6);
    }
  }
  spell_fizzled = 1;
  return planeswalker(player, card, event, 6);
}

/* ManalinkEh 2022: 0x2396660; original cards/m20.c:801. */
int card_chandra_novice_pyromancer(int player, int card, event_t event)
{
  /* Chandra, Novice Pyromancer	|3|R
   * Legendary Planeswalker - Chandra (5)
   * +1: Elementals you control get +2/+0 until end of turn.
   * -1: Add |R|R.
   * -2: Chandra deals 2 damage to any target. */

  enum
  {
    CHOICE_PUMP = 1,
    CHOICE_MANA = 2,
    CHOICE_TARGET = 3
  };

  int choice;
  card_instance_t *instance;
  int damage_available;
  int mana_available;
  target_definition_t *damage_target;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      damage_available = can_target(&td);
      mana_available = can_produce_mana(player, card);
      choice =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Pump elementals", 1,
                      count_subtype(player, 2, 0x3a), 1, "Add mana", mana_available, 2, 0xffffffff,
                      "Deal 2 damage", damage_available, 2, 0xfffffffe, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_MANA)
      {
        produce_mana(player, 4, 2);
        tapped_for_mana_color = -2;
      }
      else if (choice == CHOICE_TARGET)
      {
        new_pick_target(&td, "TARGET_ANY", 0, 1);
      }
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    instance = get_card_instance(player, card);
  }
  damage_target = &td;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    if (instance->info_slot == CHOICE_PUMP)
    {
      default_test_definition(&test, 2);
      test.subtype = 0x3a;
      pump_creatures_until_eot_merge_pt(player, card, player, 2, 0, &test);
    }
    else if ((instance->info_slot == CHOICE_TARGET) &&
             (damage_available = valid_target(damage_target), damage_available != 0))
    {
      damage_target0(player, card, 2);
    }
  }
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x2396950; original cards/m20.c:877. */
int card_colossus_hammer(int player, int card, event_t event)
{
  /* Colossus Hammer	|1
   * Artifact - Equipment
   * Equipped creature gets +10/+10 and loses flying.
   * Equip |8 (|8: Attach to target creature you control. Equip only as a sorcery.) */

  card_instance_t *instance;
  int result;

  instance = is_equipping(player, card);
  if ((((event == EVENT_ABILITIES) && (instance != NULL)) &&
       (instance->damage_target_card == affected_card)) &&
      (instance->damage_target_player == affected_card_controller))
  {
    event_result = event_result & 0xffffffdf;
  }
  result = vanilla_equipment(player, card, event, 8, 10, 10, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x23969f0; original cards/m20.c:892. */
int card_corpse_knight(int player, int card, event_t event)
{
  /* Corpse Knight	|W|B
   * Creature - Zombie Knight 2/2
   * Whenever another creature you control enters, each opponent loses 1 life. */

  test_definition_t test;
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 2);
    test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event != EVENT_ETB)
  {
    return 0;
  }
  lose_life(1 - player, 1);
  return 0;
}

/* ManalinkEh 2022: 0x2396ab0; original cards/m20.c:990. */
int card_drawn_from_dreams(int player, int card, event_t event)
{
  /* Drawn from Dreams	|2|U|U
   * Sorcery
   * Look at the top seven cards of your library. Put two of them into your hand and the rest on the bottom of your library in a random order. */

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
      new_default_test_definition(&test, 0xff, "Select a card.");
      test.qty = 2;
      reveal_top_cards_of_library_and_choose(player, card, player, 7, 1, TUTOR_HAND, 0,
                                             TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test);
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

/* ManalinkEh 2022: 0x2396bc0; original cards/m20.c:1027. */
int card_elvish_reclaimer(int player, int card, event_t event)
{
  /* Elvish Reclaimer	|G
   * Creature - Elf Warrior 1/2
   * This creature gets +2/+2 as long as there are three or more land cards in your graveyard.
   * |2, |T, Sacrifice a land: Search your library for a land card, put it onto the battlefield tapped, then shuffle. */

  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if (card == affected_card && player == affected_card_controller &&
        count_graveyard_by_type(player, TYPE_LAND) > 2)
      event_result += 2;
  }
  else if (IS_GAA_EVENT(event))
  {
    test_definition_t test;
    new_default_test_definition(&test, TYPE_LAND, "Select a land card");
    if (event == EVENT_RESOLVE_ACTIVATION)
      new_global_tutor(player, player, 1, 5, 0, 5, &test);
    return generic_activated_ability_with_sacrifice_as_cost(
        player, card, event, GAA_UNTAPPED, 2, 0, 0, 0, 0, 0, 0, NULL, NULL,
        TYPE_LAND, NULL, SARV_0);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2396d30; original cards/m20.c:1095. */
int card_field_of_the_dead(int player, int card, event_t event)
{
  /* Field of the Dead	""
   * Land
   * This land enters tapped.
   * |T: Add |C.
   * Whenever this land or another land you control enters, if you control seven or more lands with different names, create a 2/2 black Zombie creature token. */

  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && card == affected_card &&
      player == affected_card_controller && trigger_cause_controller == player &&
      reason_for_trigger_controller == player && !is_humiliated(player, card, AL_TRIG))
  {
    bool zombo = false;
    if (is_what(trigger_cause_controller, trigger_cause, TYPE_LAND) &&
        count_permanents_by_type(player, TYPE_LAND) > 6)
    {
      test_definition_t test;
      default_test_definition(&test, TYPE_LAND);
      int array[7];
      memset(array + 1, 0xff, sizeof(array) - sizeof(array[0]));
      array[0] = get_card_name(trigger_cause_controller, trigger_cause);
      for (int c = 0; c < active_cards_count[player]; ++c)
      {
        if (in_play(player, c) && is_what(player, c, TYPE_LAND))
        {
          for (int i = 0; i != 7; ++i)
          {
            if (get_card_name(player, c) == array[i])
              break;
            if (array[i] == -1)
            {
              array[i] = get_card_name(player, c);
              if (i == 6)
                zombo = true;
              break;
            }
          }
        }
        if (zombo)
          break;
      }
    }
    test_definition_t test;
    default_test_definition(&test, TYPE_LAND);
    etb_ability_global(player, event, zombo ? 2 : 0, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
    generate_token_by_id(player, card, CARD_ID_ZOMBIE);
  return plain_mana_producer_tapped(player, card, event);
}

/* ManalinkEh 2022: 0x2396f70; original cards/m20.c:1158. */
int card_fry(int player, int card, event_t event)
{
  /* Fry	|1|R
   * Instant
   * This spell can't be countered.
   * ~ deals 5 damage to target creature or planeswalker that's white or blue. */

  unsigned int white_test;
  unsigned int blue_test;
  const char *prompt;
  target_definition_t td;

  cannot_be_countered(player, card, event);
  default_target_definition(player, card, &td, 0x102);
  white_test = get_sleighted_color_test(player, card, 0x20);
  blue_test = get_sleighted_color_test(player, card, 4);
  td.required_color = blue_test | white_test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      damage_target0(player, card, 5);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  prompt = get_sleighted_color_text2(
      player, card, "Select target creature or planeswalker that\'s %s or %s.", 5, 2);
  return generic_spell(player, card, event, 0x81, &td, prompt, 1, NULL);
}

/* ManalinkEh 2022: 0x2397090; original cards/m20.c:1240. */
int card_golos_tireless_pilgrim(int player, int card, event_t event)
{
  /* Golos, Tireless Pilgrim	|5
   * Legendary Artifact Creature - Scout 3/5
   * When Golos enters, you may search your library for a land card, put that card onto the battlefield tapped, then shuffle.
   * |2|W|U|B|R|G: Exile the top three cards of your library. You may play them this turn without paying their mana costs. */

  check_legend_rule(player, card, event);
  resolve_trigger_t trigger_mode = (duh_mode(player) != 0) + RESOLVE_TRIGGER_OPTIONAL;
  etb_ability(player, card, event, trigger_mode);
  if (event == EVENT_ETB)
  {
    test_definition_t test;
    new_default_test_definition(&test, TYPE_LAND, "Choose a land card.");
    new_global_tutor(player, player, 1, 5, 0, 1, &test);
    return 0;
  }
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
    for (int i = 0; i < 3; ++i)
      if (*deck_ptr[player] != -1)
      {
        unsigned short csvid = cards_data[*deck_ptr[player]].id;
        rfg_top_card_of_deck(player);
        create_may_play_card_from_exile_effect(player, card, player, (unsigned int)csvid,
                                               MPCFE_UNTIL_EOT | MPCFE_FOR_FREE);
      }
  return generic_activated_ability(player, card, event, 0, 2, 1, 1, 1, 1, 1, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x2397240; original cards/m20.c:1274. */
int card_gorging_vulture(int player, int card, event_t event)
{
  /* Gorging Vulture	|2|B
   * Creature - Bird 2/2
   * Flying
   * When this creature enters, mill four cards. You gain 1 life for each creature card milled this way. (To mill four cards, put the top four cards of your library into your graveyard.) */

  int source_csvid;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  source_csvid = get_id(player, card);
  special_mill(player, card, source_csvid, player, 4);
  return 0;
}

/* ManalinkEh 2022: 0x23972c0; original cards/m20.c:1359. */
int card_infuriate(int player, int card, event_t event)
{
  /* Infuriate	|R
   * Instant
   * Target creature gets +3/+2 until end of turn. */

  int result;
  result = vanilla_instant_pump(player, card, event, 2, player, 3, 2, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x2397310; original cards/m20.c:1377. */
int card_kaalia_zenith_seeker(int player, int card, event_t event)
{
  /* Kaalia, Zenith Seeker	|R|W|B
   * Legendary Creature - Human Cleric 3/3
   * Flying, vigilance
   * When Kaalia enters, look at the top six cards of your library. You may reveal an Angel card, a Demon card, and/or a Dragon card from among them and put them into your hand. Put the rest on the bottom of your library in a random order. */

  unsigned char dragon_available;
  bool demon_available;
  int amount;
  unsigned int selected_dragon;
  int deck_byte_offset_or_selected;
  bool angel_or_deck_available;
  test_definition_t test;

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if ((event == EVENT_ETB) && (*deck_ptr[player] != -1))
  {

    show_deck(0, deck_ptr[player], 4, "Revealed", 0, gs_done_008b40e0);

    amount = count_deck(player);
    if (4 < amount)
    {
      amount = 4;
    }
    if (0 < amount + -1)
    {
      deck_byte_offset_or_selected = 0;
      dragon_available = 0;
      demon_available = false;
      angel_or_deck_available = false;
      do
      {
        if (has_subtype_by_id(
                player,
                (unsigned int)
                    cards_data[*(int *)((int)deck_ptr[player] + deck_byte_offset_or_selected)]
                        .id,
                SUBTYPE_ANGEL) != 0)
        {
          angel_or_deck_available = true;
        }
        if (has_subtype_by_id(
                player,
                (unsigned int)
                    cards_data[*(int *)((int)deck_ptr[player] + deck_byte_offset_or_selected)]
                        .id,
                SUBTYPE_DEMON) != 0)
        {
          demon_available = true;
        }
        if (has_subtype_by_id(
                player,
                (unsigned int)
                    cards_data[*(int *)((int)deck_ptr[player] + deck_byte_offset_or_selected)]
                        .id,
                SUBTYPE_DRAGON) != 0)
        {
          dragon_available = 1;
        }
        deck_byte_offset_or_selected = deck_byte_offset_or_selected + 4;
      } while (amount * 4 + -4 != deck_byte_offset_or_selected);
      if (angel_or_deck_available)
      {
        new_default_test_definition(&test, 0xff, "Select an Angel card.");
        test.subtype = 2;
        test.no_shuffle = 2;
        test.create_minideck = (unsigned short)amount;
        deck_byte_offset_or_selected = new_global_tutor(player, player, 1, 1, 0, 1, &test);
        if (-1 < deck_byte_offset_or_selected)
        {
          amount = amount + -1;
        }
      }
      angel_or_deck_available = *deck_ptr[player] != -1;
      if ((demon_available) && (angel_or_deck_available))
      {
        new_default_test_definition(&test, 0xff, "Select a Demon card.");
        test.subtype = 0x2c;
        test.no_shuffle = 2;
        test.create_minideck = (unsigned short)amount;
        deck_byte_offset_or_selected = new_global_tutor(player, player, 1, 1, 0, 1, &test);
        if (-1 < deck_byte_offset_or_selected)
        {
          amount = amount + -1;
        }
        angel_or_deck_available = *deck_ptr[player] != -1;
      }
      if ((bool)(dragon_available & angel_or_deck_available))
      {
        new_default_test_definition(&test, 0xff, "Select a Dragon card.");
        test.create_minideck = (unsigned short)amount;
        test.subtype = 0x30;
        test.no_shuffle = 2;
        selected_dragon = new_global_tutor(player, player, 1, 1, 0, 1, &test);
        amount = amount - (unsigned int)(selected_dragon < 0x80000000);
      }
    }
    put_top_x_on_bottom_in_random_order(player, amount);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2397690; original cards/m20.c:1449. */
int card_knight_of_the_ebon_legion(int player, int card, event_t event)
{
  /* Knight of the Ebon Legion	|B
   * Creature - Vampire Knight 1/2
   * |2|B: This creature gets +3/+3 and gains deathtouch until end of turn.
   * At the beginning of your end step, if a player lost 4 or more life this turn, put a +1/+1 counter on this creature. (Damage causes loss of life.) */

  int result;
  card_instance_t *instance;

  result = eot_trigger(player, card, event);
  if (((result != 0) && (current_turn == player)) &&
      ((result = get_trap_condition(player, 2),
        3 < result || (result = get_trap_condition(1 - player, 2), 3 < result))))
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  }
  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      instance = get_card_instance(player, card);
      pump_ability_until_eot(player, card, instance->parent_controller, instance->parent_card, 3, 3,
                             0, 0x200);
    }
  }
  result = generic_activated_ability(player, card, event, 0, 2, 1, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2397800; original cards/m20.c:1471. */
int card_kykar_winds_fury(int player, int card, event_t event)
{
  /* Kykar, Wind's Fury	|1|U|R|W
   * Legendary Creature - Bird Wizard 3/3
   * Flying
   * Whenever you cast a noncreature spell, create a 1/1 white Spirit creature token with flying.
   * Sacrifice a Spirit: Add |R. */

  int result;

  prowess_mode(player, card, event, 2);
  if (event == EVENT_SPA)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_SPIRIT, &token);
    token.pow = 1;
    token.tou = 1;
    token.color_forced = ' ';
    token.key_plus = 0x20;
    generate_token(&token);
  }
  else if (event == EVENT_COUNT_MANA)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      test_definition_t test;
      default_test_definition(&test, 0x47);
      test.subtype = SUBTYPE_SPIRIT;
      test.qty = 1;
      result = can_produce_mana(player, card);
      if ((result != 0) &&
          (result = new_can_sacrifice_as_cost(player, card, &test),
           result != 0))
      {
        result = count_subtype(player, -1, 0xbc);
        declare_mana_available(player, 4, result);
      }
    }
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    test_definition_t test2;
    default_test_definition(&test2, 0x47);
    test2.subtype = SUBTYPE_SPIRIT;
    test2.qty = 1;
    result = can_produce_mana(player, card);
    if ((result != 0) &&
        (result = new_can_sacrifice_as_cost(player, card, &test2),
         result != 0))
    {
      return 1;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    test_definition_t test3;
    new_default_test_definition(&test3, 0x47, "Select a spirit to sacrifice.");
    test3.subtype = SUBTYPE_SPIRIT;
    result = new_sacrifice(player, card, player, SAC_AS_COST, &test3);
    if (result == 0)
    {
      spell_fizzled = 1;
    }
    else
    {
      produce_mana(player, 4, 1);
      tapped_for_mana_color = -2;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2397a20; original cards/m20.c:1534. */
int card_leafkin_druid(int player, int card, event_t event)
{
  /* Leafkin Druid	|1|G
   * Creature - Elemental Druid 0/3
   * |T: Add |G. If you control four or more creatures, add |G|G instead. */

  int result;
  result = count_permanents_by_type(player, TYPE_CREATURE);
  result = mana_producing_creature(player, card, event, 0x14, COLOR_GREEN, (3 < result) + 1);
  return result;
}

/* ManalinkEh 2022: 0x2397a80; original cards/m20.c:1544. */
int card_legions_end(int player, int card, event_t event)
{
  /* Legion's End	|1|B
   * Sorcery
   * Exile target creature an opponent controls with mana value 2 or less and all other creatures that player controls with the same name as that creature. Then that player reveals their hand and exiles all cards with that name from their hand and graveyard. */

  int csvid;
  card_instance_t *instance;
  int grave_count_or_selected;
  int hand_player_or_selected;
  target_definition_t td;
  test_definition_t this_test;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  td.power_requirement = 0x2002;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (validate_target(player, card, &td, 0) != 0)
    {
      csvid = get_id(instance->targets[0].player, instance->targets[0].card);
      echoing_effect(player, card, instance->targets[0].player, instance->targets[0].card, &td, 4);
      new_default_test_definition(&this_test, 0, "");
      this_test.no_shuffle = 1;
      this_test.id = csvid;
      if (ai_is_speculating != 1)
      {
        scnprintf(this_test.message, 100, "Hand: Select a card named %s to exile.",
                  cards_ptr[csvid]->full_name);
      }
      do
      {
        hand_player_or_selected = instance->targets[0].player;
        if ((hand_count[hand_player_or_selected] < 1) ||
            (hand_player_or_selected =
                 new_global_tutor(player, hand_player_or_selected, 4, 6, 1, 5, &this_test),
             hand_player_or_selected < 0))
          break;
        hand_player_or_selected = is_id_in_hand(instance->targets[0].player, csvid);
      } while (hand_player_or_selected != 0);
      if (ai_is_speculating != 1)
      {
        scnprintf(this_test.message, 100, "Graveyard: Select a card named %s to exile.",
                  cards_ptr[csvid]->full_name);
      }
      do
      {
        grave_count_or_selected = new_special_count_grave(instance->targets[0].player, &this_test);
        if (grave_count_or_selected < 1)
          break;
        grave_count_or_selected =
            new_global_tutor(player, instance->targets[0].player, 2, 6, 1, 5, &this_test);
      } while (-1 < grave_count_or_selected);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td,
                       "Select target creature an opponent controls with CMC 2 or less", 1, NULL);
}

/* ManalinkEh 2022: 0x2397df0; original cards/m20.c:1605. */
int card_leyline_of_abundance(int player, int card, event_t event)
{
  /* Leyline of Abundance	|2|G|G
   * Enchantment
   * If this card is in your opening hand, you may begin the game with it on the battlefield.
   * Whenever you tap a creature for mana, add an additional |G.
   * |6|G|G: Put a +1/+1 counter on each creature you control. */

  int result;
  card_instance_t *instance;
  test_definition_t this_test;

  result = is_humiliated(player, card, AL_TRIG);
  if (((((uint32_t)(event - EVENT_COUNT_MANA) & 0xfffffffd) == 0 && result == 0) &&
       (affected_card_controller == player)) &&
      (result = is_what(player, affected_card, 2), result != 0))
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
      instance = get_card_instance(affected_card_controller, affected_card);
      if ((instance->state & 0x20000) != 0)
      {
        return 0;
      }
      result = can_produce_mana(affected_card_controller, affected_card);
      if (result == 0)
      {
        return 0;
      }
      declare_mana_available(affected_card_controller, 3, 1);
      return global_enchantment(player, card, event);
    }
    if (-1 < tapped_for_mana_color)
    {
      produce_mana(affected_card_controller, 3, 1);
    }
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return global_enchantment(player, card, event);
  if (event != EVENT_RESOLVE_ACTIVATION)
    return generic_activated_ability(player, card, event, 0, 6, 0, 0, 2, 0, 0, 0, NULL, NULL);
  default_test_definition(&this_test, 2);
  new_manipulate_all(player, card, player, &this_test, ACT_ADD_COUNTERS_BASE | ACT_KILL_BURY);
  return generic_activated_ability(player, card, event, 0, 6, 0, 0, 2, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x2398010; original cards/m20.c:1648. */
int card_leyline_of_combustion(int player, int card, event_t event)
{
  /* Leyline of Combustion	|2|R|R
   * Enchantment
   * If this card is in your opening hand, you may begin the game with it on the battlefield.
   * Whenever you and/or at least one permanent you control becomes the target of a spell or ability an opponent controls, this enchantment deals 2 damage to that player. */

  card_instance_t *instance;
  const target_t *targeting_source;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      targeting_source = any_becomes_target(player, card, event, player, -1, TYPE_PERMANENT, -1,
                                            TYPE_ANY, 1 - player, RESOLVE_TRIGGER_MANDATORY);
      if (targeting_source != NULL)
      {
        damage_player(1 - player, 2, player, card);
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2394390; original cards/m20.c:1737. */
static int moldervine_reclamation_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  unsigned int pending_deaths;
  card_instance_t *dying;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (get_id((int)instance->damage_target_player, instance->damage_target_card) != 0x4f96))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (effect_follows_control_of_attachment(player, card, 0x4700) != 0))
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (effect_follows_control_of_attachment(player, card, event) != 0))
    {
      return 0;
    }
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {

      instance = get_card_instance(player, card);
      dying = in_play(affected_card_controller, affected_card);
      if ((dying != NULL) &&
          (is_this_dying(affected_card_controller, affected_card) != 0))
      {

        if ((instance->damage_target_card == affected_card) &&
            (instance->damage_target_player == affected_card_controller))
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
        count_for_gfp_ability_and_store_values(player, card, 0x77, player, 2, NULL, 0, 0);
      }
    }
  }
  pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_deaths != 0)
  {
    if (!check_status(player, card, 0x10000))
    {

      for (pending_deaths = pending_deaths & 0xff; pending_deaths != 0;
           pending_deaths = pending_deaths - 1)
      {
        gain_life(player, 1);
      }
      draw_cards(player, 0);
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) && (check_status(player, card, 0x10000) == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23980d0; original cards/m20.c:1788. */
int card_moldervine_reclamation(int player, int card, event_t event)
{
  /* Moldervine Reclamation	|3|B|G
   * Enchantment
   * Whenever a creature you control dies, you gain 1 life and draw a card. */

  int c;
  unsigned int pending_deaths;
  int created_legacy;
  card_instance_t *instance;

  bool legacy_found = false;
  if (((event == EVENT_STATIC_EFFECTS) && (card == affected_card)) &&
      (player == affected_card_controller))
  {

    if (0 < active_cards_count[player])
    {
      c = 0;
      do
      {
        instance = in_play(player, c);

        if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
             (instance = get_card_instance(player, c),
              instance->info_slot_fn == moldervine_reclamation_effect)) &&
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
      created_legacy =
          create_targetted_legacy_effect(player, card, moldervine_reclamation_effect, player, card);
      instance = get_card_instance(player, created_legacy);
      instance->targets[0].player = player;
      instance->targets[0].card = card;
      instance->number_of_targets = '\x01';
      add_status(player, created_legacy, 0x10000);
    }
  }
  count_for_gfp_ability_and_store_values(player, card, event, player, 2, NULL, 0, 0);
  pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if ((pending_deaths != 0) &&
      (instance = get_card_instance(player, card), instance->kill_code == '\0'))
  {

    for (pending_deaths = pending_deaths & 0xff; pending_deaths != 0;
         pending_deaths = pending_deaths - 1)
    {
      gain_life(player, 1);
    }
    draw_cards(player, 0);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x23942a0; original cards/m20.c:1844. */
static int fx_yanling_debuff(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (event == EVENT_POWER)
  {
    instance = get_card_instance(player, card);
    if ((instance->damage_target_card == affected_card) &&
        (instance->damage_target_player == affected_card_controller))
    {
      event_result = event_result + -2;
    }
  }
  else if (event == EVENT_ABILITIES)
  {
    instance = get_card_instance(player, card);
    if ((instance->damage_target_card == affected_card) &&
        (instance->damage_target_player == affected_card_controller))
    {
      event_result = event_result & 0xffffffdf;
    }
  }
  else if ((event == EVENT_BEGIN_TURN) && (current_turn == player))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23982c0; original cards/m20.c:1863. */
int card_mu_yanling_sky_dancer(int player, int card, event_t event)
{
  /* Mu Yanling, Sky Dancer	|1|U|U
   * Legendary Planeswalker - Yanling (2)
   * +2: Until your next turn, up to one target creature gets -2/-0 and loses flying.
   * -3: Create a 4/4 blue Elemental Bird creature token with flying.
   * -8: You get an emblem with "Islands you control have '|T: Draw a card.'" */

  enum
  {
    CHOICE_DEBUFF = 1,
    CHOICE_TOKEN = 2,
    CHOICE_EMBLEM = 3
  };

  int choice;
  card_instance_t *instance;
  int result_or_mode;
  card_instance_t *emblem;
  int emblem_priority;
  target_definition_t *creature_target;
  token_generation_t token;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 1 - player;
    td.allow_cancel = 3;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      instance->targets[0].card = -1;
      instance->targets[0].player = -1;
      result_or_mode = check_battlefield_for_id(player, 0x4f99);
      emblem_priority = -0x19;
      if (result_or_mode == 0)
      {
        result_or_mode = count_subtype(player, 1, get_hacked_subtype(player, card, 0x5002));
        emblem_priority = count_counters(player, card, COUNTER_LOYALTY);
        emblem_priority = result_or_mode * 5 + emblem_priority * 5 + -0x19;
      }
      result_or_mode = can_target(&td);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                           "Debuff a creature", 1, result_or_mode * 10, 2, "Elemental Bird token",
                           1, 10, 0xfffffffd, "Emblem", 1, emblem_priority, 0xfffffff8, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_DEBUFF)
      {
        instance->number_of_targets = '\0';
        new_pick_target(&td, "TARGET_CREATURE", 0, 0);
        if (instance->targets[0].card == -1)
        {
          spell_fizzled = 1;
        }
      }
      return planeswalker(player, card, event, 2);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 2);
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 1 - player;
    td.allow_cancel = 3;
    instance = get_card_instance(player, card);
  }
  creature_target = &td;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    result_or_mode = instance->info_slot;
    if (result_or_mode == CHOICE_DEBUFF)
    {
      if (valid_target(creature_target) != 0)
      {
        create_targetted_legacy_effect(player, card, fx_yanling_debuff, instance->targets[0].player,
                                       instance->targets[0].card);
      }
    }
    else if (result_or_mode == CHOICE_TOKEN)
    {
      default_token_definition(player, card, CARD_ID_ELEMENTAL_BIRD, &token);
      token.tou = 4;
      token.pow = 4;
      token.color_forced = '\x04';
      generate_token(&token);
    }
    else if (result_or_mode == CHOICE_EMBLEM)
    {

      emblem = get_card_instance(player, generate_token_by_id(player, card, CARD_ID_MU_YANLING_SKY_DANCER_EMBLEM));
      result_or_mode =
          get_hacked_subtype(instance->parent_controller, instance->parent_card, 0x5002);
      emblem->targets[1].player = result_or_mode;
      emblem->hack_mode[2] = instance->hack_mode[2];
    }
  }
  return planeswalker(player, card, event, 2);
}
#endif

/* ManalinkEh 2022: 0x23986c0; original cards/m20.c:1943. */
int card_mu_yanling_sky_dancer_emblem(int player, int card, event_t event)
{
  /* Mu Yanling, Sky Dancer Emblem	""
   * Emblem
   * Islands you control have "|T: Draw a card." */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 1);
    instance = get_card_instance(player, card);
    td.illegal_state = TARGET_STATE_TAPPED;
    td.required_subtype = instance->targets[1].player;
    td.allowed_controller = player;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 1);
    instance = get_card_instance(player, card);
    td.allowed_controller = player;
    td.illegal_state = TARGET_STATE_TAPPED;
    td.required_subtype = instance->targets[1].player;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      draw_a_card(player);
    }
  }
  result = generic_activated_ability_with_tapsubtype_as_cost(player, card, event, GAA_NONE, 0, 0, 0,
                                                             0, 0, 0, 0, NULL, NULL, &td, 1);
  return result;
}

/* ManalinkEh 2022: 0x2398800; original cards/m20.c:1968. */
int card_mystic_forge(int player, int card, event_t event)
{
  /* Mystic Forge	|4
   * Artifact
   * You may look at the top card of your library any time.
   * You may cast artifact spells and colorless spells from the top of your library.
   * |T, Pay 1 life: Exile the top card of your library. */

  enum
  {
    CHOICE_REVEAL = 1,
    CHOICE_PLAY = 2,
    CHOICE_EXILE = 3
  };

  int choice;
  unsigned int color_or_cast_available;
  int query_result;
  int iid_or_exile_available;
  card_instance_t *instance;
  uint32_t exile_priority;
  uint32_t inspect_available;

  if (event == EVENT_CAN_ACTIVATE)
  {

    iid_or_exile_available = *deck_ptr[player];
    if (iid_or_exile_available != -1)
    {
      query_result = is_what(-1, iid_or_exile_available, 0x40);
      if ((((query_result != 0) ||
            ((query_result = get_color_by_internal_id(player, *deck_ptr[player]),
              query_result == 1 &&
                  (query_result = is_what(-1, *deck_ptr[player], 1), query_result == 0)))) &&
           (query_result = has_mana_to_cast_iid(player, EVENT_CAN_ACTIVATE, iid_or_exile_available),
            query_result != 0)) &&
          (iid_or_exile_available =
               can_legally_play_iid_now(player, iid_or_exile_available, EVENT_CAN_ACTIVATE),
           iid_or_exile_available != 0))
      {
        return iid_or_exile_available;
      }
      if (*deck_ptr[player] != -1)
      {
        return 1;
      }
    }
    iid_or_exile_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0,
                                                       0, 0, 0, 1, NULL, NULL);
    return (unsigned int)(iid_or_exile_available != 0);
  }
  if (event != EVENT_ACTIVATE)
  {
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      if (event == EVENT_CLEANUP)
      {
        return 0;
      }
      return global_enchantment(player, card, event);
    }
    instance = get_card_instance(player, card);
    if (instance->info_slot != CHOICE_EXILE)
    {
      return 0;
    }
    rfg_top_card_of_deck(player);
    return 0;
  }

  iid_or_exile_available = *deck_ptr[player];
  inspect_available = ai_is_speculating != 1 && (player != 1 || ((unsigned char)trace_mode & 2));
  if (iid_or_exile_available == -1)
    inspect_available = 0;
  bool play_available = false;
  if (iid_or_exile_available != -1)
  {
    iid_or_exile_available = is_what(-1, iid_or_exile_available, 0x40);
    if (!(((iid_or_exile_available == 0) &&
           (((color_or_cast_available = get_color_by_internal_id(player, *deck_ptr[player]),
              (color_or_cast_available & 1) == 0 &&
                  (iid_or_exile_available = get_color_by_internal_id(player, *deck_ptr[player]),
                   iid_or_exile_available != 0)) ||
             (iid_or_exile_available = is_what(-1, *deck_ptr[player], 1),
              iid_or_exile_available != 0)))) ||
          (iid_or_exile_available =
               can_legally_play_iid_now(player, *deck_ptr[player], EVENT_ACTIVATE),
           iid_or_exile_available == 0)))
    {
      query_result = has_mana_to_cast_iid(player, EVENT_ACTIVATE, *deck_ptr[player]);
      play_available = query_result != 0;
    }
  }
  color_or_cast_available = play_available;
  iid_or_exile_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0,
                                                     0, 0, 0, 1, NULL, NULL);
  exile_priority = play_available ? 0xfffffff6 : 5;
  choice =
      dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Check top card of deck",
                  inspect_available, 0, "Play top card", color_or_cast_available, 10,
                  "Exile top of library", iid_or_exile_available, exile_priority, 0, 0, 0, 0);
  if (choice == 0)
  {
    spell_fizzled = 1;
    return 0;
  }
  if (choice == CHOICE_REVEAL)
  {
    show_deck(player, deck_ptr[player], 1, "Here\'s the top card of your deck", 0,
              gs_done_008b40e0);
    spell_fizzled = 1;
  }
  else if (choice == CHOICE_PLAY)
  {
    if (charge_mana_from_id(player, -1, EVENT_ACTIVATE,
                            (unsigned int)cards_data[*deck_ptr[player]].id) == 0)
    {
      spell_fizzled = 1;
    }
    else
    {
      play_card_in_deck_for_free(player, player, 0);
      cant_be_responded_to = 1;
    }
  }
  else if (choice == CHOICE_EXILE)
  {
    return generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 1, NULL,
                                     NULL);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2398dd0; original cards/m20.c:2045. */
int card_noxious_grasp(int player, int card, event_t event)
{
  /* Noxious Grasp	|1|B
   * Instant
   * Destroy target creature or planeswalker that's green or white. You gain 1 life. */

  unsigned int green_test;
  unsigned int white_test;
  const char *prompt;
  int result;
  card_instance_t *instance;
  target_definition_t td;

  default_target_definition(player, card, &td, 0x102);
  green_test = get_sleighted_color_test(player, card, 8);
  white_test = get_sleighted_color_test(player, card, 0x20);
  td.required_color = white_test | green_test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      result = instance->targets[0].card;
      instance = get_card_instance(player, card);
      kill_card(instance->targets[0].player, result, KILL_DESTROY);
      gain_life(player, 1);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  prompt =
      get_sleighted_color_text2(player, card, "Select target creature that\'s %s or %s .", 3, 5);
  return generic_spell(player, card, event, 0x81, &td, prompt, 1, NULL);
}

/* ManalinkEh 2022: 0x2398f10; original cards/m20.c:2081. */
int card_omnath_locus_of_the_roil(int player, int card, event_t event)
{
  /* Omnath, Locus of the Roil	|1|G|U|R
   * Legendary Creature - Elemental 3/3
   * When Omnath enters, it deals damage to any target equal to the number of Elementals you control.
   * Landfall - Whenever a land you control enters, put a +1/+1 counter on target Elemental you control. If you control eight or more lands, draw a card. */

  card_instance_t *instance;
  int result;
  bool other_player_etb;
  target_definition_t td;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  landfall(player, card, event);
  if ((event == EVENT_ETB) && (trigger_cause_controller == player))
  {
    if (trigger_cause == card)
    {
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td.allow_cancel = 0;
      result = can_target(&td);
      if ((result != 0) && (result = pick_target(&td, "TARGET_ANY"), result != 0))
      {
        result = count_subtype(player, -1, 0x3a);
        damage_target0(player, card, result);
      }
      instance = get_card_instance(player, card);
      other_player_etb = player != trigger_cause_controller;
      instance->number_of_targets = '\0';
      if (other_player_etb)
      {
        return 0;
      }
    }
    if (trigger_cause != card)
    {

      default_target_definition(player, card, &td, 0x47);
      td.allowed_controller = player;
      td.preferred_controller = player;
      td.required_subtype = 0x3a;
      td.allow_cancel = 0;
      instance = get_card_instance(player, card);
      result = can_target(&td);
      if ((result != 0) &&
          (result = new_pick_target(&td, "Select target Elemental.", 0, 0x80), result != 0))
      {
        add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                     COUNTER_P1_P1, 1);
      }
      instance->number_of_targets = '\0';
      result = count_permanents_by_type(player, TYPE_LAND);
      if (7 < result)
      {
        draw_a_card(player);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2399100; original cards/m20.c:2156. */
int card_pyroclastic_elemental(int player, int card, event_t event)
{
  /* Pyroclastic Elemental	|3|R|R
   * Creature - Elemental 5/4
   * |1|R|R: This creature deals 1 damage to target player. */

  int result;
  target_definition_t td;
  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    damage_target0(player, card, 1);
  }
  result =
      generic_activated_ability(player, card, event, 4, 1, 0, 0, 0, 2, 0, 0, &td, "TARGET_PLAYER");
  return result;
}

/* ManalinkEh 2022: 0x2399230; original cards/m20.c:2219. */
int card_risen_reef(int player, int card, event_t event)
{
  /* Risen Reef	|1|G|U
   * Creature - Elemental 1/1
   * Whenever this creature or another Elemental you control enters, look at the top card of your library. If it's a land card, you may put it onto the battlefield tapped. If you don't put the card onto the battlefield, put it into your hand. */

  int *library;
  int top_iid_or_chosen;
  int added_card;
  card_instance_t *instance;
  test_definition_t test;

  if ((((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (top_iid_or_chosen = is_humiliated(player, card, AL_TRIG), top_iid_or_chosen == 0))
  {
    default_test_definition(&test, 0x47);
    test.subtype = 0x3a;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {

    library = deck_ptr[player];
    top_iid_or_chosen = *library;
    if (top_iid_or_chosen != -1)
    {
      if ((player == 1) || (ai_is_speculating == 1))
      {
        top_iid_or_chosen = is_what(-1, top_iid_or_chosen, 1);
      }
      else
      {
        if (is_what(-1, top_iid_or_chosen, 1) == 0)
        {
          reveal_card_iid(player, card, *library);
          top_iid_or_chosen = *library;
          remove_card_from_deck(player, 0);
          add_card_to_hand(player, top_iid_or_chosen);
          return 0;
        }
        top_iid_or_chosen =
            reveal_card_optional_iid(player, card, *library, "Put on the battlefield tapped");
      }
      added_card = *library;
      remove_card_from_deck(player, 0);
      added_card = add_card_to_hand(player, added_card);
      if (top_iid_or_chosen != 0)
      {
        instance = get_card_instance(player, added_card);
        instance->state = instance->state | 0x10;
        put_into_play(player, added_card);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23993f0; original cards/m20.c:2261. */
int card_rotting_regisaur(int player, int card, event_t event)
{
  /* Rotting Regisaur	|2|B
   * Creature - Zombie Dinosaur 7/6
   * At the beginning of your upkeep, discard a card. */

  upkeep_trigger_ability(player, card, event, player);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    return 0;
  }
  discard(player, 0, player);
  return 0;
}

/* ManalinkEh 2022: 0x2399450; original cards/m20.c:2320. */
int card_scorch_spitter(int player, int card, event_t event)
{
  /* Scorch Spitter	|R
   * Creature - Elemental Lizard 1/1
   * Whenever this creature attacks, it deals 1 damage to the player or planeswalker it's attacking. */

  int tgt_card;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {

    tgt_card = planeswalker_its_attacking(player, card);
    if (tgt_card != -2)
    {
      damage_creature(1 - current_turn, tgt_card, 1, player, card);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23994e0; original cards/m20.c:2362. */
int card_shifting_ceratops(int player, int card, event_t event)
{
  /* Shifting Ceratops	|2|G|G
   * Creature - Dinosaur 5/4
   * This spell can't be countered.
   * Protection from blue (This creature can't be blocked, targeted, dealt damage, enchanted, or equipped by anything blue.)
   * |G: This creature gains your choice of reach, trample, or haste until end of turn. */

  enum
  {
    CHOICE_REACH = 1,
    CHOICE_TRAMPLE = 2,
    CHOICE_HASTE = 3
  };

  int choice;
  unsigned int current_keywords;
  unsigned int special_keywords;
  card_instance_t *instance;
  int32_t player_00;
  uint32_t haste_priority;
  int32_t card_00;
  uint32_t trample_priority;
  unsigned int stored_keywords;
  int mode_or_keywords;
  int state_or_special_keywords;
  uint32_t reach_priority;

  cannot_be_countered(player, card, event);
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
      if (event == EVENT_ACTIVATE)
      {

        mode_or_keywords = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 0,
                                                     1, 0, 0, 0, NULL, NULL);
        current_keywords = get_abilities(player, card, EVENT_ABILITIES, -1);
        stored_keywords = instance->targets[1].player;
        current_keywords =
            stored_keywords & (int)(stored_keywords ^ 0xffffffff) >> 0x1f | current_keywords;
        special_keywords = get_special_abilities(player, card);
        stored_keywords = instance->targets[1].card;
        if (((((current_keywords & 0x400) == 0) && (current_turn != player)) &&
             (current_phase == 0x16)) &&
            (state_or_special_keywords = blocking(player, card, EVENT_ACTIVATE),
             state_or_special_keywords == 0))
        {
          reach_priority = 10;
        }
        else
        {
          reach_priority = 1;
        }
        if ((((current_keywords & 0x80) == 0) && (current_phase == 0x18)) &&
            (state_or_special_keywords = is_unblocked(player, card),
             state_or_special_keywords == 0))
        {
          trample_priority = 10;
        }
        else
        {
          trample_priority = 1;
        }
        if (((((special_keywords | stored_keywords & (int)(stored_keywords ^ 0xffffffff) >> 0x1f) &
               4) == 0) &&
             (current_phase < 0x15)) &&
            ((current_turn == player &&
              (state_or_special_keywords = is_sick(player, card), state_or_special_keywords != 0))))
        {
          haste_priority = 10;
        }
        else
        {
          haste_priority = 1;
        }
        choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Reach", mode_or_keywords,
                             reach_priority, "Trample", mode_or_keywords, trample_priority, "Haste",
                             mode_or_keywords, haste_priority, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (2 < choice - 1U)
        {
          return 0;
        }
        if (charge_mana_for_activated_ability(player, card, 0, 0, 0, 1, 0, 0) == 0)
        {
          return 0;
        }
        stored_keywords = instance->targets[1].player;
        if ((int)stored_keywords < -1)
        {
          instance->targets[1].player = 0;
          stored_keywords = 0;
        }
        current_keywords = instance->targets[1].card;
        if ((int)current_keywords < -1)
        {
          current_keywords = 0;
        }
        if (choice == CHOICE_REACH)
        {
          current_keywords = current_keywords | 0x400;
        }
        else if (choice == CHOICE_TRAMPLE)
        {
          instance->targets[1].player = stored_keywords | 0x80;
        }
        else if (choice == CHOICE_HASTE)
        {
          current_keywords = current_keywords | 4;
        }
        instance->targets[1].card = current_keywords;
        return 0;
      }
    }
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      return 0;
    }
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    mode_or_keywords = instance->info_slot;
    if (mode_or_keywords == CHOICE_REACH)
    {
      card_00 = instance->parent_card;
      player_00 = instance->parent_controller;
      state_or_special_keywords = 0;
      mode_or_keywords = 0x400;
    }
    else if (mode_or_keywords == CHOICE_TRAMPLE)
    {
      card_00 = instance->parent_card;
      player_00 = instance->parent_controller;
      state_or_special_keywords = 0;
      mode_or_keywords = 0x80;
    }
    else
    {
      if (mode_or_keywords != CHOICE_HASTE)
      {
        return 0;
      }
      card_00 = instance->parent_card;
      player_00 = instance->parent_controller;
      state_or_special_keywords = 4;
      mode_or_keywords = 0;
    }
    pump_ability_until_eot_no_repeat(player_00, card_00, player_00, card_00, mode_or_keywords,
                                     state_or_special_keywords);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2399990; original cards/m20.c:2481. */
int card_sorin_imperious_bloodlord(int player, int card, event_t event)
{
  /* Sorin, Imperious Bloodlord	|2|B
   * Legendary Planeswalker - Sorin (4)
   * +1: Target creature you control gains deathtouch and lifelink until end of turn. If it's a Vampire, put a +1/+1 counter on it.
   * +1: You may sacrifice a Vampire. When you do, Sorin deals 3 damage to any target and you gain 3 life.
   * -3: You may put a Vampire creature card from your hand onto the battlefield. */

  enum
  {
    CHOICE_PUMP = 1,
    CHOICE_DAMAGE = 2,
    CHOICE_CHEAT = 3
  };

  card_instance_t *instance;
  int result;
  uint32_t sacrifice_priority;
  target_definition_t *damage_target;
  int vampire_priority;
  target_definition_t *creature_target;
  target_definition_t td1;
  target_definition_t td2;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td1, 2);
    td1.allowed_controller = player;
    td1.preferred_controller = player;
    default_target_definition(player, card, &td2, 0x102);
    td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    new_default_test_definition(&test, 2, "Select a Vampire creture card.");
    test.subtype = 0xcd;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      instance->targets[0].card = -1;
      instance->targets[0].player = -1;
      vampire_priority = count_subtype_in_hand(player, SUBTYPE_VAMPIRE);
      if (vampire_priority != 0)
      {
        vampire_priority = 0xf;
      }
      result = count_subtype(player, 2, 0xcd);
      sacrifice_priority = 0;
      if (result != 0)
      {
        sacrifice_priority = 10;
      }
      result = can_target(&td1);
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Pump a creature",
                           result, 10, 1, "Sac for damage", 1, sacrifice_priority, 1,
                           "Put a Vampire into play", 1, vampire_priority, 0xfffffffd, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_PUMP)
      {
        new_pick_target(&td1, "TARGET_CREATURE_YOU_CONTROL", 0, 1);
      }
      return planeswalker(player, card, event, 4);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 4);
    default_target_definition(player, card, &td1, 2);
    td1.allowed_controller = player;
    td1.preferred_controller = player;
    default_target_definition(player, card, &td2, 0x102);
    td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    new_default_test_definition(&test, 2, "Select a Vampire creture card.");
    test.subtype = 0xcd;
    instance = get_card_instance(player, card);
  }
  creature_target = &td1;
  damage_target = &td2;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    result = instance->info_slot;
    if (result == CHOICE_PUMP)
    {
      result = valid_target(creature_target);
      if (result != 0)
      {
        pump_ability_until_eot_no_repeat(instance->parent_controller, instance->parent_card,
                                         instance->targets[0].player, instance->targets[0].card, 0,
                                         0x202);
        result =
            has_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_VAMPIRE);
        if (result != 0)
        {
          add_counters(instance->parent_controller, instance->parent_card,
                       instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
        }
      }
    }
    else if (result == CHOICE_DAMAGE)
    {
      result = new_sacrifice(player, card, player, 0, &test);
      if (result == 0)
      {
        return 0;
      }
      result = pick_target(damage_target, "TARGET_ANY");
      if (result != 0)
      {
        damage_target0(player, card, 3);
        gain_life(player, 3);
      }
    }
    else if (result == CHOICE_CHEAT)
    {
      new_global_tutor(player, player, 4, 4, 0, 2, &test);
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x2394640; original cards/m20.c:2606. */
static char *is_legendary_by_iid(int who_chooses, int player, int card)
{
  int result;
  char *requirement_error;
  result = is_legendary(player, card);
  requirement_error = NULL;
  if (result == 0)
  {
    requirement_error = "must be a Legendary spell";
  }
  return requirement_error;
}

/* ManalinkEh 2022: 0x2399e40; original cards/m20.c:2613. */
int card_tales_end(int player, int card, event_t event)
{
  /* Tale's End	|1|U
   * Instant
   * Counter target activated ability, triggered ability, or legendary spell. */

  card_instance_t *instance;
  int result;
  target_definition_t td_spell;

  instance = get_card_instance(player, card);
  counterspell_target_definition(player, card, &td_spell, 0);
  td_spell.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td_spell.extra = (intptr_t)is_legendary_by_iid;
  if (event == EVENT_CAN_CAST)
  {
    result = counterspell(player, card, EVENT_CAN_CAST, &td_spell, 0);
    if (result != 0)
    {
      instance->info_slot = 1;
      return 99;
    }
    result = can_counter_activated_ability(player, card, EVENT_CAN_CAST, NULL);
    if (result != 0)
    {
      instance->info_slot = 2;
      return 99;
    }
    result = can_counter_triggered_ability(player, card, EVENT_CAN_CAST, NULL);
    if (result != 0)
    {
      instance->info_slot = 3;
      return 99;
    }
  }
  else
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        result = instance->info_slot;
        instance->number_of_targets = '\0';
        if (result == 1)
        {
          counterspell(player, card, EVENT_CAST_SPELL, &td_spell, 0);
          return 0;
        }
        if (result != 2)
        {
          if (result != 3)
          {
            return 0;
          }
          cast_counter_triggered_ability(player, card, 0);
          return 0;
        }
        cast_counter_activated_ability(player, card, 0);
        return 0;
      }
      return counterspell(player, card, event, &td_spell, 0);
    }
    if (event != EVENT_RESOLVE_SPELL)
      return counterspell(player, card, event, &td_spell, 0);
    result = instance->info_slot;
    if (result == 1)
    {
      counterspell(player, card, EVENT_RESOLVE_SPELL, &td_spell, 0);
      return 0;
    }
    if (result == 2)
    {
      result = resolve_counter_activated_ability(player, card, NULL, 0);
      if (result != 0)
      {
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
      result = instance->info_slot;
    }
    if ((result == 3) &&
        (result = resolve_counter_triggered_ability(player, card, NULL, 0), result != 0))
    {
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x239a0e0; original cards/m20.c:2694. */
int card_thunderkin_awakener(int player, int card, event_t event)
{
  /* Thunderkin Awakener	|1|R
   * Creature - Elemental Shaman 1/2
   * Haste
   * Whenever this creature attacks, choose target Elemental creature card in your graveyard with toughness less than this creature's toughness. Return that card to the battlefield tapped and attacking. Sacrifice it at the beginning of the next end step. */

  xtrigger_t trigger;
  int result;
  test_definition_t this_test;

  store_attackers(player, card, event, 0, player, card, NULL);
  trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    trigger = xtrigger_impl_value_dont_use_directly;
  }
  if ((((trigger == XTRIGGER_ATTACKING) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    result = graveyard_has_shroud(player);
    if (result != 0)
    {
      return 0;
    }
    default_test_definition(&this_test, 2);
    this_test.toughness = get_toughness(player, card);
    this_test.toughness_flag = 3;
    result = new_special_count_grave(player, &this_test);
    if (result == 0)
    {
      return 0;
    }
    result = duh_mode(player);
    dat_ability(player, card, event, (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
  }
  if (event == EVENT_DAT_ABILITY)
  {

    result = get_toughness(player, card);
    default_test_definition(&this_test, 2);
    scnprintf(this_test.message, 100,
              "Select an Elemental creature card with toughness %d or less.", result + -1);
    this_test.toughness_flag = 3;
    this_test.toughness = result;
    result = new_special_count_grave(player, &this_test);
    if (result != 0)
    {
      result = new_global_tutor(player, player, 2, 8, 1, 2, &this_test);
      create_targetted_legacy_effect(player, card, haste_and_sacrifice_eot, player, result);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x239a2b0; original cards/m20.c:2734. */
int card_tomebound_lich(int player, int card, event_t event)
{
  /* Tomebound Lich	|1|U|B
   * Creature - Zombie Wizard 1/3
   * Deathtouch (Any amount of damage this deals to a creature is enough to destroy it.)
   * Lifelink (Damage dealt by this creature also causes you to gain that much life.)
   * Whenever this creature enters or deals combat damage to a player, draw a card, then discard a card. */

  int result;
  deathtouch(player, card, event);
  lifelink(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if ((event != EVENT_ETB) &&
      (result = damage_dealt_by_me(player, card, event,
                                   DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE),
       result == 0))
  {
    return 0;
  }
  draw_a_card(player);
  discard(player, 0, player);
  return 0;
}

/* ManalinkEh 2022: 0x2394670; original cards/m20.c:2787. */
int vos_legacy(int player, int card, event_t event)
{
  unsigned int source_colors;
  char protection_event;

  if (event == EVENT_CAST_SPELL && card != affected_card && player == affected_card_controller)
    state_untargettable(player, affected_card, 1);
  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && card == affected_card && player == affected_card_controller &&
      reason_for_trigger_controller == affected_card_controller &&
      check_state(trigger_cause_controller, trigger_cause, 0x100000))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result |= 2;
      return 0;
    }
    if (event == EVENT_RESOLVE_TRIGGER)
    {
      state_untargettable(trigger_cause_controller, trigger_cause, 0);
      return 0;
    }
  }
  if (event == EVENT_STATIC_EFFECTS)
  {
    player_bits[player] = player_bits[player] | 0x4000000;
  }
  else if (event == EVENT_PROT_MISC)
  {
    protection_event = (char)((unsigned int)event_result >> 0x18);
    if ((protection_event == '\x06') ||
        ((((protection_event == '\x05' && (affected_card_controller == player)) &&
           (affected_card == -1)) &&
          ((1 - player == attacking_card_controller &&
            (source_colors = get_color(1 - player, attacking_card), (source_colors & 6) != 0))))))
    {
      event_result = event_result | 1;
      return 0;
    }
  }
  else if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2394850; original cards/m20.c:2819. */
static int hexpr_ub_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  unsigned int source_color;
  char protection_event;

  attached_gets_sp_kw2(player, card, event, SP_KEYWORD2_GRANTED_PROT_MISC);
  instance = get_card_instance(player, card);
  if (event == EVENT_PROT_MISC)
  {
    protection_event = (char)((unsigned int)event_result >> 0x18);
    if ((protection_event == '\x06') ||
        ((((protection_event == '\x05' && (attacking_card_controller != player)) &&
           (instance->damage_target_card == affected_card)) &&
          ((instance->damage_target_player == affected_card_controller &&
            (source_color = get_color(attacking_card_controller, attacking_card),
             (source_color & 6) != 0))))))
    {
      event_result = event_result | 1;
      return 0;
    }
  }
  else if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x239a350; original cards/m20.c:2831. */
int card_veil_of_summer(int player, int card, event_t event)
{
  /* Veil of Summer	|G
   * Instant
   * Draw a card if an opponent has cast a blue or black spell this turn. Spells you control can't be countered this turn. You and permanents you control gain hexproof from blue and from black until end of turn. (You and they can't be the targets of blue or black spells or abilities your opponents control.) */

  int permanent_card;
  color_t required_color;
  card_instance_t *instance;

  if (event == EVENT_CAN_CAST)
    return basic_spell(player, card, event);
  if (event == EVENT_CAST_SPELL)
  {
    if ((card != affected_card) || (player != affected_card_controller))
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

  required_color = get_sleighted_color(player, card, 2);
  bool draw = get_stormcolor_count(1 - player, required_color) != 0;
  if (!draw)
  {
    required_color = get_sleighted_color(player, card, 1);
    draw = get_stormcolor_count(1 - player, required_color) != 0;
  }
  if (draw)
    draw_a_card(player);
  create_legacy_effect(player, card, vos_legacy);
  for (permanent_card = 0; permanent_card < active_cards_count[player]; ++permanent_card)
  {
    instance = in_play(player, permanent_card);
    if (instance && is_what(player, permanent_card, 0x47))
      create_targetted_legacy_effect(player, card, hexpr_ub_legacy, player, permanent_card);
  }
  kill_card(player, card, KILL_DESTROY);
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x239a4f0; original cards/m20.c:2861. */
int card_vilis_broker_of_blood(int player, int card, event_t event)
{
  /* Vilis, Broker of Blood	|5|B|B|B
   * Legendary Creature - Demon 8/8
   * Flying
   * |B, Pay 2 life: Target creature gets -1/-1 until end of turn.
   * Whenever you lose life, draw that many cards. (Damage causes loss of life.) */

  card_instance_t *instance;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (event == EVENT_ACTIVATE)
  {
    get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (valid_target(&td) != 0))
    {
      pump_until_eot_merge_previous(player, instance->parent_card, instance->targets[0].player,
                                    instance->targets[0].card, -1, -1);
    }
  }
  return generic_activated_ability(player, card, event, 4, 0, 1, 0, 0, 0, 0, 2, &td,
                                     "TARGET_CREATURE");
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x239a660; original cards/m20.c:2897. */
int card_vivien_arkbow_ranger(int player, int card, event_t event)
{
  /* Vivien, Arkbow Ranger	|1|G|G|G
   * Legendary Planeswalker - Vivien (4)
   * +1: Distribute two +1/+1 counters among up to two target creatures. They gain trample until end of turn.
   * -3: Target creature you control deals damage equal to its power to target creature or planeswalker.
   * -5: You may reveal a creature card you own from outside the game and put it into your hand. */

  enum
  {
    CHOICE_COUNTERS = 1,
    CHOICE_BITE = 2,
    CHOICE_WISH = 3
  };

  int i;
  card_instance_t *instance;
  int result_or_mode;
  int source_player;
  int amt;
  unsigned int damage_available;
  target_definition_t *damage_target;
  target_definition_t *counter_target;
  target_definition_t *source_target;
  target_definition_t td;
  target_definition_t td_control;
  target_definition_t td_cp;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    default_target_definition(player, card, &td_control, 2);
    td_control.preferred_controller = player;
    td_control.allowed_controller = player;
    default_target_definition(player, card, &td_cp, 0x102);
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      instance->targets[0].card = -1;
      instance->targets[0].player = -1;
      result_or_mode = can_target(&td_control);
      damage_available = 0;
      if (result_or_mode != 0)
      {
        result_or_mode = can_target(&td_cp);
        damage_available = (unsigned int)(result_or_mode != 0);
      }
      result_or_mode = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                                   "Add two +1/+1 counters", 1, 10, 1, "A creature deals damage",
                                   damage_available, 10, 0xfffffffd, "Living Wish", 1, 0x14,
                                   0xfffffffb, 0, 0, 0, 0);
      if (result_or_mode == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result_or_mode == CHOICE_COUNTERS)
      {
        result_or_mode = target_available(player, card, &td);
        if (result_or_mode == 1)
        {
          pick_target(&td, "ASHNODS_BATTLEGEAR");
        }
        else
        {

          result_or_mode = new_pick_target(&td, "ASHNODS_BATTLEGEAR", 0, 0);
          if ((result_or_mode == 0) ||
              (result_or_mode = new_pick_target(&td, "ASHNODS_BATTLEGEAR", 1, 0),
               result_or_mode == 0))
          {
            spell_fizzled = 1;
          }
        }
      }
      else if ((result_or_mode == CHOICE_BITE) &&
               ((result_or_mode = new_pick_target(&td_control, "ASHNODS_BATTLEGEAR", 0, 1),
                 result_or_mode == 0 || (result_or_mode = new_pick_target(
                                             &td_cp, "TARGET_CREATURE_OR_PLANESWALKER", 1, 1),
                                         result_or_mode == 0))))
      {
        instance->number_of_targets = '\0';
      }
      return planeswalker(player, card, event, 4);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 4);
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    default_target_definition(player, card, &td_control, 2);
    td_control.preferred_controller = player;
    td_control.allowed_controller = player;
    default_target_definition(player, card, &td_cp, 0x102);
    instance = get_card_instance(player, card);
  }
  source_target = &td_control;
  counter_target = &td;
  damage_target = &td_cp;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    result_or_mode = instance->info_slot;
    if (result_or_mode == CHOICE_COUNTERS)
    {

      if (instance->number_of_targets == '\x01')
      {
        if (valid_target(counter_target) != 0)
        {
          add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                       COUNTER_P1_P1, 2);
          pump_ability_until_eot_no_repeat(instance->parent_controller, instance->parent_card,
                                           instance->targets[0].player, instance->targets[0].card,
                                           0x80, 0);
        }
      }
      else
      {
        i = 0;
        do
        {
          if (validate_target(player, card, counter_target, i) != 0)
          {
            add_counters(player, card, instance->targets[i].player, instance->targets[i].card,
                         COUNTER_P1_P1, 1);
            pump_ability_until_eot_no_repeat(instance->parent_controller, instance->parent_card,
                                             instance->targets[i].player, instance->targets[i].card,
                                             0x80, 0);
          }
          i = i + 1;
        } while (i != 2);
      }
    }
    else if (result_or_mode == CHOICE_BITE)
    {
      result_or_mode = valid_target(source_target);
      if ((result_or_mode != 0) &&
          (result_or_mode = validate_target(player, card, damage_target, 1), result_or_mode != 0))
      {
        result_or_mode = instance->targets[0].card;
        source_player = instance->targets[0].player;
        amt = get_power(source_player, result_or_mode);
        damage_creature(instance->targets[1].player, instance->targets[1].card, amt, source_player,
                        result_or_mode);
      }
    }
    else if (result_or_mode == CHOICE_WISH)
    {
      default_test_definition(&test, 2);
      generic_wish_effect(player, card, &test, 0x4fb7);
    }
  }
  return planeswalker(player, card, event, 4);
}
#endif

/* ManalinkEh 2022: 0x239acb0; original cards/m20.c:3014. */
int card_voracious_hydra(int player, int card, event_t event)
{
  /* Voracious Hydra	|X|G|G
   * Creature - Hydra 0/1
   * Trample
   * This creature enters with X +1/+1 counters on it.
   * When this creature enters, choose one -
   * * Double the number of +1/+1 counters on this creature.
   * * This creature fights target creature you don't control. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event != EVENT_CAST_SPELL)
  {
    etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (event == EVENT_ETB)
    {
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = 1 - player;
      td.allow_cancel = 0;
      td.preferred_controller = td.allowed_controller;
      result = can_target(&td);
      result =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_AUTOCHOOSE_IF_1,
                      "Double +1/+1 counters", 1, 10, "Fight", result, 8, 0, 0, 0, 0);
      if (result == 1)
      {
        result = count_counters(player, card, COUNTER_P1_P1);
        add_counters(player, card, player, card, COUNTER_P1_P1, result);
      }
      else
      {
        result = can_target(&td);
        if (result != 0)
        {
          result = new_pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS", 0, 0);
          if (result != 0)
          {
            instance = get_card_instance(player, card);
            result = instance->targets[0].card;
            instance = get_card_instance(player, card);
            fight(player, card, instance->targets[0].player, result);
          }
        }
      }
    }
    return 0;
  }
  if ((card == affected_card) && (player == affected_card_controller))
  {
    result = is_token(player, card);
    if (result == 0)
    {
      generic_spell(player, card, EVENT_CAST_SPELL, 4, NULL, NULL, 0, NULL);
      instance = get_card_instance(player, card);
      enters_the_battlefield_with_counters(player, card, EVENT_CAST_SPELL, COUNTER_P1_P1,
                                           instance->info_slot);
    }
  }
  etb_ability(player, card, EVENT_CAST_SPELL, RESOLVE_TRIGGER_MANDATORY);
  return 0;
}

/* ManalinkEh 2022: 0x239af40; original cards/m20.c:3124. */
int card_yarok_the_desecrated(int player, int card, event_t event)
{
  /* Yarok, the Desecrated	|2|B|G|U
   * Legendary Creature - Elemental Horror 3/5
   * Deathtouch, lifelink
   * If a permanent entering causes a triggered ability of a permanent you control to trigger, that ability triggers an additional time. */

  check_legend_rule(player, card, event);
  deathtouch(player, card, event);
  lifelink(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    card_counts[player][CCOUNTS_YAROK_THE_DESECRATED] = card_counts[player][CCOUNTS_YAROK_THE_DESECRATED] + '\x01';
  }
  return 0;
}
