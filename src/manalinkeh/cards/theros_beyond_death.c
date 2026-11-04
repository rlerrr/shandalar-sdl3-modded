#include "manalink.h"

int escape(int player, int card, event_t event, int colorless, int black, int blue, int green,
           int red, int white, int num_exile);

static int escape_part(int player, int card, event_t event, int colorless, int black, int blue,
                            int green, int red, int white, int num_exile);

static int daxos_effect(int player, int card, event_t event);

static void elspeth_counters(int s_player, int s_card, int t_player, int t_card);

static int medomais_prophecy_legacy(int player, int card, event_t event);

static int thb_god(int player, int card, event_t event, int color1, int color2, int amount);

static int anax_effect(int player, int card, event_t event);

static int athreos_effect(int player, int card, event_t event);

static int is_basic_land_by_internal_id(int iid, int unused, int unused1, int unused2);

#ifndef SHANDALAR

int card_shadowspear(int player, int card, event_t event);

#endif

/* ManalinkEh 2022: 0x235a2e0; original cards/theros_beyond_death.c:4. */
int escape(int player, int card, event_t event, int colorless, int black, int blue, int green,
           int red, int white, int num_exile)
{
  int result;

  if ((event != EVENT_GRAVEYARD_ABILITY) && (event != EVENT_PAY_FLASHBACK_COSTS))
  {
    return 0;
  }
  result = escape_part(player, card, event, colorless, black, blue, green, red, white, num_exile);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2359940; original cards/theros_beyond_death.c:4. */
static int escape_part(int player, int card, event_t event, int colorless, int black, int blue,
                       int green, int red, int white, int num_exile)
{
  int32_t *grave_index_ptr;
  card_instance_t *instance;
  int result;
  int position;
  int iid;
  target_t *target_cursor;
  test_definition_t this_test;
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = has_mana_multi(player, colorless, black, blue, green, red, white);
    if ((result != 0) && (result = count_graveyard(player), num_exile < result))
    {
      result = get_id(player, card);
      result = can_legally_play_csvid_now(player, result, EVENT_GRAVEYARD_ABILITY);
      if (result != 0)
      {
        return ((result == 99) ? 9216U : 8192U);
      }
      if ((hack_allow_sorcery_if_rules_engine_is_only_effect == 1) && (stack_size == 1))
      {
        return 0x2000;
      }
    }
  }
  else if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    instance = get_card_instance(player, card);
    new_default_test_definition(&this_test, 0xff, "Select a card to exile for escape.");
    this_test.id = 0x388;
    this_test.id_flag = 1;
    result = charge_mana_multi(player, colorless, black, blue, green, red, white);
    if (result != 0)
    {
      result = instance->targets[3].card;
      position = validate_target_from_grave_source(player, card, player, 3);
      if (position == -1)
      {
        return -1;
      }
      iid = turn_card_in_grave_face_down(player, position);
      select_multiple_cards_from_graveyard(player, player, -1, 3, &this_test, num_exile,
                                           instance->targets);
      result = find_in_graveyard_by_source(player, result, position);
      turn_card_in_grave_face_up(player, result, iid);
    }
    if (spell_fizzled == 1)
    {
      return 0;
    }
    if (0 < num_exile)
    {
      target_cursor = instance->targets;
      do
      {
        grave_index_ptr = &target_cursor->player;
        target_cursor = target_cursor + 1;
        rfg_card_from_grave(player, *grave_index_ptr);
      } while (target_cursor != instance->targets + num_exile);
    }
    return 1;
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x235a200; original cards/theros_beyond_death.c:53. */
static int escape_with_counters_impl(int player, int card, event_t event, int colorless, int black,
                                     int blue, int green, int red, int white, int num_exile,
                                     int num_counters)
{
  int result;
  if ((((event == EVENT_CAST_SPELL) || (event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE)) &&
       (affected_card == card)) &&
      (affected_card_controller == player))
  {
    result = check_special_flags3(player, card, 0x80000);
    if (result != 0)
    {
      enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, num_counters);
    }
  }
  if ((event != EVENT_GRAVEYARD_ABILITY) && (event != EVENT_PAY_FLASHBACK_COSTS))
  {
    return 0;
  }
  return escape_part(player, card, event, colorless, black, blue, green, red, white, num_exile);
}
#endif

/* ManalinkEh 2022: 0x2358bf0; original cards/theros_beyond_death.c:61. */
static int thb_god(int player, int card, event_t event, int color1, int color2, int amount)
{
  int iid;
  unsigned int saved_type_or_status;
  card_instance_t *god;
  int devotion_or_current_iid;
  int noncreature_iid;
  card_instance_t *saved_record;
  card_instance_t *live_record;
  unsigned char noncreature_type;

  if (((event == EVENT_CAN_CAST) || (event == EVENT_SHOULD_AI_PLAY)) &&
      ((devotion_or_current_iid = is_what(player, card, 2),
        devotion_or_current_iid == 0 || (god = in_hand(player, card), god != NULL))))
  {
    return global_enchantment(player, card, event);
  }
  if ((event != EVENT_CHANGE_TYPE) &&
      (event != EVENT_CAST_SPELL && event != EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE))
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
  if ((((unsigned int)land_can_be_played >> 16 & 0xff) & 2) != 0)
  {
    return 0;
  }

  god = get_card_instance(player, card);
  if (event == EVENT_CAST_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE)
  {
    iid = get_original_internal_card_id(player, card);
    god->targets[7].card = iid;
  }
  devotion_or_current_iid = devotion(player, card, color1, color2);
  if (devotion_or_current_iid < amount)
  {
    devotion_or_current_iid = god->internal_card_id;
    noncreature_type = cards_data[devotion_or_current_iid].type & 0xfd;
    if ((cards_data[devotion_or_current_iid].type & 2) == 0)
    {
      return 0;
    }
    if ((god->targets[7].player != devotion_or_current_iid) ||
        (noncreature_iid = god->dummy3, cards_data[noncreature_iid].type != noncreature_type))
    {

      noncreature_iid = create_a_card_type(devotion_or_current_iid);
      if (noncreature_iid == -1)
      {
        return 0;
      }
      cards_data[noncreature_iid].type = noncreature_type;
      saved_type_or_status = god->regen_status;
      god->targets[7].player = devotion_or_current_iid;
      god->dummy3 = noncreature_iid;
      god->regen_status = saved_type_or_status | 0x1000000;
      god->internal_card_id = noncreature_iid;
      god->backup_internal_card_id = (uint16_t)noncreature_iid;
    }
    if (event == EVENT_CHANGE_TYPE)
    {
      event_result = noncreature_iid;
      return 0;
    }
    return 0;
  }
  if (event != EVENT_CHANGE_TYPE)
    return 0;
  saved_record = get_card_instance(player, card);
  live_record = get_card_instance(player, card);
  saved_type_or_status = saved_record->targets[7].card;
  live_record->targets[7].player = saved_type_or_status;
  god->internal_card_id = saved_type_or_status;
  god->backup_internal_card_id = (uint16_t)saved_type_or_status;
  event_result = saved_type_or_status & 0xffff;
  return 0;
}

/* ManalinkEh 2022: 0x235a360; original cards/theros_beyond_death.c:115. */
int card_agonizing_remorse(int player, int card, event_t event)
{
  /* Agonizing Remorse	|1|B
   * Sorcery
   * Target opponent reveals their hand. You choose a nonland card from it or a card from their graveyard. Exile that card. You lose 1 life. */

  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      test_definition_t this_test;
      bool try_hand = hand_count[instance->targets[0].player] > 0;
      int chosen = -1;
      while (true)
      {
        if (try_hand)
        {
          new_default_test_definition(&this_test, 1, "Opponent's hand: Select a card to exile.");
          this_test.type_flag = 1;
          chosen = new_global_tutor(player, instance->targets[0].player, 4, 6, 0, 1, &this_test);
        }
        if (chosen >= 0)
        {
          lose_life(player, 1);
          break;
        }
        int target_player = instance->targets[0].player;
        if (*graveyard_ptr_mutable[target_player] == -1)
        {
          if (hand_count[target_player] < 1)
          {
            spell_fizzled = 1;
            break;
          }
          try_hand = true;
        }
        else
        {
          new_default_test_definition(&this_test, 1,
                                      "Opponent's graveyard: Select a card to exile.");
          this_test.type_flag = 1;
          chosen = new_global_tutor(player, instance->targets[0].player, 2, 6, 0, 1, &this_test);
          if (chosen >= 0)
          {
            lose_life(player, 1);
            break;
          }
          target_player = instance->targets[0].player;
          try_hand = hand_count[target_player] >= 1;
          if (!try_hand && *graveyard_ptr_mutable[target_player] == -1)
          {
            spell_fizzled = 1;
            break;
          }
        }
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x40, &td, NULL, 1, NULL);
}

/* ManalinkEh 2022: 0x235a610; original cards/theros_beyond_death.c:180. */
int card_alseid_of_lifes_bounty(int player, int card, event_t event)
{
  /* Alseid of Life's Bounty	|W
   * Enchantment Creature - Nymph 1/1
   * Lifelink
   * |1, Sacrifice this creature: Target creature or enchantment you control gains protection from the color of your choice until end of turn. */

  card_instance_t *instance;
  unsigned int protection_color;
  target_definition_t td;

  lifelink(player, card, event);
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 6);
    td.preferred_controller = player;
    td.allowed_controller = player;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return global_enchantment(player, card, event);
    }
    default_target_definition(player, card, &td, 6);
    td.preferred_controller = player;
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (valid_target(&td))
      {
        protection_color = select_a_protection(player);
        pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                               instance->targets[0].player, instance->targets[0].card, 0, 0,
                               protection_color | 0x10000000, 0);
      }
    }
  }
  return generic_activated_ability(player, card, event, 0xe, 1, 0, 0, 0, 0, 0, 0, &td,
                                   "Select target creature or enchantment you control.");
}

/* ManalinkEh 2022: 0x23582f0; original cards/theros_beyond_death.c:219. */
static int anax_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  int pending_or_result;
  int attached_player;
  int dying_or_creature;
  int affected_player;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (pending_or_result =
               get_id((int)instance->damage_target_player, instance->damage_target_card),
           pending_or_result != 0x51d8))
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
      instance = in_play(affected_card_controller, affected_card);

      if (((instance != NULL) && (affected_card_controller == player)) &&
          (instance = get_card_instance(player, affected_card), instance->kill_code != '\0'))
      {

        instance = get_card_instance(player, card);
        if (instance->damage_target_player < '\0')
        {
          affected_player = affected_card_controller;
        }
        else
        {

          pending_or_result = instance->damage_target_card;
          affected_player = affected_card_controller;
          if ((pending_or_result == affected_card) &&
              (attached_player = (int)instance->damage_target_player,
               attached_player == affected_card_controller))
          {
            dying_or_creature = is_what(attached_player, pending_or_result, 2);
            if ((dying_or_creature == 0) &&
                ((dying_or_creature = is_this_dying(attached_player, pending_or_result),
                  dying_or_creature == 0 &&
                      (pending_or_result = is_token(attached_player, pending_or_result),
                       pending_or_result == 0))))
            {
              if (instance->targets[0xb].player < 0)
              {
                instance->targets[0xb].player = 0;
              }
              *(uint8_t *)&instance->targets[0xb].player =
                  (char)instance->targets[0xb].player + card_counts[player][CCOUNTS_TEYSA_KARLOV] + '\x01';
            }
            instance->damage_target_card = -1;
            instance->damage_target_player = -1;
            remove_status(player, card, 0x10000);
            affected_player = affected_card_controller;
          }
        }
        affected_card_controller = affected_player;
        pending_or_result = is_what(affected_player, affected_card, 2);
        if (((pending_or_result != 0) &&
             (pending_or_result = is_this_dying(affected_card_controller, affected_card),
              pending_or_result != 0)) &&
            (pending_or_result = is_token(affected_card_controller, affected_card),
             pending_or_result == 0))
        {
          test_definition_t test;
          default_test_definition(&test, 2);
          test.type_flag = 3;
          count_for_gfp_ability(player, card, 0x77, player, -1, &test);
          pending_or_result = get_power(affected_card_controller, affected_card);
          if (3 < pending_or_result)
          {
            instance = get_card_instance(player, card);
            instance->targets[0xb].player =
                instance->targets[0xb].player + card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
          }
        }
      }
    }
  }
  pending_or_result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_or_result != 0)
  {
    if (check_status(player, card, 0x10000) == 0)
    {
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_SATYR, &token);
      token.special_infos = 66;
      token.tou = 1;
      token.pow = 1;
      token.qty = (char)pending_or_result;
      generate_token(&token);
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) &&
      (instance = get_card_instance(player, card), instance->damage_target_player == -1))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x235a7d0; original cards/theros_beyond_death.c:285. */
int card_anax_hardened_in_the_forge(int player, int card, event_t event)
{
  /* Anax, Hardened in the Forge	|1|R|R
   * Legendary Enchantment Creature - Demigod * /3
   * Anax's power is equal to your devotion to red. (Each |R in the mana costs of permanents you control counts toward your devotion to red.)
   * Whenever Anax or another nontoken creature you control dies, create a 1/1 red Satyr creature token with "This token can't block." If the creature had power 4 or greater, create two of those tokens instead. */

  card_instance_t *instance;
  int devotion_or_created_or_pending;

  bool legacy_found = false;
  check_legend_rule(player, card, event);
  if (event == EVENT_POWER)
  {
    if (((card == affected_card) && (affected_card_controller == player)) && (player != -1))
    {
      devotion_or_created_or_pending = devotion(player, card, 4, 0);
      set_characteristic_pt(devotion_or_created_or_pending);
    }
  }
  else if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      if (0 < active_cards_count[player])
      {
        devotion_or_created_or_pending = 0;
        do
        {
          instance = in_play(player, devotion_or_created_or_pending);

          if ((((instance != NULL) &&
                (is_what(player, devotion_or_created_or_pending, 0x80) != 0)) &&
               (instance = get_card_instance(player, devotion_or_created_or_pending),
                instance->info_slot_fn == anax_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            legacy_found = true;
            break;
          }
          devotion_or_created_or_pending = devotion_or_created_or_pending + 1;
        } while (devotion_or_created_or_pending < active_cards_count[player]);
      }

      if (!legacy_found)
      {
        devotion_or_created_or_pending =
            create_targetted_legacy_effect(player, card, anax_effect, player, card);
        instance = get_card_instance(player, devotion_or_created_or_pending);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, devotion_or_created_or_pending, 0x10000);
      }
    }
  }
  else if ((event == EVENT_GRAVEYARD_FROM_PLAY) &&
           (instance = in_play(affected_card_controller, affected_card), instance != NULL))
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.type_flag = 3;
    count_for_gfp_ability(player, card, 0x77, player, -1, &test);
    devotion_or_created_or_pending = get_power(affected_card_controller, affected_card);
    if (3 < devotion_or_created_or_pending)
    {
      instance = get_card_instance(player, card);
      instance->targets[0xb].player = instance->targets[0xb].player + card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
    }
  }
  instance = get_card_instance(player, card);

  if (((instance->kill_code == '\0') && (devotion_or_created_or_pending = resolve_gfp_ability(
                                             player, card, event, RESOLVE_TRIGGER_MANDATORY),
                                         devotion_or_created_or_pending != 0)) &&
      ((instance = in_play(player, card),
        instance != NULL && (instance = in_play(player, card), instance->kill_code == '\0'))))
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_SATYR, &token);
    token.special_infos = 66;
    token.tou = 1;
    token.pow = 1;
    token.qty = (char)devotion_or_created_or_pending;
    generate_token(&token);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x235aaf0; original cards/theros_beyond_death.c:408. */
int card_ashiok_nightmare_muse(int player, int card, event_t event)
{
  /* Ashiok, Nightmare Muse	|3|U|B
   * Legendary Planeswalker - Ashiok (5)
   * +1: Create a 2/3 blue and black Nightmare creature token with "Whenever this token attacks or blocks, each opponent exiles the top two cards of their library."
   * -3: Return target nonland permanent to its owner's hand, then that player exiles a card from their hand.
   * -7: You may cast up to three spells from among face-up cards your opponents own from exile without paying their mana costs. */

  enum
  {
    CHOICE_TOKEN = 1,
    CHOICE_BOUNCE = 2,
    CHOICE_ULTIMATE = 3
  };

  int choice;
  uint8_t selected_mode;
  card_instance_t *instance;
  int result_or_opponent;
  int available_or_selected;
  int i;
  int play_limit;
  target_definition_t *permanent_target;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x47);
    td.illegal_type = TYPE_LAND;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->mode = '\0';
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, DLG_MODE,
                           "Nightmare token", 1, 5, 1, "Bounce & exile from hand", can_target(&td),
                           10, 0xfffffffd, "Cast from exile", 1, 0x14, 0xfffffff9, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_BOUNCE)
      {
        new_pick_target(&td, "TARGET_NONLAND_PERMANENT", 0, 1);
      }
      return planeswalker(player, card, event, 4);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return planeswalker(player, card, event, 5);
    }
    default_target_definition(player, card, &td, 0x47);
    td.illegal_type = TYPE_LAND;
    instance = get_card_instance(player, card);
  }
  permanent_target = &td;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    selected_mode = instance->mode;
    if (selected_mode == '\x01')
    {
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_NIGHTMARE_TOKEN, &token);
      token.pow = 2;
      token.tou = 3;
      token.special_infos = 0x42;
      generate_token(&token);
    }
    else if (selected_mode == '\x02')
    {
      result_or_opponent = valid_target(permanent_target);
      if ((result_or_opponent != 0) &&
          (bounce_permanent(instance->targets[0].player, instance->targets[0].card),
           hand_count[instance->targets[0].player] != 0))
      {
        test_definition_t this_test;

        default_test_definition(&this_test, 0xff);
        result_or_opponent = instance->targets[0].player;
        result_or_opponent =
            new_select_a_card(result_or_opponent, result_or_opponent, 4, 1, 3, -1, &this_test);
        rfg_card_in_hand(instance->targets[0].player, result_or_opponent);
      }
    }
    else if (selected_mode == '\x03')
    {
      result_or_opponent = 1 - player;
      if (count_rfg(result_or_opponent) != 0)
      {
        test_definition_t test;

        new_default_test_definition(&test, 1, "Select a card to cast for free.");
        test.type_flag = DOESNT_MATCH;

        available_or_selected = special_count_zone(player, rfg_ptr[result_or_opponent], &test);
        play_limit = available_or_selected;
        if (3 < available_or_selected)
        {
          play_limit = 3;
        }
        i = 0;
        if (0 < available_or_selected)
        {
          do
          {

            available_or_selected =
                new_select_a_card(player, result_or_opponent, 3, 0, 2, 0, &test);
            if ((available_or_selected == -1) &&
                (available_or_selected =
                     new_select_a_card(player, result_or_opponent, 3, 0, 2, 0, &test),
                 available_or_selected == -1))
              break;
            play_card_in_exile_for_free(
                player, result_or_opponent,
                (unsigned int)cards_data[rfg_ptr[result_or_opponent][available_or_selected]].id);
            i = i + 1;
          } while (i < play_limit);
        }
      }
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x2358760; original cards/theros_beyond_death.c:532. */
static int athreos_effect(int player, int card, event_t event)
{
  int owner;
  int iid;

  card_instance_t *instance;
  card_instance_t *dying;
  int pending_or_result;
  int returned_iid;
  target_t *return_cursor;
  unsigned int stored_count;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (pending_or_result =
               get_id((int)instance->damage_target_player, instance->damage_target_card),
           pending_or_result != 0x51e4))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if (instance->damage_target_player >= 0 &&
        effect_follows_control_of_attachment(player, card, 0x4700))
      return 0;
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
        pending_or_result = is_what(affected_card_controller, affected_card, 2);
        if (((pending_or_result != 0) &&
             (pending_or_result =
                  count_counters(affected_card_controller, affected_card, COUNTER_COIN),
              pending_or_result != 0)) &&
            (pending_or_result = is_token(affected_card_controller, affected_card),
             pending_or_result == 0))
        {

          pending_or_result = is_this_dying(affected_card_controller, affected_card);
          if ((pending_or_result != 0) && (pending_or_result = check_for_death_trigger_removal(
                                               affected_card_controller, affected_card),
                                           pending_or_result != 0))
          {
            return 0;
          }
          stored_count = instance->targets[0xb].player;
          if ((int)stored_count < 0)
          {
            instance->targets[0xb].player = 0;
            stored_count = 0;
          }
          else
            stored_count &= 0xff;
          if (stored_count < 10)
          {
            owner = get_owner(affected_card_controller, affected_card);
            *(char *)&instance->targets[stored_count].player = (char)owner;
            if (is_this_dying(affected_card_controller, affected_card) == 0)
            {
              SET_BYTE1(instance->targets[stored_count].player) = 4;
            }
            iid = get_original_internal_card_id(affected_card_controller, affected_card);
            instance->targets[stored_count].card = iid;
          }
          *(char *)&instance->targets[0xb].player = (char)instance->targets[0xb].player + '\x01';
        }
      }
    }
  }
  pending_or_result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (!check_status(player, card, 0x10000))
  {
    instance = get_card_instance(player, card);
    if (1 < pending_or_result)
    {
      return_cursor = instance->targets + 1;
      do
      {
        returned_iid = return_cursor->card;
        if ((char)((unsigned int)return_cursor->player >> 8) == '\x04')
        {
          if (check_rfg(player, (unsigned int)cards_data[returned_iid].id) != 0)
          {

            put_into_play(player, add_card_to_hand(player, returned_iid));
          }
        }
        else
        {

          seek_grave_for_id_to_reanimate(player, -1, return_cursor->player & 0xff,
                                         (unsigned int)cards_data[returned_iid].id,
                                         REANIMATE_DEFAULT);
        }
        return_cursor->card = -1;
        return_cursor->player = -1;
        return_cursor = return_cursor + 1;
      } while (return_cursor != instance->targets + pending_or_result);
    }
    kill_card(player, card, KILL_EXILE);
  }
  if ((event == EVENT_CLEANUP) &&
      (pending_or_result = check_status(player, card, 0x10000), pending_or_result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x235afb0; original cards/theros_beyond_death.c:609. */
int card_athreos_shroud_veiled(int player, int card, event_t event)
{
  /* Athreos, Shroud-Veiled	|4|W|B
   * Legendary Enchantment Creature - God 4/7
   * Indestructible
   * As long as your devotion to white and black is less than seven, Athreos isn't a creature.
   * At the beginning of your end step, put a coin counter on another target creature.
   * Whenever a creature with a coin counter on it dies or is put into exile, return that card to the battlefield under your control. */

  int i;
  int owner;
  int iid;

  int created_or_pending;
  card_instance_t *instance;
  int result_or_iid;
  target_t *return_cursor;
  unsigned int stored_count;
  target_definition_t td;

  bool legacy_found = false;
  check_legend_rule(player, card, event);
  indestructible(player, card, event);
  created_or_pending = eot_trigger(player, card, event);
  if ((created_or_pending != 0) && (current_turn == player))
  {
    default_target_definition(player, card, &td, 2);
    td.special = TARGET_SPECIAL_NOT_ME;
    created_or_pending = can_target(&td);
    if ((created_or_pending != 0) &&
        (created_or_pending = pick_target(&td, "TARGET_ANOTHER_CREATURE"), created_or_pending != 0))
    {

      instance = get_card_instance(player, card);
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                   COUNTER_COIN, 1);
      instance->number_of_targets = '\0';
    }
  }
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      i = 0;
      if (0 < active_cards_count[player])
      {
        do
        {
          instance = in_play(player, i);

          if ((((instance != NULL) &&
                (result_or_iid = is_what(player, i, 0x80), result_or_iid != 0)) &&
               (instance = get_card_instance(player, i),
                instance->info_slot_fn == athreos_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            legacy_found = true;
            break;
          }
          i = i + 1;
        } while (i < active_cards_count[player]);
      }

      if (!legacy_found)
      {
        created_or_pending =
            create_targetted_legacy_effect(player, card, athreos_effect, player, card);
        instance = get_card_instance(player, created_or_pending);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, created_or_pending, 0x10000);
      }
    }
  }
  if (!(((event != EVENT_GRAVEYARD_FROM_PLAY) ||
         (created_or_pending = is_what(affected_card_controller, affected_card, 2),
          created_or_pending == 0)) ||
        ((created_or_pending =
              count_counters(affected_card_controller, affected_card, COUNTER_COIN),
          created_or_pending == 0 ||
              (created_or_pending = is_token(affected_card_controller, affected_card),
               created_or_pending != 0)))))
  {

    created_or_pending = is_this_dying(affected_card_controller, affected_card);
    if ((created_or_pending != 0) && (created_or_pending = check_for_death_trigger_removal(
                                          affected_card_controller, affected_card),
                                      created_or_pending != 0))
    {
      return 0;
    }
    instance = get_card_instance(player, card);
    stored_count = instance->targets[0xb].player;
    if ((int)stored_count < 0)
    {
      instance->targets[0xb].player = 0;
      stored_count = 0;
    }
    else
      stored_count &= 0xff;
    if (stored_count < 10)
    {
      owner = get_owner(affected_card_controller, affected_card);
      *(char *)&instance->targets[stored_count].player = (char)owner;
      if (is_this_dying(affected_card_controller, affected_card) == 0)
      {
        SET_BYTE1(instance->targets[stored_count].player) = 4;
      }
      iid = get_original_internal_card_id(affected_card_controller, affected_card);
      instance->targets[stored_count].card = iid;
    }
    *(char *)&instance->targets[0xb].player = (char)instance->targets[0xb].player + '\x01';
  }
  instance = get_card_instance(player, card);
  if (instance->kill_code == '\0')
  {

    instance = get_card_instance(player, card);
    created_or_pending = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

    if (0 < created_or_pending)
    {
      return_cursor = instance->targets;
      do
      {
        result_or_iid = return_cursor->card;
        if ((char)((unsigned int)return_cursor->player >> 8) == '\x04')
        {
          if (check_rfg(player, (unsigned int)cards_data[result_or_iid].id) != 0)
          {

            put_into_play(player, add_card_to_hand(player, result_or_iid));
          }
        }
        else
        {

          seek_grave_for_id_to_reanimate(player, -1, return_cursor->player & 0xff,
                                         (unsigned int)cards_data[result_or_iid].id,
                                         REANIMATE_DEFAULT);
        }
        return_cursor->card = -1;
        return_cursor->player = -1;
        return_cursor = return_cursor + 1;
      } while (return_cursor != instance->targets + created_or_pending);
    }
  }
  return thb_god(player, card, event, 5, 1, 7);
}

/* ManalinkEh 2022: 0x2358e10; original cards/theros_beyond_death.c:752. */
static int calix_obliviation_legacy(int player, int card, event_t event)
{
  int player_00;
  int iid;
  card_instance_t *instance;
  int returned_card;
  target_t *return_cursor;

  instance = get_card_instance(player, card);
  other_leaves_play(player, card, (int)instance->damage_target_player, instance->damage_target_card,
                    event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    return_cursor = instance->targets;
    do
    {
      player_00 = return_cursor->player;
      if (player_00 == -1)
      {
        return 0;
      }

      iid = return_cursor->card;
      if (check_rfg(player_00, (unsigned int)cards_data[iid].id) != 0)
      {

        returned_card = add_card_to_hand(player_00, iid);
        if (has_subtype(player_00, returned_card, SUBTYPE_AURA) != 0)
        {
          if (call_card_function(player_00, returned_card, EVENT_CAN_CAST) == 0)
          {
            obliterate_card(player_00, returned_card);
            ++return_cursor;
            continue;
          }
          set_special_flags3(player_00, returned_card, 0x40);
        }
        remove_card_from_rfg(player_00, (unsigned int)cards_data[iid].id);
        put_into_play(player_00, returned_card);
      }

      return_cursor = return_cursor + 1;
    } while (return_cursor != instance->targets + 0x12);
  }
  return 0;
}

/* ManalinkEh 2022: 0x235b430; original cards/theros_beyond_death.c:786. */
int find_attached_legacy_effect_not_full(int player, int card, CardFunction func_ptr,
                                         int target_player_id, int target_card)
{
  card_instance_t *instance;
  int card_00;

  if (0 < active_cards_count[player])
  {
    card_00 = 0;
    do
    {
      instance = in_play(player, card_00);
      if (((((instance != NULL) && (instance->internal_card_id == LEGACY_EFFECT_CUSTOM)) &&
            (func_ptr == (CardFunction)instance->info_slot)) &&
           ((instance->damage_source_player == player &&
             (instance->damage_source_card == card)))) &&
          ((instance->damage_target_player == target_player_id &&
            ((instance->damage_target_card == target_card &&
              (instance->targets[0x12].player == -1))))))
      {
        return card_00;
      }
      card_00 = card_00 + 1;
    } while (card_00 < active_cards_count[player]);
  }
  return -1;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x235b4c0; original cards/theros_beyond_death.c:801. */
int card_calix_destinys_hand(int player, int card, event_t event)
{
  /* Calix, Destiny's Hand	|2|G|W
   * Legendary Planeswalker - Calix (4)
   * +1: Look at the top four cards of your library. You may reveal an enchantment card from among them and put that card into your hand. Put the rest on the bottom of your library in a random order.
   * -3: Exile target creature or enchantment you don't control until target enchantment you control leaves the battlefield.
   * -7: Return all enchantment cards from your graveyard to the battlefield. */

  enum
  {
    CHOICE_REVEAL = 1,
    CHOICE_OBLIVIATE = 2,
    CHOICE_REPLENISH = 3
  };

  int i;
  int card_00;
  int parent_card;
  int target_player;
  int target_player_id;
  int target_card;
  bool melded;
  card_instance_t *instance;
  int result_or_mode;
  int legacy_or_token;
  unsigned int available_or_csvid;
  int exiled_owner;
  int iid;
  target_definition_t *enchantment_target;
  target_definition_t *exile_target;
  target_definition_t td;
  target_definition_t td1;
  test_definition_t test;
  int csvids[2];

  check_legend_rule(player, card, event);
  if (IS_GAA_EVENT(event))
  {
    default_target_definition(player, card, &td, 6);
    td.allowed_controller = 1 - player;
    default_target_definition(player, card, &td1, 4);
    td1.allowed_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result_or_mode = can_target(&td);
      available_or_csvid = 0;
      if (result_or_mode != 0)
      {
        result_or_mode = can_target(&td1);
        available_or_csvid = (unsigned int)(result_or_mode != 0);
      }
      result_or_mode = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                                   "Reveal top 4", 1, 5, 1, "Obliviate", available_or_csvid, 10,
                                   0xfffffffd, "Replenish", 1, 0x14, 0xfffffff9, 0, 0, 0, 0);
      if (result_or_mode == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if ((result_or_mode == CHOICE_OBLIVIATE) &&
          (result_or_mode = new_pick_target(
               &td, "Select target creature or enchantment you don\'t control.", 0, 0x81),
           result_or_mode != 0))
      {
        new_pick_target(&td1, "Select target enchantment you control.", 1, 0x81);
      }
    }

    exile_target = &td;
    enchantment_target = &td1;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result_or_mode = instance->info_slot;
      if (result_or_mode == 1)
      {
        new_default_test_definition(&test, 4, "Select an enchantment card");
        reveal_top_cards_of_library_and_choose(player, card, player, 4, 0, TUTOR_HAND, 1,
                                               TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test);
      }
      else if (result_or_mode == 2)
      {
        result_or_mode = valid_target(exile_target);
        if ((result_or_mode != 0) &&
            (result_or_mode = validate_target(player, card, enchantment_target, 1),
             result_or_mode != 0))
        {
          result_or_mode = instance->parent_controller;
          card_00 = instance->targets[0].card;
          parent_card = instance->parent_card;
          target_player = instance->targets[0].player;
          target_player_id = instance->targets[1].player;
          target_card = instance->targets[1].card;
          legacy_or_token = is_token(target_player, card_00);
          if ((legacy_or_token == 0) && (instance = in_play(target_player, card_00), instance != NULL))
          {
            iid = get_original_internal_card_id(target_player, card_00);
            available_or_csvid = (unsigned int)cards_data[iid].id;
            if ((available_or_csvid == 0x3eae || available_or_csvid == 0x3ef4) ||
                (available_or_csvid == 0x3efb))
            {
              unmeld(available_or_csvid, &csvids[0], &csvids[1]);
              iid = get_internal_card_id_from_csv_id(csvids[0]);
              melded = true;
            }
            else
            {
              melded = false;
            }
            do
            {
              legacy_or_token = find_attached_legacy_effect_not_full(
                  result_or_mode, parent_card, calix_obliviation_legacy, target_player_id, target_card);
              if (legacy_or_token == -1)
              {
                instance = get_card_instance(
                    player,
                    create_targetted_legacy_effect(result_or_mode, parent_card, calix_obliviation_legacy,
                                                   target_player_id, target_card));
                legacy_or_token = get_owner(target_player, card_00);
                instance->targets[0].player = legacy_or_token;
                instance->targets[0].card = iid;
              }
              else
              {
                instance = get_card_instance(player, legacy_or_token);
                i = 1;
                do
                {
                  if (instance->targets[i].player == -1)
                  {
                    exiled_owner = get_owner(target_player, card_00);
                    instance->targets[i].player = exiled_owner;
                    instance->targets[i].card = iid;
                    break;
                  }
                  i = i + 1;
                } while (i != 0x12);
              }
              if (!melded)
                break;
              melded = false;
              iid = get_internal_card_id_from_csv_id(csvids[0]);
            } while (true);
          }
          kill_card(target_player, card_00, KILL_EXILE);
        }
      }
      else if (result_or_mode == 3)
      {
        default_test_definition(&test, 4);
        new_reanimate_all(player, card, player, &test, REANIMATE_DEFAULT);
      }
    }
  }
  return planeswalker(player, card, event, 4);
}
#endif

/* ManalinkEh 2022: 0x235bb10; original cards/theros_beyond_death.c:946. */
int card_cling_to_dust(int player, int card, event_t event)
{
  /* Cling to Dust	|B
   * Instant
   * Exile target card from a graveyard. If it was a creature card, you gain 3 life. Otherwise, you draw a card.
   * Escape-|3|B, Exile five other cards from your graveyard. (You may cast this card from your graveyard for its escape cost.) */

  int grave_target;
  int was_creature;
  int whose_graveyard;
  test_definition_t test;

  if ((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS))
  {
    return escape(player, card, event, 3, 1, 0, 0, 0, 0, 5);
  }
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  new_default_test_definition(&test, 0xff, "Select target card to exile.");
  if (event == EVENT_RESOLVE_SPELL)
  {

    whose_graveyard = 1 - player;
    grave_target = validate_target_from_grave_source(player, card, whose_graveyard, 1);
    if (grave_target != -1)
    {

      was_creature = is_what(-1, graveyard_ptr_mutable[whose_graveyard][grave_target], 2);
      rfg_card_from_grave(whose_graveyard, grave_target);
      if (was_creature == 0)
      {
        draw_a_card(player);
      }
      else
      {
        gain_life(player, 3);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x1000, NULL, NULL, 1, &test);
}

/* ManalinkEh 2022: 0x2358f60; original cards/theros_beyond_death.c:1003. */
static int daxos_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  card_instance_t *dying;
  unsigned int pending_deaths;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (get_id((int)instance->damage_target_player, instance->damage_target_card) != 0x51f5))
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
          return 0;
        }
        count_for_gfp_ability(player, card, 0x77, player, 2, NULL);
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
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) && (check_status(player, card, 0x10000) == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x235bd50; original cards/theros_beyond_death.c:1054. */
int card_daxos_blessed_by_the_sun(int player, int card, event_t event)
{
  /* Daxos, Blessed by the Sun	|W|W
   * Legendary Enchantment Creature - Demigod 2/ *
   * Daxos's toughness is equal to your devotion to white. (Each |W in the mana costs of permanents you control counts toward your devotion to white.)
   * Whenever another creature you control enters or dies, you gain 1 life. */

  int c;
  int created_or_pending;
  card_instance_t *instance;
  test_definition_t test;

  bool legacy_found = false;
  check_legend_rule(player, card, event);
  if (event == EVENT_TOUGHNESS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      set_characteristic_pt(devotion(player, card, 5, 0));
    }
    default_test_definition(&test, 2);
    test.not_me = 1;
    etb_ability_global(player, EVENT_TOUGHNESS, 2, TYPE_NONE, &test, player, card);
  }
  else
  {
    default_test_definition(&test, 2);
    test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
    if (event == EVENT_ETB)
    {
      gain_life(player, 1);
    }
    else if (event == EVENT_STATIC_EFFECTS)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {

        if (0 < active_cards_count[player])
        {
          c = 0;
          do
          {
            instance = in_play(player, c);

            if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
                 (instance = get_card_instance(player, c),
                  instance->info_slot_fn == daxos_effect)) &&
                (((instance->damage_target_player == player &&
                   (instance->damage_target_card == card)) ||
                  ((instance->targets[0].player == player &&
                    (instance->targets[0].card == card))))))
            {
              legacy_found = true;
              break;
            }
            c = c + 1;
          } while (c < active_cards_count[player]);
        }

        if (!legacy_found)
        {
          created_or_pending =
              create_targetted_legacy_effect(player, card, daxos_effect, player, card);
          instance = get_card_instance(player, created_or_pending);
          instance->targets[0].player = player;
          instance->targets[0].card = card;
          instance->number_of_targets = '\x01';
          add_status(player, created_or_pending, 0x10000);
        }
      }
    }
    else if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {
      count_for_gfp_ability(player, card, 0x77, player, 0, &test);
    }
  }
  created_or_pending = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (0 < created_or_pending)
  {
    do
    {
      gain_life(player, 1);
      created_or_pending = created_or_pending + -1;
    } while (created_or_pending != 0);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x235c010; original cards/theros_beyond_death.c:1142. */
int card_destiny_spinner(int player, int card, event_t event)
{
  /* Destiny Spinner	|1|G
   * Enchantment Creature - Human 2/3
   * Creature and enchantment spells you control can't be countered.
   * |3|G: Target land you control becomes an X/X Elemental creature with trample and haste until end of turn, where X is the number of enchantments you control. It's still a land. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  result = is_humiliated(player, card, AL_STATIC);
  if (result == 0)
  {
    type_uncounterable(player, card, event, player, 6, NULL);
  }
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 1);
    td.allowed_controller = player;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      result = global_enchantment(player, card, event);
      return result;
    }
    default_target_definition(player, card, &td, 1);
    td.allowed_controller = player;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = valid_target(&td);
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        result = count_permanents_by_type(player, TYPE_ENCHANTMENT);
        result = land_animation2(instance->parent_controller, instance->parent_card,
                                 instance->targets[0].player, instance->targets[0].card, 1, result,
                                 result, 0x80, 4, 0, NULL);
        instance = get_card_instance(player, result);
        instance->targets[9].card = 0x3a;
      }
    }
  }
  result = generic_activated_ability(player, card, event, 6, 3, 0, 0, 1, 0, 0, 0, &td,
                                     "Select target land you control.");
  return result;
}

/* ManalinkEh 2022: 0x235c210; original cards/theros_beyond_death.c:1205. */
int card_dream_trawler(int player, int card, event_t event)
{
  /* Dream Trawler	|2|W|W|U|U
   * Creature - Sphinx 3/5
   * Flying, lifelink
   * Whenever you draw a card, this creature gets +1/+0 until end of turn.
   * Whenever this creature attacks, draw a card.
   * Discard a card: This creature gains hexproof until end of turn. Tap it. */

  lifelink(player, card, event);
  if (card_drawn_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY))
    pump_until_eot_merge_previous(player, card, player, card, 1, 0);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    draw_a_card(player);
    return 0;
  }

  switch (event)
  {
  case EVENT_ACTIVATE:
    get_card_instance(player, card)->info_slot = 0x42;
    break;

  case EVENT_RESOLVE_ACTIVATION:
  {
    card_instance_t *activation = get_card_instance(player, card);
    int parent_player = activation->parent_controller;
    int parent_card = activation->parent_card;
    pump_ability_until_eot_no_repeat(player, card, parent_player, parent_card, 0,
                                   SP_KEYWORD_HEXPROOF);
    tap_card(parent_player, parent_card);
    if (get_card_instance(parent_player, parent_card)->info_slot == 0x42)
      ai_modifier -= 20;
    if (check_for_special_ability(parent_player, parent_card, SP_KEYWORD_HEXPROOF))
      ai_modifier -= 50;
    get_card_instance(parent_player, parent_card)->info_slot = 0;
    break;
  }

  case EVENT_CAN_ACTIVATE:
  case EVENT_CLEANUP:
    break;

  default:
    // The release's static-effects reset is behind incompatible event guards.
    return 0;
  }
  return generic_activated_ability(player, card, event, GAA_DISCARD, 0, 0, 0, 0, 0, 0,
                                   0, NULL, NULL);
}

/* ManalinkEh 2022: 0x235c4a0; original cards/theros_beyond_death.c:1264. */
int card_dryad_of_the_ilysian_grove(int player, int card, event_t event)
{
  /* Dryad of the Ilysian Grove	|2|G
   * Enchantment Creature - Nymph Dryad 2/4
   * You may play an additional land on each of your turns.
   * Lands you control are every basic land type in addition to their other types. */

  check_playable_lands(player);
  all_lands_are_basiclandtype(player, card, event, player, 1, 0x5009);
  all_lands_are_basiclandtype(player, card, event, player, 2, 0x5002);
  all_lands_are_basiclandtype(player, card, event, player, 3, 0x5001);
  all_lands_are_basiclandtype(player, card, event, player, 4, 0x5006);
  all_lands_are_basiclandtype(player, card, event, player, 5, 0x5007);
  if (event == EVENT_RESOLVE_SPELL)
  {
    recalculate_all_cards_in_play();
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2359750; original cards/theros_beyond_death.c:1319. */
static void elspeth_counters(int s_player, int s_card, int t_player, int t_card)
{
  counter_t type;
  if (is_planeswalker(t_player, t_card))
    type = COUNTER_LOYALTY;
  else if (is_what(t_player, t_card, 2))
    type = COUNTER_P1_P1;
  else
    return;
  hack_silent_counters = 1;
  add_counters(s_player, s_card, t_player, t_card, type, 1);
  hack_silent_counters = 0;
}

/* ManalinkEh 2022: 0x235c590; original cards/theros_beyond_death.c:1331. */
int card_elspeth_conquers_death(int player, int card, event_t event)
{
  /* Elspeth Conquers Death	|3|W|W
   * Enchantment - Saga
   * (As this Saga enters and after your draw step, add a lore counter. Sacrifice after III.)
   * I - Exile target permanent an opponent controls with mana value 3 or greater.
   * II - Noncreature spells your opponents cast cost |2 more to cast until your next turn.
   * III - Return target creature or planeswalker card from your graveyard to the battlefield. Put a +1/+1 counter or a loyalty counter on it. */

  int result;
  card_instance_t *instance;


  if (event == EVENT_CAN_SKIP_TURN)
  {
    if (current_turn == player)
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 0;
    }
  }
  else if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if (1 - player == affected_card_controller)
    {
      instance = get_card_instance(player, card);
      if (instance->info_slot == 0x42)
      {
        result = is_what(affected_card_controller, affected_card, 1);
        if (result == 0)
        {
          result = is_what(affected_card_controller, affected_card, 2);
          if (result == 0)
          {
            COST_COLORLESS = COST_COLORLESS + '\x02';
          }
        }
      }
    }
  }
  else if (event == EVENT_CHAPTER1)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x47);
    td.allowed_controller = 1 - player;
    td.special = TARGET_SPECIAL_CMC_GREATER_OR_EQUAL;
    td.extra = 3;
    td.allow_cancel = 0;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(
          &td,
          "Select target permanent an opponent controls with converted mana cost 3 or greater.", 0,
          0x81);
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
      }
    }
  }
  else if (event == EVENT_CHAPTER2)
  {
    instance = get_card_instance(player, card);
    instance->info_slot = 0x42;
  }
  else if (event == EVENT_CHAPTER3)
  {
    test_definition_t test;
    new_default_test_definition(&test, 0x102,
                                "Select target creature or planeswalker card.");
    result = select_target_from_grave_source(player, card, player, SFG_0, 2,
                                             &test, 0);
    if (result != 0)
    {

      result = validate_target_from_grave_source(player, card, player, 0);
      if (result != -1)
      {
        reanimate_permanent_with_function(player, card, player, result, REANIMATE_DEFAULT,
                                          elspeth_counters);
      }
    }
  }
  result = saga(player, card, event, 3);
  return result;
}

/* ManalinkEh 2022: 0x235c830; original cards/theros_beyond_death.c:1386. */
int card_elspeths_nightmare(int player, int card, event_t event)
{
  /* Elspeth's Nightmare	|2|B
   * Enchantment - Saga
   * (As this Saga enters and after your draw step, add a lore counter. Sacrifice after III.)
   * I - Destroy target creature an opponent controls with power 2 or less.
   * II - Target opponent reveals their hand. You choose a noncreature, nonland card from it. That player discards that card.
   * III - Exile target opponent's graveyard. */

  int result;
  card_instance_t *instance;
  ec_definition_t ec;


  if (event == EVENT_CHAPTER1)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - player;
    td.power_requirement = 0x2002;
    td.allow_cancel = 0;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td,
                               "Select target creature an opponent controls with power 2 or less.",
                               0, 0x81);
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
    }
  }
  else if (event == EVENT_CHAPTER2)
  {
    result = target_opponent(player, card);
    if (result != 0)
    {
      test_definition_t this_test;
      default_ec_definition(1 - player, player, &ec);
      new_default_test_definition(&this_test, 3,
                                  "Select a nonland, noncreature card.");
      this_test.type_flag = DOESNT_MATCH;
      new_effect_coercion(&ec, &this_test);
    }
  }
  else if (event == EVENT_CHAPTER3)
  {
    result = target_opponent(player, card);
    if (result != 0)
    {
      rfg_whole_graveyard(1 - player);
    }
  }
  result = saga(player, card, event, 3);
  return result;
}

/* ManalinkEh 2022: 0x235c9d0; original cards/theros_beyond_death.c:1428. */
int card_elspeth_suns_nemesis(int player, int card, event_t event)
{
  /* Elspeth, Sun's Nemesis	|2|W|W
   * Legendary Planeswalker - Elspeth (5)
   * -1: Up to two target creatures you control each get +2/+1 until end of turn.
   * -2: Create two 1/1 white Human Soldier creature tokens.
   * -3: You gain 5 life.
   * Escape-|4|W|W, Exile four other cards from your graveyard. (You may cast this card from your graveyard for its escape cost.) */

  enum
  {
    CHOICE_PUMP = 1,
    CHOICE_TOKENS = 2,
    CHOICE_LIFE = 3
  };

  int i;
  target_t *first_target;
  card_instance_t *instance;
  int mode_or_target_count;
  int available_or_selected;
  target_t *target_cursor;
  target_t *next_target;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS))
  {
    return escape(player, card, event, 4, 0, 0, 0, 0, 2, 4);
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allow_cancel = 3;
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      mode_or_target_count = dialog_impl(
          player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Pump creatures", 1, 10, 0xffffffff,
          "Create tokens", 1, 10, 0xfffffffe, "Gain life", 1, 10, 0xfffffffd, 0, 0, 0, 0);
      if (mode_or_target_count == 0)
      {
        spell_fizzled = 1;
        return 0;
      }

      if ((mode_or_target_count == CHOICE_PUMP) &&
          (mode_or_target_count = can_target(&td), mode_or_target_count != 0))
      {
        first_target = instance->targets;
        if (select_target(player, card, &td, " Select target creature.", first_target) != 0)
        {
          state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
          available_or_selected = can_target(&td);
          mode_or_target_count = 1;
          if ((available_or_selected != 0) &&
              (available_or_selected = select_target(player, card, &td, " Select target creature.",
                                                     instance->targets + 1),
               available_or_selected != 0))
          {
            state_untargettable(instance->targets[1].player, instance->targets[1].card, 1);
            mode_or_target_count = 2;
          }
          target_cursor = first_target;
          do
          {
            next_target = target_cursor + 1;
            state_untargettable(target_cursor->player, target_cursor->card, 0);
            target_cursor = next_target;
          } while (next_target != first_target + mode_or_target_count);
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
    default_target_definition(player, card, &td, 2);
  }
  td.preferred_controller = player;
  td.allow_cancel = 3;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    mode_or_target_count = instance->info_slot;
    if (mode_or_target_count == CHOICE_PUMP)
    {

      if (instance->number_of_targets != '\0')
      {
        i = 0;
        do
        {
          target_cursor = instance->targets + i;
          first_target = instance->targets + i;
          i = i + 1;
          pump_until_eot_merge_previous(player, card, first_target->player, target_cursor->card, 2,
                                        1);
        } while (i < (int)(unsigned int)instance->number_of_targets);
      }
      instance->number_of_targets = '\0';
    }
    else if (mode_or_target_count == CHOICE_TOKENS)
    {
      generate_tokens_by_id(player, card, CARD_ID_SOLDIER, 2);
    }
    else if (mode_or_target_count == CHOICE_LIFE)
    {
      gain_life(player, 5);
    }
  }
  return planeswalker(player, card, event, 5);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x235cdd0; original cards/theros_beyond_death.c:1548. */
int card_ereboss_intervention(int player, int card, event_t event)
{
  /* Erebos's Intervention	|X|B
   * Instant
   * Choose one -
   * * Target creature gets -X/-X until end of turn. You gain X life.
   * * Exile up to twice X target cards from graveyards. */

  target_t *target_cursor;
  card_instance_t *instance;
  int x_or_saved_limit;
  uint32_t selected_mode;
  int grave_id;
  unsigned int grave_mode_available;
  int i;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 4, NULL, NULL, 0, NULL);
  }
  if (event != EVENT_CAST_SPELL)
  {
    if ((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET))
    {
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
      default_target_definition(player, card, &td, 2);
      get_card_instance(player, card);
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if (event != EVENT_RESOLVE_SPELL)
    {
      return 0;
    }

    x_or_saved_limit = instance->info_slot;
    if (instance->eot_toughness == 1)
    {
      pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                     -x_or_saved_limit, -x_or_saved_limit);
      gain_life(player, x_or_saved_limit);
    }
    else
    {

      if ((instance->eot_toughness == 2) && (0 < x_or_saved_limit * 2))
      {
        i = 0;
        do
        {
          grave_id = validate_target_from_grave_source(player, card,
                                                       instance->targets[i].player >> 0x10, i);
          if (grave_id == -1)
            break;
          target_cursor = instance->targets + i;

          i = i + 1;
          rfg_card_from_grave(target_cursor->player >> 0x10, grave_id);
        } while (i != x_or_saved_limit * 2);
      }
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
  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (card != affected_card)
  {
    return 0;
  }
  if (player != affected_card_controller)
  {
    return 0;
  }

  grave_mode_available = 1;
  if (!(count_graveyard(player) != 0 && graveyard_has_shroud(player) != 0))
  {
    x_or_saved_limit = count_graveyard(1 - player);
    grave_mode_available = 0;
    if (x_or_saved_limit != 0)
    {
      x_or_saved_limit = graveyard_has_shroud(1 - player);
      grave_mode_available = x_or_saved_limit == 0;
    }
  }
  selected_mode = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_STORAGE,
                              "Debuff creature & gain life", can_target(&td), 5,
                              "Exile cards from graveyards", grave_mode_available, 5, 0, 0, 0, 0);
  x_or_saved_limit = max_x_value;
  if (selected_mode == 0)
  {
    spell_fizzled = 1;
  }
  else
  {
    if (selected_mode == 1)
    {
      generic_spell(player, card, EVENT_CAST_SPELL, 5, &td, "TARGET_CREATURE", 1, NULL);
      x_or_saved_limit = max_x_value;
    }
    else if (selected_mode == 2)
    {

      max_x_value = 9;
      if (generic_spell(player, card, EVENT_CAST_SPELL, 4, NULL, NULL, 0, NULL) != 0)
      {
        select_multiple_cards_from_either_graveyard(player, card, SFG_0, 3, 1, NULL,
                                                    instance->info_slot * 2, instance->targets);
      }
    }
    max_x_value = x_or_saved_limit;
    instance->eot_toughness = selected_mode;
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x2359220; original cards/theros_beyond_death.c:1616. */
static int erebos_thb_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  card_instance_t *dying;
  int pending_or_choice;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (pending_or_choice =
               get_id((int)instance->damage_target_player, instance->damage_target_card),
           pending_or_choice != 0x5211))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (pending_or_choice = effect_follows_control_of_attachment(player, card, 0x4700),
         pending_or_choice != 0))
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (pending_or_choice = effect_follows_control_of_attachment(player, card, event),
         pending_or_choice != 0))
    {
      return 0;
    }
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {

      instance = get_card_instance(player, card);
      dying = in_play(affected_card_controller, affected_card);
      if ((dying != NULL) &&
          (pending_or_choice = is_this_dying(affected_card_controller, affected_card),
           pending_or_choice != 0))
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
          return 0;
        }
        count_for_gfp_ability(player, card, 0x77, player, 2, NULL);
      }
    }
  }
  pending_or_choice = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_or_choice != 0)
  {
    if (check_status(player, card, 0x10000) == 0)
    {

      if ((char)pending_or_choice != 0)
        while (can_pay_life(player, 2))
        {
          pending_or_choice =
              dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
                          "Pay 2 life", 1, life[player], "Decline", 1, 5, "Done", 1, 5, 0, 0, 0, 0);
          if (pending_or_choice == 3)
            break;
          if (pending_or_choice == 1)
          {
            lose_life(player, 2);
            draw_a_card(player);
          }
        }
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) &&
      (pending_or_choice = check_status(player, card, 0x10000), pending_or_choice == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x235d270; original cards/theros_beyond_death.c:1683. */
int card_erebos_bleak_hearted(int player, int card, event_t event)
{
  /* Erebos, Bleak-Hearted	|3|B
   * Legendary Enchantment Creature - God 5/6
   * Indestructible
   * As long as your devotion to black is less than five, Erebos isn't a creature.
   * Whenever another creature you control dies, you may pay 2 life. If you do, draw a card.
   * |1|B, Sacrifice another creature: Target creature gets -2/-1 until end of turn. */

  int c;
  int created_or_pending;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t test;

  bool legacy_found = false;
  check_legend_rule(player, card, event);
  indestructible(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          instance = in_play(player, c);

          if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
               (instance = get_card_instance(player, c),
                instance->info_slot_fn == erebos_thb_effect)) &&
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
        created_or_pending =
            create_targetted_legacy_effect(player, card, erebos_thb_effect, player, card);
        instance = get_card_instance(player, created_or_pending);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, created_or_pending, 0x10000);
      }
    }
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    default_test_definition(&test, 2);
    test.not_me = 1;
    count_for_gfp_ability(player, card, 0x77, player, 0, &test);
  }
  created_or_pending = duh_mode(player);
  created_or_pending = resolve_gfp_ability(player, card, event,
                                           (created_or_pending != 0) + RESOLVE_TRIGGER_OPTIONAL);

  if (((created_or_pending != 0) &&
       (instance = get_card_instance(player, card), instance->kill_code == '\0')) &&
      ((char)created_or_pending != '\0'))
  {
    while (can_pay_life(player, 2))
    {
      created_or_pending =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
                      "Pay 2 life", 1, life[player], "Decline", 1, 5, "Done", 1, 5, 0, 0, 0, 0);
      if (created_or_pending == 3)
        break;
      if (created_or_pending == 1)
      {
        lose_life(player, 2);
        draw_a_card(player);
      }
    }
  }
  if (event == EVENT_ACTIVATE)
  {
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return thb_god(player, card, event, 1, 0, 5);
    }
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                    instance->targets[0].player, instance->targets[0].card, -2, -1);
    }
  }
  default_target_definition(player, card, &td, 2);
  new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
  test.not_me = 1;
  return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, 1, 1,
                                                          0, 0, 0, 0, 0, &td, "TARGET_CREATURE",
                                                          TYPE_NONE, &test, SARV_0);
}

/* ManalinkEh 2022: 0x235d700; original cards/theros_beyond_death.c:1839. */
int card_funeral_rites(int player, int card, event_t event)
{
  /* Funeral Rites	|2|B
   * Sorcery
   * You draw two cards, lose 2 life, then mill two cards. */

  int result;
  if (event == EVENT_RESOLVE_SPELL)
  {
    draw_cards(player, 2);
    lose_life(player, 2);
    mill(player, 2);
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
  return result;
}

/* ManalinkEh 2022: 0x235d7b0; original cards/theros_beyond_death.c:1911. */
int card_haktos_the_unscarred(int player, int card, event_t event)
{
  /* Haktos the Unscarred	|R|R|W|W
   * Legendary Creature - Human Warrior 6/1
   * Haktos attacks each combat if able.
   * As Haktos enters, choose 2, 3, or 4 at random.
   * Haktos has protection from each mana value other than the chosen number. */

  int result;
  card_instance_t *instance;
  char buffer[100];

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    instance = get_card_instance(player, card);
    result = internal_rand(2);
    instance->info_slot = result + 2;
    sprintf(buffer, " Haktos is susceptible\n to mana cost: %d ", result + 2);
    do_dialog(player, player, card, -1, -1, buffer, 0);
    result = is_humiliated(player, card, AL_STATIC);
    if (result != 0)
    {
      special_abilities2(player, card, EVENT_ETB, SP_KEYWORD2_PROT_MISC);
      return 0;
    }
  }
  else
  {
    result = is_humiliated(player, card, AL_STATIC);
  }
  if (result == 0)
    attack_if_able(player, card, event);
  special_abilities2(player, card, event, SP_KEYWORD2_PROT_MISC);
  if (((event == EVENT_PROT_MISC) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    if (((unsigned int)event_result >> 24 & 0xff) != '\x06')
    {
      result = get_cmc(attacking_card_controller, attacking_card);
      instance = get_card_instance(player, card);
      if (result == instance->info_slot)
      {
        return 0;
      }
    }
    event_result = event_result | 1;
  }
  return 0;
}

/* ManalinkEh 2022: 0x235d970; original cards/theros_beyond_death.c:1951. */
int card_heliods_intervention(int player, int card, event_t event)
{
  /* Heliod's Intervention	|X|W|W
   * Instant
   * Choose one -
   * * Destroy X target artifacts and/or enchantments.
   * * Target player gains twice X life. */

  int i;
  card_instance_t *instance;
  int available_or_saved_max_x;
  uint32_t selected_mode;
  int valid_player;
  target_definition_t td;
  target_definition_t td1;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x44);
    default_target_definition(player, card, &td1, 0);
    td1.zone = TARGET_ZONE_PLAYERS;
    get_card_instance(player, card);
    available_or_saved_max_x = can_target(&td);
    if ((available_or_saved_max_x != 0) ||
        (available_or_saved_max_x = can_target(&td1), available_or_saved_max_x != 0))
    {
      return generic_spell(player, card, EVENT_CAN_CAST, 4, NULL, NULL, 0, NULL);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x44);
      default_target_definition(player, card, &td1, 0);
      td1.zone = TARGET_ZONE_PLAYERS;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {

        available_or_saved_max_x = can_target(&td1);
        selected_mode = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
                                    DLG_NO_STORAGE, "Destroy artifacts & enchantments", can_target(&td),
                                    5, "Gain life", available_or_saved_max_x, 5, 0, 0, 0, 0);
        available_or_saved_max_x = max_x_value;
        if (selected_mode == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (selected_mode == 1)
        {

          max_x_value = target_available(player, card, &td);
          generic_spell(player, card, EVENT_CAST_SPELL, 5, &td, "TARGET_ARTIFACT_ENCHANTMENT",
                        x_value, NULL);
          max_x_value = available_or_saved_max_x;
        }
        else
        {
          max_x_value = available_or_saved_max_x;
          if (selected_mode == 2)
          {
            generic_spell(player, card, EVENT_CAST_SPELL, 5, &td1, "TARGET_PLAYER", 1, NULL);
          }
        }
        instance->eot_toughness = selected_mode;
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x44);
    default_target_definition(player, card, &td1, 0);
    td1.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      available_or_saved_max_x = instance->info_slot;
      if (instance->eot_toughness == 1)
      {

        if (0 < available_or_saved_max_x)
        {
          i = 0;
          do
          {
            if (validate_target(player, card, &td, i) != 0)
            {
              kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
            }
            i = i + 1;
          } while (available_or_saved_max_x != i);
        }
      }
      else if ((instance->eot_toughness == 2) &&
               (valid_player = valid_target(&td1), valid_player != 0))
      {
        gain_life(instance->targets[0].player, available_or_saved_max_x * 2);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x235ddf0; original cards/theros_beyond_death.c:2026. */
int card_heliod_sun_crowned(int player, int card, event_t event)
{
  /* Heliod, Sun-Crowned	|2|W
   * Legendary Enchantment Creature - God 5/5
   * Indestructible
   * As long as your devotion to white is less than five, Heliod isn't a creature.
   * Whenever you gain life, put a +1/+1 counter on target creature or enchantment you control.
   * |1|W: Another target creature gains lifelink until end of turn. */

  int result;
  card_instance_t *heliod;
  card_instance_t *pending_activation;
  target_definition_t *creature_target;
  target_definition_t td;

  check_legend_rule(player, card, event);
  indestructible(player, card, event);
  result = trigger_gain_life(player, card, event, player, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {

    heliod = get_card_instance(player, card);
    heliod->number_of_targets = '\0';
    default_target_definition(player, card, &td, 6);
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.allow_cancel = 0;
    result = can_target(&td);
    if ((result != 0) &&
        (result = new_pick_target(&td, "TARGET_CREATURE_ENCHANTMENT", 0, 1), result != 0))
    {
      add_counters(player, card, heliod->targets[0].player, heliod->targets[0].card, COUNTER_P1_P1,
                   1);
    }
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.special = TARGET_SPECIAL_NOT_ME;
    td.preferred_controller = player;
    heliod = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      result = check_for_special_ability(heliod->targets[0].player, heliod->targets[0].card, 2);
      if ((result == 0) &&
          (((pending_activation = has_activation_on_stack(player, card),
             pending_activation == NULL ||
                 (pending_activation->targets[0].player != heliod->targets[0].player)) ||
            (pending_activation->targets[0].card != heliod->targets[0].card))))
      {
        ai_modifier = ai_modifier + 0x28;
      }
      else
      {
        ai_modifier = ai_modifier + -0x28;
      }
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return thb_god(player, card, event, 5, 0, 5);
    }
    default_target_definition(player, card, &td, 2);
    td.special = TARGET_SPECIAL_NOT_ME;
    td.preferred_controller = player;
    heliod = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    pump_ability_until_eot_no_repeat(heliod->parent_controller, heliod->parent_card,
                                     heliod->targets[0].player, heliod->targets[0].card, 0, 2);
  }
  creature_target = &td;
  return generic_activated_ability(player, card, event, 4, 1, 0, 0, 0, 0, 1, 0, creature_target,
                                   "TARGET_ANOTHER_CREATURE");
}

/* ManalinkEh 2022: 0x235e100; original cards/theros_beyond_death.c:2212. */
int card_kiora_bests_the_sea_god(int player, int card, event_t event)
{
  /* Kiora Bests the Sea God	|5|U|U
   * Enchantment - Saga
   * (As this Saga enters and after your draw step, add a lore counter. Sacrifice after III.)
   * I - Create an 8/8 blue Kraken creature token with hexproof.
   * II - Tap all nonland permanents target opponent controls. They don't untap during their controller's next untap step.
   * III - Gain control of target permanent an opponent controls. Untap it. */

  if (event == EVENT_CHAPTER1)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_KRAKEN, &token);
    token.pow = token.tou = 8;
    token.s_key_plus = SP_KEYWORD_HEXPROOF;
    generate_token(&token);
  }
  else if (event == EVENT_CHAPTER2)
  {
    int opponent = 1-player;
    for (int c = 0; c < active_cards_count[opponent]; ++c)
      if (in_play(opponent, c) && is_what(opponent, c, TYPE_PERMANENT) && !is_what(opponent, c, TYPE_LAND))
        effect_frost_titan(player, card, opponent, c);
  }
  else if (event == EVENT_CHAPTER3)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_PERMANENT);
    td.allowed_controller = 1-player;
    td.allow_cancel = 0;
    if (can_target(&td) && new_pick_target(&td, "Select target permanent an opponent controls.", 0, 0x80))
    {
      int target_card = get_card_instance(player, card)->targets[0].card;
      card_instance_t *instance = get_card_instance(player, card);
      gain_control_permanently(player, instance->targets[0].player, target_card);
      target_card = get_card_instance(player, card)->targets[0].card;
      instance = get_card_instance(player, card);
      untap_card(instance->targets[0].player, target_card);
    }
  }
  return saga(player, card, event, 3);
}

/* ManalinkEh 2022: 0x235e310; original cards/theros_beyond_death.c:2259. */
int card_klothys_god_of_destiny(int player, int card, event_t event)
{
  /* Klothys, God of Destiny	|1|R|G
   * Legendary Enchantment Creature - God 4/5
   * Indestructible
   * As long as your devotion to red and green is less than seven, Klothys isn't a creature.
   * At the beginning of your first main phase, exile target card from a graveyard. If it was a land card, add |R or |G. Otherwise, you gain 2 life and Klothys deals 2 damage to each opponent. */

  int target_player;
  int result;
  card_instance_t *instance;
  unsigned char was_land;
  test_definition_t test;

  check_legend_rule(player, card, event);
  indestructible(player, card, event);
  result = beginning_of_phase(player, card, event, player, card, 0x14);
  if (result != 0)
  {
    new_default_test_definition(&test, 0xff, "Select target card to exile.");
    result = select_target_from_either_grave(player, card, SFG_0, 3, 1, &test, 0, 1);
    if (result != -1)
    {

      instance = get_card_instance(player, card);
      result = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
      if (result != -1)
      {

        target_player = instance->targets[0].player;
        was_land = cards_data[graveyard_ptr_mutable[target_player][result]].type & 1;
        rfg_card_from_grave(target_player, result);
        if (was_land == 0)
        {
          gain_life(player, 2);
          damage_player(1 - player, 2, player, card);
        }
        else
        {
          produce_mana_all_one_color(player, COLOR_TEST_RED | COLOR_TEST_GREEN, 1);
        }
      }
    }
  }
  return thb_god(player, card, event, 4, 3, 7);
}

/* ManalinkEh 2022: 0x235e4d0; original cards/theros_beyond_death.c:2294. */
int card_kroxa_titan_of_deaths_hunger(int player, int card, event_t event)
{
  /* Kroxa, Titan of Death's Hunger	|B|R
   * Legendary Creature - Elder Giant 6/6
   * When Kroxa enters, sacrifice it unless it escaped.
   * Whenever Kroxa enters or attacks, each opponent discards a card, then each opponent who didn't discard a nonland card this way loses 3 life.
   * Escape-|B|B|R|R, Exile five other cards from your graveyard. (You may cast this card from your graveyard for its escape cost.) */

  int discarded_card;
  int opponent;
  test_definition_t test_land;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  new_default_test_definition(&test_land, 1, "Select a land card.");
  test_land.zone = 0x100;
  if (event != EVENT_ETB)
  {
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
    if (event == EVENT_DAT_ABILITY)
    {

      opponent = 1 - player;
      new_default_test_definition(&this_test, 0xff, "Select a card to discard.");
      discarded_card = new_select_a_card(opponent, opponent, 4, 1, 3, -1, &this_test);
      if (discarded_card != -1)
      {
        new_discard_card(opponent, discarded_card, player, 2);
        if (is_what(opponent, discarded_card, 1) == 0)
        {
          return 0;
        }
      }
      lose_life(opponent, 3);
    }
    else if ((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS))
    {
      return escape(player, card, event, 0, 2, 0, 0, 2, 0, 5);
    }
    return 0;
  }

  bool lose_life_needed = true;
  new_default_test_definition(&this_test, 0xff, "Select a card to discard.");
  opponent = 1 - player;
  discarded_card = new_select_a_card(opponent, opponent, 4, 1, 3, -1, &this_test);
  if (discarded_card != -1)
  {
    new_discard_card(opponent, discarded_card, player, 2);
    if (is_what(opponent, discarded_card, 1) == 0)
      lose_life_needed = false;
  }
  if (lose_life_needed)
    lose_life(opponent, 3);
  if (check_special_flags3(player, card, 0x80000) == 0)
  {
    kill_card(player, card, KILL_SACRIFICE);
  }
  dat_ability(player, card, EVENT_ETB, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  return 0;
}

/* ManalinkEh 2022: 0x235e7c0; original cards/theros_beyond_death.c:2338. */
int card_kunoros_hound_of_athreos(int player, int card, event_t event)
{
  /* Kunoros, Hound of Athreos	|1|W|B
   * Legendary Creature - Dog 3/3
   * Vigilance, menace, lifelink
   * Creature cards in graveyards can't enter the battlefield.
   * Players can't cast spells from graveyards. */

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  menace(player, card, event);
  lifelink(player, card, event);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x235e820; original cards/theros_beyond_death.c:2384. */
int card_loathsome_chimera(int player, int card, event_t event)
{
  /* Loathsome Chimera	|2|G
   * Creature - Chimera 4/1
   * Escape-|4|G, Exile three other cards from your graveyard. (You may cast this card from your graveyard for its escape cost.)
   * This creature escapes with a +1/+1 counter on it. */

  int result;
  result = escape_with_counters_impl(player, card, event, 4, 0, 0, 1, 0, 0, 3, 1);
  return result;
}
#endif

/* ManalinkEh 2022: 0x23595a0; original cards/theros_beyond_death.c:2404. */
static int medomais_prophecy_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  test_definition_t test;

  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 0xff);
    instance = get_card_instance(player, card);
    test.id = instance->targets[0].card;
    sp_ability_trigger(1 - player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_SPA)
  {
    draw_cards(player, 2);
    kill_card(player, card, KILL_EXILE);
  }
  if (!eot_trigger(player, card, event))
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x235e870; original cards/theros_beyond_death.c:2423. */
int card_medomais_prophecy(int player, int card, event_t event)
{
  /* Medomai's Prophecy	|1|U
   * Enchantment - Saga
   * (As this Saga enters and after your draw step, add a lore counter. Sacrifice after IV.)
   * I - Scry 2.
   * II - Choose a card name.
   * III - When you cast a spell with the chosen name for the first time this turn, draw two cards.
   * IV - Look at the top card of each player's library. */

  int result;
  card_instance_t *saga_record;
  card_instance_t *draw_effect;
  test_definition_t test;

  if (event == EVENT_CHAPTER1)
  {
    scry(player, 2);
  }
  else if (event == EVENT_CHAPTER2)
  {
    default_test_definition(&test, 0xff);
    saga_record = get_card_instance(player, card);
    result = new_card_from_list(player, 3, &test);
    saga_record->targets[0].card = result;
  }
  else if (event == EVENT_CHAPTER3)
  {

    result = create_legacy_effect(player, card, medomais_prophecy_legacy);
    saga_record = get_card_instance(player, card);
    draw_effect = get_card_instance(player, result);
    draw_effect->targets[0].card = saga_record->targets[0].card;
    saga_record = get_card_instance(player, card);
    saga_record->targets[0].card = -1;
  }
  else if (event == EVENT_CHAPTER4)
  {
    show_deck(player, deck_ptr[player], 1, "Here\'s the top card of your library.", 0,
              gs_done_008b40e0);
    show_deck(player, deck_ptr[1 - player], 1, "Here\'s the top card of your opponent\'s library.",
              0, gs_done_008b40e0);
  }
  return saga(player, card, event, 4);
}

/* ManalinkEh 2022: 0x235ea20; original cards/theros_beyond_death.c:2481. */
int card_mire_triton(int player, int card, event_t event)
{
  /* Mire Triton	|1|B
   * Creature - Zombie Merfolk 2/1
   * Deathtouch
   * When this creature enters, mill two cards and you gain 2 life. (To mill two cards, put the top two cards of your library into your graveyard.) */

  deathtouch(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  mill(player, 2);
  gain_life(player, 2);
  return 0;
}

/* ManalinkEh 2022: 0x235eaa0; original cards/theros_beyond_death.c:2613. */
int card_nyleas_intervention(int player, int card, event_t event)
{
  /* Nylea's Intervention	|X|G|G
   * Sorcery
   * Choose one -
   * * Search your library for up to X land cards, reveal them, put them into your hand, then shuffle.
   * * ~ deals twice X damage to each creature with flying. */

  card_instance_t *instance;
  uint32_t selected_mode;
  int result;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 4, NULL, NULL, 0, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (instance = get_card_instance(player, card), card == affected_card)) &&
        (player == affected_card_controller))
    {

      selected_mode = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                                  "Search for lands", 1, 5, "Damage flyers", 1, 5, 0, 0, 0, 0);
      if (selected_mode == 0)
      {
        spell_fizzled = 1;
      }
      else
      {
        generic_spell(player, card, EVENT_CAST_SPELL, 4, NULL, NULL, 0, NULL);
        instance->eot_toughness = selected_mode;
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      result = instance->info_slot;
      if ((instance->eot_toughness != 1) || (result < 1))
      {
        if ((instance->eot_toughness == 2) && (result >= 1))
        {
          default_test_definition(&test, 2);
          test.keyword = 0x20;
          new_damage_all(player, card, 2, result * 2, 0, &test);
        }
      }
      else
      {
        new_default_test_definition(&test, 1, "Select a land card");
        test.qty = (unsigned short)result;
        new_global_tutor(player, player, 1, 1, 0, 1, &test);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x235ed80; original cards/theros_beyond_death.c:2663. */
int card_nylea_keen_eyed(int player, int card, event_t event)
{
  /* Nylea, Keen-Eyed	|3|G
   * Legendary Enchantment Creature - God 5/6
   * Indestructible
   * As long as your devotion to green is less than five, Nylea isn't a creature.
   * Creature spells you cast cost |1 less to cast.
   * |2|G: Reveal the top card of your library. If it's a creature card, put it into your hand. Otherwise, you may put it into your graveyard. */

  int *deck;
  int result;

  check_legend_rule(player, card, event);
  indestructible(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if ((affected_card_controller == player) &&
        (result = is_what(player, affected_card, 2), result != 0))
    {
      COST_COLORLESS = COST_COLORLESS + -1;
    }
    return thb_god(player, card, event, 3, 0, 5);
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return thb_god(player, card, event, 3, 0, 5);
  if (event != EVENT_RESOLVE_ACTIVATION)
    return generic_activated_ability(player, card, event, 0, 2, 0, 0, 1, 0, 0, 0, NULL, NULL);
  deck = deck_ptr[player];
  if (*deck != -1)
  {

    show_deck(current_turn, deck, 1, "Here\'s the first card of deck.", 0, gs_done_008b40e0);
    show_deck(current_turn ^ 1, deck, 1, "Here\'s the first card of deck.", 0, gs_done_008b40e0);
    result = is_what(-1, *deck, 2);
    if (result == 0)
    {
      result = dialog_impl(player, card, EVENT_RESOLVE_ACTIVATION, DLG_RANDOM, DLG_NO_CANCEL,
                           DLG_NO_STORAGE, DLG_SMALLCARD_ID(*deck), "Put the card in your graveyard,", 1, 5,
                           "Pass", 1, 5, 0, 0, 0, 0);
      if (result == 1)
      {
        mill(player, 1);
      }
    }
    else
    {
      add_card_to_hand(player, *deck);
      remove_card_from_deck(player, 0);
    }
  }
  return generic_activated_ability(player, card, event, 0, 2, 0, 0, 1, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x235f030; original cards/theros_beyond_death.c:2711. */
int card_nyx_lotus(int player, int card, event_t event)
{
  /* Nyx Lotus	|4
   * Legendary Artifact
   * ~ enters tapped.
   * |T: Choose a color. Add an amount of mana of that color equal to your devotion to that color. (Your devotion to a color is the number of mana symbols of that color in the mana costs of permanents you control.) */

  card_instance_t *instance;
  int highest_devotion;
  int color_devotion;
  int clr1;

  comes_into_play_tapped(player, card, event);
  if (event == EVENT_COUNT_MANA)
  {
    clr1 = 1;
    highest_devotion = 0;
    do
    {
      color_devotion = devotion(player, -1, clr1, 0);
      if (highest_devotion < color_devotion)
      {
        highest_devotion = color_devotion;
      }
      clr1 = clr1 + 1;
    } while (clr1 != 6);
    instance = get_card_instance(player, card);
    instance->info_slot = highest_devotion;
  }
  instance = get_card_instance(player, card);
  return artifact_mana_all_one_color(player, card, event, instance->info_slot, 0);
}

/* ManalinkEh 2022: 0x235f0e0; original cards/theros_beyond_death.c:2732. */
int card_nyxbloom_ancient(int player, int card, event_t event)
{
  /* Nyxbloom Ancient	|4|G|G|G
   * Enchantment Creature - Elemental 5/5
   * Trample
   * If you tap a permanent for mana, it produces three times as much of that mana instead. */

  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if ((instance != NULL) &&
      (result = is_humiliated(player, card, AL_STATIC), result != 0))
  {
    return 0;
  }
  if (event == EVENT_TAP_CARD)
  {
    if (affected_card_controller == player)
    {

      result = is_what(player, affected_card, 1);
      if ((result != 0) && (multi_mana_transmutation[player][0] != 0))
      {
        return 0;
      }
      if (6 < tapped_for_mana_color)
      {
        if (tapped_for_mana_color != 0x100)
        {
          return 0;
        }
        result = 0;
        do
        {
          if (0 < tapped_for_mana[result])
          {
            produce_mana(affected_card_controller, result, tapped_for_mana[result] * 2);
            tapped_for_mana[result] = tapped_for_mana[result] * 3;
          }
          result = result + 1;
        } while (result != 7);
        return 0;
      }
      if (tapped_for_mana_color < 0)
      {
        return 0;
      }
      produce_mana(affected_card_controller, tapped_for_mana_color, 2);
      result = tapped_for_mana_color;
      tapped_for_mana[0] = 0;
      tapped_for_mana[1] = 0;
      tapped_for_mana[2] = 0;
      tapped_for_mana[3] = 0;
      tapped_for_mana[4] = 0;
      tapped_for_mana[5] = 0;
      tapped_for_mana[6] = 0;
      tapped_for_mana_color = 0x100;
      tapped_for_mana[result] = 3;
      return 0;
    }
  }
  else if ((((event == EVENT_COUNT_MANA) && (affected_card_controller == player)) &&
            (result = is_tapped(player, affected_card), result == 0)) &&
           ((result = is_animated_and_sick(affected_card_controller, affected_card),
             result == 0 && (result = can_produce_mana(affected_card_controller, affected_card),
                             result != 0))))
  {

    result = is_what(affected_card_controller, affected_card, 1);
    if ((result != 0) && (multi_mana_transmutation[player][0] != 0))
    {
      return 0;
    }
    instance = get_card_instance(affected_card_controller, affected_card);
    if ((cards_at_7c7000[instance->internal_card_id]->extra_ability & 0x1000) != 0)
    {
      call_card_function(affected_card_controller, affected_card, EVENT_COUNT_MANA);
      call_card_function(affected_card_controller, affected_card, EVENT_COUNT_MANA);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x235f370; original cards/theros_beyond_death.c:2870. */
int card_one_with_the_stars(int player, int card, event_t event)
{
  /* One with the Stars	|3|U
   * Enchantment - Aura
   * Enchant creature or enchantment
   * Enchanted permanent is an enchantment and loses all other card types. (It still has its abilities, but it's no longer a creature.) */

  card_instance_t *instance;
  unsigned char original_type;
  int argument1;
  card_data_t *new_type_data;
  unsigned char enchantment_type;
  card_instance_t *aura_record;
  int attached_card_or_planeswalker;
  card_instance_t *source_in_play;
  int new_iid_or_result;
  int target_player;
  target_definition_t td;

  aura_record = in_play(player, card);
  if (aura_record != NULL &&
      (aura_record = get_card_instance(player, card), aura_record->damage_target_player >= 0) &&
      !is_humiliated(player, card, AL_STATIC))
  {
    instance = get_card_instance(player, card);
    target_player = (int)instance->damage_target_player;
    attached_card_or_planeswalker = instance->damage_target_card;
    instance = get_card_instance(target_player, attached_card_or_planeswalker);
    if (event == EVENT_RESOLVE_SPELL)
      instance->regen_status |= 0x1000000;
    else if (event == EVENT_CHANGE_TYPE)
    {
      source_in_play = in_play(player, card);
      new_iid_or_result = event_result;
      if (((source_in_play != NULL) && ((attached_card_or_planeswalker == affected_card &&
                                         (target_player == affected_card_controller)))) &&
          ((((unsigned int)land_can_be_played >> 16 & 0xff) & 2) == 0))
      {
        argument1 = instance->internal_card_id;
        original_type = cards_data[argument1].type;
        enchantment_type = original_type & 0xb8 | 4;
        attached_card_or_planeswalker = is_planeswalker(target_player, attached_card_or_planeswalker);
        if ((original_type == enchantment_type) && (attached_card_or_planeswalker == 0))
        {
          return 0;
        }
        if ((instance->targets[7].player != argument1) ||
            (new_iid_or_result = instance->dummy3,
             cards_data[instance->dummy3].type != enchantment_type))
        {
          new_iid_or_result = create_a_card_type(argument1);
          if (new_iid_or_result == -1)
          {
            return 0;
          }
          new_type_data = cards_at_7c7000[new_iid_or_result];
          new_type_data->type = enchantment_type;
          if (attached_card_or_planeswalker != 0)
          {
            new_type_data->cc[2] = '\0';
          }
          instance->regen_status = instance->regen_status | 0x1000000;
          instance->targets[7].player = argument1;
          instance->dummy3 = new_iid_or_result;
        }
      }
      event_result = new_iid_or_result;
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
  default_target_definition(player, card, &td, 6);
  return disabling_targeted_aura(player, card, event, &td, "Select target creature or enchantment.",
                                 0);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x235f610; original cards/theros_beyond_death.c:2937. */
int card_ox_of_agonas(int player, int card, event_t event)
{
  /* Ox of Agonas	|3|R|R
   * Creature - Ox 4/2
   * When this creature enters, discard your hand, then draw three cards.
   * Escape-|R|R, Exile eight other cards from your graveyard. (You may cast this card from your graveyard for its escape cost.)
   * This creature escapes with a +1/+1 counter on it. */

  int result;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    discard_all(player);
    draw_cards(player, 3);
  }
  result = escape_with_counters_impl(player, card, event, 0, 0, 0, 0, 2, 0, 8, 1);
  return result;
}
#endif

/* ManalinkEh 2022: 0x235f6b0; original cards/theros_beyond_death.c:3004. */
int card_polukranos_unchained(int player, int card, event_t event)
{
  /* Polukranos, Unchained	|2|B|G
   * Legendary Creature - Zombie Hydra 0/0
   * Polukranos enters with six +1/+1 counters on it. It escapes with twelve +1/+1 counters on it instead.
   * If damage would be dealt to Polukranos while it has a +1/+1 counter on it, prevent that damage and remove that many +1/+1 counters from it.
   * |1|B|G: Polukranos fights another target creature.
   * Escape-|4|B|G, Exile six other cards from your graveyard. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  check_legend_rule(player, card, event);
  result = check_special_flags3(player, card, 0x80000);
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1,
                                       ((result == 0) ? 6U : 12U));
  if (event == EVENT_DEAL_DAMAGE)
  {
    result = is_humiliated(player, card, AL_STATIC);

    if (((((result == 0) && (result = count_counters(player, card, COUNTER_P1_P1), result != 0)) &&
          (instance = get_card_instance(affected_card_controller, affected_card),
           instance->internal_card_id == damage_card)) &&
         ((instance->damage_target_card == card && (instance->damage_target_player == player)))) &&
        (0 < instance->info_slot))
    {

      result = count_counters(player, card, COUNTER_P1_P1);
      result = prevent_damage(affected_card_controller, affected_card, result);
      remove_counters(player, card, player, card, COUNTER_P1_P1, result);
    }
  }
  else
  {
    if ((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS))
    {
      return escape(player, card, event, 4, 1, 0, 1, 0, 0, 6);
    }
    if (event == EVENT_ACTIVATE)
    {
      default_target_definition(player, card, &td, 2);
      td.special = TARGET_SPECIAL_NOT_ME;
      return generic_activated_ability(player, card, event, 4, 1, 1, 0, 1, 0, 0, 0, &td,
                                       "SELECT_ANOTHER_CREATURE");
    }
  }
  if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.special = TARGET_SPECIAL_NOT_ME;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    fight(instance->parent_controller, instance->parent_card, instance->targets[0].player,
          instance->targets[0].card);
  }
  return generic_activated_ability(player, card, event, 4, 1, 1, 0, 1, 0, 0, 0, &td,
                                   "SELECT_ANOTHER_CREATURE");
}

/* ManalinkEh 2022: 0x235f990; original cards/theros_beyond_death.c:3053. */
int card_purphoross_intervention(int player, int card, event_t event)
{
  /* Purphoros's Intervention	|X|R
   * Sorcery
   * Choose one -
   * * Create an X/1 red Elemental creature token with trample and haste. Sacrifice it at the beginning of the next end step.
   * * ~ deals twice X damage to target creature or planeswalker. */

  card_instance_t *instance;
  int damage_or_power;
  uint32_t selected_mode;
  token_generation_t token;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x102);
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 4, NULL, NULL, 0, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x102);
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {

        selected_mode =
            dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_STORAGE,
                        "Create a token", 1, 5, "Damage target", can_target(&td), 5, 0, 0, 0, 0);
        if (selected_mode == 0)
        {
          spell_fizzled = 1;
        }
        else
        {
          if (selected_mode == 1)
          {
            generic_spell(player, card, EVENT_CAST_SPELL, 4, NULL, NULL, 0, NULL);
          }
          else if (selected_mode == 2)
          {
            generic_spell(player, card, EVENT_CAST_SPELL, 5, &td, "TARGET_CREATURE_OR_PLANESWALKER",
                          1, NULL);
          }
          instance->eot_toughness = selected_mode;
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x102);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      damage_or_power = instance->info_slot;
      if ((instance->eot_toughness != 1) || (damage_or_power < 1))
      {
        if ((instance->eot_toughness == 2) && ((damage_or_power >= 1 && (valid_target(&td) != 0))))
        {
          damage_target0(player, card, damage_or_power * 2);
        }
      }
      else
      {
        default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
        token.tou = 1;
        token.key_plus = 0x80;
        token.special_code_on_generation = haste_and_sacrifice_token_at_end_of_next_turn;
        token.pow = damage_or_power;
        generate_token(&token);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 0x102);
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23597f0; original cards/theros_beyond_death.c:3111. */
static int sacrifice_eot(int player, int card, event_t event)
{
  int turns_or_attached_card;
  card_instance_t *instance;
  int target_player;

  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {

    instance = get_card_instance(player, card);
    if ((((trigger_condition == TRIGGER_EOT) && (card == affected_card)) &&
         (player == affected_card_controller)) &&
        (player == reason_for_trigger_controller))
    {
      turns_or_attached_card = instance->targets[1].player;
      if (turns_or_attached_card < 1)
      {
        if (event == EVENT_TRIGGER)
        {
          event_result = event_result | 2;
        }
        else if (event == EVENT_RESOLVE_TRIGGER)
        {
          target_player = (int)instance->damage_target_player;
          turns_or_attached_card = instance->damage_target_card;
          if (check_state(target_player, turns_or_attached_card, STATE_OUBLIETTED) == 0)
          {
            if (target_player == instance->targets[0].player)
            {
              kill_card(target_player, turns_or_attached_card, KILL_SACRIFICE);
            }
          }
          else
          {
            kill_card(player, card, KILL_EXILE);
          }
        }
      }
      else
      {
        instance->targets[1].player = turns_or_attached_card + -1;
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
static int sac_at_beginning_of_next_end_step(int player, int card, int t_player, int t_card)
{
  int legacy = create_targetted_legacy_effect(player, card, sacrifice_eot, t_player, t_card);
  get_card_instance(player, legacy)->number_of_targets = 0;
  if (current_phase == 0x1f && legacy >= 0)
    get_card_instance(player, legacy)->targets[1].player = 1;
  return legacy;
}
#endif

/* ManalinkEh 2022: 0x23596d0; original cards/theros_beyond_death.c:3154. */
static int purphoros_test(int iid, int color, int player, int card)
{
  unsigned int creature_color;
  unsigned int matches;
  int result;

  creature_color = get_color_by_internal_id(player, iid);
  matches = 1;
  if ((color & creature_color) == 0)
  {
    result = is_artifact_creature_by_internal_id(iid, 0, 0, 0);
    matches = (unsigned int)(result != 0);
  }
  return matches;
}

/* ManalinkEh 2022: 0x235fd40; original cards/theros_beyond_death.c:3162. */
int card_purphoros_bronze_blooded(int player, int card, event_t event)
{
  /* Purphoros, Bronze-Blooded	|4|R
   * Legendary Enchantment Creature - God 7/6
   * Indestructible
   * As long as your devotion to red is less than five, Purphoros isn't a creature.
   * Other creatures you control have haste.
   * |2|R: You may put a red creature card or an artifact creature card from your hand onto the battlefield. Sacrifice it at the beginning of the next end step. */

  int parent_controller;
  int result;
  const char *msg;
  card_instance_t *instance;
  bool end_step;
  test_definition_t test;

  check_legend_rule(player, card, event);
  indestructible(player, card, event);
  boost_subtype(player, card, event, ~SUBTYPE_INVALID, 0, 0, 0, 4, BCT_CONTROLLER_ONLY);
  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return thb_god(player, card, event, 4, 0, 5);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      msg = get_sleighted_color_text(player, card,
                                     "Select a %s creature card or artifact creature card", 4);
      new_default_test_definition(&test, 2, msg);
      test.special_selection_function = purphoros_test;
      test.value_for_special_selection_function = get_sleighted_color_test(player, card, 0x10);
      result = new_global_tutor(player, player, 4, 4, 0, 2, &test);
      if (-1 < result)
      {

        instance = get_card_instance(player, card);

        parent_controller = instance->parent_controller;
        result = create_targetted_legacy_effect(parent_controller, instance->parent_card, sacrifice_eot,
                                                parent_controller, result);
        instance = get_card_instance(parent_controller, result);
        end_step = current_phase == 0x1f;
        instance->number_of_targets = '\0';
        if (end_step)
        {
          if (-1 < result)
          {
            instance = get_card_instance(parent_controller, result);
            instance->targets[1].player = 1;
          }
        }
      }
    }
  }
  return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 1, 0, 0, NULL, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x235ffa0; original cards/theros_beyond_death.c:3211. */
int card_renata_called_to_the_hunt(int player, int card, event_t event)
{
  /* Renata, Called to the Hunt	|2|G|G
   * Legendary Enchantment Creature - Demigod * /3
   * Renata's power is equal to your devotion to green. (Each |G in the mana costs of permanents you control counts toward your devotion to green.)
   * Each other creature you control enters with an additional +1/+1 counter on it. */

  int result;
  card_instance_t *instance;
  test_definition_t test;
  check_legend_rule(player, card, event);
  if (event == EVENT_POWER)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = devotion(player, card, 3, 0);
      set_characteristic_pt(result);
    }
  }
  else if ((event == EVENT_CAST_SPELL) || (event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        default_test_definition(&test, 2);
        test.not_me = 1;
        test_enters_the_battlefield_with_counters(player, card, event, player, &test,
                                                       COUNTER_P1_P1, 1);
      }
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23600c0; original cards/theros_beyond_death.c:3339. */
int card_shadowspear(int player, int card, event_t event)
{
  /* Shadowspear	|1
   * Legendary Artifact - Equipment
   * Equipped creature gets +1/+1 and has trample and lifelink.
   * |1: Permanents your opponents control lose hexproof and indestructible until end of turn.
   * Equip |2 */

  enum
  {
    CHOICE_EQUIP = 1,
    CHOICE_CRIPPLE = 2
  };

  int choice;
  card_instance_t *instance;
  int equip_available;

  check_legend_rule(player, card, event);
  instance = in_play(player, card);
  if ((((instance == NULL) ||
        (equip_available = is_humiliated(player, card, AL_STATIC), equip_available != 0)) ||
       (instance = is_equipping(player, card), instance == NULL)) ||
      (instance = get_card_instance(player, card), 2 < (uint32_t)(event - EVENT_POWER)))
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0,
                                      NULL, NULL) == 0)
        {
          return basic_equipment(player, card, EVENT_CAN_ACTIVATE, 2);
        }
        return 1;
      }
      if (event == EVENT_ACTIVATE)
      {
        equip_available = can_activate_basic_equipment(player, card, EVENT_ACTIVATE, 2);
        choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Equip", equip_available, 5,
                             "Cripple opponent's creatures",
                             generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0,
                                                       0, 0, 0, 0, NULL, NULL),
                             8, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (choice != CHOICE_EQUIP)
        {
          if (choice != CHOICE_CRIPPLE)
          {
            return 0;
          }
          ai_modifier = ai_modifier + (unsigned int)((1 - player == current_turn) ? 0 : -24) + 0x30;
          return generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0,
                                           NULL, NULL);
        }
        return basic_equipment(player, card, EVENT_ACTIVATE, 2);
      }
    }
  }
  else if ((instance->damage_target_card == affected_card) &&
           (instance->damage_target_player == affected_card_controller))
  {
    return vanilla_equipment(player, card, event, 2, 1, 1, 0x80, 2);
  }
  if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->info_slot == 1)
    {
      return basic_equipment(player, card, EVENT_RESOLVE_ACTIVATION, 2);
    }
    if (instance->info_slot == 2)
    {
      negate_ability_and_special_ability_until_eot(
          instance->parent_controller, instance->parent_card, 1 - instance->parent_controller, -1,
          KEYWORD_0, (SP_KEYWORD_HEXPROOF | SP_KEYWORD_INDESTRUCTIBLE));
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x23604d0; original cards/theros_beyond_death.c:3482. */
int card_soul_guide_lantern(int player, int card, event_t event)
{
  /* Soul-Guide Lantern	|1
   * Artifact
   * When this artifact enters, exile target card from a graveyard.
   * |T, Sacrifice this artifact: Exile each opponent's graveyard.
   * |1, |T, Sacrifice this artifact: Draw a card. */

  enum
  {
    CHOICE_EXILE = 1,
    CHOICE_DRAW = 2
  };

  int choice;
  int result_or_mode;
  int draw_available;
  card_instance_t *instance;
  card_instance_t *lantern;
  test_definition_t test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    if ((*graveyard_ptr_mutable[player] == -1) ||
        (result_or_mode = graveyard_has_shroud(player), result_or_mode != 0))
    {
      if (count_graveyard_by_type(1 - player, 2) == 0)
      {
        return 0;
      }
      if (graveyard_has_shroud(1 - player) != 0)
      {
        return 0;
      }
    }
    new_default_test_definition(&test, 0xff, "Select target card.");
    result_or_mode =
        select_target_from_either_grave(player, card, SFG_CANNOT_CANCEL, 3, 1, &test, 0, 1);
    if (result_or_mode != -1)
    {

      instance = get_card_instance(player, card);
      result_or_mode =
          validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
      if (result_or_mode != -1)
      {
        rfg_card_from_grave(instance->targets[0].player, result_or_mode);
      }
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      lantern = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9, 0, 0, 0, 0, 0, 0, 0,
                                      NULL, NULL) == 0)
        {
          result_or_mode = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9, 1, 0, 0,
                                                     0, 0, 0, 0, NULL, NULL);
          return (unsigned int)(result_or_mode != 0);
        }
        return 1;
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      {
        return 0;
      }
      lantern = get_card_instance(player, card);
    }
    result_or_mode = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9, 0, 0, 0, 0, 0,
                                               0, 0, NULL, NULL);
    draw_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9, 1, 0, 0, 0, 0,
                                               0, 0, NULL, NULL);
    if (event == EVENT_ACTIVATE)
    {

      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Exile opponent's grave",
                           result_or_mode, 5, "Draw a card", draw_available, 5, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_EXILE)
      {
        result_or_mode = 0;
      }
      else
      {
        if (choice != CHOICE_DRAW)
        {
          return 0;
        }
        result_or_mode = 1;
      }
      return generic_activated_ability(player, card, EVENT_ACTIVATE, 9, result_or_mode, 0, 0, 0, 0,
                                       0, 0, NULL, NULL);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result_or_mode = lantern->info_slot;
      if (result_or_mode == CHOICE_EXILE)
      {
        rfg_whole_graveyard(1 - player);
        result_or_mode = lantern->info_slot;
      }
      if (result_or_mode == CHOICE_DRAW)
      {
        draw_a_card(player);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23609c0; original cards/theros_beyond_death.c:3687. */
int card_thassas_intervention(int player, int card, event_t event)
{
  /* Thassa's Intervention	|X|U|U
   * Instant
   * Choose one -
   * * Look at the top X cards of your library. Put up to two of them into your hand and the rest on the bottom of your library in a random order.
   * * Counter target spell unless its controller pays twice |X. */

  card_instance_t *instance;
  int available_or_reveal_count;
  int deck_or_pick_count;
  uint32_t selected_mode;
  int remaining_revealed;
  int remaining_picks;
  target_definition_t td_spell;
  test_definition_t this_test;

  if (event == EVENT_CAN_COUNTER)
  {
    if (1 < raw_mana_available[player][2])
    {
      ai_modifier = ai_modifier + 0x18;
    }
  }
  else if (event == EVENT_CAN_CAST)
  {
    counterspell_target_definition(player, card, &td_spell, 0);
    get_card_instance(player, card);
    available_or_reveal_count = counterspell(player, card, EVENT_CAN_CAST, &td_spell, -1);
    if (player == 0)
    {
      return ((available_or_reveal_count == 0) ? 1U : 99U);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      counterspell_target_definition(player, card, &td_spell, 0);
      instance = get_card_instance(player, card);
      available_or_reveal_count = counterspell(player, card, EVENT_CAN_CAST, &td_spell, -1);
      if ((card == affected_card) &&
          ((player == affected_card_controller &&
            (generic_spell(player, card, EVENT_CAST_SPELL, 4, NULL, NULL, 0, NULL),
             spell_fizzled != 1))))
      {
        instance->number_of_targets = '\0';
        if (check_special_flags2(player, card, -0x80000000) == 0)
        {

          selected_mode = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                                      "Counter a spell", available_or_reveal_count, 1000000,
                                      "Browse top of library", 1, 1, 0, 0, 0, 0);
          if (selected_mode == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
          if (selected_mode == 1)
          {
            counterspell(player, card, EVENT_CAST_SPELL, &td_spell, 0);
          }
          instance->eot_toughness = selected_mode;
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    counterspell_target_definition(player, card, &td_spell, 0);
    instance = get_card_instance(player, card);
    counterspell(player, card, EVENT_CAN_CAST, &td_spell, -1);
    if (event == EVENT_RESOLVE_SPELL)
    {
      selected_mode = instance->eot_toughness;
      if (selected_mode == 1)
      {
        counterspell_resolve_unless_pay_x(player, card, &td_spell, 0, instance->info_slot * 2);
        selected_mode = instance->eot_toughness;
      }
      if (selected_mode == 2)
      {

        deck_or_pick_count = count_deck(player);
        available_or_reveal_count = instance->info_slot;
        if (deck_or_pick_count < instance->info_slot)
        {
          available_or_reveal_count = deck_or_pick_count;
        }
        if (available_or_reveal_count != 0)
        {

          deck_or_pick_count = available_or_reveal_count;
          if (2 < available_or_reveal_count)
          {
            deck_or_pick_count = 2;
          }
          do
          {
            remaining_revealed = available_or_reveal_count;
            remaining_picks = deck_or_pick_count;
            new_default_test_definition(&this_test, 0xff, "Select a card to add to your hand");
            this_test.create_minideck = (unsigned short)remaining_revealed;
            this_test.no_shuffle = 1;
            new_global_tutor(player, player, 1, 1, 1, 1, &this_test);
            deck_or_pick_count = remaining_picks + -1;
            if (deck_or_pick_count == 0)
              break;
            available_or_reveal_count = remaining_revealed + -1;
          } while (remaining_revealed + -1 != 0);
          remaining_revealed = remaining_revealed - remaining_picks;
          if (0 < remaining_revealed)
          {
            put_top_x_on_bottom_in_random_order(player, remaining_revealed);
          }
        }
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    counterspell_target_definition(player, card, &td_spell, 0);
    get_card_instance(player, card);
    counterspell(player, card, EVENT_CAN_CAST, &td_spell, -1);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2360e70; original cards/theros_beyond_death.c:3761. */
int card_thassas_oracle(int player, int card, event_t event)
{
  /* Thassa's Oracle	|U|U
   * Creature - Merfolk Wizard 1/3
   * When this creature enters, look at the top X cards of your library, where X is your devotion to blue. Put up to one of them on top of your library and the rest on the bottom of your library in a random order. If X is greater than or equal to the number of cards in your library, you win the game. (Each |U in the mana costs of permanents you control counts toward your devotion to blue.) */

  int num_cards;
  int result;
  test_definition_t test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    num_cards = devotion(player, card, 2, 0);
    default_test_definition(&test, 0xff);
    reveal_top_cards_of_library_and_choose(player, card, player, num_cards, 1, TUTOR_DECK, 0,
                                           TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test);
    result = count_deck(player);
    if (result <= num_cards)
    {
      lose_the_game(1 - player);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2360f60; original cards/theros_beyond_death.c:3780. */
int card_thassa_deep_dwelling(int player, int card, event_t event)
{
  /* Thassa, Deep-Dwelling	|3|U
   * Legendary Enchantment Creature - God 6/5
   * Indestructible
   * As long as your devotion to blue is less than five, Thassa isn't a creature.
   * At the beginning of your end step, exile up to one other target creature you control, then return that card to the battlefield under your control.
   * |3|U: Tap another target creature. */

  int result;
  resolve_trigger_t trig_mode;
  card_instance_t *instance;
  target_definition_t td;

  check_legend_rule(player, card, event);
  indestructible(player, card, event);
  if ((((trigger_condition == TRIGGER_EOT) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (current_turn == player))
  {

    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.special = TARGET_SPECIAL_NOT_ME;
    instance = get_card_instance(player, card);
    instance->number_of_targets = '\0';
    result = can_target(&td);
    trig_mode = RESOLVE_TRIGGER_0;
    if (result != 0)
    {
      result = duh_mode(player);
      trig_mode = (result != 0) + RESOLVE_TRIGGER_OPTIONAL;
    }
    result = eot_trigger_mode(player, card, event, player, trig_mode);
    if (result != 0)
    {
      result = pick_target(&td, "ASHNODS_BATTLEGEAR");
      if (result != 0)
      {
        blink_effect_controller(instance->targets[0].player, instance->targets[0].card, NULL);
      }
    }
  }
  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      result = thb_god(player, card, event, 2, 0, 5);
      return result;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      instance = get_card_instance(player, card);
      tap_card(instance->targets[0].player, instance->targets[0].card);
    }
  }
  default_target_definition(player, card, &td, 2);
  td.special = TARGET_SPECIAL_NOT_ME;
  return generic_activated_ability(player, card, event, 4, 3, 0, 1, 0, 0, 0, 0, &td,
                                     "TARGET_ANOTHER_CREATURE");
}

/* ManalinkEh 2022: 0x23598e0; original cards/theros_beyond_death.c:3834. */
static int akroan_war_legacy(int player, int card, event_t event)
{
  all_must_attack_if_able(1 - player, event, -1);
  if ((event == EVENT_CAN_SKIP_TURN) && (player == current_turn))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23611a0; original cards/theros_beyond_death.c:3840. */
int card_the_akroan_war(int player, int card, event_t event)
{
  /* The Akroan War	|3|R
   * Enchantment - Saga
   * (As this Saga enters and after your draw step, add a lore counter. Sacrifice after III.)
   * I - Gain control of target creature for as long as this Saga remains on the battlefield.
   * II - Until your next turn, creatures your opponents control attack each combat if able.
   * III - Each tapped creature deals damage to itself equal to its power. */

  int p;
  int creature_power;
  card_instance_t *instance;
  int target_selected;
  int card_00;
  target_definition_t td;

  if (event == EVENT_CHAPTER1)
  {

    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    target_selected = can_target(&td);
    if ((target_selected != 0) &&
        (target_selected = pick_target(&td, "TARGET_CREATURE"), target_selected != 0))
    {
      gain_control_until_source_is_in_play_and_tapped(player, card, instance->targets[0].player,
                                                      instance->targets[0].card,
                                                      GCUS_SOURCE_IN_PLAY);
    }
    instance->number_of_targets = '\0';
  }
  else if (event == EVENT_CHAPTER2)
  {
    alternate_legacy_text(2, player, create_legacy_effect(player, card, akroan_war_legacy));
  }
  else
  {

    p = 0;
    if (event == EVENT_CHAPTER3)
    {
      do
      {
        for (card_00 = 0; card_00 < active_cards_count[p]; ++card_00)
          if (is_what(p, card_00, 2) && is_tapped(p, card_00))
          {
            creature_power = get_power(p, card_00);
            damage_creature(p, card_00, creature_power, p, card_00);
          }
        p = p + 1;
      } while (p != 2);
    }
  }
  return saga(player, card, event, 3);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2361340; original cards/theros_beyond_death.c:3883. */
int card_the_binding_of_the_titans(int player, int card, event_t event)
{
  /* The Binding of the Titans	|1|G
   * Enchantment - Saga
   * (As this Saga enters and after your draw step, add a lore counter. Sacrifice after III.)
   * I - Each player mills three cards.
   * II - Exile up to two target cards from graveyards. For each creature card exiled this way, you gain 1 life.
   * III - Return target creature or land card from your graveyard to your hand. */

  int selected_count;
  card_instance_t *instance;
  int grave_id;
  int was_creature;
  int target_location;
  int creature_count_local;
  test_definition_t test;

  if (event == EVENT_CHAPTER1)
  {

    APNAP(p, {
      mill(p, 3);
    });
  }
  else if (event == EVENT_CHAPTER2)
  {

    instance = get_card_instance(player, card);
    selected_count = select_multiple_cards_from_either_graveyard(player, card, SFG_0, 3, 1, NULL, 2,
                                                                 instance->targets);
    creature_count_local = 0;
    if (0 < selected_count)
    {
      target_location = 0;
      do
      {

        grave_id = validate_target_from_grave_source(
            player, card, instance->targets[target_location].player >> 0x10, target_location);
        if (grave_id != -1)
        {
          was_creature = is_what(
              -1,
              graveyard_ptr_mutable[instance->targets[target_location].player >> 0x10][grave_id],
              2);
          creature_count_local = (creature_count_local + 1) - (unsigned int)(was_creature == 0);
          rfg_card_from_grave(instance->targets[target_location].player >> 0x10, grave_id);
        }
        target_location = target_location + 1;
      } while (selected_count != target_location);
    }
    gain_life(player, creature_count_local);
  }
  else if (event == EVENT_CHAPTER3)
  {
    new_default_test_definition(&test, 3, "Select target creature or land card.");
    if (new_special_count_grave(player, &test) != 0)
    {
      if (graveyard_has_shroud(player) == 0)
      {
        new_global_tutor(player, player, 2, 1, 1, 1, &test);
      }
    }
  }
  return saga(player, card, event, 3);
}
#endif

/* ManalinkEh 2022: 0x2359730; original cards/theros_beyond_death.c:3921. */
static int is_basic_land_by_internal_id(int iid, int unused, int unused1, int unused2)
{
  int result;

  result = is_basic_land_by_id((unsigned int)cards_data[iid].id);
  return result;
}

/* ManalinkEh 2022: 0x2361560; original cards/theros_beyond_death.c:3924. */
int card_the_birth_of_meletis(int player, int card, event_t event)
{
  /* The Birth of Meletis	|1|W
   * Enchantment - Saga
   * (As this Saga enters and after your draw step, add a lore counter. Sacrifice after III.)
   * I - Search your library for a basic Plains card, reveal it, put it into your hand, then shuffle.
   * II - Create a 0/4 colorless Wall artifact creature token with defender.
   * III - You gain 2 life. */

  const char *msg;


  if (event == EVENT_CHAPTER1)
  {
    test_definition_t this_test;
    msg = get_hacked_land_text(player, card, "Select a basic %s card.", 0x5007);
    new_default_test_definition(&this_test, 1, msg);
    this_test.subtype = get_hacked_subtype(player, card, 0x5007);
    this_test.special_selection_function = is_basic_land_by_internal_id;
    new_global_tutor(player, player, 1, 1, 0, 5, &this_test);
  }
  else if (event == EVENT_CHAPTER2)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_WALL, &token);
    token.pow = 0;
    token.tou = 4;
    generate_token(&token);
  }
  else if (event == EVENT_CHAPTER3)
  {
    gain_life(player, 2);
  }
  return saga(player, card, event, 3);
}

/* ManalinkEh 2022: 0x23616b0; original cards/theros_beyond_death.c:3957. */
int card_the_first_iroan_games(int player, int card, event_t event)
{
  /* The First Iroan Games	|2|G
   * Enchantment - Saga
   * (As this Saga enters and after your draw step, add a lore counter. Sacrifice after IV.)
   * I - Create a 1/1 white Human Soldier creature token.
   * II - Put three +1/+1 counters on target creature you control.
   * III - If you control a creature with power 4 or greater, draw two cards.
   * IV - Create a Gold token. (It's an artifact with "Sacrifice this token: Add one mana of any color.") */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event == EVENT_CHAPTER1)
  {
    generate_token_by_id(player, card, CARD_ID_HUMAN_SOLDIER);
  }
  else if (event == EVENT_CHAPTER2)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 1);
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_P1_P1, 3);
      }
    }
  }
  else if (event == EVENT_CHAPTER3)
  {
    result = ferocious(player, card);
    if (result != 0)
    {
      draw_cards(player, 2);
    }
  }
  else if (event == EVENT_CHAPTER4)
  {
    generate_token_by_id(player, card, CARD_ID_GOLD);
  }
  result = saga(player, card, event, 4);
  return result;
}

/* ManalinkEh 2022: 0x2361800; original cards/theros_beyond_death.c:3995. */
int card_the_triumph_of_anax(int player, int card, event_t event)
{
  /* The Triumph of Anax	|2|R
   * Enchantment - Saga
   * (As this Saga enters and after your draw step, add a lore counter. Sacrifice after IV.)
   * I, II, III - Until end of turn, target creature gains trample and gets +X/+0, where X is the number of lore counters on this Saga.
   * IV - Target creature you control fights up to one target creature you don't control. (Each deals damage equal to its power to the other.) */

  int result;
  card_instance_t *instance;
  target_definition_t td0;
  target_definition_t td;

  if (((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CHAPTER1) || (event == EVENT_CHAPTER3))
  {
    default_target_definition(player, card, &td, 2);
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_CREATURE");
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        result = count_counters(player, card, COUNTER_LORE);
        pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                               result, 0, 0x80, 0);
      }
    }
  }
  if (event == EVENT_CHAPTER4)
  {

    default_target_definition(player, card, &td0, 2);
    td0.allowed_controller = player;
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - player;
    td.allow_cancel = 1;
    instance = get_card_instance(player, card);
    instance->number_of_targets = '\0';
    result = new_pick_target(&td0, "Select target creature you control.", 0, 0x81);
    if (result != 0)
    {
      new_pick_target(&td, "Select target creature opponent controls.", 1, 0x81);
    }
    result = valid_target(&td0);
    if (result != 0)
    {
      result = validate_target(player, card, &td, 1);
      if (result != 0)
      {
        fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player,
              instance->targets[1].card);
      }
    }
  }
  result = saga(player, card, event, 4);
  return result;
}

/* ManalinkEh 2022: 0x2361a10; original cards/theros_beyond_death.c:4071. */
int card_towering_wave_mystic(int player, int card, event_t event)
{
  /* Towering-Wave Mystic	|1|U
   * Creature - Merfolk Wizard 2/1
   * Whenever this creature deals damage, target player mills that many cards. */

  int damage_amount;
  card_instance_t *instance;
  target_definition_t td;

  check_damage(player, card, event, DDBM_MAY_DAMAGE_PLANESWALKER | DDBM_REPORT_DAMAGE_DEALT, player,
               card);
  if (resolve_damage_trigger(player, card, event,
                             DDBM_MAY_DAMAGE_PLANESWALKER | DDBM_REPORT_DAMAGE_DEALT, player,
                             card) == 0)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  damage_amount = instance->targets[0x10].player;
  instance->targets[0x10].player = 0;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.allow_cancel = 0;
  if (can_target(&td) != 0)
  {
    if (new_pick_target(&td, "TARGET_PLAYER", 0, 0) != 0)
    {
      mill(instance->targets[0].player, damage_amount);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x2361b20; original cards/theros_beyond_death.c:4138. */
int card_tymaret_calls_the_dead(int player, int card, event_t event)
{
  /* Tymaret Calls the Dead	|2|B
   * Enchantment - Saga
   * (As this Saga enters and after your draw step, add a lore counter. Sacrifice after III.)
   * I, II - Mill three cards. Then you may exile a creature or enchantment card from your graveyard. If you do, create a 2/2 black Zombie creature token.
   * III - You gain X life and scry X, where X is the number of Zombies you control. */

  int result;
  test_definition_t test;

  if ((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CHAPTER1)
  {
    mill(player, 3);
    new_default_test_definition(&test, 6, "Select a creature or enchantment card to exile.");
    result = select_target_from_grave_source(player, card, player, SFG_NOTARGET, 3, &test, 0);
    if (result != -1)
    {

      result = validate_target_from_grave_source(player, card, player, 0);
      if (result != -1)
      {
        rfg_card_from_grave(player, result);
        generate_token_by_id(player, card, CARD_ID_ZOMBIE);
      }
    }
  }
  else if (event == EVENT_CHAPTER3)
  {

    result = count_subtype(player, -1, 0xde);
    gain_life(player, result);
    scry(player, result);
  }
  result = saga(player, card, event, 3);
  return result;
}

/* ManalinkEh 2022: 0x2361c60; original cards/theros_beyond_death.c:4170. */
int card_tymaret_chosen_from_death(int player, int card, event_t event)
{
  /* Tymaret, Chosen from Death	|B|B
   * Legendary Enchantment Creature - Demigod 2/ *
   * Tymaret's toughness is equal to your devotion to black. (Each |B in the mana costs of permanents you control counts toward your devotion to black.)
   * |1|B: Exile up to two target cards from graveyards. You gain 1 life for each creature card exiled this way. */

  int i;
  card_instance_t *instance;
  int grave_id;
  int exiled_creature;
  int creature_count_local;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if (event != EVENT_TOUGHNESS)
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      default_test_definition(&test, 0xff);
      test.ai_selection_mode = 3;
      test.qty = 2;
      if (event == EVENT_CAN_ACTIVATE)
      {
        return generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE,
                                                              GAA_NONE, 1, 1, 0, 0, 0, 0, 0, NULL,
                                                              NULL, TYPE_NONE, &test, 0x2000);
      }
      if (event == EVENT_ACTIVATE)
      {
        memset(instance->targets, 0xff, sizeof(instance->targets));
        return generic_activated_ability_with_target_in_grave(player, card, EVENT_ACTIVATE,
                                                              GAA_NONE, 1, 1, 0, 0, 0, 0, 0, NULL,
                                                              NULL, TYPE_NONE, &test, 0x2000);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return global_enchantment(player, card, event);
      instance = get_card_instance(player, card);
      default_test_definition(&test, 0xff);
    }
    test.ai_selection_mode = 3;
    test.qty = 2;
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      return 0;
    }

    creature_count_local = 0;
    i = 0;
    do
    {

      grave_id =
          validate_target_from_grave_source(player, card, instance->targets[i].player >> 0x10, i);
      if (grave_id != -1)
      {
        exiled_creature =
            is_what(-1, graveyard_ptr_mutable[instance->targets[i].player >> 0x10][grave_id], 2);
        creature_count_local = (creature_count_local + 1) - (unsigned int)(exiled_creature == 0);
        rfg_card_from_grave(instance->targets[i].player >> 0x10, grave_id);
      }
      i = i + 1;
    } while (i != 2);
    gain_life(player, creature_count_local);
    return 0;
  }
  if (((card == affected_card) && (affected_card_controller == player)) && (player != -1))
  {
    set_characteristic_pt(devotion(player, card, 1, 0));
  }
  return global_enchantment(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2359bb0; original cards/theros_beyond_death.c:4212. */
static int breach_effect(int player, int card, event_t event)
{
  int i;
  int graveyard_count;
  int choice;
  int *graveyard;
  card_ptr_t *card_data;
  card_instance_t *instance;
  int creature_cage_or_grave_index;
  int grave_index_or_result;
  unsigned int escape_options;
  int card_00;
  int grave_count_or_added_card;
  int iid_or_text_length;
  int spell_cage_or_grave_source;
  int option_count;
  int buffer_length;
  int opts[100][2];

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      if (-1 < instance->damage_target_player)
      {
        creature_cage_or_grave_index =
            get_id((int)instance->damage_target_player, instance->damage_target_card);
        if (creature_cage_or_grave_index != 0x52b7)
        {
          kill_card(player, card, KILL_EXILE);
        }
      }
    }
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    graveyard = graveyard_ptr_mutable[player];
    if ((*graveyard != -1) && ((player_bits[player] & PB_GY_ABILITIES_DISABLED) == 0))
    {
      instance = get_card_instance(player, card);
      creature_cage_or_grave_index = instance->targets[0].card;
      instance = get_card_instance(player, card);
      if (is_humiliated(instance->targets[0].player, creature_cage_or_grave_index, AL_STATIC) == 0)
      {
        graveyard_count = count_graveyard(player);
        if (0 < graveyard_count)
        {
          i = 0;
          do
          {
            grave_count_or_added_card = graveyard[i];
            if ((cards_data[grave_count_or_added_card].type & 1) == 0)
            {
              grave_index_or_result = add_card_to_hand(player, grave_count_or_added_card);
              add_state(player, grave_index_or_result, 0x20);
              hand_count[player] = hand_count[player] + -1;
              card_data = cards_ptr[cards_data[grave_count_or_added_card].id];
              escape_options = escape_part(
                  player, grave_index_or_result, EVENT_GRAVEYARD_ABILITY,
                  (unsigned int)card_data->req.req_colorless, (unsigned int)card_data->req.req_black,
                  (unsigned int)card_data->req.req_blue, (unsigned int)card_data->req.req_green,
                  (unsigned int)card_data->req.req_red, (unsigned int)card_data->req.req_white, 3);
              obliterate_card_and_recycle(player, grave_index_or_result);
              if (escape_options != 0)
              {
                return (((escape_options & 0x400) == 0) ? 1U : 99U);
              }
            }
            i = i + 1;
          } while (graveyard_count != i);
        }
      }
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    char buffer[3000];
    buffer_length = scnprintf(buffer, 3000, " Cancel\n");
    graveyard = graveyard_ptr_mutable[player];
    creature_cage_or_grave_index = grafdiggers_cages(player, TYPE_CREATURE, TUTOR_FROM_GRAVE);
    spell_cage_or_grave_source = grafdiggers_cages(player, TYPE_SPELL, TUTOR_FROM_GRAVE);
    if ((player_bits[player] & PB_GY_ABILITIES_DISABLED) == 0)
    {
      grave_count_or_added_card = count_graveyard(player);
      hack_allow_sorcery_if_rules_engine_is_only_effect = 1;
      if (0 < grave_count_or_added_card)
      {
        option_count = 1;
        grave_index_or_result = 0;
        do
        {
          iid_or_text_length = graveyard[grave_index_or_result];
          if ((cards_data[iid_or_text_length].type & 1) == 0)
          {
            card_00 = add_card_to_hand(player, iid_or_text_length);
            add_state(player, card_00, 0x20);
            hand_count[player] = hand_count[player] + -1;
            card_data = cards_ptr[cards_data[iid_or_text_length].id];
            escape_options = escape_part(
                player, card_00, EVENT_GRAVEYARD_ABILITY, (unsigned int)card_data->req.req_colorless,
                (unsigned int)card_data->req.req_black, (unsigned int)card_data->req.req_blue,
                (unsigned int)card_data->req.req_green, (unsigned int)card_data->req.req_red,
                (unsigned int)card_data->req.req_white, 3);
            if (escape_options != 0)
            {
              if ((cards_data[iid_or_text_length].type & 2)
                      ? creature_cage_or_grave_index == 0
                      : (spell_cage_or_grave_source == 0 ||
                         (cards_data[iid_or_text_length].type & 0x38) == 0))
              {
                if ((escape_options & 0x2000) != 0)
                {
                  iid_or_text_length = scnprintf(buffer + buffer_length, 3000 - buffer_length,
                                                 " Breach %s\n", card_data->name);
                  buffer_length = buffer_length + iid_or_text_length;
                  opts[option_count][0] = grave_index_or_result;
                  opts[option_count][1] = escape_options;
                  option_count = option_count + 1;
                }
              }
            }
            obliterate_card_and_recycle(player, card_00);
          }
          grave_index_or_result = grave_index_or_result + 1;
        } while (grave_count_or_added_card != grave_index_or_result);
      }
    }
    choice = do_dialog(player, player, card, -1, -1, buffer, 1);
    if (choice == 0)
    {
      spell_fizzled = 1;
      hack_allow_sorcery_if_rules_engine_is_only_effect = 0;
      return 0;
    }
    if ((opts[choice][1] & 0x2000U) != 0)
    {
      creature_cage_or_grave_index = opts[choice][0];
      grave_count_or_added_card = add_card_to_hand(player, graveyard[creature_cage_or_grave_index]);
      add_state(player, grave_count_or_added_card, 0x20);
      hand_count[player] = hand_count[player] + -1;
      instance = get_card_instance(player, grave_count_or_added_card);
      spell_cage_or_grave_source = graveyard_source[player][creature_cage_or_grave_index];
      instance->targets[3].card = spell_cage_or_grave_source;
      grave_index_or_result = get_id(player, grave_count_or_added_card);
      card_data = cards_ptr[grave_index_or_result];
      grave_index_or_result =
          escape_part(player, grave_count_or_added_card, EVENT_PAY_FLASHBACK_COSTS,
                      (unsigned int)card_data->req.req_colorless, (unsigned int)card_data->req.req_black,
                      (unsigned int)card_data->req.req_blue, (unsigned int)card_data->req.req_green,
                      (unsigned int)card_data->req.req_red, (unsigned int)card_data->req.req_white, 3);
      if ((grave_index_or_result == 0) || (spell_fizzled == 1))
      {
        obliterate_card(player, grave_count_or_added_card);
        spell_fizzled = 1;
        hack_allow_sorcery_if_rules_engine_is_only_effect = 0;
        return 0;
      }
      creature_cage_or_grave_index = find_in_graveyard_by_source(player, spell_cage_or_grave_source,
                                                                 creature_cage_or_grave_index);
      if (grave_index_or_result == 1)
      {
        set_special_flags3(
            player, play_card_in_grave_for_free(player, player, creature_cage_or_grave_index),
            0x80000);
        cant_be_responded_to = 1;
      }
      obliterate_card(player, grave_count_or_added_card);
    }
    hack_allow_sorcery_if_rules_engine_is_only_effect = 0;
    return 0;
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2361fd0; original cards/theros_beyond_death.c:4332. */
int card_underworld_breach(int player, int card, event_t event)
{
  /* Underworld Breach	|1|R
   * Enchantment
   * Each nonland card in your graveyard has escape. The escape cost is equal to the card's mana cost plus exile three other cards from your graveyard. (You may cast cards from your graveyard for their escape cost.)
   * At the beginning of the end step, sacrifice this enchantment. */

  int c;
  int sacrifice_at_eot;
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
             (instance = get_card_instance(player, c), instance->info_slot_fn == breach_effect)) &&
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
      instance = get_card_instance(
          player, create_targetted_legacy_activate(player, card, breach_effect, player, card));
      instance->targets[0].player = player;
      instance->targets[0].card = card;
      instance->number_of_targets = '\x01';
    }
  }
  sacrifice_at_eot = is_humiliated(player, card, AL_TRIG);
  if (((sacrifice_at_eot == 0) &&
       (sacrifice_at_eot = check_state(player, card, STATE_OUBLIETTED), sacrifice_at_eot == 0)) &&
      (sacrifice_at_eot = eot_trigger(player, card, event), sacrifice_at_eot != 0))
  {
    kill_card(player, card, KILL_SACRIFICE);
  }
  return global_enchantment(player, card, event);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2362150; original cards/theros_beyond_death.c:4373. */
int card_underworld_charger(int player, int card, event_t event)
{
  /* Underworld Charger	|2|B
   * Creature - Nightmare Horse 3/3
   * This creature can't block.
   * Escape-|4|B, Exile three other cards from your graveyard. (You may cast this card from your graveyard for its escape cost.)
   * This creature escapes with two +1/+1 counters on it. */

  int result;
  cannot_block(player, card, event);
  result = escape_with_counters_impl(player, card, event, 4, 1, 0, 0, 0, 0, 3, 2);
  return result;
}
#endif

/* ManalinkEh 2022: 0x23621b0; original cards/theros_beyond_death.c:4411. */
int card_uro_titan_of_natures_wrath(int player, int card, event_t event)
{
  /* Uro, Titan of Nature's Wrath	|1|G|U
   * Legendary Creature - Elder Giant 6/6
   * When Uro enters, sacrifice it unless it escaped.
   * Whenever Uro enters or attacks, you gain 3 life and draw a card, then you may put a land card from your hand onto the battlefield.
   * Escape-|G|G|U|U, Exile five other cards from your graveyard. (You may cast this card from your graveyard for its escape cost.) */

  int result;
  test_definition_t test_land;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  new_default_test_definition(&test_land, 1, "Select a land card.");
  test_land.zone = 0x100;
  if (event == EVENT_ETB)
  {
    gain_life(player, 3);
    draw_a_card(player);
    new_global_tutor(player, player, 4, 4, 0, 1, &test_land);
    result = check_special_flags3(player, card, 0x80000);
    if (result == 0)
    {
      kill_card(player, card, KILL_SACRIFICE);
    }
    dat_ability(player, card, EVENT_ETB, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  }
  else
  {
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
    if (event == EVENT_DAT_ABILITY)
    {
      gain_life(player, 3);
      draw_a_card(player);
      new_global_tutor(player, player, 4, 4, 0, 1, &test_land);
      return 0;
    }
    if ((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS))
    {
      result = escape(player, card, event, 0, 0, 2, 2, 0, 0, 5);
      return result;
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23623d0; original cards/theros_beyond_death.c:4471. */
int card_voracious_typhon(int player, int card, event_t event)
{
  /* Voracious Typhon	|2|G|G
   * Creature - Snake Beast 4/4
   * Escape-|5|G|G, Exile four other cards from your graveyard. (You may cast this card from your graveyard for its escape cost.)
   * This creature escapes with three +1/+1 counters on it. */

  int result;
  result = escape_with_counters_impl(player, card, event, 5, 0, 0, 2, 0, 0, 4, 3);
  return result;
}
#endif

/* ManalinkEh 2022: 0x2362420; original cards/theros_beyond_death.c:4506. */
int counter_activated_ability_resolve_unless_pay_x(int player, int card, target_definition_t *td,
                                                   int target_num, int x)
{
  int valid_activation;
  int result_or_player;
  card_instance_t *instance;
  char options[40];

  valid_activation = validate_counter_activated_ability(player, card, td, target_num);
  result_or_player = 0;

  if ((valid_activation == 0) || (result_or_player = 1, x < 1))
  {
    return result_or_player;
  }
  sprintf(options, " Pay %d\n Decline", x);
  instance = get_card_instance(player, card);
  result_or_player = has_mana(instance->targets[target_num].player, 7, x);
  if ((result_or_player != 0) &&
      ((result_or_player = instance->targets[target_num].player,
        result_or_player = do_dialog(result_or_player, player, card, result_or_player,
                                     instance->targets[target_num].card, options, 0),
        result_or_player == 0 && (result_or_player = charge_mana_while_resolving(
                                      player, card, EVENT_RESOLVE_SPELL,
                                      instance->targets[target_num].player, COLOR_COLORLESS, x),
                                  result_or_player != 0))))
  {
    return 2;
  }
  raw_counter_activated_ability(instance->targets[target_num].player,
                                instance->targets[target_num].card);
  return 1;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2362540; original cards/theros_beyond_death.c:4557. */
int card_woe_strider(int player, int card, event_t event)
{
  /* Woe Strider	|2|B
   * Creature - Horror 3/2
   * When this creature enters, create a 0/1 white Goat creature token.
   * Sacrifice another creature: Scry 1.
   * Escape-|3|B|B, Exile four other cards from your graveyard. (You may cast this card from your graveyard for its escape cost.)
   * This creature escapes with two +1/+1 counters on it. */

  test_definition_t test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    generate_token_by_id(player, card, CARD_ID_GOAT);
  }
  else
  {
    if (((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS)) ||
        (((event == EVENT_CAST_SPELL || (event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE)) &&
          ((card == affected_card && (player == affected_card_controller))))))
    {
      return escape_with_counters_impl(player, card, event, 3, 2, 0, 0, 0, 0, 4, 2);
    }
  }
  if (event != EVENT_ACTIVATE &&
      (1U < (unsigned int)event - (unsigned int)EVENT_RESOLVE_ACTIVATION) &&
      (event != EVENT_CLEANUP))
  {
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    scry(player, 1);
  }
  new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
  test.not_me = 1;
  return generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test, SARV_0);
}
#endif
