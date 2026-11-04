#include "manalink.h"

static char *is_monocolored(int who_chooses, int player, int card, int targeting_player,
                            int targeting_card);

static void make_pest(int player, int card, int amount);
static void make_inkling(int player, int card, int amount, int action);

int ris_legacy(int player, int card, event_t event);
static int card_exiled_with_study_counter(int player, int card, event_t event);

static void learn(int player, int card);

static int shadewing_laureate_effect(int player, int card, event_t event);

static int valentin_effect(int player, int card, event_t event);

int ee_legacy(int player, int card, event_t event);

static void add_hofry_legacy(token_generation_t *token, int card_added, int number);
int rushed_rebirth_legacy(int player, int card, event_t event);
static int hofri_legacy(int player, int card, event_t event);

static int is_basic_forest_or_island(int iid, int unused2, int player, int card);

#ifndef SHANDALAR

int uvilda_legacy(int player, int card, event_t event);

#endif

#ifndef SHANDALAR

int lukka_legacy(int player, int card, event_t event);

#endif

/* ManalinkEh 2022: 0x23c6810; original cards/strixhaven.c:8. */
static void make_pest(int player, int card, int amount)
{
  token_generation_t token;

  default_token_definition(player, card, CARD_ID_PEST, &token);
  token.qty = (unsigned char)amount;
  token.color_forced = '\n';
  token.tou = 1;
  token.pow = 1;
  token.special_infos = 0x42;
  generate_token(&token);
  return;
}

/* ManalinkEh 2022: 0x23c8b60; original cards/strixhaven.c:18. */
/* Compiler specialization: quantity is always one in this binary. */
static void make_one_fractal(int player, int card, int amount, int number_of_p1_p1_counters)
{
  token_generation_t token;
  default_token_definition(player, card, CARD_ID_FRACTAL, &token);
  token.color_forced = '\f';
  token.qty = amount;
  token.action = TOKEN_ACTION_ADD_COUNTERS;
  token.action_argument = number_of_p1_p1_counters & 0xffff;
  generate_token(&token);
  return;
}

/* ManalinkEh 2022: 0x23c6860; original cards/strixhaven.c:29. */
static void make_inkling(int player, int card, int amount, int action)
{
  token_generation_t token;

  default_token_definition(player, card, CARD_ID_INKLING, &token);
  token.qty = (unsigned char)amount;
  token.pow = 2;
  token.tou = 1;
  token.key_plus = 0x20;
  token.color_forced = '\"';
  token.action = action;
  generate_token(&token);
  return;
}

/* ManalinkEh 2022: 0x23c89e0; original cards/strixhaven.c:41. */
/* Concrete compiler clone: only EAX/EDX are ABI arguments. */
static void make_one_elemental_strixhaven(int player, int card, int amount)
{
  token_generation_t token;
  default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
  token.tou = 4;
  token.pow = 4;
  token.color_forced = '\x14';
  token.qty = amount;
  generate_token(&token);
  return;
}

/* ManalinkEh 2022: 0x23c8990; original cards/strixhaven.c:50. */
/* Compiler specialization: quantity is always one in this binary. */
static void make_one_spirit_strixhaven(int player, int card, int amount)
{
  token_generation_t token;
  default_token_definition(player, card, CARD_ID_SPIRIT, &token);
  token.pow = 3;
  token.tou = 2;
  token.color_forced = '0';
  token.qty = amount;
  generate_token(&token);
  return;
}

/* ManalinkEh 2022: 0x23c6560; original cards/strixhaven.c:60. */
static void learn(int player, int card)
{
  int choice;
  int *graveyard;
  bool found;
  int iid_or_choice;
  int grave_index;
  test_definition_t test;

  if ((((unsigned int)player_bits[player] >> 24 & 0xff) & 2) == 0)
  {

    graveyard = graveyard_ptr_mutable[player];
    iid_or_choice = *graveyard;
    if (iid_or_choice != -1)
    {
      found = false;
      grave_index = 0;
      while (true)
      {

        if ((cards_data[iid_or_choice].id == CARD_ID_RETRIEVER_PHOENIX) &&
            (iid_or_choice = dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                                         "Retrieve Retriever Phoenix", 1, 0x14, "Learn as normal",
                                         1, 5, 0, 0, 0, 0),
             iid_or_choice == 1))
        {

          add_card_to_hand(player, graveyard_ptr_mutable[player][grave_index]);
          remove_card_from_grave(player, grave_index);
          found = true;
          iid_or_choice = graveyard[grave_index + 1];
        }
        else
        {
          iid_or_choice = graveyard[grave_index + 1];
        }
        if (iid_or_choice == -1)
          break;
        iid_or_choice = graveyard_ptr_mutable[player][grave_index + 1];
        grave_index = grave_index + 1;
      }
      if (found)
      {
        return;
      }
    }
  }
  choice =
      dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_RANDOM, DLG_NO_STORAGE, "Get a Lesson", 1, 10,
                  "Discard & Draw", hand_count[player], 10, "Do nothing", 1, 1, 0, 0, 0, 0);
  if (choice == 1)
  {

    new_default_test_definition(&test, 0xff, "Choose a Lesson card.");
    test.subtype = 0x6003;
    iid_or_choice = new_card_from_list(player, 5, &test);
    reveal_card(player, card, player, add_card_to_hand(player, iid_or_choice));
    update_rules_engine(check_card_for_rules_engine(iid_or_choice));
    return;
  }
  if (choice != 2)
  {
    return;
  }
  discard(player, 0, player);
  draw_cards(player, 1);
  return;
}

/* ManalinkEh 2022: 0x23c6490; original cards/strixhaven.c:113. */
static int academic_probation_legacy(int player, int card, event_t event)
{
  int target_card;
  card_instance_t *instance;
  int target_player;

  if (event == EVENT_ABILITIES)
  {
    instance = get_card_instance(player, card);
    if (((-1 < instance->damage_target_player) &&
         (instance->damage_target_card == affected_card)) &&
        (instance->damage_target_player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      target_player = (int)instance->damage_target_player;
      target_card = instance->damage_target_card;
      disable_all_activated_abilities(target_player, target_card, 1);
      cannot_attack(target_player, target_card, EVENT_ABILITIES);
      cannot_block(target_player, target_card, EVENT_ABILITIES);
    }
  }
  else if ((event == EVENT_CAN_SKIP_TURN) && (current_turn == player))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23c8bb0; original cards/strixhaven.c:128. */
int card_academic_probation(int player, int card, event_t event)
{
  /* Academic Probation	|1|W
   * Sorcery - Lesson
   * Choose one -
   * * Choose a nonland card name. Opponents can't cast spells with the chosen name until your next turn.
   * * Choose target nonland permanent. Until your next turn, it can't attack or block, and its activated abilities can't be activated. */

  unsigned char mode_flags;
  card_instance_t *instance;
  int result;
  card_instance_t *legacy;
  target_definition_t td;
  test_definition_t this_test;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x47);
    td.illegal_type = TYPE_LAND;
    get_card_instance(player, card);
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x47);
      td.illegal_type = TYPE_LAND;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        result = check_special_flags2(player, card, -0x80000000);
        if (result == 0)
        {

          result = can_target(&td);
          result =
              dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_MODE, "Name a card", 1,
                          10, "Target a permanent", result, 0x14, 0, 0, 0, 0);
          if (result == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
        }
        if ((instance->mode & 2) != 0)
        {
          new_pick_target(&td, "Select target nonland permanent.", 0, 0x81);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x47);
    td.illegal_type = TYPE_LAND;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      mode_flags = instance->mode;
      if ((mode_flags & 1) != 0)
      {

        new_default_test_definition(&this_test, 1, "Select nonland card name.");
        this_test.type_flag = 1;
        do
        {
          result = name_a_card(player, 1 - player, &this_test, 3);
        } while (result == -1);
        result = target_player_cant_cast_csvid(player, card, 1 - player, result);
        if (-1 < result)
        {

          legacy = get_card_instance(player, result);
          add_flag_to_special_effect(player, result, 0x20);
          legacy->targets[0].card = legacy->targets[0].card & 0xfffffffe;
        }
        mode_flags = instance->mode;
      }
      if (((mode_flags & 2) != 0) && (result = valid_target(&td), result != 0))
      {
        disable_all_activated_abilities(instance->targets[0].player, instance->targets[0].card, 1);
        create_targetted_legacy_effect(player, card, academic_probation_legacy,
                                       instance->targets[0].player, instance->targets[0].card);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 0x47);
    td.illegal_type = TYPE_LAND;
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23c8f80; original cards/strixhaven.c:192. */
int card_access_tunnel(int player, int card, event_t event)
{
  /* Access Tunnel	""
   * Land
   * |T: Add |C.
   * |3, |T: Target creature with power 3 or less can't be blocked this turn. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_GIVE_UNBLOCKABLE = 2
  };

  int result;
  card_instance_t *instance;
  bool human;
  target_definition_t td;

  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      td.power_requirement = 0x2003;
      instance = get_card_instance(player, card);

      result = paying_mana();
      if ((result == 0) && (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5,
                                                               4, 0, 0, 0, 0, 0, 0, &td, NULL),
                            result != 0))
      {
        human = ai_is_speculating != 1;
        instance->mode = '\x02';
        if ((human) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
        {
          result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                               "Give 'Unblockable' to a creature with power 3 or less", 1, 10, 0, 0,
                               0, 0);
          if (result == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
          if (result == CHOICE_MANA)
            return mana_producer(player, card, EVENT_ACTIVATE);
          if (result != CHOICE_GIVE_UNBLOCKABLE)
          {
            return 0;
          }
        }
        if (current_turn == player)
        {
          result = (unsigned int)((current_phase < 0x17) ? 48 : -48);
        }
        else
        {
          result = -0x30;
        }
        ai_modifier = ai_modifier + result;
        add_state(player, card, 0x10);
        generic_activated_ability(player, card, EVENT_ACTIVATE, 7, 3, 0, 0, 0, 0, 0, 0, &td,
                                  "Select target creature with power 3 or less.");
        if (spell_fizzled == 1)
        {
          remove_state(player, card, 0x10);
        }
        return 0;
      }
      instance->mode = '\x01';
      return mana_producer(player, card, EVENT_ACTIVATE);
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      td.power_requirement = 0x2003;
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->mode != '\x02')
      {
        return 0;
      }
      result = valid_target(&td);
      if (result == 0)
      {
        return 0;
      }
      pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                             instance->targets[0].player, instance->targets[0].card, 0, 0, 0, 0x10);
      return 0;
    }
  }
  return mana_producer(player, card, event);
}

/* ManalinkEh 2022: 0x23c9310; original cards/strixhaven.c:255. */
int card_accomplished_alchemist(int player, int card, event_t event)
{
  /* Accomplished Alchemist	|3|G
   * Creature - Elf Druid 2/5
   * |T: Add one mana of any color.
   * |T: Add X mana of any one color, where X is the amount of life you gained this turn. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_MANA_AS_LIFEGAIN = 2
  };

  if (event == EVENT_COUNT_MANA && card == affected_card && player == affected_card_controller)
  {
    int gained = get_trap_condition(player, 1);
    int amount = 1;
    if (gained != 0)
      amount = get_trap_condition(player, 1);
    return mana_producing_creature_all_one_color(player, card, EVENT_COUNT_MANA, 0xc,
                                                 COLOR_TEST_ANY_COLORED, amount);
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return mana_producing_creature_all_one_color(player, card, event, 0xc, COLOR_TEST_ANY_COLORED,
                                                 1);
  int gained = get_trap_condition(player, 1);
  if (event == EVENT_CAN_ACTIVATE)
    return (unsigned int)(mana_producing_creature_all_one_color(
                              player, card, event, 0xc, COLOR_TEST_ANY_COLORED, gained) != 0);
  if (event != EVENT_ACTIVATE)
    return 0;
  if (gained > 1)
  {
    if (player == 1)
      return mana_producing_creature_all_one_color(player, card, event, 0xc, COLOR_TEST_ANY_COLORED,
                                                   gained);
    int choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, "Get mana", 1, 1,
                             "Add X mana", 1, 1, 0, 0, 0, 0);
    if (!choice)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (choice == CHOICE_MANA_AS_LIFEGAIN)
      return mana_producing_creature_all_one_color(player, card, event, 0xc, COLOR_TEST_ANY_COLORED,
                                                   gained);
    if (choice != CHOICE_MANA)
      return 0;
  }
  return mana_producing_creature_all_one_color(player, card, event, 0xc, COLOR_TEST_ANY_COLORED, 1);
}

/* ManalinkEh 2022: 0x23c9580; original cards/strixhaven.c:317. */
int card_arcane_subtraction(int player, int card, event_t event)
{
  /* Arcane Subtraction	|1|U
   * Instant
   * Target creature gets -4/-0 until end of turn.
   * Learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -4,
                     0);
      learn(player, card);
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return vanilla_instant_pump(player, card, event, 2, 1 - player, -4, 0, 0, 0);
}

/* ManalinkEh 2022: 0x23c96f0; original cards/strixhaven.c:343. */
int card_archmage_emeritus(int player, int card, event_t event)
{
  /* Archmage Emeritus	|2|U|U
   * Creature - Human Wizard 2/2
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, draw a card. */

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  draw_a_card(player);
  return 0;
}

/* ManalinkEh 2022: 0x23c9760; original cards/strixhaven.c:375. */
int card_augmenter_pugilist(int player, int card, event_t event)
{
  /* Augmenter Pugilist	|1|G|G
   * Creature - Troll Druid 3/3
   * Trample
   * As long as you control eight or more lands, this creature gets +5/+5. */

  int result;

  mdfc_front(player, card, event);
  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      result = count_subtype(player, 1, -1);
      if (7 < result)
      {
        event_result = event_result + 5;
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23c97f0; original cards/strixhaven.c:391. */
int card_echoing_equation(int player, int card, event_t event)
{
  /* Echoing Equation	|3|U|U
   * Sorcery
   * Choose target creature you control. Each other creature you control becomes a copy of it until end of turn, except those creatures aren't legendary. */

  int c;
  int target_or_card;
  card_instance_t *spell;
  card_instance_t *instance;
  int *marked_cursor;
  bool more_marked;
  int marked_count;
  target_definition_t td;
  int marked[151];
  mdfc_back(player, card, event);
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = player;
  if (event == EVENT_RESOLVE_SPELL)
  {
    target_or_card = valid_target(&td);
    if ((target_or_card != 0) &&
        (spell = get_card_instance(player, card), 0 < active_cards_count[player]))
    {
      marked_count = 0;
      c = 0;
      do
      {
        instance = in_play(player, c);
        if (((instance != NULL) && (is_what(player, c, 2) != 0)) && (spell->targets[0].card != c))
        {
          marked[marked_count] = c;
          marked_count = marked_count + 1;
        }
        c = c + 1;
      } while (c < active_cards_count[player]);
      if (0 < marked_count)
      {
        marked_cursor = marked + marked_count + -1;
        do
        {
          target_or_card = *marked_cursor;
          set_special_flags3(player, target_or_card, 0x1000000);
          real_shapeshift_target(player, card, player, target_or_card, spell->targets[0].player,
                                 spell->targets[0].card, 9);
          more_marked = marked_cursor != marked;
          marked_cursor = marked_cursor + -1;
        } while (more_marked);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td, "Select target creature you control.", 1,
                       NULL);
}
#endif

/* ManalinkEh 2022: 0x23c9a40; original cards/strixhaven.c:432. */
int card_baleful_mastery(int player, int card, event_t event)
{
  /* Baleful Mastery	|3|B
   * Instant
   * You may pay |1|B rather than pay this spell's mana cost.
   * If the |1|B cost was paid, an opponent draws a card.
   * Exile target creature or planeswalker. */

  int choice;
  card_instance_t *instance;
  int result;
  card_instance_t *payment_state;
  int fizzle_snapshot;
  target_definition_t td;

  if (event == EVENT_MODIFY_COST)
  {
    if (has_mana_multi(player, 1, 1, 0, 0, 0, 0) != 0)
    {
      null_casting_cost(player, card);
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      return 0;
    }
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
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
      default_target_definition(player, card, &td, 0x102);
      instance = get_card_instance(player, card);
      if ((card != affected_card) || (player != affected_card_controller))
        return generic_spell(player, card, event, 0x81, &td,
                             "Select target creature or planeswalker.", 1, NULL);
      instance->targets[1].player = 0;
      payment_state = get_card_instance(player, card);
      if ((payment_state->info_slot != 1) || (result = is_token(player, card), result != 0))
        return generic_spell(player, card, event, 0x81, &td,
                             "Select target creature or planeswalker.", 1, NULL);

      choice = 1;
      if (has_mana_to_cast_id(player, EVENT_CAST_SPELL, 0x583b) != 0)
        choice = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                             "Pay alternate mana cost", 1, 0x14, "Pay full mana cost", 1, 10, 0, 0,
                             0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        fizzle_snapshot = spell_fizzled;
      }
      else if (choice == 1)
      {
        charge_mana_multi(player, get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, 1),
                          1, 0, 0, 0, 0);
        fizzle_snapshot = spell_fizzled;
        if (spell_fizzled == 1)
          return 0;
        instance->targets[1].player = 0x42;
      }
      else
      {
        fizzle_snapshot = spell_fizzled;
        if (choice == 2)
        {
          charge_mana_from_id(player, -1, EVENT_CAST_SPELL, 0x583b);
          fizzle_snapshot = spell_fizzled;
        }
      }
      if (fizzle_snapshot != 0)
      {
        return 0;
      }
      return generic_spell(player, card, event, 0x81, &td,
                           "Select target creature or planeswalker.", 1, NULL);
    }
    if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
        (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  default_target_definition(player, card, &td, 0x102);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td) != 0)
    {
      if (instance->targets[1].player == 0x42)
      {
        draw_cards(1 - player, 1);
      }
      action_on_target(player, card, 0, ACT_KILL_REMOVE);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td, "Select target creature or planeswalker.", 1,
                       NULL);
}

/* ManalinkEh 2022: 0x23c9e00; original cards/strixhaven.c:496. */
int card_basic_conjuration(int player, int card, event_t event)
{
  /* Basic Conjuration	|1|G|G
   * Sorcery - Lesson
   * Look at the top six cards of your library. You may reveal a creature card from among them and put it into your hand. Put the rest on the bottom of your library in a random order. You gain 3 life. */

  int result;
  test_definition_t test3;
  if (event == EVENT_RESOLVE_SPELL)
  {
    new_default_test_definition(&test3, 2, "Select a creature card to reveal.");
    reveal_top_cards_of_library_and_choose(player, card, player, 6, 0, TUTOR_HAND, 1,
                                           TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test3);
    gain_life(player, 3);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23c9ed0; original cards/strixhaven.c:513. */
int card_bayou_groff(int player, int card, event_t event)
{
  /* Bayou Groff	|1|G
   * Creature - Plant Dog 5/4
   * As an additional cost to cast this spell, sacrifice a creature or pay |3. */

  card_instance_t *instance;
  int result;
  unsigned int sacrificed;
  unsigned int player_00;
  test_definition_t this_test;

  if (event == EVENT_MODIFY_COST)
  {

    result = get_updated_casting_cost(player, card, -1, EVENT_MODIFY_COST, 1);
    result = has_mana_multi(player, 1, 0, 0, result, 0, 0);
    if ((result != 0) ||
        (result = can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE), result != 0))
    {
      null_casting_cost(player, card);
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      return 0;
    }
    infinite_casting_cost();
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
  }
  else if ((((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
            (player == affected_card_controller)) &&
           ((instance = get_card_instance(player, card),
             instance->info_slot == 1 && (result = is_token(player, card), result == 0))))
  {

    result = can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE);
    if (result != 0)
    {
      result = count_subtype(player, 2, -1);
      result = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                           "Sacrifice a creature", 1, 0x14, "Pay 3", 1, result * 10, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == 1)
      {

        new_default_test_definition(&this_test, 2, "Select a creature to sacrifice.");
        sacrificed = new_sacrifice(player, card, player,
                                   SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &this_test);
        if (sacrificed == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        charge_mana_from_id(player, -1, EVENT_CAST_SPELL, 0x583d);
        player_00 = (int)sacrificed >> 0x10 & 0xff;
        if (spell_fizzled != 1)
        {
          kill_card(player_00, sacrificed >> 0x18, KILL_SACRIFICE);
          return 0;
        }
        state_untargettable(player_00, sacrificed >> 0x18, 0);
        return 0;
      }
      if (result != 2)
      {
        return 0;
      }
    }

    result = get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, 3);
    charge_mana_multi(player, 1, 0, 0, result, 0, 0);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23ca200; original cards/strixhaven.c:573. */
int card_beledros_witherbloom(int player, int card, event_t event)
{
  /* Beledros Witherbloom	|5|B|G
   * Legendary Creature - Elder Dragon 4/4
   * Flying
   * At the beginning of each upkeep, create a 1/1 black and green Pest creature token with "When this token dies, you gain 1 life."
   * Pay 10 life: Untap all lands you control. Activate only once each turn. */

  int c;
  card_instance_t *instance;

  check_legend_rule(player, card, event);
  upkeep_trigger_ability(player, card, event, 2);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    make_pest(player, card, 1);
  }
  else
  {

    if ((event == EVENT_RESOLVE_ACTIVATION) && (0 < active_cards_count[player]))
    {
      c = 0;
      do
      {
        if (is_what(player, c, 1) != 0)
        {
          instance = in_play(player, c);
          if (instance != NULL)
          {
            action_on_card(player, card, player, c, ACT_UNTAP);
          }
        }
        c = c + 1;
      } while (c < active_cards_count[player]);
    }
  }
  return generic_activated_ability(player, card, event, 0x2000, 0, 0, 0, 0, 0, 0, 10, NULL, NULL);
}

/* ManalinkEh 2022: 0x23ca340; original cards/strixhaven.c:610. */
int card_biomathematician(int player, int card, event_t event)
{
  /* Biomathematician	|1|G|U
   * Creature - Human Wizard 2/2
   * When this creature enters, create a 0/0 green and blue Fractal creature token. Put a +1/+1 counter on each Fractal you control. */

  test_definition_t this_test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  make_one_fractal(player, card, 1, 0);
  default_test_definition(&this_test, 0x47);
  this_test.subtype = 0x10a;
  new_manipulate_all(player, card, player, &this_test, ACT_ADD_COUNTERS_BASE | ACT_KILL_BURY);
  return 0;
}

/* ManalinkEh 2022: 0x23ca3e0; original cards/strixhaven.c:629. */
int card_blade_historian(int player, int card, event_t event)
{
  /* Blade Historian	|RW|RW|RW|RW
   * Creature - Human Cleric 2/3
   * Attacking creatures you control have double strike. */

  int result;
  card_instance_t *instance;

  if ((event == EVENT_ABILITIES) && (affected_card_controller == player))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = is_what(affected_card_controller, affected_card, 2);
        if (result != 0)
        {
          result = is_attacking(affected_card_controller, affected_card);
          if (result != 0)
          {
            event_result = event_result | 0x40000000;
          }
        }
      }
    }
  }
  result = hybrid(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23ca4a0; original cards/strixhaven.c:642. */
int card_blex_vexing_pest(int player, int card, event_t event)
{
  /* Blex, Vexing Pest	|2|G
   * Legendary Creature - Pest 3/2
   * Other Pests, Bats, Insects, Snakes, and Spiders you control get +1/+1.
   * When Blex dies, you gain 4 life. */

  int result;
  card_instance_t *instance;

  check_legend_rule(player, card, event);
  mdfc_front(player, card, event);
  if ((((((uint32_t)(event - EVENT_POWER) < 2) && (affected_card_controller == player)) &&
        (instance = in_play(player, card), instance != NULL)) &&
       (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
      (((result = has_subtype(affected_card_controller, affected_card, SUBTYPE_PEST),
         result != 0 || (result = has_subtype(affected_card_controller, affected_card, SUBTYPE_BAT),
                         result != 0)) ||
        ((result = has_subtype(affected_card_controller, affected_card, SUBTYPE_INSECT),
          result != 0 ||
              ((result = has_subtype(affected_card_controller, affected_card, SUBTYPE_SNAKE),
                result != 0 ||
                    (result = has_subtype(affected_card_controller, affected_card, SUBTYPE_SPIDER),
                     result != 0))))))))
  {
    event_result = event_result + 1;
  }
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    gain_life(player, 4);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x23ca620; original cards/strixhaven.c:666. */
int card_search_for_blex(int player, int card, event_t event)
{
  /* Search for Blex	|2|B|B
   * Sorcery
   * Look at the top five cards of your library. You may put any number of them into your hand and the rest into your graveyard. You lose 3 life for each card you put into your hand this way. */

  int remaining_or_count;
  int library_size;
  test_definition_t test3;

  mdfc_back(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {

    library_size = count_deck(player);
    remaining_or_count = library_size;
    if (5 < library_size)
    {
      remaining_or_count = 5;
    }
    new_default_test_definition(&test3, 0xff, "Select a card to add to your hand.");
    test3.create_minideck = (unsigned short)remaining_or_count;
    test3.no_shuffle = 1;
    if (library_size == 0)
    {
      remaining_or_count = 0;
      library_size = 0;
    }
    else
    {
      library_size = 0;
      do
      {

        if ((((ai_is_speculating == 1) ||
              ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
             (life[player] - (library_size * 3 + 3) < 6)) ||
            (new_global_tutor(player, player, 1, 1, 0, 1, &test3) < 0))
          break;
        library_size = library_size + 1;
        remaining_or_count = remaining_or_count + -1;
      } while (remaining_or_count != 0);
      library_size = library_size * 3;
    }
    mill(player, remaining_or_count);
    lose_life(player, library_size);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23ca7a0; original cards/strixhaven.c:698. */
int card_blood_age_general(int player, int card, event_t event)
{
  /* Blood Age General	|1|R
   * Creature - Spirit Warrior 2/2
   * |T: Attacking Spirits get +1/+0 until end of turn. */

  int priority;
  int spirits;
  test_definition_t test3;
  if (event == EVENT_ACTIVATE)
  {
    if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
    {
      default_test_definition(&test3, 2);
      test3.state = 4;
      test3.subtype = 0xbc;
      spirits = check_battlefield_for_special_card(player, card, player, 4, &test3);
      priority = spirits * 8;
      if ((current_turn != player) || (0x18 < current_phase))
      {
        priority = spirits * -8;
      }
      ai_modifier = ai_modifier + priority;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    default_test_definition(&test3, 2);
    test3.state = 4;
    test3.subtype = 0xbc;
    pump_creatures_until_eot(player, card, 2, 0, 1, 0, 0, 0, &test3);
  }
  return generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x23ca940; original cards/strixhaven.c:727. */
int card_blood_researcher(int player, int card, event_t event)
{
  /* Blood Researcher	|1|B|G
   * Creature - Vampire Druid 2/2
   * Menace (This creature can't be blocked except by two or more creatures.)
   * Whenever you gain life, put a +1/+1 counter on this creature. */

  int result;

  menace(player, card, event);
  result = card_ajanis_pridemate(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23ca980; original cards/strixhaven.c:737. */
int card_blot_out_the_sky(int player, int card, event_t event)
{
  /* Blot Out the Sky	|X|W|B
   * Sorcery
   * Create X tapped 2/1 white and black Inkling creature tokens with flying. If X is 6 or more, destroy all noncreature, nonland permanents. */

  int result;
  card_instance_t *instance;
  test_definition_t this_test;

  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    make_inkling(player, card, instance->info_slot, 1);
    if (5 < instance->info_slot)
    {
      default_test_definition(&this_test, 3);
      this_test.type_flag = 1;
      new_manipulate_all(player, card, 2, &this_test, ACT_KILL_DESTROY);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
  return result;
}

/* ManalinkEh 2022: 0x23caa60; original cards/strixhaven.c:760. */
int card_body_of_research(int player, int card, event_t event)
{
  /* Body of Research	|G|G|G|U|U|U
   * Sorcery
   * Create a 0/0 green and blue Fractal creature token. Put X +1/+1 counters on it, where X is the number of cards in your library. */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    result = count_deck(player);
    make_one_fractal(player, card, 1, result);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23caac0; original cards/strixhaven.c:800. */
int card_callous_bloodmage(int player, int card, event_t event)
{
  /* Callous Bloodmage	|2|B
   * Creature - Vampire Warlock 2/1
   * When this creature enters, choose one -
   * * Create a 1/1 black and green Pest creature token with "When this token dies, you gain 1 life."
   * * You draw a card and you lose 1 life.
   * * Exile target player's graveyard. */

  enum
  {
    CHOICE_PEST = 1,
    CHOICE_DRAW = 2,
    CHOICE_EXILE_GRAVE = 3
  };

  int choice;
  card_instance_t *instance;
  int can_exile;
  int what_player;
  int other_creatures;
  int own_creatures;
  int valid_opponent;
  int exile_priority;
  int draw_priority;
  target_definition_t td1;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }

  default_target_definition(player, card, &td1, 0);
  td1.zone = TARGET_ZONE_PLAYERS;
  td1.allow_cancel = 0;
  instance = get_card_instance(player, card);
  can_exile = can_target(&td1);
  what_player = 1 - player;
  other_creatures = count_subtype(what_player, 2, -1);
  own_creatures = count_subtype(player, 2, -1);
  draw_priority = 0;
  if (6 < life[player])
  {
    draw_priority = hand_count[player] * -10 + 0x46;
  }
  valid_opponent = would_validate_arbitrary_target(&td1, what_player, -1);
  exile_priority = 0;
  if (valid_opponent != 0)
  {
    exile_priority = count_graveyard(what_player);
  }
  choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
                       "Create a Pest token", 1, (other_creatures - own_creatures) * 10,
                       "Draw a card and you lose 1 life", 1, draw_priority,
                       "Exile target player's graveyard", can_exile, exile_priority, 0, 0, 0, 0);
  if (choice == CHOICE_PEST)
  {
    make_pest(player, card, 1);
  }
  else if (choice == CHOICE_DRAW)
  {
    draw_cards(player, 1);
    lose_life(player, 1);
  }
  else if (choice == CHOICE_EXILE_GRAVE)
  {
    if (can_target(&td1) != 0)
    {
      if (new_pick_target(&td1, "TARGET_PLAYER", 0, 0) != 0)
      {
        rfg_whole_graveyard(instance->targets[0].player);
      }
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x23cad20; original cards/strixhaven.c:870. */
int card_clever_lumimancer(int player, int card, event_t event)
{
  /* Clever Lumimancer	|W
   * Creature - Human Wizard 0/1
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, this creature gets +2/+2 until end of turn. */

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  pump_until_eot_merge_previous(player, card, player, card, 2, 2);
  return 0;
}

/* ManalinkEh 2022: 0x23c68c0; original cards/strixhaven.c:891. */
static int codie_effect(int player, int card, event_t event)
{
  int result;
  test_definition_t test;
  if ((((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (result = check_special_flags(trigger_cause_controller, trigger_cause, 0x40000), result == 0))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
      return 0;
    }
    if (event == EVENT_RESOLVE_TRIGGER)
    {
      default_test_definition(&test, 0xff);
      result = new_specific_spell_played(player, card, EVENT_RESOLVE_TRIGGER, player, 2, &test);
      if (result == 0)
      {
        return 0;
      }
      default_test_definition(&test, 0x38);
      test.cmc = get_cmc(trigger_cause_controller, trigger_cause);
      test.cmc_flag = 3;
      result = reveal_card_until_you_reveal_test(player, player, &test, 0xa04);
      if (-1 < result)
      {
        create_may_play_card_from_exile_effect(player, card, player,
                                               (unsigned int)cards_data[result].id,
                                               MPCFE_UNTIL_END_OF_YOUR_NEXT_TURN | MPCFE_FOR_FREE);
      }
      kill_card(player, card, KILL_EXILE);
      return 0;
    }
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x23cada0; original cards/strixhaven.c:915. */
int card_codie_vociferous_codex(int player, int card, event_t event)
{
  /* Codie, Vociferous Codex	|3
   * Legendary Artifact Creature - Book Construct 1/4
   * You can't cast permanent spells.
   * |4, |T: Add |W|U|B|R|G. When you next cast a spell this turn, exile cards from the top of your library until you exile an instant or sorcery card with lesser mana value. Until end of turn, you may cast that card without paying its mana cost. Put each other card exiled this way on the bottom of your library in a random order. */

  int result;
  card_instance_t *instance;

  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    instance = in_play(player, card);
    if ((((instance != NULL) &&
          (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
         (affected_card_controller == player)) &&
        (result = is_what(player, affected_card, 0x146), result != 0))
    {
      infinite_casting_cost();
    }
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    result = can_produce_mana(player, card);
    if ((((result != 0) && (result = is_tapped(player, card), result == 0)) &&
         ((result = is_animated_and_sick(player, card),
           result == 0 && ((instance = get_card_instance(player, card),
                            instance->targets[1].player != 0x42 &&
                                (result = has_mana(player, 0, 9), result != 0)))))) &&
        (result = can_produce_mana(player, card), result != 0))
    {
      return 1;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {

    instance = get_card_instance(player, card);
    instance->targets[1].player = 0x42;
    charge_mana(player, COLOR_COLORLESS, 4);
    instance->targets[1].player = 0;
    if (spell_fizzled != 1)
    {
      add_state(player, card, 0x10);
      produce_mana_multi(player, 0, 1, 1, 1, 1, 1);
      create_legacy_effect(instance->parent_controller, instance->parent_card, codie_effect);
    }
  }
  else if (((((event == EVENT_COUNT_MANA) && (card == affected_card)) &&
             (player == affected_card_controller)) &&
            ((result = is_tapped(player, card),
              result == 0 && (result = is_animated_and_sick(player, card), result == 0)))) &&
           ((result = can_produce_mana(player, card),
             result != 0 && (instance = get_card_instance(player, card),
                             instance->targets[1].player != 0x42))))
  {
    declare_mana_available(player, 1, 1);
    declare_mana_available(player, 2, 1);
    declare_mana_available(player, 3, 1);
    declare_mana_available(player, 4, 1);
    declare_mana_available(player, 5, 1);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x23cb090; original cards/strixhaven.c:975. */
int card_confront_the_past(int player, int card, event_t event)
{
  /* Confront the Past	|X|B
   * Sorcery - Lesson
   * Choose one -
   * * Return target planeswalker card with mana value X or less from your graveyard to the battlefield.
   * * Remove twice X loyalty counters from target planeswalker an opponent controls. */

  enum
  {
    CHOICE_REANIMATE = 1,
    CHOICE_REMOVE_LOYALTY = 2
  };

  card_instance_t *instance;
  int value;
  int offset_or_shroud;
  target_definition_t td;
  char buffer[100];
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x100);
    td.allowed_controller = 1 - player;
    get_card_instance(player, card);
    if (generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL) != 0)
    {
      return 1;
    }
    value = graveyard_has_shroud(player);

    if ((value == 0) && (value = *graveyard_ptr_mutable[player], value != -1))
    {
      offset_or_shroud = 4;
      do
      {
        if (is_what(-1, value, 0x100) != 0)
        {
          value = get_cmc_by_internal_id(value);
          if (has_mana(player, 0, value) != 0)
          {
            return 1;
          }
        }
        value = *(int *)((int)graveyard_ptr_mutable[player] + offset_or_shroud);
        offset_or_shroud = offset_or_shroud + 4;
      } while (value != -1);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x100);
      td.allowed_controller = 1 - player;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {

        if (check_special_flags2(player, card, -0x80000000) == 0)
        {

          instance->number_of_targets = '\0';
          instance->info_slot = 0;
          instance->targets[1].player = 0;
          generic_spell(player, card, EVENT_CAST_SPELL, 4, NULL, NULL, 1, NULL);
          offset_or_shroud = graveyard_has_shroud(player);
          value = 0;
          if (offset_or_shroud == 0)
          {
            default_test_definition(&test, 0x100);
            test.cmc = instance->info_slot + 1;
            test.cmc_flag = 3;
            value = new_special_count_grave(player, &test);
          }
          offset_or_shroud = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
          if (dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_MODE,
                          "Reanimate a Planewalke with MV X or less", value, 5,
                          "Remove 2*X loyalty from an oppoent's planeswalker", offset_or_shroud, 10,
                          0, 0, 0, 0) == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
        }
        if (instance->mode == CHOICE_REANIMATE)
        {
          scnprintf(buffer, 100, "Select a planeswalker card with mana value %d or less.",
                    instance->info_slot);
          new_default_test_definition(&test, 0x100, buffer);
          test.cmc = instance->info_slot + 1;
          test.cmc_flag = 3;
          return generic_spell(player, card, EVENT_CAST_SPELL, 8, NULL, NULL, 1, &test);
        }
        if (instance->mode == CHOICE_REMOVE_LOYALTY)
        {
          return generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                               "Select target planeswalker an opponent controls.", 1, NULL);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x100);
    td.allowed_controller = 1 - player;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      if (instance->mode == CHOICE_REANIMATE)
      {

        value = validate_target_from_grave_source(player, card, player, 0);
        if (value != -1)
        {
          reanimate_permanent(player, card, player, value, REANIMATE_DEFAULT);
        }
      }
      else if ((instance->mode == CHOICE_REMOVE_LOYALTY) && (value = valid_target(&td), value != 0))
      {
        remove_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                        COUNTER_LOYALTY, instance->info_slot * 2);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23cb610; original cards/strixhaven.c:1075. */
int card_containment_breach(int player, int card, event_t event)
{
  /* Containment Breach	|2|G
   * Sorcery - Lesson
   * Destroy target artifact or enchantment. If its mana value is 2 or less, create a 1/1 black and green Pest creature token with "When this token dies, you gain 1 life." */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x44);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      result = get_cmc(instance->targets[0].player, instance->targets[0].card);
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      if (result < 3)
      {
        make_pest(player, card, 1);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "DISENCHANT", 1, NULL);
}

/* ManalinkEh 2022: 0x23cb790; original cards/strixhaven.c:1102. */
int card_crackle_with_power(int player, int card, event_t event)
{
  /* Crackle with Power	|X|X|X|R|R
   * Sorcery
   * ~ deals five times X damage to each of up to X targets. */

  int i;
  target_t *target;
  target_t *selected_target;
  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 3;
    get_card_instance(player, card);
    result = generic_spell(player, card, EVENT_CAN_CAST, 4, NULL, NULL, 0, NULL);
  }
  else
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        default_target_definition(player, card, &td, 0x102);
        td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
        td.allow_cancel = 3;
        get_card_instance(player, card);
        if ((card == affected_card) && (player == affected_card_controller))
        {
          multi_x_spell(player, card, EVENT_CAST_SPELL, 3);
          return generic_spell(player, card, EVENT_CAST_SPELL, 0x25, &td, "TARGET_ANY", x_value,
                               NULL);
        }
      }
    }
    else if ((event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET) ||
             (event == EVENT_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td.allow_cancel = 3;
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_SPELL)
      {

        i = 0;
        if (instance->number_of_targets != '\0')
        {
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
            damage_creature(target->player, selected_target->card, instance->info_slot * 5, player,
                            card);
          } while (i < (int)(unsigned int)instance->number_of_targets);
        }
        kill_card(player, card, KILL_DESTROY);
      }
    }
    result = 0;
  }
  return result;
}

/* ManalinkEh 2022: 0x23cba10; original cards/strixhaven.c:1139. */
int card_cram_session(int player, int card, event_t event)
{
  /* Cram Session	|1|BG
   * Sorcery
   * You gain 4 life.
   * Learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    gain_life(player, 4);
    learn(player, card);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23cba70; original cards/strixhaven.c:1160. */
int card_crushing_disappointment(int player, int card, event_t event)
{
  /* Crushing Disappointment	|3|B
   * Instant
   * Each player loses 2 life. You draw two cards. */

  int result;
  if (event == EVENT_RESOLVE_SPELL)
  {
    lose_life(player, 2);
    lose_life(1 - player, 2);
    draw_cards(player, 2);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23cbaf0; original cards/strixhaven.c:1175. */
int card_culling_ritual(int player, int card, event_t event)
{
  /* Culling Ritual	|2|B|G
   * Sorcery
   * Destroy each nonland permanent with mana value 2 or less. Add |B or |G for each permanent destroyed this way. */

  int opponent_permanents;
  int own_permanents;
  test_definition_t test;
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_test_definition(&test, 1);
      test.type_flag = 1;
      test.cmc = 3;
      test.cmc_flag = 3;
      opponent_permanents = check_battlefield_for_special_card(player, card, 1 - player, 4, &test);
      own_permanents = check_battlefield_for_special_card(player, card, player, 4, &test);
      ai_modifier = ai_modifier + opponent_permanents * 8 + own_permanents * -10;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&test, 1);
    test.type_flag = 1;
    test.cmc = 3;
    test.cmc_flag = 3;
    produce_mana_any_combination_of_colors(
        player, COLOR_TEST_GREEN | COLOR_TEST_BLACK,
        new_manipulate_all(player, card, 2, &test, ACT_KILL_DESTROY),
        "Select any combination of B and/or G");
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23cbc70; original cards/strixhaven.c:1206. */
int card_culmination_of_studies(int player, int card, event_t event)
{
  /* Culmination of Studies	|X|U|R
   * Sorcery
   * Exile the top X cards of your library. For each land card exiled this way, create a Treasure token. For each blue card exiled this way, draw a card. For each red card exiled this way, ~ deals 1 damage to each opponent. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_RESOLVE_SPELL)
  {

    instance = get_card_instance(player, card);
    result = instance->info_slot;
    reveal_top_cards_of_library(player, result);
    if (result != 0)
    {
      do
      {

        result = *deck_ptr[player];
        rfg_top_card_of_deck(player);
        is_what(-1, result, 1);
        get_color_by_internal_id(player, result);
        get_sleighted_color_test(player, card, 4);
        get_color_by_internal_id(player, result);
        get_sleighted_color_test(player, card, 0x10);
      } while (true);
    }
    generate_tokens_by_id(player, card, CARD_ID_TREASURE, 0);
    draw_cards(player, 0);
    damage_player(1 - player, 0, player, card);
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
}

/* ManalinkEh 2022: 0x23cbdd0; original cards/strixhaven.c:1238. */
int card_curate(int player, int card, event_t event)
{
  /* Curate	|1|U
   * Instant
   * Surveil 2. (Look at the top two cards of your library, then put any number of them into your graveyard and the rest on top of your library in any order.)
   * Draw a card. */

  int minideck_size;
  int library_size;
  int selected;
  test_definition_t test3;
  if (event == EVENT_RESOLVE_SPELL)
  {
    new_default_test_definition(&test3, 0xff, "Select a card to put into your graveyard.");
    library_size = count_deck(player);
    minideck_size = library_size;
    if (0 < library_size)
    {
      minideck_size = 1;
    }
    do
    {
      if (library_size == 0)
        break;
      test3.create_minideck = (unsigned short)minideck_size;
      selected = new_global_tutor(player, player, 1, 3, 0, 3, &test3);
    } while (selected != -1);
    draw_a_card(player);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23cbeb0; original cards/strixhaven.c:1261. */
int card_daemogoth_titan(int player, int card, event_t event)
{
  /* Daemogoth Titan	|BG|BG|BG|BG
   * Creature - Demon 11/10
   * Whenever this creature attacks or blocks, sacrifice a creature. */

  int result;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    result = blocking(player, card, event);
    if (result == 0)
      return hybrid(player, card, event);
    result = is_humiliated(player, card, AL_TRIG);
    if (result != 0)
      return hybrid(player, card, event);
  }
  player_sacrifices_a_permanent(player, card, player, 2, SAC_CAUSED | SAC_NO_CANCEL);
  return hybrid(player, card, event);
}

/* ManalinkEh 2022: 0x23cbf60; original cards/strixhaven.c:1273. */
int card_daemogoth_woe_eater(int player, int card, event_t event)
{
  /* Daemogoth Woe-Eater	|1|B|BG|G
   * Creature - Demon 7/6
   * At the beginning of your upkeep, sacrifice a creature.
   * When you sacrifice this creature, each opponent discards a card, you draw a card, and you gain 2 life. */

  int result;
  test_definition_t test;

  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
    new_sacrifice(player, card, player, SAC_AS_COST, &test);
  }
  result = this_is_sacrificed_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    discard(1 - player, 0, player);
    draw_a_card(player);
    gain_life(player, 2);
  }
  result = hybrid(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23cc050; original cards/strixhaven.c:1297. */
int card_deadly_brew(int player, int card, event_t event)
{
  /* Deadly Brew	|B|G
   * Sorcery
   * Each player sacrifices a creature or planeswalker of their choice. If you sacrificed a permanent this way, you may return another permanent card from your graveyard to your hand. */

  int result;
  test_definition_t test3;
  if (event == EVENT_RESOLVE_SPELL)
  {
    new_default_test_definition(&test3, 0x102, "Select a creature or planeswalker to sacrifice.");
    result = new_sacrifice(player, card, player, SAC_RETURN_CHOICE | SAC_CAUSED, &test3);
    new_sacrifice(player, card, 1 - player, SAC_CAUSED, &test3);
    if (result != 0)
    {
      new_default_test_definition(&test3, 0x47, "Select a permanent card.");
      new_global_tutor(player, player, 2, 1, 0, 1, &test3);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23cc160; original cards/strixhaven.c:1318. */
int card_decisive_denial(int player, int card, event_t event)
{
  /* Decisive Denial	|G|U
   * Instant
   * Choose one -
   * * Target creature you control fights target creature you don't control. (Each deals damage equal to its power to the other.)
   * * Counter target noncreature spell unless its controller pays |3. */

  enum
  {
    CHOICE_FIGHT = 1,
    CHOICE_COUNTERSPELL = 2
  };

  card_instance_t *instance;
  int result;
  unsigned int can_fight;
  target_definition_t td;
  target_definition_t td1;
  target_definition_t td2;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    default_target_definition(player, card, &td1, 2);
    td1.allowed_controller = 1 - player;
    td1.preferred_controller = td1.allowed_controller;
    counterspell_target_definition(player, card, &td2, 0xff);
    td2.illegal_type = TYPE_CREATURE;
    get_card_instance(player, card);
    result = counterspell(player, card, EVENT_CAN_CAST, &td2, 0);
    if (result == 0)
    {
      result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
      if (result == 0)
      {
        return 0;
      }
      result = can_target(&td1);
    }
    else
    {
      result = 99;
    }
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = player;
      td.preferred_controller = player;
      default_target_definition(player, card, &td1, 2);
      td1.allowed_controller = 1 - player;
      td1.preferred_controller = td1.allowed_controller;
      counterspell_target_definition(player, card, &td2, 0xff);
      td2.illegal_type = TYPE_CREATURE;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {

        result = check_special_flags2(player, card, -0x80000000);
        if (result == 0)
        {

          result = generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, NULL, 1, NULL);
          can_fight = 0;
          if (result != 0)
          {
            result = can_target(&td1);
            can_fight = (unsigned int)(result != 0);
          }
          result = counterspell(player, card, EVENT_CAN_CAST, &td2, 0);
          result = dialog_impl(
              player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_MODE,
              "Target creature you control fights target creature you don't control", can_fight, 5,
              "Counter target noncreature spell unless its controller pays |3", result, 10, 0, 0, 0,
              0);
          if (result == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
        }
        if (instance->mode == CHOICE_FIGHT)
        {
          generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CRETURE_YOU_CONTROL", 1,
                        NULL);
          if (spell_fizzled != 1)
          {
            new_pick_target(&td1, "Select target creature opponent controls.", 1, 0x81);
          }
        }
        else if (instance->mode == CHOICE_COUNTERSPELL)
        {
          return counterspell(player, card, EVENT_CAST_SPELL, &td2, 0);
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
    td1.preferred_controller = td1.allowed_controller;
    counterspell_target_definition(player, card, &td2, 0xff);
    td2.illegal_type = TYPE_CREATURE;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      if (instance->mode == CHOICE_FIGHT)
      {
        result = validate_target(player, card, &td, 0);
        if ((result != 0) && (result = validate_target(player, card, &td1, 1), result != 0))
        {
          fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player,
                instance->targets[1].card);
        }
      }
      else if (instance->mode == CHOICE_COUNTERSPELL)
      {
        counterspell_resolve_unless_pay_x(player, card, &td2, 0, 3);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23cc710; original cards/strixhaven.c:1412. */
int card_devastating_mastery(int player, int card, event_t event)
{
  /* Devastating Mastery	|2|W|W|W|W
   * Sorcery
   * You may pay |2|W|W rather than pay this spell's mana cost.
   * If the |2|W|W cost was paid, an opponent chooses up to two nonland permanents they control and returns them to their owner's hand.
   * Destroy all nonland permanents. */

  int choice;
  card_instance_t *instance;
  int result;
  card_instance_t *payment_state;
  int fizzle_snapshot;
  int remaining_bounces;

  if (event == EVENT_MODIFY_COST)
  {
    if (has_mana_multi(player, 2, 0, 0, 0, 0, 2) != 0)
    {
      null_casting_cost(player, card);
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      return 0;
    }
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
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
      instance = get_card_instance(player, card);
      if ((card != affected_card) || (player != affected_card_controller))
        return basic_spell(player, card, event);
      instance->targets[1].player = 0;
      payment_state = get_card_instance(player, card);
      if ((payment_state->info_slot != 1) || (result = is_token(player, card), result != 0))
        return basic_spell(player, card, event);
      choice = 1;
      if (has_mana_to_cast_id(player, EVENT_CAST_SPELL, 0x5866) != 0)
        choice = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                             "Pay alternate mana cost", 1, 0x14, "Pay full mana cost", 1, 10, 0, 0,
                             0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        fizzle_snapshot = spell_fizzled;
      }
      else if (choice == 1)
      {
        charge_mana_multi(player, get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, 2),
                          0, 0, 0, 0, 2);
        fizzle_snapshot = spell_fizzled;
        if (spell_fizzled == 1)
          return 0;
        instance->targets[1].player = 0x42;
      }
      else
      {
        fizzle_snapshot = spell_fizzled;
        if (choice == 2)
        {
          charge_mana_from_id(player, -1, EVENT_CAST_SPELL, 0x5866);
          fizzle_snapshot = spell_fizzled;
        }
      }
      if (fizzle_snapshot != 0)
      {
        return 0;
      }
      return basic_spell(player, card, event);
    }
    if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
        (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    test_definition_t this_test;
    if (instance->targets[1].player == 0x42)
    {
      target_definition_t td;
      default_target_definition(player, card, &td, 0x47);
      td.who_chooses = (int32_t)(1 - player);
      td.illegal_type = TYPE_LAND;
      td.illegal_abilities = 0;
      td.allow_cancel = 3;
      remaining_bounces = 2;
      td.allowed_controller = (int32_t)((uint32_t)(td.who_chooses));
      do
      {
        result = can_target(&td);
        if ((result == 0) ||
            (result = new_pick_target(&td, "Select a nonland permanent you control.", 0, 0x80),
             result == 0))
          break;
        action_on_target(player, card, 0, ACT_BOUNCE);
        remaining_bounces = remaining_bounces + -1;
      } while (remaining_bounces != 0);
    }
    default_test_definition(&this_test, 1);
    this_test.type_flag = 1;
    new_manipulate_all(player, card, 2, &this_test, ACT_KILL_DESTROY);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23ccb40; original cards/strixhaven.c:1496. */
int card_dina_soul_steeper(int player, int card, event_t event)
{
  /* Dina, Soul Steeper	|B|G
   * Legendary Creature - Dryad Druid 1/3
   * Whenever you gain life, each opponent loses 1 life.
   * |1, Sacrifice another creature: Dina gets +X/+0 until end of turn, where X is the sacrificed creature's power. */

  int life_triggers;
  unsigned int sacrificed;
  card_instance_t *instance;
  uint32_t data;
  unsigned int card_00;
  inst_info_t card_info;
  test_definition_t test;

  check_legend_rule(player, card, event);
  life_triggers = trigger_gain_life(player, card, event, player, RESOLVE_TRIGGER_MANDATORY);
  if ((life_triggers != 0) && (instance = in_play(player, card), instance != NULL))
  {
    if (check_battlefield_for_id(player, CARD_ID_EXQUISITE_BLOOD) == 0)
    {
      lose_life(1 - player, 1);
    }
    else
    {
      lose_the_game(1 - player);
    }
  }
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event != EVENT_ACTIVATE)
    {
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }

      instance = get_card_instance(player, card);
      card_info = inst_info(instance, 0x5868);
      pump_until_eot(instance->parent_controller, instance->parent_card,
                     instance->parent_controller, instance->parent_card, card_info.data, 0);
      return 0;
    }

    new_default_test_definition(&test, 2, "Select another creature to sacrifice.");
    test.not_me = 1;
    sacrificed =
        new_sacrifice(player, card, player, SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
    if (sacrificed == 0)
    {
      spell_fizzled = 1;
    }
    else
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
      card_00 = sacrificed >> 0x18;
      sacrificed = (int)sacrificed >> 0x10 & 0xff;
      if (spell_fizzled == 1)
      {
        state_untargettable(sacrificed, card_00, 0);
        return 0;
      }
      data = get_power(sacrificed, card_00);
      instance = get_card_instance(player, card);
      set_inst_info(instance, 0x5868, data, 0);
      kill_card(sacrificed, card_00, KILL_SACRIFICE);
    }
    return 0;
  }
  return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x40200, 0, 1, 0, 0, 0, 0, 0,
                                   NULL, NULL);
}

/* ManalinkEh 2022: 0x23cce60; original cards/strixhaven.c:1544. */
int card_divide_by_zero(int player, int card, event_t event)
{
  /* Divide by Zero	|2|U
   * Instant
   * Return target spell or permanent with mana value 1 or greater to its owner's hand.
   * Learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  target_definition_t td2;

  if (event == EVENT_CAN_CAST)
  {
    counterspell_target_definition(player, card, &td, 0xff);
    td.extra = 1;
    td.special = TARGET_SPECIAL_CMC_GREATER_OR_EQUAL;
    default_target_definition(player, card, &td2, 0x47);
    td2.extra = 1;
    td2.special = TARGET_SPECIAL_CMC_GREATER_OR_EQUAL;
    result = counterspell(player, card, EVENT_CAN_CAST, &td, 0);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      instance->mode = '\x01';
      return 99;
    }
    instance = get_card_instance(player, card);
    instance->mode = '\0';
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td2, NULL, 1, NULL);
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
    counterspell_target_definition(player, card, &td, 0xff);
    td.extra = 1;
    td.special = TARGET_SPECIAL_CMC_GREATER_OR_EQUAL;
    default_target_definition(player, card, &td2, 0x47);
    td2.extra = 1;
    td2.special = TARGET_SPECIAL_CMC_GREATER_OR_EQUAL;
    if (card != affected_card)
    {
      return 0;
    }
    if (player != affected_card_controller)
    {
      return 0;
    }
    instance = get_card_instance(player, card);
    if (instance->mode != '\x01')
    {
      return generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td2,
                           "Select target spell or permanent with CMC 1 or greater.", 1, NULL);
    }
    return counterspell(player, card, EVENT_CAST_SPELL, &td, 0);
  }
  if ((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET))
  {
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
    counterspell_target_definition(player, card, &td, 0xff);
    td.extra = 1;
    td.special = TARGET_SPECIAL_CMC_GREATER_OR_EQUAL;
    default_target_definition(player, card, &td2, 0x47);
    return 0;
  }
  counterspell_target_definition(player, card, &td, 0xff);
  td.extra = 1;
  td.special = TARGET_SPECIAL_CMC_GREATER_OR_EQUAL;
  default_target_definition(player, card, &td2, 0x47);
  td2.extra = 1;
  td2.special = TARGET_SPECIAL_CMC_GREATER_OR_EQUAL;
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  bool valid;
  if (instance->mode == 1)
  {
    valid = counterspell_validate(player, card, &td, 0) != 0;
    if (valid)
    {
      if (pre_counterspell(player, card, instance->targets[0].player, instance->targets[0].card))
        bounce_permanent(instance->targets[0].player, instance->targets[0].card);
      else
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
    }
  }
  else
  {
    valid = valid_target(&td2) != 0;
    if (valid)
      bounce_permanent(instance->targets[0].player, instance->targets[0].card);
  }
  if (valid)
    learn(player, card);
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

/* ManalinkEh 2022: 0x23cd240; original cards/strixhaven.c:1608. */
int card_double_major(int player, int card, event_t event)
{
  /* Double Major	|G|U
   * Instant
   * Copy target creature spell you control, except it isn't legendary if the spell is legendary. (A copy of a creature spell becomes a token.) */

  card_instance_t *instance;
  int result;
  target_definition_t td2;

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        counterspell_target_definition(player, card, &td2, 2);
        td2.allowed_controller = player;
        get_card_instance(player, card);
        if ((card == affected_card) && (player == affected_card_controller))
          return counterspell(player, card, event, &td2, 0);
      }
    }
    else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      counterspell_target_definition(player, card, &td2, 2);
      td2.allowed_controller = player;
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_SPELL)
      {
        result = counterspell_validate(player, card, &td2, 0);
        if (result != 0)
        {
          result =
              copy_spell_from_stack(player, instance->targets[0].player, instance->targets[0].card);
          if (-1 < result)
          {
            set_special_flags3(player, result, 0x1000000);
          }
        }
        kill_card(player, card, KILL_DESTROY);
      }
    }
    else if (event == EVENT_CHANGE_TARGET)
    {
      counterspell_target_definition(player, card, &td2, 2);
      td2.allowed_controller = player;
      get_card_instance(player, card);
      return 0;
    }
    return 0;
  }
  counterspell_target_definition(player, card, &td2, 2);
  td2.allowed_controller = player;
  get_card_instance(player, card);
  return counterspell(player, card, event, &td2, 0);
}

/* ManalinkEh 2022: 0x23cd420; original cards/strixhaven.c:1638. */
int card_draconic_intervention(int player, int card, event_t event)
{
  /* Draconic Intervention	|2|R|R
   * Sorcery
   * As an additional cost to cast this spell, exile an instant or sorcery card from your graveyard.
   * ~ deals X damage to each non-Dragon creature, where X is the exiled card's mana value. If a creature dealt damage this way would die this turn, exile it instead.
   * Exile ~. */

  int id;
  card_instance_t *instance;
  int grave_index;
  uint32_t data;
  inst_info_t card_info;
  test_definition_t test;
  test_definition_t test2;

  if (event == EVENT_CAN_CAST)
  {
    new_default_test_definition(&test, 0x38, "Select an instant or sorcery card.");
    get_card_instance(player, card);
    if (new_special_count_grave(player, &test) != 0)
    {
      return basic_spell(player, card, EVENT_CAN_CAST);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      new_default_test_definition(&test, 0x38, "Select an instant or sorcery card.");
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {

        instance->info_slot = 0;
        grave_index = new_select_a_card(player, player, 2, 0, 2, -1, &test);
        if (grave_index < 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        id = graveyard_ptr_mutable[player][grave_index];
        rfg_card_from_grave(player, grave_index);
        data = get_cmc_by_internal_id(id);
        set_inst_info(instance, 0x586b, data, 0);
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    new_default_test_definition(&test, 0x38, "Select an instant or sorcery card.");
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      default_test_definition(&test2, 2);
      test2.subtype = 0x30;
      test2.subtype_flag = 1;
      card_info = inst_info(instance, 0x586b);
      new_damage_all(player, card, 2, card_info.data, 8, &test2);
      kill_card(player, card, KILL_EXILE);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23cd680; original cards/strixhaven.c:1684. */
int card_dragons_approach(int player, int card, event_t event)
{
  /* Dragon's Approach	|2|R
   * Sorcery
   * ~ deals 3 damage to each opponent. You may exile this spell and four cards named ~ from your graveyard. If you do, search your library for a Dragon creature card, put it onto the battlefield, then shuffle.
   * A deck can have any number of cards named ~. */

  int count;
  int count_or_index;
  unsigned int csvid;
  int exiled;
  test_definition_t test;

  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_test_definition(&test, 0xff);
      test.id = get_id(player, card);
      count_or_index = new_special_count_grave(player, &test);
      if (3 < count_or_index)
      {
        ai_modifier = ai_modifier + 0x60;
      }
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    damage_player(1 - player, 3, player, card);
    default_test_definition(&test, 0xff);
    test.id = CARD_ID_DRAGONS_APPROACH;
    count_or_index = new_special_count_grave(player, &test);

    if ((3 < count_or_index) &&
        (count_or_index = do_dialog(player, player, card, -1, -1, " Tutor a Dragon\n Decline", 0),
         count_or_index == 0))
    {

      csvid = get_id(player, card);
      count = count_graveyard(player);
      count_or_index = count + -1;
      if (-1 < count_or_index)
      {
        exiled = 0;
        do
        {
          if (cards_data[graveyard_ptr_mutable[player][count_or_index]].id == csvid)
          {
            rfg_card_from_grave(player, count_or_index);
            exiled = exiled + 1;
          }

          count_or_index = count_or_index + -1;
        } while ((-1 < count_or_index) && (exiled < 4));
      }
      new_default_test_definition(&test, 2, "Select a Dragon creature card.");
      test.subtype = 0x30;
      new_global_tutor(player, player, 1, 4, 1, 2, &test);
      kill_card(player, card, KILL_EXILE);
      return 0;
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23cd8d0; original cards/strixhaven.c:1732. */
int card_dragonsguard_elite(int player, int card, event_t event)
{
  /* Dragonsguard Elite	|1|G
   * Creature - Human Druid 2/2
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, put a +1/+1 counter on this creature.
   * |4|G|G: Double the number of +1/+1 counters on this creature. */

  int result;
  card_instance_t *instance;

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    result = count_counters(instance->parent_controller, instance->parent_card, COUNTER_P1_P1);
    add_counters(player, card, instance->parent_controller, instance->parent_card,
                             COUNTER_P1_P1, result);
  }
  result = generic_activated_ability(player, card, event, 0, 4, 0, 0, 2, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x23c6b10; original cards/strixhaven.c:1752. */
static int dramatic_finale_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (event == EVENT_STATIC_EFFECTS && card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player >= 0 &&
        get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_DRAMATIC_FINALE)
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
        instance->damage_target_card = -1;
        instance->damage_target_player = -1;
        remove_status(player, card, 0x10000);
        if (instance->targets[11].player < 0)
          instance->targets[11].player = 0;
        *((uint8_t *)&instance->targets[11].player + 2) = 1;
      }
      count_for_gfp_ability_and_store_values(player, card, EVENT_GRAVEYARD_FROM_PLAY, player, 2,
                                             NULL, 0x40, 0);
    }
  }
  unsigned int deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (deaths && !check_status(player, card, 0x10000))
  {
    make_inkling(player, card, 1, 0);
    kill_card(player, card, KILL_EXILE);
  }
  if (event == EVENT_CLEANUP && !check_status(player, card, 0x10000))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x23cda20; original cards/strixhaven.c:1799. */
int card_dramatic_finale(int player, int card, event_t event)
{
  /* Dramatic Finale	|WB|WB|WB|WB
   * Enchantment
   * Creature tokens you control get +1/+1.
   * Whenever one or more nontoken creatures you control die, create a 2/1 white and black Inkling creature token with flying. This ability triggers only once each turn. */

  int c;
  card_instance_t *instance;
  int legacy_or_result;
  inst_info_t card_info;

  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if ((((affected_card_controller == player) &&
          (legacy_or_result = is_token(player, affected_card), legacy_or_result != 0)) &&
         (instance = in_play(player, card), instance != NULL)) &&
        (legacy_or_result = is_humiliated(player, card, AL_STATIC), legacy_or_result == 0))
    {
      event_result = event_result + 1;
    }
  }
  else
  {
    if ((event == EVENT_CAN_CAST) || (event == EVENT_SHOULD_AI_PLAY))
    {
      return global_enchantment(player, card, event);
    }
    if (((event == EVENT_STATIC_EFFECTS) && (card == affected_card)) &&
        (player == affected_card_controller))
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
                instance->info_slot_fn == dramatic_finale_effect)) &&
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
            create_targetted_legacy_effect(player, card, dramatic_finale_effect, player, card);
        instance = get_card_instance(player, legacy_or_result);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, legacy_or_result, 0x10000);
      }
    }
  }
  count_for_gfp_ability_and_store_values(player, card, event, player, 2, NULL, 0x40, 0);
  instance = get_card_instance(player, card);
  card_info = inst_info(instance, 0x586e);
  legacy_or_result =
      resolve_gfp_ability(player, card, event, (unsigned int)(card_info.data2 != 0x42) * 2);
  if ((legacy_or_result != 0) &&
      (instance = get_card_instance(player, card), instance->kill_code == '\0'))
  {
    instance = get_card_instance(player, card);
    set_inst_info(instance, 0x586e, 0, 0x42);
    make_inkling(player, card, 1, 0);
  }
  if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(player, card);
    clear_info(instance, 0x586e);
  }
  return hybrid(player, card, event);
}

/* ManalinkEh 2022: 0x23cdd10; original cards/strixhaven.c:1852. */
int card_dream_strix(int player, int card, event_t event)
{
  /* Dream Strix	|2|U
   * Creature - Bird Illusion 3/2
   * Flying
   * When this creature becomes the target of a spell, sacrifice it.
   * When this creature dies, learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  int result;

  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    learn(player, card);
  }
  result = card_skulking_ghost(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23cdd70; original cards/strixhaven.c:1876. */
int card_ecological_appreciation(int player, int card, event_t event)
{
  /* Ecological Appreciation	|X|2|G
   * Sorcery
   * Search your library and graveyard for up to four creature cards with different names that each have mana value X or less and reveal them. An opponent chooses two of those cards. Shuffle the chosen cards into your library and put the rest onto the battlefield. Exile ~. */

  int i;
  card_instance_t *instance;
  int tutored_card;
  int original_iid;
  int zone_count;
  int chosen[4];
  int cards[4];
  int iids[4];
  test_definition_t this_test;

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card != affected_card) || (player != affected_card_controller))
      {
        return 0;
      }
      set_special_flags(player, card, 0x20000);
      if (check_special_flags2(player, card, -0x80000000) != 0)
      {
        return 0;
      }
      charge_mana(player, COLOR_COLORLESS, -1);
      if (spell_fizzled != 1)
      {
        set_x_for_x_spells(player, card, EVENT_CAST_SPELL, x_value);
        return 0;
      }
      return 0;
    }
    if (event == EVENT_RESOLVE_SPELL)
    {

      zone_count = 0;
      instance = get_card_instance(player, card);
      i = instance->info_slot;
      default_test_definition(&this_test, 2);
      this_test.cmc = i + 1;
      this_test.no_shuffle = 1;
      this_test.value_for_special_selection_function = (int)chosen;
      this_test.cmc_flag = 3;
      this_test.special_selection_function = test_not_in_array;
      chosen[0] = -1;
      chosen[1] = -1;
      chosen[2] = -1;
      chosen[3] = -1;
      cards[0] = -1;
      cards[1] = -1;
      cards[2] = -1;
      cards[3] = -1;
      iids[0] = -1;
      iids[1] = -1;
      iids[2] = -1;
      iids[3] = -1;
      do
      {

        if ((ai_is_speculating == 1) || ((player == 1 && ((trace_mode & 2U) == 0))))
        {
          this_test.message[0] = '\0';
        }
        else
        {
          sprintf(this_test.message, "Select a creature card with CMC %d or less (%d of 4)", i,
                  zone_count + 1);
        }
        tutored_card = new_global_tutor(player, player, 1, 1, 0, 2, &this_test);
        if ((tutored_card < 0) &&
            ((tutored_card = new_special_count_grave(player, &this_test),
              tutored_card == 0 ||
                  (tutored_card = new_global_tutor(player, player, 2, 1, 0, 2, &this_test),
                   tutored_card < 0))))
        {
          break;
        }
        original_iid = get_original_internal_card_id(player, tutored_card);
        chosen[zone_count] = original_iid;
        add_state(player, tutored_card, 0x20);
        cards[zone_count] = tutored_card;
        tutored_card = get_original_internal_card_id(player, tutored_card);
        iids[zone_count] = tutored_card;
        zone_count = zone_count + 1;
      } while (zone_count != 4);

      int rejected_count = zone_count > 2 ? 2 : zone_count;
      default_test_definition(&this_test, 0xff);
      this_test.id_flag = 1;
      this_test.id = 0x388;
      this_test.message[0] = '\0';
      if (zone_count > 0)
      {
        for (int rejected = 1; rejected <= rejected_count; ++rejected)
        {
          if (ai_is_speculating != 1 && ((trace_mode & 2U) != 0 || player != 0))
            sprintf(this_test.message, "Select a creature card (%d of %d)", rejected,
                    rejected_count);
          int selected =
              select_card_from_zone(1 - player, player, iids, zone_count, 1, 2, 0, &this_test);
          if (selected >= 0)
          {
            iids[selected] = get_internal_card_id_from_csv_id(0x388);
            put_on_top_of_deck(player, cards[selected]);
            cards[selected] = -1;
          }
        }
      }
      shuffle(player);
      if (zone_count > 0)
        for (int *cursor = cards; cursor != cards + zone_count; ++cursor)
          if (*cursor != -1)
            put_into_play(player, *cursor);
      kill_card(player, card, KILL_EXILE);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
}

/* ManalinkEh 2022: 0x23ce2b0; original cards/strixhaven.c:1960. */
int card_efreet_flamepainter(int player, int card, event_t event)
{
  /* Efreet Flamepainter	|3|R
   * Creature - Efreet Shaman 1/4
   * Double strike
   * Whenever this creature deals combat damage to a player, you may cast target instant or sorcery card from your graveyard without paying its mana cost. If that spell would be put into your graveyard, exile it instead. */

  int result;
  test_definition_t this_test;
  result = damage_dealt_by_me(player, card, event,
                              DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE |
                                  DDBM_TRIGGER_OPTIONAL);
  if (result != 0)
  {
    new_default_test_definition(&this_test, 0x38, "Select an instant or sorcery card.");
    result = new_special_count_grave(player, &this_test);
    if (result != 0)
    {
      result = new_select_a_card(player, player, 2, 0, 2, -1, &this_test);
      if (result != -1)
      {
        play_card_in_grave_for_free_and_exile_it(player, player, result);
        return 0;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23c8a30; original cards/strixhaven.c:1982. */
int ee_legacy(int player, int card, event_t event)
{
  if (trigger_condition == TRIGGER_LEAVE_PLAY && card == affected_card && player == affected_card_controller &&
      reason_for_trigger_controller == player)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int target_card = instance->targets[0].card;
    if (trigger_cause == target_card)
    {
      int target_player = instance->targets[0].player;
      if (trigger_cause_controller == target_player)
      {
        if (event == EVENT_TRIGGER)
        {
          card_instance_t *target = get_card_instance(target_player, target_card);
          event_result |= 2;
          target->kill_code = 4;
          instance->damage_target_card = -1;
          instance->damage_target_player = -1;
          return 0;
        }
        if (event == EVENT_RESOLVE_TRIGGER)
        {
          make_one_elemental_strixhaven(player, card, 1);
          return 0;
        }
        if (event == EVENT_END_TRIGGER)
        {
          kill_card(player, card, KILL_EXILE);
          return 0;
        }
      }
    }
  }
  if (event == EVENT_CLEANUP)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int target_player = instance->targets[0].player;
    int target_card = instance->targets[0].card;
    remove_special_flags(target_player, target_card, 0x4000);
    remove_special_flags2(target_player, target_card, 0x10000000);
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23ce370; original cards/strixhaven.c:2019. */
int card_elemental_expressionist(int player, int card, event_t event)
{
  /* Elemental Expressionist	|UR|UR|UR|UR
   * Creature - Orc Wizard 4/4
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, choose target creature you control. Until end of turn, it gains "If this creature would leave the battlefield, exile it instead of putting it anywhere else" and "When this creature is put into exile, create a 4/4 blue and red Elemental creature token." (Each instance of that ability triggers separately.) */

  int32_t target_player;
  card_instance_t *instance;
  card_instance_t *legacy;
  target_definition_t td;

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    if (can_target(&td) != 0)
    {
      if (new_pick_target(&td, "Select target creature you control.", 0, 0x80) != 0)
      {

        instance = get_card_instance(player, card);
        set_special_flags2(instance->targets[0].player, instance->targets[0].card, 0x10000000);
        set_special_flags(instance->targets[0].player, instance->targets[0].card, 0x4000);
        legacy = get_card_instance(player, create_targetted_legacy_effect(
                                               player, card, ee_legacy, instance->targets[0].player,
                                               instance->targets[0].card));
        target_player = instance->targets[0].player;
        legacy->targets[0].card = instance->targets[0].card;
        legacy->targets[0].player = target_player;
        instance->number_of_targets = '\0';
      }
    }
  }
  return hybrid(player, card, event);
}

/* ManalinkEh 2022: 0x23ce4d0; original cards/strixhaven.c:2053. */
int card_elemental_summoning(int player, int card, event_t event)
{
  /* Elemental Summoning	|3|UR|UR
   * Sorcery - Lesson
   * Create a 4/4 blue and red Elemental creature token. */

  int result;

  hybrid(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {
    make_one_elemental_strixhaven(player, card, 1);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23ce530; original cards/strixhaven.c:2075. */
int card_emergent_sequence(int player, int card, event_t event)
{
  /* Emergent Sequence	|1|G
   * Sorcery
   * Search your library for a basic land card, put it onto the battlefield tapped, then shuffle. That land becomes a 0/0 green and blue Fractal creature that's still a land. Put a +1/+1 counter on it for each land you had enter the battlefield under your control this turn. */

  int result;
  int num;
  card_instance_t *instance;
  test_definition_t this_test;

  if (event == EVENT_RESOLVE_SPELL)
  {

    new_default_test_definition(&this_test, 1, "Select a basic land card.");
    this_test.subtype = 0x2000;
    result = new_global_tutor(player, player, 1, 5, 1, 5, &this_test);
    if (-1 < result)
    {

      num = get_trap_condition(player, 0x402);
      add_counters(player, card, player, result, COUNTER_P1_P1, num);
      result = turn_into_creature(player, card, player, result, 0, 0, 0);
      instance = get_card_instance(player, result);
      result = get_id(player, card);
      instance->targets[2].card = result;
      result = get_sleighted_color_test(player, card, 0xc);
      instance->targets[9].card = 0x10a;
      instance->targets[6].card = result;
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23ce6b0; original cards/strixhaven.c:2100. */
int card_enthusiastic_study(int player, int card, event_t event)
{
  /* Enthusiastic Study	|2|R
   * Instant
   * Target creature gets +3/+1 and gains trample until end of turn.
   * Learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  card_instance_t *instance;
  target_definition_t td;

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
    else
    {
      if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
      {
        default_target_definition(player, card, &td, 2);
        instance = get_card_instance(player, card);
        if (event == EVENT_RESOLVE_SPELL)
        {
          if (valid_target(&td))
          {
            pump_ability_until_eot(player, card, instance->targets[0].player,
                                   instance->targets[0].card, 3, 1, 0x80, 0);
            learn(player, card);
          }
          kill_card(player, card, KILL_DESTROY);
          return 0;
        }
        return vanilla_instant_pump(player, card, event, 2, player, 3, 1, 0x80, 0);
      }
      if ((event != EVENT_CHANGE_TARGET) && (event != EVENT_CHECK_PUMP))
      {
        return 0;
      }
    }
  }
  default_target_definition(player, card, &td, 2);
  get_card_instance(player, card);
  return vanilla_instant_pump(player, card, event, 2, player, 3, 1, 0x80, 0);
}

/* ManalinkEh 2022: 0x23ce830; original cards/strixhaven.c:2126. */
int card_environmental_sciences(int player, int card, event_t event)
{
  /* Environmental Sciences	|2
   * Sorcery - Lesson
   * Search your library for a basic land card, reveal it, put it into your hand, then shuffle. You gain 2 life. */

  int result;
  if (event == EVENT_RESOLVE_SPELL)
  {
    tutor_basic_land(player, 0, 0);
    gain_life(player, 2);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23ce8a0; original cards/strixhaven.c:2146. */
int card_eureka_moment(int player, int card, event_t event)
{
  /* Eureka Moment	|2|G|U
   * Instant
   * Draw two cards. You may put a land card from your hand onto the battlefield. */

  int result;
  test_definition_t this_test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    draw_cards(player, 2);
    new_default_test_definition(&this_test, 1, "Select a land card.");
    new_global_tutor(player, player, 4, 4, 0, 1, &this_test);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23ce960; original cards/strixhaven.c:2175. */
int card_expanded_anatomy(int player, int card, event_t event)
{
  /* Expanded Anatomy	|3
   * Sorcery - Lesson
   * Put two +1/+1 counters on target creature. It gains vigilance until end of turn. */

  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      add_counters(player, card, instance->targets[0].player,
                               instance->targets[0].card, COUNTER_P1_P1, 2);
      pump_ability_until_eot(player, card, instance->targets[0].player,
                             instance->targets[0].card, 0, 0, 0, 8);
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return vanilla_instant_pump(player, card, event, 2, player, 2, 2, 0, 8);
}

/* ManalinkEh 2022: 0x23ceb00; original cards/strixhaven.c:2210. */
int card_exponential_growth(int player, int card, event_t event)
{
  /* Exponential Growth	|X|X|G|G
   * Sorcery
   * Until end of turn, double target creature's power X times. */

  int amount;
  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    get_card_instance(player, card);
    result = generic_spell(player, card, EVENT_CAN_CAST, 5, &td, NULL, 1, NULL);
  }
  else
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        default_target_definition(player, card, &td, 2);
        td.preferred_controller = player;
        get_card_instance(player, card);
        if ((card == affected_card) && (player == affected_card_controller))
        {
          multi_x_spell(player, card, EVENT_CAST_SPELL, 2);
          result =
              generic_spell(player, card, EVENT_CAST_SPELL, 5, &td, "TARGET_CREATURE", 1, NULL);
          return result;
        }
      }
    }
    else if ((event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET) ||
             (event == EVENT_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_SPELL)
      {
        result = valid_target(&td);
        if (result != 0)
        {

          for (amount = instance->info_slot; amount != 0; amount = amount + -1)
          {

            result = get_power(instance->targets[0].player, instance->targets[0].card);
            pump_until_eot_merge_previous(player, card, instance->targets[0].player,
                                          instance->targets[0].card, result, 0);
          }
        }
        kill_card(player, card, KILL_DESTROY);
      }
    }
    result = 0;
  }
  return result;
}

/* ManalinkEh 2022: 0x23ced10; original cards/strixhaven.c:2248. */
int card_expressive_iteration(int player, int card, event_t event)
{
  /* Expressive Iteration	|U|R
   * Sorcery
   * Look at the top three cards of your library. Put one of them into your hand, put one of them on the bottom of your library, and exile one of them. You may play the exiled card this turn. */

  if (event == EVENT_CAST_SPELL)
  {
    if (card == affected_card && player == affected_card_controller)
      ai_modifier += 0x60;
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    int count = count_deck(player);
    int remaining = count > 3 ? 3 : count;
    if (count != 0)
      for (int stage = 1; remaining != 0; ++stage, --remaining)
      {
        test_definition_t test;
        if (stage == 1)
        {
          new_default_test_definition(&test, 0xff, "Select a card to add to your hand.");
          test.no_shuffle = 1;
          test.create_minideck = (unsigned short)remaining;
          new_global_tutor(player, player, 1, 1, (unsigned int)(remaining == 3), 1, &test);
        }
        else if (stage == 2)
        {
          new_default_test_definition(&test, 0xff,
                                      "Select a card to put on the bottom of your library.");
          test.no_shuffle = 1;
          test.create_minideck = (unsigned short)remaining;
          new_global_tutor(player, player, 1, 10, (unsigned int)(remaining == 3), 1, &test);
        }
        else if (stage == 3)
          exile_top_x_card_you_may_play_them(player, card, 1, NULL, player, MPCFE_UNTIL_EOT);
      }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23cef00; original cards/strixhaven.c:2286. */
int card_extus_oriq_overlord(int player, int card, event_t event)
{
  /* Extus, Oriq Overlord	|1|W|B|B
   * Legendary Creature - Human Warlock 2/4
   * Double strike
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, return target nonlegendary creature card from your graveyard to your hand. */

  int result;
  test_definition_t this_test;

  mdfc_front_suppress_charging_mana(player, card, event);
  check_legend_rule(player, card, event);
  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    new_default_test_definition(&this_test, 2, "Select target nonlegendary creature,");
    this_test.subtype = 0x2001;
    this_test.subtype_flag = 1;
    result = new_special_count_grave(player, &this_test);
    if (result != 0)
    {
      result = graveyard_has_shroud(player);
      if (result == 0)
      {
        new_global_tutor(player, player, 2, 1, 1, 1, &this_test);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23cf000; original cards/strixhaven.c:2311. */
int card_awaken_the_blood_avatar(int player, int card, event_t event)
{
  /* Awaken the Blood Avatar	|6|B|R
   * Sorcery
   * As an additional cost to cast this spell, you may sacrifice any number of creatures. This spell costs |2 less to cast for each creature sacrificed this way.
   * Each opponent sacrifices a creature of their choice. Create a 3/6 black and red Avatar creature token with haste and "Whenever this token attacks, it deals 3 damage to each opponent." */

  enum
  {
    CHOICE_SAC = 1,
    CHOICE_NORMAL = 2
  };

  int c;
  int choice;
  int count_or_result;
  unsigned int available_or_discounted;
  unsigned int generic_cost;
  int updated_cost;
  unsigned int sacrifices_needed;
  card_instance_t *instance;
  char sac[151];

  if (event == EVENT_CAN_CAST)
  {
    test_definition_t test;

    default_test_definition(&test, 2);
    count_or_result = max_can_sacrifice_as_cost(player, card, &test);

    updated_cost = get_updated_casting_cost(player, card, -1, EVENT_CAN_CAST, -1);
    available_or_discounted = updated_cost + count_or_result * -2;
    count_or_result = has_mana_multi(
        player, available_or_discounted & (int)(available_or_discounted ^ 0xffffffff) >> 0x1f, 1, 0,
        0, 1, 0);
    return (unsigned int)(count_or_result != 0);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      test_definition_t test2;

      new_default_test_definition(&test2, 2, "Select a creature to sacrifice");
      available_or_discounted = max_can_sacrifice_as_cost(player, card, &test2);
      generic_cost = get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, -1);
      choice = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_STORAGE,
                           "Sacrifice creatures", available_or_discounted, 5, "Cast Normally",
                           has_mana_multi(player, generic_cost, 1, 0, 0, 1, 0), 10, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_SAC)
      {

        sacrifices_needed = round_up_value(generic_cost);

        if ((int)available_or_discounted < (int)sacrifices_needed)
        {
          sacrifices_needed = available_or_discounted;
        }
        test2.qty = (unsigned short)sacrifices_needed;
        count_or_result = mark_sacrifice(player, card, player,
                                         SAC_AS_COST | SAC_JUST_MARK | SAC_DONE, &test2, sac);
        if (spell_fizzled != 1)
        {

          generic_cost = generic_cost + count_or_result * -2;
          charge_mana_multi(player, generic_cost & (int)(generic_cost ^ 0xffffffff) >> 0x1f, 1, 0,
                            0, 1, 0);
        }
        if (spell_fizzled != 1)
        {

          if (0 < active_cards_count[player])
          {
            c = 0;
            do
            {
              if (sac[c] != '\0')
              {
                instance = get_card_instance(player, c);
                instance->state = instance->state & 0xffefffff;
                kill_card(player, c, KILL_SACRIFICE);
              }
              c = c + 1;
            } while (c < active_cards_count[player]);
          }
          return get_id(player, card);
        }
        ai_modifier = ai_modifier + -99;
        return 0;
      }
      if (choice == CHOICE_NORMAL)
      {
        charge_mana_multi(player, generic_cost, 1, 0, 0, 1, 0);
      }
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    token_generation_t token;
    player_sacrifices_a_permanent(player, card, 1 - player, 2, SAC_CAUSED);
    default_token_definition(player, card, CARD_ID_AVATAR, &token);
    token.pow = 3;
    token.tou = 6;
    token.color_forced = '\x12';
    token.special_infos = 67;
    generate_token(&token);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23cf450; original cards/strixhaven.c:2397. */
int card_eyetwitch(int player, int card, event_t event)
{
  /* Eyetwitch	|B
   * Creature - Eye Bat 1/1
   * Flying
   * When this creature dies, learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  int result;

  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  learn(player, card);
  return 0;
}

/* ManalinkEh 2022: 0x23cf490; original cards/strixhaven.c:2410. */
int card_fervent_mastery(int player, int card, event_t event)
{
  /* Fervent Mastery	|3|R|R
   * Sorcery
   * You may pay |2|R|R rather than pay this spell's mana cost.
   * If the |2|R|R cost was paid, an opponent discards any number of cards, then draws that many cards.
   * Search your library for up to three cards, put them into your hand, shuffle, then discard three cards at random. */

  int p;
  int choice;
  card_instance_t *instance;
  int result_or_index;
  card_instance_t *payment_state;
  int card_00;
  int fizzle_snapshot;
  unsigned int alternate_available;
  test_definition_t this_test;

  if (event == EVENT_MODIFY_COST)
  {
    if (has_mana_multi(player, 2, 0, 0, 0, 2, 0) != 0)
    {
      null_casting_cost(player, card);
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      return 0;
    }
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
    return 0;
  }
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card != affected_card) || (player != affected_card_controller))
      {
        return 0;
      }
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {

        instance->targets[1].player = 0;
        payment_state = get_card_instance(player, card);
        if ((payment_state->info_slot == 1) &&
            (result_or_index = played_for_free(player, card), result_or_index == 0))
        {
          result_or_index = is_token(player, card);
          alternate_available = (unsigned int)(result_or_index == 0);
        }
        else
        {
          alternate_available = 0;
        }
        choice =
            dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1,
                        "Pay alternate mana cost", alternate_available, 10, "Pay full mana cost",
                        has_mana_to_cast_id(player, EVENT_CAST_SPELL, 0x5887), 0xf, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          fizzle_snapshot = spell_fizzled;
        }
        else if (choice == 1)
        {
          charge_mana_multi(player, 2, 0, 0, 0, 2, 0);
          fizzle_snapshot = spell_fizzled;
          if (spell_fizzled == 1)
          {
            return 0;
          }
          instance->targets[1].player = 0x42;
        }
        else
        {
          fizzle_snapshot = spell_fizzled;
          if (choice == 2)
          {
            charge_mana_from_id(player, card, EVENT_CAST_SPELL, 0x5887);
            fizzle_snapshot = spell_fizzled;
          }
        }
        if (fizzle_snapshot != 0)
        {
          return 0;
        }
      }
      return basic_spell(player, card, event);
    }
    if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
        (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (instance->targets[1].player == 0x42)
    {

      p = 1 - player;
      new_default_test_definition(&this_test, 0xff, "Select a card to discard.");
      result_or_index = 0;
      if (hand_count[p] != 0)
      {
        result_or_index = 0;

        while (card_00 = new_select_a_card(p, p, 4, 0, 3, -1, &this_test), -1 < card_00)
        {
          result_or_index = result_or_index + 1;
          discard_card(p, card_00);
          if (((result_or_index == 3) &&
               ((ai_is_speculating == 1 ||
                 ((p == 1 && (((unsigned char)trace_mode & 2) == 0)))))) ||
              (hand_count[p] == 0))
            break;
        }
      }
      draw_cards(p, result_or_index);
    }
    new_default_test_definition(&this_test, 0xff, "Select a card.");
    this_test.qty = 3;
    new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
    new_multidiscard(player, 3, 1, player);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23cf8f0; original cards/strixhaven.c:2487. */
int card_field_trip(int player, int card, event_t event)
{
  /* Field Trip	|2|G
   * Sorcery
   * Search your library for a basic Forest card, put that card onto the battlefield tapped, then shuffle.
   * Learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  int result;
  const char *msg;
  test_definition_t this_test;

  if (event == EVENT_RESOLVE_SPELL)
  {
    msg = get_hacked_land_text(player, card, "Select a basic  %s card.", 0x5001);
    new_default_test_definition(&this_test, 0xff, msg);
    this_test.subtype = get_hacked_subtype(player, card, 0x5001);
    this_test.sub2 = 0x2000;
    this_test.subtype_flag = 4;
    new_global_tutor(player, player, 1, 5, 0, 5, &this_test);
    learn(player, card);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23c7530; original cards/strixhaven.c:2509. */
static int fdoc_legacy(int player, int card, event_t event)
{
  etb_ability_global(player, event, 2, TYPE_CREATURE, NULL, player, card);
  if (event == EVENT_ETB)
  {
    add_counters(player, card, trigger_cause_controller, trigger_cause, COUNTER_P1_P1,
                             1);
    pump_ability_until_eot(player, card, trigger_cause_controller, trigger_cause, 0, 0, 0, 4);
    return 0;
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x23cf9e0; original cards/strixhaven.c:2523. */
int card_first_day_of_class(int player, int card, event_t event)
{
  /* First Day of Class	|1|R
   * Instant
   * Whenever a creature you control enters this turn, put a +1/+1 counter on it and it gains haste until end of turn.
   * Learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  int i;

  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      i = 1;
      do
      {

        if (has_mana(player, 7, i) != 0)
        {
          ai_modifier = ai_modifier + 8;
        }
        i = i + 1;
      } while (i != 0x10);
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    create_legacy_effect(player, card, fdoc_legacy);
    learn(player, card);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23cfaa0; original cards/strixhaven.c:2550. */
int card_flamescroll_celebrant(int player, int card, event_t event)
{
  /* Flamescroll Celebrant	|1|R
   * Creature - Human Shaman 2/1
   * Whenever an opponent activates an ability that isn't a mana ability, this creature deals 1 damage to that player.
   * |1|R: This creature gets +2/+0 until end of turn. */

  int result;
  card_instance_t *instance;

  mdfc_front(player, card, event);
  if (((event == EVENT_PLAY_ABILITY) || (event == EVENT_TAPPED_TO_PLAY_ABILITY)) &&
      (1 - player == affected_card_controller))
  {
    instance = get_card_instance(1 - player, affected_card);
    if (((cards_data[instance->internal_card_id].extra_ability & 0x1000) == 0) ||
        (tapped_for_mana_color < 0))
    {
      instance = in_play(player, card);
      if (instance != NULL)
      {
        result = is_humiliated(player, card, AL_TRIG);
        if (result == 0)
        {
          damage_player(affected_card_controller, 1, player, card);
        }
      }
    }
  }
  result = generic_shade(player, card, event, 0, 1, 0, 0, 0, 1, 0, 2, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x23c7630; original cards/strixhaven.c:2571. */
int ris_legacy(int player, int card, event_t event)
{
  test_definition_t test;

  if ((event == EVENT_ABILITIES) || (event == EVENT_CAST_SPELL))
  {
    default_test_definition(&test, 0x100);
    remove_abilities_by_test(1 - player, event, &test, AL_LOYALTY);
  }
  if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x23cfbe0; original cards/strixhaven.c:2584. */
int card_revel_in_silence(int player, int card, event_t event)
{
  /* Revel in Silence	|W|W
   * Instant
   * Your opponents can't cast spells or activate planeswalkers' loyalty abilities this turn.
   * Exile ~. */

  int result;

  mdfc_back(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {

    target_player_cant_cast_type(player, card, 1 - player, 0x1fe);
    result = create_legacy_effect(player, card, ris_legacy);
    add_status(player, result, 0x10000);
    kill_card(player, card, KILL_EXILE);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23cfc80; original cards/strixhaven.c:2611. */
int card_fractal_summoning(int player, int card, event_t event)
{
  /* Fractal Summoning	|X|GU|GU
   * Sorcery - Lesson
   * Create a 0/0 green and blue Fractal creature token. Put X +1/+1 counters on it. */

  int result;
  card_instance_t *instance;

  modify_cost_for_hybrid_spells(player, card, event, 0);
  if (event == EVENT_CAN_CAST)
  {
    result = generic_spell(player, card, EVENT_CAN_CAST, 4, NULL, NULL, 1, NULL);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = hybrid_casting(player, card, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
      }
      else
      {
        charge_mana(player, COLOR_COLORLESS, -1);
        if (spell_fizzled != 1)
        {
          set_x_for_x_spells(player, card, EVENT_CAST_SPELL, x_value);
        }
      }
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    make_one_fractal(player, card, 1, instance->info_slot);
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x23cfdd0; original cards/strixhaven.c:2642. */
int card_fracture2(int player, int card, event_t event)
{
  /* Fracture	|W|B
   * Instant
   * Destroy target artifact, enchantment, or planeswalker. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x144);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td,
                         "Select target artifact, enchantment, or planeswalker.", 1, NULL);
}

/* ManalinkEh 2022: 0x23cff20; original cards/strixhaven.c:2672. */
int card_frostboil_snarl(int player, int card, event_t event)
{
  /* Frostboil Snarl	""
   * Land
   * As this land enters, you may reveal an Island or Mountain card from your hand. If you don't, this land enters tapped.
   * |T: Add |U or |R. */

  int result;

  soi_need_subtype_land(player, card, event, SUBTYPE_ISLAND, SUBTYPE_MOUNTAIN);
  result = plain_mana_producer(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23cff70; original cards/strixhaven.c:2687. */
int card_furycalm_snarl(int player, int card, event_t event)
{
  /* Furycalm Snarl	""
   * Land
   * As this land enters, you may reveal a Mountain or Plains card from your hand. If you don't, this land enters tapped.
   * |T: Add |R or |W. */

  int result;

  soi_need_subtype_land(player, card, event, SUBTYPE_PLAINS, SUBTYPE_MOUNTAIN);
  result = plain_mana_producer(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23cffc0; original cards/strixhaven.c:2704. */
int card_gnarled_professor(int player, int card, event_t event)
{
  /* Gnarled Professor	|2|G|G
   * Creature - Treefolk Druid 5/4
   * Trample
   * When this creature enters, learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  learn(player, card);
  return 0;
}

/* ManalinkEh 2022: 0x23d0010; original cards/strixhaven.c:2723. */
int card_golden_ratio(int player, int card, event_t event)
{
  /* Golden Ratio	|1|G|U
   * Sorcery
   * Draw a card for each different power among creatures you control. */

  if ((event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) ||
      event == EVENT_RESOLVE_SPELL)
  {
    int8_t powers[255] = {0};
    int distinct = 0;
    for (int permanent = 0; permanent < active_cards_count[player]; ++permanent)
      if (in_play(player, permanent) && is_what(player, permanent, 2))
      {
        int power = get_power(player, permanent);
        if (!powers[power])
        {
          powers[power] = 1;
          ++distinct;
        }
      }
    if (event == EVENT_CAST_SPELL)
      ai_modifier += distinct * 10;
    else
    {
      draw_cards(player, distinct);
      kill_card(player, card, KILL_DESTROY);
    }
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23d02b0; original cards/strixhaven.c:2764. */
int card_guiding_voice(int player, int card, event_t event)
{
  /* Guiding Voice	|W
   * Sorcery
   * Put a +1/+1 counter on target creature.
   * Learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  if (IS_GS_EVENT(player, card, event))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    card_instance_t *instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      if (valid_target(&td))
      {
        add_counters(player, card, instance->targets[0].player,
                     instance->targets[0].card, COUNTER_P1_P1, 1);
        learn(player, card);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return vanilla_instant_pump(player, card, event, ANYBODY, player, 1, 1, 0, 0);
}

/* ManalinkEh 2022: 0x23d0410; original cards/strixhaven.c:2790. */
int card_hall_monitor(int player, int card, event_t event)
{
  /* Hall Monitor	|R
   * Creature - Lizard Shaman 1/1
   * Haste
   * |1|R, |T: Target creature can't block this turn. */

  haste(player, card, event);

  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 2);
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_ACTIVATE)
  {
    generic_activated_ability(player, card, EVENT_ACTIVATE, 5, 1, 0, 0, 0, 1, 0, 0, &td,
                              "TARGET_CREATURE");
    if (spell_fizzled != 1)
      ai_modifier += 0x18;
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
    pump_ability_until_eot_no_repeat(instance->parent_controller, instance->parent_card,
                                     instance->targets[0].player, instance->targets[0].card, 0,
                                     0x20);
  return generic_activated_ability(player, card, event, 5, 1, 0, 0, 0, 1, 0, 0, &td,
                                   "TARGET_CREATURE");
}

/* ManalinkEh 2022: 0x23d0620; original cards/strixhaven.c:2828. */
int card_harness_infinity(int player, int card, event_t event)
{
  /* Harness Infinity	|1|B|B|B|G|G|G
   * Instant
   * Exchange your hand and graveyard.
   * Exile ~. */

  if (event == EVENT_RESOLVE_SPELL)
  {
    int ids[151];
    int saved = 0;
    for (int hand_card = active_cards_count[player] - 1; hand_card >= 0; --hand_card)
      if (in_hand(player, hand_card))
      {
        ids[saved] = get_original_internal_card_id(player, hand_card);
        obliterate_card(player, hand_card);
        ++saved;
      }
    for (int index = count_graveyard(player) - 1; index >= 0; --index)
    {
      add_card_to_hand(player, graveyard_ptr_mutable[player][index]);
      remove_card_from_grave(player, index);
    }
    for (int index = saved - 1; index >= 0; --index)
      raw_put_iid_on_top_of_graveyard(player, ids[index]);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23c8600; original cards/strixhaven.c:2865. */
static int hofri_legacy(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;
  int position_in_exile;

  result = attached_leaves_play(player, card, event, 1, 0);
  if (result == 0)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  result = instance->targets[1].player;
  position_in_exile = find_iid_in_rfg(result, instance->targets[1].card);
  if (-1 < position_in_exile)
  {
    from_exile_to_graveyard(result, position_in_exile);
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x23c76d0; original cards/strixhaven.c:2877. */
static void add_hofry_legacy(token_generation_t *token, int card_added, int number)
{
  unsigned int packed_source;
  int card;
  card_instance_t *instance;

  card = create_targetted_legacy_effect(token->s_player, token->s_card, hofri_legacy,
                                        token->t_player, card_added);
  instance = get_card_instance(token->s_player, card);
  packed_source = token->action_argument;
  instance->targets[1].player = packed_source & 0xf;
  instance->targets[1].card = (int)packed_source >> 4;
  add_a_subtype_permanently(token->s_player, token->s_card, token->t_player, card_added,
                            SUBTYPE_SPIRIT);
  return;
}

#ifndef SHANDALAR
static void create_hofri_token(int player, int card, int iid, int owner, int original_iid)
{
  token_generation_t token;
  default_token_definition(player, card, cards_data[iid].id, &token);
  token.action_argument = (int32_t)(((uint32_t)original_iid << 4) | (uint32_t)owner);
  token.special_code_on_generation = add_hofry_legacy;
  generate_token(&token);
}
#endif

/* ManalinkEh 2022: 0x23c6dc0; original cards/strixhaven.c:2892. */
static int hofri_effect(int player, int card, event_t event)
{
  unsigned int packed_source;
  card_instance_t *instance;
  card_instance_t *dying;
  unsigned int deaths;
  int iid_or_result;
  int grave_iid;
  int grave_id;
  target_t *stored_death;
  unsigned int grave_owner;
  token_generation_t token;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (iid_or_result =
               get_id((int)instance->damage_target_player, instance->damage_target_card),
           iid_or_result != 0x589e))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (iid_or_result = effect_follows_control_of_attachment(player, card, 0x4700),
         iid_or_result != 0))
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (iid_or_result = effect_follows_control_of_attachment(player, card, event),
         iid_or_result != 0))
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
            ((affected_card == instance->damage_target_card &&
              (instance->damage_target_player == affected_card_controller))))
        {
          instance->damage_target_card = -1;
          instance->damage_target_player = -1;
          remove_status(player, card, 0x10000);
          return 0;
        }
        count_for_gfp_ability_and_store_values(player, card, 0x77, player, 2, NULL, 0x10d0, 0);
      }
    }
  }
  deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_GRAVEYARD_FROM_PLAY && deaths == 0)
    return 0;
  if (deaths != 0)
  {
    iid_or_result = check_status(player, card, 0x10000);
    if (iid_or_result == 0)
    {

      instance = get_card_instance(player, card);
      if ((deaths & 0xff) != 0)
      {
        stored_death = instance->targets;

        do
        {
          packed_source = stored_death->player;
          iid_or_result = stored_death->card;
          stored_death->card = 0;
          grave_owner = packed_source & 0xf;
          stored_death->player = 0;
          grave_iid = *graveyard_ptr_mutable[grave_owner];
          if (grave_iid != -1)
          {
            grave_id = 0;
            do
            {
              if (iid_or_result == grave_iid)
              {
                rfg_card_from_grave(grave_owner, grave_id);
                default_token_definition(player, card, (unsigned int)cards_data[iid_or_result].id,
                                         &token);
                token.special_code_on_generation = add_hofry_legacy;
                token.action_argument = packed_source;
                generate_token(&token);
                break;
              }
              grave_id = grave_id + 1;
              grave_iid = graveyard_ptr_mutable[grave_owner][grave_id];
            } while (grave_iid != -1);
          }
          stored_death = stored_death + 1;
        } while (stored_death != instance->targets + (deaths & 0xff));
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) &&
      (iid_or_result = check_status(player, card, 0x10000), iid_or_result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d0750; original cards/strixhaven.c:2957. */
int card_hofri_ghostforge(int player, int card, event_t event)
{
  /* Hofri Ghostforge	|3|R|W
   * Legendary Creature - Dwarf Cleric 4/5
   * Spirits you control get +1/+1 and have trample and haste.
   * Whenever another nontoken creature you control dies, exile it. If you do, create a token that's a copy of that creature, except it's a Spirit in addition to its other types and it has "When this token leaves the battlefield, return the exiled card to its owner's graveyard." */

  unsigned int packed_source;
  unsigned int deaths;
  card_instance_t *instance;
  int index_or_iid;
  int type_or_grave_iid;
  int grave_id;
  unsigned int grave_owner;
  target_t *stored_death;

  check_legend_rule(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      bool found = false;
      if (0 < active_cards_count[player])
      {
        index_or_iid = 0;
        do
        {
          instance = in_play(player, index_or_iid);
          if ((((instance != NULL) && (type_or_grave_iid = is_what(player, index_or_iid, 0x80),
                                       type_or_grave_iid != 0)) &&
               (instance = get_card_instance(player, index_or_iid),
                instance->info_slot_fn == hofri_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            found = true;
            break;
          }
          index_or_iid = index_or_iid + 1;
        } while (index_or_iid < active_cards_count[player]);
      }
      if (!found)
      {
        index_or_iid = create_targetted_legacy_effect(player, card, hofri_effect, player, card);
        instance = get_card_instance(player, index_or_iid);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, index_or_iid, 0x10000);
      }
    }
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.not_me = 1;
    count_for_gfp_ability_and_store_values(player, card, 0x77, player, 2, &test, 0x10d0, 0);
  }
  deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (deaths == 0 && (event == EVENT_STATIC_EFFECTS || event == EVENT_GRAVEYARD_FROM_PLAY))
    return 0;
  if (deaths != 0)
  {
    instance = get_card_instance(player, card);
    if (instance->kill_code != 0 && event == EVENT_GRAVEYARD_FROM_PLAY)
      return 0;
    if (instance->kill_code == 0)
    {
      if ((deaths & 0xff) != 0)
      {
        stored_death = instance->targets;
        do
        {
          packed_source = stored_death->player;
          index_or_iid = stored_death->card;
          stored_death->card = 0;
          grave_owner = packed_source & 0xf;
          stored_death->player = 0;
          type_or_grave_iid = *graveyard_ptr_mutable[grave_owner];
          if (type_or_grave_iid != -1)
          {
            grave_id = 0;
            do
            {
              if (index_or_iid == type_or_grave_iid)
              {
                token_generation_t token;
                rfg_card_from_grave(grave_owner, grave_id);
                default_token_definition(player, card, (unsigned int)cards_data[index_or_iid].id,
                                         &token);
                token.special_code_on_generation = add_hofry_legacy;
                token.action_argument = (int32_t)(packed_source);
                generate_token(&token);
                break;
              }
              grave_id = grave_id + 1;
              type_or_grave_iid = graveyard_ptr_mutable[grave_owner][grave_id];
            } while (type_or_grave_iid != -1);
          }
          stored_death = stored_death + 1;
        } while (stored_death != instance->targets + (deaths & 0xff));
      }
    }
  }
  if (((((uint32_t)(event - EVENT_POWER) < 3) && (instance = in_play(player, card), instance != NULL)) &&
       ((index_or_iid = is_humiliated(player, card, AL_STATIC),
         index_or_iid == 0 &&
             (((affected_card_controller == player && (affected_card != card)) &&
               (instance = in_play(player, affected_card), instance != NULL)))))) &&
      (index_or_iid = has_subtype(affected_card_controller, affected_card, SUBTYPE_SPIRIT),
       index_or_iid != 0))
  {
    if (event < EVENT_ABILITIES)
    {
      event_result = event_result + 1;
    }
    else
    {
      haste(affected_card_controller, affected_card, EVENT_ABILITIES);
      event_result = event_result | 0x80;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d0b50; original cards/strixhaven.c:3047. */
int card_honor_troll(int player, int card, event_t event)
{
  /* Honor Troll	|2|G
   * Creature - Troll Druid 2/3
   * Vigilance
   * If you would gain life, you gain that much life plus 1 instead.
   * This creature gets +2/+1 as long as you have 25 or more life. */

  enum
  {
    XTRIGGER_REPLACE_GAIN_LIFE = 16778752
  };
  vigilance(player, card, event);
  xtrigger_t trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
    trigger = xtrigger_impl_value_dont_use_directly;
  if (trigger == XTRIGGER_REPLACE_GAIN_LIFE)
  {
    if (card != affected_card)
      return 0;
    if (player == affected_card_controller && reason_for_trigger_controller == player &&
        trigger_cause_controller == player && life_gained > 0 &&
        !is_humiliated(player, card, AL_TRIG))
    {
      if (event == EVENT_TRIGGER)
      {
        event_result |= 2;
        return 0;
      }
      if (event == EVENT_RESOLVE_TRIGGER)
      {
        ++life_gained;
        return 0;
      }
    }
  }
  if ((uint32_t)(event - EVENT_POWER) > 1 || card != affected_card || player != affected_card_controller)
    return 0;
  if (!is_humiliated(player, card, AL_STATIC) && life[player] > 0x18)
  {
    if (event == EVENT_POWER)
      event_result += 2;
    else if (event == EVENT_TOUGHNESS)
      event_result += 1;
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d0c90; original cards/strixhaven.c:3086. */
int card_hunt_for_specimens(int player, int card, event_t event)
{
  /* Hunt for Specimens	|1|B
   * Sorcery
   * Create a 1/1 black and green Pest creature token with "When this token dies, you gain 1 life."
   * Learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    make_pest(player, card, 1);
    learn(player, card);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23d0cf0; original cards/strixhaven.c:3102. */
int card_igneous_inspiration(int player, int card, event_t event)
{
  /* Igneous Inspiration	|2|R
   * Sorcery
   * ~ deals 3 damage to any target.
   * Learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      damage_target0(player, card, 3);
      learn(player, card);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_ANY", 1, NULL);
}

/* ManalinkEh 2022: 0x23d0e40; original cards/strixhaven.c:3127. */
int card_illuminate_history(int player, int card, event_t event)
{
  /* Illuminate History	|2|R|R
   * Sorcery - Lesson
   * Discard any number of cards, then draw that many cards. Then if there are seven or more cards in your graveyard, create a 3/2 red and white Spirit creature token. */

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
    ai_modifier += 0x18;
  else if (event == EVENT_RESOLVE_SPELL)
  {
    test_definition_t test;
    new_default_test_definition(&test, 0xff, "Select a card to discard.");
    int disc = 0;
    if (hand_count[player] != 0)
      do
      {
        int selected = new_select_a_card(player, player, 4, 0, 3, -1, &test);
        if (selected < 0)
          break;
        ++disc;
        discard_card(player, selected);
        if (disc == 3 && (ai_is_speculating == 1 || (player == AI && !(trace_mode & 2))))
          break;
      } while (hand_count[player] != 0);
    if (count_graveyard(player) > 6)
      make_one_spirit_strixhaven(player, card, 1);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23d0f90; original cards/strixhaven.c:3172. */
int card_ingenious_mastery(int player, int card, event_t event)
{
  /* Ingenious Mastery	|X|2|U
   * Sorcery
   * You may pay |2|U rather than pay this spell's mana cost.
   * If the |2|U cost was paid, you draw three cards, then an opponent creates two Treasure tokens and they scry 2. If that cost wasn't paid, you draw X cards. */

  if (event == EVENT_MODIFY_COST)
  {
    if (has_mana_multi(player, 2, 0, 1, 0, 0, 0))
    {
      null_casting_cost(player, card);
      get_card_instance(player, card)->info_slot = 1;
    }
    else
      get_card_instance(player, card)->info_slot = 0;
    return 0;
  }

  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;

  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card || player != affected_card_controller)
      return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
    instance->targets[1].player = 0;
    card_instance_t *state = get_card_instance(player, card);
    if (state->info_slot != 1 || is_token(player, card))
      return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
    int choice = 1;
    if (has_mana_to_cast_id(player, EVENT_CAST_SPELL, 0x58a6))
      choice =
          dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                      "Pay alternate mana cost", 1, 0x14, "Pay full mana cost", 1, 10, 0, 0, 0, 0);
    int fizzle_snapshot;
    if (!choice)
    {
      spell_fizzled = 1;
      fizzle_snapshot = spell_fizzled;
    }
    else if (choice == 1)
    {
      charge_mana_multi(player, 2, 0, 1, 0, 0, 0);
      fizzle_snapshot = spell_fizzled;
      if (spell_fizzled == 1)
        return 0;
      instance->targets[1].player = 0x42;
    }
    else
    {
      fizzle_snapshot = spell_fizzled;
      if (choice == 2)
      {
        charge_mana_from_id(player, -1, EVENT_CAST_SPELL, 0x58a6);
        if (spell_fizzled == 1)
          return 0;
        charge_mana(player, COLOR_COLORLESS, -1);
        fizzle_snapshot = spell_fizzled;
      }
    }
    if (fizzle_snapshot != 0)
      return 0;
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (instance->targets[1].player == 0x42)
    {
      draw_cards(player, 3);
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_TREASURE, &token);
      token.qty = 2;
      token.t_player = 1 - player;
      generate_token(&token);
      scry(1 - player, 2);
    }
    else
      draw_cards(player, instance->info_slot);
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
}

/* ManalinkEh 2022: 0x23d1330; original cards/strixhaven.c:3244. */
int card_inkling_summoning(int player, int card, event_t event)
{
  /* Inkling Summoning	|1|WB|WB
   * Sorcery - Lesson
   * Create a 2/1 white and black Inkling creature token with flying. */

  if (event == EVENT_RESOLVE_SPELL)
  {
    make_inkling(player, card, 1, 0);
    kill_card(player, card, KILL_DESTROY);
  }
  return hybrid(player, card, event);
}

/* ManalinkEh 2022: 0x23d1390; original cards/strixhaven.c:3259. */
int card_introduction_to_annihilation(int player, int card, event_t event)
{
  /* Introduction to Annihilation	|5
   * Sorcery - Lesson
   * Exile target nonland permanent. Its controller draws a card. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.illegal_type = TYPE_LAND;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
      draw_cards(instance->targets[0].player, 1);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_NONLAND_PERMANENT", 1, NULL);
}

/* ManalinkEh 2022: 0x23d1500; original cards/strixhaven.c:3290. */
int card_jadzi_oracle_of_arcavios(int player, int card, event_t event)
{
  /* Jadzi, Oracle of Arcavios	|6|U|U
   * Legendary Creature - Human Wizard 5/5
   * Discard a card: Return Jadzi to its owner's hand.
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, reveal the top card of your library. If it's a nonland card, you may cast it by paying |1 rather than paying its mana cost. If it's a land card, put it onto the battlefield. */

  int choice;
  int value;
  card_instance_t *activation;
  card_instance_t *source;
  char buffer[100];

  mdfc_front(player, card, event);
  check_legend_rule(player, card, event);
  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {

    value = *deck_ptr[player];
    reveal_card_iid(player, card, value);
    if (is_what(-1, value, 1) != 0)
    {
      put_into_play_a_card_from_deck(player, player, 0);
      return 0;
    }

    if (can_legally_play_iid(player, value) == 0)
    {
      return 0;
    }
    scnprintf(buffer, 100, "Cast %s", cards_ptr[cards_data[value].id]->name);
    value = has_mana(player, 0, 1);
    choice = dialog_impl(player, card, EVENT_SPA, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1,
                         buffer, value, 10, "Decline", 1, 1, 0, 0, 0, 0);
    if (choice == 2)
    {
      return 0;
    }
    charge_mana(player, COLOR_COLORLESS, 1);
    if (spell_fizzled == 1)
    {
      return 0;
    }
    play_card_in_deck_for_free(player, player, 0);
    return 0;
  }

  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    activation = get_card_instance(player, card);
    source = in_play(activation->parent_controller, activation->parent_card);
    if (source)
      bounce_permanent(activation->parent_controller, activation->parent_card);
  }
  return generic_activated_ability(player, card, event, 0x20, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x23d17f0; original cards/strixhaven.c:3339. */
int card_journey_to_the_oracle(int player, int card, event_t event)
{
  /* Journey to the Oracle	|2|G|G
   * Sorcery
   * You may put any number of land cards from your hand onto the battlefield. Then if you control eight or more lands, you may discard a card. If you do, return ~ to its owner's hand. */

  mdfc_back(player, card, event);
  if (event != EVENT_RESOLVE_SPELL)
    return basic_spell(player, card, event);
  test_definition_t test;
  new_default_test_definition(&test, 1, "Select a Land card to put into play.");
  test.id = 0x388;
  test.id_flag = 1;
  int marker = get_internal_card_id_from_csv_id(0x388);
  int lands[151];
  int iids[151];
  int land_count = 0;
  for (int hand_card = 0; hand_card < active_cards_count[player]; ++hand_card)
    if (in_hand(player, hand_card) && is_what(player, hand_card, 1))
    {
      lands[land_count] = hand_card;
      iids[land_count] = get_original_internal_card_id(player, hand_card);
      ++land_count;
    }
  if (land_count > 0)
    for (int selected = 0; selected != land_count; ++selected)
    {
      int index = select_card_from_zone(player, player, iids, land_count, 0, 1, -1, &test);
      if (index == -1)
        break;
      iids[index] = marker;
    }
  // Native 0x023d1983 starts at land_count, including the uninitialized entry.
  // Preserve that boundary (also for an empty hand) during source recovery.
  if (land_count >= 0)
    for (int index = land_count; index >= 0; --index)
      if (iids[index] == marker)
        put_into_play(player, lands[index]);
  if (hand_count[player] != 0 && count_subtype(player, 1, -1) > 7)
  {
    new_default_test_definition(&test, 0xff, "Select a card to discard.");
    int discard = new_select_a_card(player, player, 4, 0, 3, -1, &test);
    if (discard != -1)
    {
      new_discard_card(player, discard, 2, player);
      bounce_permanent(player, card);
      return 0;
    }
  }
  kill_card(player, card, KILL_BURY);
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23c7760; original cards/strixhaven.c:3395. */
static int kasmina_effect(int player, int card, event_t event)
{
  card_instance_t *effect;
  if (event == EVENT_STATIC_EFFECTS)
  {
    if (card == affected_card && player == affected_card_controller)
    {
      effect = get_card_instance(player, card);
      if (effect->damage_target_player >= 0 &&
          get_id(effect->damage_target_player, effect->damage_target_card) != CARD_ID_KASMINA_ENIGMA_SAGE)
        kill_card(player, card, KILL_EXILE);
    }
    effect = get_card_instance(player, card);
    if (effect->damage_target_player < 0 ||
        effect_follows_control_of_attachment(player, card, 0x4700))
      return 0;
  }
  else
  {
    effect = get_card_instance(player, card);
    if (effect->damage_target_player >= 0 &&
        effect_follows_control_of_attachment(player, card, event))
      return 0;
    if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
    {
      effect = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        for (int candidate = 0; candidate < active_cards_count[player]; ++candidate)
        {
          int attached_card = effect->damage_target_card;
          card_instance_t *planeswalker = in_play(player, candidate);
          if (planeswalker)
          {
            int eligible = is_planeswalker(player, candidate);
            if (attached_card != candidate && eligible)
            {
              int iid = get_internal_card_id_from_csv_id(CARD_ID_KASMINA_ENIGMA_SAGE);
              if (call_card_fn(cards_data[iid].code_pointer, planeswalker, player, candidate,
                               EVENT_CAN_ACTIVATE))
                return 1;
            }
          }
        }
        return 0;
      }
    }
  }
  if (event != EVENT_ACTIVATE)
  {
    if (event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
      return 0;
    effect = get_card_instance(player, card);
  }
  if (event == EVENT_ACTIVATE)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x100);
    td.illegal_abilities = 0;
    td.allowed_controller = player;
    if (can_target(&td) && new_pick_target(&td, "Select a planeswalker to activate.", 1, 0x81))
    {
      int target_player = effect->targets[1].player;
      int target_card = effect->targets[1].card;
      if (effect->damage_target_player == target_player &&
          effect->damage_target_card == target_card)
      {
        spell_fizzled = 1;
        return 0;
      }
      card_instance_t *planeswalker = get_card_instance(target_player, target_card);
      int iid = get_internal_card_id_from_csv_id(CARD_ID_KASMINA_ENIGMA_SAGE);
      call_card_fn(cards_data[iid].code_pointer, planeswalker, target_player, target_card,
                   EVENT_ACTIVATE);
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    int iid = get_internal_card_id_from_csv_id(CARD_ID_KASMINA_ENIGMA_SAGE);
    CardFunction address = cards_data[iid].code_pointer;
    int target_player = effect->targets[1].player;
    int target_card = effect->targets[1].card;
    card_instance_t *planeswalker = get_card_instance(target_player, target_card);
    call_card_fn(address, planeswalker, target_player, target_card, EVENT_RESOLVE_ACTIVATION);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d1ab0; original cards/strixhaven.c:3456. */
int card_kasmina_enigma_sage(int player, int card, event_t event)
{
  /* Kasmina, Enigma Sage	|1|G|U
   * Legendary Planeswalker - Kasmina (2)
   * Each other planeswalker you control has the loyalty abilities of Kasmina.
   * +2: Scry 1.
   * -X: Create a 0/0 green and blue Fractal creature token. Put X +1/+1 counters on it.
   * -8: Search your library for an instant or sorcery card that shares a color with this planeswalker, exile that card, then shuffle. You may cast that card without paying its mana cost. */

  enum
  {
    CHOICE_SCRY = 1,
    CHOICE_FRACTAL = 2,
    CHOICE_EXILE_SPELL = 3
  };

  int c;
  card_instance_t *instance;
  int choice_or_count;
  int loyalty_or_iid;
  unsigned int clr;
  const char *color_text;
  target_definition_t td;
  char buffer[100];
  char buffer3[100];
  char buffer2[100];
  test_definition_t this_test;

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

          if ((((instance != NULL) &&
                (loyalty_or_iid = is_what(player, c, 0x80), loyalty_or_iid != 0)) &&
               (instance = get_card_instance(player, c),
                instance->info_slot_fn == kasmina_effect)) &&
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
        instance = get_card_instance(
            player, create_targetted_legacy_activate(player, card, kasmina_effect, player, card));
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
      }
    }
    check_legend_rule(player, card, EVENT_STATIC_EFFECTS);
  }
  else
  {
    check_legend_rule(player, card, event);
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      default_target_definition(player, card, &td, 2);
      choice_or_count = dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, DLG_MODE,
                                    "Scry 2", 1, 10, 2, "Create a Fractal", 1, 0x14, 0,
                                    "Exile a spell from your deck and you can cast it", 1, 0x32,
                                    0xfffffff8, 0, 0, 0, 0);
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (choice_or_count == 0)
        {
          return 0;
        }
      }
      else
      {
        instance->number_of_targets = '\0';
        if (choice_or_count == CHOICE_FRACTAL)
        {

          choice_or_count = count_counters(player, card, COUNTER_LOYALTY);
          if (choice_or_count < 1)
          {
            choice_or_count = 1;
          }
          if ((ai_is_speculating != 1) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
          {
            do
            {
              do
              {
                choice_or_count = choose_a_number(player, "How many Loyalty you\'ll remove?", 1);
              } while (choice_or_count < 0);
              loyalty_or_iid = count_counters(player, card, COUNTER_LOYALTY);
            } while (loyalty_or_iid < choice_or_count);
          }
          remove_counters(player, card, player, card, COUNTER_LOYALTY, choice_or_count);
          instance->targets[1].player = choice_or_count;
        }
      }
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {

      instance = get_card_instance(player, card);
      default_target_definition(player, card, &td, 2);
      choice_or_count = dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, DLG_MODE,
                                    "Scry 2", 1, 10, 2, "Create a Fractal", 1, 0x14, 0,
                                    "Exile a spell from your deck and you can cast it", 1, 0x32,
                                    0xfffffff8, 0, 0, 0, 0);
      if (choice_or_count == CHOICE_SCRY)
      {
        scry(player, 2);
      }
      else if (choice_or_count == CHOICE_FRACTAL)
      {
        make_one_fractal(player, card, 1, instance->targets[1].player);
      }
      else if (choice_or_count == CHOICE_EXILE_SPELL)
      {

        clr = get_color(instance->parent_controller, instance->parent_card);
        color_text = get_color_text(player, buffer3, clr);
        scnprintf(buffer, 100, "Select a %s instant or sorcery card.", color_text);
        new_default_test_definition(&this_test, 0x38, buffer);
        this_test.color = clr;
        choice_or_count = new_select_a_card(player, player, 1, 0, 2, -1, &this_test);
        if (-1 < choice_or_count)
        {

          loyalty_or_iid = deck_ptr[player][choice_or_count];
          rfg_card_in_deck(player, choice_or_count);
          if (can_legally_play_iid(player, loyalty_or_iid) != 0)
          {

            scnprintf(buffer2, 100, "Play %s", cards_ptr[cards_data[loyalty_or_iid].id]->name);
            choice_or_count = dialog_impl(player, card, event, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
                                          buffer2, 1, 0x14, "Decline", 1, 10, 0, 0, 0, 0);
            if (choice_or_count == 1)
            {
              play_card_in_exile_for_free(player, player,
                                          (unsigned int)cards_data[loyalty_or_iid].id);
            }
          }
        }
      }
    }
  }
  return planeswalker(player, card, event, 2);
}

/* ManalinkEh 2022: 0x23d20e0; original cards/strixhaven.c:3564. */
int card_kelpie_guide(int player, int card, event_t event)
{
  /* Kelpie Guide	|2|U
   * Creature - Beast 2/2
   * |T: Untap another target permanent you control.
   * |T: Tap target permanent. Activate only if you control eight or more lands. */

  int choice;
  card_instance_t *instance;
  int value;
  int lands_or_available;
  unsigned int can_tap;
  target_definition_t *tap_target;
  target_definition_t td;
  target_definition_t td2;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x47);
    td.preferred_controller = player;
    td.special = TARGET_SPECIAL_NOT_ME;
    default_target_definition(player, card, &td2, 0x47);
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                                    NULL) == 0)
      {
        value = count_permanents_by_type(player, TYPE_LAND);
        if (value < 8)
        {
          return 0;
        }
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0, 0, 0, 0,
                                      &td2, NULL) == 0)
        {
          return 0;
        }
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->info_slot = 0;
      instance->number_of_targets = '\0';
      value = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0, 0, 0, 0,
                                        &td, NULL);
      lands_or_available = count_subtype(player, 1, -1);
      can_tap = 0;
      if (7 < lands_or_available)
      {
        lands_or_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0,
                                                       0, 0, 0, 0, &td2, NULL);
        can_tap = (unsigned int)(lands_or_available != 0);
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_MODE, DLG_AUTOCHOOSE_IF_1,
                           "Untap another target permanent", value,
                           ((current_turn == player) ? 30U : 10U),
                           "Tap target permanent", can_tap, 0x14, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == 1)
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, 7, 0, 0, 0, 0, 0, 0, 0, &td,
                                  "Select another target permanent you control.");
        if (spell_fizzled == 1)
        {
          return 0;
        }
        ai_mod_for_untap_effects(player, card, 0, 0);
        return 0;
      }
      if (choice != 2)
      {
        return 0;
      }
      generic_activated_ability(player, card, EVENT_ACTIVATE, 5, 0, 0, 0, 0, 0, 0, 0, &td2,
                                "TARGET_PERMANENT");
      if (spell_fizzled == 1)
      {
        return 0;
      }

      lands_or_available = get_cmc(instance->targets[0].player, instance->targets[0].card);
      value = lands_or_available * 5;
      if (instance->targets[0].player != 1 - player)
      {
        value = lands_or_available * -5;
      }
      ai_modifier = ai_modifier + value;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0x47);
    td.preferred_controller = player;
    td.special = TARGET_SPECIAL_NOT_ME;
    default_target_definition(player, card, &td2, 0x47);
    instance = get_card_instance(player, card);
  }
  tap_target = &td2;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->mode == '\x01')
    {
      if (valid_target(&td) != 0)
      {
        untap_card(instance->targets[0].player, instance->targets[0].card);
      }
    }
    else if ((instance->mode == '\x02') && (value = valid_target(tap_target), value != 0))
    {
      tap_card(instance->targets[0].player, instance->targets[0].card);
      return 0;
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x23c86a0; original cards/strixhaven.c:3632. */
static int card_exiled_with_study_counter(int player, int card, event_t event)
{
  int target_player;
  card_instance_t *instance;
  int study_count;
  int iid_or_count;
  int card_00;
  int matches;

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
      scnprintf(set_legacy_effect_name_addr, 0x33, "Exiled with a study counter.");
      return 0;
    }
    if (event == EVENT_STATIC_EFFECTS)
    {

      instance = get_card_instance(player, card);
      iid_or_count = instance->targets[0].card;
      instance = get_card_instance(player, card);
      matches = 0;
      target_player = instance->targets[0].player;
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
              if (instance->info_slot_fn == card_exiled_with_study_counter)
              {
                study_count = count_counters(player, card_00, COUNTER_STUDY);
                if ((study_count != 0) && (iid_or_count == instance->targets[0].card))
                {
                  matches = matches + 1;
                }
              }
            }
          }
          card_00 = card_00 + 1;
        } while (card_00 < active_cards_count[player]);
      }
      iid_or_count = count_iid_in_rfg(target_player, iid_or_count);
      if (iid_or_count < matches)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23c7a80; original cards/strixhaven.c:3659. */
static void create_exile_with_study_counter_legacy(int player, int card, int iid, int owner)
{
  unsigned int csvid;
  int result;
  card_instance_t *instance;

  result = create_legacy_effect(player, card, card_exiled_with_study_counter);
  if (result != -1)
  {
    add_counters(player, card, player, result, COUNTER_STUDY, 1);
    instance = get_card_instance(player, result);
    csvid = cards_data[iid].id;
    instance->targets[0].player = owner;
    result = instance->damage_source_card;
    instance->display_pic_csv_id = (uint16_t)csvid;
    instance->targets[0].card = iid;
    instance->eot_toughness = 0x300;
    result = get_card_image_number(csvid & 0xffff, (int)instance->damage_source_player, result);
    instance->display_pic_num = (uint16_t)result;
  }
  return;
}

/* ManalinkEh 2022: 0x23c6a40; original cards/strixhaven.c:3672. */
static int count_different_mana_values_among_card_exiled_with_study_counter(int player)
{
  int manacosts[20] = {0};
  int mc = 0;
  for (int c = 0; c < active_cards_count[player]; ++c)
  {
    card_instance_t *inst = in_play(player, c);
    if (inst && is_what(player, c, 0x80))
    {
      inst = get_card_instance(player, c);
      if (inst->info_slot_fn == card_exiled_with_study_counter &&
          count_counters(player, c, COUNTER_STUDY))
      {
        int cmc = get_cmc_by_internal_id(inst->targets[0].card);
        if (!manacosts[cmc])
        {
          manacosts[cmc] = 1;
          ++mc;
        }
      }
    }
  }
  return mc;
}

/* ManalinkEh 2022: 0x23d26a0; original cards/strixhaven.c:3690. */
int card_kianne_dean_of_substance(int player, int card, event_t event)
{
  /* Kianne, Dean of Substance	|2|G
   * Legendary Creature - Elf Druid 2/2
   * |T: Exile the top card of your library. If it's a land card, put it into your hand. Otherwise, put a study counter on it.
   * |4|G: Create a 0/0 green and blue Fractal creature token. Put a +1/+1 counter on it for each different mana value among nonland cards you own in exile with study counters on them. */

  int choice;
  card_instance_t *instance;
  int count_or_available;
  int can_exile;

  check_legend_rule(player, card, event);
  mdfc_front(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      count_or_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0,
                                                     0, 0, 0, 0, NULL, NULL);
      if ((count_or_available == 0) &&
          (count_or_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 4, 0,
                                                          0, 1, 0, 0, 0, NULL, NULL),
           count_or_available == 0))
      {
        return 0;
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      count_or_available = count_different_mana_values_among_card_exiled_with_study_counter(player);
      instance->info_slot = 0;
      instance->number_of_targets = '\0';
      can_exile = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0, 0,
                                            0, NULL, NULL);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_MODE, DLG_AUTOCHOOSE_IF_1,
                           "Tap and exile top card", can_exile, 0x14, "Create a Fractal",
                           generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 4, 0, 0,
                                                     1, 0, 0, 0, NULL, NULL),
                           count_or_available * 10, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == 1)
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
        if (spell_fizzled == 1)
        {
          return 0;
        }
        ai_modifier = ai_modifier + 0x18;
        return 0;
      }
      if (choice != 2)
      {
        return 0;
      }
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 4, 0, 0, 1, 0, 0, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      ai_modifier = ai_modifier + count_or_available * 10;
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
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->mode == '\x01')
    {

      count_or_available = *deck_ptr[player];
      if (count_or_available != -1)
      {
        if (is_what(-1, count_or_available, 1) == 0)
        {
          create_exile_with_study_counter_legacy(player, card, count_or_available, player);
          rfg_top_card_of_deck(player);
        }
        else
        {
          put_into_play_a_card_from_deck(player, player, 0);
        }
      }
    }
    else if (instance->mode == '\x02')
    {
      make_one_fractal(player, card, 1,
          count_different_mana_values_among_card_exiled_with_study_counter(player));
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d2b40; original cards/strixhaven.c:3759. */
int card_imbraham_dean_of_theory(int player, int card, event_t event)
{
  /* Imbraham, Dean of Theory	|2|U|U
   * Legendary Creature - Bird Wizard 3/3
   * Flying
   * |X|U|U, |T: Exile the top X cards of your library and put a study counter on each of them. Then you may put a card you own in exile with a study counter on it into your hand. */

  card_instance_t *instance;
  int matches_or_index;
  int x;
  int card_or_selection;
  int stored_cards;
  test_definition_t test;
  int iids[151];

  mdfc_back(player, card, event);
  check_legend_rule(player, card, event);
  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      instance = get_card_instance(player, card);
      x = instance->info_slot;
      instance->info_slot = 0;
      while ((x != 0 && (*deck_ptr[player] != -1)))
      {

        create_exile_with_study_counter_legacy(player, card, *deck_ptr[player], player);
        rfg_top_card_of_deck(player);
        x = x + -1;
      }
      card_or_selection = 0;
      stored_cards = 0;
      if (0 < active_cards_count[player])
      {
        do
        {
          instance = in_play(player, card_or_selection);
          if ((((instance != NULL) && (matches_or_index = is_what(player, card_or_selection, 0x80),
                                       matches_or_index != 0)) &&
               (instance->info_slot_fn == card_exiled_with_study_counter)) &&
              (matches_or_index = count_counters(player, card_or_selection, COUNTER_STUDY),
               matches_or_index != 0))
          {
            iids[stored_cards] = instance->targets[0].card;
            stored_cards = stored_cards + 1;
          }
          card_or_selection = card_or_selection + 1;
        } while (card_or_selection < active_cards_count[player]);
      }
      new_default_test_definition(&test, 0xff, "Select a card to put into your hand.");
      card_or_selection =
          select_card_from_zone(player, player, iids, stored_cards, 0, 1, -1, &test);
      if (card_or_selection == -1)
      {
        return 0;
      }
      card_or_selection = iids[card_or_selection];
      matches_or_index = find_iid_in_rfg(player, card_or_selection);
      if (-1 < matches_or_index)
      {
        add_card_to_hand(player, card_or_selection);
        remove_card_from_rfg_by_position(player, matches_or_index);
      }
    }
  }
  card_or_selection =
      generic_activated_ability(player, card, event, 1, -1, 0, 2, 0, 0, 0, 0, NULL, NULL);
  return card_or_selection;
}

/* ManalinkEh 2022: 0x23d2da0; original cards/strixhaven.c:3830. */
int card_leonin_lightscribe(int player, int card, event_t event)
{
  /* Leonin Lightscribe	|1|W
   * Creature - Cat Cleric 2/2
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, creatures you control get +1/+1 until end of turn. */

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  pump_creatures_until_eot_merge_pt(player, card, player, 1, 1, NULL);
  return 0;
}

/* ManalinkEh 2022: 0x23d2e30; original cards/strixhaven.c:3845. */
int card_letter_of_acceptance(int player, int card, event_t event)
{
  /* Letter of Acceptance	|3
   * Artifact
   * |T: Add one mana of any color.
   * |2, |T, Sacrifice this artifact: Draw a card. */

  card_instance_t *instance;
  int result;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = mana_producer(player, card, EVENT_CAN_ACTIVATE);
      if ((result == 0) && (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9,
                                                               3, 0, 0, 0, 0, 0, 0, NULL, NULL),
                            result == 0))
      {
        return 0;
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->mode = '\0';
      result = paying_mana();
      if ((result == 0) && (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9,
                                                               3, 0, 0, 0, 0, 0, 0, NULL, NULL),
                            result != 0))
      {

        result = count_permanents_by_type(player, TYPE_LAND);
        result = do_dialog(player, player, card, -1, -1, " Produce mana\n Sac and draw 1\n Cancel",
                           (unsigned int)(6 < result));
        if (result != 0)
        {
          if (result == 1)
          {
            add_state(player, card, 0x10);
            generic_activated_ability(player, card, EVENT_ACTIVATE, 8, 2, 0, 0, 0, 0, 0, 0, NULL,
                                      NULL);
            if (spell_fizzled != 1)
            {
              result = hand_count[player];
              instance->mode = '\x01';
              ai_modifier = ai_modifier + result * -10 + 0x46;
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
      }
      event = EVENT_ACTIVATE;
      return mana_producer(player, card, event);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return mana_producer(player, card, event);
    }
    instance = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (instance->mode == '\x01'))
  {
    draw_cards(player, 1);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d3120; original cards/strixhaven.c:3899. */
int card_leyline_invocation(int player, int card, event_t event)
{
  /* Leyline Invocation	|5|G
   * Sorcery
   * Create a 0/0 green and blue Fractal creature token. Put X +1/+1 counters on it, where X is the number of lands you control. */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    result = count_subtype(player, 1, -1);
    make_one_fractal(player, card, 1, result);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23d3190; original cards/strixhaven.c:3912. */
int card_lorehold_apprentice(int player, int card, event_t event)
{
  /* Lorehold Apprentice	|R|W
   * Creature - Human Cleric 2/2
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, until end of turn, Spirit creatures you control gain "|T: This creature deals 1 damage to each opponent." */

  card_instance_t *instance;
  int result;

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {

    if (0 < active_cards_count[player])
    {
      result = 0;
      do
      {
        if ((((instance = in_play(player, result),
                  instance == NULL || (instance->internal_card_id != LEGACY_EFFECT_ACTIVATED)) ||
                 (instance->info_slot_fn != effect_flame_fusillade)) ||
                (instance->targets[2].player != 2)))
        {
          result = result + 1;
          continue;
        }
        if (instance->targets[2].card == 0xbc)
        {
          return 0;
        }
        result = result + 1;
      } while (result < active_cards_count[player]);
    }
    result = create_legacy_activate(player, card, effect_flame_fusillade);
    if (-1 < result)
    {

      instance = get_card_instance(player, result);
      instance->targets[2].player = 2;
      instance->targets[2].card = 0xbc;
      instance->eot_toughness = 0x300;
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d3290; original cards/strixhaven.c:3943. */
int card_lorehold_campus(int player, int card, event_t event)
{
  /* Lorehold Campus	""
   * Land
   * This land enters tapped.
   * |T: Add |R or |W.
   * |4, |T: Scry 1. (Look at the top card of your library. You may put that card on the bottom.) */

  int choice;
  card_instance_t *instance;
  int mana_or_available;
  int priority;
  unsigned int argument7;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      mana_or_available = mana_producer(player, card, EVENT_CAN_ACTIVATE);
      if ((mana_or_available == 0) &&
          (mana_or_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 5, 0,
                                                         0, 0, 0, 0, 0, NULL, NULL),
           mana_or_available == 0))
      {
        return 0;
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->mode = '\0';
      mana_or_available = paying_mana();
      if ((mana_or_available == 0) &&
          (mana_or_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 5, 0,
                                                         0, 0, 0, 0, 0, NULL, NULL),
           mana_or_available != 0))
      {

        argument7 = 0;
        if (hand_count[player] < 3)
        {
          argument7 = (unsigned int)(current_turn != player);
        }
        choice =
            do_dialog(player, player, card, -1, -1, " Produce mana\n Scry 1\n Cancel", argument7);
        if (choice != 0)
        {
          if (choice == 1)
          {
            add_state(player, card, 0x10);
            generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 4, 0, 0, 0, 0, 0, 0, NULL,
                                      NULL);
            if (spell_fizzled != 1)
            {
              priority = -0x30;
              mana_or_available = hand_count[player];
              instance->mode = '\x01';
              if (mana_or_available < 3)
              {
                priority = (unsigned int)((current_turn != player) ? 48 : -48);
              }
              ai_modifier = ai_modifier + priority;
              return 0;
            }
            remove_state(player, card, 0x10);
            return 0;
          }
          if (choice != 2)
          {
            return 0;
          }
          spell_fizzled = 1;
          return 0;
        }
      }
      return mana_producer(player, card, EVENT_ACTIVATE);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return mana_producer_tapped(player, card, event);
    }
    instance = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (instance->mode == '\x01'))
  {
    scry(player, 1);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d35a0; original cards/strixhaven.c:3998. */
int card_lorehold_command(int player, int card, event_t event)
{
  /* Lorehold Command	|3|R|W
   * Instant
   * Choose two -
   * * Create a 3/2 red and white Spirit creature token.
   * * Creatures you control get +1/+0 and gain indestructible and haste until end of turn.
   * * ~ deals 3 damage to any target. Target player gains 3 life.
   * * Sacrifice a permanent, then draw two cards. */

  typedef enum
  {
    CHOICE_INVALID_TARGET = 0,
    CHOICE_SPIRIT = 1,
    CHOICE_P1_AND_INDESTRUCTIBLE = 2,
    CHOICE_DAMAGE_AND_GAIN = 3,
    CHOICE_SAC_AND_DRAW = 4
  } CommandChoices;

  CommandChoices choice;
  int result;
  unsigned int damage_available;
  uint32_t damage_priority;
  int draw_priority;
  int spirit_priority;
  card_instance_t *instance;
  target_definition_t *player_target;
  int pump_priority;
  target_definition_t *damage_target;
  CommandChoices choices[2];
  target_definition_t td;
  target_definition_t td2;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    default_target_definition(player, card, &td2, 0);
    td2.zone = TARGET_ZONE_PLAYERS;
    td2.preferred_controller = player;
    new_default_test_definition(&test, 0x47, "Select a permanent to sacrifice.");
    get_card_instance(player, card);
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return basic_spell(player, card, event);
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return basic_spell(player, card, event);
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  default_target_definition(player, card, &td2, 0);
  td2.zone = TARGET_ZONE_PLAYERS;
  td2.preferred_controller = player;
  new_default_test_definition(&test, 0x47, "Select a permanent to sacrifice.");
  instance = get_card_instance(player, card);

  if (event == EVENT_RESOLVE_SPELL)
  {
    choices[0] = dialog_impl(player, card, event, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS, DLG_RANDOM,
                             "Create a 3/2 Spirit", 0, 1, "Pump your creatures", 0, 1,
                             "Damage & gain life", 0, 1, "Sac & draw", 0, 1, 0, 0, 0, 0);
    for (int choice_index = 0; choice_index < 2; ++choice_index)
      switch (choices[choice_index])
      {
      case CHOICE_SPIRIT:
        make_one_spirit_strixhaven(player, card, 1);
        break;
      case CHOICE_P1_AND_INDESTRUCTIBLE:
        pump_creatures_until_eot(player, card, player, 0, 1, 0, 0, 0x800, NULL);
        break;
      case CHOICE_DAMAGE_AND_GAIN:
        if (valid_target(&td))
        {
          damage_target0(player, card, 3);
          if (validate_target(player, card, &td2, 1))
            gain_life(instance->targets[1].player, 3);
        }
        break;
      case CHOICE_SAC_AND_DRAW:
        player_sacrifices_a_permanent(player, card, player, 0x47, SAC_CAUSED | SAC_NO_CANCEL);
        draw_cards(player, 2);
        break;
      default:
        break;
      }
    kill_card(player, card, KILL_DESTROY);
    return basic_spell(player, card, event);
  }
  draw_priority = 1;
  damage_priority = 1;
  spirit_priority = 1;
  pump_priority = 1;
  if (ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0))
  {
    result = count_subtype(1 - player, 2, -1);
    pump_priority = result * 8;
    spirit_priority = result * -8 + 0x20;
    if (current_turn == player && current_phase != 0x18)
      pump_priority = 5;
    damage_priority = 0x14;
    draw_priority = hand_count[player] * -10 + 0x46;
  }
  damage_target = &td;
  player_target = &td2;
  result = generic_spell(player, card, EVENT_CAN_CAST, 1, damage_target, NULL, 1, &test);
  damage_available = 0;
  if (result != 0)
  {
    result = can_target(player_target);
    damage_available = (unsigned int)(result != 0);
  }
  choice = dialog_impl(player, card, event, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS, DLG_RANDOM,
                       "Create a 3/2 Spirit", 1, spirit_priority, "Pump your creatures", 1,
                       pump_priority, "Damage & gain life", damage_available, damage_priority,
                       "Sac & draw", 1, draw_priority, 0, 0, 0, 0);
  choices[0] = choice;

  if (event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller)
  {
    instance->number_of_targets = 0;
    for (int choice_index = 0; choice_index < 2; ++choice_index)
    {
      CommandChoices selected = choices[choice_index];
      if (selected == CHOICE_INVALID_TARGET)
        spell_fizzled = 1;
      else if (selected == CHOICE_DAMAGE_AND_GAIN)
      {
        generic_spell(player, card, EVENT_CAST_SPELL, 0x81, damage_target,
                      "Select target to damage.", 1, NULL);
        if (spell_fizzled != 1)
          new_pick_target(player_target, "Select target player to gain life.", 1, 0x81);
      }
    }
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23d3d20; original cards/strixhaven.c:4118. */
int card_lorehold_excavation(int player, int card, event_t event)
{
  /* Lorehold Excavation	|R|W
   * Enchantment
   * At the beginning of your end step, mill a card. If a land card was milled this way, you gain 1 life. Otherwise, this enchantment deals 1 damage to each opponent. (To mill a card, put the top card of your library into your graveyard.)
   * |5, Exile a creature card from your graveyard: Create a tapped 3/2 red and white Spirit creature token. */

  int result;
  card_instance_t *instance;
  token_generation_t token;
  test_definition_t test;

  result = eot_trigger(player, card, event);
  if (((result != 0) && (current_turn == player)) &&
      (result = is_humiliated(player, card, AL_TRIG), result == 0))
  {
    special_mill(player, card, 0x58bb, player, 1);
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    new_default_test_definition(&test, 2, "Select a creature card to exile from your graveyard.");
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 5, 0, 0, 0, 0, 0, 0,
                                         NULL, NULL);
      if (result == 0)
      {
        return 0;
      }
      result = new_special_count_grave(player, &test);
      return (unsigned int)(result != 0);
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 5, 0, 0, 0, 0, 0, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      result = new_global_tutor(player, player, 2, 6, 0, 3, &test);
      if (-1 < result)
      {
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
      result = global_enchantment(player, card, event);
      return result;
    }
    new_default_test_definition(&test, 2, "Select a creature card to exile from your graveyard.");
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_SPIRIT, &token);
  token.pow = 3;
  token.tou = 2;
  token.color_forced = '0';
  token.action = TOKEN_ACTION_TAPPED;
  generate_token(&token);
  return 0;
}

/* ManalinkEh 2022: 0x23d4000; original cards/strixhaven.c:4191. */
int card_mage_hunter(int player, int card, event_t event)
{
  /* Mage Hunter	|3|B
   * Creature - Horror 3/4
   * Whenever an opponent casts or copies an instant or sorcery spell, they lose 1 life. */

  magecraft_trigger(1 - player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  lose_life(1 - player, 1);
  return 0;
}

/* ManalinkEh 2022: 0x23d4080; original cards/strixhaven.c:4222. */
int card_manifestation_sage(int player, int card, event_t event)
{
  /* Manifestation Sage	|GU|GU|GU|GU
   * Creature - Human Wizard 2/2
   * When this creature enters, create a 0/0 green and blue Fractal creature token. Put X +1/+1 counters on it, where X is the number of cards in your hand. */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    make_one_fractal(player, card, 1, hand_count[player]);
  }
  result = hybrid(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23d40e0; original cards/strixhaven.c:4236. */
int card_mascot_exhibition(int player, int card, event_t event)
{
  /* Mascot Exhibition	|7
   * Sorcery - Lesson
   * Create a 2/1 white and black Inkling creature token with flying, a 3/2 red and white Spirit creature token, and a 4/4 blue and red Elemental creature token. */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    make_inkling(player, card, 1, 0);
    make_one_spirit_strixhaven(player, card, 1);
    make_one_elemental_strixhaven(player, card, 1);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23d4150; original cards/strixhaven.c:4258. */
int card_master_symmetrist(int player, int card, event_t event)
{
  /* Master Symmetrist	|2|G|G
   * Creature - Rhino Druid 4/4
   * Reach
   * Whenever a creature you control with power equal to its toughness attacks, it gains trample until end of turn. */

  store_attackers(player, card, event, DAT_TRACK, player, -1, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    const uint8_t *attackers = (const uint8_t *)&instance->targets + 2 * sizeof(target_t);
    for (int index = instance->info_slot - 1; index >= 0; --index)
      if (in_play(current_turn, attackers[index]))
      {
        int power = get_power(current_turn, attackers[index]);
        int toughness = get_toughness(current_turn, attackers[index]);
        if (power == toughness)
          pump_ability_until_eot_no_repeat(player, card, current_turn, attackers[index],
                                            KEYWORD_TRAMPLE, 0);
      }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d4290; original cards/strixhaven.c:4290. */
int card_mercurial_transformation(int player, int card, event_t event)
{
  /* Mercurial Transformation	|1|U
   * Sorcery - Lesson
   * Until end of turn, target nonland permanent loses all abilities and becomes your choice of a blue Frog creature with base power and toughness 1/1 or a blue Octopus creature with base power and toughness 4/4. */

  uint8_t selected_mode;
  int value;
  int target_or_power;
  card_instance_t *instance;
  int32_t base_identity;
  target_definition_t td;
  test_definition_t test;

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card != affected_card) || (player != affected_card_controller))
      {
        return 0;
      }
      default_target_definition(player, card, &td, 0x47);
      td.illegal_type = TYPE_LAND;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_NONLAND_PERMANENT", 1, NULL);
        if (spell_fizzled == 1)
        {
          return 0;
        }
        dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_MODE, "Frog", 1, 1, "Octopus",
                    1, 1, 0, 0, 0, 0);
        if (spell_fizzled == 1)
        {
          return 0;
        }
        if (ai_is_speculating != 1)
        {
          if (player != 1)
          {
            return 0;
          }
          if (((unsigned char)trace_mode & 2) != 0)
          {
            return 0;
          }
        }

        value = instance->targets[0].player;
        target_or_power = instance->targets[0].card;
        if (value == 1 - player)
        {
          value = get_power(value, target_or_power);
          if (is_what(instance->targets[0].player, instance->targets[0].card, 2) == 0)
          {
            value = get_cmc(instance->targets[0].player, instance->targets[0].card);
          }
          ai_modifier = ai_modifier + value * 8;
          return 0;
        }
        value = get_power(value, target_or_power);
        if (is_what(instance->targets[0].player, instance->targets[0].card, 2) != 0)
        {
          if (3 < value)
          {
            ai_modifier = ai_modifier + value * -8;
            return 0;
          }
          ai_modifier = ai_modifier + (4 - value) * 8;
          return 0;
        }
        value = get_cmc(instance->targets[0].player, instance->targets[0].card);
        ai_modifier = ai_modifier + (4 - value) * 8;
        return 0;
      }
      return generic_spell(player, card, event, 1, &td, NULL, 1, NULL);
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 0x47);
      td.illegal_type = TYPE_LAND;
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_SPELL)
      {
        if (valid_target(&td) != 0)
        {

          selected_mode = instance->mode;
          if (is_what(instance->targets[0].player, instance->targets[0].card, 2) == 0)
          {

            value = instance->targets[0].card;
            target_or_power = instance->targets[0].player;
            if (selected_mode == '\x01')
            {
              instance = get_card_instance(
                  player, turn_into_creature(player, card, target_or_power, value, 1, 1, 1));
              value = get_sleighted_color_test(player, card, 4);
              instance->targets[6].player = value;
              instance->targets[9].player = 1;
              base_identity = 0x44;
            }
            else
            {
              instance = get_card_instance(
                  player, turn_into_creature(player, card, target_or_power, value, 1, 4, 4));
              value = get_sleighted_color_test(player, card, 4);
              instance->targets[6].player = value;
              instance->targets[9].player = 1;
              base_identity = 0x86;
            }
            instance->targets[9].card = base_identity;
          }
          else
          {
            default_test_definition(&test, 0);
            if (selected_mode == '\x01')
            {
              test.toughness = 1;
              test.power = 1;
              test.subtype = 0x44;
            }
            else
            {
              test.toughness = 4;
              test.power = 4;
              test.subtype = 0x86;
            }
            test.color = get_sleighted_color_test(player, card, 4);
            humiliate_and_set_pt_abilities(player, card, instance->targets[0].player,
                                           instance->targets[0].card, 4, &test);
          }
        }
        kill_card(player, card, KILL_DESTROY);
      }
      return generic_spell(player, card, event, 1, &td, NULL, 1, NULL);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  default_target_definition(player, card, &td, 0x47);
  td.illegal_type = TYPE_LAND;
  get_card_instance(player, card);
  return generic_spell(player, card, event, 1, &td, NULL, 1, NULL);
}

/* ManalinkEh 2022: 0x23d4840; original cards/strixhaven.c:4367. */
int card_mila_crafty_companion(int player, int card, event_t event)
{
  /* Mila, Crafty Companion	|1|W|W
   * Legendary Creature - Fox 2/3
   * Whenever an opponent attacks one or more planeswalkers you control, put a loyalty counter on each planeswalker you control.
   * Whenever a permanent you control becomes the target of a spell or ability an opponent controls, you may draw a card. */

  check_legend_rule(player, card, event);
  mdfc_front(player, card, event);
  store_attackers(player, card, event, DAT_TRACK, 1 - player, -1, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
  {
    card_instance_t *record = get_card_instance(player, card);
    const uint8_t *attackers = (const uint8_t *)&record->targets + 2 * sizeof(target_t);
    for (int index = record->info_slot - 1; index >= 0; --index)
      if (in_play(current_turn, attackers[index]) && planeswalker_its_attacking(current_turn, attackers[index]) >= 0)
      {
        test_definition_t test;
        default_test_definition(&test, TARGET_TYPE_PLANESWALKER);
        new_manipulate_all(player, card, player, &test, 0x1420001);
        break;
      }
  }
  const target_t *spell_target = any_becomes_target(player, card, event, player, -1,
      TYPE_PERMANENT, -1, TYPE_ANY, 1 - player, (duh_mode(player) != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (spell_target && in_play(player, card) && !is_humiliated(player, card, AL_TRIG))
    draw_cards(player, 1);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23c7b30; original cards/strixhaven.c:4404. */
int lukka_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0)
    haste(instance->damage_target_player, instance->damage_target_card, event);
  if (instance->damage_target_player == current_turn && upkeep_trigger(player, card, event))
  {
    if (!check_state(instance->damage_target_player, instance->damage_target_card, STATE_OUBLIETTED))
      kill_card(instance->damage_target_player, instance->damage_target_card, KILL_EXILE);
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23d4a50; original cards/strixhaven.c:4419. */
int card_lukka_wayward_bonder(int player, int card, event_t event)
{
  /* Lukka, Wayward Bonder	|4|R|R
   * Legendary Planeswalker - Lukka (5)
   * +1: You may discard a card. If you do, draw a card. If a creature card was discarded this way, draw two cards instead.
   * -2: Return target creature card from your graveyard to the battlefield. It gains haste. Exile it at the beginning of your next upkeep.
   * -7: You get an emblem with "Whenever a creature you control enters, it deals damage equal to its power to any target." */

  enum
  {
    CHOICE_DISCARD_DRAW = 1,
    CHOICE_REANIMATE = 2,
    CHOICE_EMBLEM = 3
  };

  int choice_or_card;
  int creature_or_reanimated;
  card_instance_t *instance;
  test_definition_t this_test;
  test_definition_t this_test2;

  check_legend_rule(player, card, event);
  mdfc_back(player, card, event);

  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return planeswalker(player, card, event, 5);
  instance = get_card_instance(player, card);
  new_default_test_definition(&this_test, 2, "Select target creature card.");
  int can_reanimate = 0;
  if (graveyard_has_shroud(player) == 0)
    can_reanimate = new_special_count_grave(player, &this_test) != 0;
  choice_or_card = dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, "Discard & draw", 1, 10,
                               1, "Reanimate target creature", can_reanimate, 0x14, 0xfffffffe,
                               "Emblem", 1, 5, 0xfffffff9, 0, 0, 0, 0);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (choice_or_card == 0)
      return 0;
  }
  else if (event == EVENT_ACTIVATE)
  {
    instance->number_of_targets = 0;
    if (choice_or_card == CHOICE_REANIMATE && select_multiple_cards_from_graveyard(
                                   player, player, 0, 2, &this_test, 1, instance->targets) == -1)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  else
  {
    if (choice_or_card == CHOICE_DISCARD_DRAW)
    {

      new_default_test_definition(&this_test2, 0xff, "Select a card to discard.");
      choice_or_card = new_select_a_card(player, player, 4, 0, 3, -1, &this_test2);
      if (choice_or_card == -1)
      {
        return 0;
      }
      creature_or_reanimated = is_what(player, choice_or_card, 2);
      new_discard_card(player, choice_or_card, 2, player);
      draw_cards(player, creature_or_reanimated + 1);
    }
    else if (choice_or_card == CHOICE_REANIMATE)
    {

      choice_or_card = validate_target_from_grave_source(player, card, player, 0);

      if ((choice_or_card != -1) && (choice_or_card = reanimate_permanent(
                                         player, card, player, choice_or_card, REANIMATE_DEFAULT),
                                     -1 < choice_or_card))
      {
        create_targetted_legacy_effect(player, card, lukka_legacy, player, choice_or_card);
      }
    }
    else if (choice_or_card == CHOICE_EMBLEM)
    {
      generate_token_by_id(player, card, CARD_ID_LUKKA_WAYWARD_BONDER_EMBLEM);
    }
  }
  return planeswalker(player, card, event, 5);
}
#endif

/* ManalinkEh 2022: 0x23d4f50; original cards/strixhaven.c:4497. */
int card_mortality_spear(int player, int card, event_t event)
{
  /* Mortality Spear	|2|B|G
   * Instant
   * This spell costs |2 less to cast if you gained life this turn.
   * Destroy target nonland permanent. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_MODIFY_COST)
  {
    result = get_trap_condition(player, 1);
    if (result != 0)
    {
      COST_COLORLESS = COST_COLORLESS + -2;
    }
    return 0;
  }
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.illegal_type = TYPE_LAND;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_NONLAND_PERMANENT", 1, NULL);
}

/* ManalinkEh 2022: 0x23d50f0; original cards/strixhaven.c:4529. */
int card_multiple_choice(int player, int card, event_t event)
{
  /* Multiple Choice	|X|U
   * Sorcery
   * If X is 1, scry 1, then draw a card.
   * If X is 2, you may choose a player. They return a creature they control to its owner's hand.
   * If X is 3, create a 4/4 blue and red Elemental creature token.
   * If X is 4 or more, do all of the above. */

  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    unsigned int mode = instance->info_slot;
    bool scry_and_draw = mode == 4 || (mode != 3 && (mode & 1));
    unsigned int bounce = mode == 4 ? 2 : mode & 2;
    unsigned int elemental = mode == 4 ? 4 : mode & 4;
    if (mode != 3)
    {
      if (scry_and_draw)
      {
        scry(player, 1);
        draw_cards(player, 1);
      }
      if (bounce)
      {
        target_definition_t td;
        default_target_definition(player, card, &td, 0);
        td.zone = TARGET_ZONE_PLAYERS;
        td.illegal_abilities = 0;
        if (new_pick_target(&td, "Select a player", 0, 0x80))
        {
          target_definition_t creature;
          default_target_definition(player, card, &creature, 2);
          creature.allow_cancel = 0;
          creature.illegal_abilities = 0;
          creature.who_chooses = instance->targets[0].player;
          creature.allowed_controller = creature.who_chooses;
          if (can_target(&creature) &&
              new_pick_target(&creature, "Select a creature you control.", 1, 0x80))
            action_on_target(player, card, 1, ACT_BOUNCE);
        }
      }
    }
    if (mode == 3 || elemental)
      make_one_elemental_strixhaven(player, card, 1);
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 4, NULL, NULL, 1, NULL);
}

/* ManalinkEh 2022: 0x23d53c0; original cards/strixhaven.c:4580. */
int card_necroblossom_snarl(int player, int card, event_t event)
{
  /* Necroblossom Snarl	""
   * Land
   * As this land enters, you may reveal a Swamp or Forest card from your hand. If you don't, this land enters tapped.
   * |T: Add |B or |G. */

  int result;

  soi_need_subtype_land(player, card, event, SUBTYPE_SWAMP, SUBTYPE_FOREST);
  result = plain_mana_producer(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23d5410; original cards/strixhaven.c:4590. */
int card_necrotic_fumes(int player, int card, event_t event)
{
  /* Necrotic Fumes	|1|B|B
   * Sorcery - Lesson
   * As an additional cost to cast this spell, exile a creature you control.
   * Exile target creature or planeswalker. */

  int target_card;
  card_instance_t *instance;
  int result;
  target_definition_t td;
  target_definition_t td2;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x102);
    default_target_definition(player, card, &td2, 2);
    td2.allowed_controller = player;
    td2.illegal_abilities = 0;
    get_card_instance(player, card);
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    if (result != 0)
    {
      result = can_target(&td2);
      return result;
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x102);
      default_target_definition(player, card, &td2, 2);
      td2.allowed_controller = player;
      td2.illegal_abilities = 0;
      instance = get_card_instance(player, card);

      if ((card == affected_card) &&
          ((player == affected_card_controller &&
            (new_pick_target(&td2, "Select a creature you control", 0, 0x81), spell_fizzled != 1))))
      {
        result = instance->targets[0].player;
        target_card = instance->targets[0].card;
        instance->number_of_targets = '\0';
        generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE_OR_PLANESWALKER", 1,
                      NULL);
        if (spell_fizzled != 1)
        {
          kill_card(result, target_card, KILL_EXILE);
        }
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x102);
    default_target_definition(player, card, &td2, 2);
    td2.allowed_controller = player;
    td2.illegal_abilities = 0;
    get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        kill_target0(player, card, KILL_EXILE);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d56f0; original cards/strixhaven.c:4653. */
int card_oriq_loremage(int player, int card, event_t event)
{
  /* Oriq Loremage	|2|B|B
   * Creature - Human Warlock 3/3
   * |T: Search your library for a card, put it into your graveyard, then shuffle. If it's an instant or sorcery card, put a +1/+1 counter on this creature. */

  card_instance_t *instance;
  int result;
  test_definition_t this_test;

  if (!IS_GAA_EVENT(event))
    return 0;
  instance = get_card_instance(player, card);
  if (event == EVENT_ACTIVATE)
  {
    ai_modifier = ai_modifier + 0x30;
  }

  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    new_default_test_definition(&this_test, 0xff, "Select a card to put into your graveyard.");
    result = new_global_tutor(player, player, 1, 3, 0, 6, &this_test);
    if ((-1 < result) && (result = is_what(player, result, 0x38), result != 0))
    {
      add_counters(instance->parent_controller, instance->parent_card,
                               instance->parent_controller, instance->parent_card, COUNTER_P1_P1,
                               1);
    }
  }
  return generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x23d58a0; original cards/strixhaven.c:4678. */
int card_overgrown_arch(int player, int card, event_t event)
{
  /* Overgrown Arch	|1|G
   * Creature - Plant Wall 0/4
   * Defender
   * |T: You gain 1 life.
   * |2, Sacrifice this creature: Learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  int choice;
  card_instance_t *instance;
  int mode_or_available;
  int can_learn;
  int learn_priority;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      mode_or_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0,
                                                    0, 0, 0, NULL, NULL);
      if ((mode_or_available == 0) &&
          (mode_or_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 2, 0,
                                                         0, 0, 0, 0, 0, NULL, NULL),
           mode_or_available == 0))
      {
        return 0;
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      mode_or_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0,
                                                    0, 0, 0, NULL, NULL);
      can_learn = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 2, 0, 0, 0, 0, 0,
                                            0, NULL, NULL);
      learn_priority = 5;
      if (5 < life[player])
      {
        learn_priority = (6 - hand_count[player]) * 8;
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_MODE, DLG_AUTOCHOOSE_IF_1,
                           "Gain 1 life", mode_or_available, 0x1e, "Learn", can_learn,
                           learn_priority, 0, 0, 0, 0);
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
        generic_activated_ability(player, card, EVENT_ACTIVATE, 8, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
        if (spell_fizzled == 1)
        {
          return 0;
        }
        mode_or_available = -0x32;
        if (5 < life[player])
        {
          mode_or_available = (6 - hand_count[player]) * 8;
        }
        ai_modifier = ai_modifier + mode_or_available;
        return 0;
      }
      generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      if (1 - player != current_turn)
      {
        return 0;
      }
      if (current_phase < 0x18)
      {
        return 0;
      }
      ai_modifier = ai_modifier + 0x30;
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
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    if (instance->mode == '\x01')
    {
      gain_life(player, 1);
    }
    else if (instance->mode == '\x02')
    {
      learn(player, card);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d5d00; original cards/strixhaven.c:4736. */
int card_owlin_shieldmage(int player, int card, event_t event)
{
  /* Owlin Shieldmage	|3|W|B
   * Creature - Bird Warlock 3/3
   * Flying
   * Ward-Pay 3 life. (Whenever this creature becomes the target of a spell or ability an opponent controls, counter it unless that player pays 3 life.) */

  ward_life(player, card, event, 3);
  return 0;
}

/* ManalinkEh 2022: 0x23d5d30; original cards/strixhaven.c:4746. */
int card_pest_summoning(int player, int card, event_t event)
{
  /* Pest Summoning	|1|BG|BG
   * Sorcery - Lesson
   * Create two 1/1 black and green Pest creature tokens with "When this token dies, you gain 1 life." */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    make_pest(player, card, 2);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23d5d90; original cards/strixhaven.c:4759. */
int card_pestilent_cauldron(int player, int card, event_t event)
{
  /* Pestilent Cauldron	|2|B
   * Artifact
   * |T, Discard a card: Create a 1/1 black and green Pest creature token with "When this token dies, you gain 1 life."
   * |1, |T: Each opponent mills cards equal to the amount of life you gained this turn.
   * |4, |T: Exile four target cards from a single graveyard. Draw a card. */

  int p;
  int i;
  int choice;
  uint8_t selected_mode;
  unsigned char available_modes;
  card_instance_t *instance;
  int grave_count_or_shroud;
  int value;
  int life_gained;
  int mode_or_available;
  int exiled_count;
  test_definition_t test;

  mdfc_front(player, card, event);
  if (((event == EVENT_ACTIVATE) || ((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2)) ||
      (event == EVENT_CLEANUP))
  {
    instance = get_card_instance(player, card);
    grave_count_or_shroud = graveyard_has_shroud(player);
    available_modes = false;
    if (grave_count_or_shroud == 0)
    {
      grave_count_or_shroud = count_graveyard(player);
      available_modes = 3 < grave_count_or_shroud;
    }
    p = 1 - player;
    value = graveyard_has_shroud(p);
    if ((value == 0) && (value = count_graveyard(p), 3 < value))
    {
      available_modes = available_modes | 2;
    }
    new_default_test_definition(&test, 0xff, "Select target card to exile.");
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x21, 0, 0, 0, 0, 0, 0, 0,
                                    NULL, NULL) != 0)
      {
        return 1;
      }
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 1, 0, 0, 0, 0, 0, 0, NULL,
                                    NULL) != 0)
      {
        return 1;
      }
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 4, 0, 0, 0, 0, 0, 0, NULL,
                                    NULL) != 0)
      {
        return (unsigned int)(available_modes != 0);
      }
    }
    else if (event == EVENT_ACTIVATE)
    {

      value = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x21, 0, 0, 0, 0, 0, 0, 0,
                                        NULL, NULL);
      mode_or_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 1, 0, 0, 0,
                                                    0, 0, 0, NULL, NULL);
      generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 4, 0, 0, 0, 0, 0, 0, NULL,
                                NULL);
      life_gained = get_trap_condition(player, 1);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_OMIT_ILLEGAL, DLG_AUTOCHOOSE_IF_1,
                           DLG_MODE, "Create a Pest", value, 0x14, "Mill", mode_or_available,
                           life_gained << 3, "Exile and draw", mode_or_available,
                           (((available_modes & 2) == 0) ? 5U : 30U), 0, 0,
                           0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == 1)
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, 0x21, 0, 0, 0, 0, 0, 0, 0, NULL,
                                  NULL);
        if (spell_fizzled != 1)
        {
          grave_count_or_shroud = count_subtype(p, 2, -1);
          ai_modifier = ai_modifier + grave_count_or_shroud * 8;
        }
      }
      else if (choice == 2)
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
        if (spell_fizzled != 1)
        {
          grave_count_or_shroud = get_trap_condition(player, 1);
          ai_modifier = ai_modifier + grave_count_or_shroud * 8;
        }
      }
      else if (((choice == 3) &&
                (select_multiple_cards_from_either_graveyard(player, card, SFG_CANNOT_CANCEL, 3, 1,
                                                             NULL, 4, instance->targets + 1),
                 spell_fizzled != 1)) &&
               (generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 4, 0, 0, 0, 0, 0, 0,
                                          NULL, NULL),
                spell_fizzled != 1))
      {
        ai_modifier = ai_modifier + 0x18;
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {

      selected_mode = instance->mode;
      if (selected_mode == '\x01')
      {
        make_pest(player, card, 1);
      }
      else
      {
        if (selected_mode == '\x02')
        {
          mill(p, get_trap_condition(player, 1));
          return 0;
        }
        if (selected_mode == '\x03')
        {

          exiled_count = 0;
          i = 1;
          do
          {

            mode_or_available = instance->targets[i].player >> 0x10;
            value = validate_target_from_grave_source(player, card, mode_or_available, i);
            if (value != -1)
            {
              rfg_card_from_grave(mode_or_available, value);
              exiled_count = exiled_count + 1;
            }
            i = i + 1;
          } while (i != 5);
          if (exiled_count != 0)
          {
            draw_cards(player, 1);
            return 0;
          }
        }
      }
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x23d6550; original cards/strixhaven.c:4853. */
int card_restorative_burst(int player, int card, event_t event)
{
  /* Restorative Burst	|3|G|G
   * Sorcery
   * Return up to two target creature, land, and/or planeswalker cards from your graveyard to your hand. Each player gains 4 life. Exile ~. */

  mdfc_back(player, card, event);

  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;

  test_definition_t test;
  new_default_test_definition(&test, 0x103, "Select a creature, land, and/or planeswalker card.");
  if (event == EVENT_RESOLVE_SPELL)
  {
    bool returned = false;
    for (int index = 0; index < 2; ++index)
    {
      int grave_index = validate_target_from_grave(player, card, player, index);
      if (grave_index != -1)
      {
        from_grave_to_hand(player, grave_index, 1);
        returned = true;
      }
    }
    if (returned)
    {
      gain_life(player, 4);
      gain_life(1 - player, 4);
    }
    kill_card(player, card, KILL_EXILE);
  }
  return generic_spell(player, card, event, 0x28, NULL, NULL, 2, &test);
}

/* ManalinkEh 2022: 0x23d6740; original cards/strixhaven.c:4907. */
int card_plargg_dean_of_chaos(int player, int card, event_t event)
{
  /* Plargg, Dean of Chaos	|1|R
   * Legendary Creature - Orc Shaman 2/2
   * |T, Discard a card: Draw a card.
   * |4|R, |T: Reveal cards from the top of your library until you reveal a nonlegendary, nonland card with mana value 3 or less. You may cast that card without paying its mana cost. Put all revealed cards not cast this way on the bottom of your library in a random order. */

  int choice;
  card_instance_t *instance;
  int mode_or_index;
  int matches;
  int available_or_iid;
  char buffer[100];
  test_definition_t test;

  check_legend_rule(player, card, event);
  mdfc_front(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      mode_or_index = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x21, 0, 0, 0, 0,
                                                0, 0, 0, NULL, NULL);
      if ((mode_or_index == 0) &&
          (mode_or_index = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 4, 0, 0,
                                                     0, 1, 0, 0, NULL, NULL),
           mode_or_index == 0))
      {
        return 0;
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      mode_or_index = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x21, 0, 0, 0, 0,
                                                0, 0, 0, NULL, NULL);
      available_or_iid = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 4, 0, 0, 0,
                                                   1, 0, 0, NULL, NULL);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_MODE,
                           "Discard & Draw", mode_or_index, 0x14, "Reveal cards", available_or_iid,
                           count_deck(player), 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == 1)
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, 0x21, 0, 0, 0, 0, 0, 0, 0, NULL,
                                  NULL);
        if (spell_fizzled == 1)
        {
          return 0;
        }
        if (1 - player != current_turn)
        {
          return 0;
        }
        if (current_phase < 0x18)
        {
          return 0;
        }
        ai_modifier = ai_modifier + (7 - hand_count[player]) * 8;
        return 0;
      }
      if (choice != 2)
      {
        return 0;
      }
      generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 4, 0, 0, 0, 1, 0, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      mode_or_index = count_deck(player);
      ai_modifier = ai_modifier + mode_or_index;
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
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    if (instance->mode == '\x01')
    {
      draw_a_card(player);
    }
    else if (instance->mode == '\x02')
    {

      default_test_definition(&test, 1);
      test.subtype = 0x2001;
      test.type_flag = 1;
      test.subtype_flag = 1;
      test.cmc = 4;
      test.cmc_flag = 3;
      mode_or_index = reveal_card_until_you_reveal_test(player, player, &test, 0x400);
      if (mode_or_index != 0)
      {
        available_or_iid = deck_ptr[player][mode_or_index + -1];
        if ((available_or_iid != -1) &&
            (matches = new_make_test(player, available_or_iid, -1, &test), matches != 0))
        {

          scnprintf(buffer, 100, " Play %s\n Decline",
                    cards_ptr[cards_data[available_or_iid].id]->name);
          if (do_dialog(player, player, card, -1, -1, buffer, 0) == 0)
          {
            mode_or_index = mode_or_index + -1;
            play_card_in_deck_for_free(player, player, mode_or_index);
          }
        }
        put_top_x_on_bottom_in_random_order(player, mode_or_index);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d6d00; original cards/strixhaven.c:4991. */
int card_augusta_dean_of_order(int player, int card, event_t event)
{
  /* Augusta, Dean of Order	|2|W
   * Legendary Creature - Human Cleric 1/3
   * Other tapped creatures you control get +1/+0.
   * Other untapped creatures you control get +0/+1.
   * Whenever you attack, untap each creature you control, then tap any number of creatures you control. */

  mdfc_back(player, card, event);
  check_legend_rule(player, card, event);
  if (!in_play(player, card) && is_humiliated(player, card, AL_STATIC))
    return 0;
  if (card != affected_card && player == affected_card_controller &&
      is_what(player, affected_card, TYPE_CREATURE))
  {
    if (event == EVENT_POWER)
    {
      if (is_tapped(affected_card_controller, affected_card))
        ++event_result;
    }
    else if (event == EVENT_TOUGHNESS && !is_tapped(affected_card_controller, affected_card))
      ++event_result;
  }
  store_attackers(player, card, event, 0, player, -1, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_CREATURE);
    new_manipulate_all(player, card, player, &test, ACT_UNTAP);
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.illegal_abilities = 0;
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.allow_cancel = 3;
    card_instance_t *instance = get_card_instance(player, card);
    int marked[151];
    int mc = 0;
    while (can_target(&td) && new_pick_target(&td, "Select a creature to tap.", 0, 0x80))
    {
      int target_card = instance->targets[0].card;
      marked[mc] = target_card;
      state_untargettable(player, target_card, 1);
      ++mc;
    }
    for (int i = mc - 1; i >= 0; --i)
    {
      int target_card = marked[i];
      state_untargettable(player, target_card, 0);
      if (spell_fizzled != 1)
        tap_card(player, target_card);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d6ff0; original cards/strixhaven.c:5058. */
int card_plumb_the_forbidden(int player, int card, event_t event)
{
  /* Plumb the Forbidden	|1|B
   * Instant
   * As an additional cost to cast this spell, you may sacrifice one or more creatures. When you do, copy this spell for each creature sacrificed this way.
   * You draw a card and lose 1 life. */

  int result;
  card_instance_t *instance;
  test_definition_t test;

  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      result = is_token(player, card);
      if (result != 0)
      {
        return 0;
      }
      new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
      test.qty = 0x97;
      result = new_sacrifice(player, card, player,
                                  SAC_OPTIONAL | SAC_AS_COST | SAC_DONE, &test);
      if ((spell_fizzled != 1) && (0 < result))
      {
        instance = get_card_instance(player, card);
        instance->ext->times_replicate_paid = (uint8_t)result;
      }
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    draw_cards(player, 1);
    lose_life(player, 1);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23d7120; original cards/strixhaven.c:5085. */
int card_poets_quill(int player, int card, event_t event)
{
  /* Poet's Quill	|1|B
   * Artifact - Equipment
   * When this Equipment enters, learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.)
   * Equipped creature gets +1/+1 and has lifelink.
   * Equip |1|B */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
    learn(player, card);
  attached_gets_pt(player, card, event, 1, 1);
  attached_gets_kw(player, card, event, KEYWORD_0, SP_KEYWORD_LIFELINK);
  if (IS_GAA_EVENT(event))
    return basic_equipment(player, card, event, 0x10101);
  return 0;
}

/* ManalinkEh 2022: 0x23d7250; original cards/strixhaven.c:5112. */
int card_pop_quiz(int player, int card, event_t event)
{
  /* Pop Quiz	|2|U
   * Instant
   * Draw a card.
   * Learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    draw_cards(player, 1);
    learn(player, card);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23d72b0; original cards/strixhaven.c:5127. */
int card_practical_research(int player, int card, event_t event)
{
  /* Practical Research	|3|U|R
   * Instant
   * Draw four cards. Then discard two cards unless you discard an instant or sorcery card. */

  if (event != EVENT_RESOLVE_SPELL)
    return basic_spell(player, card, event);
  draw_cards(player, 4);
  test_definition_t test;
  new_default_test_definition(&test, 0x38, "Select an instant or sorcery card to discard");
  test.zone = 0x100;
  int selected = -1;
  if (check_battlefield_for_special_card(player, card, player, 0, &test))
    selected = new_select_a_card(player, player, 4, 0, 3, -1, &test);
  if (selected == -1)
    multidiscard(player, 2, 0);
  else
    discard_card(player, selected);
  kill_card(player, card, KILL_SACRIFICE);
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23d73c0; original cards/strixhaven.c:5155. */
int card_prismari_apprentice(int player, int card, event_t event)
{
  /* Prismari Apprentice	|U|R
   * Creature - Human Shaman 2/2
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, this creature can't be blocked this turn. If that spell has mana value 5 or greater, put a +1/+1 counter on this creature. */

  int result;

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    pump_ability_until_eot_no_repeat(player, card, player, card, 0, 0x10);
    result = get_cmc(trigger_cause_controller, trigger_cause);
    if (4 < result)
    {
      add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d7480; original cards/strixhaven.c:5179. */
int card_prismari_command(int player, int card, event_t event)
{
  /* Prismari Command	|1|U|R
   * Instant
   * Choose two -
   * * ~ deals 2 damage to any target.
   * * Target player draws two cards, then discards two cards.
   * * Target player creates a Treasure token.
   * * Destroy target artifact. */

  typedef enum
  {
    CHOICE_INVALID_TARGET = 0,
    CHOICE_DAMAGE = 1,
    CHOICE_DRAW_DISCARD = 2,
    CHOICE_TREASURE = 3,
    CHOICE_KILL_ARTIFACT = 4
  } CommandChoices;

  card_instance_t *instance;
  CommandChoices choice;
  int result;
  int player_available;
  int artifact_available;
  const char *prompt;
  unsigned int target_number;
  uint32_t artifact_priority;
  uint32_t treasure_priority;
  uint32_t damage_priority;
  uint32_t draw_priority;
  CommandChoices choices[2];
  token_generation_t token;
  target_definition_t td;
  target_definition_t td2;
  target_definition_t td3;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    default_target_definition(player, card, &td2, 0);
    td2.zone = TARGET_ZONE_PLAYERS;
    td2.preferred_controller = player;
    default_target_definition(player, card, &td3, 0x40);
    get_card_instance(player, card);
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    if (generic_spell(player, card, EVENT_CAN_CAST, 1, &td2, NULL, 1, NULL) == 0)
    {
      if (generic_spell(player, card, EVENT_CAN_CAST, 1, &td3, NULL, 1, NULL) == 0)
      {
        return 0;
      }
      if (result == 0)
      {
        return 0;
      }
    }
    else
    {
      generic_spell(player, card, EVENT_CAN_CAST, 1, &td3, NULL, 1, NULL);
    }
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  default_target_definition(player, card, &td2, 0);
  td2.zone = TARGET_ZONE_PLAYERS;
  td2.preferred_controller = player;
  default_target_definition(player, card, &td3, 0x40);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    choice = dialog_impl(player, card, event, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS, DLG_RANDOM,
                         DLG_MODE, "2 damage to any target", 0, 1, "Draw 2 & discard 2", 0, 1,
                         "Create a Treasure", 0, 1, "Destroy target artifact", 0, 1, 0, 0, 0, 0);
    choices[0] = choice;
    target_number = 0;
    for (int choice_index = 0; choice_index < 2; ++choice_index)
    {
      choice = choices[choice_index];
      switch (choice)
      {
      case CHOICE_INVALID_TARGET:
        break;
      case CHOICE_DAMAGE:
        result = validate_target(player, card, &td, target_number);
        if (result != 0)
        {
          damage_creature(instance->targets[target_number].player,
                          instance->targets[target_number].card, 2, player, card);
        }
        break;
      case CHOICE_DRAW_DISCARD:
        result = validate_target(player, card, &td2, target_number);
        if (result != 0)
        {
          draw_cards(instance->targets[target_number].player, 2);
          new_multidiscard(instance->targets[target_number].player, 2, 2, player);
        }
        break;
      case CHOICE_TREASURE:
        result = validate_target(player, card, &td2, target_number);
        if (result != 0)
        {
          default_token_definition(player, card, CARD_ID_TREASURE, &token);
          token.t_player = instance->targets[target_number].player;
          generate_token(&token);
        }
        break;
      case CHOICE_KILL_ARTIFACT:
        result = validate_target(player, card, &td3, target_number);
        if (result != 0)
        {
          action_on_target(player, card, target_number, ACT_KILL_DESTROY);
        }
      default:
        continue;
      }
      ++target_number;
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  artifact_priority = 1;
  treasure_priority = 1;
  damage_priority = 1;
  draw_priority = 1;
  if (ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0))
  {
    artifact_priority = 0x14;
    treasure_priority = 10;
    damage_priority = 0x1e;
    draw_priority = 0x28;
  }
  result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
  player_available = generic_spell(player, card, EVENT_CAN_CAST, 1, &td2, NULL, 1, NULL);
  artifact_available = generic_spell(player, card, EVENT_CAN_CAST, 1, &td3, NULL, 1, NULL);
  choice = dialog_impl(player, card, event, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS, DLG_RANDOM,
                       DLG_MODE, "2 damage to any target", result, damage_priority,
                       "Draw 2 & discard 2", player_available, draw_priority, "Create a Treasure",
                       player_available, treasure_priority, "Destroy target artifact",
                       artifact_available, artifact_priority, 0, 0, 0, 0);
  if (((event != EVENT_CAST_SPELL) || (card != affected_card)) ||
      (player != affected_card_controller))
  {

    return 0;
  }

  int target_index = 0;
  instance->number_of_targets = 0;
  choices[0] = choice;
  for (int choice_index = 0; choice_index < 2; ++choice_index)
  {
    choice = choices[choice_index];
    if (choice == CHOICE_INVALID_TARGET)
    {
      spell_fizzled = 1;
      continue;
    }
    if (choice < CHOICE_KILL_ARTIFACT)
    {
      if (choice < CHOICE_DRAW_DISCARD)
      {
        if (!new_pick_target(&td, "Select a target to damage", target_index, 0x81))
          return 0;
      }
      else
      {
        prompt = choice == CHOICE_DRAW_DISCARD ? "Select target player to draw & discard."
                                               : "Select target player to create a treasure.";
        if (!new_pick_target(&td2, prompt, target_index, 0x81))
          return 0;
      }
      ++target_index;
    }
    else if (choice == CHOICE_KILL_ARTIFACT)
    {
      if (!new_pick_target(&td3, "TARGET_ARTIFACT", target_index, 1))
        return 0;
      ++target_index;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d7cd0; original cards/strixhaven.c:5336. */
int card_prismari_pledgemage(int player, int card, event_t event)
{
  /* Prismari Pledgemage	|UR|UR
   * Creature - Orc Wizard 3/3
   * Defender
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, this creature can attack this turn as though it didn't have defender. */

  int result;

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    create_targetted_legacy_effect(player, card, effect_defender_can_attack_until_eot, player,
                                   card);
  }
  result = hybrid(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23d7d50; original cards/strixhaven.c:5351. */
int card_professor_onyx(int player, int card, event_t event)
{
  /* Professor Onyx	|4|B|B
   * Legendary Planeswalker - Liliana (5)
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, each opponent loses 2 life and you gain 2 life.
   * +1: You lose 1 life. Look at the top three cards of your library. Put one of them into your hand and the rest into your graveyard.
   * -3: Each opponent sacrifices a creature with the greatest power among creatures that player controls.
   * -8: Each opponent may discard a card. If they don't, they lose 3 life. Repeat this process six more times. */

  enum
  {
    CHOICE_REVEAL3 = 1,
    CHOICE_OPP_SAC = 2,
    CHOICE_DISC_OR_LOSE3 = 3
  };

  check_legend_rule(player, card, event);
  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    lose_life(1 - player, 2);
    gain_life(player, 2);
    return planeswalker(player, card, event, 5);
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return planeswalker(player, card, event, 5);
  int priority = (unsigned int)(life[player] > 6) * 5 + 5;
  int mode = dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, DLG_MODE,
                         "Lose 1 life and reval 3 cards", 1, priority, 1,
                         "Opp. sac the biggest creature", 1, 0x14, 0xfffffffd,
                         "Opp. discards or loses life", 1, 0x32, 0xfffffff8, 0, 0, 0, 0);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (!mode)
      return 0;
    return planeswalker(player, card, event, 5);
  }
  int saved_fizzle = spell_fizzled;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (mode == CHOICE_REVEAL3)
    {
      lose_life(player, 1);
      test_definition_t test;
      new_default_test_definition(&test, 0xff, "Select a card to add to your hand.");
      reveal_top_cards_of_library_and_choose(player, card, player, 3, 1, TUTOR_HAND, 0, TUTOR_GRAVE,
                                             0, &test);
    }
    else if (mode == CHOICE_OPP_SAC)
      player_sacrifices_the_creature_with_highest_power(player, card, 1 - player);
    else if (mode == CHOICE_DISC_OR_LOSE3)
    {
      int remaining_life_loss = 0x15;
      int opponent = 1 - player;
      while (hand_count[opponent] != 0)
      {
        spell_fizzled = 0;
        discard(opponent, 6, player);
        if (spell_fizzled == 1 || (remaining_life_loss -= 3) == 0)
          break;
      }
      spell_fizzled = saved_fizzle;
      lose_life(opponent, remaining_life_loss);
    }
  }
  return planeswalker(player, card, event, 5);
}

/* October 2022 COFF entry 0x23d8110: native tail jump to card_gnarled_professor. */
int card_professor_of_symbology(int player, int card, event_t event)
{
  /* Professor of Symbology	|1|W
   * Creature - Kor Cleric 2/1
   * When this creature enters, learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.) */

  return card_gnarled_professor(player, card, event);
}

/* ManalinkEh 2022: 0x23d8120; original cards/strixhaven.c:5432. */
int card_professor_of_zoomancy(int player, int card, event_t event)
{
  /* Professor of Zoomancy	|3|G
   * Creature - Bear Druid 4/3
   * When this creature enters, create a 1/1 black and green Pest creature token with "When this token dies, you gain 1 life." */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  make_pest(player, card, 1);
  return 0;
}

/* ManalinkEh 2022: 0x23d8170; original cards/strixhaven.c:5458. */
int card_quandrix_apprentice(int player, int card, event_t event)
{
  /* Quandrix Apprentice	|G|U
   * Creature - Human Wizard 2/2
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, look at the top three cards of your library. You may reveal a land card from among them and put that card into your hand. Put the rest on the bottom of your library in any order. */

  test_definition_t test3;

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  new_default_test_definition(&test3, 1, "Select a land card to reveal.");
  reveal_top_cards_of_library_and_choose(player, card, player, 3, 0, TUTOR_HAND, 1,
                                         TUTOR_BOTTOM_OF_DECK, 0, &test3);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23d8240; original cards/strixhaven.c:5482. */
int card_quandrix_command(int player, int card, event_t event)
{
  /* Quandrix Command	|1|G|U
   * Instant
   * Choose two -
   * * Return target creature or planeswalker to its owner's hand.
   * * Counter target artifact or enchantment spell.
   * * Put two +1/+1 counters on target creature.
   * * Target player shuffles up to three target cards from their graveyard into their library. */

  int i;
  typedef enum
  {
    CHOICE_INVALID_TARGET = 0,
    CHOICE_BOUNCE = 1,
    CHOICE_COUNTERSPELL = 2,
    CHOICE_P1_P1_COUNTERS = 3,
    CHOICE_RECYCLE = 4
  } CommandChoices;
  CommandChoices choice;
  int bounce_available;
  int counterspell_available;
  int counters_available;
  int recycle_available;
  const char *prompt;
  uint32_t bounce_priority;
  int counters_priority;
  int recycle_priority;
  target_definition_t *creature_target;
  target_definition_t *spell_target;
  card_instance_t *instance;
  target_definition_t *permanent_target;
  CommandChoices choices[2];
  target_definition_t td;
  target_definition_t td2;
  target_definition_t td3;
  target_definition_t td4;
  test_definition_t test;
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x102);
    counterspell_target_definition(player, card, &td2, 0x44);
    default_target_definition(player, card, &td3, 2);
    td3.preferred_controller = player;
    default_target_definition(player, card, &td4, 0);
    td4.zone = TARGET_ZONE_PLAYERS;
    td4.preferred_controller = player;
    new_default_test_definition(&test, 0xff, "Select target card.");
    get_card_instance(player, card);
    unsigned int available_modes =
        generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL) != 0;
    bool can_counter = counterspell(player, card, EVENT_CAN_CAST, &td2, 0) != 0;
    available_modes += generic_spell(player, card, EVENT_CAN_CAST, 1, &td3, NULL, 1, NULL) != 0;
    available_modes +=
        generic_spell(player, card, EVENT_CAN_CAST, 0x1000, NULL, NULL, 3, &test) != 0;
    if (available_modes == 0)
      return 0;
    if (can_counter)
      return 99;
    return available_modes >= 2;
  }

  if (event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 0x102);
  counterspell_target_definition(player, card, &td2, 0x44);
  default_target_definition(player, card, &td3, 2);
  td3.preferred_controller = player;
  default_target_definition(player, card, &td4, 0);
  td4.zone = TARGET_ZONE_PLAYERS;
  td4.preferred_controller = player;
  new_default_test_definition(&test, 0xff, "Select target card.");
  instance = get_card_instance(player, card);

  if (event == EVENT_RESOLVE_SPELL)
  {
    choice = dialog_impl(player, card, event, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS, DLG_RANDOM,
                         DLG_MODE, "Bounce target creature or planeswalker", 0, 1,
                         "Counter target artifact or enchantment spell", 0, 1,
                         "Target creature gets 2 +1/+1 counters", 0, 1, "Recycle 3 target cards", 0,
                         1, 0, 0, 0, 0);
    choices[0] = choice;
    unsigned int target_index = 0;
    int recycled = 0;
    for (int choice_index = 0; choice_index < 2; ++choice_index)
    {
      switch (choices[choice_index])
      {
      case CHOICE_INVALID_TARGET:
        ++target_index;
        break;
      case CHOICE_BOUNCE:
        if (validate_target(player, card, &td, target_index))
          action_on_target(player, card, target_index, ACT_BOUNCE);
        ++target_index;
        break;
      case CHOICE_COUNTERSPELL:
        counterspell(player, card, EVENT_RESOLVE_SPELL, &td2, target_index);
        ++target_index;
        break;
      case CHOICE_P1_P1_COUNTERS:
        if (validate_target(player, card, &td3, target_index))
          action_on_target(player, card, target_index, ACT_ADD_COUNTERS_BASE | ACT_KILL_DESTROY);
        ++target_index;
        break;
      case CHOICE_RECYCLE:
        for (i = target_index + 1; i <= (int)(target_index + 3); ++i)
        {
          int packed_owner = instance->targets[i].player;
          if (packed_owner == -1)
            break;
          int grave_index =
              validate_target_from_grave_source(player, card, packed_owner >> 0x10, i);
          if (grave_index != -1)
          {
            int owner = instance->targets[target_index].player;
            int added = add_card_to_hand(owner, graveyard_ptr_mutable[owner][grave_index]);
            put_on_top_of_deck(instance->targets[target_index].player, added);
            ++recycled;
          }
        }
        if (recycled != 0)
          shuffle(instance->targets[target_index].player);
        break;
      }
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  bounce_priority = 1;
  recycle_priority = 1;
  counters_priority = 1;
  if (ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0))
  {
    recycle_priority = count_graveyard(player);
    bounce_priority = 10;
    recycle_priority *= 2;
    counters_priority = 0x14;
  }
  permanent_target = &td;
  spell_target = &td2;
  creature_target = &td3;
  bounce_available =
      generic_spell(player, card, EVENT_CAN_CAST, 1, permanent_target, NULL, 1, NULL);
  counterspell_available = counterspell(player, card, EVENT_CAN_CAST, spell_target, 0);
  counters_available =
      generic_spell(player, card, EVENT_CAN_CAST, 1, creature_target, NULL, 1, NULL);
  recycle_available = generic_spell(player, card, EVENT_CAN_CAST, 0x1000, NULL, NULL, 3, &test);
  choice =
      dialog_impl(player, card, event, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS, DLG_RANDOM, DLG_MODE,
                  "Bounce target creature or planeswalker", bounce_available, bounce_priority,
                  "Counter target artifact or enchantment spell", counterspell_available, 1,
                  "Target creature gets 2 +1/+1 counters", counters_available, counters_priority,
                  "Recycle 3 target cards", recycle_available, recycle_priority, 0, 0, 0, 0);
  if (((event != EVENT_CAST_SPELL) || (card != affected_card)) ||
      (player != affected_card_controller))
  {
    return 0;
  }

  instance->number_of_targets = 0;
  int target_index = 0;
  choices[0] = choice;
  for (int choice_index = 0; choice_index < 2; ++choice_index)
  {
    switch (choices[choice_index])
    {
    case CHOICE_INVALID_TARGET:
      spell_fizzled = 1;
      break;
    case CHOICE_BOUNCE:
    case CHOICE_P1_P1_COUNTERS:
      prompt = choices[choice_index] == CHOICE_BOUNCE
                   ? "Select target creature or planeswalker to bounce."
                   : "Select target creature to get +1/+1 counters";
      // The native handler uses the creature/planeswalker td for both modes.
      if (!new_pick_target(permanent_target, prompt, target_index, 0x81))
        return 0;
      ++target_index;
      break;
    case CHOICE_COUNTERSPELL:
      counterspell(player, card, EVENT_CAST_SPELL, spell_target, target_index);
      ++target_index;
      break;
    case CHOICE_RECYCLE:
      select_multiple_cards_from_either_graveyard(player, card, SFG_NO_SPELL_FIZZLED, 1, 1, &test,
                                                  3, instance->targets + target_index + 1);
      break;
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x23c7c00; original cards/strixhaven.c:5659. */
static int is_basic_forest_or_island(int iid, int unused2, int player, int card)
{
  subtype_t subtype;
  subtype_t subtype_00;
  int basic_or_first_type;
  int second_type;

  subtype = get_hacked_subtype(player, card, 0x5001);
  subtype_00 = get_hacked_subtype(player, card, 0x5002);
  basic_or_first_type = has_subtype_by_id(player, (unsigned int)cards_data[iid].id, SUBTYPE_BASIC);
  second_type = 0;
  if (basic_or_first_type != 0)
  {
    basic_or_first_type = has_subtype_by_id(player, (unsigned int)cards_data[iid].id, subtype);
    second_type = 1;
    if (basic_or_first_type == 0)
    {
      second_type = has_subtype_by_id(player, (unsigned int)cards_data[iid].id, subtype_00);
      return (unsigned int)(second_type != 0);
    }
  }
  return second_type;
}

/* ManalinkEh 2022: 0x23d8cc0; original cards/strixhaven.c:5668. */
int card_quandrix_cultivator(int player, int card, event_t event)
{
  /* Quandrix Cultivator	|1|G|GU|U
   * Creature - Turtle Druid 3/4
   * When this creature enters, you may search your library for a basic Forest or Island card, put it onto the battlefield, then shuffle. */

  int result;
  const char *land_text;
  char buffer[100];
  test_definition_t test3;

  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {

    land_text = get_hacked_land_text(player, card, "Select a %s ", 0x5001);
    result = scnprintf(buffer, 100, "%s", land_text);
    land_text = get_hacked_land_text(player, card, "or %s card.", 0x5002);
    scnprintf(buffer + result, 100 - result, "%s", land_text);
    new_default_test_definition(&test3, 0xff, buffer);
    test3.special_selection_function = is_basic_forest_or_island;
    new_global_tutor(player, player, 1, 4, 0, 5, &test3);
  }
  return hybrid(player, card, event);
}

/* ManalinkEh 2022: 0x23d8e10; original cards/strixhaven.c:5688. */
int card_quandrix_pledgemage(int player, int card, event_t event)
{
  /* Quandrix Pledgemage	|1|GU|GU
   * Creature - Merfolk Druid 2/2
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, put a +1/+1 counter on this creature. */

  int result;

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  }
  result = hybrid(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23d8ea0; original cards/strixhaven.c:5702. */
int card_quintorius_field_historian(int player, int card, event_t event)
{
  /* Quintorius, Field Historian	|3|R|W
   * Legendary Creature - Elephant Cleric 2/4
   * Spirits you control get +1/+0.
   * Whenever one or more cards leave your graveyard, create a 3/2 red and white Spirit creature token. */

  card_instance_t *instance;

  check_legend_rule(player, card, event);
  boost_creature_type(player, card, event, SUBTYPE_SPIRIT, 1, 0, 0,
                      BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  if (event == EVENT_REMOVED_FROM_GRAVEYARD)
  {
    if (affected_card_controller == player)
    {
      instance = get_card_instance(player, card);
      if (instance->info_slot != 0x42)
      {
        make_one_spirit_strixhaven(player, card, 1);
        instance = get_card_instance(player, card);
        instance->info_slot = 0x42;
      }
    }
  }
  else if (event == EVENT_STATIC_EFFECTS)
  {
    instance = get_card_instance(player, card);
    if (instance->info_slot == 0x42)
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d8f80; original cards/strixhaven.c:5724. */
int card_radiant_scrollwielder(int player, int card, event_t event)
{
  /* Radiant Scrollwielder	|2|R|W
   * Creature - Dwarf Cleric 2/4
   * Instant and sorcery spells you control have lifelink.
   * At the beginning of your upkeep, exile an instant or sorcery card at random from your graveyard. You may cast it this turn. If a spell cast this way would be put into your graveyard, exile it instead. */

  enum
  {
    MPCFE_EXILE_ON_RESOLUTION = 128
  };

  int csvid;
  int *grave;
  int iid_or_selection;
  int fc;
  int i;
  int found[151];

  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {

    grave = graveyard_ptr_mutable[player];
    iid_or_selection = *grave;
    if (iid_or_selection != -1)
    {
      fc = 0;
      i = 0;
      do
      {
        if (is_what(-1, iid_or_selection, 0x38) != 0)
        {
          found[fc] = i;
          fc = fc + 1;
        }
        i = i + 1;
        iid_or_selection = grave[i];
      } while (iid_or_selection != -1);
      if (fc != 0)
      {
        int stp = internal_rand(fc);
        csvid = cards_data[grave[found[stp]]].id;
        rfg_card_from_grave(player, found[stp]);
        create_may_play_card_from_exile_effect(player, card, player, csvid,
                                               MPCFE_EXILE_ON_RESOLUTION | MPCFE_UNTIL_EOT);
        return 0;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d9080; original cards/strixhaven.c:5762. */
int card_reconstruct_history(int player, int card, event_t event)
{
  /* Reconstruct History	|2|R|W
   * Sorcery
   * Return up to one target artifact card, up to one target enchantment card, up to one target instant card, up to one target sorcery card, and up to one target planeswalker card from your graveyard to your hand.
   * Exile ~. */

  int i;
  unsigned int ret_location;
  const char *type_name;
  card_instance_t *instance;
  int grave_position;
  int *type_cursor;
  int type_or_selection;
  inst_info_t card_info;
  int types[5];
  char msg[100];
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      clear_info(instance, 0x58f6);
      type_cursor = types;
      types[0] = 0x40;
      types[1] = 4;
      types[2] = 0x10;
      types[3] = 8;
      types[4] = 0x100;
      type_or_selection = 0x40;
      while (true)
      {
        default_test_definition(&test, type_or_selection);
        if (new_special_count_grave(player, &test) != 0)
        {

          card_info = inst_info(instance, 0x58f6);
          ret_location = ((uint32_t)card_info.id | ((uint32_t)card_info.data2 << 16)) >> 0x10;
          type_name = type_text(msg, 100, test.type, TYPETEXT_0);
          sprintf(test.message, "Select target %s card.", type_name);
          type_or_selection =
              select_target_from_grave_source(player, card, player, SFG_0, 1, &test, ret_location);
          if (-1 < type_or_selection)
          {
            type_or_selection =
                turn_card_in_grave_face_down(player, instance->targets[ret_location].player);
            instance->targets[ret_location].card = type_or_selection;
            increment_data2(instance, 0x58f6);
          }
        }
        type_cursor = type_cursor + 1;
        if (type_cursor == types + 5)
          break;
        type_or_selection = *type_cursor;
      }
      for (i = 0; card_info = inst_info(instance, 0x58f6),
          i < (int)(((uint32_t)card_info.id | ((uint32_t)card_info.data2 << 16)) >> 0x10);
           i = i + 1)
      {
        turn_card_in_grave_face_up(player, instance->targets[i].player, instance->targets[i].card);
        instance->targets[i].card = graveyard_source[player][instance->targets[i].player];
      }
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);

    for (i = 0; card_info = inst_info(instance, 0x58f6),
        i < (int)(((uint32_t)card_info.id | ((uint32_t)card_info.data2 << 16)) >> 0x10);
         i = i + 1)
    {

      grave_position = validate_target_from_grave_source(player, card, player, i);
      if (grave_position != -1)
      {
        from_grave_to_hand(player, grave_position, 1);
      }
    }
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d9370; original cards/strixhaven.c:5813. */
int card_reduce_to_memory(int player, int card, event_t event)
{
  /* Reduce to Memory	|1|W|W
   * Sorcery - Lesson
   * Exile target nonland permanent. Its controller creates a 3/2 red and white Spirit creature token. */

  card_instance_t *instance;
  token_generation_t token;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.illegal_type = TYPE_LAND;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      default_token_definition(player, card, CARD_ID_SPIRIT, &token);
      token.t_player = instance->targets[0].player;
      token.pow = 3;
      token.tou = 2;
      token.color_forced = '0';
      generate_token(&token);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_NONLAND_PERMANENT", 1, NULL);
}

#ifndef SHANDALAR
static void exile_and_give_elemental(int player, int card, int t_player, int t_card)
{
  kill_card(t_player, t_card, KILL_EXILE);
  token_generation_t token;
  default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
  token.tou = 4;
  token.pow = 4;
  token.color_forced = 0x14;
  token.t_player = t_player;
  generate_token(&token);
}
#endif

/* ManalinkEh 2022: 0x23d9540; original cards/strixhaven.c:5871. */
int card_resculpt(int player, int card, event_t event)
{
  /* Resculpt	|1|U
   * Instant
   * Exile target artifact or creature. Its controller creates a 4/4 blue and red Elemental creature token. */

  card_instance_t *instance;
  int result;
  token_generation_t token;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x42);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      result = instance->targets[0].player;
      kill_card(result, instance->targets[0].card, KILL_EXILE);
      default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
      token.tou = 4;
      token.pow = 4;
      token.color_forced = '\x14';
      token.t_player = result;
      generate_token(&token);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td, "Select target artifact or creature.", 1, NULL);
}

/* ManalinkEh 2022: 0x23d96f0; original cards/strixhaven.c:5895. */
int card_retriever_phoenix(int player, int card, event_t event)
{
  /* Retriever Phoenix	|3|R
   * Creature - Phoenix 2/2
   * Flying, haste
   * When this creature enters, if you cast it, learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.)
   * As long as this card is in your graveyard, if you would learn, you may instead return this card to the battlefield. */

  int result;

  haste(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    result = check_special_flags(player, card, 0x40000);
    if (result == 0)
    {
      learn(player, card);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d9770; original cards/strixhaven.c:5920. */
int card_rip_apart(int player, int card, event_t event)
{
  /* Rip Apart	|R|W
   * Sorcery
   * Choose one -
   * * ~ deals 3 damage to target creature or planeswalker.
   * * Destroy target artifact or enchantment. */

  enum
  {
    CHOICE_DAMAGE = 1,
    CHOICE_ARTIFACT_ENCHANTMENT = 2
  };

  int choice;
  card_instance_t *instance;
  int can_destroy_or_valid;
  target_definition_t *td;
  const char *prompt;
  target_definition_t td1;
  target_definition_t td2;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td1, 0x102);
    default_target_definition(player, card, &td2, 0x44);
    get_card_instance(player, card);
    if (generic_spell(player, card, EVENT_CAN_CAST, 1, &td1, NULL, 1, NULL) == 0)
    {
      return generic_spell(player, card, EVENT_CAN_CAST, 1, &td2, NULL, 1, NULL);
    }
    return 1;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      td = &td2;
      default_target_definition(player, card, &td1, 0x102);
      default_target_definition(player, card, td, 0x44);
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        if (check_special_flags2(player, card, -0x80000000) == 0)
        {

          can_destroy_or_valid = can_target(td);
          choice = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_MODE,
                               "Damage target creature or planeswalker", can_target(&td1), 5,
                               "Destroy target artifact or enchantment", can_destroy_or_valid, 5, 0,
                               0, 0, 0);
          if (choice == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
          if (choice == CHOICE_DAMAGE)
          {
            td = &td1;
            prompt = "TARGET_CREATURE_OR_PLANESWALKER";
          }
          else
          {
            if (choice != CHOICE_ARTIFACT_ENCHANTMENT)
            {
              return 0;
            }
            prompt = "DISENCHANT";
          }
          return generic_spell(player, card, EVENT_CAST_SPELL, 1, td, prompt, 1, NULL);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td1, 0x102);
    default_target_definition(player, card, &td2, 0x44);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      if (instance->mode == '\x01')
      {
        if (valid_target(&td1) != 0)
        {
          damage_target0(player, card, 3);
        }
      }
      else if ((instance->mode == '\x02') &&
               (can_destroy_or_valid = valid_target(&td2), can_destroy_or_valid != 0))
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td1, 0x102);
    default_target_definition(player, card, &td2, 0x44);
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23d9b90; original cards/strixhaven.c:5986. */
int card_rootha_mercurial_artist(int player, int card, event_t event)
{
  /* Rootha, Mercurial Artist	|1|U|R
   * Legendary Creature - Orc Shaman 1/4
   * |2, Return Rootha to its owner's hand: Copy target instant or sorcery spell you control. You may choose new targets for the copy. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  check_legend_rule(player, card, event);
  if (!IS_GAA_EVENT(event))
    return 0;

  counterspell_target_definition(player, card, &td, 0x38);
  td.allowed_controller = player;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = counterspell_validate(player, card, &td, 0);
    if (result != 0)
    {
      copy_spell_from_stack(player, instance->targets[0].player, instance->targets[0].card);
    }
  }
  result = generic_activated_ability(player, card, event, 0x20400, 2, 0, 0, 0, 0, 0, 0, &td, NULL);
  return result;
}

/* ManalinkEh 2022: 0x23d9cf0; original cards/strixhaven.c:6010. */
int card_rowan_scholar_of_sparks(int player, int card, event_t event)
{
  /* Rowan, Scholar of Sparks	|2|R
   * Legendary Planeswalker - Rowan (2)
   * Instant and sorcery spells you cast cost |1 less to cast.
   * +1: Rowan deals 1 damage to each opponent. If you've drawn three or more cards this turn, she deals 3 damage to each opponent instead.
   * -4: You get an emblem with "Whenever you cast an instant or sorcery spell, you may pay |2. If you do, copy that spell. You may choose new targets for the copy." */

  enum
  {
    CHOICE_DAMAGE = 1,
    CHOICE_EMBLEM = 2
  };

  card_instance_t *instance;
  int result;

  mdfc_front(player, card, event);
  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    instance = in_play(player, card);
    if ((((instance != NULL) &&
          (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
         (affected_card_controller == player)) &&
        (result = is_what(player, affected_card, 0x38), result != 0))
    {
      COST_COLORLESS = COST_COLORLESS + -1;
    }
  }
  else if ((event == EVENT_RESOLVE_ACTIVATION) ||
           (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE))
  {

    instance = get_card_instance(player, card);
    result = dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, "Damage opponent", 1, 10, 1,
                         "Emblem", 1, 0x32, 0xfffffffc, 0, 0, 0, 0);
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
    }
    else if (result == CHOICE_DAMAGE)
    {

      damage_player(1 - player,
                    ((cards_drawn_this_turn[player] < 3) ? 1U : 3U), player,
                    card);
    }
    else
    {
      generate_token_by_id(player, card, CARD_ID_ROWAN_SCHOLAR_OF_SPARKS_EMBLEM);
    }
  }
  result = planeswalker(player, card, event, 2);
  return result;
}

/* ManalinkEh 2022: 0x23d9ee0; original cards/strixhaven.c:6059. */
int card_will_scholar_of_frost(int player, int card, event_t event)
{
  /* Will, Scholar of Frost	|4|U
   * Legendary Planeswalker - Will (4)
   * Instant and sorcery spells you cast cost |1 less to cast.
   * +1: Up to one target creature has base power and toughness 0/2 until your next turn.
   * -3: Draw two cards.
   * -7: Exile up to five target permanents. For each permanent exiled this way, its controller creates a 4/4 blue and red Elemental creature token. */

  enum
  {
    CHOICE_WEAKEN = 1,
    CHOICE_DRAW = 2,
    CHOICE_EXILE = 3
  };

  int i;
  target_t *target;
  target_t *selected_target;
  card_instance_t *instance;
  int result_or_mode;
  int valid[5];
  token_generation_t token;
  target_definition_t td;
  target_definition_t td2;

  mdfc_back(player, card, event);
  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    instance = in_play(player, card);
    if ((((instance != NULL) &&
          (result_or_mode = is_humiliated(player, card, AL_STATIC), result_or_mode == 0)) &&
         (affected_card_controller == player)) &&
        (result_or_mode = is_what(player, affected_card, 0x38), result_or_mode != 0))
    {
      COST_COLORLESS = COST_COLORLESS + -1;
    }
  }
  else if ((event == EVENT_RESOLVE_ACTIVATION) ||
           (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE))
  {

    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 3;
    default_target_definition(player, card, &td2, 0x47);
    result_or_mode =
        dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, "Target creature is 0/2",
                    can_target(&td), 10, 1, "Draw 2 cards", 1, 0x14, 0xfffffffd,
                    "Exile 5 permanents", 1, 0x32, 0xfffffff9, 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (result_or_mode == 0)
      {
        return 0;
      }
    }
    else if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      if (result_or_mode == CHOICE_WEAKEN)
      {
        new_pick_target(&td, "TARGET_CREATURE", 0, 1);
      }
      else if (result_or_mode == CHOICE_EXILE)
      {

        i = 0;
        do
        {
          if (new_pick_target(&td2, "TARGET_PERMANENT", i, 0) != 0)
          {
            state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
          }
          i = i + 1;
        } while (i != 5);
        if (instance->number_of_targets != '\0')
        {
          i = 0;
          do
          {
            selected_target = instance->targets + i;
            target = instance->targets + i;
            i = i + 1;
            state_untargettable(target->player, selected_target->card, 0);
          } while (i < (int)(unsigned int)instance->number_of_targets);
        }
      }
    }
    else if (result_or_mode == CHOICE_WEAKEN)
    {
      if (validate_target(player, card, &td, 0) != 0)
      {
        set_pt_and_abilities_until_eot(player, card, instance->targets[0].player,
                                       instance->targets[0].card, 0, 2, 0, 0, 8);
      }
    }
    else if (result_or_mode == CHOICE_DRAW)
    {
      draw_cards(player, 2);
    }
    else
    {

      valid[0] = 0;
      valid[1] = 0;
      valid[2] = 0;
      valid[3] = 0;
      valid[4] = 0;
      if (instance->number_of_targets != '\0')
      {
        i = 0;
        do
        {
          if (validate_target(player, card, &td2, i) != 0)
          {
            valid[i] = 1;
          }
          i = i + 1;
        } while (i < (int)(unsigned int)instance->number_of_targets);
        if (instance->number_of_targets != 0)
        {
          i = 0;
          do
          {
            while (true)
            {
              if (valid[i] != 0)
              {
                default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
                token.tou = 4;
                token.pow = 4;
                token.color_forced = '\x14';
                token.t_player = instance->targets[i].player;
                generate_token(&token);
              }
              if (validate_target(player, card, &td2, i) != 0)
                break;
              i = i + 1;
              if ((int)(unsigned int)instance->number_of_targets <= i)
                return planeswalker(player, card, event, 4);
            }
            selected_target = instance->targets + i;
            target = instance->targets + i;
            i = i + 1;
            kill_card(target->player, selected_target->card, KILL_EXILE);
          } while (i < (int)(unsigned int)instance->number_of_targets);
        }
      }
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x23da3d0; original cards/strixhaven.c:6155. */
int card_rowan_scholar_of_sparks_emblem(int player, int card, event_t event)
{
  /* Rowan, Scholar of Sparks Emblem	""
   * Emblem
   * Whenever you cast an instant or sorcery spell, you may pay |2. If you do, copy that spell. You may choose new targets for the copy. */

  int result;

  sp_ability_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    result = has_mana(player, 0, 2);
    if (result != 0)
    {
      charge_mana(player, COLOR_COLORLESS, 2);
      if (spell_fizzled != 1)
      {
        copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23c7cd0; original cards/strixhaven.c:6173. */
int rushed_rebirth_legacy(int player, int card, event_t event)
{
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    card_instance_t *inst = get_card_instance(player, card);
    int c = inst->damage_target_card;
    int p = inst->damage_target_player;
    if (c == affected_card && p == affected_card_controller &&
        !check_for_death_trigger_removal(p, c))
    {
      card_instance_t *affected = get_card_instance(p, c);
      if ((unsigned char)(affected->kill_code - 1) < 3)
      {
        p = inst->damage_target_player;
        c = inst->damage_target_card;
        inst->targets[11].player = 0x42;
        inst->targets[0].card = c;
        inst->targets[0].player = p;
        int mv = get_cmc(p, c);
        inst->targets[1].player = mv;
        int num = 1;
        if (is_what(inst->damage_target_player, inst->damage_target_card, TYPE_CREATURE))
          num = card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
        inst->targets[11].player = num;
        inst->damage_target_player = -1;
        inst->damage_target_card = -1;
      }
    }
  }
  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result)
  {
    card_instance_t *inst = get_card_instance(player, card);
    int mv = inst->targets[1].player;
    char buffer[100];
    scnprintf(buffer, sizeof(buffer), "Select a creature card with mana value %d or less.", mv - 1);
    test_definition_t test;
    new_default_test_definition(&test, TYPE_CREATURE, buffer);
    test.cmc_flag = 3;
    test.cmc = mv;
    do
    {
      new_global_tutor(player, player, 1, 5, 0, 1, &test);
    } while (--result != 0);
    kill_card(player, card, KILL_EXILE);
  }
  if (event == EVENT_CLEANUP)
    kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x23da480; original cards/strixhaven.c:6221. */
int card_rushed_rebirth(int player, int card, event_t event)
{
  /* Rushed Rebirth	|B|G
   * Instant
   * Choose target creature. When that creature dies this turn, search your library for a creature card with lesser mana value, put it onto the battlefield tapped, then shuffle. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CHECK_PUMP)
  {
    instance = in_hand(player, card);
    if ((instance != NULL) &&
        (result = has_mana_to_cast_iid(player, EVENT_CAN_CAST, instance->internal_card_id),
         result != 0))
    {
      pumpable_toughness[player] = pumpable_toughness[player] + 99;
    }
    return 0;
  }

  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
      create_targetted_legacy_effect(player, card, rushed_rebirth_legacy,
                                     instance->targets[0].player, instance->targets[0].card);
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x23da640; original cards/strixhaven.c:6264. */
int card_secret_rendezvous(int player, int card, event_t event)
{
  /* Secret Rendezvous	|1|W|W
   * Sorcery
   * You and target opponent each draw three cards. */

  int result;
  target_definition_t td;
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
      default_target_definition(player, card, &td, 0);
      td.zone = TARGET_ZONE_PLAYERS;
      result = valid_target(&td);
      if (result != 0)
      {
        draw_cards(player, 3);
        draw_cards(1 - player, 3);
      }
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  result = generic_spell(player, card, event, 0x40, NULL, NULL, 1, NULL);
  return result;
}

/* ManalinkEh 2022: 0x23da760; original cards/strixhaven.c:6286. */
int card_sedgemoor_witch(int player, int card, event_t event)
{
  /* Sedgemoor Witch	|2|B
   * Creature - Human Warlock 3/2
   * Menace
   * Ward-Pay 3 life. (Whenever this creature becomes the target of a spell or ability an opponent controls, counter it unless that player pays 3 life.)
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, create a 1/1 black and green Pest creature token with "When this token dies, you gain 1 life." */

  ward_life(player, card, event, 3);
  menace(player, card, event);
  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  make_pest(player, card, 1);
  return 0;
}

/* ManalinkEh 2022: 0x23da7f0; original cards/strixhaven.c:6305. */
int card_selfless_glyphweaver(int player, int card, event_t event)
{
  /* Selfless Glyphweaver	|2|W
   * Creature - Human Cleric 2/3
   * Exile this creature: Creatures you control gain indestructible until end of turn. */

  int result;

  mdfc_front(player, card, event);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    pump_creatures_until_eot_no_repeat(player, card, player, 0, 0, 0x800, NULL);
  }
  result = generic_activated_ability(player, card, event, 0x4000, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x23da8c0; original cards/strixhaven.c:6317. */
int card_deadly_vanity(int player, int card, event_t event)
{
  /* Deadly Vanity	|5|B|B|B
   * Sorcery
   * Choose a creature or planeswalker, then destroy all other creatures and planeswalkers. */

  mdfc_back(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x102);
    td.preferred_controller = player;
    td.illegal_abilities = 0;
    td.allow_cancel = 0;
    card_instance_t *inst = get_card_instance(player, card);
    int chosen = can_target(&td);
    if (chosen)
    {
      new_pick_target(&td, "Select a creature or planeswalker.", 0, 0x80);
      inst->number_of_targets = 0;
    }
    APNAP(p, {
      for (int c = 0; c < active_cards_count[p]; ++c)
        if (is_what(p, c, 0x102) &&
            (!chosen || inst->targets[0].player != p || inst->targets[0].card != c))
          kill_card(p, c, KILL_DESTROY);
    });
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23daa60; original cards/strixhaven.c:6352. */
int card_semesters_end(int player, int card, event_t event)
{
  /* Semester's End	|3|W
   * Instant
   * Exile any number of target creatures and/or planeswalkers you control. At the beginning of the next end step, return each of them to the battlefield under its owner's control. Each of them enters with an additional +1/+1 counter on it if it's a creature and an additional loyalty counter on it if it's a planeswalker. */

  target_t *target;
  target_t *selected_target;
  card_instance_t *instance;
  int i;
  target_definition_t *permanent_target;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x102);
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.allow_cancel = 3;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
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
        remove_until_eot(player, card, target->player, selected_target->card);
      } while (i < (int)(unsigned int)instance->number_of_targets);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  permanent_target = &td;
  return generic_spell(player, card, event, 0xa0, permanent_target,
                       "Select target creature or planeswalker you control.", 10, NULL);
}

/* ManalinkEh 2022: 0x23dac10; original cards/strixhaven.c:6382. */
int card_serpentine_curve(int player, int card, event_t event)
{
  /* Serpentine Curve	|3|U
   * Sorcery
   * Create a 0/0 green and blue Fractal creature token. Put X +1/+1 counters on it, where X is one plus the total number of instant and sorcery cards you own in exile and in your graveyard. */

  int count;
  int exiled_spells;

  if (event == EVENT_RESOLVE_SPELL)
  {

    count = count_graveyard_by_type(player, 0x38);
    exiled_spells = count_rfg_by_type(player, 0x38);
    make_one_fractal(player, card, 1, count + 1 + exiled_spells);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23c7f10; original cards/strixhaven.c:6398. */
static int shadewing_laureate_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (event == EVENT_STATIC_EFFECTS && card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player >= 0 &&
        get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_SHADEWING_LAUREATE)
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
        get_card_instance(affected_card_controller, affected_card)->kill_code &&
        check_for_ability(affected_card_controller, affected_card, 0x20))
    {
      if (instance->damage_target_player >= 0 && instance->damage_target_card == affected_card &&
          instance->damage_target_player == affected_card_controller)
      {
        instance->damage_target_card = -1;
        instance->damage_target_player = -1;
        remove_status(player, card, 0x10000);
        return 0;
      }
      count_for_gfp_ability(player, card, EVENT_GRAVEYARD_FROM_PLAY, player, 2, NULL);
    }
  }
  unsigned int deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (deaths && !check_status(player, card, 0x10000))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    for (deaths &= 0xff; deaths; --deaths)
    {
      if (!can_target(&td))
        break;
      if (new_pick_target(&td, "TARGET_CREATURE", 1, 0))
        add_counters(player, card, instance->targets[1].player, instance->targets[1].card,
                     COUNTER_P1_P1, 1);
      instance->number_of_targets = 0;
    }
    kill_card(player, card, KILL_EXILE);
  }
  if (event == EVENT_CLEANUP && !check_status(player, card, 0x10000))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x23dac90; original cards/strixhaven.c:6456. */
int card_shadewing_laureate(int player, int card, event_t event)
{
  /* Shadewing Laureate	|W|WB|B
   * Creature - Human Warlock 2/2
   * Flying
   * Whenever another creature you control with flying dies, put a +1/+1 counter on target creature you control. */

  int c;
  card_instance_t *instance;
  int legacy_or_result;
  unsigned int deaths;
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
                instance->info_slot_fn == shadewing_laureate_effect)) &&
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
            create_targetted_legacy_effect(player, card, shadewing_laureate_effect, player, card);
        instance = get_card_instance(player, legacy_or_result);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, legacy_or_result, 0x10000);
      }
    }
  }
  else if ((event == EVENT_GRAVEYARD_FROM_PLAY) &&
           (legacy_or_result = check_for_ability(affected_card_controller, affected_card, 0x20),
            legacy_or_result != 0))
  {
    count_for_gfp_ability(player, card, 0x77, player, 2, NULL);
  }
  instance = get_card_instance(player, card);

  if ((instance->kill_code == '\0') &&
      (deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY), deaths != 0))
  {

    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    deaths = deaths & 0xff;
    if (deaths != 0)
    {
      do
      {
        if (can_target(&td) == 0)
          break;
        if (new_pick_target(&td, "TARGET_CREATURE", 0, 0) != 0)
        {
          add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                       COUNTER_P1_P1, 1);
        }
        deaths = deaths - 1;
        instance->number_of_targets = '\0';
      } while (0 < (int)deaths);
    }
  }
  return hybrid(player, card, event);
}

/* ManalinkEh 2022: 0x23c8540; original cards/strixhaven.c:6513. */
static int player_is_valid_target(int player, int card, int t_player)
{
  uint8_t saved_target_count;
  int32_t saved_player;
  int32_t saved_card;
  card_instance_t *instance;
  int valid;
  target_definition_t td;

  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 0);
  td.allow_cancel = 0;
  saved_player = instance->targets[0].player;
  saved_card = instance->targets[0].card;
  saved_target_count = instance->number_of_targets;
  instance->targets[0].player = t_player;
  instance->targets[0].card = -1;
  instance->number_of_targets = '\x01';
  td.zone = TARGET_ZONE_PLAYERS;
  td.preferred_controller = t_player;
  valid = would_valid_target(&td);
  instance->targets[0].player = saved_player;
  instance->targets[0].card = saved_card;
  instance->number_of_targets = saved_target_count;
  return valid;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23daf20; original cards/strixhaven.c:6537. */
int card_shadrix_silverquill(int player, int card, event_t event)
{
  /* Shadrix Silverquill	|3|W|B
   * Legendary Creature - Elder Dragon 2/5
   * Flying, double strike
   * At the beginning of combat on your turn, you may choose two. Each mode must target a different player.
   * * Target player creates a 2/1 white and black Inkling creature token with flying.
   * * Target player draws a card and loses 1 life.
   * * Target player puts a +1/+1 counter on each creature they control. */

  int choice;
  int token_recipient_or_valid;
  unsigned int t_player;
  int draw_recipient_or_valid;
  const char *recipient_text;
  int counter_recipient_or_valid;
  uint32_t token_priority;
  int draw_priority;
  int counter_priority;
  int can_add_counters;
  unsigned int selection_pass;
  int abils[4];
  target_definition_t td;
  char buffer1[100];
  char buffer2[100];
  char buffer3[100];
  check_legend_rule(player, card, event);
  if (beginning_of_combat(player, card, event, player) != 0)
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    token_recipient_or_valid = would_validate_arbitrary_target(&td, player, -1);
    if ((token_recipient_or_valid != 0) ||
        (token_recipient_or_valid = would_validate_arbitrary_target(&td, 1 - player, -1),
         token_recipient_or_valid != 0))
    {
      abils[0] = -1;
      abils[1] = -1;
      abils[2] = -1;
      abils[3] = -1;
      selection_pass = 0;
      do
      {
        counter_recipient_or_valid = abils[3];
        draw_recipient_or_valid = abils[2];
        token_recipient_or_valid = abils[1];
        t_player = selection_pass ^ current_turn;
        if (player == t_player)
        {
          draw_priority = ((6 < life[player]) ? 30U : 5U);
          recipient_text = "You";
          counter_priority = count_subtype(player, 2, -1);
          token_priority = 10;
          counter_priority = counter_priority * 10;
        }
        else
        {
          recipient_text = "Opponent";
          draw_priority = (2 - life[1 - player]) * 10;
          counter_priority = count_subtype(player, 2, -1);
          token_priority = 0xfffffff6;
          counter_priority = counter_priority * -5;
        }
        scnprintf(buffer1, 100, "%s get token", recipient_text);
        scnprintf(buffer2, 100, "%s draw and lose 1 life", recipient_text);
        scnprintf(buffer3, 100, "%s get counters", recipient_text);
        can_add_counters = 0;
        if (counter_recipient_or_valid == -1)
        {
          can_add_counters = player_is_valid_target(player, card, t_player);
        }
        counter_recipient_or_valid = 0;
        if (draw_recipient_or_valid == -1)
        {
          counter_recipient_or_valid = player_is_valid_target(player, card, t_player);
        }
        draw_recipient_or_valid = 0;
        if (token_recipient_or_valid == -1)
        {
          draw_recipient_or_valid = player_is_valid_target(player, card, t_player);
        }
        choice = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL,
                             buffer1, draw_recipient_or_valid, token_priority, buffer2,
                             counter_recipient_or_valid, draw_priority, buffer3, can_add_counters,
                             counter_priority, 0, 0, 0, 0);
        abils[choice] = t_player;
        selection_pass = selection_pass + 1;
      } while (selection_pass != 2);

      for (int mode = 1; mode != 4; ++mode)
      {
        int recipient = abils[mode];
        if (recipient < 0)
          continue;
        if (mode == 1)
        {
          token_generation_t token;
          default_token_definition(player, card, CARD_ID_INKLING, &token);
          token.pow = 2;
          token.tou = 1;
          token.key_plus = KEYWORD_FLYING;
          token.color_forced = 34;
          token.t_player = recipient;
          generate_token(&token);
        }
        else if (mode == 2)
        {
          draw_cards(recipient, 1);
          lose_life(recipient, 1);
        }
        else
        {
          manipulate_type(player, card, recipient, TYPE_CREATURE,
                          ACT_ADD_COUNTERS_BASE | ACT_KILL_BURY);
          return 0;
        }
      }
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x23db3a0; original cards/strixhaven.c:6613. */
int card_shaile_dean_of_radiance(int player, int card, event_t event)
{
  /* Shaile, Dean of Radiance	|1|W
   * Legendary Creature - Bird Cleric 1/1
   * Flying, vigilance
   * |T: Put a +1/+1 counter on each creature that entered the battlefield under your control this turn. */

  card_instance_t *instance;
  int eligible;
  int c;

  mdfc_front(player, card, event);
  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  if (event == EVENT_ACTIVATE)
  {

    c = active_cards_count[player] + -1;
    if (-1 < c)
    {
      do
      {
        instance = in_play(player, c);
        if (((instance != NULL) && (eligible = is_what(player, c, 2), eligible != 0)) &&
            (eligible = check_special_flags(player, c, 0x1000), eligible != 0))
        {
          ai_modifier = ai_modifier + 10;
        }
        c = c + -1;
      } while (c != -1);
    }
  }
  else
  {

    if ((event == EVENT_RESOLVE_ACTIVATION) &&
        (c = active_cards_count[player] + -1, -1 < active_cards_count[player] + -1))
    {
      do
      {
        while ((instance = in_play(player, c),
                instance == NULL ||
                    ((eligible = is_what(player, c, 2),
                      eligible == 0 ||
                          (eligible = check_special_flags(player, c, 0x1000), eligible == 0)))))
        {
          c = c + -1;
          if (c == -1)
            return generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 0, 0, 0, NULL,
                                             NULL);
        }
        eligible = c + -1;
        add_counters(player, card, player, c, COUNTER_P1_P1, 1);
        c = eligible;
      } while (eligible != -1);
    }
  }
  return generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x23c8280; original cards/strixhaven.c:6645. */
static int embrose_dean_of_shadow_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (event == EVENT_STATIC_EFFECTS && card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player >= 0 &&
        get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_EMBROSE_DEAN_OF_SHADOW)
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
        get_card_instance(affected_card_controller, affected_card)->kill_code)
    {
      if (instance->damage_target_player >= 0 && instance->damage_target_card == affected_card &&
          instance->damage_target_player == affected_card_controller)
      {
        instance->damage_target_card = -1;
        instance->damage_target_player = -1;
        remove_status(player, card, 0x10000);
      }
      if (count_counters(affected_card_controller, affected_card, COUNTER_P1_P1))
        count_for_gfp_ability(player, card, EVENT_GRAVEYARD_FROM_PLAY, player, 2, NULL);
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

/* ManalinkEh 2022: 0x23db560; original cards/strixhaven.c:6693. */
int card_embrose_dean_of_shadow(int player, int card, event_t event)
{
  /* Embrose, Dean of Shadow	|2|B|B
   * Legendary Creature - Human Warlock 4/4
   * |T: Put a +1/+1 counter on another target creature, then Embrose deals 2 damage to that creature.
   * Whenever a creature you control with a +1/+1 counter on it dies, draw a card. */

  card_instance_t *instance;
  bool favorable_target;
  card_instance_t *legacy;
  int index_or_toughness;
  card_instance_t *target;
  target_definition_t td;

  mdfc_back(player, card, event);
  check_legend_rule(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      bool found = false;
      if (0 < active_cards_count[player])
      {
        index_or_toughness = 0;
        do
        {
          legacy = in_play(player, index_or_toughness);

          if ((((legacy != NULL) && (is_what(player, index_or_toughness, 0x80) != 0)) &&
               (legacy = get_card_instance(player, index_or_toughness),
                legacy->info_slot_fn == embrose_dean_of_shadow_effect)) &&
              (((legacy->damage_target_player == player && (legacy->damage_target_card == card)) ||
                ((legacy->targets[0].player == player && (legacy->targets[0].card == card))))))
          {
            found = true;
            break;
          }
          index_or_toughness = index_or_toughness + 1;
        } while (index_or_toughness < active_cards_count[player]);
      }

      if (!found)
      {
        index_or_toughness = create_targetted_legacy_effect(
            player, card, embrose_dean_of_shadow_effect, player, card);
        instance = get_card_instance(player, index_or_toughness);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, index_or_toughness, 0x10000);
      }
    }
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    if (count_counters(affected_card_controller, affected_card, COUNTER_P1_P1))
      count_for_gfp_ability(player, card, 0x77, player, 2, NULL);
  }
  instance = get_card_instance(player, card);
  if (instance->kill_code == 0)
  {
    unsigned int deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (deaths != 0)
      draw_cards(player, deaths & 0xff);
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
    return 0;
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 2;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
      return generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                                       "TARGET_CREATURE");
    generic_activated_ability(player, card, EVENT_ACTIVATE, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                              "TARGET_CREATURE");
    if (spell_fizzled != 1)
    {
      index_or_toughness = get_toughness(instance->targets[0].player, instance->targets[0].card);
      target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
      index_or_toughness = (index_or_toughness + 1) - (unsigned int)target->damage_on_card;
      if (instance->targets[0].player == player)
      {
        favorable_target = 2 < index_or_toughness;
      }
      else
      {
        favorable_target = index_or_toughness < 3;
      }
      ai_modifier = ai_modifier + (favorable_target - 1 & 0xffffffa0) + 0x30;
    }
  }
  else if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 2;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) &&
        (index_or_toughness = valid_target(&td), index_or_toughness != 0))
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                   COUNTER_P1_P1, 1);
      damage_target0(player, card, 2);
    }
    return generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
  }
  return 0;
}

/* ManalinkEh 2022: 0x23db9f0; original cards/strixhaven.c:6771. */
int card_shineshadow_snarl(int player, int card, event_t event)
{
  /* Shineshadow Snarl	""
   * Land
   * As this land enters, you may reveal a Plains or Swamp card from your hand. If you don't, this land enters tapped.
   * |T: Add |W or |B. */

  int result;

  soi_need_subtype_land(player, card, event, SUBTYPE_PLAINS, SUBTYPE_SWAMP);
  result = plain_mana_producer(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23dba40; original cards/strixhaven.c:6787. */
int card_silverquill_apprentice(int player, int card, event_t event)
{
  /* Silverquill Apprentice	|W|B
   * Creature - Human Warlock 2/2
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, target creature gets +1/+0 until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    base_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE", 0, 0);
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        pump_until_eot_merge_previous(player, card, instance->targets[0].player,
                                      instance->targets[0].card, 1, 0);
        instance->number_of_targets = '\0';
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23dbb20; original cards/strixhaven.c:6815. */
int card_silverquill_command(int player, int card, event_t event)
{
  /* Silverquill Command	|2|W|B
   * Sorcery
   * Choose two -
   * * Target creature gets +3/+3 and gains flying until end of turn.
   * * Return target creature card with mana value 2 or less from your graveyard to the battlefield.
   * * Target player draws a card and loses 1 life.
   * * Target opponent sacrifices a creature of their choice. */

  typedef enum
  {
    CHOICE_INVALID_TARGET = 0,
    CHOICE_PUMP = 1,
    CHOICE_REANIMATE = 2,
    CHOICE_DRAW1_LOSE1 = 3,
    CHOICE_OPP_SAC = 4
  } CommandChoices;

  CommandChoices choice;
  int pump_available;
  int grave_available;
  int player_available;
  int sacrifice_available;
  int result;
  int sacrifice_priority;
  uint32_t pump_priority;
  uint32_t grave_priority;
  int opponent;
  card_instance_t *instance;
  target_definition_t *creature_target;
  target_definition_t *player_target;
  CommandChoices choices[2];
  target_definition_t td;
  target_definition_t td3;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    new_default_test_definition(&test, 2, "Select target creature card with MV 2 or less.");
    test.cmc = 3;
    test.cmc_flag = 3;
    default_target_definition(player, card, &td3, 0);
    td3.zone = TARGET_ZONE_PLAYERS;
    get_card_instance(player, card);

    int can_pump = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    int can_reanimate = generic_spell(player, card, EVENT_CAN_CAST, 8, NULL, NULL, 1, &test);
    int can_draw = generic_spell(player, card, EVENT_CAN_CAST, 1, &td3, NULL, 1, NULL);
    int can_sacrifice = would_validate_arbitrary_target(&td3, 1 - player, -1);
    return (can_pump != 0) + (can_reanimate != 0) + (can_draw != 0) + (can_sacrifice != 0) >= 2;
  }

  if (event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  new_default_test_definition(&test, 2, "Select target creature card with MV 2 or less.");
  test.cmc = 3;
  test.cmc_flag = 3;
  default_target_definition(player, card, &td3, 0);
  td3.zone = TARGET_ZONE_PLAYERS;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    player_target = &td3;
    creature_target = &td;
    choice = dialog_impl(player, card, event, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS, DLG_RANDOM,
                         DLG_MODE, "Pump a creature", 0, 1, "Reanimate", 0, 1,
                         "Draw & lose 1 life", 0, 1, "Opponent sacrifices", 0, 1, 0, 0, 0, 0);
    choices[0] = choice;
    int target_index = 0;
    for (int choice_index = 0; choice_index < 2; ++choice_index)
    {
      choice = choices[choice_index];
      switch (choice)
      {
      case CHOICE_INVALID_TARGET:
        break;
      case CHOICE_PUMP:
        result = validate_target(player, card, creature_target, target_index);
        if (result != 0)
        {
          pump_ability_until_eot(player, card, instance->targets[target_index].player,
                                 instance->targets[target_index].card, 3, 3, 0x20, 0);
        }
        break;
      case CHOICE_REANIMATE:
        result = validate_target_from_grave_source(player, card, player, target_index);
        if (result != -1)
        {
          reanimate_permanent(player, card, player, result, REANIMATE_DEFAULT);
        }
        break;
      case CHOICE_DRAW1_LOSE1:
        result = validate_target(player, card, player_target, target_index);
        if (result != 0)
        {
          draw_cards(instance->targets[target_index].player, 1);
          lose_life(instance->targets[target_index].player, 1);
        }
        break;
      case CHOICE_OPP_SAC:
        result = validate_target(player, card, player_target, target_index);
        if (result != 0)
        {
          player_sacrifices_a_permanent(player, card, instance->targets[target_index].player, 2,
                                        SAC_CAUSED);
        }
      default:
        continue;
      }
      ++target_index;
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  sacrifice_priority = 1;
  opponent = 1 - player;
  int draw_priority = 1;
  pump_priority = 1;
  grave_priority = 1;
  if (ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0))
  {
    draw_priority = ((5 < life[player]) ? 40U : 5U);
    sacrifice_priority = count_subtype(opponent, 2, -1);
    pump_priority = 10;
    grave_priority = 0x14;
    sacrifice_priority = ((sacrifice_priority == 0) ? 1U : 30U);
  }
  player_target = &td3;
  creature_target = &td;
  pump_available = generic_spell(player, card, EVENT_CAN_CAST, 1, creature_target, NULL, 1, NULL);
  grave_available = generic_spell(player, card, EVENT_CAN_CAST, 8, NULL, NULL, 1, &test);
  player_available = generic_spell(player, card, EVENT_CAN_CAST, 1, player_target, NULL, 1, NULL);
  sacrifice_available = would_validate_arbitrary_target(player_target, opponent, -1);
  choice =
      dialog_impl(player, card, event, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS, DLG_RANDOM, DLG_MODE,
                  "Pump a creature", pump_available, pump_priority, "Reanimate", grave_available,
                  grave_priority, "Draw & lose 1 life", player_available, draw_priority,
                  "Opponent sacrifices", sacrifice_available, sacrifice_priority, 0, 0, 0, 0);
  if (((event != EVENT_CAST_SPELL) || (card != affected_card)) ||
      (player != affected_card_controller))
  {

    return 0;
  }
  instance->number_of_targets = 0;
  int target_index = 0;
  choices[0] = choice;
  for (int choice_index = 0; choice_index < 2; ++choice_index)
  {
    choice = choices[choice_index];
    switch (choice)
    {
    case CHOICE_INVALID_TARGET:
      spell_fizzled = 1;
      break;
    case CHOICE_PUMP:
      result = new_pick_target(creature_target, "Select target creature to get +3/+3 and flying.",
                               target_index, 0x81);
      if (result == 0)
      {
        return 0;
      }
      target_index = target_index + 1;
      break;
    case CHOICE_REANIMATE:
      select_target_from_grave_source(player, card, player, SFG_0, 2, &test, target_index);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      target_index = target_index + 1;
      break;
    case CHOICE_DRAW1_LOSE1:
      if ((life[opponent] != 1) ||
          (result = would_validate_arbitrary_target(player_target, opponent, -1), result == 0))
      {
        td3.preferred_controller = player;
      }
      result =
          new_pick_target(player_target, "Select target player to draw a card and lose 1 life.",
                          target_index, 0x81);
      if (result == 0)
      {
        return 0;
      }
      target_index = target_index + 1;
      break;
    case CHOICE_OPP_SAC:
      instance->targets[target_index].player = opponent;
      instance->targets[target_index].card = -1;
      instance->number_of_targets = instance->number_of_targets + '\x01';
      target_index = target_index + 1;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23dc500; original cards/strixhaven.c:6979. */
int card_silverquill_pledgemage(int player, int card, event_t event)
{
  /* Silverquill Pledgemage	|1|WB|WB
   * Creature - Vampire Cleric 3/1
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, this creature gains your choice of flying or lifelink until end of turn. */

  magecraft_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    int priority = 1;
    if (life[player] > 6)
      priority = check_for_ability(player, card, KEYWORD_FLYING) != 0;
    int choice = do_dialog(player, player, card, -1, -1,
                           " This gains flying\n This gains lifelink", priority);
    int ability = choice == 0 ? KEYWORD_FLYING : 0;
    int sp_ability = choice == 1 ? SP_KEYWORD_LIFELINK : 0;
    pump_ability_until_eot_no_repeat_alt(player, card, player, card, ability, sp_ability,
                                        choice + 1);
  }
  return hybrid(player, card, event);
}

/* ManalinkEh 2022: 0x23dc610; original cards/strixhaven.c:6996. */
int card_silverquill_silencer(int player, int card, event_t event)
{
  /* Silverquill Silencer	|W|B
   * Creature - Human Cleric 3/2
   * As this creature enters, choose a nonland card name.
   * Whenever an opponent casts a spell with the chosen name, they lose 3 life and you draw a card. */

  test_definition_t test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    card_instance_t *instance = get_card_instance(player, card);
    new_default_test_definition(&test, 1, "Select a nonland card.");
    test.type_flag = 1;
    uint32_t data;
    if (ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0))
      data = ai_name_a_random_csvid(player, card, 1 - player, TYPE_NONE, &test);
    else
      data = new_card_from_list(player, 3, &test);
    if ((int)data >= 0)
    {
      set_inst_info(instance, 0x5916, data, 0);
      create_card_name_legacy(player, card, data);
    }
  }
  if (trigger_condition == TRIGGER_SPELL_CAST && card == affected_card && player == affected_card_controller)
  {
    default_test_definition(&test, 0xff);
    card_instance_t *instance = get_card_instance(player, card);
    inst_info_t named = inst_info(instance, 0x5916);
    test.id = named.data;
    sp_ability_trigger(1 - player, event, 2, TYPE_ANY, &test, player, card);
  }
  if (event == EVENT_SPA)
  {
    lose_life(1 - player, 3);
    draw_a_card(player);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23dc7f0; original cards/strixhaven.c:7043. */
int card_solve_the_equation(int player, int card, event_t event)
{
  /* Solve the Equation	|2|U
   * Sorcery
   * Search your library for an instant or sorcery card, reveal it, put it into your hand, then shuffle. */

  int result;
  test_definition_t this_test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    new_default_test_definition(&this_test, 0x38, "Select an instant or sorcery card.");
    new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23dc8a0; original cards/strixhaven.c:7064. */
int card_sparring_regimen(int player, int card, event_t event)
{
  /* Sparring Regimen	|2|W
   * Enchantment
   * When this enchantment enters, learn. (You may reveal a Lesson card you own from outside the game and put it into your hand, or discard a card to draw a card.)
   * Whenever you attack, put a +1/+1 counter on target attacking creature and untap it. */

  int result;
  card_instance_t *instance;
  target_definition_t td1;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    learn(player, card);
    store_attackers(player, card, EVENT_ETB, 0, player, -1, NULL);
    dat_ability(player, card, EVENT_ETB, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  }
  else
  {
    store_attackers(player, card, event, 0, player, -1, NULL);
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {

      default_target_definition(player, card, &td1, 2);
      td1.required_state = TARGET_STATE_ATTACKING;
      td1.allow_cancel = 0;
      instance = get_card_instance(player, card);
      result = can_target(&td1);
      if (result != 0)
      {
        result = new_pick_target(&td1, "Select target attacking creature.", 0, 0x80);
        if (result != 0)
        {
          add_counters(player, card, instance->targets[0].player,
                                   instance->targets[0].card, COUNTER_P1_P1, 1);
          untap_card(instance->targets[0].player, instance->targets[0].card);
        }
      }
      instance->number_of_targets = '\0';
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23dca70; original cards/strixhaven.c:7095. */
int card_spectacle_mage(int player, int card, event_t event)
{
  /* Spectacle Mage	|1|U|R
   * Creature - Bird Shaman 2/2
   * Flying
   * Instant and sorcery spells you cast with mana value 5 or greater cost |1 less to cast. */

  card_instance_t *instance;
  int result;

  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if ((result == 0) && (affected_card_controller == player))
      {
        result = is_what(player, affected_card, 0x38);
        if (result != 0)
        {
          result = get_cmc(affected_card_controller, affected_card);
          if (4 < result)
          {
            COST_COLORLESS = COST_COLORLESS + -1;
          }
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23dcb20; original cards/strixhaven.c:7116. */
int card_spell_satchel(int player, int card, event_t event)
{
  /* Spell Satchel	|2
   * Artifact
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, put a book counter on this artifact.
   * |T, Remove a book counter from this artifact: Add |C.
   * |3, |T, Remove three book counters from this artifact: Draw a card. */

  card_instance_t *instance;
  int result;

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    add_counters(player, card, player, card, COUNTER_BOOK, 1);

    return mana_producer(player, card, event);
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = count_counters(player, card, COUNTER_BOOK);
      if ((result == 0) || (result = mana_producer(player, card, EVENT_CAN_ACTIVATE), result == 0))
      {
        result = count_counters(player, card, COUNTER_BOOK);
        if (result < 3)
        {
          return 0;
        }
        result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 4, 0, 0, 0, 0, 0, 0,
                                           NULL, NULL);
        if (result == 0)
        {
          return 0;
        }
      }
      return 1;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return mana_producer(player, card, event);
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_ACTIVATE)
  {
    instance->mode = '\0';
    result = paying_mana();
    if ((((result != 0) || (result = count_counters(player, card, COUNTER_BOOK), result == 0)) ||
         (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 4, 0, 0, 0, 0, 0,
                                             0, NULL, NULL),
          result == 0)) ||
        (result =
             do_dialog(player, player, card, -1, -1, " Produce mana\n Draw a card\n Cancel", 1),
         result == 0))
    {
      remove_counters(player, card, player, card, COUNTER_BOOK, 1);
      event = EVENT_ACTIVATE;
      return mana_producer(player, card, event);
    }
    if (result == 1)
    {
      add_state(player, card, 0x10);
      generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        remove_state(player, card, 0x10);
      }
      else
      {
        remove_counters(player, card, player, card, COUNTER_BOOK, 3);
        result = hand_count[player];
        instance->mode = '\x01';
        ai_modifier = ai_modifier + result * -10 + 0x46;
      }
    }
    else if (result == 2)
    {
      spell_fizzled = 1;
    }
  }
  else if ((event == EVENT_RESOLVE_ACTIVATION) && (instance->mode == '\x01'))
  {
    draw_a_card(player);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x23dcf00; original cards/strixhaven.c:7182. */
int card_spirit_summoning(int player, int card, event_t event)
{
  /* Spirit Summoning	|1|RW|RW
   * Sorcery - Lesson
   * Create a 3/2 red and white Spirit creature token. */

  int result;

  hybrid(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {
    make_one_spirit_strixhaven(player, card, 1);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23dcf60; original cards/strixhaven.c:7230. */
int card_stonebound_mentor(int player, int card, event_t event)
{
  /* Stonebound Mentor	|1|R|W
   * Creature - Spirit Advisor 3/3
   * Whenever one or more cards leave your graveyard, scry 1. */

  card_instance_t *instance;

  if (event == EVENT_REMOVED_FROM_GRAVEYARD)
  {
    if (affected_card_controller == player)
    {
      instance = get_card_instance(player, card);
      if (instance->info_slot != 0x42)
      {
        scry(player, 1);
        instance = get_card_instance(player, card);
        instance->info_slot = 0x42;
      }
    }
  }
  else if (event == EVENT_STATIC_EFFECTS)
  {
    instance = get_card_instance(player, card);
    if (instance->info_slot == 0x42)
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23dd010; original cards/strixhaven.c:7253. */
int card_storm_kiln_artist(int player, int card, event_t event)
{
  /* Storm-Kiln Artist	|3|R
   * Creature - Dwarf Shaman 2/2
   * This creature gets +1/+0 for each artifact you control.
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, create a Treasure token. (It's an artifact with "|T, Sacrifice this token: Add one mana of any color.") */

  int result;

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    if (((event == EVENT_POWER) && (card == affected_card)) && (player == affected_card_controller))
    {
      result = count_permanents_by_type(player, TYPE_ARTIFACT);
      event_result = event_result + result;
    }
    return 0;
  }
  generate_token_by_id(player, card, CARD_ID_TREASURE);
  return 0;
}

/* ManalinkEh 2022: 0x23dd0c0; original cards/strixhaven.c:7277. */
int card_strixhaven_stadium(int player, int card, event_t event)
{
  /* Strixhaven Stadium	|3
   * Artifact
   * |T: Add |C. Put a point counter on this artifact.
   * Whenever a creature deals combat damage to you, remove a point counter from this artifact.
   * Whenever a creature you control deals combat damage to an opponent, put a point counter on this artifact. Then if it has ten or more point counters on it, remove them all and that player loses the game. */

  unsigned int result = damage_dealt_to_me_arbitrary(
      2, -1, event, DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE, player, card);
  if (result)
  {
    int times_damaged[2] = { result & 0xff, (result >> 8) & 0xff };
    for (int p = 0; p != 2; ++p)
    {
      unsigned int num = times_damaged[p];
      if (num)
      {
        if (1 - player == p)
        {
          add_counters(player, card, player, card, COUNTER_POINT, num);
          if (count_counters(player, card, COUNTER_POINT) > 9)
            lose_the_game(1 - player);
        }
        if (player == p)
          remove_counters(player, card, player, card, COUNTER_POINT, num);
      }
    }
  }
  if (event == EVENT_ACTIVATE)
  {
    add_counters(player, card, player, card, COUNTER_POINT, 1);
    ai_modifier += 0x100;
  }
  return mana_producer(player, card, event);
}

/* ManalinkEh 2022: 0x23dd240; original cards/strixhaven.c:7324. */
int card_symmetry_sage(int player, int card, event_t event)
{
  /* Symmetry Sage	|U
   * Creature - Human Wizard 0/2
   * Flying
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, target creature you control has base power 2 until end of turn. */

  card_instance_t *instance;
  int target_player;
  int power;
  int opponent_power;
  target_definition_t td;

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {

    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    if (can_target(&td) != 0)
    {
      if (new_pick_target(&td, "TARGET_CREATURE", 0, 0) != 0)
      {
        set_pt_and_abilities_until_eot(player, card, instance->targets[0].player,
                                       instance->targets[0].card, 2, -1, 0, 0, 8);
        target_player = instance->targets[0].player;
        if (target_player == player)
        {

          power = get_power(target_player, instance->targets[0].card);
          target_player = instance->targets[0].player;
          ai_modifier = ai_modifier + (unsigned int)((power < 2) ? 0 : -96) + 0x30;
        }
        if (1 - player == target_player)
        {

          opponent_power = get_power(target_player, instance->targets[0].card);
          ai_modifier = ai_modifier + (unsigned int)((2 < opponent_power) ? 0 : -96) + 0x30;
        }
        instance->number_of_targets = '\0';
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23dd3c0; original cards/strixhaven.c:7355. */
int card_tanazir_quandrix(int player, int card, event_t event)
{
  /* Tanazir Quandrix	|3|G|U
   * Legendary Creature - Elder Dragon 4/4
   * Flying, trample
   * When ~ enters, double the number of +1/+1 counters on target creature you control.
   * Whenever ~ attacks, you may have the base power and toughness of other creatures you control become equal to ~'s power and toughness until end of turn. */

  int count_or_choice;
  card_instance_t *instance;
  int unused_info;

  int mc;
  int marked[151];
  target_definition_t td;
  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
    if (new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0) != 0)
    {
      count_or_choice =
          count_counters(instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1);
      if (count_or_choice == 0)
      {
        ai_modifier = ai_modifier + -0x60;
      }
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                   COUNTER_P1_P1, count_or_choice);
    }
    dat_ability(player, card, EVENT_ETB, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
    return 0;
  }
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);

  if (((event == EVENT_DAT_ABILITY) &&
       (count_or_choice =
            dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
                        "Set P/T of your creatures", 1, 10, "Decline", 1, 5, 0, 0, 0, 0),
        count_or_choice == 1)) &&
      (count_or_choice = active_cards_count[player] + -1, -1 < count_or_choice))
  {
    mc = 0;
    do
    {
      instance = in_play(player, count_or_choice);
      if (instance != (card_instance_t *)0 && is_what(player, count_or_choice, 2))
        marked[mc++] = count_or_choice;
      --count_or_choice;
    } while (count_or_choice != -1);
    if (mc > 0)
    {
      int *cursor = marked + mc - 1;
      do
      {
        int toughness = get_toughness(player, card);
        int power = get_power(player, card);
        set_pt_and_abilities_until_eot(player, card, player, *cursor, power, toughness, 0, 0, 8);
        if (cursor == marked)
          break;
        --cursor;
      } while (1);
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x23dd6a0; original cards/strixhaven.c:7412. */
int card_teach_by_example(int player, int card, event_t event)
{
  /* Teach by Example	|UR|UR
   * Instant
   * (|UR can be paid with either |U or |R.)
   * When you next cast an instant or sorcery spell this turn, copy that spell. You may choose new targets for the copy. */

  if (event == EVENT_CAN_CAST)
    return basic_spell(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
    return card_doublecast(player, card, event);
  return hybrid(player, card, event);
}

/* ManalinkEh 2022: 0x23dd6d0; original cards/strixhaven.c:7432. */
int card_team_pennant(int player, int card, event_t event)
{
  /* Team Pennant	|1
   * Artifact - Equipment
   * Equipped creature gets +1/+1 and has vigilance and trample.
   * Equip creature token |1
   * Equip |3 */

  card_instance_t *instance;
  int result;
  unsigned int token_equip_available;
  unsigned int normal_equip_available;
  target_definition_t *equip_target;
  const char *prompt;
  target_definition_t *token_target;
  target_definition_t *normal_target;
  target_definition_t td;
  target_definition_t td1;

  attached_gets_pt(player, card, event, 1, 1);
  attached_gets_kw(player, card, event, KEYWORD_TRAMPLE, SP_KEYWORD_VIGILANCE);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    equip_target = &td;
    default_target_definition(player, card, equip_target, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    default_target_definition(player, card, &td1, 2);
    td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td1.extra = (int32_t)target_is_token_too;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x100, 3, 0, 0, 0, 0, 0,
                                         0, equip_target, NULL);
      if (result == 0)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x100, 1, 0, 0, 0, 0, 0,
                                         0, &td1, NULL);
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 3, 0, 0, 0, 0, 0, 0,
                                         equip_target, NULL);
      normal_equip_available = 0;
      if (result != 0)
      {
        result = can_sorcery_be_played(player, EVENT_ACTIVATE);
        normal_equip_available = (unsigned int)(result != 0);
      }
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 1, 0, 0, 0, 0, 0, 0,
                                         &td1, NULL);
      token_equip_available = 0;
      if (result != 0)
      {
        result = can_sorcery_be_played(player, EVENT_ACTIVATE);
        token_equip_available = (unsigned int)(result != 0);
      }
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_MODE,
                           "Normal equip", normal_equip_available, 1, "Token equip",
                           token_equip_available, 5, 0, 0, 0, 0);
      if (result != 0)
      {
        if (result == 1)
        {
          prompt = "Select target creature you control.";
          result = 3;
        }
        else
        {
          if (result != 2)
          {
            return 0;
          }
          prompt = "Select target creature token you control.";
          result = 1;
          equip_target = &td1;
        }
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 0x102, result, 0, 0, 0, 0, 0,
                                         0, equip_target, prompt);
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
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    default_target_definition(player, card, &td1, 2);
    td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td1.extra = (int32_t)target_is_token_too;
    instance = get_card_instance(player, card);
  }
  normal_target = &td;
  token_target = &td1;
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }

  unsigned char mode = instance->mode;
  bool valid = false;
  if (mode == 1)
  {
    valid = validate_target(player, card, normal_target, 0) != 0;
    if (!valid)
      mode = instance->mode;
  }
  if (!valid)
  {
    if (mode != 2 || !validate_target(player, card, token_target, 0))
      return 0;
  }
  equip_target_creature(instance->parent_controller, instance->parent_card,
                        instance->targets[0].player, instance->targets[0].card);
  return 0;
}

/* ManalinkEh 2022: 0x23ddc60; original cards/strixhaven.c:7501. */
int card_tend_the_pests(int player, int card, event_t event)
{
  /* Tend the Pests	|B|G
   * Instant
   * As an additional cost to cast this spell, sacrifice a creature.
   * Create X 1/1 black and green Pest creature tokens with "When this token dies, you gain 1 life," where X is the sacrificed creature's power. */

  card_instance_t *instance;

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
        return card_carrion(player, card, event);
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      instance = get_card_instance(player, card);
      make_pest(player, card, instance->info_slot);
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
    return 0;
  }
  return card_carrion(player, card, event);
}

/* ManalinkEh 2022: 0x23ddcf0; original cards/strixhaven.c:7520. */
int card_tenured_inkcaster(int player, int card, event_t event)
{
  /* Tenured Inkcaster	|4|B
   * Creature - Vampire Warlock 2/2
   * When this creature enters, put a +1/+1 counter on target creature.
   * Whenever a creature you control with a +1/+1 counter on it attacks, each opponent loses 1 life and you gain 1 life. */

  test_definition_t test;
  card_instance_t *instance;
  int result;


  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE", 0, 0);
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_P1_P1, 1);
      }
    }
    instance->number_of_targets = '\0';
    dat_ability(player, card, EVENT_ETB, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    return 0;
  }
  if (event != EVENT_DECLARE_ATTACKERS)
  {
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      instance = get_card_instance(player, card);
      result = instance->info_slot + -1;
      if (-1 < result)
      {
        do
        {
          lose_life(1 - player, 1);
          result = result + -1;
          gain_life(player, 1);
        } while (result != -1);
        return 0;
      }
    }
    return 0;
  }
  new_default_test_definition(&test, 2, "");
  test.power = 0;
  test.power_flag = F5_POWER_1_1_COUNTERS_GREATER_THAN_VALUE;
  store_attackers(player, card, EVENT_DECLARE_ATTACKERS, 0, player, -1, &test);
  dat_ability(player, card, EVENT_DECLARE_ATTACKERS, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  return 0;
}

/* ManalinkEh 2022: 0x23ddf40; original cards/strixhaven.c:7567. */
int card_test_of_talents(int player, int card, event_t event)
{
  /* Test of Talents	|1|U
   * Instant
   * Counter target instant or sorcery spell. Search its controller's graveyard, hand, and library for any number of cards with the same name as that spell and exile them. That player shuffles, then draws a card for each card exiled from their hand this way. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  counterspell_target_definition(player, card, &td, 0x38);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {

      result = get_id(instance->targets[0].player, instance->targets[0].card);
      real_counter_a_spell(player, card, instance->targets[0].player,
                           instance->targets[0].card);
      result = lobotomy_effect(player, instance->targets[0].player, result, -1, 0);
      draw_cards(player, result);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 2, &td, NULL, 1, NULL);
}

/* ManalinkEh 2022: 0x23de0e0; original cards/strixhaven.c:7596. */
int card_the_biblioplex(int player, int card, event_t event)
{
  /* The Biblioplex	""
   * Land
   * |T: Add |C.
   * |2, |T: Look at the top card of your library. If it's an instant or sorcery card, you may reveal it and put it into your hand. If you don't put the card into your hand, you may put it into your graveyard. Activate only if you have exactly zero or seven cards in hand. */

  card_instance_t *instance;
  int mana_or_available;
  bool other_turn;
  char buffer[100];

  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if ((hand_count[player] == 7) || (hand_count[player] == 0))
    {
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 3, 0, 0, 0, 0, 0, 0, NULL,
                                    NULL) != 0)
      {
        return 1;
      }
    }
    return mana_producer(player, card, EVENT_CAN_ACTIVATE);
  }
  if (event == EVENT_ACTIVATE)
  {

    instance->mode = '\0';
    mana_or_available = paying_mana();
    if (((mana_or_available != 0) ||
         (((hand_count[player] != 7 && (hand_count[player] != 0)) ||
           (mana_or_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 3, 0,
                                                          0, 0, 0, 0, 0, NULL, NULL),
            mana_or_available == 0)))) ||
        (mana_or_available = do_dialog(player, player, card, -1, -1,
                                       " Produce mana\n Look at top card\n Cancel", 1),
         mana_or_available == 0))
    {
      return mana_producer(player, card, EVENT_ACTIVATE);
    }
    if (mana_or_available == 1)
    {
      add_state(player, card, 0x10);
      generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        remove_state(player, card, 0x10);
      }
      else
      {
        other_turn = current_turn != player;
        instance->mode = '\x01';
        ai_modifier = ai_modifier + (other_turn - 1 & 0xffffffa0) + 0x30;
      }
    }
    else
    {
      spell_fizzled = 1;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->mode == '\0')
    {
      return mana_producer(player, card, EVENT_RESOLVE_ACTIVATION);
    }

    mana_or_available = *deck_ptr[player];
    if (mana_or_available != -1)
    {
      if (is_what(-1, mana_or_available, 0x38) != 0)
      {

        scnprintf(buffer, 100, " %s to hand\n Decline",
                  cards_ptr[cards_data[mana_or_available].id]->name);
        if (do_dialog(player, player, card, -1, -1, buffer, 0) == 0)
        {
          reveal_card_iid(player, card, mana_or_available);
          add_card_to_hand(player, mana_or_available);
          remove_card_from_deck(player, 0);
          return 0;
        }
      }
      scnprintf(buffer, 100, " %s to grave\n Leave it on top of deck",
                cards_ptr[cards_data[mana_or_available].id]->name);
      mana_or_available = internal_rand(0x32);
      if (do_dialog(player, player, card, -1, -1, buffer,
                    (unsigned int)(0x19 < mana_or_available)) == 0)
      {
        mill(player, 1);
      }
    }
  }
  else if (event != EVENT_CLEANUP)
  {
    return mana_producer(player, card, event);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23de590; original cards/strixhaven.c:7711. */
int card_umbral_juke(int player, int card, event_t event)
{
  /* Umbral Juke	|2|B
   * Instant
   * Choose one -
   * * Target player sacrifices a creature or planeswalker of their choice.
   * * Create a 2/1 white and black Inkling creature token with flying. */

  unsigned char mode_flags;
  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    get_card_instance(player, card);
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0);
      td.zone = TARGET_ZONE_PLAYERS;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        result = check_special_flags2(player, card, -0x80000000);
        if (result == 0)
        {

          instance->mode = '\0';
          result = can_target(&td);
          result = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_MODE,
                               "Target player sac a creature or a planeswalker", result, 10,
                               "Create a 2/1 Inkling", 1, 5, 0, 0, 0, 0);
          if (result == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
        }
        if ((instance->mode & 1) != 0)
        {
          new_pick_target(&td, "TARGET_PLAYER", 0, 1);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      mode_flags = instance->mode;
      if ((mode_flags & 1) != 0)
      {
        result = validate_target(player, card, &td, 0);
        if (result != 0)
        {
          player_sacrifices_a_permanent(player, card, instance->targets[0].player, 0x102,
                                        SAC_CAUSED);
        }
        mode_flags = instance->mode;
      }
      if ((mode_flags & 2) != 0)
      {
        make_inkling(player, card, 1, 0);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    get_card_instance(player, card);
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23c8860; original cards/strixhaven.c:7771. */
int uvilda_legacy(int player, int card, event_t event)
{

  int counter_or_iid;
  card_instance_t *instance;
  int exiled;
  unsigned int csvid;

  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    remove_counters(player, card, player, card, COUNTER_HONE, 1);
    return 0;
  }
  if ((event == EVENT_STATIC_EFFECTS) &&
      (counter_or_iid = count_counters(player, card, COUNTER_HONE), counter_or_iid == 0))
  {

    instance = get_card_instance(player, card);
    counter_or_iid = instance->targets[1].player;
    if (counter_or_iid != -1)
    {
      csvid = (unsigned int)cards_data[counter_or_iid].id;
      exiled = check_rfg(player, csvid);
      if ((exiled != 0) &&
          (counter_or_iid = can_legally_play_iid(player, counter_or_iid), counter_or_iid != 0))
      {
        if (charge_mana_from_id_with_cost_mod(player, card, EVENT_STATIC_EFFECTS, csvid, -4) == 0)
        {
          return 0;
        }
        play_card_in_exile_for_free(player, player, csvid);
        return 0;
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23de890; original cards/strixhaven.c:7793. */
int card_uvilda_dean_of_perfection(int player, int card, event_t event)
{
  /* Uvilda, Dean of Perfection	|2|U
   * Legendary Creature - Djinn Wizard 2/2
   * |T: You may exile an instant or sorcery card from your hand and put three refine counters on it. It gains "At the beginning of your upkeep, if this card is exiled, remove a refine counter from it" and "When the last refine counter is removed from this card, if it's exiled, you may cast it. It costs |4 less to cast this way." */

  int hand_card;
  int iid;
  card_instance_t *instance;
  test_definition_t test;

  mdfc_front(player, card, event);
  check_legend_rule(player, card, event);
  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      new_default_test_definition(&test, 0x38, "Select an instant or sorcery to exile.");
      hand_card = new_select_a_card(player, player, 4, 0, 2, -1, &test);
      if (hand_card == -1)
      {
        return 0;
      }
      iid = get_original_internal_card_id(player, hand_card);
      if (iid == -1)
      {
        return 0;
      }
      rfg_card_in_hand(player, hand_card);
      hand_card = create_legacy_effect(player, card, uvilda_legacy);
      add_counters(player, card, player, hand_card, COUNTER_HONE, 3);
      instance = get_card_instance(player, hand_card);
      instance->targets[1].player = iid;
    }
  }
  return generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x23dea60; original cards/strixhaven.c:7821. */
int card_nassari_dean_of_expression(int player, int card, event_t event)
{
  /* Nassari, Dean of Expression	|3|R|R
   * Legendary Creature - Efreet Shaman 4/4
   * At the beginning of your upkeep, exile the top card of each opponent's library. Until end of turn, you may cast spells from among those exiled cards, and you may spend mana as though it were mana of any color to cast those spells.
   * Whenever you cast a spell from exile, put a +1/+1 counter on Nassari. */

  mdfc_back(player, card, event);
  check_legend_rule(player, card, event);
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    test_definition_t test;
    default_test_definition(&test, 0x7e);
    exile_top_x_card_you_may_play_them(player, card, 1, &test, 1 - player,
                                       MPCFE_UNTIL_EOT | MPCFE_FOR_CMC);
  }
  if (trigger_condition == TRIGGER_SPELL_CAST && card == affected_card && player == affected_card_controller &&
      check_special_flags(trigger_cause_controller, trigger_cause, -0x80000000))
    sp_ability_trigger(player, event, 2,
                       TYPE_ARTIFACT | TYPE_SPELL | TYPE_ENCHANTMENT | TYPE_CREATURE, NULL, player,
                       card);
  if (event == EVENT_SPA)
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  return 0;
}

/* ManalinkEh 2022: 0x23c7140; original cards/strixhaven.c:7853. */
static int valentin_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (event == EVENT_STATIC_EFFECTS && card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player >= 0 &&
        get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_KALITAS_TRAITOR_OF_GHET)
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
      test_definition_t test;
      default_test_definition(&test, 2);
      int opponent = 1 - player;
      test.type_flag = 3;
      count_for_gfp_ability_and_store_values(player, card, 0x77, opponent, 0, &test, 0x40, 0);
      if (!is_token(affected_card_controller, affected_card) &&
          is_this_dying(affected_card_controller, affected_card) &&
          opponent == affected_card_controller && is_what(opponent, affected_card, 2))
      {
        dying = get_card_instance(affected_card_controller, affected_card);
        dying->kill_code = 4;
        set_special_flags2(affected_card_controller, affected_card, 0x10000000);
      }
    }
  }
  unsigned int deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (!check_status(player, card, 0x10000))
  {
    deaths &= 0xff;
    if (event == EVENT_GRAVEYARD_FROM_PLAY && deaths == 0)
      return 0;
    while (deaths != 0)
    {
      if (!has_mana(player, 0, 2) || !charge_mana(player, COLOR_COLORLESS, 2))
        break;
      make_pest(player, card, 1);
      --deaths;
    }
  }
  if (event == EVENT_CLEANUP && !check_status(player, card, 0x10000))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x23debd0; original cards/strixhaven.c:7915. */
int card_valentin_dean_of_the_vein(int player, int card, event_t event)
{
  /* Valentin, Dean of the Vein	|B
   * Legendary Creature - Vampire Warlock 1/1
   * Menace, lifelink
   * If a nontoken creature an opponent controls would die, exile it instead. When you do, you may pay |2. If you do, create a 1/1 black and green Pest creature token with "When this token dies, you gain 1 life." */

  unsigned int deaths;
  card_instance_t *instance;
  int index_or_result;
  int opponent_or_type;
  test_definition_t test;

  mdfc_front(player, card, event);
  check_legend_rule(player, card, event);
  menace(player, card, event);
  lifelink(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      bool found = false;
      if (0 < active_cards_count[player])
      {
        index_or_result = 0;
        do
        {
          instance = in_play(player, index_or_result);

          if ((((instance != NULL) && (opponent_or_type = is_what(player, index_or_result, 0x80),
                                       opponent_or_type != 0)) &&
               (instance = get_card_instance(player, index_or_result),
                instance->info_slot_fn == valentin_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            found = true;
            break;
          }
          index_or_result = index_or_result + 1;
        } while (index_or_result < active_cards_count[player]);
      }

      if (!found)
      {
        index_or_result =
            create_targetted_legacy_effect(player, card, valentin_effect, player, card);
        instance = get_card_instance(player, index_or_result);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, index_or_result, 0x10000);
      }
    }
  }
  else if ((event == EVENT_GRAVEYARD_FROM_PLAY) &&
           (instance = in_play(affected_card_controller, affected_card), instance != NULL))
  {
    default_test_definition(&test, 2);
    opponent_or_type = 1 - player;
    test.type_flag = 3;
    count_for_gfp_ability_and_store_values(player, card, 0x77, opponent_or_type, 0, &test, 0x40, 0);
    index_or_result = is_token(affected_card_controller, affected_card);
    if ((index_or_result == 0) &&
        (((index_or_result = is_this_dying(affected_card_controller, affected_card),
           index_or_result != 0 && (opponent_or_type == affected_card_controller)) &&
          (index_or_result = is_what(opponent_or_type, affected_card, 2), index_or_result != 0))))
    {
      instance = get_card_instance(affected_card_controller, affected_card);
      instance->kill_code = '\x04';
      set_special_flags2(affected_card_controller, affected_card, 0x10000000);
    }
  }
  deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  instance = get_card_instance(player, card);
  if ((instance->kill_code == '\0') && (deaths != 0))
  {

    deaths = deaths & 0xff;
    while (((deaths != 0 && (index_or_result = has_mana(player, 0, 2), index_or_result != 0)) &&
            (index_or_result = charge_mana(player, COLOR_COLORLESS, 2), index_or_result != 0)))
    {
      make_pest(player, card, 1);
      deaths = deaths - 1;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23def30; original cards/strixhaven.c:7981. */
int card_lisette_dean_of_the_root(int player, int card, event_t event)
{
  /* Lisette, Dean of the Root	|2|G|G
   * Legendary Creature - Human Druid 4/4
   * Whenever you gain life, you may pay |1. If you do, put a +1/+1 counter on each creature you control and those creatures gain trample until end of turn. */

  int result;
  card_instance_t *instance;

  mdfc_back(player, card, event);
  check_legend_rule(player, card, event);
  result = trigger_gain_life(player, card, event, player, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = has_mana(player, 0, 1);
      if (result != 0)
      {
        result = charge_mana(player, COLOR_COLORLESS, 1);
        if (result != 0)
        {
          manipulate_type(player, card, player, TYPE_CREATURE,
                          ACT_ADD_COUNTERS_BASE | ACT_KILL_BURY);
          pump_creatures_until_eot(player, card, player, 0, 0, 0, 0x80, 0, NULL);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23c85d0; original cards/strixhaven.c:8002. */
static char *is_monocolored(int who_chooses, int player, int card, int targeting_player,
                            int targeting_card)
{
  return count_colors(player, card) == 1 ? NULL : "must be monocolored.";
}

/* ManalinkEh 2022: 0x23df040; original cards/strixhaven.c:8007. */
int card_vanishing_verse(int player, int card, event_t event)
{
  /* Vanishing Verse	|W|B
   * Instant
   * Exile target monocolored permanent. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (int32_t)is_monocolored;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td, "Select target monocolored permanent", 1, NULL);
}

/* ManalinkEh 2022: 0x23df1b0; original cards/strixhaven.c:8032. */
int card_velomachus_lorehold(int player, int card, event_t event)
{
  /* Velomachus Lorehold	|5|R|W
   * Legendary Creature - Elder Dragon 5/5
   * Flying, vigilance, haste
   * Whenever ~ attacks, look at the top seven cards of your library. You may cast an instant or sorcery spell with mana value less than or equal to ~'s power from among them without paying its mana cost. Put the rest on the bottom of your library in a random order. */

  check_legend_rule(player, card, event);
  special_abilities(player, card, event, 0xc, player, card);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    int pow = get_power(player, card);
    int max = count_deck(player);
    if (max != 0)
    {
      if (max > 7)
        max = 7;
      test_definition_t test;
      default_test_definition(&test, 0x38);
      test.cmc = pow + 1;
      test.cmc_flag = 3;
      scnprintf(test.message, sizeof(test.message),
                "Choose an instant or sorcery card with MV %d or less", pow);
      test.create_minideck = (unsigned short)max;
      int result = new_select_a_card(player, player, 1, 0, 1, -1, &test);
      if (result >= 0)
      {
        --max;
        play_card_in_deck_for_free(player, player, result);
      }
      put_top_x_on_bottom_in_random_order(player, max);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23df310; original cards/strixhaven.c:8068. */
int card_venerable_warsinger(int player, int card, event_t event)
{
  /* Venerable Warsinger	|1|R|W
   * Creature - Spirit Cleric 3/3
   * Vigilance, trample
   * Whenever this creature deals combat damage to a player, you may return target creature card with mana value X or less from your graveyard to the battlefield, where X is the amount of damage this creature dealt to that player. */

  int result;
  card_instance_t *instance;
  test_definition_t test;
  vigilance(player, card, event);
  result = damage_dealt_by_me(player, card, event,
                              DDBM_MUST_DAMAGE_PLAYER | DDBM_REPORT_DAMAGE_DEALT |
                                  DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_TRIGGER_OPTIONAL);
  if (result == 0)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  result = instance->targets[0x10].player;
  instance = get_card_instance(player, card);
  instance->targets[0x10].player = 0;
  default_test_definition(&test, 2);
  test.cmc = result + 1;
  test.cmc_flag = 3;
  scnprintf(test.message, 100, "Choose a creature card with MV %d or less", result);
  new_global_tutor(player, player, 2, 4, 0, 3, &test);
  return 0;
}

/* ManalinkEh 2022: 0x23df410; original cards/strixhaven.c:8092. */
int card_verdant_mastery(int player, int card, event_t event)
{
  /* Verdant Mastery	|5|G
   * Sorcery
   * You may pay |3|G rather than pay this spell's mana cost.
   * Search your library for up to four basic land cards and reveal them. Put one of them onto the battlefield tapped under an opponent's control if the |3|G cost was paid. Put two of them onto the battlefield tapped under your control and the rest into your hand. Then shuffle. */

  int i;
  int choice;
  card_instance_t *instance;
  int token;
  int chosen[4];
  test_definition_t test;
  test_definition_t test2;

  if (event == EVENT_MODIFY_COST)
  {
    if (has_mana_multi(player, 3, 0, 0, 1, 0, 0) != 0)
    {
      null_casting_cost(player, card);
      instance = get_card_instance(player, card);
      instance->mode = '\x01';
      return 0;
    }
    instance = get_card_instance(player, card);
    instance->mode = '\0';
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
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->targets[1].player = 0;
        instance = get_card_instance(player, card);
        if ((instance->mode == '\x01') && (token = is_token(player, card), token == 0))
        {

          choice =
              dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_MODE, DLG_AUTOCHOOSE_IF_1,
                          "Pay alternate mana cost", 1, 0x14, "Pay full mana cost",
                          has_mana_to_cast_id(player, EVENT_CAST_SPELL, 0x5947), 10, 0, 0, 0, 0);
          if (choice == 0)
          {
            spell_fizzled = 1;
          }
          else if (choice == 1)
          {
            charge_mana_multi(player, 3, 0, 0, 1, 0, 0);
          }
          else if (choice == 2)
          {
            charge_mana_from_id(player, -1, EVENT_CAST_SPELL, 0x5947);
          }
          if (spell_fizzled != 0)
          {
            return 0;
          }
        }
      }
      return basic_spell(player, card, event);
    }
    if (((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_RESOLVE_SPELL)) &&
        (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  instance = get_card_instance(player, card);
  if (event != EVENT_RESOLVE_SPELL)
    return basic_spell(player, card, event);

  chosen[0] = -1;
  chosen[1] = -1;
  chosen[2] = -1;
  chosen[3] = -1;
  default_test_definition(&test, 1);
  test.subtype = 0x2000;
  test.no_shuffle = 1;
  if (ai_is_speculating != 1)
  {
    scnprintf(test.message, 100, "Select four basic land cards: #%d", 1);
  }

  i = 0;
  bool stopped_early = false;
  do
  {
    int selected = new_select_a_card(player, player, 1, 0, 1, -1, &test);
    if (selected == -1)
    {
      stopped_early = true;
      break;
    }
    chosen[i] = deck_ptr[player][selected];
    ++i;
    remove_card_from_deck(player, selected);
  } while (i != 4);
  shuffle(player);
  bool give_opponent_land = instance->mode == 1 && (!stopped_early || i != 0);
  if (give_opponent_land)
  {
    new_default_test_definition(&test2, 0xff,
                                "Select a land card to put into play under opponent\'s control.");
    int selected = select_card_from_zone(player, player, chosen, i, 1, 1, -1, &test2);
    if (selected >= 0)
    {
      int opponent = 1 - player;
      int land = add_card_to_hand(opponent, chosen[selected]);
      add_state(opponent, land, 0x10);
      if (player == 0)
        add_state(1, land, 0x1000);
      else
        remove_state(opponent, land, 0x1000);
      put_into_play(opponent, land);
      if (selected < i)
        for (int *cursor = chosen + selected; cursor != chosen + i; ++cursor)
          *cursor = cursor[1];
      --i;
      chosen[i] = -1;
    }
  }
  new_default_test_definition(&test, 1, "Select a land card to put into play.");
  int battlefield_count = give_opponent_land || stopped_early ? (i > 2 ? 2 : i) : 2;
  if (i > 0 || (!give_opponent_land && !stopped_early))
  {
    for (int attempt = 0; attempt < battlefield_count; ++attempt)
    {
      int selected = select_card_from_zone(player, player, chosen, i, 1, 1, -1, &test);
      if (selected < 0)
        continue;
      int land = add_card_to_hand(player, chosen[selected]);
      add_state(player, land, 0x10);
      put_into_play(player, land);
      if (selected < 3)
        for (int *cursor = chosen + selected; cursor != chosen + 3; ++cursor)
          *cursor = cursor[1];
      --i;
      chosen[i] = -1;
    }
    for (int *cursor = chosen; cursor != chosen + i; ++cursor)
      if (*cursor != -1)
        add_card_to_hand(player, *cursor);
  }
  kill_card(player, card, KILL_DESTROY);
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23dfaf0; original cards/strixhaven.c:8208. */
int card_vineglimmer_snarl(int player, int card, event_t event)
{
  /* Vineglimmer Snarl	""
   * Land
   * As this land enters, you may reveal a Forest or Island card from your hand. If you don't, this land enters tapped.
   * |T: Add |G or |U. */

  int result;

  soi_need_subtype_land(player, card, event, SUBTYPE_ISLAND, SUBTYPE_FOREST);
  result = plain_mana_producer(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23dfb40; original cards/strixhaven.c:8223. */
int card_wandering_archaic(int player, int card, event_t event)
{
  /* Wandering Archaic	|5
   * Creature - Avatar 4/4
   * Whenever an opponent casts an instant or sorcery spell, they may pay |2. If they don't, you may copy that spell. You may choose new targets for the copy. */

  int mana_available;
  int opponent_or_paid;

  mdfc_front(player, card, event);
  opponent_or_paid = 1 - player;
  sp_ability_trigger(opponent_or_paid, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }

  mana_available = has_mana(opponent_or_paid, 0, 2);
  if ((mana_available != 0) &&
      (opponent_or_paid = charge_mana(opponent_or_paid, COLOR_COLORLESS, 2), opponent_or_paid != 0))
  {
    return 0;
  }
  copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23dfc10; original cards/strixhaven.c:8243. */
int card_explore_the_vastlands(int player, int card, event_t event)
{
  /* Explore the Vastlands	|3
   * Sorcery
   * Each player looks at the top five cards of their library and may reveal a land card and/or an instant or sorcery card from among them. Each player puts the cards they revealed this way into their hand and the rest on the bottom of their library in a random order. Each player gains 3 life. */

  enum { TUTOR_0 = 0 };
  mdfc_back(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {
    test_definition_t test;
    new_default_test_definition(&test, TYPE_INSTANT | TYPE_INTERRUPT | TYPE_SORCERY | TYPE_LAND,
                                "Select an instant, sorcery or land card.");
    test.ai_selection_mode = 1;
    APNAP(p, {
      int selected = reveal_top_cards_of_library_and_choose_who_chooses(
          player, card, p, p, 5, 0, TUTOR_TPLAYER_HAND, 1, TUTOR_0, 0, &test);
      if (selected == -1)
        put_top_x_on_bottom_in_random_order(p, 5);
      else
      {
        if (!is_what(p, selected, TYPE_LAND))
          new_default_test_definition(&test, TYPE_LAND, "Select a land card.");
        else
          new_default_test_definition(&test, TYPE_INSTANT | TYPE_INTERRUPT | TYPE_SORCERY,
                                      "Select an instant or sorcery card.");
        reveal_top_cards_of_library_and_choose_who_chooses(
            player, card, p, p, 4, 0, TUTOR_TPLAYER_HAND, 1, TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test);
      }
      gain_life(p, 3);
    });
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}
#endif

/* ManalinkEh 2022: 0x23dfdf0; original cards/strixhaven.c:8285. */
int card_witherbloom_apprentice(int player, int card, event_t event)
{
  /* Witherbloom Apprentice	|B|G
   * Creature - Human Druid 2/2
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, each opponent loses 1 life and you gain 1 life. */

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  lose_life(1 - player, 1);
  gain_life(player, 1);
  return 0;
}

/* ManalinkEh 2022: 0x23dfe70; original cards/strixhaven.c:8307. */
int card_witherbloom_command(int player, int card, event_t event)
{
  /* Witherbloom Command	|B|G
   * Sorcery
   * Choose two -
   * * Target player mills three cards, then you return a land card from your graveyard to your hand.
   * * Destroy target noncreature, nonland permanent with mana value 2 or less.
   * * Target creature gets -3/-1 until end of turn.
   * * Target opponent loses 2 life and you gain 2 life. */

  typedef enum
  {
    CHOICE_INVALID_TARGET = 0,
    CHOICE_MILL = 1,
    CHOICE_KIL = 2,
    CHOICE_PUMP = 3,
    CHOICE_OPP_LOSE2 = 4
  } CommandChoices;

  CommandChoices choice;
  int mill_available;
  int result;
  int pump_available;
  int drain_available;
  unsigned int target_number;
  uint32_t drain_priority;
  uint32_t pump_priority;
  uint32_t mill_priority;
  uint32_t destroy_priority;
  int opponent;
  card_instance_t *instance;
  target_definition_t *permanent_target;
  target_definition_t *creature_target;
  target_definition_t *player_target;
  CommandChoices choices[2];
  target_definition_t td;
  target_definition_t td2;
  target_definition_t td3;
  test_definition_t this_test;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    default_target_definition(player, card, &td2, 0x47);
    td2.illegal_type = TYPE_LAND | TYPE_CREATURE;
    td2.extra = 2;
    td2.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
    default_target_definition(player, card, &td3, 2);
    td3.preferred_controller = 1 - player;
    get_card_instance(player, card);

    mill_available = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td2, NULL, 1, NULL);
    pump_available = generic_spell(player, card, EVENT_CAN_CAST, 1, &td3, NULL, 1, NULL);
    drain_available = opponent_is_valid_target(player, card);
    return (
        unsigned int)(1 <
                      (int)(((((unsigned int)(mill_available != 0) - (unsigned int)(result == 0)) -
                              (unsigned int)(pump_available == 0)) +
                             3) -
                            (unsigned int)(drain_available == 0)));
  }
  if (event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  default_target_definition(player, card, &td2, 0x47);
  td2.illegal_type = TYPE_LAND | TYPE_CREATURE;
  td2.extra = 2;
  td2.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
  default_target_definition(player, card, &td3, 2);
  opponent = 1 - player;
  td3.preferred_controller = opponent;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    player_target = &td;
    creature_target = &td3;
    permanent_target = &td2;
    choice = dialog_impl(player, card, event, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS, DLG_RANDOM,
                         "Mill 3 & recycle land", 0, 1, "Destroy a permanent", 0, 1,
                         "Shrink creature", 0, 1, "Drain 2 life", 0, 1, 0, 0, 0, 0);
    choices[0] = choice;
    target_number = 0;
    for (int choice_index = 0; choice_index < 2; ++choice_index)
    {
      choice = choices[choice_index];
      switch (choice)
      {
      case CHOICE_INVALID_TARGET:
        break;
      case CHOICE_MILL:
        mill_available = validate_target(player, card, player_target, target_number);
        if (mill_available != 0)
        {
          mill(instance->targets[target_number].player, 3);
          new_default_test_definition(&this_test, 1, "Select a land card.");
          mill_available = new_special_count_grave(player, &this_test);
          if (mill_available != 0)
          {
            new_global_tutor(player, player, 2, 1, 0, 1, &this_test);
          }
        }
        break;
      case CHOICE_KIL:
        mill_available = validate_target(player, card, permanent_target, target_number);
        if (mill_available != 0)
        {
          action_on_target(player, card, target_number, ACT_KILL_DESTROY);
        }
        break;
      case CHOICE_PUMP:
        mill_available = validate_target(player, card, creature_target, target_number);
        if (mill_available != 0)
        {
          pump_until_eot(player, card, instance->targets[target_number].player,
                         instance->targets[target_number].card, -3, -1);
        }
        break;
      case CHOICE_OPP_LOSE2:
        td.allowed_controller = opponent;
        mill_available = validate_target(player, card, player_target, target_number);
        if (mill_available != 0)
        {
          lose_life(instance->targets[target_number].player, 2);
          gain_life(player, 2);
        }
      default:
        continue;
      }
      ++target_number;
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  drain_priority = 1;
  pump_priority = 1;
  mill_priority = 1;
  destroy_priority = 1;
  if (ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0))
  {
    drain_priority = 5;
    pump_priority = 0x14;
    mill_priority = 10;
    destroy_priority = 0x1e;
  }
  player_target = &td;
  creature_target = &td3;
  permanent_target = &td2;
  mill_available = generic_spell(player, card, EVENT_CAN_CAST, 1, player_target, NULL, 1, NULL);
  result = generic_spell(player, card, EVENT_CAN_CAST, 1, permanent_target, NULL, 1, NULL);
  pump_available = generic_spell(player, card, EVENT_CAN_CAST, 1, creature_target, NULL, 1, NULL);
  drain_available = opponent_is_valid_target(player, card);
  choice =
      dialog_impl(player, card, event, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS, DLG_RANDOM,
                  "Mill 3 & recycle land", mill_available, mill_priority, "Destroy a permanent",
                  result, destroy_priority, "Shrink creature", pump_available, pump_priority,
                  "Drain 2 life", drain_available, drain_priority, 0, 0, 0, 0);
  if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    int target_index = 0;
    instance->number_of_targets = 0;
    choices[0] = choice;
    for (int choice_index = 0; choice_index < 2; ++choice_index)
    {
      choice = choices[choice_index];
      switch (choice)
      {
      case CHOICE_INVALID_TARGET:
        spell_fizzled = 1;
        break;
      case CHOICE_MILL:
        result =
            new_pick_target(player_target, "Select target player to mill 3.", target_index, 0x81);
        if (result == 0)
        {
          return 0;
        }
        target_index = target_index + 1;
        break;
      case CHOICE_KIL:
        result = new_pick_target(
            permanent_target,
            "Select target noncreature, nonland permanent with mana value 2 or less", target_index,
            0x81);
        if (result == 0)
        {
          return 0;
        }
        target_index = target_index + 1;
        break;
      case CHOICE_PUMP:
        result = new_pick_target(creature_target, "Select target creature to get -3/-1.",
                                 target_index, 0x81);
        if (result == 0)
        {
          return 0;
        }
        target_index = target_index + 1;
        break;
      case CHOICE_OPP_LOSE2:
        instance->targets[target_index].player = opponent;
        instance->targets[target_index].card = -1;
        instance->number_of_targets = instance->number_of_targets + '\x01';
        target_index = target_index + 1;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23e0810; original cards/strixhaven.c:8473. */
int card_witherbloom_pledgemage(int player, int card, event_t event)
{
  /* Witherbloom Pledgemage	|3|BG|BG
   * Creature - Treefolk Warlock 5/5
   * Magecraft - Whenever you cast or copy an instant or sorcery spell, you gain 1 life. */

  int result;

  magecraft_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    gain_life(player, 1);
  }
  result = hybrid(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23e0890; original cards/strixhaven.c:8492. */
int card_zephyr_boots(int player, int card, event_t event)
{
  /* Zephyr Boots	|1
   * Artifact - Equipment
   * Equipped creature has flying.
   * Whenever equipped creature deals combat damage to a player, draw a card, then discard a card.
   * Equip |2 (|2: Attach to target creature you control. Equip only as a sorcery.) */

  int result;

  result = equipped_creature_deals_damage(player, card, event,
                                          DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE);
  if (0 < result)
  {
    do
    {
      draw_a_card(player);
      discard(player, 0, player);
      result = result + -1;
    } while (result != 0);
  }
  result = vanilla_equipment(player, card, event, 2, 0, 0, 0x20, 0);
  return result;
}

/* ManalinkEh 2022: 0x23e0930; original cards/strixhaven.c:8510. */
int card_zimone_quandrix_prodigy(int player, int card, event_t event)
{
  /* Zimone, Quandrix Prodigy	|G|U
   * Legendary Creature - Human Wizard 1/2
   * |1, |T: You may put a land card from your hand onto the battlefield tapped.
   * |4, |T: Draw a card. If you control eight or more lands, draw two cards instead. */

  check_legend_rule(player, card, event);
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
    return generic_activated_ability(player, card, event, 1, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
  if (event == EVENT_ACTIVATE)
  {
    instance->mode = 0;
    int can_draw = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 4, 0, 0, 0, 0, 0,
                                             0, NULL, NULL);
    int choice = dialog_impl(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_MODE,
                             "Put a land into play from hand",
                             generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 1, 0, 0,
                                                       0, 0, 0, 0, NULL, NULL),
                             5, "Draw a card", can_draw, 10, 0, 0, 0, 0);
    if (choice == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    bool had_active_cards = active_cards_count[player] > 0;
    int lands_in_hand = 0;
    int lands_in_play = 0;
    for (int candidate = 0; candidate < active_cards_count[player]; ++candidate)
      if (is_what(player, candidate, 1))
      {
        lands_in_hand += in_hand(player, candidate) != NULL;
        lands_in_play += in_play(player, candidate) != NULL;
      }
    int cost;
    if (choice == 2)
    {
      if (!had_active_cards || (unsigned int)(lands_in_play - 3 + lands_in_hand) > 4)
        ai_modifier += 0x30;
      cost = 4;
    }
    else if (choice == 1)
    {
      if (lands_in_hand != 0)
        ai_modifier += 0x18;
      cost = 1;
    }
    else
      return 0;
    return generic_activated_ability(player, card, event, 1, cost, 0, 0, 0, 0, 0, 0, NULL, NULL);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    unsigned char mode = instance->mode;
    if (mode == 1)
    {
      test_definition_t test;
      new_default_test_definition(&test, 1, "Select a land card to put into play.");
      new_global_tutor(player, player, 4, 5, 0, 1, &test);
      mode = instance->mode;
    }
    if (mode == 2)
    {
      int lands = count_permanents_by_type(player, TYPE_LAND);
      draw_cards(player, (lands > 7) + 1);
    }
  }
  return 0;
}
