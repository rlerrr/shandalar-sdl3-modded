#include "manalink.h"

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int doublecast_legacy_mana_value_2_or_less(int player, int card, event_t event);
int triplecast_legacy_mana_value_2_or_less(int player, int card, event_t event);

/* ManalinkEh 2022: 0x2306e80; original cards/zendikar_rising.c:143. */
void mdfc_transform(int player, int card, int csvid)
{
  int result;
  card_instance_t *instance;

  true_transform_to_csvid(player, card, csvid);
  result = check_special_flags3(player, card, 0x200);
  if (result == 0)
  {
    set_special_flags3(player, card, 0x200);
  }
  else
  {
    remove_special_flags3(player, card, 0x200);
  }
  instance = get_card_instance(player, card);
  instance->regen_status = instance->regen_status | 0x1f000000;
  get_abilities(player, card, EVENT_CHANGE_TYPE, -1);
  get_abilities(player, card, EVENT_SET_COLOR, -1);
  result = is_what(player, card, 2);
  if (result != 0)
  {
    get_abilities(player, card, EVENT_POWER, -1);
    get_abilities(player, card, EVENT_TOUGHNESS, -1);
    get_abilities(player, card, EVENT_ABILITIES, -1);
  }
  if (csvid == instance->original_internal_card_id)
  {
    instance->targets[0xc].card = -1;
    instance->targets[0xd].player = -1;
    instance->targets[0xd].card = -1;
  }
  return;
}

/* ManalinkEh 2022: 0x23077a0; original cards/zendikar_rising.c:169. */
int mdfc_back(int player, int card, event_t event)
{
  int result;

  if (((event == EVENT_COUNTERSPELL) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_token(player, card);
    if (result == 0)
    {

      result = get_id(player, card);
      result = dfc_back_to_front(result);
      mdfc_transform(player, card, result);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2306fe0; original cards/zendikar_rising.c:177. */
static int mdfc_front_impl(int player, int card, event_t event, int can_play_front,
                           int suppress_charging_back_cost)
{
  card_ptr_t *front_card_data;
  unsigned char front_type;
  int front_iid_or_result;
  int back_csvid;
  int legality_or_token;
  card_instance_t *instance;
  const char *front_name;
  unsigned int csvid;
  unsigned int front_or_back_available;
  bool back_or_success;
  unsigned int back_available;
  unsigned int skip_front_cost;
  const char *back_name;

  if (((event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE) &&
       event != EVENT_PLAY_FIRST_HALF) &&
      (event != EVENT_CAN_CAST))
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
    else if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
             (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  front_iid_or_result = get_original_internal_card_id(player, card);
  csvid = (unsigned int)cards_data[front_iid_or_result].id;
  front_type = cards_data[front_iid_or_result].type;
  bool check_land_timing = (front_type & 0x30) != 0;
  if (!check_land_timing)
  {
    if (can_sorcery_be_played(player, event))
    {
      front_type = cards_data[front_iid_or_result].type;
      check_land_timing = true;
    }
    else
      can_play_front = 0;
  }
  if (check_land_timing && (front_type & 1) &&
      (((unsigned char)land_can_be_played & 1) || player != current_turn))
    can_play_front = 0;
  back_csvid = dfc_front_to_back(csvid);
  if (((stack_size == 1) && (card_on_stack_controller == player)) &&
      ((card_on_stack == card &&
        (legality_or_token = can_legally_play_csvid(player, back_csvid), legality_or_token != 0))))
  {
    back_or_success = true;
  }
  else
  {
    legality_or_token = can_legally_play_csvid_now(player, back_csvid, event);
    back_or_success = legality_or_token != 0;
  }
  if (event == EVENT_MODIFY_COST)
  {
    if ((((suppress_charging_back_cost == 0) &&
          (front_iid_or_result = has_mana_to_cast_id(player, EVENT_MODIFY_COST, back_csvid),
           front_iid_or_result == 0)) ||
         (!back_or_success)) &&
        ((front_iid_or_result = has_mana_to_cast_id(player, EVENT_MODIFY_COST, csvid),
          front_iid_or_result == 0 || (can_play_front == 0))))
    {
      infinite_casting_cost();
      return 0;
    }

    front_iid_or_result = get_id(player, card);
    front_card_data = cards_ptr[front_iid_or_result];
    front_iid_or_result = get_updated_casting_cost(player, card, -1, EVENT_MODIFY_COST, -1);
    COST_COLORLESS = COST_COLORLESS - (char)front_iid_or_result;
    front_iid_or_result = get_id(player, card);
    back_or_success = false;
    if (front_iid_or_result == 0x5560)
    {
      front_iid_or_result = count_basic_lands(1 - player, -1);
      back_or_success = front_iid_or_result == 0;
    }
    COST_COLORLESS = COST_COLORLESS - back_or_success;
    COST_BLACK = COST_BLACK - front_card_data->req.req_black;
    COST_BLUE = COST_BLUE - front_card_data->req.req_blue;
    COST_GREEN = COST_GREEN - front_card_data->req.req_green;
    COST_RED = COST_RED - front_card_data->req.req_red;
    COST_WHITE = COST_WHITE - front_card_data->req.req_white;
    return 0;
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_CHANGE_TYPE)
  {
    if (card != affected_card)
    {
      return 0;
    }
    if (player != affected_card_controller)
    {
      return 0;
    }
    front_iid_or_result = instance->targets[0xc].card;
    if (front_iid_or_result < 0)
    {
      return 0;
    }
    event_result = front_iid_or_result;
    instance->regen_status = instance->regen_status | 0x9000000;
    return 0;
  }
  if (event == EVENT_CAN_CAST)
  {

    if ((suppress_charging_back_cost == 0) &&
        (front_iid_or_result = has_mana_to_cast_id(player, EVENT_CAN_CAST, back_csvid),
         front_iid_or_result == 0))
    {
      front_or_back_available = 0;
    }
    else
    {
      front_or_back_available = (unsigned int)back_or_success;
    }
    front_iid_or_result = has_mana_to_cast_id(player, EVENT_CAN_CAST, csvid);
    if (can_play_front != 0)
    {
      if (front_iid_or_result != 0)
      {
        return ((can_play_front == 99) ? 99U : 1U);
      }
      return front_or_back_available;
    }
    return front_or_back_available;
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
  if (check_special_flags3(player, card, 1) != 0)
  {
    return 0;
  }

  bool choose_face = false;
  if (!played_for_free(player, card))
  {
    legality_or_token = is_token(player, card);
    skip_front_cost = legality_or_token != 0;
    if (!check_special_flags2(player, card, -0x80000000))
    {
      skip_front_cost |= suppress_charging_back_cost;
      if (skip_front_cost == 0)
      {
        if (!has_mana_to_cast_id(player, EVENT_CAST_SPELL, back_csvid))
          back_available = 0;
        else
          back_available = back_or_success;
      }
      else
        back_available = back_or_success;
      back_name = get_card_name_by_id(back_csvid);
      if (legality_or_token != 0)
        front_or_back_available = can_play_front != 0;
      else
      {
        legality_or_token = has_mana_to_cast_id(player, EVENT_CAST_SPELL, csvid);
        skip_front_cost = 0;
        front_or_back_available = legality_or_token != 0 && can_play_front != 0;
      }
      choose_face = true;
    }
  }
  else
  {
    skip_front_cost = 1;
    if (!check_special_flags2(player, card, -0x80000000))
    {
      back_available = back_or_success;
      back_name = get_card_name_by_id(back_csvid);
      front_or_back_available = can_play_front != 0;
      choose_face = true;
    }
  }
  if (choose_face)
  {
    front_name = get_card_name_by_instance(instance);
    if (!dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_MODE, DLG_AUTOCHOOSE_IF_1, front_name,
                     front_or_back_available, 1, back_name, back_available, 1, 0, 0, 0, 0))
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  if (instance->mode != '\x01')
  {
    if (instance->mode != '\x02')
    {
      return 0;
    }
    instance->targets[0xd].player = csvid;
    instance->targets[0xd].card = csvid;
    mdfc_transform(player, card, back_csvid);
    if (skip_front_cost == 0 && suppress_charging_back_cost == 0)
    {
      charge_mana_from_id(player, card, EVENT_CAST_SPELL, back_csvid);
    }
    back_or_success = spell_fizzled != 1;
    instance->targets[1].card = x_value;
    if (back_or_success)
    {

      front_iid_or_result = get_internal_card_id_from_csv_id(back_csvid);
      call_card_fn(cards_data[front_iid_or_result].code_pointer, instance, player, card,
                   EVENT_CAST_SPELL);
      if (spell_fizzled != 1)
      {
        return 0;
      }
    }
    mdfc_transform(player, card, csvid);
    return 0;
  }
  if (skip_front_cost == 0)
  {
    charge_mana_from_id(player, card, EVENT_CAST_SPELL, csvid);
    if (is_x_spell(player, card) != 0)
    {
      set_x_for_x_spells(player, card, EVENT_CAST_SPELL, x_value);
    }
  }
  if (spell_fizzled == 1)
  {
    return 0;
  }

  call_card_fn(cards_data[front_iid_or_result].code_pointer, instance, player, card,
               EVENT_PLAY_FIRST_HALF);
  return 0;
}

/* ManalinkEh 2022: 0x2307820; original cards/zendikar_rising.c:270. */
int mdfc_front_conditional(int player, int card, event_t event, int can_play_front)
{
  int result;

  result = mdfc_front_impl(player, card, event, can_play_front, 0);
  return result;
}

/* ManalinkEh 2022: 0x2307840; original cards/zendikar_rising.c:273. */
int mdfc_front_suppress_charging_mana(int player, int card, event_t event)
{
  int result;

  result = mdfc_front_impl(player, card, event, 1, 1);
  return result;
}

/* ManalinkEh 2022: 0x2307860; original cards/zendikar_rising.c:277. */
int mdfc_front(int player, int card, event_t event)
{
  int result;

  result = mdfc_front_impl(player, card, event, 1, 0);
  return result;
}

/* ManalinkEh 2022: 0x2307880; original cards/zendikar_rising.c:291. */
int cmc_not_in_array(int iid, int arrptr, int player, int card)
{
  int candidate_cmc;
  int existing_cmc;
  int i;

  i = 0;
  do
  {
    candidate_cmc = get_cmc_by_id((unsigned int)cards_data[iid].id);
    existing_cmc = get_cmc_by_id((unsigned int)cards_data[*(int *)arrptr].id);
    if (candidate_cmc == existing_cmc)
    {
      return 0;
    }
    i = i + 1;
    arrptr = arrptr + 4;
  } while ((i < 4) && (i != -1));
  return 1;
}

/* ManalinkEh 2022: 0x2307c90; original cards/zendikar_rising.c:377. */
int mdfc_back_etb_tapped_land(int player, int card, event_t event)
{
  int result;

  mdfc_back(player, card, event);
  result = plain_mana_producer_tapped(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2306b30; original cards/zendikar_rising.c:405. */
static int angel_of_destiny_legacy(int player, int card, event_t event)
{
  int opponent_life;
  int opponent_or_starting_life;
  opponent_or_starting_life = eot_trigger(player, card, event);
  if ((opponent_or_starting_life != 0) &&
      (opponent_or_starting_life = 1 - player, opponent_or_starting_life == current_turn))
  {
    opponent_life = life[opponent_or_starting_life];
    opponent_or_starting_life = get_starting_life_total(opponent_or_starting_life);
    if (opponent_or_starting_life <= opponent_life)
    {
      lose_the_game(player);
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2307d10; original cards/zendikar_rising.c:414. */
int card_angel_of_destiny(int player, int card, event_t event)
{
  /* Angel of Destiny	|3|W|W
   * Creature - Angel Cleric 2/6
   * Flying, double strike
   * Whenever a creature you control deals combat damage to a player, you and that player each gain that much life.
   * At the beginning of your end step, if you have at least 15 life more than your starting life total, each player this creature attacked this turn loses the game. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_DEAL_DAMAGE)
  {
    instance = get_card_instance(affected_card_controller, affected_card);
    if (instance->internal_card_id != damage_card)
    {
      return 0;
    }
    if (instance->info_slot < 1)
    {
      return 0;
    }
    if (instance->damage_source_player != player)
    {
      return 0;
    }
    if ((instance->token_status & 0x140000) == 0)
    {
      return 0;
    }
    if ((instance->targets[3].player & 2) == 0)
    {
      return 0;
    }

    APNAP(p, {
      gain_life(p, instance->info_slot);
    });
  }

  result = declare_attackers_trigger(
      player, card, event, DAT_ATTACKS_PLAYER | DAT_RESOLVE_TRIGGER_MANDATORY, player, card);
  if (result == 0)
  {
    return 0;
  }
  create_legacy_effect_for_opponent(player, card, angel_of_destiny_legacy, -1, -1);
  return 0;
}

/* ManalinkEh 2022: 0x2307e10; original cards/zendikar_rising.c:444. */
int card_anowon_the_ruin_thief(int player, int card, event_t event)
{
  /* Anowon, the Ruin Thief	|2|U|B
   * Legendary Creature - Vampire Rogue 2/4
   * Other Rogues you control get +1/+1.
   * Whenever one or more Rogues you control deal combat damage to a player, that player mills a card for each 1 damage dealt to them. If the player mills at least one creature card this way, you draw a card. (To mill a card, a player puts the top card of their library into their graveyard.) */

  int result;
  card_instance_t *instance;
  test_definition_t test;
  check_legend_rule(player, card, event);
  boost_subtype(player, card, event, SUBTYPE_ROGUE, 1, 1, 0, 0, BCT_CONTROLLER_ONLY);
  if (event == EVENT_DEAL_DAMAGE)
  {
    default_test_definition(&test, 0xff);
    test.subtype = 0x9f;
    check_damage_test(player, -1, EVENT_DEAL_DAMAGE,
                      DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                          DDBM_REPORT_DAMAGE_DEALT | DDBM_MUST_BE_COMBAT_DAMAGE,
                      player, card, 0, &test);
  }
  result = resolve_damage_trigger(player, card, event,
                                  DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                                      DDBM_REPORT_DAMAGE_DEALT | DDBM_MUST_BE_COMBAT_DAMAGE,
                                  player, card);
  if (result == 0)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  special_mill(player, card, 0x550c, 0,
               (unsigned int)(unsigned char)instance->targets[0x10].player);
  special_mill(player, card, 0x550c, 1,
               (unsigned int)SET_BYTE1(instance->targets[0x10].player));
  return 0;
}

/* ManalinkEh 2022: 0x2307f80; original cards/zendikar_rising.c:475. */
int card_archon_of_emeria(int player, int card, event_t event)
{
  /* Archon of Emeria	|2|W
   * Creature - Archon 2/3
   * Flying
   * Each player can't cast more than one spell each turn.
   * Nonbasic lands your opponents control enter tapped. */

  int result;
  test_definition_t test;

  if ((event == EVENT_CAST_SPELL) || (event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE))
  {
    default_test_definition(&test, 1);
    test.subtype = 0x2000;
    test.subtype_flag = 1;
    permanents_enters_battlefield_tapped(player, card, event, 1 - player, 0, &test);
  }
  if ((((event == EVENT_MODIFY_COST_GLOBAL) &&
        (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
       (result = is_what(affected_card_controller, affected_card, 1), result == 0)) &&
      (result = get_specific_storm_count(affected_card_controller), 0 < result))
  {
    infinite_casting_cost();
  }
  return 0;
}

/* ManalinkEh 2022: 0x2308080; original cards/zendikar_rising.c:521. */
int card_bela_ged_recovery(int player, int card, event_t event)
{
  /* Bala Ged Recovery	|2|G
   * Sorcery
   * Return target card from your graveyard to your hand. */

  int result;
  card_instance_t *instance;
  test_definition_t this_test;

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_test_definition(&this_test, 0xff);
  this_test.ai_selection_mode = 1;
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    generic_spell(player, card, EVENT_CAST_SPELL, 8, NULL, NULL, 1, &this_test);
    if (spell_fizzled != 1)
    {

      instance = get_card_instance(player, card);
      ai_modifier =
          ai_modifier +
          cards_ptr[graveyard_ptr_mutable[player][instance->targets[0].player]]->ai_base_value;
    }
    return 0;
  }
  if (event == EVENT_RESOLVE_SPELL)
  {

    this_test.ai_selection_mode = 1;
    result = validate_target_from_grave_source(player, card, player, 0);
    if (result != -1)
    {
      from_grave_to_hand(player, result, 1);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, EVENT_CAN_CAST, 8, NULL, NULL, 1, &this_test);
  result = mdfc_front_impl(player, card, event, result, 0);
  return result;
}

/* ManalinkEh 2022: 0x23082c0; original cards/zendikar_rising.c:569. */
int card_beyeen_veil(int player, int card, event_t event)
{
  /* Beyeen Veil	|1|U
   * Instant
   * Creatures your opponents control get -2/-0 until end of turn. */

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    pump_subtype_until_eot(player, card, 1 - player, -1, -2, 0, 0, 0);
    kill_card(player, card, KILL_DESTROY);
  }
  return mdfc_front_impl(player, card, event, 1, 0);
}

/* ManalinkEh 2022: 0x23083c0; original cards/zendikar_rising.c:591. */
int card_blackbloom_rogue(int player, int card, event_t event)
{
  /* Blackbloom Rogue	|2|B
   * Creature - Human Rogue 2/3
   * Menace (This creature can't be blocked except by two or more creatures.)
   * This creature gets +3/+0 as long as an opponent has eight or more cards in their graveyard. */

  int result;

  menace(player, card, event);
  if (((event == EVENT_POWER) && (card == affected_card)) && (player == affected_card_controller))
  {
    result = count_graveyard(1 - player);
    if (7 < result)
    {
      event_result = event_result + 3;
    }
  }
  result = mdfc_front_impl(player, card, event, 1, 0);
  return result;
}

/* ManalinkEh 2022: 0x2308450; original cards/zendikar_rising.c:619. */
int card_bloodchiefs_thirst(int player, int card, event_t event)
{
  /* Bloodchief's Thirst	|B
   * Sorcery
   * Kicker |2|B (You may pay an additional |2|B as you cast this spell.)
   * Destroy target creature or planeswalker with mana value 2 or less. If this spell was kicked, instead destroy target creature or planeswalker. */

  int result;
  const char *prompt;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x102);
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x102);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        result = is_token(player, card);
        if ((result == 0) && (result = check_special_flags(player, card, 0x40000), result == 0))
        {
          do_kicker(player, card, 2, 1, 0, 0, 0, 0);
        }
        result = kicked(player, card);
        if (result == 0)
        {
          td.extra = 2;
          td.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
          result = can_target(&td);
          if (result == 0)
          {
            spell_fizzled = 1;
            ai_modifier = ai_modifier + -999;
            return 0;
          }
        }
        result = kicked(player, card);
        prompt = "Select target permanent or player";
        if (result == 0)
        {
          prompt = "Select target permanent or player with mana value 2 or less";
        }
        generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td, prompt, 1, NULL);
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x102);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        kill_target0(player, card, KILL_DESTROY);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 0x102);
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x23086d0; original cards/zendikar_rising.c:659. */
int mdfc_front_land(int player, int card, event_t event)
{
  int result;

  if (event != EVENT_CAN_CAST)
  {
    mdfc_front_impl(player, card, event, 1, 0);
    result = plain_mana_producer(player, card, event);
    return result;
  }
  return mdfc_front_impl(player, card, EVENT_CAN_CAST, 1, 0);
}

/* ManalinkEh 2022: 0x2308740; original cards/zendikar_rising.c:691. */
int mdfc_back_land(int player, int card, event_t event)
{
  int result;

  mdfc_back(player, card, event);
  result = plain_mana_producer(player, card, event);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2308780; original cards/zendikar_rising.c:726. */
int card_brushfire_elemental(int player, int card, event_t event)
{
  /* Brushfire Elemental	|R|G
   * Creature - Elemental 1/1
   * Haste
   * This creature can't be blocked by creatures with power 2 or less.
   * Landfall - Whenever a land you control enters, this creature gets +2/+2 until end of turn. */

  int result;

  result = is_humiliated(player, card, AL_STATIC);
  if ((((event == EVENT_BLOCK_LEGALITY) && (result == 0)) &&
       (attacking_card_controller == player)) &&
      (attacking_card == card))
  {
    result = get_power(affected_card_controller, affected_card);
    if (result < 3)
    {
      event_result = 1;
    }
  }
  return card_steppe_lynx(player, card, event);
}

/* ManalinkEh 2022: 0x2308800; original cards/zendikar_rising.c:756. */
int card_canyon_jerboa(int player, int card, event_t event)
{
  /* Canyon Jerboa	|2|W
   * Creature - Mouse 1/2
   * Landfall - Whenever a land you control enters, creatures you control get +1/+1 until end of turn. */

  landfall(player, card, event);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  pump_creatures_until_eot_merge_pt(player, card, player, 1, 1, NULL);
  return 0;
}

/* ManalinkEh 2022: 0x2308870; original cards/zendikar_rising.c:790. */
int card_cleansing_wildfire(int player, int card, event_t event)
{
  /* Cleansing Wildfire	|1|R
   * Sorcery
   * Destroy target land. Its controller may search their library for a basic land card, put it onto the battlefield tapped, then shuffle.
   * Draw a card. */

  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, 1);
  if (player == 1)
  {
    td.required_subtype = 0x2000;
    td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      kill_target0(player, card, KILL_DESTROY);
      card_instance_t *instance = get_card_instance(player, card);
      int land_controller = instance->targets[0].player;
      if (land_controller != 0)
        tutor_basic_land(land_controller, 1, 0);
      else if (do_dialog(0, player, card, -1, -1, " Tutor a basic land\n Pass", 0) != 1)
        tutor_basic_land(instance->targets[0].player, 1, 0);
      draw_a_card(player);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_LAND", 1, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int is_instant_sorcery_or_wizard(int iid, int unused, int player, int card);
int mdfc_front(int player, int card, event_t event);
static int malakir_rebirth_effect(int player, int card, event_t event);
static int mdfc_front_impl(int player, int card, event_t event, int can_play_front,
                           int suppress_charging_back_cost);
int mdfc_back(int player, int card, event_t event);

/* ManalinkEh 2022: 0x2308a50; original cards/zendikar_rising.c:851. */
int card_confounding_conundrum(int player, int card, event_t event)
{
  /* Confounding Conundrum	|1|U
   * Enchantment
   * When this enchantment enters, draw a card.
   * Whenever a land an opponent controls enters, if that player had another land enter the battlefield under their control this turn, they return a land they control to its owner's hand. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  result = get_trap_condition(1 - player, 0x402);
  if (1 < result)
  {
    etb_ability_global(1 - player, event, 2, TYPE_LAND, NULL, player, card);
  }
  if (event == EVENT_ETB)
  {
    if ((trigger_cause_controller == player) && (trigger_cause == card))
    {
      draw_a_card(player);
    }
    else
    {
      default_target_definition(player, card, &td, 1);
      td.illegal_abilities = 0;
      td.allow_cancel = 0;
      td.allowed_controller = trigger_cause_controller;
      td.preferred_controller = trigger_cause_controller;
      td.who_chooses = trigger_cause_controller;
      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
      result = can_target(&td);
      if (result != 0)
      {
        result = new_pick_target(&td, "Select a land you control to return to your hand.", 0, 0x80);
        if (result != 0)
        {
          bounce_permanent(instance->targets[0].player, instance->targets[0].card);
        }
      }
      instance->number_of_targets = '\0';
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2308bb0; original cards/zendikar_rising.c:906. */
int card_crawling_barrens(int player, int card, event_t event)
{
  /* Crawling Barrens	""
   * Land
   * |T: Add |C.
   * |4: Put two +1/+1 counters on this land. Then you may have it become a 0/0 Elemental creature until end of turn. It's still a land. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_ANIMATE = 2
  };

  unsigned int saved_token_status;
  int paying_or_choice;
  card_instance_t *instance;
  int mana_available;
  unsigned int ability_available;

  if (is_humiliated(player, card, AL_TRIG) == 0)
  {
    comes_into_play_tapped(player, card, event);
  }
  if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_ACTIVATE))
  {
    has_subtype_if_animated_self(player, card, event, SUBTYPE_ELEMENTAL);
    return mana_producer(player, card, event);
  }

  instance = get_card_instance(player, card);
  saved_token_status = instance->token_status;
  paying_or_choice = paying_mana();
  ability_available = 0;
  if (paying_or_choice == 0)
  {
    paying_or_choice = can_use_activated_abilities(player, card);
    ability_available = (unsigned int)(paying_or_choice != 0);
  }
  paying_or_choice = paying_mana();
  mana_available = mana_producer(player, card, EVENT_CAN_ACTIVATE);
  paying_or_choice =
      dialog_impl(player, card, event, DLG_AUTOCHOOSE_IF_1, DLG_NO_DISPLAY_FOR_AI, "Produce mana", mana_available,
                  paying_or_choice, "Add counters", ability_available,
                  (saved_token_status >> 0xc ^ 1) & 1, DLG_MANA6(4, 0, 0, 0, 0, 0), 0, 0, 0, 0);
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (paying_or_choice != 0)
    {
      if (paying_or_choice != CHOICE_ANIMATE)
      {
        has_subtype_if_animated_self(player, card, event, SUBTYPE_ELEMENTAL);
        return mana_producer(player, card, event);
      }
      if (event == EVENT_RESOLVE_ACTIVATION)
      {

        instance = get_card_instance(player, card);
        add_counters(player, card, instance->parent_controller, instance->parent_card,
                     COUNTER_P1_P1, 2);
        paying_or_choice =
            dialog_impl(player, card, EVENT_RESOLVE_ACTIVATION, DLG_RANDOM, DLG_NO_DISPLAY_FOR_AI, "Animate",
                        1, 1, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        if (paying_or_choice == 1)
        {
          animate_self(player, card, 0, 0, KEYWORD_0, SP_KEYWORD_0, COLOR_TEST_0, 0);
          return 0;
        }
      }
    }
    paying_or_choice = 0;
  }
  return paying_or_choice;
}

/* ManalinkEh 2022: 0x2308e40; original cards/zendikar_rising.c:1014. */
int card_emerias_call(int player, int card, event_t event)
{
  /* Emeria's Call	|4|W|W|W
   * Sorcery
   * Create two 4/4 white Angel Warrior creature tokens with flying. Non-Angel creatures you control gain indestructible until your next turn. */

  pump_ability_t pump;
  test_definition_t test;

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    generate_tokens_by_id(player, card, CARD_ID_ANGEL_WARRIOR, 2);
    default_pump_ability_definition(player, card, &pump, 0, 0, 0, 0x800);
    pump.paue_flags = 0x8000;
    default_test_definition(&test, 2);
    test.subtype = 2;
    test.subtype_flag = 1;
    pump_ability_by_test(player, card, player, &pump, &test);
    kill_card(player, card, KILL_DESTROY);
  }
  return mdfc_front_impl(player, card, event, 1, 0);
}

/* ManalinkEh 2022: 0x2308ff0; original cards/zendikar_rising.c:1091. */
int card_feed_the_swarm(int player, int card, event_t event)
{
  /* Feed the Swarm	|1|B
   * Sorcery
   * Destroy target creature or enchantment an opponent controls. You lose life equal to that permanent's mana value. */

  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 6);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {

      kill_target0(player, card, KILL_DESTROY);
      card_instance_t *inst = get_card_instance(player, card);
      int cmc = get_cmc(inst->targets[0].player, inst->targets[0].card);
      lose_life(player, cmc);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td, "Select target enchantment or creature.",
                         1, NULL);
}

/* ManalinkEh 2022: 0x2309150; original cards/zendikar_rising.c:1112. */
int card_felidar_retreat(int player, int card, event_t event)
{
  /* Felidar Retreat	|3|W
   * Enchantment
   * Landfall - Whenever a land you control enters, choose one -
   * * Create a 2/2 white Cat Beast creature token.
   * * Put a +1/+1 counter on each creature you control. Those creatures gain vigilance until end of turn. */

  int result;
  card_instance_t *instance;
  int creature_card;

  landfall(player, card, event);
  if (event == EVENT_ETB)
  {

    result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
                         "Cat Beast", 1, 1, "+1/+1 counters", 1, 1, 0, 0, 0, 0);
    if (result == 1)
    {
      generate_token_by_id(player, card, CARD_ID_CAT_BEAST);
    }
    else
    {

      if ((result == 2) && (creature_card = active_cards_count[player] + -1, -1 < creature_card))
      {
        do
        {
          instance = in_play(player, creature_card);
          if ((instance != NULL) && (result = is_what(player, creature_card, 2), result != 0))
          {
            add_counters(player, card, player, creature_card, COUNTER_P1_P1, 1);
            pump_ability_until_eot_no_repeat(player, card, player, creature_card, 0, 8);
          }
          creature_card = creature_card + -1;
        } while (creature_card != -1);
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x23092f0; original cards/zendikar_rising.c:1189. */
int card_glasspool_mimic(int player, int card, event_t event)
{
  /* Glasspool Mimic	|2|U
   * Creature - Shapeshifter Rogue 0/0
   * You may have this creature enter as a copy of a creature you control, except it's a Shapeshifter Rogue in addition to its other types. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event == EVENT_RESOLVE_SPELL)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.allow_cancel = 1;
    result = enters_the_battlefield_as_copy_of_any(player, card, EVENT_RESOLVE_SPELL, &td,
                                                   "ASHNODS_BATTLEGEAR");

    if ((result != 0) &&
        (result = add_a_subtype_permanently(player, card, player, card, SUBTYPE_ROGUE),
         -1 < result))
    {
      instance = get_card_instance(player, result);
      instance->targets[2].card = 0xac;
    }
    return 0;
  }
  enters_the_battlefield_as_copy_of_any(player, card, event, NULL, "ASHNODS_BATTLEGEAR");
  result = mdfc_front_impl(player, card, event, 1, 0);
  return result;
}

/* ManalinkEh 2022: 0x23093f0; original cards/zendikar_rising.c:1262. */
int card_hagra_mauling(int player, int card, event_t event)
{
  /* Hagra Mauling	|2|B|B
   * Instant
   * This spell costs |1 less to cast if an opponent controls no basic lands.
   * Destroy target creature. */

  int result;
  target_definition_t td1;

  if (((event == EVENT_MODIFY_COST || event == EVENT_CHANGE_TYPE) ||
       event == EVENT_PLAY_FIRST_HALF) ||
      (event == EVENT_CAN_CAST))
  {
    default_target_definition(player, card, &td1, 2);
    if (event == EVENT_PLAY_FIRST_HALF)
    {
      generic_spell(player, card, EVENT_CAST_SPELL, 1, &td1, "TARGET_CREATURE", 1, NULL);
    }
  }
  else
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
    else if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
             (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
    default_target_definition(player, card, &td1, 2);
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td1);
    if (result != 0)
    {
      kill_target0(player, card, KILL_DESTROY);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td1, "TARGET_CREATURE", 1, NULL);
  return mdfc_front_impl(player, card, event, result, 0);
}

/* ManalinkEh 2022: 0x230aab0; original cards/zendikar_rising.c:1560. */
int card_jace_mirror_mage(int player, int card, event_t event)
{
  /* Jace, Mirror Mage	|1|U|U
   * Legendary Planeswalker - Jace (4)
   * Kicker |2
   * When Jace enters, if Jace was kicked, create a token that's a copy of Jace, except it's not legendary and its starting loyalty is 1.
   * +1: Scry 2.
   * 0: Draw a card and reveal it. Remove a number of loyalty counters equal to that card's mana value from Jace. */

  enum
  {
    CHOICE_SCRY = 1,
    CHOICE_DRAW = 2
  };

  target_definition_t td;
  int result;
  card_instance_t *instance;

  kicker(player, card, event, 2, 0, 0, 0, 0, 0);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = kicked(player, card);
    if (result != 0)
    {
      token_generation_t token;
      copy_token_definition(player, card, &token, player, card);
      token.action = 0x180;
      generate_token(&token);
    }
  }
  else if (event == EVENT_STATIC_EFFECTS)
  {
    if (!is_legendary(player, card))
      check_legend_rule(player, card, EVENT_STATIC_EFFECTS);
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if (card == affected_card && player == affected_card_controller)
    {
      instance = get_card_instance(player, card);
      return planeswalker(player, card, EVENT_CAST_SPELL,
                          (unsigned int)(instance->damage_source_card != 0x5568) * 3 + 1);
    }
    return planeswalker(player, card, event, 4);
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return planeswalker(player, card, event, 4);
  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (event == EVENT_ACTIVATE)
  {
    result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Scry 2", 1, 5, 1,
                         "Draw and reveal", 1, 6, 0, 0, 0, 0, 0);
    if (!result)
    {
      spell_fizzled = 1;
      return 0;
    }
    return planeswalker(player, card, event, 4);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    if (instance->info_slot == CHOICE_SCRY)
    {
      scry(player, 2);
    }
    else if (instance->info_slot == CHOICE_DRAW)
    {

      if (*graveyard_ptr_mutable[player] != -1)
      {
        reveal_card_iid(player, card, *graveyard_ptr_mutable[player]);
      }
      result = draw_a_card(player);
      if (result != -1)
      {

        instance = get_card_instance(player, card);
        result = get_cmc(player, result);
        remove_counters(player, card, instance->parent_controller, instance->parent_card,
                        COUNTER_LOYALTY, result);
      }
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x230ade0; original cards/zendikar_rising.c:1645. */
int card_jwari_disruption(int player, int card, event_t event)
{
  /* Jwari Disruption	|1|U
   * Instant
   * Counter target spell unless its controller pays |1. */

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_PLAY_FIRST_HALF)
    return counterspell(player, card, EVENT_CAST_SPELL, NULL, 0);
  if (event == EVENT_RESOLVE_SPELL)
  {
    counterspell_resolve_unless_pay_x(player, card, NULL, 0, 1);
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  int available = counterspell(player, card, EVENT_CAN_CAST, NULL, 0);
  return mdfc_front_impl(player, card, event, available, 0);
}

/* ManalinkEh 2022: 0x230af30; original cards/zendikar_rising.c:1676. */
int card_kabira_takedown(int player, int card, event_t event)
{
  /* Kabira Takedown	|1|W
   * Instant
   * ~ deals damage equal to the number of creatures you control to target creature or planeswalker. */

  int result;
  target_definition_t td;

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 0x102);
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    result = generic_spell(player, card, EVENT_CAST_SPELL, 1, &td,
                           "TARGET_CREATURE_OR_PLANESWALKER", 1, NULL);
    return result;
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      result = count_subtype(player, 2, -1);
      damage_target0(player, card, result);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE_OR_PLANESWALKER", 1,
                         NULL);
  result = mdfc_front_impl(player, card, event, result, 0);
  return result;
}

/* ManalinkEh 2022: 0x230b140; original cards/zendikar_rising.c:1724. */
int card_kazandu_mammoth(int player, int card, event_t event)
{
  /* Kazandu Mammoth	|1|G|G
   * Creature - Elephant 3/3
   * Landfall - Whenever a land you control enters, this creature gets +2/+2 until end of turn. */

  int result;

  card_steppe_lynx(player, card, event);
  result = mdfc_front_impl(player, card, event, 1, 0);
  return result;
}

/* ManalinkEh 2022: 0x230b4a0; original cards/zendikar_rising.c:1800. */
int card_khalni_ambush(int player, int card, event_t event)
{
  /* Khalni Ambush	|2|G
   * Instant
   * Target creature you control fights target creature you don't control. (Each deals damage equal to its power to the other.) */

  card_instance_t *instance;
  int result;
  unsigned int can_play_front;
  target_definition_t *opponent_target;
  target_definition_t td;
  target_definition_t td1;

  if (((event == EVENT_MODIFY_COST || event == EVENT_CHANGE_TYPE) ||
       event == EVENT_PLAY_FIRST_HALF) ||
      (event == EVENT_CAN_CAST))
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    default_target_definition(player, card, &td1, 2);
    td1.allowed_controller = 1 - player;
    td1.preferred_controller = td1.allowed_controller;
    instance = get_card_instance(player, card);
    if (event == EVENT_PLAY_FIRST_HALF)
    {
      instance->number_of_targets = '\0';
      result = new_pick_target(&td, "Select target creature you control.", 0, 0x81);
      if (result != 0)
      {
        new_pick_target(&td1, "Select target creature opponent controls.", 1, 0x81);
      }
    }
  }
  else
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
    else if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
             (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    default_target_definition(player, card, &td1, 2);
    td1.allowed_controller = 1 - player;
    td1.preferred_controller = td1.allowed_controller;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    opponent_target = &td1;
    result = validate_target(player, card, &td, 0);
    if ((result != 0) && (result = validate_target(player, card, opponent_target, 1), result != 0))
    {
      fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player,
            instance->targets[1].card);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  opponent_target = &td1;
  result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
  can_play_front = 0;
  if (result != 0)
  {
    result = can_target(opponent_target);
    can_play_front = (unsigned int)(result != 0);
  }
  return mdfc_front_impl(player, card, event, can_play_front, 0);
}

/* ManalinkEh 2022: 0x230b7a0; original cards/zendikar_rising.c:1863. */
int card_legion_angel(int player, int card, event_t event)
{
  /* Legion Angel	|2|W|W
   * Creature - Angel Warrior 4/3
   * Flying
   * When this creature enters, you may reveal a card you own named ~ from outside the game and put it into your hand. */

  int result;
  card_instance_t *instance;
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  result = add_card_to_hand(player, instance->internal_card_id);
  reveal_card(player, card, player, result);
  return 0;
}

/* ManalinkEh 2022: 0x230b830; original cards/zendikar_rising.c:1878. */
int card_leyline_tyrant(int player, int card, event_t event)
{
  /* Leyline Tyrant	|2|R|R
   * Creature - Dragon 4/4
   * Flying
   * You don't lose unspent red mana as steps and phases end.
   * When this creature dies, you may pay any amount of |R. When you do, it deals that much damage to any target. */

  int result;
  target_definition_t td;

  if (event == EVENT_MANA_POOL_DRAINING)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      mana_doesnt_drain_from_pool[player][4] = mana_doesnt_drain_from_pool[player][4] | 0x80;
    }
  }
  result = duh_mode(player);
  result = this_dies_trigger(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (result != 0)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    result = new_pick_target(&td, "TARGET_ANY", 0, 1);
    if (result != 0)
    {

      result =
          charge_mana_for_activated_ability_while_resolving(player, card, event, 0, 0, 0, 0, -1, 0);
      if (0 < result)
      {
        damage_target0(player, card, result);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x230b970; original cards/zendikar_rising.c:1941. */
int card_luminarch_aspirant(int player, int card, event_t event)
{
  /* Luminarch Aspirant	|1|W
   * Creature - Human Cleric 1/1
   * At the beginning of combat on your turn, put a +1/+1 counter on target creature you control. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  result = beginning_of_combat(player, card, event, player);
  if (result != 0)
  {

    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    instance->number_of_targets = '\0';
    td.preferred_controller = player;
    td.allowed_controller = player;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0);
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_P1_P1, 1);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x230ba50; original cards/zendikar_rising.c:1963. */
int card_maddening_cacophony(int player, int card, event_t event)
{
  /* Maddening Cacophony	|1|U
   * Sorcery
   * Kicker |3|U
   * Each opponent mills eight cards. If this spell was kicked, instead each opponent mills half their library, rounded up. */

  unsigned int mill_amount;
  int result;
  int opponent;
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card != affected_card) || (player != affected_card_controller))
      {
        return 0;
      }
      kicker(player, card, EVENT_CAST_SPELL, 3, 0, 1, 0, 0, 0);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        ai_modifier = ai_modifier + 0x60;
      }
      return basic_spell(player, card, event);
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      kicker(player, card, event, 3, 0, 1, 0, 0, 0);
      if (event == EVENT_RESOLVE_SPELL)
      {
        result = kicked(player, card);
        opponent = 1 - player;
        mill_amount = 8;
        if (result != 0)
        {
          mill_amount = count_deck(opponent);
          mill_amount = round_up_value(mill_amount);
        }
        mill(opponent, mill_amount);
        kill_card(player, card, KILL_DESTROY);
      }
      return basic_spell(player, card, event);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  kicker(player, card, event, 3, 0, 1, 0, 0, 0);
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x230bc20; original cards/zendikar_rising.c:1987. */
int card_magmatic_channeler(int player, int card, event_t event)
{
  /* Magmatic Channeler	|1|R
   * Creature - Human Wizard 1/3
   * As long as there are four or more instant and/or sorcery cards in your graveyard, this creature gets +3/+1.
   * |T, Discard a card: Exile the top two cards of your library, then choose one of them. You may play that card this turn. */

  enum
  {
    TUTOR_EXILE = 6
  };

  int result;
  int iids[2];
  test_definition_t test;

  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (result = count_graveyard_by_type(player, 0x38), 3 < result))
    {
      event_result = event_result + (unsigned int)(event == EVENT_POWER) * 2 + 1;
    }
    return 0;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event != EVENT_RESOLVE_ACTIVATION)
    return generic_activated_ability(player, card, event, 0x21, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  default_test_definition(&test, 0xff);
  iids[0] = *deck_ptr[player];
  iids[1] = deck_ptr[player][1];
  result = reveal_top_cards_of_library_and_choose(player, card, player, 2, 1, TUTOR_EXILE, 1,
                                                  TUTOR_EXILE, 0, &test);
  create_may_play_card_from_exile_effect(
      player, card, player, (unsigned int)cards_data[iids[result]].id, MPCFE_UNTIL_EOT);
  return generic_activated_ability(player, card, event, 0x21, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x230bde0; original cards/zendikar_rising.c:2011. */
int card_makindi_ox(int player, int card, event_t event)
{
  /* Makindi Ox	|4|W
   * Creature - Ox 4/4
   * Landfall - Whenever a land you control enters, tap target creature an opponent controls. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  landfall(player, card, event);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  td.allow_cancel = 0;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS");
    if (result != 0)
    {
      tap_card(instance->targets[0].player, instance->targets[0].card);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x230bea0; original cards/zendikar_rising.c:2034. */
int card_makindi_stampede(int player, int card, event_t event)
{
  /* Makindi Stampede	|3|W|W
   * Sorcery
   * Creatures you control get +2/+2 until end of turn. */

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    pump_creatures_until_eot(player, card, player, 0, 2, 2, 0, 0, NULL);
    kill_card(player, card, KILL_DESTROY);
  }
  return mdfc_front_impl(player, card, event, 1, 0);
}

/* ManalinkEh 2022: 0x2306820; original cards/zendikar_rising.c:2059. */
static int malakir_rebirth_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (!resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    return 0;
  }
  int owner = instance->targets[0].player;
  seek_grave_for_id_to_reanimate(
      owner, card, owner, (unsigned int)cards_data[instance->targets[0].card].id, REANIMATE_TAP);
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x2306700; original cards/zendikar_rising.c:2070. */
static int malakir_rebirth_effect(int player, int card, event_t event)
{
  char attached_player;
  card_instance_t *instance;
  int result;
  card_instance_t *return_effect;

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {

    instance = in_play(player, card);
    if ((((instance != NULL) &&
          (attached_player = instance->damage_target_player, attached_player != -1)) &&
         (instance->damage_target_card == affected_card)) &&
        (attached_player == affected_card_controller))
    {
      result = is_this_dying((int)attached_player, instance->damage_target_card);
      if (result != 0)
      {
        result = check_for_death_trigger_removal(affected_card_controller, affected_card);
        if (result == 0)
        {

          result = create_legacy_effect(player, card, malakir_rebirth_legacy);
          return_effect = get_card_instance(player, result);
          result = get_owner((int)instance->damage_target_player, instance->damage_target_card);
          return_effect->targets[0].player = result;
          result = get_original_internal_card_id((int)instance->damage_target_player,
                                                 instance->damage_target_card);
          return_effect->targets[0xb].player = 1;
          return_effect->targets[0].card = result;
        }
      }
    }
  }
  else if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x230bfa0; original cards/zendikar_rising.c:2089. */
int card_malakir_rebirth(int player, int card, event_t event)
{
  /* Malakir Rebirth	|B
   * Instant
   * Choose target creature. You lose 2 life. Until end of turn, that creature gains "When this creature dies, return it to the battlefield tapped under its owner's control." */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    result = generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
    return result;
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {

      instance = get_card_instance(player, card);
      create_targetted_legacy_effect(player, card, malakir_rebirth_effect,
                                     instance->targets[0].player, instance->targets[0].card);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
  result = mdfc_front_impl(player, card, event, result, 0);
  return result;
}

/* ManalinkEh 2022: 0x230c1b0; original cards/zendikar_rising.c:2212. */
int card_myriad_construct(int player, int card, event_t event)
{
  /* Myriad Construct	|4
   * Artifact Creature - Construct 4/4
   * Kicker |3
   * If this creature was kicked, it enters with a +1/+1 counter on it for each nonbasic land your opponents control.
   * When this creature becomes the target of a spell, sacrifice it and create a number of 1/1 colorless Construct artifact creature tokens equal to its power. */

  int was_kicked;
  const target_t *targeting_spell;
  card_instance_t *instance;
  int counters_or_power;
  token_generation_t token;

  kicker(player, card, event, 3, 0, 0, 0, 0, 0);
  was_kicked = kicked(player, card);
  counters_or_power = 0;
  if (was_kicked != 0)
  {
    counters_or_power = count_nonbasic_lands(1 - player);
  }
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, counters_or_power);
  targeting_spell =
      becomes_target_of_spell(player, card, event, player, card, 2, RESOLVE_TRIGGER_MANDATORY);
  if (targeting_spell != NULL)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      if (is_humiliated(player, card, AL_TRIG) == 0)
      {

        counters_or_power = get_power(player, card);
        kill_card(player, card, KILL_SACRIFICE);
        default_token_definition(player, card, CARD_ID_CONSTRUCT, &token);
        token.tou = 1;
        token.pow = 1;
        token.qty = (unsigned char)counters_or_power;
        generate_token(&token);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x230c310; original cards/zendikar_rising.c:2245. */
int card_nahiri_heir_of_the_ancients(int player, int card, event_t event)
{
  /* Nahiri, Heir of the Ancients	|2|R|W
   * Legendary Planeswalker - Nahiri (4)
   * +1: Create a 1/1 white Kor Warrior creature token. You may attach an Equipment you control to it.
   * -2: Look at the top six cards of your library. You may reveal a Warrior or Equipment card from among them and put it into your hand. Put the rest on the bottom of your library in a random order.
   * -3: Nahiri deals damage to target creature or planeswalker equal to twice the number of Equipment you control. */

  enum
  {
    CHOICE_KOR = 1,
    CHOICE_DIG = 2,
    CHOICE_DAMAGE = 3
  };

  enum
  {
    TOKEN_ACTION_EQUIP_ANY = 8
  };

  int result;
  card_instance_t *instance;
  test_definition_t *equipment_test;
  target_definition_t *damage_target;
  token_generation_t token;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x102);
    default_test_definition(&test, 0xff);
    test.subtype = 0xd2;
    test.sub2 = 0x3001;
    test.subtype_flag = 3;
    if (event == EVENT_ACTIVATE)
    {

      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = can_target(&td);
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Kor token", 1, 5,
                           1, "Dig for a warrior or equpment", 1, 10, 0xfffffffe,
                           "Damage creature or planeswalker", result, 0x14, 0xfffffffd, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_DAMAGE)
      {
        new_pick_target(&td, "TARGET_CREATURE_OR_PLANESWALKER", 0, 1);
      }
      return planeswalker(player, card, event, 4);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 4);
    default_target_definition(player, card, &td, 0x102);
    default_test_definition(&test, 0xff);
  }
  damage_target = &td;
  equipment_test = &test;
  test.sub2 = 0x3001;
  test.subtype_flag = 3;
  test.subtype = 0xd2;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    result = instance->info_slot;
    if (result == CHOICE_KOR)
    {
      default_token_definition(player, card, CARD_ID_KOR_WARRIOR, &token);
      token.action = TOKEN_ACTION_EQUIP_ANY;
      generate_token(&token);
    }
    else if (result == CHOICE_DIG)
    {
      reveal_top_cards_of_library_and_choose(player, card, player, 6, 0, TUTOR_HAND, 1,
                                             TUTOR_BOTTOM_OF_DECK_RANDOM, 0, equipment_test);
    }
    else if ((result == CHOICE_DAMAGE) && (result = valid_target(damage_target), result != 0))
    {
      result = count_subtype(player, 0xff, 0x3001);
      damage_target0(player, card, result * 2);
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x230c650; original cards/zendikar_rising.c:2317. */
int card_nighthawk_scavenger(int player, int card, event_t event)
{
  /* Nighthawk Scavenger	|1|B|B
   * Creature - Vampire Rogue 1+* /3
   * Flying, deathtouch, lifelink
   * ~'s power is equal to 1 plus the number of card types among cards in your opponents' graveyards. */

  int result;

  deathtouch(player, card, event);
  lifelink(player, card, event);
  if (((event == EVENT_POWER) && (card == affected_card)) && (player == affected_card_controller))
  {
    result = count_types_in_grave(1 - player);
    set_base_pt(result + 1);
  }
  return 0;
}

/* ManalinkEh 2022: 0x230c6d0; original cards/zendikar_rising.c:2348. */
int card_nissa_of_shadowed_boughs(int player, int card, event_t event)
{
  /* Nissa of Shadowed Boughs	|2|B|G
   * Legendary Planeswalker - Nissa (4)
   * Landfall - Whenever a land you control enters, put a loyalty counter on Nissa.
   * +1: Untap target land you control. You may have it become a 3/3 Elemental creature with haste and menace until end of turn. It's still a land.
   * -5: You may put a creature card with mana value less than or equal to the number of lands you control onto the battlefield from your hand or graveyard with two +1/+1 counters on it. */

  enum
  {
    CHOICE_UNTAP = 1,
    CHOICE_CHEAT_IN = 2
  };

  int choice;
  int land_count_or_hand_available;
  card_instance_t *instance;
  int grave_available;
  int selected_creature;
  card_instance_t *animation;
  unsigned int creature_available;
  target_definition_t *land_target;
  test_definition_t *creature_test;
  target_definition_t td;
  test_definition_t test;

  landfall(player, card, event);
  if (event == EVENT_ETB)
  {
    if ((trigger_cause_controller != player) || (trigger_cause != card))
    {
      add_counters(player, card, player, card, COUNTER_LOYALTY, 1);
    }
    check_legend_rule(player, card, EVENT_ETB);
  }
  else
  {
    check_legend_rule(player, card, event);
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_target_definition(player, card, &td, 1);
      td.allowed_controller = player;
      default_test_definition(&test, 2);
      land_count_or_hand_available = count_permanents_by_type(player, TYPE_LAND);
      test.cmc = land_count_or_hand_available + 1;
      test.cmc_flag = 3;
      test.zone = 0x100;
      instance = get_card_instance(player, card);
      if (event == EVENT_ACTIVATE)
      {

        instance->number_of_targets = '\0';
        instance->info_slot = 0;
        land_count_or_hand_available = new_special_count_grave(player, &test);
        creature_available = 1;
        if (land_count_or_hand_available == 0)
        {
          land_count_or_hand_available =
              check_battlefield_for_special_card(player, card, player, 0, &test);
          creature_available = (unsigned int)(land_count_or_hand_available != 0);
        }
        choice =
            dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                        "Untap & animate a land", can_target(&td), 5, 1, "Put a creature into play",
                        creature_available, 10, 0xfffffffb, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (choice == CHOICE_UNTAP)
        {
          new_pick_target(&td, "Select a land you control.", 0, 0x81);
        }
        return planeswalker(player, card, event, 4);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return planeswalker(player, card, event, 4);
      default_target_definition(player, card, &td, 1);
      td.allowed_controller = player;
      default_test_definition(&test, 2);
      land_count_or_hand_available = count_permanents_by_type(player, TYPE_LAND);
      test.cmc = land_count_or_hand_available + 1;
      test.cmc_flag = 3;
      test.zone = 0x100;
      instance = get_card_instance(player, card);
    }
    creature_test = &test;
    land_target = &td;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      if (instance->info_slot == CHOICE_UNTAP)
      {
        if (valid_target(land_target) != 0)
        {
          untap_card(instance->targets[0].player, instance->targets[0].card);
          choice = dialog_impl(player, card, EVENT_RESOLVE_ACTIVATION, DLG_RANDOM, DLG_NO_CANCEL,
                               "Animate", 1, 10, "Do not animate", 1, 1, 0, 0, 0, 0);
          if (choice == 1)
          {

            instance = get_card_instance(player, card);
            land_count_or_hand_available =
                land_animation2(player, card, instance->targets[0].player,
                                instance->targets[0].card, 1, 3, 3, 0, 0x10004, 0, NULL);
            animation = get_card_instance(player, land_count_or_hand_available);
            animation->targets[9].card = 0x3a;
            animation = get_card_instance(player, land_count_or_hand_available);
            animation->targets[9].player = 1;
            instance = get_card_instance(instance->targets[0].player, instance->targets[0].card);
            instance->regen_status = instance->regen_status | 0x1f000000;
          }
        }
      }
      else if (instance->info_slot == CHOICE_CHEAT_IN)
      {

        land_count_or_hand_available =
            check_battlefield_for_special_card(player, card, player, 0, creature_test);
        grave_available = new_special_count_grave(player, creature_test);
        if (land_count_or_hand_available == 0 && grave_available == 0)
        {
          return 0;
        }
        do
        {
          if (land_count_or_hand_available != 0 &&
              (selected_creature = new_select_a_card(player, player, 4, 0, 2, -1, creature_test)) >=
                  0)
          {
            put_into_play(player, selected_creature);
            break;
          }
          if (grave_available != 0 && (selected_creature = new_select_a_card(
                                           player, player, 2, 0, 2, -1, creature_test)) >= 0)
          {
            selected_creature =
                reanimate_permanent(player, card, player, selected_creature, REANIMATE_DEFAULT);
            break;
          }
        } while (true);
        hack_silent_counters = 1;
        add_counters(player, card, player, selected_creature, COUNTER_P1_P1, 2);
        hack_silent_counters = 0;
      }
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x230ccf0; original cards/zendikar_rising.c:2464. */
int card_obuun_mul_daya_ancestor(int player, int card, event_t event)
{
  /* Obuun, Mul Daya Ancestor	|1|R|G|W
   * Legendary Creature - Elf Spirit 3/3
   * At the beginning of combat on your turn, up to one target land you control becomes an X/X Elemental creature with trample and haste until end of turn, where X is Obuun's power. It's still a land.
   * Landfall - Whenever a land you control enters, put a +1/+1 counter on target creature. */

  int result;
  card_instance_t *obuun;
  card_instance_t *animation_or_land;
  target_definition_t td;

  check_legend_rule(player, card, event);
  landfall(player, card, event);
  result = beginning_of_combat(player, card, event, player);
  if (result != 0)
  {

    default_target_definition(player, card, &td, 1);
    td.allowed_controller = player;
    obuun = get_card_instance(player, card);
    result = can_target(&td);
    if ((result != 0) &&
        (result = new_pick_target(&td, "Select a land you control.", 0, 0x80), result != 0))
    {

      result = get_power(player, card);
      result = land_animation2(player, card, obuun->targets[0].player, obuun->targets[0].card, 1,
                               result, result, 0x80, 4, 0, NULL);
      animation_or_land = get_card_instance(player, result);
      animation_or_land->targets[9].card = 0x3a;
      animation_or_land = get_card_instance(player, result);
      animation_or_land->targets[9].player = 1;
      animation_or_land = get_card_instance(obuun->targets[0].player, obuun->targets[0].card);
      animation_or_land->regen_status = animation_or_land->regen_status | 0x1f000000;
      get_abilities(obuun->targets[0].player, obuun->targets[0].card, EVENT_CHANGE_TYPE, -1);
    }
    obuun->number_of_targets = '\0';
  }
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allow_cancel = 0;
    obuun = get_card_instance(player, card);
    result = can_target(&td);
    if ((result != 0) && (result = new_pick_target(&td, "TARGET_CREATURE", 0, 0), result != 0))
    {
      add_counters(player, card, obuun->targets[0].player, obuun->targets[0].card, COUNTER_P1_P1,
                   1);
    }
    obuun->number_of_targets = '\0';
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x230d0f0; original cards/zendikar_rising.c:2542. */
int card_ondu_inversion(int player, int card, event_t event)
{
  /* Ondu Inversion	|6|W|W
   * Sorcery
   * Destroy all nonland permanents. */

  test_definition_t this_test;

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&this_test, 1);
    this_test.type_flag = 1;
    new_manipulate_all(player, card, 2, &this_test, ACT_KILL_DESTROY);
    kill_card(player, card, KILL_DESTROY);
  }
  return mdfc_front_impl(player, card, event, 1, 0);
}

/* ManalinkEh 2022: 0x230d1f0; original cards/zendikar_rising.c:2584. */
int card_pelakka_predation(int player, int card, event_t event)
{
  /* Pelakka Predation	|2|B
   * Sorcery
   * Target opponent reveals their hand. You choose a card from it with mana value 3 or greater. That player discards that card. */

  int result;
  card_instance_t *instance;
  ec_definition_t ec;
  target_definition_t td;
  test_definition_t this_test;

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_PLAY_FIRST_HALF)
    target_opponent(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    result = valid_target(&td);
    if (result != 0)
    {

      instance = get_card_instance(player, card);
      default_ec_definition(instance->targets[0].player, player, &ec);
      new_default_test_definition(&this_test, 0xff, "Select a card with CMC 3 or greater.");
      this_test.cmc = 2;
      this_test.cmc_flag = 2;
      new_effect_coercion(&ec, &this_test);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = opponent_is_valid_target(player, card);
  return mdfc_front_impl(player, card, event, result, 0);
}

/* ManalinkEh 2022: 0x230d390; original cards/zendikar_rising.c:2622. */
int card_phylath_world_sculptor(int player, int card, event_t event)
{
  /* Phylath, World Sculptor	|4|R|G
   * Legendary Creature - Elemental 5/5
   * When Phylath enters, create a 0/1 green Plant creature token for each basic land you control.
   * Landfall - Whenever a land you control enters, put four +1/+1 counters on target Plant you control. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  landfall(player, card, event);
  if (event == EVENT_ETB)
  {
    if ((trigger_cause_controller == player) && (trigger_cause == card))
    {
      result = count_basic_lands(player, -1);
      generate_tokens_by_id(player, card, CARD_ID_PLANT, result);
    }
    else
    {
      default_target_definition(player, card, &td, 0xff);
      td.required_subtype = 0x96;
      td.allowed_controller = player;
      td.allow_cancel = 0;
      result = can_target(&td);
      if (result != 0)
      {
        result = new_pick_target(&td, "Select a plant you control.", 0, 0x81);
        if (result != 0)
        {

          instance = get_card_instance(player, card);
          add_counters(player, card, instance->targets[0].player,
                                   instance->targets[0].card, COUNTER_P1_P1, 4);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x230d4d0; original cards/zendikar_rising.c:2655. */
int card_prowling_felidar(int player, int card, event_t event)
{
  /* Prowling Felidar	|3|W
   * Creature - Cat Beast 2/3
   * Vigilance
   * Landfall - Whenever a land you control enters, put a +1/+1 counter on this creature. */

  vigilance(player, card, event);
  landfall(player, card, event);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  return 0;
}

/* ManalinkEh 2022: 0x230d550; original cards/zendikar_rising.c:2768. */
int card_ruin_crab(int player, int card, event_t event)
{
  /* Ruin Crab	|U
   * Creature - Crab 0/3
   * Landfall - Whenever a land you control enters, each opponent mills three cards. (To mill a card, a player puts the top card of their library into their graveyard.) */

  landfall(player, card, event);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  mill(1 - player, 3);
  return 0;
}

/* ManalinkEh 2022: 0x230d5b0; original cards/zendikar_rising.c:2780. */
int card_scale_the_heights(int player, int card, event_t event)
{
  /* Scale the Heights	|2|G
   * Sorcery
   * Put a +1/+1 counter on up to one target creature. You gain 2 life. You may play an additional land this turn.
   * Draw a card. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
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
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      td.allow_cancel = 3;
      if ((card == affected_card) && (player == affected_card_controller))
      {
        result = can_target(&td);
        if (result != 0)
        {
          new_pick_target(&td, "TARGET_CREATURE", 0, 0);
        }
        default_test_definition(&test, 1);
        test.zone = 0x100;
        result = check_battlefield_for_special_card(player, card, player, 4, &test);
        ai_modifier = ai_modifier + result * 0xc;
      }
      return basic_spell(player, card, event);
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      td.allow_cancel = 3;
      if (event == EVENT_RESOLVE_SPELL)
      {

        instance = get_card_instance(player, card);
        if ((instance->number_of_targets != '\0') && (result = valid_target(&td), result != 0))
        {
          add_counters(player, card, instance->targets[0].player,
                                   instance->targets[0].card, COUNTER_P1_P1, 1);
        }
        gain_life(player, 2);
        result = create_legacy_effect(player, card, check_playable_lands_legacy);
        instance = get_card_instance(player, result);
        result = get_id(player, card);
        instance->targets[2].card = result;
        draw_cards(player, 1);
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
  td.preferred_controller = player;
  td.allow_cancel = 3;
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x230d850; original cards/zendikar_rising.c:2839. */
int card_scourge_of_the_skyclaves(int player, int card, event_t event)
{
  /* Scourge of the Skyclaves	|1|B
   * Creature - Demon * / *
   * Kicker |4|B
   * When you cast this spell, if it was kicked, each player loses half their life, rounded up.
   * ~'s power and toughness are each equal to 20 minus the highest life total among players. */

  kicker(player, card, event, 4, 1, 0, 0, 0, 0);
  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if (card == affected_card && player == affected_card_controller)
    {
      int highest_life = life[1 - player];
      if (life[1 - player] < life[player])
        highest_life = life[player];
      set_base_pt(0x14 - highest_life);
    }
  }
  else if (event == EVENT_CAST_SPELL && card == affected_card &&
           player == affected_card_controller)
  {
    int was_kicked = kicked(player, card);
    int p = current_turn;
    if (was_kicked)
    {
      unsigned int life_loss = round_up_value(life[current_turn]);
      lose_life(p, life_loss);
      p = current_turn ^ 1;
      life_loss = round_up_value(life[p]);
      lose_life(p, life_loss);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x230d980; original cards/zendikar_rising.c:2856. */
int card_scute_swarm(int player, int card, event_t event)
{
  /* Scute Swarm	|2|G
   * Creature - Insect 1/1
   * Landfall - Whenever a land you control enters, create a 1/1 green Insect creature token. If you control six or more lands, create a token that's a copy of this creature instead. */

  int result;
  token_generation_t token;
  landfall(player, card, event);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  result = count_permanents_by_type(player, TYPE_LAND);
  if (result < 6)
  {
    default_token_definition(player, card, CARD_ID_INSECT, &token);
  }
  else
  {
    copy_token_definition(player, card, &token, player, card);
  }
  generate_token(&token);
  return 0;
}

/* ManalinkEh 2022: 0x230da20; original cards/zendikar_rising.c:2883. */
int card_sea_gate_restoration(int player, int card, event_t event)
{
  /* Sea Gate Restoration	|4|U|U|U
   * Sorcery
   * Draw cards equal to the number of cards in your hand plus one. You have no maximum hand size for the rest of the game. */

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    draw_cards(player, hand_count[player] + 1);
    create_legacy_effect(player, card, card_spellbook);
    kill_card(player, card, KILL_DESTROY);
  }
  return mdfc_front_impl(player, card, event, 1, 0);
}

/* ManalinkEh 2022: 0x23068c0; original cards/zendikar_rising.c:2906. */
int doublecast_legacy_mana_value_2_or_less(int player, int card, event_t event)
{
  int result;
  test_definition_t test;
  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = check_special_flags(trigger_cause_controller, trigger_cause, 0x40000);
    if (result == 0)
    {
      default_test_definition(&test, 0x38);
      test.cmc = 3;
      test.cmc_flag = 3;
      result = new_specific_spell_played(player, card, event, player, 2, &test);
      if (result != 0)
      {
        copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x23069f0; original cards/zendikar_rising.c:2923. */
int triplecast_legacy_mana_value_2_or_less(int player, int card, event_t event)
{
  int t_card;
  int spell_controller;
  int triggered;
  test_definition_t test;
  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    if (check_special_flags(trigger_cause_controller, trigger_cause, 0x40000) == 0)
    {
      default_test_definition(&test, 0x38);
      test.cmc = 3;
      test.cmc_flag = 3;
      triggered = new_specific_spell_played(player, card, event, player, 2, &test);
      t_card = trigger_cause;
      spell_controller = trigger_cause_controller;
      if (triggered != 0)
      {
        copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
        copy_spell_from_stack(player, spell_controller, t_card);
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x230db10; original cards/zendikar_rising.c:2942. */
int card_sea_gate_stormcaller(int player, int card, event_t event)
{
  /* Sea Gate Stormcaller	|1|U
   * Creature - Human Wizard 2/1
   * Kicker |4|U
   * When this creature enters, copy the next instant or sorcery spell with mana value 2 or less you cast this turn when you cast it. If this creature was kicked, copy that spell twice instead. You may choose new targets for the copies. */

  int result;
  kicker(player, card, event, 1, 0, 1, 0, 0, 0);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  result = kicked(player, card);
  if (result != 0)
  {
    create_legacy_effect(player, card, triplecast_legacy_mana_value_2_or_less);
    return 0;
  }
  create_legacy_effect(player, card, doublecast_legacy_mana_value_2_or_less);
  return 0;
}

/* ManalinkEh 2022: 0x230dbe0; original cards/zendikar_rising.c:2964. */
int card_sejiri_shelter(int player, int card, event_t event)
{
  /* Sejiri Shelter	|1|W
   * Instant
   * Target creature you control gains protection from the color of your choice until end of turn. */

  card_instance_t *instance;
  int result;
  unsigned int protection_color;
  target_definition_t td;

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = player;
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  if (event == EVENT_PLAY_FIRST_HALF)
    return generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {

      protection_color = select_a_protection(player);
      pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                             0, 0, protection_color | 0x10000000, 0);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
  return mdfc_front_impl(player, card, event, result, 0);
}

/* ManalinkEh 2022: 0x230e2d0; original cards/zendikar_rising.c:3085. */
int card_silundi_vision(int player, int card, event_t event)
{
  /* Silundi Vision	|2|U
   * Instant
   * Look at the top six cards of your library. You may reveal an instant or sorcery card from among them and put it into your hand. Put the rest on the bottom of your library in a random order. */

  test_definition_t test;

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    new_default_test_definition(&test, 0x38, "Select an instant or sorcery card.");
    reveal_top_cards_of_library_and_choose(player, card, player, 6, 0, TUTOR_HAND, 1,
                                           TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test);
    kill_card(player, card, KILL_DESTROY);
  }
  return mdfc_front_impl(player, card, event, 1, 0);
}

/* ManalinkEh 2022: 0x230e5d0; original cards/zendikar_rising.c:3147. */
int card_skyclave_cleric(int player, int card, event_t event)
{
  /* Skyclave Cleric	|1|W
   * Creature - Kor Cleric 1/3
   * When this creature enters, you gain 2 life. */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    gain_life(player, 2);
    return 0;
  }
  if (((event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE) &&
       event != EVENT_PLAY_FIRST_HALF) &&
      (event != EVENT_CAN_CAST))
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card != affected_card) || (player != affected_card_controller))
      {
        return 0;
      }
    }
    else if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
             (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  result = mdfc_front_impl(player, card, event, 1, 0);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x230e6a0; original cards/zendikar_rising.c:3183. */
int card_skyclave_relic(int player, int card, event_t event)
{
  /* Skyclave Relic	|3
   * Artifact
   * Kicker |3
   * Indestructible
   * When this artifact enters, if it was kicked, create two tapped tokens that are copies of this artifact.
   * |T: Add one mana of any color. */

  int result;
  token_generation_t token;

  indestructible(player, card, event);
  kicker(player, card, event, 3, 0, 0, 0, 0, 0);
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_token(player, card);
    if (result == 0)
    {
      etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    }
  }
  if (event == EVENT_ETB)
  {
    result = kicked(player, card);
    if (result != 0)
    {
      copy_token_definition(player, card, &token, player, card);
      token.action = TOKEN_ACTION_TAPPED;
      token.qty = '\x02';
      generate_token(&token);
    }
  }
  result = plain_mana_producer(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x230e8e0; original cards/zendikar_rising.c:3240. */
int card_skyclave_squid(int player, int card, event_t event)
{
  /* Skyclave Squid	|1|U
   * Creature - Squid 3/2
   * Defender
   * Landfall - Whenever a land you control enters, this creature can attack this turn as though it didn't have defender. */

  int leg;
  card_instance_t *instance;
  landfall(player, card, event);
  if (event == EVENT_ETB)
  {
    leg = create_targetted_legacy_effect_no_repeat(
        player, card, effect_defender_can_attack_until_eot, player, card);
    if (leg != -1)
    {
      instance = get_card_instance(player, leg);
      instance->token_status = instance->token_status | 0x10000;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x230e960; original cards/zendikar_rising.c:3269. */
int card_song_mad_treachery(int player, int card, event_t event)
{
  /* Song-Mad Treachery	|3|R|R
   * Sorcery
   * Gain control of target creature until end of turn. Untap that creature. It gains haste until end of turn. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = 2;
  instance = get_card_instance(player, card);
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
    if (spell_fizzled != 1)
    {
      ai_modifier_for_act_of_treason(player, card);
    }
    return 0;
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
  result = mdfc_front_impl(player, card, event, result, 0);
  return result;
}

/* ManalinkEh 2022: 0x230eba0; original cards/zendikar_rising.c:3306. */
int card_soul_shatter(int player, int card, event_t event)
{
  /* Soul Shatter	|2|B
   * Instant
   * Each opponent sacrifices a creature or planeswalker with the greatest mana value among creatures and planeswalkers they control. */

  int scan_card;
  card_instance_t *instance;
  card_instance_t *candidate;
  int type_or_cmc;
  int planeswalker;
  int32_t player_00;
  int32_t card_00;
  int opponent;
  int highest_ties;
  int highest_cmc;
  target_definition_t td;
  char msg[100];

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

      opponent = 1 - player;
      instance = get_card_instance(player, card);
      scan_card = active_cards_count[opponent] + -1;
      instance->targets[0].card = -1;
      instance->targets[0].player = -1;
      if (scan_card < 0)
      {
        player_00 = -1;
        card_00 = -1;
      }
      else
      {
        highest_ties = 1;
        highest_cmc = -1;
        for (; scan_card >= 0; --scan_card)
        {
          candidate = in_play(opponent, scan_card);
          if (!candidate)
            continue;
          type_or_cmc = is_what(opponent, scan_card, 2);
          planeswalker = is_planeswalker(opponent, scan_card);
          if (type_or_cmc == 0 && planeswalker == 0)
            continue;
          type_or_cmc = get_cmc(opponent, scan_card);
          if (type_or_cmc <= highest_cmc)
          {
            type_or_cmc = get_cmc(opponent, scan_card);
            highest_ties += type_or_cmc == highest_cmc;
          }
          else
          {
            instance->targets[0].card = scan_card;
            instance->targets[0].player = opponent;
            highest_cmc = get_cmc(opponent, scan_card);
            highest_ties = 1;
          }
        }
        if (1 < highest_ties)
        {
          default_target_definition(player, card, &td, 0x102);
          td.illegal_abilities = 0;
          td.allow_cancel = 0;
          td.extra = highest_cmc;
          td.special = TARGET_SPECIAL_CMC_GREATER_OR_EQUAL;
          td.who_chooses = opponent;
          td.allowed_controller = opponent;
          td.preferred_controller = opponent;
          scnprintf(msg, 100, "Select a creature you control with CMC %d.", highest_cmc);
          instance->number_of_targets = '\0';
          new_pick_target(&td, msg, 0, 0x80);
        }
        card_00 = instance->targets[0].card;
        player_00 = instance->targets[0].player;
      }
      kill_card(player_00, card_00, KILL_SACRIFICE);
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x230ee40; original cards/zendikar_rising.c:3363. */
int card_spikefield_hazard(int player, int card, event_t event)
{
  /* Spikefield Hazard	|R
   * Instant
   * ~ deals 1 damage to any target. If a permanent dealt damage this way would die this turn, exile it instead. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    result = generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_ANY", 1, NULL);
    return result;
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    result = valid_target(&td);
    if (result != 0)
    {

      result = damage_target0(player, card, 1);
      instance = get_card_instance(player, result);
      instance->targets[3].card = instance->targets[3].card | 0x80;
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_ANY", 1, NULL);
  result = mdfc_front_impl(player, card, event, result, 0);
  return result;
}

/* ManalinkEh 2022: 0x230f070; original cards/zendikar_rising.c:3459. */
int card_tajuru_paragon(int player, int card, event_t event)
{
  /* Tajuru Paragon	|1|G
   * Creature - Elf 3/2
   * ~ is also a Cleric, Rogue, Warrior, and Wizard.
   * Kicker |3
   * When this creature enters, if it was kicked, reveal the top six cards of your library. You may put a card that shares a creature type with it from among them into your hand. Put the rest on the bottom of your library in a random order. */

  kicker(player, card, event, MANACOST_X(3));
  etb_ability(player, card, event, kicked(player, card) ? RESOLVE_TRIGGER_MANDATORY : 0);
  if (event != EVENT_ETB)
    return 0;

  reported_subtypes_t my_stypes;
  memset(&my_stypes, 0, sizeof(my_stypes));
  get_subtypes(player, card, &my_stypes);
  test_definition_t test;
  default_test_definition(&test, TYPE_ANY);
  test.subtype = my_stypes.reported_subtypes[0];
  test.sub2 = my_stypes.reported_subtypes[1];
  test.sub3 = my_stypes.reported_subtypes[2];
  test.sub4 = my_stypes.reported_subtypes[3];
  test.subtype_flag = F2_MULTISUBTYPE;
  test.sub5 = my_stypes.reported_subtypes[4];
  reveal_top_cards_of_library_and_choose(player, card, player, 6, 0, TUTOR_HAND, 1,
                                        TUTOR_BOTTOM_OF_DECK_RANDOM, 1, &test);
  return 0;
}

/* ManalinkEh 2022: 0x230f1d0; original cards/zendikar_rising.c:3489. */
int card_tangled_florahedron(int player, int card, event_t event)
{
  /* Tangled Florahedron	|1|G
   * Creature - Elemental 1/1
   * |T: Add |G. */

  if ((event != EVENT_PLAY_FIRST_HALF &&
       (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE)) &&
      (event != EVENT_CAN_CAST))
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card != affected_card) || (player != affected_card_controller))
        return mana_producing_creature(player, card, event, 0x18, COLOR_GREEN, 1);
    }
    else if ((event != EVENT_RESOLVE_SPELL) &&
             ((event != EVENT_CAN_CHANGE_TARGET && (event != EVENT_CHANGE_TARGET))))
    {
      return mana_producing_creature(player, card, event, 0x18, COLOR_GREEN, 1);
    }
  }
  return mdfc_front_impl(player, card, event, 1, 0);
}

/* ManalinkEh 2022: 0x230f290; original cards/zendikar_rising.c:3539. */
int card_thieving_skydiver(int player, int card, event_t event)
{
  /* Thieving Skydiver	|1|U
   * Creature - Merfolk Rogue 2/1
   * Kicker |X. X can't be 0. (You may pay an additional |X as you cast this spell.)
   * Flying
   * When this creature enters, if it was kicked, gain control of target artifact with mana value X or less. If that artifact is an Equipment, attach it to this creature. */

  int stolen_card;
  card_instance_t *instance;
  target_definition_t td;
  char buffer[100];

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  kicker(player, card, event, -1, 0, 0, 0, 0, 0);
  if (event == EVENT_ETB)
  {
    if (kicked(player, card) != 0)
    {
      instance = get_card_instance(player, card);
      if ((instance != NULL) && (0 < instance->info_slot))
      {
        default_target_definition(player, card, &td, 0x40);
        td.extra = instance->info_slot;
        td.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
        if (can_target(&td) != 0)
        {
          scnprintf(buffer, 100, " Select an artifact with mana value %d or less.",
                    instance->info_slot);
          if (new_pick_target(&td, buffer, 0, 0x81) != 0)
          {
            stolen_card = gain_control_permanently(player, instance->targets[0].player,
                                                   instance->targets[0].card);
            if (is_equipment(player, stolen_card) != 0)
            {
              equip_target_creature(player, stolen_card, player, card);
            }
          }
        }
      }
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2306bc0; original cards/zendikar_rising.c:3608. */
static int three_counters_for_mv_less_than_three(int player, int card)
{
  int result;

  result = get_cmc(player, card);
  if (3 < result)
  {
    return 0;
  }
  hack_silent_counters = hack_silent_counters + 1;
  add_counters(player, card, player, card, COUNTER_P1_P1, 3);
  hack_silent_counters = hack_silent_counters + -1;
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x23066a0; original cards/zendikar_rising.c:3646. */
static int is_instant_sorcery_or_wizard(int iid, int unused, int player, int card)
{
  int result;
  unsigned int matches;

  result = is_what(player, card, 0x38);
  matches = 1;
  if (result == 0)
  {
    result = has_subtype(player, card, SUBTYPE_WIZARD);
    matches = (unsigned int)(result != 0);
  }
  return matches;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x230f550; original cards/zendikar_rising.c:3654. */
int card_umara_wizard(int player, int card, event_t event)
{
  /* Umara Wizard	|4|U
   * Creature - Merfolk Wizard 4/3
   * Whenever you cast an instant, sorcery, or Wizard spell, this creature gains flying until end of turn. */

  test_definition_t test;

  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 0xff);
    test.special_selection_function = is_instant_sorcery_or_wizard;
    if (new_specific_spell_played(player, card, event, player, 2, &test))
    {
      pump_ability_until_eot_no_repeat(player, card, player, card, 0x20, 0);
    }
  }
  return mdfc_front_impl(player, card, event, 1, 0);
}

/* ManalinkEh 2022: 0x230f940; original cards/zendikar_rising.c:3757. */
int card_vastwood_fortification(int player, int card, event_t event)
{
  /* Vastwood Fortification	|G
   * Instant
   * Put a +1/+1 counter on target creature. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    result = generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
    return result;
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {

      instance = get_card_instance(player, card);
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                   COUNTER_P1_P1, 1);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_ANY", 1, NULL);
  result = mdfc_front_impl(player, card, event, result, 0);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x230fbc0; original cards/zendikar_rising.c:3839. */
int card_yasharn_implacable_earth(int player, int card, event_t event)
{
  /* Yasharn, Implacable Earth	|2|G|W
   * Legendary Creature - Elemental Boar 4/4
   * When Yasharn enters, search your library for a basic Forest card and a basic Plains card, reveal those cards, put them into your hand, then shuffle.
   * Players can't pay life or sacrifice nonland permanents to cast spells or activate abilities. */

  const char *land_prompt;
  test_definition_t test;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  land_prompt = get_hacked_land_text(player, card, "Select a basic %s.", 0x5001);
  new_default_test_definition(&test, 1, land_prompt);
  test.subtype = get_hacked_subtype(player, card, 0x5001);
  test.sub2 = 0x2000;
  test.subtype_flag = 4;
  test.no_shuffle = 1;
  new_global_tutor(player, player, 1, 1, 0, 1, &test);
  land_prompt = get_hacked_land_text(player, card, "Select a basic %s.", 0x5007);
  new_default_test_definition(&test, 1, land_prompt);
  test.subtype = get_hacked_subtype(player, card, 0x5007);
  test.sub2 = 0x2000;
  test.subtype_flag = 4;
  new_global_tutor(player, player, 1, 1, 0, 1, &test);
  return 0;
}

/* ManalinkEh 2022: 0x230fe80; original cards/zendikar_rising.c:3889. */
int card_zareth_san_the_trickster(int player, int card, event_t event)
{
  /* Zareth San, the Trickster	|3|U|B
   * Legendary Creature - Merfolk Rogue 4/4
   * Flash
   * |2|U|B, Return an unblocked attacking Rogue you control to its owner's hand: Put this card from your hand onto the battlefield tapped and attacking.
   * Whenever Zareth San deals combat damage to a player, you may put target permanent card from that player's graveyard onto the battlefield under your control. */

  target_definition_t td;
  unsigned int packed_damage;
  int damaged_player;
  card_instance_t *instance;
  int grave_target;
  unsigned int remaining_hits;
  unsigned int *damage_count_cursor;
  int times_damaged[2];

  check_legend_rule(player, card, event);
  if (1 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    ninjutsu(player, card, event, 2, 1, 1, 0, 0, 0, 0x5628);
    if (damage_dealt_by_me(player, card, event,
                           DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE |
                               DDBM_TRIGGER_OPTIONAL) != 0)
    {
      test_definition_t test;

      damage_count_cursor = (unsigned int *)times_damaged;
      default_test_definition(&test, 0x47);
      damaged_player = 0;
      instance = get_card_instance(player, card);
      packed_damage = instance->targets[1].player;
      instance->targets[1].player = 0;
      remaining_hits = packed_damage & 0xff;
      times_damaged[1] = packed_damage >> 8 & 0xff;
      times_damaged[0] = remaining_hits;
      while (true)
      {
        while (remaining_hits != 0)
        {
          grave_target = any_in_graveyard_by_type(damaged_player, 0x47);
          if ((grave_target == 0) || (grave_target = new_select_target_from_grave(
                                          player, card, damaged_player, SFG_0, 2, &test, 0),
                                      grave_target < 0))
          {
            remaining_hits = remaining_hits - 1;
            *damage_count_cursor = remaining_hits;
          }
          else
          {
            reanimate_permanent(player, card, damaged_player, instance->targets[0].player,
                                REANIMATE_DEFAULT);
            remaining_hits = remaining_hits - 1;
            *damage_count_cursor = remaining_hits;
          }
        }
        damaged_player = damaged_player + 1;
        damage_count_cursor = damage_count_cursor + 1;
        if (damaged_player == 2)
          break;
        remaining_hits = *damage_count_cursor;
      }
    }
    return flash(player, card, event);
  }
  default_target_definition(player, card, &td, 2);
  td.required_subtype = 0x9f;
  return ninjutsu_custom_target(player, card, event, 2, 1, 1, 0, 0, 0, &td, 0x5628);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x23100f0; original cards/zendikar_rising.c:3925. */
int card_zof_consumption(int player, int card, event_t event)
{
  /* Zof Consumption	|4|B|B
   * Sorcery
   * Each opponent loses 4 life and you gain 4 life. */

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    lose_life(1 - player, 4);
    gain_life(player, 4);
    kill_card(player, card, KILL_DESTROY);
  }
  return mdfc_front_impl(player, card, event, 1, 0);
}

/* ManalinkEh 2022 private Party bit-count cache @ 0x2774020; initialized by party_size. */
static int number_bits_set[16];

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int party_size(int player);

/* ManalinkEh 2022: 0x2306c30; original cards/zendikar_rising.c:107. */
int party_size(int player)
{
  int candidates_by_class[16] = {0};
  int candidates = 0;
  for (int c = 0; c < active_cards_count[player]; ++c)
  {
    card_instance_t *instance = in_play(player, c);
    if (!instance)
      continue;
    instance = get_card_instance(player, c);
    if (instance->internal_card_id < 0 || !is_what(player, c, 2))
      continue;
    unsigned int classes = has_subtype(player, c, SUBTYPE_CLERIC) != 0;
    if (has_subtype(player, c, SUBTYPE_ROGUE))
      classes |= 4;
    if (has_subtype(player, c, SUBTYPE_WARRIOR))
      classes |= 8;
    if (has_subtype(player, c, SUBTYPE_WIZARD))
      classes |= 2;
    if (classes)
    {
      ++candidates_by_class[classes];
      ++candidates;
    }
  }
  for (unsigned int classes = 0; classes != 16; ++classes)
    number_bits_set[classes] = num_bits_set(classes);
  for (int classes = 0; classes != 16; ++classes)
    if (number_bits_set[classes] < candidates_by_class[classes])
    {
      candidates -= candidates_by_class[classes] - number_bits_set[classes];
      candidates_by_class[classes] = number_bits_set[classes];
    }
  for (unsigned int first = 1; first != 8; first *= 2)
    for (unsigned int second = first * 2; second <= 8; second *= 2)
    {
      int first_count = candidates_by_class[first];
      int combined_count = candidates_by_class[first | second];
      if (first_count != 0 && candidates_by_class[second] != 0)
      {
        if (combined_count != 0)
        {
          candidates -= combined_count;
          candidates_by_class[first | second] = 0;
        }
      }
      else if (combined_count >= 2)
      {
        candidates_by_class[first] = 0;
        candidates = (candidates - first_count) - candidates_by_class[second];
        candidates_by_class[second] = 0;
      }
    }
  unsigned int represented_classes = 0;
  for (unsigned int classes = 0; classes != 16; ++classes)
    if (candidates_by_class[classes] != 0)
      represented_classes |= classes;
  int represented = number_bits_set[represented_classes];
  return candidates < represented ? candidates : represented;
}

/* ManalinkEh 2022: 0x2307900; original cards/zendikar_rising.c:302. */
int card_agadeems_awakening(int player, int card, event_t event)
{
  /* Agadeem's Awakening	|X|B|B|B
   * Sorcery
   * Return from your graveyard to the battlefield any number of target creature cards that each have a different mana value X or less. */

  int i;
  card_instance_t *instance;
  int grave_index_or_iid;
  int grave_count;
  int *chosen_cursor;
  int *next_chosen;
  inst_info_t target_count_info;
  int maximum_targets;
  int chosen[2][19];
  test_definition_t test;

  if (((event == EVENT_MODIFY_COST || event == EVENT_CHANGE_TYPE) ||
       event == EVENT_PLAY_FIRST_HALF) ||
      (event == EVENT_CAN_CAST))
  {
    new_default_test_definition(&test, 0x47, "Select a permanent card.");
    if (event == EVENT_PLAY_FIRST_HALF)
    {

      instance = get_card_instance(player, card);
      maximum_targets = instance->info_slot;
      test.cmc_flag = 3;
      test.cmc = maximum_targets + 1;
      test.no_shuffle = 1;
      test.value_for_special_selection_function = (int)(chosen + 1);
      test.special_selection_function = test_not_in_array;

      if (graveyard_has_shroud(player) == 0)
      {
        i = 0;
        if (0x13 < maximum_targets)
        {
          maximum_targets = 0x13;
        }

        while (((grave_index_or_iid = new_select_a_card(player, player, 5, 0, 5, 0, &test),
                 grave_index_or_iid != -1 &&
                     (grave_count = new_special_count_grave(player, &test), i < maximum_targets)) &&
                (grave_count != 0)))
        {
          instance->targets[i].player = grave_index_or_iid;
          chosen[0][i] = grave_index_or_iid;
          grave_index_or_iid = turn_card_in_grave_face_down(player, grave_index_or_iid);
          instance->targets[i].card = grave_index_or_iid;
          chosen[1][i] = grave_index_or_iid;
          i = i + 1;
        }
        if (0 < i)
        {
          chosen_cursor = chosen[0];
          do
          {
            next_chosen = chosen_cursor + 1;
            turn_card_in_grave_face_up(player, *chosen_cursor, chosen_cursor[0x13]);
            chosen_cursor = next_chosen;
          } while (next_chosen != chosen[0] + i);
        }
        descending_qsort_on_player(i, instance->targets);
        set_inst_info(instance, 0x5501, 0, (uint16_t)i);
      }
    }
  }
  else
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
    else if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
             (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
    new_default_test_definition(&test, 0x47, "Select a permanent card.");
  }
  if (event == EVENT_RESOLVE_SPELL)
  {

    instance = get_card_instance(player, card);
    for (i = 0; target_count_info = inst_info(instance, 0x5501),
        i < (int)(((uint32_t)target_count_info.id | ((uint32_t)target_count_info.data2 << 16)) >>
                  0x10);
         i = i + 1)
    {

      grave_index_or_iid = validate_target_from_grave(player, card, player, i);
      if (grave_index_or_iid != -1)
      {
        reanimate_permanent(player, card, player, grave_index_or_iid, REANIMATE_DEFAULT);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return mdfc_front_impl(player, card, event,
                         generic_spell(player, card, EVENT_CAN_CAST, 0xc, NULL, NULL, 1, &test), 0);
}

/* ManalinkEh 2022: 0x2307cd0; original cards/zendikar_rising.c:393. */
int card_ancient_greenwarden(int player, int card, event_t event)
{
  /* Ancient Greenwarden	|4|G|G
   * Creature - Elemental 5/7
   * Reach (This creature can block creatures with flying.)
   * You may play lands from your graveyard.
   * If a land entering causes a triggered ability of a permanent you control to trigger, that ability triggers an additional time. */

  int result;

  if (event == EVENT_STATIC_EFFECTS)
  {
    card_counts[player][CCOUNTS_ANCIENT_GREENWARDEN] = card_counts[player][CCOUNTS_ANCIENT_GREENWARDEN] + '\x01';
  }
  result = card_crucible_of_worlds(player, card, event);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2308fb0; original cards/zendikar_rising.c:1042. */
int mdfc_back_boltland(int player, int card, event_t event)
{
  int result;

  card_generic_boltland(player, card, event);
  result = mdfc_back(player, card, event);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2309d50; original cards/zendikar_rising.c:1381. */
int card_inscription_of_insight(int player, int card, event_t event)
{
  /* Inscription of Insight	|3|U
   * Sorcery
   * Kicker |2|U|U
   * Choose one. If this spell was kicked, choose any number instead.
   * * Return up to two target creatures to their owners' hands.
   * * Scry 2, then draw two cards.
   * * Target player creates an X/X blue Illusion creature token, where X is the number of cards in their hand. */

  int choice;
  int kicked_or_valid;
  int target_available;
  unsigned int bounce_available;
  card_instance_t *instance;
  unsigned int token_available_or_modes;
  inst_info_t mode_info;
  unsigned int selected_modes;
  token_generation_t token;
  target_definition_t td_creature;
  target_definition_t td_player;

  kicker(player, card, event, 2, 0, 2, 0, 0, 0);
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
      default_target_definition(player, card, &td_creature, 2);
      default_target_definition(player, card, &td_player, 0);
      td_player.zone = TARGET_ZONE_PLAYERS;
      if ((card == affected_card) && (player == affected_card_controller))
      {
        selected_modes = 0;
        do
        {
          kicked_or_valid = kicked(player, card);
          target_available = can_target(&td_player);
          token_available_or_modes = 0;
          if (target_available != 0)
          {
            token_available_or_modes = ((unsigned char)selected_modes >> 3 ^ 1) & 1;
          }
          if (((ai_is_speculating == 1) ||
               ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
              (target_available = can_target(&td_creature), target_available == 0))
          {
            bounce_available = 0;
          }
          else
          {
            bounce_available = (selected_modes >> 1 ^ 1) & 1;
          }
          choice = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_OMIT_ILLEGAL,
                               "Bounce creatures", bounce_available, 1, "Scry 2 & draw",
                               ((unsigned char)selected_modes >> 2 ^ 1) & 1, 1, "Illusion token",
                               token_available_or_modes, 1, "Done", kicked_or_valid, 0xfffffffb, 0,
                               0, 0, 0);
          if (2 < choice - 1U)
          {
            if (kicked(player, card) != 0)
            {
              if (choice == 0)
              {
                spell_fizzled = 1;
                return 0;
              }
              if (choice >= 4)
                break;
            }
            else
            {
              if (choice == 0)
              {
                spell_fizzled = 1;
                return 0;
              }
              break;
            }
          }
          else
          {

            selected_modes =
                (unsigned int)(unsigned char)((unsigned char)selected_modes |
                                              (unsigned char)(1 << ((unsigned char)choice & 0x1f)));
            if (kicked(player, card) == 0)
              break;
          }

          token_available_or_modes = num_bits_set((int)(char)(unsigned char)selected_modes);
        } while (token_available_or_modes < 3);
        if ((selected_modes & 2) != 0)
        {

          instance = get_card_instance(player, card);
          if (new_pick_target(&td_creature, "Select a creature to bounce.", 0, 0x80) != 0)
          {
            state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
            if (can_target(&td_creature) != 0)
            {
              new_pick_target(&td_creature, "Select a creature to bounce.", 1, 0x80);
            }
            state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
          }
        }
        if ((selected_modes & 8) != 0)
        {
          new_pick_target(&td_player, "TARGET_PLAYER", 3, 1);
        }
        instance = get_card_instance(player, card);
        set_inst_info(instance, 0x5565, 0, (short)(char)(unsigned char)selected_modes);
      }
      return basic_spell(player, card, event);
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td_creature, 2);
      default_target_definition(player, card, &td_player, 0);
      td_player.zone = TARGET_ZONE_PLAYERS;
      if (event == EVENT_RESOLVE_SPELL)
      {

        instance = get_card_instance(player, card);
        mode_info = inst_info(instance, 0x5565);
        token_available_or_modes =
            ((uint32_t)mode_info.id | ((uint32_t)mode_info.data2 << 16)) >> 0x10;
        if (((((uint64_t)mode_info.id | ((uint64_t)mode_info.data2 << 16) |
               ((uint64_t)mode_info.data << 32)) &
              0x20000) != 0) &&
            (kicked_or_valid = validate_arbitrary_target(&td_creature, instance->targets[0].player,
                                                         instance->targets[0].card),
             kicked_or_valid != 0))
        {
          bounce_permanent(instance->targets[0].player, instance->targets[0].card);
          kicked_or_valid = validate_arbitrary_target(&td_creature, instance->targets[1].player,
                                                      instance->targets[1].card);
          if (kicked_or_valid != 0)
          {
            bounce_permanent(instance->targets[1].player, instance->targets[1].card);
          }
        }
        if ((token_available_or_modes & 4) != 0)
        {
          scry(player, 2);
          draw_cards(player, 2);
        }
        if (((token_available_or_modes & 8) != 0) &&
            (kicked_or_valid = validate_arbitrary_target(&td_player, instance->targets[3].player,
                                                         instance->targets[3].card),
             kicked_or_valid != 0))
        {
          default_token_definition(player, card, CARD_ID_ILLUSION_TOKEN, &token);
          token.t_player = instance->targets[3].player;
          token.pow = hand_count[token.t_player];
          token.special_infos = 0x42;
          token.tou = token.pow;
          generate_token(&token);
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
  default_target_definition(player, card, &td_creature, 2);
  default_target_definition(player, card, &td_player, 0);
  td_player.zone = TARGET_ZONE_PLAYERS;
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x230a390; original cards/zendikar_rising.c:1457. */
int card_inscription_of_ruin(int player, int card, event_t event)
{
  /* Inscription of Ruin	|2|B
   * Sorcery
   * Kicker |2|B|B
   * Choose one. If this spell was kicked, choose any number instead.
   * * Target opponent discards two cards.
   * * Return target creature card with mana value 2 or less from your graveyard to the battlefield.
   * * Destroy target creature with mana value 3 or less. */

  int choice;
  int32_t saved_target_card;
  int kicked_or_result;
  int target_available;
  unsigned int discard_available;
  card_instance_t *instance;
  unsigned char selected_modes;
  unsigned int destroy_available_or_modes;
  unsigned int reanimate_available;
  inst_info_t mode_info;
  target_definition_t td_creature;
  target_definition_t td_player;
  test_definition_t test_grave;

  kicker(player, card, event, 2, 2, 0, 0, 0, 0);
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td_creature, 2);
    td_creature.extra = 3;
    td_creature.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
    default_target_definition(player, card, &td_player, 0);
    td_player.zone = TARGET_ZONE_PLAYERS;
    new_default_test_definition(&test_grave, 2, "Select a creature card.");
    test_grave.cmc = 3;
    test_grave.cmc_flag = 3;
    kicked_or_result = opponent_is_valid_target(player, card);
    if (((kicked_or_result == 0) &&
         (kicked_or_result = new_special_count_grave(player, &test_grave),
          kicked_or_result == 0)) &&
        (kicked_or_result = can_target(&td_creature), kicked_or_result == 0))
    {
      return 0;
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card)
    {
      return 0;
    }
    if (player != affected_card_controller)
    {
      return 0;
    }
    default_target_definition(player, card, &td_creature, 2);
    td_creature.extra = 3;
    td_creature.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
    default_target_definition(player, card, &td_player, 0);
    td_player.zone = TARGET_ZONE_PLAYERS;
    new_default_test_definition(&test_grave, 2, "Select a creature card.");
    test_grave.cmc = 3;
    test_grave.cmc_flag = 3;
    if ((card == affected_card) && (player == affected_card_controller))
    {
      selected_modes = 0;
      do
      {
        kicked_or_result = kicked(player, card);
        target_available = can_target(&td_creature);
        destroy_available_or_modes = 0;
        if (target_available != 0)
        {
          destroy_available_or_modes = (selected_modes >> 3 ^ 1) & 1;
        }
        target_available = new_special_count_grave(player, &test_grave);
        reanimate_available = 0;
        if (target_available != 0)
        {
          reanimate_available = (selected_modes >> 2 ^ 1) & 1;
        }
        target_available = opponent_is_valid_target(player, card);
        discard_available = 0;
        if (target_available != 0)
        {
          discard_available = (selected_modes >> 1 ^ 1) & 1;
        }
        choice =
            dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_OMIT_ILLEGAL, "Opponent discards",
                        discard_available, 1, "Return creature from grave", reanimate_available, 1,
                        "Kill creature cmc <= 3", destroy_available_or_modes, 1, "Done",
                        kicked_or_result, 0xfffffffb, 0, 0, 0, 0);
        if (2 < choice - 1U)
        {
          if (kicked(player, card) != 0)
          {
            if (choice == 0)
            {
              spell_fizzled = 1;
              return 0;
            }
            if (choice >= 4)
              break;
          }
          else
          {
            if (choice == 0)
            {
              spell_fizzled = 1;
              return 0;
            }
            break;
          }
        }
        else
        {

          selected_modes = selected_modes | (unsigned char)(1 << ((unsigned char)choice & 0x1f));
          if (kicked(player, card) == 0)
            break;
        }

        destroy_available_or_modes = num_bits_set((int)(char)selected_modes);
      } while (destroy_available_or_modes < 3);
      instance = get_card_instance(player, card);
      if ((selected_modes & 2) != 0)
      {
        instance->number_of_targets = '\0';
        target_opponent(player, card);
        saved_target_card = instance->targets[0].card;
        instance->targets[1].player = instance->targets[0].player;
        instance->targets[1].card = saved_target_card;
      }
      if ((selected_modes & 4) != 0)
      {
        instance->number_of_targets = '\0';
        generic_spell(player, card, EVENT_CAST_SPELL, 8, NULL, NULL, 1, &test_grave);
        if (spell_fizzled != 1)
        {

          ai_modifier =
              ai_modifier +
              cards_ptr[graveyard_ptr_mutable[player][instance->targets[0].player]]->ai_base_value;
        }
      }
      if ((selected_modes & 8) != 0)
      {
        new_pick_target(&td_creature, "Select a creature with mana value 3 or less.", 3, 0x81);
      }
      set_inst_info(instance, 0x5566, 0, (short)(char)selected_modes);
    }
  }
  else
  {
    if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
        (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
    default_target_definition(player, card, &td_creature, 2);
    td_creature.extra = 3;
    td_creature.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
    default_target_definition(player, card, &td_player, 0);
    td_player.zone = TARGET_ZONE_PLAYERS;
    new_default_test_definition(&test_grave, 2, "Select a creature card.");
    test_grave.cmc = 3;
    test_grave.cmc_flag = 3;
    if (event == EVENT_RESOLVE_SPELL)
    {

      instance = get_card_instance(player, card);
      mode_info = inst_info(instance, 0x5566);
      destroy_available_or_modes =
          ((uint32_t)mode_info.id | ((uint32_t)mode_info.data2 << 16)) >> 0x10;
      if (((((uint64_t)mode_info.id | ((uint64_t)mode_info.data2 << 16) |
             ((uint64_t)mode_info.data << 32)) &
            0x20000) != 0) &&
          (kicked_or_result = validate_arbitrary_target(&td_player, instance->targets[1].player,
                                                        instance->targets[1].card),
           kicked_or_result != 0))
      {
        discard(instance->targets[1].player, 2, 2);
      }

      if (((destroy_available_or_modes & 4) != 0) &&
          (kicked_or_result = validate_target_from_grave_source(player, card, player, 0),
           kicked_or_result != -1))
      {
        reanimate_permanent(player, card, player, kicked_or_result, REANIMATE_DEFAULT);
      }
      if (((destroy_available_or_modes & 8) != 0) &&
          (kicked_or_result = validate_arbitrary_target(&td_creature, instance->targets[3].player,
                                                        instance->targets[3].card),
           kicked_or_result != 0))
      {
        kill_card(instance->targets[3].player, instance->targets[3].card, KILL_DESTROY);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  return basic_spell(player, card, event);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x230b190; original cards/zendikar_rising.c:1750. */
int card_kazuuls_fury(int player, int card, event_t event)
{
  /* Kazuul's Fury	|2|R
   * Instant
   * As an additional cost to cast this spell, sacrifice a creature.
   * ~ deals damage equal to the sacrificed creature's power to any target. */

  card_instance_t *instance;
  int target_selected;
  unsigned int sacrifice_or_player;
  uint32_t data;
  unsigned int card_00;
  inst_info_t sacrifice_info;
  target_definition_t td1;
  test_definition_t test;

  if (((event == EVENT_MODIFY_COST || event == EVENT_CHANGE_TYPE) ||
       event == EVENT_PLAY_FIRST_HALF) ||
      (event == EVENT_CAN_CAST))
  {
    default_target_definition(player, card, &td1, 0x102);
    td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    instance = get_card_instance(player, card);
    if (event == EVENT_PLAY_FIRST_HALF)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {

        instance->number_of_targets = '\0';
        new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
        sacrifice_or_player = new_sacrifice(player, card, player,
                                            SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
        if (sacrifice_or_player == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        target_selected = pick_target(&td1, "TARGET_ANY");
        card_00 = sacrifice_or_player >> 0x18;
        sacrifice_or_player = (int)sacrifice_or_player >> 0x10 & 0xff;
        if (target_selected == 0)
        {
          state_untargettable(sacrifice_or_player, card_00, 1);
        }
        else
        {
          data = get_power(sacrifice_or_player, card_00);
          set_inst_info(instance, 0x5577, data, 0);
          kill_card(sacrifice_or_player, card_00, KILL_SACRIFICE);
        }
      }
    }
  }
  else
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
    else if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
             (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
    default_target_definition(player, card, &td1, 0x102);
    td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td1) != 0)
    {
      sacrifice_info = inst_info(instance, 0x5577);
      damage_target0(player, card, sacrifice_info.data);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return mdfc_front_impl(player, card, event,
                         generic_spell(player, card, EVENT_CAN_CAST, 0x401, &td1, NULL, 1, NULL),
                         0);
}

/* ManalinkEh 2022: 0x230cf60; original cards/zendikar_rising.c:2504. */
int card_omnath_locus_of_creation(int player, int card, event_t event)
{
  /* Omnath, Locus of Creation	|R|G|W|U
   * Legendary Creature - Elemental 4/4
   * When Omnath enters, draw a card.
   * Landfall - Whenever a land you control enters, you gain 4 life if this is the first time this ability has resolved this turn. If it's the second time, add |R|G|W|U. If it's the third time, Omnath deals 4 damage to each opponent and each planeswalker you don't control. */

  uint16_t landfall_count;
  card_instance_t *instance;
  inst_info_t result;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  landfall(player, card, event);
  if (event == EVENT_ETB)
  {
    if ((trigger_cause_controller == player) && (trigger_cause == card))
    {
      draw_a_card(player);
    }
    else
    {

      instance = get_card_instance(player, card);
      increment_data2(instance, 0x55ae);
      result = inst_info(instance, 0x55ae);
      landfall_count = result.data2;
      if (landfall_count == 2)
      {
        produce_mana_multi(player, 0, 0, 1, 1, 1, 1);
      }
      else if (landfall_count == 3)
      {
        new_damage_all(player, card, 1 - player, 4, 0x22, NULL);
      }
      else if (landfall_count == 1)
      {
        gain_life(player, 4);
      }
    }
  }
  else if (event == EVENT_CLEANUP)
  {

    instance = get_card_instance(player, card);
    clear_info(instance, 0x55ae);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x230de40; original cards/zendikar_rising.c:3022. */
int card_shatterskull_smashing(int player, int card, event_t event)
{
  /* Shatterskull Smashing	|X|R|R
   * Sorcery
   * ~ deals X damage divided as you choose among up to two target creatures and/or planeswalkers. If X is 6 or more, ~ deals twice X damage divided as you choose among them instead. */

  const char *second_target_name;
  int target_csvid;
  card_instance_t *instance;
  uint32_t data;
  unsigned int dialog_player;
  uint32_t argument3;
  unsigned int player_pass;
  inst_info_t damage_info;
  uint16_t second_damage;
  target_definition_t td;
  char buffer[250];

  if (((event == EVENT_MODIFY_COST || event == EVENT_CHANGE_TYPE) ||
       event == EVENT_PLAY_FIRST_HALF) ||
      (event == EVENT_CAN_CAST))
  {
    default_target_definition(player, card, &td, 0x102);
    td.allow_cancel = 3;
    if (event == EVENT_PLAY_FIRST_HALF)
    {

      pick_up_to_n_targets(&td, "TARGET_CREATURE_OR_PLANESWALKER", 2);
      instance = get_card_instance(player, card);
      if (instance->number_of_targets == 0)
        spell_fizzled = 1;
      argument3 = instance->info_slot;
      if ((int)argument3 > 5)
        argument3 *= 2;
      second_damage = 0;
      data = argument3;
      if (instance->number_of_targets == 2)
      {
        target_csvid = get_id(instance->targets[0].player, instance->targets[0].card);
        scnprintf(buffer, 0xfa, "Deal how much damage to %s?", cards_ptr[target_csvid]->name);
        data = choose_a_number(player, buffer, argument3);
        target_csvid = get_id(instance->targets[1].player, instance->targets[1].card);
        second_target_name = cards_ptr[target_csvid]->name;
        target_csvid = get_id(instance->targets[0].player, instance->targets[0].card);
        scnprintf(buffer, 0xfa, "%d damage will be dealt to\n %s\n and %d will be dealt to\n %s.",
                  data, cards_ptr[target_csvid]->name, argument3 - data, second_target_name);

        player_pass = 0;
        do
        {
          dialog_player = current_turn ^ player_pass;
          player_pass = player_pass + 1;
          dialog_impl(player, card, EVENT_PLAY_FIRST_HALF, DLG_HEADER(""), DLG_NO_STORAGE, DLG_NO_CANCEL,
                      DLG_WHO_CHOOSES(dialog_player), buffer, 1, 1, 0, 0, 0, 0);
        } while (player_pass != 2);
        second_damage = (uint16_t)(argument3 - data);
      }
      set_inst_info(instance, 0x55dd, data, second_damage);
      return mdfc_front_impl(player, card, event,
                             generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL), 0);
    }
  }
  else
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
    else if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
             (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0x102);
  }
  td.allow_cancel = 3;
  if (event == EVENT_RESOLVE_SPELL)
  {

    td.allow_cancel = 3;
    instance = get_card_instance(player, card);
    if (valid_target(&td) != 0)
    {
      damage_info = inst_info(instance, 0x55dd);
      damage_target0(player, card, damage_info.data);
    }
    if (validate_target(player, card, &td, 1) != 0)
    {
      damage_info = inst_info(instance, 0x55dd);
      damage_creature(instance->targets[1].player, instance->targets[1].card,
                      ((uint32_t)damage_info.id | ((uint32_t)damage_info.data2 << 16)) >> 0x10,
                      player, card);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return mdfc_front_impl(player, card, event,
                         generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL), 0);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x230e400; original cards/zendikar_rising.c:3114. */
int card_skyclave_apparition(int player, int card, event_t event)
{
  /* Skyclave Apparition	|1|W|W
   * Creature - Kor Spirit 2/2
   * When this creature enters, exile up to one target nonland, nontoken permanent you don't control with mana value 4 or less.
   * When this creature leaves the battlefield, the exiled card's owner creates an X/X blue Illusion creature token, where X is the mana value of the exiled card. */

  int trigger_mode;
  uint32_t data;
  card_instance_t *instance;
  inst_info_t exile_info;

  trigger_mode = duh_mode(player);
  etb_ability(player, card, event, (trigger_mode != 0) + RESOLVE_TRIGGER_OPTIONAL);
  leaves_play(player, card, event, 1, 1);
  if (event == EVENT_ETB)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x47);
    td.allowed_controller = 1 - trigger_cause_controller;
    td.illegal_type = TYPE_LAND | TARGET_TYPE_TOKEN;
    td.extra = 4;
    td.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
    if (can_target(&td) != 0)
    {
      if (new_pick_target(
              &td,
              "Select a nonland, nontoken permanent you don\'t control with mana value 4 or less.",
              0, 0x80) != 0)
      {
        instance = get_card_instance(player, card);
        int owner = get_owner(instance->targets[0].player, instance->targets[0].card);
        data = get_cmc(instance->targets[0].player, instance->targets[0].card);
        set_inst_info(instance, 0x55e4, data, (uint16_t)owner);
        kill_target0(player, card, KILL_EXILE);
      }
    }
  }
  else if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    instance = get_card_instance(player, card);
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_ILLUSION_TOKEN, &token);
    exile_info = inst_info(instance, 0x55e4);
    token.pow = exile_info.data;
    token.tou = token.pow;
    exile_info = inst_info(instance, 0x55e4);
    token.special_infos = 0x42;
    token.t_player = exile_info.data2;
    generate_token(&token);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x230fb60; original cards/zendikar_rising.c:3801. */
int card_veteran_adventurer(int player, int card, event_t event)
{
  /* Veteran Adventurer	|5|G
   * Creature - Human 5/5
   * ~ is also a Cleric, Rogue, Warrior, and Wizard.
   * This spell costs |1 less to cast for each creature in your party.
   * Vigilance */

  int result;

  vigilance(player, card, event);
  if (event != EVENT_MODIFY_COST)
  {
    return 0;
  }
  result = party_size(player);
  COST_COLORLESS = COST_COLORLESS - (char)result;
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x230fd40; original cards/zendikar_rising.c:3867. */
int card_zagras_thief_of_heartbeats(int player, int card, event_t event)
{
  /* Zagras, Thief of Heartbeats	|4|B|R
   * Legendary Creature - Vampire Rogue 4/4
   * This spell costs |1 less to cast for each creature in your party.
   * Flying, deathtouch, haste
   * Other creatures you control have deathtouch.
   * Whenever a creature you control deals combat damage to a planeswalker, destroy that planeswalker. */

  card_instance_t *instance;
  int result;

  check_legend_rule(player, card, event);
  deathtouch(player, card, event);
  haste(player, card, event);
  if (event == EVENT_MODIFY_COST)
  {
    result = party_size(player);
    COST_COLORLESS = COST_COLORLESS - (char)result;
  }
  else if (((event == EVENT_ABILITIES) && (affected_card_controller == player)) &&
           (affected_card != card))
  {
    result = is_what(player, affected_card, 2);
    if (result != 0)
    {
      deathtouch(affected_card_controller, affected_card, EVENT_ABILITIES);
    }
  }
  instance = combat_damage_being_dealt(event);
  if (instance != NULL)
  {
    result = damage_is_to_planeswalker(instance);
    if ((result != 0) && (instance->damage_source_player == player))
    {
      result = is_what(player, instance->damage_source_card, 2);
      if (result != 0)
      {
        kill_card(instance->targets[4].player, instance->targets[4].card, KILL_DESTROY);
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x23095c0; original cards/zendikar_rising.c:1306. */
int card_inscription_of_abundance(int player, int card, event_t event)
{
  /* Inscription of Abundance	|1|G
   * Instant
   * Kicker |2|G
   * Choose one. If this spell was kicked, choose any number instead.
   * * Put two +1/+1 counters on target creature.
   * * Target player gains X life, where X is the greatest power among creatures they control.
   * * Target creature you control fights target creature you don't control. */

  int target_available;
  unsigned int counters_available;
  card_instance_t *instance;
  int kicked_or_choice_or_valid;
  unsigned int life_available;
  unsigned int fight_priority_or_modes;
  inst_info_t mode_info;
  unsigned int fight_available;
  unsigned int selected_modes;
  target_definition_t td_creature;
  target_definition_t td_control;
  target_definition_t td_dont_control;
  target_definition_t td_player;
  kicker(player, card, event, 2, 0, 0, 1, 0, 0);
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td_creature, 2);
    td_dont_control = td_creature;
    td_control = td_dont_control;
    td_dont_control.allowed_controller = 1 - player;
    td_player = td_control;
    td_control.allowed_controller = player;
    td_player.zone = TARGET_ZONE_PLAYERS;
    kicked_or_choice_or_valid = can_target(&td_creature);
    if (((kicked_or_choice_or_valid != 0) ||
         (kicked_or_choice_or_valid = can_target(&td_player), kicked_or_choice_or_valid != 0)) ||
        ((kicked_or_choice_or_valid = can_target(&td_control),
          kicked_or_choice_or_valid != 0 &&
              (kicked_or_choice_or_valid = can_target(&td_dont_control),
               kicked_or_choice_or_valid != 0))))
      return basic_spell(player, card, event);
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td_creature, 2);
      td_dont_control = td_creature;
      td_control = td_dont_control;
      td_dont_control.allowed_controller = 1 - player;
      td_player = td_control;
      td_control.allowed_controller = player;
      td_player.zone = TARGET_ZONE_PLAYERS;
      if ((card == affected_card) && (player == affected_card_controller))
      {
        selected_modes = 0;
        do
        {
          fight_priority_or_modes = 1;
          kicked_or_choice_or_valid = kicked(player, card);
          if (kicked(player, card) != 0)
          {
            fight_priority_or_modes = selected_modes & 1;
          }
          target_available = can_target(&td_control);
          if ((target_available == 0) ||
              (target_available = can_target(&td_dont_control), target_available == 0))
          {
            fight_available = 0;
          }
          else
          {
            fight_available = ((unsigned char)selected_modes >> 3 ^ 1) & 1;
          }
          target_available = can_target(&td_player);
          life_available = 0;
          if (target_available != 0)
          {
            life_available = ((unsigned char)selected_modes >> 2 ^ 1) & 1;
          }
          target_available = can_target(&td_creature);
          counters_available = 0;
          if (target_available != 0)
          {
            counters_available = ((unsigned char)selected_modes >> 1 ^ 1) & 1;
          }
          kicked_or_choice_or_valid = dialog_impl(
              player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_OMIT_ILLEGAL, "Two +1/+1 counters",
              counters_available, 1, "Gain life", life_available, 1, "Fight", fight_available,
              fight_priority_or_modes, "Done", kicked_or_choice_or_valid, 0xfffffffb, 0, 0, 0, 0);
          if (2 < kicked_or_choice_or_valid - 1U)
          {
            if (kicked(player, card) != 0)
            {
              if (kicked_or_choice_or_valid == 0)
              {
                spell_fizzled = 1;
                return 0;
              }
              if (kicked_or_choice_or_valid >= 4)
                break;
            }
            else
            {
              if (kicked_or_choice_or_valid == 0)
              {
                spell_fizzled = 1;
                return 0;
              }
              break;
            }
          }
          else
          {

            selected_modes =
                (unsigned int)(unsigned char)((unsigned char)selected_modes |
                                              (unsigned char)(1 << ((unsigned char)
                                                                        kicked_or_choice_or_valid &
                                                                    0x1f)));
            kicked_or_choice_or_valid = kicked(player, card);
            if (kicked_or_choice_or_valid == 0)
              break;
          }

          fight_priority_or_modes = num_bits_set((int)(char)(unsigned char)selected_modes);
        } while (fight_priority_or_modes < 3);
        if ((selected_modes & 2) != 0)
        {
          new_pick_target(&td_creature, "Select a creature to gain counters.", 1, 0x81);
        }
        if ((selected_modes & 4) != 0)
        {
          new_pick_target(&td_player, "TARGET_PLAYER", 2, 1);
        }
        if (((selected_modes & 8) != 0) && (kicked_or_choice_or_valid = new_pick_target(
                                                &td_control, "TARGET_CREATURE_YOU_CONTROL", 3, 1),
                                            kicked_or_choice_or_valid != 0))
        {
          new_pick_target(&td_dont_control, "Select target creature you don\'t control.", 4, 0x81);
        }
        instance = get_card_instance(player, card);
        set_inst_info(instance, 0x5564, 0, (short)(char)(unsigned char)selected_modes);
      }
      return basic_spell(player, card, event);
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td_creature, 2);
    td_dont_control = td_creature;
    td_control = td_dont_control;
    td_dont_control.allowed_controller = 1 - player;
    td_player = td_control;
    td_control.allowed_controller = player;
    td_player.zone = TARGET_ZONE_PLAYERS;
    if (event == EVENT_RESOLVE_SPELL)
    {
      instance = get_card_instance(player, card);
      mode_info = inst_info(instance, 0x5564);
      fight_priority_or_modes =
          ((uint32_t)mode_info.id | ((uint32_t)mode_info.data2 << 16)) >> 0x10;
      if (((((uint64_t)mode_info.id | ((uint64_t)mode_info.data2 << 16) |
             ((uint64_t)mode_info.data << 32)) &
            0x20000) != 0) &&
          (kicked_or_choice_or_valid = validate_arbitrary_target(
               &td_creature, instance->targets[1].player, instance->targets[1].card),
           kicked_or_choice_or_valid != 0))
      {
        add_counters(player, card, instance->targets[1].player, instance->targets[1].card,
                     COUNTER_P1_P1, 2);
      }
      if (((fight_priority_or_modes & 4) != 0) &&
          (kicked_or_choice_or_valid = validate_arbitrary_target(
               &td_player, instance->targets[2].player, instance->targets[2].card),
           kicked_or_choice_or_valid != 0))
      {
        kicked_or_choice_or_valid = get_highest_power(instance->targets[2].player);
        gain_life(instance->targets[2].player, kicked_or_choice_or_valid);
      }
      if ((((fight_priority_or_modes & 8) != 0) &&
           (kicked_or_choice_or_valid = validate_arbitrary_target(
                &td_control, instance->targets[3].player, instance->targets[3].card),
            kicked_or_choice_or_valid != 0)) &&
          (kicked_or_choice_or_valid = validate_arbitrary_target(
               &td_dont_control, instance->targets[4].player, instance->targets[4].card),
           kicked_or_choice_or_valid != 0))
      {
        fight(instance->targets[3].player, instance->targets[3].card, instance->targets[4].player,
              instance->targets[4].card);
      }
      kill_card(player, card, KILL_DESTROY);
    }
    return basic_spell(player, card, event);
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x230e7c0; original cards/zendikar_rising.c:3214. */
int card_skyclave_shade(int player, int card, event_t event)
{
  /* Skyclave Shade	|1|B
   * Creature - Shade 3/1
   * Kicker |2|B
   * This creature can't block.
   * If this creature was kicked, it enters with two +1/+1 counters on it.
   * Landfall - Whenever a land you control enters, if this card is in your graveyard and it's your turn, you may cast it from your graveyard this turn. */

  int was_kicked;
  int counters_or_paid;
  card_data_t *cd;
  cannot_block(player, card, event);
  kicker(player, card, event, 2, 1, 0, 0, 0, 0);
  was_kicked = kicked(player, card);
  counters_or_paid = 0;
  if (was_kicked != 0)
  {
    counters_or_paid = 2;
  }
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, counters_or_paid);
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    counters_or_paid = has_mana_to_cast_id(player, EVENT_GRAVEYARD_ABILITY, get_id(player, card));
    if (((counters_or_paid != 0) && (current_turn == player)) && (0 < lands_played))
    {
      return 2;
    }
  }
  else if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    cd = get_card_data(player, card);
    counters_or_paid = charge_spell_cost(cd, player, card);
    return (unsigned int)(counters_or_paid != 0);
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x230f420; original cards/zendikar_rising.c:3616. */
int card_turntimber_symbiosis(int player, int card, event_t event)
{
  /* Turntimber Symbiosis	|4|G|G|G
   * Sorcery
   * Look at the top seven cards of your library. You may put a creature card from among them onto the battlefield. If that card has mana value 3 or less, it enters with three additional +1/+1 counters on it. Put the rest on the bottom of your library in a random order. */

  test_definition_t test;

  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&test, 2);
    reveal_top_cards_of_library_and_choose_fn(player, card, player, 7, 0, TUTOR_PLAY, 1,
                                              TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test,
                                              three_counters_for_mv_less_than_three);
    kill_card(player, card, KILL_DESTROY);
  }
  return mdfc_front_impl(player, card, event, 1, 0);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x230f630; original cards/zendikar_rising.c:3682. */
int card_valakut_awakening(int player, int card, event_t event)
{
  /* Valakut Awakening	|2|R
   * Instant
   * Put any number of cards from your hand on the bottom of your library, then draw that many cards plus one. */

  int selected_card;
  int bottomed_cards;
  char msg[100];
  test_definition_t this_test;
  if (event != EVENT_MODIFY_COST && event != EVENT_CHANGE_TYPE && event != EVENT_PLAY_FIRST_HALF &&
      event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    memset(msg, 0, sizeof msg);
    memcpy(msg, "Select a card to put on the bottom of your library.", 52);
    bottomed_cards = 0;
    new_default_test_definition(&this_test, 0xff, msg);
    if (0 < hand_count[player])
    {
      do
      {
        selected_card = new_select_a_card(player, player, 4, 0, 3, -1, &this_test);
        if (selected_card == -1)
          break;
        put_on_bottom_of_deck(player, selected_card);
        bottomed_cards = bottomed_cards + 1;
      } while (0 < hand_count[player]);
      if (0 < bottomed_cards)
      {
        draw_cards(player, bottomed_cards + 1);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return mdfc_front_impl(player, card, event, 1, 0);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x230f7b0; original cards/zendikar_rising.c:3723. */
int card_valakut_exploration(int player, int card, event_t event)
{
  /* Valakut Exploration	|2|R
   * Enchantment
   * Landfall - Whenever a land you control enters, exile the top card of your library. You may play that card for as long as it remains exiled.
   * At the beginning of your end step, if there are cards exiled with this enchantment, put them into their owner's graveyard, then this enchantment deals that much damage to each opponent. */

  int c;
  int iid;
  int top_iid_or_triggered;
  card_instance_t *instance;
  int effect_or_exile_index;
  int expired_cards;
  landfall(player, card, event);
  if ((event == EVENT_ETB) &&
      (top_iid_or_triggered = *deck_ptr[player], top_iid_or_triggered != -1))
  {
    rfg_top_card_of_deck(player);
    instance = get_card_instance(
        player, create_may_play_card_from_exile_effect(
                    player, card, player, (unsigned int)cards_data[top_iid_or_triggered].id, 0));
    instance->eot_toughness = 0x561c;
  }
  top_iid_or_triggered = eot_trigger(player, card, event);
  if ((top_iid_or_triggered != 0) && (0 < active_cards_count[player]))
  {
    expired_cards = 0;
    c = 0;
    do
    {
      instance = in_play(player, c);
      if ((((instance != NULL) &&
            (effect_or_exile_index = is_what(player, c, 0x80), effect_or_exile_index != 0)) &&
           (instance->info_slot_fn == effect_may_play_card_from_exile)) &&
          (instance->eot_toughness == 0x561c))
      {
        expired_cards = expired_cards + 1;
        iid = get_internal_card_id_from_csv_id(instance->targets[0].card);
        effect_or_exile_index = find_iid_in_rfg(player, iid);
        if (effect_or_exile_index != -1)
        {
          from_exile_to_graveyard(player, effect_or_exile_index);
        }
      }
      c = c + 1;
    } while (c < active_cards_count[player]);
    if (expired_cards != 0)
    {
      damage_player(1 - player, expired_cards, player, card);
    }
  }
  return global_enchantment(player, card, event);
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline-only helpers; native code remains in their callers. */
typedef enum
{
  PARTY_NONE = 0,
  PARTY_CLERIC = 1,
  PARTY_WIZARD = 2,
  PARTY_ROGUE = 4,
  PARTY_WARRIOR = 8,
} PartyBit_t;
_Static_assert(sizeof(PartyBit_t) == 4, "native Party bit width");

static void InitNumberBitsSet(void)
{
  for (unsigned int i = 0; i < 16; ++i)
    number_bits_set[i] = num_bits_set(i);
}

static int count_party_guts(int num_candidates, int *candidates_by_class)
{
  InitNumberBitsSet();
  for (int bits = 0; bits < 16; ++bits)
    if (candidates_by_class[bits] > number_bits_set[bits])
    {
      num_candidates -= candidates_by_class[bits] - number_bits_set[bits];
      candidates_by_class[bits] = number_bits_set[bits];
    }
  for (int first = 1; first <= 4; first *= 2)
    for (int second = first * 2; second <= 8; second *= 2)
      if (candidates_by_class[first] && candidates_by_class[second])
      {
        num_candidates -= candidates_by_class[first | second];
        candidates_by_class[first | second] = 0;
      }
      else if (candidates_by_class[first | second] >= 2)
      {
        num_candidates -= candidates_by_class[first] + candidates_by_class[second];
        candidates_by_class[first] = candidates_by_class[second] = 0;
      }
  int available = 0;
  for (int bits = 0; bits < 16; ++bits)
    if (candidates_by_class[bits])
      available |= bits;
  return MIN(num_candidates, number_bits_set[available]);
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline helpers; existing native callers contain the expansions. */
static PartyBit_t party_bits(int player, int card)
{
  if (get_card_instance(player, card)->internal_card_id < 0 ||
      !is_what(player, card, TYPE_CREATURE))
    return PARTY_NONE;
  unsigned int bits = PARTY_NONE;
  if (has_subtype(player, card, SUBTYPE_CLERIC))
    bits |= PARTY_CLERIC;
  if (has_subtype(player, card, SUBTYPE_ROGUE))
    bits |= PARTY_ROGUE;
  if (has_subtype(player, card, SUBTYPE_WARRIOR))
    bits |= PARTY_WARRIOR;
  if (has_subtype(player, card, SUBTYPE_WIZARD))
    bits |= PARTY_WIZARD;
  return (PartyBit_t)bits;
}

static void count_party_classes(int player, int *num_candidates,
                                int *candidates_by_class)
{
  for (int i = 0; i < 16; ++i)
    candidates_by_class[i] = 0;
  *num_candidates = 0;
  for (int card = 0; card < active_cards_count[player]; ++card)
  {
    if (!in_play(player, card))
      continue;
    PartyBit_t bits = party_bits(player, card);
    if (bits != PARTY_NONE)
    {
      ++candidates_by_class[bits];
      ++*num_candidates;
    }
  }
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline mechanics; native callers retain their expansions. */
static void special_null_casting_cost(int player, int card)
{
  card_ptr_t *cp = cards_ptr[get_id(player, card)];
  int cost = get_updated_casting_cost(player, card, -1, EVENT_MODIFY_COST, -1);
  COST_COLORLESS -= cost;
  int extra = get_id(player, card) == CARD_ID_HAGRA_MAULING && count_basic_lands(1 - player, -1) == 0;
  COST_COLORLESS -= extra;
  COST_BLACK -= cp->req.req_black;
  COST_BLUE -= cp->req.req_blue;
  COST_GREEN -= cp->req.req_green;
  COST_RED -= cp->req.req_red;
  COST_WHITE -= cp->req.req_white;
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline card mechanics; original native callers contain the expansions. */
static int can_legally_play_iid_now_lite(int player, int internal_card_id, event_t event)
{
  if (!(cards_data[internal_card_id].type & (TYPE_INSTANT | TYPE_INTERRUPT)) &&
      !can_sorcery_be_played(player, event))
    return 0;
  return !(cards_data[internal_card_id].type & TYPE_LAND) ||
         (!(land_can_be_played & 1) && player == current_turn);
}
#endif
