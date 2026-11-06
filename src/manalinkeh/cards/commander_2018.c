#include "manalink.h"

static int effect_estrids_invocation(int player, int card, event_t event);

static int amaug_leg_part(int player, int card, event_t event);

static int effect_xantcha(int player, int card, event_t event);

static int vedalken_humiliation(int player, int card, event_t event);

#ifndef SHANDALAR

static int is_enchanted_or_enchantment_creature(int unused1, int unused2, int player, int card);
static char *is_aura_or_equipment(int who_chooses, int player, int card);

#endif

/* ManalinkEh 2022: 0x2560360; original cards/commander_2018.c:3. */
static int amaug_leg(int player, int card, event_t event)
{
  if (eot_trigger(player, card, event))
  {
    kill_card(player, card, KILL_DESTROY);
  }
  if (((event != EVENT_ACTIVATE) && (1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION))) &&
      (event != EVENT_CLEANUP))
  {
    return 0;
  }
  return amaug_leg_part(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x255fd50; original cards/commander_2018.c:3. */
static int amaug_leg_part(int player, int card, event_t event)
{
  card_instance_t *leg = get_card_instance(player, card);
  int playable[2][8];
  test_definition_t this_test;
  if (event == EVENT_CAN_ACTIVATE)
  {
    type_t used_types = leg->targets[0].player;
    if (used_types == ~TYPE_NONE)
    {
      leg->targets[0].player = 0;
      used_types = TYPE_NONE;
    }
    int any_playable = 0;
    for (int i = 0; i < 7; ++i)
    {
      int iid = leg->targets[i].card;
      if (iid != -1)
      {
        type_t raw_types = get_type_with_iid(-1, -1, iid);
        type_t types = raw_types & (TARGET_TYPE_PLANESWALKER | TYPE_ARTIFACT | TYPE_SPELL |
                                   TYPE_ENCHANTMENT | TYPE_CREATURE);
        int tribal = is_tribal(-1, iid);
        if (raw_types & TYPE_INTERRUPT)
          types |= TYPE_INSTANT;
        if (tribal)
          types |= TYPE_EFFECT;
        if (types != TYPE_NONE && (types | used_types) != used_types)
        {
          int available = can_legally_play_iid_now(player, iid, EVENT_CAN_ACTIVATE);
          if (available == 99)
            return 99;
          if (available != 0)
            any_playable = 1;
        }
      }
    }
    return any_playable;
  }
  if (event != EVENT_ACTIVATE)
    return 0;

  type_t used_types = leg->targets[0].player;
  int playable_count = 0;
  for (int i = 0; i < 7; ++i)
  {
    int iid = leg->targets[i].card;
    if (iid != -1 && check_rfg(player, (unsigned int)cards_data[iid].id) != 0)
    {
      type_t raw_types = get_type_with_iid(-1, -1, iid);
      type_t allowed_types = raw_types & (TARGET_TYPE_PLANESWALKER | TYPE_ARTIFACT | TYPE_SPELL |
                                        TYPE_ENCHANTMENT | TYPE_CREATURE);
      if (raw_types & TYPE_INTERRUPT)
        allowed_types |= TYPE_INSTANT;
      int tribal = is_tribal(-1, iid);
      if (tribal)
        allowed_types |= TYPE_EFFECT;
      if (allowed_types != TYPE_NONE && (allowed_types | used_types) != used_types &&
          can_legally_play_iid_now(player, iid, EVENT_ACTIVATE) != 0)
      {
        playable[0][playable_count] = iid;
        playable[1][playable_count] = i;
        ++playable_count;
      }
    }
  }
  new_default_test_definition(&this_test, 0xff, "Select a card to cast.");
  int selected = select_card_from_zone(player, player, playable[0], playable_count,
                                       0, 1, -1, &this_test);
  if (selected == -1)
  {
    spell_fizzled = 1;
    return 0;
  }
  type_t types = get_type_with_iid(-1, -1, playable[0][selected]);
  unsigned int type_count = num_bits_set(types);
  bool choose_type = type_count >= 2;
  if (!choose_type)
  {
    int tribal = is_tribal(-1, playable[0][selected]);
    choose_type = tribal != 0 && (used_types & TYPE_EFFECT) == TYPE_NONE;
    if (!choose_type)
      leg->targets[0].player |= types;
  }
  if (choose_type)
  {
    unsigned int tribal_available = (types & TYPE_EFFECT) != TYPE_NONE &&
                                   (used_types & TYPE_EFFECT) == TYPE_NONE;
    unsigned int sorcery_available = (types & TYPE_SORCERY) != TYPE_NONE &&
                                    (used_types & TYPE_SORCERY) == TYPE_NONE;
    type_t planeswalker_available = (type_t)((types & TARGET_TYPE_PLANESWALKER) != TYPE_NONE &&
                                            (used_types & TARGET_TYPE_PLANESWALKER) == TYPE_NONE);
    type_t instant_available = (type_t)((types & TYPE_INSTANT) != TYPE_NONE &&
                                      (used_types & TYPE_INSTANT) == TYPE_NONE);
    type_t enchantment_available = (type_t)((types & TYPE_ENCHANTMENT) != TYPE_NONE &&
                                          (used_types & TYPE_ENCHANTMENT) == TYPE_NONE);
    unsigned int creature_available = (types & TYPE_CREATURE) != TYPE_NONE &&
                                     (used_types & TYPE_CREATURE) == TYPE_NONE;
    unsigned int artifact_available = (types & TYPE_ARTIFACT) != TYPE_NONE &&
                                     (used_types & TYPE_ARTIFACT) == TYPE_NONE;
    int choice = dialog_impl(
        player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
        DLG_OMIT_ILLEGAL, DLG_AUTOCHOOSE_IF_1, "Artifact", artifact_available, 1,
        "Creature", creature_available, 1, "Enchantment", enchantment_available, 1,
        "Instant", instant_available, 1, "Planeswalker", planeswalker_available, 1,
        "Sorcery", sorcery_available, 1, "Tribal", tribal_available, 1, 0, 0, 0, 0);
    switch (choice)
    {
    default:
      spell_fizzled = 1;
      return 0;
    case 1:
      leg->targets[0].player |= TYPE_ARTIFACT;
      break;
    case 2:
      leg->targets[0].player |= TYPE_CREATURE;
      break;
    case 3:
      leg->targets[0].player |= TYPE_ENCHANTMENT;
      break;
    case 4:
      leg->targets[0].player |= TYPE_INSTANT;
      break;
    case 5:
      leg->targets[0].player |= TARGET_TYPE_PLANESWALKER;
      break;
    case 6:
      leg->targets[0].player |= TYPE_SORCERY;
      break;
    case 7:
      leg->targets[0].player |= TYPE_EFFECT;
    }
  }
  play_card_in_exile_for_free(player, player,
                              (unsigned int)cards_data[playable[0][selected]].id);
  cant_be_responded_to = 1;
  return 0;
}
#endif

/* ManalinkEh 2022: 0x25604e0; original cards/commander_2018.c:123. */
int card_aminatous_augury(int player, int card, event_t event)
{
  /* Aminatou's Augury	|6|U|U
   * Sorcery
   * Exile the top eight cards of your library. You may put a land card from among them onto the battlefield. Until end of turn, for each nonland card type, you may cast a card of that type from among the exiled cards without paying its mana cost. */

  int *deck_end;
  int exile_count;
  int selected_or_copied_card;
  int cage_present;
  card_instance_t *instance;
  int32_t *stored_iid_cursor;
  int *deck_cursor;
  bool play_land;
  int selected_land_iid;
  test_definition_t test;

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
  if (event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET)
  {
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
    return basic_spell(player, card, event);
  }
  if (event != EVENT_RESOLVE_SPELL)
    return basic_spell(player, card, event);

  exile_count = count_deck(player);
  if (exile_count < 1)
  {
    return 0;
  }
  if (8 < exile_count)
  {
    exile_count = 8;
  }
  new_default_test_definition(&test, 1, "Select a land card to put onto the battlefield.");
  test.create_minideck = (unsigned short)exile_count;
  selected_or_copied_card =
      select_card_from_zone(player, player, deck_ptr[player], exile_count, 0, 1, -1, &test);
  bool create_storage = true;
  play_land = false;
  selected_land_iid = -1;
  if (selected_or_copied_card != -1)
  {
    selected_land_iid = deck_ptr[player][selected_or_copied_card];
    reveal_card_iid(player, card, selected_land_iid);
    cage_present = grafdiggers_cages(player, TYPE_CREATURE, TUTOR_FROM_DECK);
    if (!(cage_present != 0 && is_what(-1, deck_ptr[player][selected_or_copied_card], 2)))
    {
      remove_card_from_deck(player, selected_or_copied_card);
      play_land = selected_land_iid != -1;
      --exile_count;
      create_storage = exile_count != 0;
    }
  }
  if (create_storage)
  {
    instance = get_card_instance(player, create_legacy_activate(player, card, amaug_leg));
    if (exile_count > 0)
    {
      deck_cursor = deck_ptr[player];
      stored_iid_cursor = &instance->targets[0].card;
      deck_end = deck_cursor + exile_count;
      do
      {
        selected_or_copied_card = *deck_cursor;
        ++deck_cursor;
        *stored_iid_cursor = selected_or_copied_card;
        stored_iid_cursor += 2;
      } while (deck_end != deck_cursor);
    }
    rfg_top_n_cards_of_deck(player, exile_count);
  }
  if (play_land)
    set_special_flags(player, add_card_to_hand(player, selected_land_iid), 0x10000);
  kill_card(player, card, KILL_DESTROY);
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x25607b0; original cards/commander_2018.c:177. */
int card_aminatou_the_fateshifter(int player, int card, event_t event)
{
  /* Aminatou, the Fateshifter	|W|U|B
   * Legendary Planeswalker - Aminatou (3)
   * +1: Draw a card, then put a card from your hand on top of your library.
   * -1: Exile another target permanent you own, then return it to the battlefield under your control.
   * -6: Choose left or right. Each player gains control of all nonland permanents other than ~ controlled by the next player in the chosen direction.
   * ~ can be your commander. */

  enum
  {
    CHOICE_DRAW = 1,
    CHOICE_BLINK = 2,
    CHOICE_TRADE = 3
  };

  int choice;
  card_instance_t *instance;
  int turn_or_result;
  card_instance_t *scanned_instance;
  int scan_player_or_permanent;
  int card_00;
  int pass_or_marked_index;
  int i;
  int permanent_count;
  int mc[2];
  target_definition_t td;
  test_definition_t this_test;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0x47);
    td.allowed_controller = player | 4;
    td.special = TARGET_SPECIAL_NOT_ME;
    default_test_definition(&this_test, 0x47);
    this_test.type = 1;
    this_test.type_flag = 1;
    this_test.not_me = 1;
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      turn_or_result = check_battlefield_for_special_card(player, card, 1 - player, 4, &this_test);
      pass_or_marked_index =
          check_battlefield_for_special_card(player, card, player, 4, &this_test);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                           "Draw and put a card on your library", 1, 1, 1, "Blink", can_target(&td),
                           4, 0xffffffff, "Trade permanents", 1,
                           turn_or_result - pass_or_marked_index, 0xfffffffa, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_BLINK)
      {
        new_pick_target(&td, "Select a permanent you own.", 0, 0x81);
      }
      return planeswalker(player, card, event, 3);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 3);
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0x47);
    td.allowed_controller = player | 4;
    td.special = TARGET_SPECIAL_NOT_ME;
    default_test_definition(&this_test, 0x47);
    this_test.type = 1;
    this_test.type_flag = 1;
    this_test.not_me = 1;
  }
  this_test.not_me = 1;
  this_test.type_flag = 1;
  this_test.type = 1;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    turn_or_result = instance->info_slot;
    if (turn_or_result == CHOICE_DRAW)
    {

      draw_a_card(player);
      new_default_test_definition(&test, 0xff, "Select a card to put on top.");

      if ((hand_count[player] != 0) &&
          (turn_or_result = new_select_a_card(player, player, 4, 1, 3, -1, &test),
           turn_or_result != -1))
      {
        put_on_top_of_deck(player, turn_or_result);
      }
    }
    else if (turn_or_result == CHOICE_BLINK)
    {
      if (valid_target(&td) != 0)
      {
        blink_effect(instance->targets[0].player, instance->targets[0].card, NULL);
      }
    }
    else if (turn_or_result == CHOICE_TRADE)
    {
      int marked[2][100];

      mc[0] = 0;
      mc[1] = 0;
      turn_or_result = current_turn;
      for (pass_or_marked_index = 0; pass_or_marked_index < 2; ++pass_or_marked_index)
      {
        scan_player_or_permanent = pass_or_marked_index == 0 ? turn_or_result : 1 - turn_or_result;
        for (card_00 = active_cards_count[scan_player_or_permanent] - 1; card_00 >= 0; --card_00)
        {
          scanned_instance = in_play(scan_player_or_permanent, card_00);
          if ((((scanned_instance != NULL) &&
                (turn_or_result = check_state(scan_player_or_permanent, card_00, 0x100000),
                 turn_or_result == 0)) &&
               (turn_or_result = is_what(scan_player_or_permanent, card_00, 0x47),
                turn_or_result != 0)) &&
              ((turn_or_result =
                    new_make_test_in_play(scan_player_or_permanent, card_00, -1, &this_test),
                turn_or_result != 0 && ((instance->parent_controller != scan_player_or_permanent ||
                                         (instance->parent_card != card_00))))))
          {
            turn_or_result = mc[scan_player_or_permanent];
            mc[scan_player_or_permanent] = turn_or_result + 1;
            marked[scan_player_or_permanent][turn_or_result] = card_00;
          }
          turn_or_result = current_turn;
        }
      }
      for (i = 0; i < 2; ++i)
      {
        if (i != 0)
          turn_or_result = 1 - turn_or_result;
        permanent_count = mc[turn_or_result];
        for (pass_or_marked_index = 0; pass_or_marked_index < permanent_count;
             ++pass_or_marked_index)
        {
          scan_player_or_permanent = marked[turn_or_result][pass_or_marked_index];
          if (instance->parent_controller == turn_or_result)
            give_control(turn_or_result, instance->parent_card, turn_or_result,
                         scan_player_or_permanent);
          else
            gain_control_permanently(instance->parent_controller, turn_or_result,
                                     scan_player_or_permanent);
        }
        turn_or_result = current_turn;
      }
    }
  }
  return planeswalker(player, card, event, 3);
}

/* ManalinkEh 2022: 0x2560da0; original cards/commander_2018.c:283. */
int card_ancient_stone_idol(int player, int card, event_t event)
{
  /* Ancient Stone Idol	|10
   * Artifact Creature - Golem 12/12
   * Flash
   * This spell costs |1 less to cast for each attacking creature.
   * Trample
   * When ~ dies, create a 6/12 colorless Construct artifact creature token with trample. */

  int result;
  token_generation_t token;
  if (event == EVENT_MODIFY_COST)
  {
    result = count_attackers(current_turn);
    COST_COLORLESS = COST_COLORLESS - (char)result;
  }
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    default_token_definition(player, card, CARD_ID_CONSTRUCT, &token);
    token.pow = 6;
    token.tou = 0xc;
    token.key_plus = 0x80;
    generate_token(&token);
  }
  result = flash(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2560e50; original cards/commander_2018.c:305. */
int card_arixmethes_slumbering_isle(int player, int card, event_t event)
{
  /* Arixmethes, Slumbering Isle	|2|G|U
   * Legendary Creature - Kraken 12/12
   * ~ enters the battlefield tapped with five slumber counters on it.
   * As long as ~ has a slumber counter on it, it's a land. (It's not a creature.)
   * Whenever you cast a spell, you may remove a slumber counter from ~.
   * |T: Add |G|U. */

  unsigned int regen_flags;
  int result;
  card_instance_t *instance;
  unsigned char land_types;
  int land_type_iid;

  check_legend_rule(player, card, event);
  comes_into_play_tapped(player, card, event);
  enters_the_battlefield_with_counters(player, card, event, COUNTER_SLUMBER, 5);
  if (event == EVENT_CHANGE_TYPE && (instance = in_play(player, card), instance != NULL) &&
      card == affected_card && player == affected_card_controller &&
      (land_can_be_played & 0x20000U) == 0)
  {
    result = count_counters(player, card, COUNTER_SLUMBER);
    if (result != 0)
    {

      result = instance->internal_card_id;
      land_types = cards_data[result].type & 0xfd | 1;
      if (cards_data[result].type == land_types)
      {
        return 0;
      }
      if ((instance->targets[7].player != result) ||
          (land_type_iid = instance->dummy3, cards_data[land_type_iid].type != land_types))
      {

        land_type_iid = create_a_card_type(result);
        if (land_type_iid == -1)
        {
          return 0;
        }
        cards_data[land_type_iid].type = land_types;
        instance->dummy3 = land_type_iid;
        regen_flags = instance->regen_status;
        instance->targets[7].player = result;
        instance->regen_status = regen_flags | 0x1000000;
      }
      event_result = land_type_iid;
    }
  }
  if (trigger_condition == TRIGGER_SPELL_CAST && card == affected_card && player == affected_card_controller)
  {
    if ((trigger_cause != card) &&
        (result = count_counters(player, card, COUNTER_SLUMBER), result != 0))
    {
      result = duh_mode(player);
      result = new_specific_spell_played(player, card, event, player, (result != 0) + 1, NULL);
      if (result != 0)
      {
        instance = in_play(player, card);
        remove_counters(player, card, player, card, COUNTER_SLUMBER, 1);
        instance->regen_status = instance->regen_status | 0x1000000;
        get_abilities(player, card, EVENT_CHANGE_TYPE, -1);
        result = is_what(player, card, 1);
        if (result == 0)
        {
          play_sound_effect(WAV_CHANGET);
        }
      }
    }
  }
  result = is_what(player, card, 1);
  if (result == 0)
  {
    return mana_producing_creature_multi(player, card, event, 0, 0, 0, 1, 1, 0, 0);
  }
  return two_mana_land(player, card, event, COLOR_GREEN, COLOR_BLUE);
}

/* Bloodtracker	|3|B
 * Creature - Vampire Wizard 2/2
 * Flying
 * |B, Pay 2 life: Put a +1/+1 counter on ~.
 * When ~ leaves the battlefield, draw a card for each +1/+1 counter on it. */

/* Boreas Charger	|2|W
 * Creature - Pegasus 2/1
 * Flying
 * When ~ leaves the battlefield, choose an opponent who controls more lands than you. Search your library for a number of |H1Plains cards equal to the difference and reveal them. Put one of them onto the battlefield tapped and the rest into your hand. Then shuffle your library. */

/* ManalinkEh 2022: 0x255f760; original cards/commander_2018.c:373. */
static char *is_token_targ(int who_chooses, int player, int card)
{
  int result;
  char *requirement_error;
  result = is_token(player, card);
  requirement_error = NULL;
  if (result == 0)
  {
    requirement_error = "must be a token.";
  }
  return requirement_error;
}

/* ManalinkEh 2022: 0x255f730; original cards/commander_2018.c:380. */
static int is_token_test(int unused1, int unused2, int player, int card)
{
  int result;
  result = is_token(player, card);
  return (unsigned int)(result != 0);
}

/* ManalinkEh 2022: 0x25611a0; original cards/commander_2018.c:388. */
int card_brudiclad_telchor_engineer(int player, int card, event_t event)
{
  /* Brudiclad, Telchor Engineer	|4|U|R
   * Legendary Artifact Creature - Artificer 4/4
   * Creature tokens you control have haste.
   * At the beginning of combat on your turn, create a 2/1 |Sblue Myr artifact creature token. Then you may choose a token you control. If you do, each other token you control becomes a copy of that token. */

  int result;
  card_instance_t *instance;
  token_generation_t token;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  result = is_humiliated(player, card, AL_STATIC);
  if (result == 0)
  {
    instance = in_play(player, card);
    if (((event == EVENT_ABILITIES) && (instance != NULL)) && (affected_card_controller == player))
    {
      result = is_token(player, affected_card);
      if (result != 0)
      {
        haste(affected_card_controller, affected_card, EVENT_ABILITIES);
      }
    }
  }
  result = beginning_of_combat(player, card, event, player);
  if (result != 0)
  {
    default_token_definition(player, card, CARD_ID_MYR, &token);
    token.pow = 2;
    token.tou = 1;
    token.color_forced = '\x04';
    generate_token(&token);
    default_target_definition(player, card, &td, 0x47);
    td.allowed_controller = player;
    td.illegal_abilities = 0;
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (intptr_t)is_token_targ;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "Choose a token you control.", 0, 0x80);
      if (result != 0)
      {
        default_test_definition(&test, 0x47);
        test.special_selection_function = is_token_test;
        instance = get_card_instance(player, card);
        result = instance->targets[0].card;
        instance = get_card_instance(player, card);
        shapeshift_all(player, card, player, &test, instance->targets[0].player, result, 0);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2561390; original cards/commander_2018.c:430. */
int card_coveted_jewel(int player, int card, event_t event)
{
  /* Coveted Jewel	|6
   * Artifact
   * When ~ enters the battlefield, draw three cards.
   * |T: Add three mana of any one color.
   * Whenever one or more creatures an opponent controls attack you and aren't blocked, that player draws three cards and gains control of ~. Untap it. */

  int p;
  int c;
  int turn_player;
  int humiliated;
  card_instance_t *instance;
  int scan_or_result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    draw_cards(player, 3);
  }
  else if ((((event == EVENT_PHASE_CHANGED) && (current_phase == 0x18)) &&
            (current_turn != player)) &&
           (humiliated = is_humiliated(player, card, AL_TRIG), turn_player = current_turn,
            humiliated == 0))
  {

    c = active_cards_count[current_turn] + -1;
    if (-1 < c)
    {
      do
      {
        if ((((turn_player == 1 - player) &&
              (instance = in_play(turn_player, c), instance != NULL)) &&
             ((scan_or_result = is_what(turn_player, c, 2),
               scan_or_result != 0 &&
                   ((instance = get_card_instance(turn_player, c),
                     (instance->state & 4) != 0 && (instance = get_card_instance(turn_player, c),
                                                    (instance->state & 0x200) == 0)))))) &&
            (scan_or_result = check_special_flags(player, card, 0x40), scan_or_result == 0))
        {
          draw_cards(turn_player, 3);
          untap_card(player, card);
          give_control_of_self(player, card);
        }
        c = c + -1;
      } while (c != -1);
    }
    turn_player = current_turn;
    p = 1 - current_turn;
    scan_or_result = active_cards_count[p] + -1;
    if (-1 < scan_or_result)
    {
      do
      {
        if (((((player == turn_player) &&
               (instance = in_play(p, scan_or_result), instance != NULL)) &&
              (is_what(p, scan_or_result, 2) != 0)) &&
             ((instance = get_card_instance(p, scan_or_result),
               (instance->state & 4) != 0 && (instance = get_card_instance(p, scan_or_result),
                                              (instance->state & 0x200) == 0)))) &&
            (check_special_flags(player, card, 0x40) == 0))
        {
          draw_cards(1 - player, 3);
          untap_card(player, card);
          give_control_of_self(player, card);
        }
        scan_or_result = scan_or_result + -1;
      } while (scan_or_result != -1);
    }
  }
  return artifact_mana_all_one_color(player, card, event, 3, 0);
}

/* ManalinkEh 2022: 0x2561630; original cards/commander_2018.c:454. */
int card_crash_of_rhino_beetles(int player, int card, event_t event)
{
  /* Crash of Rhino Beetles	|4|G
   * Creature - Insect 5/5
   * Trample
   * ~ gets +10/+10 as long as you control ten or more lands. */

  int result;
  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = count_permanents_by_type(player, TYPE_LAND);
    if (9 < result)
    {
      event_result = event_result + 10;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2561690; original cards/commander_2018.c:467. */
int card_echo_storm(int player, int card, event_t event)
{
  /* Echo Storm	|3|U|U
   * Sorcery
   * When you cast this spell, copy it for each time you've cast your commander from the command zone this game. You may choose new targets for the copies.
   * Create a token that's a copy of target artifact. */

  card_instance_t *instance;
  int storm_count;
  int commander_or_copy_index;
  token_generation_t token;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x40);
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x40);
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_ARTIFACT", 1, NULL);
        if (spell_fizzled != 1)
        {
          if (check_special_flags(player, card, 0x40000) == 0)
          {
            commander_or_copy_index = locate_edh_card(player);
            if (-1 < commander_or_copy_index)
            {
              commander_or_copy_index = 0;
              while (true)
              {

                storm_count = get_commander_storm_count(player);
                if (storm_count <= commander_or_copy_index)
                  break;
                if (new_pick_target(&td, "TARGET_ARTIFACT", 1, 0) == 0)
                {
                  commander_or_copy_index = commander_or_copy_index + 1;
                }
                else
                {
                  commander_or_copy_index = commander_or_copy_index + 1;
                  copy_token_definition(player, card, &token, instance->targets[0].player,
                                        instance->targets[0].card);
                  generate_token(&token);
                }
              }
              instance->number_of_targets = '\x01';
            }
          }
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x40);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      if (valid_target(&td) != 0)
      {
        copy_token_definition(player, card, &token, instance->targets[0].player,
                              instance->targets[0].card);
        generate_token(&token);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 0x40);
    get_card_instance(player, card);
  }
  return 0;
}

/* Emissary of Grudges	|5|R
 * Creature - Efreet 6/5
 * Flying, haste
 * As ~ enters the battlefield, secretly choose an opponent.
 * Reveal the player you chose: Choose new targets for target spell or ability if it's controlled by the chosen player and if it targets you or a permanent you control. Activate this ability only once. */

/* ManalinkEh 2022: 0x25619a0; original cards/commander_2018.c:518. */
int card_empyrial_storm(int player, int card, event_t event)
{
  /* Empyrial Storm	|4|W|W
   * Sorcery
   * When you cast this spell, copy it for each time you've cast your commander from the command zone this game.
   * Create a 4/4 |Swhite Angel creature token with flying. */

  if (event == EVENT_CAN_CAST)
    return basic_spell(player, card, EVENT_CAN_CAST);
  if (event == EVENT_CAST_SPELL)
  {
    if (card == affected_card && player == affected_card_controller)
    {
      basic_spell(player, card, EVENT_CAST_SPELL);
      if (spell_fizzled != 1 && !check_special_flags(player, card, 0x40000) &&
          locate_edh_card(player) >= 0)
        for (int i = 0; i < get_commander_storm_count(player); ++i)
          generate_token_by_id(player, card, CARD_ID_ANGEL);
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    generate_token_by_id(player, card, CARD_ID_ANGEL);
    kill_card(player, card, KILL_DESTROY);
  }
  return 0;
}

/* Enchanter's Bane	|1|R
 * Enchantment
 * At the beginning of your end step, target enchantment deals damage equal to its converted mana cost to its controller unless that player sacrifices it. */

/* ManalinkEh 2022: 0x2561ab0; original cards/commander_2018.c:554. */
int card_endless_atlas(int player, int card, event_t event)
{
  /* Endless Atlas	|2
   * Artifact
   * |2, |T: Draw a card. Activate this ability only if you control three or more lands with the same name. */

  int p;
  int c;
  int turn_player;
  int activation_available;
  card_instance_t *instance;
  int matching_lands;

  if (event == EVENT_CAN_ACTIVATE)
  {
    activation_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 2, 0, 0,
                                                     0, 0, 0, 0, NULL, NULL);
    turn_player = current_turn;
    if (activation_available != 0)
    {

      c = active_cards_count[current_turn] + -1;
      if (-1 < c)
      {
        do
        {
          if ((player == turn_player) && (instance = in_play(player, c), instance != NULL))
          {
            matching_lands = count_permanents_by_name(player, TYPE_LAND, get_card_name(player, c));
            if (2 < matching_lands)
            {
              return 1;
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
          if ((player == p) && (instance = in_play(player, c), instance != NULL))
          {
            matching_lands = count_permanents_by_name(player, TYPE_LAND, get_card_name(player, c));
            if (2 < matching_lands)
            {
              return 1;
            }
          }
          c = c + -1;
        } while (c != -1);
      }
    }
    return 0;
  }
  if (event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
    draw_a_card(player);
  return generic_activated_ability(player, card, event, 1, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2561cc0; original cards/commander_2018.c:580. */
int card_entreat_the_dead(int player, int card, event_t event)
{
  /* Entreat the Dead	|X|X|B|B|B
   * Sorcery
   * Return X target creature cards from your graveyard to the battlefield.
   * Miracle |X|B|B (You may cast this card for its miracle cost when you draw it if it's the first card you drew this turn.) */

  int i;
  card_instance_t *instance;
  int grave_index_or_mana;
  card_instance_t *spell_instance;
  unsigned int grave_creature_count;
  unsigned int payable_count;
  int32_t value;
  int available_selected_or_paid;

  unsigned int selection_limit;
  char buf[100];
  test_definition_t this_test;

  new_default_test_definition(&this_test, 2, "Select a creature card.");
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, EVENT_CAN_CAST, 0xc, NULL, NULL, 1, &this_test);
  }
  if (event != EVENT_CAST_SPELL)
  {
    if (event != EVENT_RESOLVE_SPELL)
    {
      return 0;
    }
    if (0 < instance->info_slot)
    {
      i = 0;
      do
      {
        grave_index_or_mana = validate_target_from_grave(player, card, player, i);
        if (grave_index_or_mana != -1)
        {
          reanimate_permanent(player, card, player, grave_index_or_mana, REANIMATE_DEFAULT);
        }
        i = i + 1;
      } while (i < instance->info_slot);
    }
    kill_card(player, card, KILL_DESTROY);
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
  if (check_special_flags2(player, card, -0x80000000) != 0)
  {
    return 0;
  }
  spell_instance = get_card_instance(player, card);
  spell_instance->info_slot = 0;
  if (check_special_flags2(player, card, 0x100000) == 0)
  {
    if (played_for_free(player, card) != 0)
    {
      return 0;
    }
    available_selected_or_paid = is_token(player, card);
    if (available_selected_or_paid != 0)
    {
      return 0;
    }
    grave_creature_count = count_graveyard_by_type(player, 2);
    payable_count = has_mana(player, 7, 1);
    payable_count = round_down_value(payable_count);
    if ((int)grave_creature_count < (int)payable_count)
    {
      payable_count = grave_creature_count;
    }
    if ((0 < (int)payable_count) && (available_selected_or_paid = graveyard_has_shroud(player),
                                     available_selected_or_paid == 0))
    {
      selection_limit = payable_count;
      if (0x13 < (int)payable_count)
      {
        selection_limit = 0x13;
      }
      if (ai_is_speculating == 1)
      {
        buf[0] = '\0';
      }
      else if (payable_count == 1)
      {
        memcpy(buf, "Select a creature card.", 24);
      }
      else
      {
        sprintf(buf, "Select up to %d creature cards.", selection_limit);
      }
      new_default_test_definition(&this_test, 2, buf);
      available_selected_or_paid = select_multiple_cards_from_graveyard(
          player, player, 0, 1, &this_test, selection_limit, instance->targets);
      instance->info_slot = available_selected_or_paid;
      if (available_selected_or_paid < 1)
      {
        return 0;
      }
      available_selected_or_paid = charge_mana_for_double_x(player, 0);
      value = available_selected_or_paid / 2;
      set_x_for_x_spells(player, card, EVENT_CAST_SPELL, value);
      return 0;
    }
  }
  else
  {
    available_selected_or_paid = count_graveyard_by_type(player, 2);
    grave_index_or_mana = has_mana(player, 7, 1);
    if (grave_index_or_mana < available_selected_or_paid)
    {
      available_selected_or_paid = grave_index_or_mana;
    }
    if ((0 < available_selected_or_paid) &&
        (grave_index_or_mana = graveyard_has_shroud(player), grave_index_or_mana == 0))
    {
      selection_limit = available_selected_or_paid;
      if (0x13 < available_selected_or_paid)
      {
        selection_limit = 0x13;
      }
      if (ai_is_speculating == 1)
      {
        buf[0] = '\0';
      }
      else if (available_selected_or_paid == 1)
      {
        memcpy(buf, "Select a creature card.", 24);
      }
      else
      {
        sprintf(buf, "Select up to %d creature cards.", selection_limit);
      }
      new_default_test_definition(&this_test, 2, buf);
      available_selected_or_paid = select_multiple_cards_from_graveyard(
          player, player, 0, 1, &this_test, selection_limit, instance->targets);
      instance->info_slot = available_selected_or_paid;
      if (0 < available_selected_or_paid)
      {
        charge_mana(player, COLOR_COLORLESS, available_selected_or_paid);
      }
      if (spell_fizzled == 1)
      {
        return 0;
      }
      value = instance->info_slot;
      set_x_for_x_spells(player, card, EVENT_CAST_SPELL, value);
      return 0;
    }
  }
  ai_modifier = ai_modifier + -0x30;
  instance->info_slot = 0;
  return 0;
}
#endif

/* ManalinkEh 2022: 0x25603e0; original cards/commander_2018.c:674. */
static int effect_estrids_invocation(int player, int card, event_t event)
{
  int attached_card;
  card_instance_t *instance;
  int upkeep_mode;
  int target_player;

  if (effect_follows_control_of_attachment(player, card, event) == 0)
  {
    instance = get_card_instance(player, card);
    if (player == current_turn)
    {
      target_player = (int)instance->damage_target_player;
      attached_card = instance->damage_target_card;
      upkeep_mode = duh_mode(target_player);
      if (upkeep_trigger_mode(target_player, attached_card, event,
                              (upkeep_mode != 0) + RESOLVE_TRIGGER_OPTIONAL) != 0)
      {
        blink_effect(target_player, attached_card, NULL);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25621a0; original cards/commander_2018.c:686. */
int card_estrids_invocation(int player, int card, event_t event)
{
  /* Estrid's Invocation	|2|U
   * Enchantment
   * You may have ~ enter the battlefield as a copy of any enchantment you control, except it has "At the beginning of your upkeep, you may exile this enchantment. If you do, return it to the battlefield under its owner's control." */

  int result;
  target_definition_t td;
  default_target_definition(player, card, &td, 4);
  td.allowed_controller = player;
  result = enters_the_battlefield_as_copy_of_any(player, card, event, &td,
                                                 "SELECT_AN_ENCHANTMENT_YOU_CONTROL");
  if (result != 0)
  {
    create_targetted_legacy_effect(player, card, effect_estrids_invocation, player, card);
  }
  result = global_enchantment(player, card, event);
  return result;
}

#ifndef SHANDALAR
static int totem_armor_any(int player, int card, event_t event)
{
  card_instance_t *aura;
  int player_00;
  card_instance_t *attached_or_aura;
  int dying_or_tapped;
  int attached_card_or_result;
  target_definition_t td;

  aura = get_card_instance(player, card);
  player_00 = (intptr_t)aura->damage_target_player;
  attached_card_or_result = aura->damage_target_card;
  attached_or_aura = in_play(player, card);
  if ((attached_or_aura != NULL) && (player_00 != -1))
  {
    if (aura->targets[7].player == 1)
    {
      return 0;
    }
    aura->targets[7].player = 1;
    dying_or_tapped = is_this_dying(player_00, attached_card_or_result);
    if (dying_or_tapped != 0)
    {

      attached_or_aura = get_card_instance(player_00, attached_card_or_result);
      if ((unsigned char)(attached_or_aura->kill_code - 1) < 2)
      {

        dying_or_tapped = is_tapped(player_00, attached_card_or_result);
        regenerate_target_exe(player_00, attached_card_or_result);
        if (dying_or_tapped == 0)
        {
          untap_card_no_event(player_00, attached_card_or_result);
        }
        kill_card(player, card, KILL_SACRIFICE);
      }
    }
    aura->targets[7].player = 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.preferred_controller = player;
  attached_card_or_result = targeted_aura(player, card, event, &td, "TARGET_PERMANENT");
  return attached_card_or_result;
}
#endif

/* ManalinkEh 2022: 0x2562230; original cards/commander_2018.c:729. */
int card_mask(int player, int card, event_t event)
{
  /* Mask	""
   * Token Enchantment - Aura
   * Enchant permanent
   * Totem armor (If enchanted permanent would be destroyed, instead remove all damage from it and destroy this Aura.) */

  card_instance_t *instance;
  int target_player;
  card_instance_t *attached_or_source;
  int was_tapped;
  int attached_card;
  target_definition_t td;

  instance = get_card_instance(player, card);
  target_player = (int)instance->damage_target_player;
  attached_card = instance->damage_target_card;
  attached_or_source = in_play(player, card);
  if ((attached_or_source != NULL) && (target_player != -1))
  {
    if (instance->targets[7].player == 1)
    {
      return 0;
    }
    instance->targets[7].player = 1;
    if (is_this_dying(target_player, attached_card) != 0)
    {

      attached_or_source = get_card_instance(target_player, attached_card);
      if ((unsigned char)(attached_or_source->kill_code - 1) < 2)
      {

        was_tapped = is_tapped(target_player, attached_card);
        regenerate_target_exe(target_player, attached_card);
        if (was_tapped == 0)
        {
          untap_card_no_event(target_player, attached_card);
        }
        kill_card(player, card, KILL_SACRIFICE);
      }
    }
    instance->targets[7].player = 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.preferred_controller = player;
  return targeted_aura(player, card, event, &td, "TARGET_PERMANENT");
}

/* ManalinkEh 2022: 0x255f790; original cards/commander_2018.c:732. */
static int is_enchanted_test(int unused1, int unused2, int player, int card)
{
  int result;
  result = is_enchanted(player, card);
  return (unsigned int)(result != 0);
}

/* ManalinkEh 2022: 0x2562370; original cards/commander_2018.c:738. */
int card_estrid_the_masked(int player, int card, event_t event)
{
  /* Estrid, the Masked	|1|G|W|U
   * Legendary Planeswalker - Estrid (3)
   * +2: Untap each enchanted permanent you control.
   * -1: Create a |Swhite Aura enchantment token named Mask attached to another target permanent. The token has enchant permanent and totem armor.
   * -7: Put the top seven cards of your library into your graveyard. Return all non-Aura enchantment cards from your graveyard to the battlefield, then do the same for Aura cards.
   * ~ can be your commander. */

  enum
  {
    CHOICE_UNTAP = 1,
    CHOICE_MASK = 2,
    CHOICE_REPLENISH = 3
  };

  card_instance_t *instance;
  int permanent_count;
  int mask_count;
  int grave_enchantment_count;
  int priority_mode_or_loyalty;
  test_definition_t test;
  test_definition_t test2;
  test_definition_t test_ench;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_test_definition(&test, 0x47);
    test.special_selection_function = is_enchanted_test;
    default_test_definition(&test2, 0x47);
    if (event != EVENT_ACTIVATE)
      return planeswalker(player, card, event, 3);

    instance->number_of_targets = '\0';
    instance->info_slot = 0;
    test.state = 0x10;
    priority_mode_or_loyalty = check_battlefield_for_special_card(player, card, player, 4, &test);
    permanent_count = check_battlefield_for_special_card(player, card, player, 4, &test2);
    mask_count = count_cards_by_id(player, 0x4aa4);
    grave_enchantment_count = count_graveyard_by_type(player, 4);
    if (dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                    "Untap all enchanted permanents", 1, priority_mode_or_loyalty, 2,
                    "Create a Mask", 1, (permanent_count - mask_count) * 3, 0xffffffff,
                    "Replenish enchantments", 1, grave_enchantment_count * 5, 0xfffffff9, 0, 0, 0,
                    0) == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      priority_mode_or_loyalty = 4;
      return planeswalker(player, card, event, priority_mode_or_loyalty);
    }
    instance = get_card_instance(player, card);
    default_test_definition(&test, 0x47);
    test.special_selection_function = is_enchanted_test;
    default_test_definition(&test2, 0x47);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      priority_mode_or_loyalty = instance->info_slot;
      if (priority_mode_or_loyalty == CHOICE_UNTAP)
      {
        new_manipulate_all(player, card, player, &test, ACT_UNTAP);
      }
      else if (priority_mode_or_loyalty == CHOICE_MASK)
      {
        generate_token_by_id(player, card, CARD_ID_MASK);
      }
      else if (priority_mode_or_loyalty == CHOICE_REPLENISH)
      {
        mill(player, 7);
        default_test_definition(&test_ench, 4);
        test_ench.subtype = 0x4000;
        test_ench.subtype_flag = 1;
        new_reanimate_all(player, card, player, &test_ench, REANIMATE_DEFAULT);
        test_ench.subtype_flag = 0;
        new_reanimate_all(player, card, player, &test_ench, REANIMATE_DEFAULT);
      }
    }
  }
  priority_mode_or_loyalty = 3;
  return planeswalker(player, card, event, priority_mode_or_loyalty);
}

/* ManalinkEh 2022: 0x25626e0; original cards/commander_2018.c:813. */
int card_ever_watching_threshold(int player, int card, event_t event)
{
  /* Ever-Watching Threshold	|2|U
   * Enchantment
   * Whenever an opponent attacks you and/or a planeswalker you control with one or more creatures, draw a card. */

  int result;

  store_attackers(player, card, event, 0, 1 - player, -1, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
  {
    draw_a_card(player);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* Forge of Heroes	""
 * Land
 * |T: Add |C.
 * |T: Choose target commander that entered the battlefield this turn. Put a +1/+1 counter on it if it's a creature and a loyalty counter on it if it's a planeswalker. */

/* ManalinkEh 2022: 0x2562780; original cards/commander_2018.c:832. */
int card_fury_storm(int player, int card, event_t event)
{
  /* Fury Storm	|2|R|R
   * Instant
   * When you cast this spell, copy it for each time you've cast your commander from the command zone this game. You may choose new targets for the copies.
   * Copy target instant or sorcery spell. You may choose new targets for the copy. */

  int result;
  card_instance_t *instance;
  int i;

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
      twincast(player, card, 0x6c, NULL, NULL);
      result = counterspell_validate(player, card, NULL, 0);
      if (((result != 0) && (result = check_special_flags(player, card, 0x40000), result == 0)) &&
          (result = locate_edh_card(player), -1 < result))
      {

        instance = get_card_instance(player, card);
        for (i = 0; result = get_commander_storm_count(player), i < result;
             i = i + 1)
        {
          copy_spell_from_stack(player, instance->targets[0].player, instance->targets[0].card);
        }
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      result = counterspell_validate(player, card, NULL, 0);
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        copy_spell_from_stack(player, instance->targets[0].player, instance->targets[0].card);
      }
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return twincast(player, card, event, NULL, NULL);
}

/* ManalinkEh 2022: 0x2562940; original cards/commander_2018.c:863. */
int card_genesis_storm(int player, int card, event_t event)
{
  /* Genesis Storm	|4|G|G
   * Sorcery
   * When you cast this spell, copy it for each time you've cast your commander from the command zone this game.
   * Reveal cards from the top of your library until you reveal a nonland permanent card. You may put that card onto the battlefield. Then put all cards revealed this way that weren't put onto the battlefield on the bottom of your library in a random order. */

  int i;
  card_instance_t *instance;
  int storm_checked;
  int commander_or_storm_count;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    default_test_definition(&test, 1);
    test.type_flag = 1;
    get_card_instance(player, card);
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_test_definition(&test, 1);
      test.type_flag = 1;
      instance = get_card_instance(player, card);
      if ((card == affected_card) &&
          (((player == affected_card_controller &&
             (basic_spell(player, card, EVENT_CAST_SPELL), spell_fizzled != 1)) &&
            (storm_checked = check_special_flags(player, card, 0x40000), storm_checked == 0))))
      {
        commander_or_storm_count = locate_edh_card(player);
        i = 0;
        if (-1 < commander_or_storm_count)
        {
          for (; commander_or_storm_count = get_commander_storm_count(player),
                 i < commander_or_storm_count;
               i = i + 1)
          {
            reveal_card_until_you_reveal_test(player, player, &test, 0x44);
          }
          instance->number_of_targets = '\x01';
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_test_definition(&test, 1);
    test.type_flag = 1;
    get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      reveal_card_until_you_reveal_test(player, player, &test, 0x44);
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_test_definition(&test, 1);
    test.type_flag = 1;
    get_card_instance(player, card);
  }
  return 0;
}

/* Geode Golem	|5
 * Artifact Creature - Golem 5/3
 * Trample
 * Whenever ~ deals combat damage to a player, you may cast your commander from the command zone without paying its mana cost. (You still pay any additional costs.) */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2560470; original cards/commander_2018.c:906. */
static int rfg_at_end_of_combat(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (!end_of_combat_trigger(player, card, event, 2))
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  kill_card((int)instance->damage_target_player, instance->damage_target_card, KILL_EXILE);
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2562b60; original cards/commander_2018.c:915. */
int card_gyrus_waker_of_corpses(int player, int card, event_t event)
{
  /* Gyrus, Waker of Corpses	|X|B|R|G
   * Legendary Creature - Hydra 0/0
   * ~ enters the battlefield with a number of +1/+1 counters on it equal to the amount of mana spent to cast it.
   * Whenever ~ attacks, you may exile target creature card with lesser power from your graveyard. If you do, create a token that's a copy of that card and that's tapped and attacking. Exile the token at end of combat. */

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAST_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE) &&
      card == affected_card && player == affected_card_controller)
  {
    if (!check_special_flags2(player, card, 0x100) && !played_for_free(player, card) &&
        !is_token(player, card))
    {
      int mp = x_value + 3;
      int cost = get_updated_casting_cost(player, card, -1, event, 0);
      ++hack_silent_counters;
      add_counters(player, card, player, card, COUNTER_P1_P1, cost + mp);
      --hack_silent_counters;
    }
  }
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    test_definition_t this_test;
    default_test_definition(&this_test, TYPE_CREATURE);
    int power = get_power(player, card);
    scnprintf(this_test.message, 100, "Select target creatrue card with power less than %d", power);
    this_test.power = get_power(player, card);
    this_test.power_flag = 3;
    if (new_special_count_grave(player, &this_test) && !graveyard_has_shroud(player))
    {
      int grave_count = count_graveyard(player);
      int playable[2][grave_count];
      memset(playable, 0xff, sizeof playable);
      int count = 0;
      int scanned = 0;
      int *grave = graveyard_ptr_mutable[player];
      int iid = *grave;
      while (iid != -1)
      {
        if (is_what(-1, iid, TYPE_CREATURE))
        {
          int base_power = cards_data[grave[scanned]].power;
          if (base_power < get_power(player, card))
          {
            playable[0][count] = grave[scanned];
            playable[1][count] = scanned;
            ++count;
          }
        }
        ++scanned;
        iid = grave[scanned];
      }
      if (scanned != 0)
      {
        int selected =
            select_card_from_zone(player, player, playable[0], count, 0, 1, -1, &this_test);
        if (selected == -1)
        {
          spell_fizzled = 1;
        }
        else
        {
          int csvid = cards_data[playable[0][selected]].id;
          rfg_card_from_grave(player, playable[1][selected]);
          token_generation_t token;
          default_token_definition(player, card, csvid, &token);
          token.no_sleight = 1;
          token.legacy = 1;
          token.special_code_for_legacy = rfg_at_end_of_combat;
          token.action = TOKEN_ACTION_ATTACKING;
          generate_token(&token);
        }
      }
    }
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x255f810; original cards/commander_2018.c:978. */
static char *is_aura_or_equipment(int who_chooses, int player, int card)
{
  int result;

  result = has_subtype(player, card, SUBTYPE_AURA);
  if (result == 0)
  {
    result = has_subtype(player, card, SUBTYPE_EQUIPMENT);
    if (result == 0)
    {
      return "must be an aura or equipment.";
    }
  }
  return NULL;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2562ee0; original cards/commander_2018.c:985. */
int card_heavenly_blademaster(int player, int card, event_t event)
{
  /* Heavenly Blademaster	|5|W
   * Creature - Angel 3/6
   * Flying, double strike
   * When ~ enters the battlefield, you may attach any number of Auras and Equipment you control to it.
   * Other creatures you control get +1/+1 for each Aura and Equipment attached to ~. */

  int i;
  target_t *target_cursor;
  target_t *other_target_cursor;
  int mode_or_aura_count;
  card_instance_t *instance;
  card_instance_t *source_in_play;
  int equipment_count;
  target_definition_t td;

  mode_or_aura_count = duh_mode(player);
  etb_ability(player, card, event, (mode_or_aura_count != 0) + RESOLVE_TRIGGER_OPTIONAL);
  instance = get_card_instance(player, card);
  if (event != EVENT_ETB)
  {
    if ((uint32_t)(event - EVENT_POWER) < 2)
    {
      source_in_play = in_play(player, card);
      if (((source_in_play != NULL) && (affected_card_controller == player)) &&
          (affected_card != card))
      {
        if (is_humiliated(instance->targets[0].player, instance->targets[0].card, AL_STATIC) == 0)
        {
          mode_or_aura_count = count_auras_enchanting_me(player, card);
          equipment_count = equipments_attached_to_me(player, card, 1);
          event_result = event_result + mode_or_aura_count + equipment_count;
        }
      }
    }
    return 0;
  }

  instance->number_of_targets = '\0';
  default_target_definition(player, card, &td, 0x47);
  td.allowed_controller = player;
  td.illegal_abilities = 0;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (intptr_t)is_aura_or_equipment;
  td.allow_cancel = 2;
  i = 0;
  do
  {
    if (can_target(&td) == 0)
      break;
    if (new_pick_target(&td, "Select target Aura or Equipment you control", i, 0x80) == 0)
      break;
    other_target_cursor = instance->targets + i;
    target_cursor = instance->targets + i;
    i = i + 1;
    state_untargettable(target_cursor->player, other_target_cursor->card, 1);
  } while (i != 10);
  i = 0;
  if (instance->number_of_targets != '\0')
  {
    do
    {
      state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
      if (has_subtype(instance->targets[i].player, instance->targets[i].card, SUBTYPE_AURA) == 0)
      {
        if (has_subtype(instance->targets[i].player, instance->targets[i].card,
                        SUBTYPE_EQUIPMENT) != 0)
        {
          equip_target_creature(instance->targets[i].player, instance->targets[i].card, player,
                                card);
        }
      }
      else
      {
        attach_aura_to_target(instance->targets[i].player, instance->targets[i].card, EVENT_ETB,
                              player, card);
      }
      i = i + 1;
    } while (i < (int)(unsigned int)instance->number_of_targets);
  }
  instance->number_of_targets = '\0';
  return 0;
}
#endif

/* Isolated Watchtower	""
 * Land
 * |T: Add |C.
 * |2, |T: Scry 1, then you may reveal the top card of your library. If a basic land card is revealed this way, put it onto the battlefield tapped. Activate this ability only if an opponent controls at least two more lands than you. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x255f7c0; original cards/commander_2018.c:1033. */
static int is_enchanted_or_enchantment_creature(int unused1, int unused2, int player, int card)
{
  return is_enchanted(player, card) || is_what(player, card, TYPE_ENCHANTMENT);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2563140; original cards/commander_2018.c:1041. */
int card_kestia_the_cultivator(int player, int card, event_t event)
{
  /* Kestia, the Cultivator	|1|G|W|U
   * Legendary Enchantment Creature - Nymph 4/4
   * Bestow |3|G|W|U (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached to a creature.)
   * Enchanted creature gets +4/+4.
   * Whenever an enchanted creature or enchantment creature you control attacks, draw a card. */

  int result;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if (event == EVENT_DECLARE_ATTACKERS)
  {
    default_test_definition(&test, 2);
    test.special_selection_function = is_enchanted_or_enchantment_creature;
    store_attackers(player, card, EVENT_DECLARE_ATTACKERS, DAT_SEPARATE_TRIGGERS, player, -1,
                         &test);
    dat_ability(player, card, EVENT_DECLARE_ATTACKERS,
                DAT_SEPARATE_TRIGGERS | DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  }
  else
  {
    dat_ability(player, card, event, DAT_SEPARATE_TRIGGERS | DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      draw_cards(player, 1);
    }
  }
  result = new_generic_creature_with_bestow(player, card, event, 3, 0, 1, 1, 0, 1, 4, 4, 0, 0);
  return result;
}
#endif

/* ManalinkEh 2022: 0x25632a0; original cards/commander_2018.c:1066. */
int card_lord_windgrace(int player, int card, event_t event)
{
  /* Lord Windgrace	|2|B|R|G
   * Legendary Planeswalker - Windgrace (5)
   * +2: Discard a card, then draw a card. If a land card is discarded this way, draw an additional card.
   * -3: Return up to two target land cards from your graveyard to the battlefield.
   * -11: Destroy up to six target nonland permanents, then create six 2/2 |Sgreen Cat Warrior creature tokens with |H2forestwalk.
   * ~ can be your commander. */

  enum
  {
    CHOICE_DISCARD_DRAW = 1,
    CHOICE_LANDS = 2,
    CHOICE_HEX = 3
  };

  int i;
  int choice;
  uint8_t selected_mode;
  card_instance_t *instance;
  unsigned int selected_land_count;
  int scan_or_result;
  int selection_or_quantity;
  int *dead_land_cursor;
  target_t *target_cursor;
  target_t *other_target_cursor;
  inst_info_t stored_info;
  char msg[50];
  target_definition_t td;
  test_definition_t this_test;
  test_definition_t land_test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_test_definition(&this_test, 0xff);
    i = 7;
    this_test.zone = 0x100;
    strncpy(msg, "Select a land card.", 0x14);
    msg[0x14] = '\0';
    msg[0x15] = '\0';
    memset(msg + 0x16, 0, sizeof(msg) - 0x16);
    new_default_test_definition(&land_test, 1, msg);
    default_target_definition(player, card, &td, 0xff);
    td.illegal_type = TYPE_LAND;
    td.allow_cancel = 3;
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->mode = '\0';
      clear_info(instance, 0x4a9d);
      scan_or_result = count_graveyard_by_type(player, 1);
      selection_or_quantity = 0;
      if (0 < scan_or_result)
      {
        selection_or_quantity = (unsigned int)(1 < scan_or_result) * 5 + 5;
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, DLG_MODE,
                           "Discard and Draw", (unsigned int)(0 < hand_count[player]), 1, 2,
                           "Reanimate lands", 1, selection_or_quantity, 0xfffffffd,
                           "Destroy and make cats", 1, 0xf, 0xfffffff5, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_LANDS)
      {

        instance->number_of_targets = '\0';
        selection_or_quantity = select_multiple_cards_from_graveyard(
            player, player, 0, 1, &land_test, 2, instance->targets);
        set_inst_info(instance, 0x4a9d, 0, (uint16_t)selection_or_quantity);
      }
      else if (choice == CHOICE_HEX)
      {

        scan_or_result = 0;
        selection_or_quantity = can_target(&td);
        do
        {
          if ((selection_or_quantity == 0) ||
              (selection_or_quantity =
                   new_pick_target(&td, "TARGET_NONLAND_PERMANENT", scan_or_result, 0),
               selection_or_quantity == 0))
            break;
          other_target_cursor = instance->targets + scan_or_result;
          target_cursor = instance->targets + scan_or_result;
          scan_or_result = scan_or_result + 1;
          state_untargettable(target_cursor->player, other_target_cursor->card, 1);
          selection_or_quantity = can_target(&td);
        } while (scan_or_result < 6);
        if (0 < scan_or_result)
        {
          target_cursor = instance->targets;
          do
          {
            other_target_cursor = target_cursor + 1;
            state_untargettable(target_cursor->player, target_cursor->card, 0);
            target_cursor = other_target_cursor;
          } while (other_target_cursor != instance->targets + scan_or_result);
        }
      }
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    instance = get_card_instance(player, card);
    default_test_definition(&this_test, 0xff);
    i = 7;
    this_test.zone = 0x100;
    strncpy(msg, "Select a land card.", 0x14);
    msg[0x14] = '\0';
    msg[0x15] = '\0';
    memset(msg + 0x16, 0, sizeof(msg) - 0x16);
    new_default_test_definition(&land_test, 1, msg);
    default_target_definition(player, card, &td, 0xff);
  }
  td.allow_cancel = 3;
  td.illegal_type = TYPE_LAND;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    selected_mode = instance->mode;
    if (selected_mode == '\x01')
    {
      if (0 < hand_count[player])
      {

        if (player == 1)
        {
          this_test.type = 1;
        }
        selection_or_quantity = new_select_a_card(player, player, 4, 1, 3, -1, &this_test);
        scan_or_result = is_what(player, selection_or_quantity, 1);
        discard_card(player, selection_or_quantity);
        selection_or_quantity = 1;
        if (scan_or_result != 0)
        {
          selection_or_quantity = 2;
        }
        draw_cards(player, selection_or_quantity);
      }
    }
    else
    {
      if (selected_mode == '\x02')
      {

        stored_info = inst_info(instance, 0x4a9d);
        selected_land_count =
            ((uint32_t)stored_info.id | ((uint32_t)stored_info.data2 << 16)) >> 0x10;
        if (selected_land_count != 0)
        {
          scan_or_result = graveyard_has_shroud(player);
          i = 0;
          if (scan_or_result == 0)
          {
            int dead[10];
            do
            {
              if ((i < (int)selected_land_count) &&
                  (scan_or_result = validate_target_from_grave(player, card, player, i),
                   scan_or_result != -1))
              {
                dead[i] = graveyard_ptr_mutable[player][scan_or_result];
                remove_card_from_grave(player, scan_or_result);
              }
              else
              {
                dead[i] = -1;
              }
              i = i + 1;
            } while (i != 2);
            dead_land_cursor = dead;
            do
            {
              if (*dead_land_cursor != -1)
              {

                selection_or_quantity = raw_put_iid_on_top_of_graveyard(player, *dead_land_cursor);
                increase_trap_condition(player, 0x202, -1);
                reanimate_permanent(player, card, player, selection_or_quantity, REANIMATE_DEFAULT);
              }
              dead_land_cursor = dead_land_cursor + 1;
            } while (dead_land_cursor != dead + 2);
            instance->targets[9].card = -1;
            return planeswalker(player, card, event, 5);
          }
        }
        return 0;
      }
      if (selected_mode == '\x03')
      {

        if (instance->number_of_targets != '\0')
        {
          i = 0;
          do
          {
            other_target_cursor = instance->targets + i;
            target_cursor = instance->targets + i;
            i = i + 1;
            kill_card(target_cursor->player, other_target_cursor->card, KILL_DESTROY);
          } while (i < (int)(unsigned int)instance->number_of_targets);
        }
        token_generation_t token;
        default_token_definition(player, card, CARD_ID_CAT, &token);
        token.qty = 6;
        token.color_forced = COLOR_TEST_GREEN;
        token.key_plus = get_hacked_walk(player, card, 4);
        generate_token(&token);
      }
    }
  }
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x25639c0; original cards/commander_2018.c:1202. */
int card_loyal_apprentice(int player, int card, event_t event)
{
  /* Loyal Apprentice	|1|R
   * Creature - Human Artificer 2/1
   * Haste
   * Lieutenant - At the beginning of combat on your turn, if you control your commander, create a 1/1 colorless Thopter artifact creature token with flying. That token gains haste until end of turn. */

  int result;
  token_generation_t token;

  result = beginning_of_combat(player, card, event, player);
  if (result != 0)
  {
    result = liutenant(player);
    if (result != 0)
    {
      default_token_definition(player, card, CARD_ID_THOPTER, &token);
      token.action = TOKEN_ACTION_HASTE;
      generate_token(&token);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2563a40; original cards/commander_2018.c:1217. */
int card_loyal_drake(int player, int card, event_t event)
{
  /* Loyal Drake	|2|U
   * Creature - Drake 2/2
   * Flying
   * Lieutenant - At the beginning of combat on your turn, if you control your commander, draw a card. */

  int result;

  result = beginning_of_combat(player, card, event, player);
  if (result != 0)
  {
    result = liutenant(player);
    if (result != 0)
    {
      draw_a_card(player);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2563a90; original cards/commander_2018.c:1230. */
int card_loyal_guardian(int player, int card, event_t event)
{
  /* Loyal Guardian	|4|G
   * Creature - Rhino 4/4
   * Trample
   * Lieutenant - At the beginning of combat on your turn, if you control your commander, put a +1/+1 counter on each creature you control. */

  int result;
  test_definition_t this_test;

  result = beginning_of_combat(player, card, event, player);
  if (result != 0)
  {
    result = liutenant(player);
    if (result != 0)
    {
      default_test_definition(&this_test, 2);
      new_manipulate_all(player, card, player, &this_test, ACT_ADD_COUNTERS_BASE | ACT_KILL_BURY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2563b20; original cards/commander_2018.c:1245. */
int card_loyal_subordinate(int player, int card, event_t event)
{
  /* Loyal Subordinate	|2|B
   * Creature - Zombie 3/1
   * Menace
   * Lieutenant - At the beginning of combat on your turn, if you control your commander, each opponent loses 3 life. */

  int result;

  menace(player, card, event);
  result = beginning_of_combat(player, card, event, player);
  if (result != 0)
  {
    result = liutenant(player);
    if (result != 0)
    {
      lose_life(1 - player, 3);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x255f870; original cards/commander_2018.c:1261. */
static int loyal_unicorn_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  instance = combat_damage_being_prevented(event);
  if (((instance != NULL) && (instance->damage_target_card != -1)) &&
      (instance->damage_target_player == player))
  {
    prevent_damage(affected_card_controller, affected_card, -1);
  }
  instance = get_card_instance(player, card);
  if (((event == EVENT_ABILITIES) && (affected_card_controller == player)) &&
      (result = instance->targets[0].card, result != affected_card))
  {
    result = is_humiliated(instance->targets[0].player, result, AL_STATIC);
    if (result == 0)
    {
      special_abilities(affected_card_controller, affected_card, EVENT_ABILITIES, 8,
                        instance->targets[0].player, instance->targets[0].card);
    }
  }
  result = eot_trigger(player, card, event);
  if ((event != EVENT_CAN_SKIP_TURN) && (result == 0))
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x2563ba0; original cards/commander_2018.c:1275. */
int card_loyal_unicorn(int player, int card, event_t event)
{
  /* Loyal Unicorn	|3|W
   * Creature - Unicorn 3/4
   * Vigilance
   * Lieutenant - At the beginning of combat on your turn, if you control your commander, prevent all combat damage that would be dealt to creatures you control this turn. Other creatures you control gain vigilance until end of turn. */

  int result;
  card_instance_t *instance;
  test_definition_t test;

  result = beginning_of_combat(player, card, event, player);
  if (result != 0)
  {
    result = liutenant(player);
    if (result != 0)
    {

      result = create_legacy_effect(player, card, loyal_unicorn_legacy);
      alternate_legacy_text(1, player, result);
      instance = get_card_instance(player, result);
      instance->targets[0].player = player;
      instance = get_card_instance(player, result);
      instance->targets[0].card = card;
      default_test_definition(&test, 2);
      test.not_me = 1;
      pump_creatures_until_eot(player, card, player, 2, 0, 0, 0, 8, &test);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2563ca0; original cards/commander_2018.c:1297. */
int card_magus_of_the_balance(int player, int card, event_t event)
{
  /* Magus of the Balance	|1|W
   * Creature - Human Wizard 2/2
   * |4|W, |T, Sacrifice ~: Each player chooses a number of lands they control equal to the number of lands controlled by the player who controls the fewest, then sacrifices the rest. Players discard cards and sacrifice creatures the same way. */

  int p;
  int c;
  int own_difference_or_result;
  int maximum_or_player;
  int other_difference_or_turn;
  card_instance_t *instance;
  unsigned int pass_or_other_player;
  int t_player;
  int t_player_00;
  unsigned int what_player;
  int diff[2][3];
  test_definition_t test;

  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      pass_or_other_player = 0;
      do
      {
        what_player = pass_or_other_player ^ current_turn;
        own_difference_or_result = count_subtype(what_player, 1, -1);
        maximum_or_player = count_subtype(1 - what_player, 1, -1);
        other_difference_or_turn = current_turn;
        pass_or_other_player = pass_or_other_player + 1;
        diff[what_player][0] = own_difference_or_result - maximum_or_player;
      } while (pass_or_other_player != 2);

      p = 1 - current_turn;
      diff[current_turn][1] = hand_count[current_turn] - hand_count[p];
      pass_or_other_player = current_turn ^ 1;

      diff[pass_or_other_player][1] =
          hand_count[pass_or_other_player] - hand_count[1 - pass_or_other_player];
      own_difference_or_result = count_subtype(current_turn, 2, -1);
      maximum_or_player = count_subtype(p, 2, -1);
      diff[other_difference_or_turn][2] = own_difference_or_result - maximum_or_player;
      pass_or_other_player = current_turn ^ 1;
      other_difference_or_turn = count_subtype(pass_or_other_player, 2, -1);
      own_difference_or_result = count_subtype(1 - pass_or_other_player, 2, -1);
      t_player = 1 - player;
      diff[pass_or_other_player][2] = other_difference_or_turn - own_difference_or_result;
      other_difference_or_turn = diff[t_player][0];
      own_difference_or_result = diff[player][0];
      maximum_or_player = own_difference_or_result;
      if (own_difference_or_result <= other_difference_or_turn)
      {
        maximum_or_player = other_difference_or_turn;
      }
      if (maximum_or_player != 0)
      {

        t_player_00 = player;
        if (own_difference_or_result <= other_difference_or_turn)
        {
          t_player_00 = t_player;
        }
        new_default_test_definition(&test, 1, "Select a land to sacrifice.");
        do
        {
          new_sacrifice(player, card, t_player_00, SAC_JUST_MARK, &test);
          maximum_or_player = maximum_or_player + -1;
        } while (maximum_or_player != 0);
        c = active_cards_count[t_player_00] + -1;
        if (-1 < c)
        {
          do
          {
            instance = in_play(t_player_00, c);
            if (((instance != NULL) && (own_difference_or_result = is_what(t_player_00, c, 1),
                                        own_difference_or_result != 0)) &&
                (own_difference_or_result = check_state(t_player_00, c, 0x100000),
                 own_difference_or_result != 0))
            {
              kill_card(t_player_00, c, KILL_SACRIFICE);
            }
            c = c + -1;
          } while (c != -1);
        }
      }
      other_difference_or_turn = diff[t_player][1];
      own_difference_or_result = diff[player][1];
      maximum_or_player = other_difference_or_turn;
      if (other_difference_or_turn < own_difference_or_result)
      {
        maximum_or_player = own_difference_or_result;
      }
      if (maximum_or_player != 0)
      {

        maximum_or_player = player;
        if (own_difference_or_result <= other_difference_or_turn)
        {
          own_difference_or_result = other_difference_or_turn;
          maximum_or_player = t_player;
        }
        new_multidiscard(maximum_or_player, own_difference_or_result, 0, player);
      }
      other_difference_or_turn = diff[t_player][2];
      own_difference_or_result = diff[player][2];
      maximum_or_player = other_difference_or_turn;
      if (other_difference_or_turn < own_difference_or_result)
      {
        maximum_or_player = own_difference_or_result;
      }
      if (maximum_or_player != 0)
      {

        if (other_difference_or_turn < own_difference_or_result)
        {
          t_player = player;
        }
        new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
        do
        {
          new_sacrifice(player, card, t_player, SAC_JUST_MARK, &test);
          maximum_or_player = maximum_or_player + -1;
        } while (maximum_or_player != 0);
        c = active_cards_count[t_player] + -1;
        if (-1 < c)
        {
          do
          {
            instance = in_play(t_player, c);
            if (((instance != NULL) && (own_difference_or_result = is_what(t_player, c, 2),
                                        own_difference_or_result != 0)) &&
                (own_difference_or_result = check_state(t_player, c, 0x100000),
                 own_difference_or_result != 0))
            {
              kill_card(t_player, c, KILL_SACRIFICE);
            }
            c = c + -1;
          } while (c != -1);
        }
      }
    }
  }
  return generic_activated_ability(player, card, event, 9, 4, 0, 0, 0, 0, 1, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x25640e0; original cards/commander_2018.c:1355. */
int card_myth_unbound(int player, int card, event_t event)
{
  /* Myth Unbound	|2|G
   * Enchantment
   * Your commander costs |1 less to cast for each time it's been cast from the command zone this game.
   * Whenever your commander is put into the command zone from anywhere, draw a card. */

  int result;

  if (event == EVENT_COMMANDER_TO_COMMAND_ZONE)
  {
    draw_a_card(player);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2564120; original cards/commander_2018.c:1366. */
int card_nesting_dragon(int player, int card, event_t event)
{
  /* Nesting Dragon	|3|R|R
   * Creature - Dragon 5/4
   * Flying
   * Landfall - Whenever a land enters the battlefield under your control, create a 0/2 |Sred Dragon Egg creature token with defender and "When this creature dies, create a 2/2 |Sred Dragon creature token with flying and '|R: This creature gets +1/+0 until end of turn.'" */

  landfall(player, card, event);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  generate_token_by_id(player, card, CARD_ID_DRAGON_EGG);
  return 0;
}

/* Night Incarnate	|4|B
 * Creature - Elemental 3/4
 * Deathtouch
 * When ~ leaves the battlefield, all creatures get -3/-3 until end of turn.
 * Evoke |3|B (You may cast this spell for its evoke cost. If you do, it's sacrificed when it enters the battlefield.) */

/* Nylea's Colossus	|6|G
 * Enchantment Creature - Giant 6/6
 * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, double target creature's power and toughness until end of turn. */

/* ManalinkEh 2022: 0x2564180; original cards/commander_2018.c:1388. */
int card_octopus_umbra(int player, int card, event_t event)
{
  /* Octopus Umbra	|3|U|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has base power and toughness 8/8 and has "Whenever this creature attacks, you may tap target creature with power 8 or less."
   * Totem armor (If enchanted creature would be destroyed, instead remove all damage from it and destroy this Aura.) */

  card_instance_t *instance;
  int attached_card;
  int mode_or_target_card;
  int attacker_player;
  target_definition_t td;

  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {

    instance = get_card_instance(player, card);
    attacker_player = (int)instance->damage_target_player;
    attached_card = instance->damage_target_card;
    store_attackers(player, card, event, 0, attacker_player, attached_card, NULL);
    mode_or_target_card = duh_mode(player);
    dat_ability(player, card, event, (mode_or_target_card != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      default_target_definition(attacker_player, attached_card, &td, 2);
      td.power_requirement = 0x2008;
      instance = get_card_instance(attacker_player, attached_card);
      instance->number_of_targets = '\0';
      if (new_pick_target(&td, "Select target creature with power 8 or less", 0, 0x81) != 0)
      {
        instance = get_card_instance(attacker_player, attached_card);
        mode_or_target_card = instance->targets[0].card;
        instance = get_card_instance(attacker_player, attached_card);
        tap_card(instance->targets[0].player, mode_or_target_card);
      }
    }
  }
  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (is_humiliated(player, card, AL_STATIC) == 0)
    {
      instance = get_card_instance(player, card);
      if (instance->damage_target_player != -1)
      {

        instance = get_card_instance(player, card);
        if ((((event == EVENT_POWER) || (event == EVENT_TOUGHNESS)) &&
             (instance->damage_target_card == affected_card)) &&
            (instance->damage_target_player == affected_card_controller))
        {
          set_base_pt(8);
        }
      }
    }
  }
  return totem_armor(player, card, event, 0, 0, 0, 0);
}

/* Primordial Mist	|4|U
 * Enchantment
 * At the beginning of your end step, you may manifest the top card of your library. (Put it onto the battlefield face down as a 2/2 creature. Turn it face up any time for its mana cost if it's a creature card.)
 * Exile a face-down permanent you control face up: You may play that card this turn. (You still pay its costs. Timing rules still apply.) */

/* Ravenous Slime	|2|G
 * Creature - Ooze 1/1
 * ~ can't be blocked by creatures with power 2 or less.
 * If a creature an opponent controls would die, instead exile it and put a number of +1/+1 counters equal to that creature's power on ~. */

/* Reality Scramble	|2|R|R
 * Sorcery
 * Put target permanent you own on the bottom of your library. Reveal cards from the top of your library until you reveal a card that shares a card type with that permanent. Put that card onto the battlefield and the rest on the bottom of your library in a random order.
 * Retrace (You may cast this card from your graveyard by discarding a land card in addition to paying its other costs.) */

/* ManalinkEh 2022: 0x2564390; original cards/commander_2018.c:1445. */
int card_retrofitter_foundry(int player, int card, event_t event)
{
  /* Retrofitter Foundry	|1
   * Artifact
   * |3: Untap ~.
   * |2, |T: Create a 1/1 colorless Servo artifact creature token.
   * |1, |T, Sacrifice a Servo: Create a 1/1 colorless Thopter artifact creature token with flying.
   * |T, Sacrifice a Thopter: Create a 4/4 colorless Construct artifact creature token. */

  enum
  {
    CHOICE_UNTAP = 1,
    CHOICE_SERVO = 2,
    CHOICE_THOPTER = 3,
    CHOICE_CONSTRUCT = 4
  };

  int choice;
  card_instance_t *instance;
  int thopter_available;
  int construct_available;
  int tapped;
  uint32_t untap_priority;
  int available_cost_or_token;
  const char *prompt;
  test_definition_t *test;
  int untap_available;
  char servo_buffer[50];
  char thopter_buffer[50];
  token_generation_t token;
  test_definition_t test_servo;
  test_definition_t test_thopter;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    scnprintf(servo_buffer, 0x32, " Select a Servo to sacrifice");
    new_default_test_definition(&test_servo, 0xff, servo_buffer);
    test_servo.subtype = 0xee;
    scnprintf(thopter_buffer, 0x32, " Select a Thopter to sacrifice");
    new_default_test_definition(&test_thopter, 0xff, thopter_buffer);
    test_thopter.subtype = 0xc6;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 2, 0, 0, 0, 0, 0, 0, NULL,
                                    NULL) != 0)
      {
        return 1;
      }
      available_cost_or_token = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 1, 0,
                                                          0, 0, 0, 0, 0, NULL, NULL);
      if ((available_cost_or_token != 0) &&
          (available_cost_or_token = new_can_sacrifice_as_cost(player, card, &test_servo),
           available_cost_or_token != 0))
      {
        return 1;
      }
      available_cost_or_token = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0,
                                                          0, 0, 0, 0, 0, NULL, NULL);
      if ((available_cost_or_token != 0) &&
          (available_cost_or_token = new_can_sacrifice_as_cost(player, card, &test_thopter),
           available_cost_or_token != 0))
      {
        return 1;
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 3, 0, 0, 0, 0, 0, 0,
                                       NULL, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {

      if (player == 1)
      {
        untap_available = generic_activated_ability(1, card, EVENT_CAN_ACTIVATE, 0x80000, 3, 0, 0,
                                                    0, 0, 0, 0, NULL, NULL);
      }
      else
      {
        untap_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 3, 0, 0, 0,
                                                    0, 0, 0, NULL, NULL);
      }
      available_cost_or_token = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 2, 0,
                                                          0, 0, 0, 0, 0, NULL, NULL);
      thopter_available = generic_activated_ability_with_sacrifice_as_cost(
          player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, 1, 0, 0, 0, 0, 0, 0, NULL, NULL,
          TYPE_NONE, &test_servo, SARV_0);
      construct_available = generic_activated_ability_with_sacrifice_as_cost(
          player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, 0, 0, 0, 0, 0, 0, 0, NULL, NULL,
          TYPE_NONE, &test_thopter, SARV_0);
      tapped = is_tapped(player, card);
      untap_priority = 0;
      if (tapped != 0)
      {
        untap_priority = 0x14;
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
                           "Untap Retrofitter Foundry", untap_available, untap_priority,
                           "Create a Servo", available_cost_or_token, 5, "Create a Thopter",
                           thopter_available, 10, "Create a Construct", construct_available, 0xf, 0,
                           0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_UNTAP)
      {
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 3, 0, 0, 0, 0, 0, 0, NULL,
                                         NULL);
      }
      if (choice != CHOICE_SERVO)
      {
        if (choice == CHOICE_THOPTER)
        {
          test = &test_servo;
          prompt = "Select a a Servo to sacrifice";
          available_cost_or_token = 1;
        }
        else
        {
          if (choice != CHOICE_CONSTRUCT)
          {
            return 0;
          }
          test = &test_thopter;
          prompt = "Select a a Thopter to sacrifice";
          available_cost_or_token = 0;
        }
        return generic_activated_ability_with_sacrifice_as_cost(
            player, card, EVENT_ACTIVATE, GAA_LITERAL_PROMPT | GAA_UNTAPPED,
            available_cost_or_token, 0, 0, 0, 0, 0, 0, NULL, prompt, TYPE_NONE, test, SARV_0);
      }
      return generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 2, 0, 0, 0, 0, 0, 0, NULL,
                                       NULL);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    scnprintf(servo_buffer, 0x32, " Select a Servo to sacrifice");
    new_default_test_definition(&test_servo, 0xff, servo_buffer);
    test_servo.subtype = 0xee;
    scnprintf(thopter_buffer, 0x32, " Select a Thopter to sacrifice");
    new_default_test_definition(&test_thopter, 0xff, thopter_buffer);
    test_thopter.subtype = 0xc6;
    instance = get_card_instance(player, card);
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  available_cost_or_token = instance->info_slot;
  if (available_cost_or_token == CHOICE_THOPTER)
  {
    available_cost_or_token = 0x4995;
  }
  else
  {
    if (CHOICE_THOPTER < available_cost_or_token)
    {
      if (available_cost_or_token != CHOICE_CONSTRUCT)
      {
        return 0;
      }
      default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_CONSTRUCT, &token);
      token.tou = 4;
      token.pow = 4;
      generate_token(&token);
      return 0;
    }
    if (available_cost_or_token == CHOICE_UNTAP)
    {
      untap_card(instance->parent_controller, instance->parent_card);
      return 0;
    }
    if (available_cost_or_token != CHOICE_SERVO)
    {
      return 0;
    }
    available_cost_or_token = 0x4980;
  }
  generate_token_by_id(instance->parent_controller, instance->parent_card, available_cost_or_token);

  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2564cc0; original cards/commander_2018.c:1543. */
int card_saheelis_directive(int player, int card, event_t event)
{
  /* Saheeli's Directive	|X|R|R|R
   * Sorcery
   * Improvise (Each artifact you tap after you're done activating mana abilities pays for |1.)
   * Reveal the top X cards of your library. You may put any number of artifact cards with converted mana cost X or less from among them onto the battlefield. Then put all cards revealed this way that weren't put onto the battlefield into your graveyard. */

  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card || player != affected_card_controller)
      goto native_improvise;
    improvise(player, card, EVENT_CAST_SPELL);
    generic_spell(player, card, EVENT_CAST_SPELL, GS_X_SPELL, NULL, NULL, 0, NULL);
    if (spell_fizzled == 1)
      return 0;
    card_instance_t *instance = get_card_instance(player, card);
    int adjustment = -48;
    if (instance->info_slot > 5)
      adjustment = instance->info_slot * 12;
    ai_modifier += adjustment;
    return 0;
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int deck_count = count_deck(player);
    int max = instance->info_slot;
    if (deck_count < max)
      max = deck_count;
    if (max != 0)
    {
      int cage = grafdiggers_cages(player, TYPE_CREATURE, TUTOR_FROM_DECK);
      show_deck(player, deck_ptr[player], max, "Cards revealed by Saheeli's Directive", 0,
                     gs_done_008b40e0);
      if (cage == 0)
      {
        int deck_copy[max];
        for (int i = 0; i < max; ++i)
        {
          deck_copy[i] = deck_ptr[player][0];
          remove_card_from_deck(player, 0);
        }
        char msg[100];
        scnprintf(msg, sizeof(msg), "Select an artifact card with CMC <= %d.", max);
        test_definition_t test;
        new_default_test_definition(&test, TYPE_ARTIFACT, msg);
        test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
        test.cmc = max + 1;
        int amount = max;
        do
        {
          int selected =
              select_card_from_zone(player, player, deck_copy, amount, 0, 1, -1, &test);
          if (selected == -1)
          {
            if (amount != 0)
            {
              if (amount > 0)
                for (int i = cage; i < amount; ++i)
                  raw_put_iid_on_top_of_deck(player, deck_copy[i]);
              mill(player, amount);
            }
            break;
          }
          is_what(-1, deck_copy[selected], TYPE_CREATURE);
          int card_added = add_card_to_hand(player, deck_copy[selected]);
          set_special_flags(player, card_added, SF_PLAYED_FROM_DECK);
          put_into_play(player, card_added);
          --amount;
          for (int i = selected; i < amount; ++i)
            deck_copy[i] = deck_copy[i + 1];
        } while (amount != 0);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  else if (event == EVENT_CAN_CAST || event == EVENT_CHANGE_TARGET)
    improvise(player, card, event);
  else if (event != EVENT_CAN_CHANGE_TARGET)
    goto native_improvise;
  return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);

native_improvise:
  return improvise(player, card, event);
}
#endif

/* ManalinkEh 2022: 0x25650d0; original cards/commander_2018.c:1617. */
int card_saheeli_the_gifted(int player, int card, event_t event)
{
  /* Saheeli, the Gifted	|2|U|R
   * Legendary Planeswalker - Saheeli (4)
   * +1: Create a 1/1 colorless Servo artifact creature token.
   * +1: The next spell you cast this turn costs |1 less to cast for each artifact you control as you cast it.
   * -7: For each artifact you control, create a token that's a copy of it. Those tokens gain haste. Exile those tokens at the beginning of the next end step.
   * ~ can be your commander. */

  enum
  {
    CHOICE_SERVO = 1,
    CHOICE_COST = 2,
    CHOICE_COPIES = 3
  };

  int c;
  int mode_token_or_scan;
  card_instance_t *scanned_or_token_instance;
  int source_id_or_result;
  card_instance_t *instance;
  token_generation_t token;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_test_definition(&test, 0x40);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      test.state = 0x10;
      mode_token_or_scan = check_battlefield_for_special_card(player, card, player, 4, &test);
      if (dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Create a Servo", 1, 3,
                      1, "Your next spell costs less", 1, mode_token_or_scan, 1,
                      "Copy your artifacts", 1, mode_token_or_scan * 2, 0xfffffff9, 0, 0, 0,
                      0) == 0)
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
    default_test_definition(&test, 0x40);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    mode_token_or_scan = instance->info_slot;
    if (mode_token_or_scan == CHOICE_SERVO)
    {
      generate_token_by_id(player, card, CARD_ID_SERVO);
    }
    else if (mode_token_or_scan == CHOICE_COST)
    {

      mode_token_or_scan = generate_reserved_token_by_id(player, CARD_ID_SPECIAL_EFFECT);
      scanned_or_token_instance = get_card_instance(player, mode_token_or_scan);
      scanned_or_token_instance->targets[0].player = player;
      scanned_or_token_instance->targets[0].card = 0x41;
      source_id_or_result = get_id(instance->parent_controller, instance->parent_card);
      scanned_or_token_instance->targets[1].card = source_id_or_result;
      put_into_play(player, mode_token_or_scan);
    }
    else if (mode_token_or_scan == CHOICE_COPIES)
    {

      char marked[151] = {0};
      c = 0;
      if (0 < active_cards_count[player])
      {
        do
        {
          scanned_or_token_instance = in_play(player, c);
          if ((scanned_or_token_instance != NULL) &&
              (source_id_or_result = is_what(player, c, 0x40), source_id_or_result != 0))
          {
            marked[c] = '\x01';
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
        if (0 < active_cards_count[player])
        {
          mode_token_or_scan = 0;
          do
          {
            if (marked[mode_token_or_scan] != '\0')
            {
              copy_token_definition(instance->parent_controller, instance->parent_card, &token,
                                    player, mode_token_or_scan);
              token.legacy = '\x01';
              token.special_code_for_legacy = haste_and_remove_eot;
              generate_token(&token);
            }
            mode_token_or_scan = mode_token_or_scan + 1;
          } while (mode_token_or_scan < active_cards_count[player]);
        }
      }
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x25654a0; original cards/commander_2018.c:1694. */
int card_skull_storm(int player, int card, event_t event)
{
  /* Skull Storm	|7|B|B
   * Sorcery
   * When you cast this spell, copy it for each time you've cast your commander from the command zone this game.
   * Each opponent sacrifices a creature. Each opponent who can't loses half their life, rounded up. */

  card_instance_t *instance;
  int commander_or_opponent;
  unsigned int life_loss;
  int storm_count_or_opponent;
  int copy_index;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    get_card_instance(player, card);
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      default_test_definition(&test, 2);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        basic_spell(player, card, EVENT_CAST_SPELL);
        if (spell_fizzled != 1)
        {
          if (check_special_flags(player, card, 0x40000) == 0)
          {
            commander_or_opponent = locate_edh_card(player);
            if (-1 < commander_or_opponent)
            {

              commander_or_opponent = 1 - player;
              copy_index = 0;
              while (true)
              {
                storm_count_or_opponent = get_commander_storm_count(player);
                if (storm_count_or_opponent <= copy_index)
                  break;
                if (new_can_sacrifice(player, card, commander_or_opponent, &test) == 0)
                {
                  life_loss = round_up_value(life[commander_or_opponent]);
                  lose_life(commander_or_opponent, life_loss);
                }
                else
                {
                  new_sacrifice(player, card, commander_or_opponent, SAC_CAUSED | SAC_NO_CANCEL,
                                &test);
                }
                copy_index = copy_index + 1;
              }
              instance->number_of_targets = '\x01';
            }
          }
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    get_card_instance(player, card);
    default_test_definition(&test, 2);
    if (event == EVENT_RESOLVE_SPELL)
    {
      storm_count_or_opponent = 1 - player;
      if (new_can_sacrifice(player, card, storm_count_or_opponent, &test) == 0)
      {
        life_loss = round_up_value(life[storm_count_or_opponent]);
        lose_life(storm_count_or_opponent, life_loss);
      }
      else
      {
        new_sacrifice(player, card, storm_count_or_opponent, SAC_CAUSED | SAC_NO_CANCEL, &test);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    get_card_instance(player, card);
    default_test_definition(&test, 2);
  }
  return 0;
}

/* Sower of Discord	|4|B|B
 * Creature - Demon 6/6
 * Flying
 * As ~ enters the battlefield, choose two players.
 * Whenever damage is dealt to one of the chosen players, the other chosen player also loses that much life. */

/* Tawnos, Urza's Apprentice	|U|R
 * Legendary Creature - Human Artificer 1/3
 * Haste
 * |U|R, |T: Copy target activated or triggered ability you control from an artifact source. You may choose new targets for the copy. (Mana abilities can't be targeted.) */

/* ManalinkEh 2022: 0x2565750; original cards/commander_2018.c:1751. */
int card_thantis_the_warweaver(int player, int card, event_t event)
{
  /* Thantis, the Warweaver	|3|B|R|G
   * Legendary Creature - Spider 5/5
   * Vigilance, reach
   * All creatures attack each combat if able.
   * Whenever a creature attacks you or a planeswalker you control, put a +1/+1 counter on ~. */

  card_instance_t *instance;

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {
      all_must_attack_if_able(current_turn, event, -1);
    }
  }
  store_attackers(player, card, event, DAT_SEPARATE_TRIGGERS, 1 - player, -1, NULL);
  dat_ability(player, card, event, DAT_SEPARATE_TRIGGERS | DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }
  add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  return 0;
}

/* ManalinkEh 2022: 0x2565870; original cards/commander_2018.c:1775. */
int card_treasure_nabber(int player, int card, event_t event)
{
  /* Treasure Nabber	|2|R
   * Creature - Goblin Rogue 3/2
   * Whenever an opponent taps an artifact for mana, gain control of that artifact until the end of your next turn. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_TAP_CARD)
  {
    result = is_what(affected_card_controller, affected_card, 0x40);
    if (((result != 0) && (1 - player == affected_card_controller)) && (-1 < tapped_for_mana_color))
    {
      instance = get_card_instance(1 - player, affected_card);
      if ((cards_data[instance->internal_card_id].extra_ability & 0x1000) != 0)
      {
        result = is_humiliated(player, card, AL_TRIG);
        if (result == 0)
        {
          gain_control_until_eoyt(player, card, affected_card_controller, affected_card);
        }
      }
    }
  }
  return 0;
}

/* Turntimber Sower	|2|G
 * Creature - Elf Druid 3/3
 * Whenever one or more land cards are put into your graveyard from anywhere, create a 0/1 |Sgreen Plant creature token.
 * |G, Sacrifice three creatures: Return target land card from your graveyard to your hand. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2565940; original cards/commander_2018.c:1789. */
int card_tuvasa_the_sunlit(int player, int card, event_t event)
{
  /* Tuvasa the Sunlit	|G|W|U
   * Legendary Creature - Merfolk Shaman 1/1
   * ~ gets +1/+1 for each enchantment you control.
   * Whenever you cast your first enchantment spell each turn, draw a card. */

  int result;
  check_legend_rule(player, card, event);
  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = get_stormenchantment_count(player);
    if (result == 1)
    {
      sp_ability_trigger(player, event, 2, TYPE_ENCHANTMENT, NULL, player, card);
    }
  }
  if (event != EVENT_SPA)
  {
    if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
        (player == affected_card_controller))
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = count_subtype(player, 4, -1);
        event_result = event_result + result;
      }
    }
    return 0;
  }
  draw_a_card(player);
  return 0;
}
#endif

/* ManalinkEh 2022: 0x2565a50; original cards/commander_2018.c:1810. */
int card_varchild_betrayer_of_kjeldor(int player, int card, event_t event)
{
  /* Varchild, Betrayer of Kjeldor	|2|R
   * Legendary Creature - Human Knight 3/3
   * Whenever ~ deals combat damage to a player, that player creates that many 1/1 |Sred Survivor creature tokens.
   * Survivors your opponents control can't block, and they can't attack you or a planeswalker you control.
   * When ~ leaves the battlefield, gain control of all Survivors. */

  int32_t packed_damage;
  card_instance_t *instance;
  unsigned char survivor_count;

  check_legend_rule(player, card, event);
  if (whenever_i_deal_damage_to_a_player(
          player, card, event, DDBM_REPORT_DAMAGE_DEALT | DDBM_MUST_BE_COMBAT_DAMAGE) != 0)
  {
    token_generation_t token;

    instance = get_card_instance(player, card);
    packed_damage = instance->targets[0x10].player;
    instance->targets[0x10].player = 0;
    survivor_count = (char)((unsigned int)packed_damage >> 8) + (char)packed_damage;
    default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_SURVIVOR, &token);
    token.t_player = 1 - player;
    token.qty = survivor_count;
    generate_token(&token);
  }
  if (event != EVENT_ABILITIES)
  {
    test_definition_t test;
    leaves_play(player, card, event, 1, 1);
    if (event != EVENT_LEAVES_PLAY_ABILITY)
    {
      return 0;
    }
    default_test_definition(&test, 2);
    test.subtype = SUBTYPE_SURVIVOR;
    new_manipulate_all(player, card, 2, &test, ACT_GAIN_CONTROL);
    return 0;
  }
  if (1 - player == affected_card_controller)
  {
    if (has_subtype(1 - player, affected_card, SUBTYPE_SURVIVOR) != 0)
    {
      cannot_attack(affected_card_controller, affected_card, EVENT_ABILITIES);
      cannot_block(affected_card_controller, affected_card, EVENT_ABILITIES);
    }
  }
  leaves_play(player, card, EVENT_ABILITIES, 1, 1);
  return 0;
}

/* ManalinkEh 2022: 0x2565c60; original cards/commander_2018.c:1845. */
int card_varina_lich_queen(int player, int card, event_t event)
{
  /* Varina, Lich Queen	|1|W|U|B
   * Legendary Creature - Zombie Wizard 4/4
   * Whenever you attack with one or more Zombies, draw that many cards, then discard that many cards. You gain that much life.
   * |2, Exile two cards from your graveyard: Create a tapped 2/2 |Sblack Zombie creature token. */

  token_generation_t token;
  card_instance_t *instance;
  int result;

  char msg[100];

  check_legend_rule(player, card, event);
  if (event == EVENT_DECLARE_ATTACKERS)
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.subtype = 0xde;
    store_attackers(player, card, EVENT_DECLARE_ATTACKERS, 0, player, -1,
                    &test);
    dat_ability(player, card, EVENT_DECLARE_ATTACKERS, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  }
  else
  {
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      instance = get_card_instance(player, card);
      result = instance->info_slot;
      draw_cards(player, result);
      new_multidiscard(player, result, 0, player);
      gain_life(player, result);
      return 0;
    }
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = count_graveyard(player);
      if (result < 2)
      {
        return 0;
      }
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 2, 0, 0, 0, 0, 0, 0,
                                         NULL, NULL);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      test_definition_t this_test;
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      memcpy(msg, "Select a card to exile.", 24);
      memset(msg + 24, 0, 76);
      new_default_test_definition(&this_test, 0xff, msg);
      result = new_global_tutor(player, player, 2, 6, 0, 3, &this_test);
      if (result < 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      new_global_tutor(player, player, 2, 6, 1, 3, &this_test);
      return 0;
    }
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_ZOMBIE,
                                &token);
  token.action = TOKEN_ACTION_TAPPED;
  generate_token(&token);
  return 0;
}

/* ManalinkEh 2022: 0x255f990; original cards/commander_2018.c:1903. */
static int vedalken_humiliation(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  if (((uint32_t)(event - EVENT_POWER) < 3) || (event == EVENT_CAST_SPELL))
  {
    instance = in_play(player, card);
    if (instance == NULL)
    {
      return 0;
    }
    instance = in_play(affected_card_controller, affected_card);
    if (instance == NULL)
    {
      return 0;
    }
    if (affected_card_controller == player)
    {
      return 0;
    }
    result = is_what(affected_card_controller, affected_card, 2);
    if (result == 0)
    {
      return 0;
    }
    if ((uint32_t)(event - EVENT_POWER) < 2)
    {
      set_base_pt(1);
      return 0;
    }
    if ((event == EVENT_ABILITIES) || (event == EVENT_CAST_SPELL))
    {

      event_result = event_result | 0x1f000000;
      instance = get_card_instance(affected_card_controller, affected_card);
      event_result = event_result & 0x9f080000;
      instance->state = instance->state & 0xffffdfff;
      instance->ext->sp_keywords = instance->ext->sp_keywords & 0x993c00c0;
      humiliate(player, card, affected_card_controller, affected_card, 1);
    }
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x2565fd0; original cards/commander_2018.c:1931. */
int card_vedalken_humiliator(int player, int card, event_t event)
{
  /* Vedalken Humiliator	|3|U
   * Creature - Vedalken Wizard 3/4
   * Metalcraft - Whenever ~ attacks, if you control three or more artifacts, creatures your opponents control lose all abilities and have base power and toughness 1/1 until end of turn. */

  int result;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    result = metalcraft(player, card);
    if (result != 0)
    {
      create_legacy_effect(player, card, vedalken_humiliation);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2566040; original cards/commander_2018.c:1945. */
int card_whiptongue_hydra(int player, int card, event_t event)
{
  /* Whiptongue Hydra	|5|G
   * Creature - Lizard Hydra 4/4
   * Reach
   * When ~ enters the battlefield, destroy all creatures with flying. Put a +1/+1 counter on ~ for each creature destroyed this way. */

  int num;
  test_definition_t this_test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }

  default_test_definition(&this_test, 2);
  this_test.keyword = 0x20;
  num = new_manipulate_all(player, card, 2, &this_test, ACT_KILL_DESTROY);
  add_counters(player, card, player, card, COUNTER_P1_P1, num);
  return 0;
}

/* ManalinkEh 2022: 0x255fb00; original cards/commander_2018.c:1966. */
static int effect_xantcha(int player, int card, event_t event)
{
  int p;
  int target_card;
  card_instance_t *instance;
  int id_or_activation_cost;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        id_or_activation_cost = get_id(p, instance->targets[0].card);
        if (id_or_activation_cost != 0x4abc)
        {
          kill_card(player, card, KILL_EXILE);
        }
        instance = in_play(instance->targets[0].player, instance->targets[0].card);
        if (instance == NULL)
        {
          kill_card(player, card, KILL_EXILE);
        }
      }
    }
    instance = get_card_instance(player, card);
    if (instance->targets[0].player != -1)
    {
      get_card_instance(player, card);
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if (instance->targets[0].player != -1)
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      target_card = instance->targets[0].card;
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (can_use_activated_abilities(p, target_card) != 0)
        {

          id_or_activation_cost =
              get_cost_mod_for_activated_abilities(p, target_card, 3, 0, 0, 0, 0, 0);
          if (has_mana_multi(player, id_or_activation_cost, 0, 0, 0, 0, 0) != 0)
          {
            return 1;
          }
        }
      }
      else
      {
        if (event == EVENT_ACTIVATE)
        {

          charge_mana_multi(player,
                            get_cost_mod_for_activated_abilities(p, target_card, 3, 0, 0, 0, 0, 0), 0,
                            0, 0, 0, 0);
          return 0;
        }
        if (event == EVENT_RESOLVE_ACTIVATION)
        {
          lose_life(p, 2);
          draw_a_card(player);
        }
      }
    }
  }
  return 0;
}

/* Windgrace's Judgment	|3|B|G
 * Instant
 * For any number of opponents, destroy target nonland permanent that player controls. */

/* ManalinkEh 2022: 0x2566100; original cards/commander_2018.c:2007. */
int card_xantcha_sleeper_agent(int player, int card, event_t event)
{
  /* Xantcha, Sleeper Agent	|1|B|R
   * Legendary Creature - Minion 5/5
   * As ~ enters the battlefield, an opponent of your choice gains control of it.
   * ~ attacks each combat if able and can't attack its owner or planeswalkers its owner controls.
   * |3: ~'s controller loses 2 life and you draw a card. Any player may activate this ability. */

  int p;
  int c;
  bool legacy_found;
  card_instance_t *instance;
  int owner_or_temporary_card;
  card_instance_t *scanned_instance;
  int other_player_or_result;

  check_legend_rule(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {
    give_control_of_self(player, card);
  }
  instance = in_play(player, card);
  if (((instance != NULL) && (owner_or_temporary_card = is_humiliated(player, card, AL_STATIC),
                              owner_or_temporary_card == 0)) &&
      (owner_or_temporary_card = get_owner(player, card), owner_or_temporary_card != player))
  {
    cannot_attack(player, card, event);
  }
  if (is_humiliated(player, card, AL_STATIC) == 0)
  {
    attack_if_able(player, card, event);
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (scanned_instance = in_play(player, card), scanned_instance != NULL))
    {

      legacy_found = false;
      p = 0;
      do
      {
        c = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            scanned_instance = in_play(p, c);

            if (((scanned_instance != NULL) &&
                 (other_player_or_result = is_what(p, c, 0x80), other_player_or_result != 0)) &&
                ((scanned_instance = get_card_instance(p, c),
                  scanned_instance->info_slot_fn == effect_xantcha &&
                      ((scanned_instance->targets[0].player == player &&
                        (scanned_instance->targets[0].card == card))))))
            {
              if (player == p)
              {
                kill_card(player, c, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                  return generic_activated_ability(player, card, event, 0, 3, 0, 0, 0, 0, 0, 0,
                                                   NULL, NULL);
                legacy_found = true;
              }
            }
            c = c + 1;
          } while (c <= active_cards_count[p]);
        }
        p = p + 1;
      } while (p != 2);
      if (!legacy_found)
      {

        other_player_or_result = 1 - player;
        owner_or_temporary_card =
            add_card_to_hand(other_player_or_result, instance->internal_card_id);
        instance =
            get_card_instance(other_player_or_result,
                              create_legacy_activate(other_player_or_result,
                                                     owner_or_temporary_card, effect_xantcha));
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        obliterate_card(other_player_or_result, owner_or_temporary_card);
      }
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    lose_life(player, 2);
    draw_a_card(player);
  }
  return generic_activated_ability(player, card, event, 0, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x25663f0; original cards/commander_2018.c:2072. */
int card_yennett_cryptic_sovereign(int player, int card, event_t event)
{
  /* Yennett, Cryptic Sovereign	|2|W|U|B
   * Legendary Creature - Sphinx 3/5
   * Flying, vigilance, menace
   * Whenever ~ attacks, reveal the top card of your library. If that card's converted mana cost is odd, you may cast it without paying its mana cost. Otherwise, draw a card. */

  int *deck;
  unsigned int top_cmc;
  int choice;
  char buffer[50];

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  menace(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }

  deck = deck_ptr[player];
  show_deck(player, deck, 1, "Here\'s the first card of the deck", 0, gs_done_008b40e0);
  top_cmc = get_cmc_by_internal_id(*deck);
  if ((top_cmc - ((int)top_cmc >> 0x1f) & 0xfffffffe) != top_cmc)
  {

    scnprintf(buffer, 0x32, " Play %s\n Pass", cards_ptr[cards_data[*deck].id]->name);
    choice = do_dialog(player, player, card, -1, -1, buffer, 0);
    if (choice == 0)
    {
      play_card_in_deck_for_free(player, player, 0);
      return 0;
    }
  }
  draw_a_card(player);
  return 0;
}

/* ManalinkEh 2022: 0x2566560; original cards/commander_2018.c:2108. */
int card_yuriko_the_tigers_shadow(int player, int card, event_t event)
{
  /* Yuriko, the Tiger's Shadow	|1|U|B
   * Legendary Creature - Human Ninja 1/3
   * Commander ninjutsu |U|B (|U|B, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand or the command zone tapped and attacking.)
   * Whenever a Ninja you control deals combat damage to a player, reveal the top card of your library and put that card into your hand. Each opponent loses life equal to that card's converted mana cost. */

  int damage_triggers;
  int top_iid;
  int i;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if (event == EVENT_DEAL_DAMAGE)
  {
    default_test_definition(&test, 2);
    test.subtype = 0x83;
    check_damage_test(player, -1, EVENT_DEAL_DAMAGE,
                      DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                          DDBM_MUST_BE_COMBAT_DAMAGE,
                      player, card, 0, &test);
  }
  damage_triggers = resolve_damage_trigger(
      player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE, player, card);
  if (0 < damage_triggers)
  {

    i = 0;
    do
    {
      top_iid = *deck_ptr[player];
      if (top_iid != -1)
      {
        remove_card_from_deck(player, 0);
        reveal_card_iid(player, card, top_iid);
        add_card_to_hand(player, top_iid);
        lose_life(1 - player, get_cmc_by_internal_id(top_iid));
      }
      i = i + 1;
    } while (damage_triggers != i);
  }
  return ninjutsu(player, card, event, 0, 1, 1, 0, 0, 0, 0x4abe);
}
