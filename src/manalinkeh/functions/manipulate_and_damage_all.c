#include "manalink.h"

/* Canonical recovered helper declarations. */
int new_damage_all(int player, int card, int targ_player, int dmg, int mode,
                        test_definition_t *this_test);
static actions_t cook_action(int player, int card, actions_t raw_action,
                                  counter_t *counter_type, int *counter_num);
int action_on_card(int player, int card, int t_player, int t_card, actions_t raw_action);
int new_manipulate_all(int player, int card, int t_player, test_definition_t *this_test,
                            actions_t raw_action);
int manipulate_all(int player, int card, int t_player, int type, int flag1, int subtype,
                        int flag2, int color, int flag3, int id, int flag4, int cc, int flag5,
                        actions_t action);
static int action_on_card_impl(int player, int card, int p, int c, actions_t action,
                                    counter_t counter_type, int counter_num);

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2666bc0; original functions/manipulate_and_damage_all.c:3. */
static int action_on_card_impl(int player, int card, int p, int c, actions_t action,
                               counter_t counter_type, int counter_num)
{
  int result;
  if (ACT_DETAIN < action)
  {
    if (action == ACT_REMOVE_COUNTERS_BASE)
    {
      result = count_counters(p, c, counter_type);
      if (counter_num < result)
      {
        result = counter_num;
      }
      remove_counters(player, card, p, c, counter_type, result);
      return result;
    }
    if (action != ACT_PARAMETERIZED_ACTIONS_MASK)
    {
      if (action == ACT_ADD_COUNTERS_BASE)
      {
        add_counters_predoubled(player, card, p, c, counter_type, counter_num);
        return counter_num;
      }
      return 0;
    }
    abort();
    return -1;
  }
  switch (action)
  {
  case ACT_KILL_BURY:
  case ACT_KILL_DESTROY:
    result = check_for_special_ability(p, c, 0x800);
    if (result != 0)
    {
      return 0;
    }
  case ACT_KILL_SACRIFICE:
  case ACT_KILL_REMOVE:
  case ACT_KILL_STATE_BASED_ACTION:
    kill_card(p, c, action);
    break;
  case ACT_BOUNCE:
    bounce_permanent(p, c);
    break;
  case ACT_TAP:
    result = is_tapped(p, c);
    if (result != 0)
    {
      return 0;
    }
    tap_card(p, c);
    break;
  case ACT_UNTAP:
    result = is_tapped(p, c);
    if (result == 0)
    {
      return 0;
    }
    untap_card(p, c);
    break;
  default:
    return 0;
  case ACT_RFG_UNTIL_EOT:
    remove_until_eot(player, card, p, c);
    break;
  case ACT_PUT_ON_TOP:
    put_on_top_of_deck(p, c);
    break;
  case ACT_PUT_ON_BOTTOM:
    put_on_bottom_of_deck(p, c);
    break;
  case ACT_DISABLE_NONMANA_ACTIVATED_ABILITIES:
  case ACT_ENABLE_NONMANA_ACTIVATED_ABILITIES:
    disable_nonmana_activated_abilities(p, c, action == ACT_DISABLE_NONMANA_ACTIVATED_ABILITIES);
    break;
  case ACT_HUMILIATE:
  case ACT_DE_HUMILIATE:
    humiliate(player, card, p, c, action == ACT_HUMILIATE ? 2 : 0);
    break;
  case ACT_MAKE_UNTARGETTABLE:
  case ACT_REMOVE_UNTARGETTABLE:
    state_untargettable(p, c, action == ACT_MAKE_UNTARGETTABLE);
    break;
  case ACT_DISABLE_ALL_ACTIVATED_ABILITIES:
  case ACT_ENABLE_ALL_ACTIVATED_ABILITIES:
    disable_all_activated_abilities(p, c, action == ACT_DISABLE_ALL_ACTIVATED_ABILITIES);
    break;
  case ACT_GET_COUNT:
    break;
  case ACT_GAIN_CONTROL:
    gain_control_permanently(player, p, c);
    break;
  case ACT_DOES_NOT_UNTAP:
    does_not_untap_effect(player, card, p, c, 0, 1);
    break;
  case ACT_PHASE_OUT:
    phase_out(p, c);
    break;
  case ACT_OF_TREASON:
    effect_act_of_treason(player, card, p, c);
    break;
  case ACT_DETAIN:
    detain(player, card, p, c);
  }
  return 1;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2666f10; original functions/manipulate_and_damage_all.c:127. */
static actions_t cook_action(int player, int card, actions_t raw_action, counter_t *counter_type,
                             int *counter_num)
{
  short encoded_counter_count;
  int result;
  actions_t parameterized_action;
  parameterized_action = raw_action & ACT_PARAMETERIZED_ACTIONS_MASK;
  encoded_counter_count = (short)raw_action;
  if (parameterized_action == ACT_ADD_COUNTERS_BASE)
  {
    *counter_type = raw_action >> 0x10 & COUNTER_invalid;
    result = (int)encoded_counter_count;
    *counter_num = result;
    if (0 < result)
    {
      result = get_updated_counters_number(player, card, *counter_type, result);
      *counter_num = result;
      if (0 < result)
      {
        return ACT_ADD_COUNTERS_BASE;
      }
      return ~ACT_KILL_BURY;
    }
  }
  else
  {
    if (parameterized_action != ACT_REMOVE_COUNTERS_BASE)
    {
      if (parameterized_action == 0)
      {
        return raw_action;
      }
      backtrace_and_abort(
          "functions/manipulate_and_damage_all.c:155\n!\"Unknown parameterized action\"");
    }
    *counter_type = raw_action >> 0x10 & COUNTER_invalid;
    *counter_num = (int)encoded_counter_count;
    if (0 < encoded_counter_count)
    {
      return ACT_REMOVE_COUNTERS_BASE;
    }
  }
  return 0xffffffff;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2666fe0; original functions/manipulate_and_damage_all.c:160. */
int action_on_card(int player, int card, int t_player, int t_card, actions_t raw_action)
{
  actions_t action;
  int result;
  counter_t counter_type;
  int counter_num;

  counter_type = COUNTER_invalid;
  counter_num = 0;
  action = cook_action(t_player, t_card, raw_action, &counter_type, &counter_num);
  result = 0;
  if ((uint32_t)action < (uint32_t)~ACT_KILL_BURY)
  {
    result =
        action_on_card_impl(player, card, t_player, t_card, action, counter_type, counter_num);
  }
  return result;
}
#endif

int action_on_target(int player, int card, unsigned int target_number, actions_t raw_action)
{
  card_instance_t* instance = get_card_instance(player, card);
  ASSERT((instance->targets[target_number].player & ~1) == 0);	// i.e., it's either 0 or 1
  return action_on_card(player, card, instance->targets[target_number].player, instance->targets[target_number].card, raw_action);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26670c0; original functions/manipulate_and_damage_all.c:178. */
int new_manipulate_all(int player, int card, int t_player, test_definition_t *this_test,
                       actions_t raw_action)
{
  int i;
  unsigned int test_zone;
  int candidate_card_or_turn_player;
  actions_t action;
  card_instance_t *instance;
  int turn_player_or_test_result;
  int *shift_cursor;
  int pass_or_sort_result;
  int candidate_card_or_next_index;
  int scan_player_or_sort_limit;
  int *sort_cursor;
  bool both_players;
  int *marked_row;
  int effects_applied;
  int marked_count;
  int pass_or_sort_player;
  counter_t counter_type;
  int counter_num;
  int mc[2];
  int marked[2][100];

  both_players = t_player == 2;
  mc[0] = 0;
  mc[1] = 0;
  for (pass_or_sort_player = 0; pass_or_sort_player < 2; ++pass_or_sort_player)
  {
    scan_player_or_sort_limit = pass_or_sort_player == 0 ? current_turn : 1 - current_turn;
    if (t_player != scan_player_or_sort_limit && !both_players)
      continue;
    candidate_card_or_next_index = active_cards_count[scan_player_or_sort_limit] - 1;
    for (; candidate_card_or_next_index >= 0; --candidate_card_or_next_index)
    {
      test_zone = this_test->zone;
      bool matches_zone = false;
      if (test_zone == 0x200)
      {
        instance = in_play(scan_player_or_sort_limit, candidate_card_or_next_index);
        if (instance != NULL &&
            (turn_player_or_test_result =
                 is_what(scan_player_or_sort_limit, candidate_card_or_next_index, 0x47),
             turn_player_or_test_result != 0) &&
            (turn_player_or_test_result =
                 check_state(scan_player_or_sort_limit, candidate_card_or_next_index, 0x100000),
             turn_player_or_test_result == 0))
          matches_zone = true;
        else
          test_zone = this_test->zone;
      }
      if (!matches_zone && test_zone == 0x100)
      {
        instance = in_hand(scan_player_or_sort_limit, candidate_card_or_next_index);
        matches_zone = instance != NULL;
      }
      if (matches_zone)
      {
        turn_player_or_test_result = new_make_test_in_play(
            scan_player_or_sort_limit, candidate_card_or_next_index, -1, this_test);
        if (turn_player_or_test_result != 0 &&
            (this_test->not_me == 0 || player != scan_player_or_sort_limit ||
             card != candidate_card_or_next_index))
        {
          turn_player_or_test_result = mc[scan_player_or_sort_limit];
          mc[scan_player_or_sort_limit] = turn_player_or_test_result + 1;
          marked[scan_player_or_sort_limit][turn_player_or_test_result] =
              candidate_card_or_next_index;
          if (raw_action < ACT_BOUNCE || raw_action == ACT_KILL_STATE_BASED_ACTION)
          {
            instance->kill_code = (uint8_t)raw_action;
            if (raw_action == ACT_KILL_STATE_BASED_ACTION)
              set_special_flags(scan_player_or_sort_limit, candidate_card_or_next_index, 0x800);
          }
        }
      }
    }
  }
  if (raw_action == ACT_BOUNCE)
  {
    marked_row = marked[0];
    for (pass_or_sort_player = 0; pass_or_sort_player < 2; ++pass_or_sort_player)
    {
      if (mc[pass_or_sort_player] > 1)
      {
        scan_player_or_sort_limit = mc[pass_or_sort_player] - 1;
        sort_cursor = marked_row;
        for (turn_player_or_test_result = 0; turn_player_or_test_result < scan_player_or_sort_limit;
             ++turn_player_or_test_result, ++sort_cursor)
        {
          candidate_card_or_turn_player = *sort_cursor;
          pass_or_sort_result =
              has_subtype(pass_or_sort_player, candidate_card_or_turn_player, SUBTYPE_AURA);
          if (pass_or_sort_result == 0)
          {
            pass_or_sort_result = sort_cursor[1];
            if (has_subtype(pass_or_sort_player, pass_or_sort_result, SUBTYPE_AURA) != 0)
            {
              shift_cursor = sort_cursor;
              if (turn_player_or_test_result > 0)
              {
                while (true)
                {
                  shift_cursor[1] = candidate_card_or_turn_player;
                  --shift_cursor;
                  if (shift_cursor == marked_row)
                    break;
                  candidate_card_or_turn_player = *shift_cursor;
                }
              }
              *marked_row = pass_or_sort_result;
            }
          }
        }
      }
      marked_row += 100;
    }
  }
  effects_applied = 0;
  for (pass_or_sort_player = 0; pass_or_sort_player < 2; ++pass_or_sort_player)
  {
    scan_player_or_sort_limit = pass_or_sort_player == 0 ? current_turn : 1 - current_turn;
    if (t_player != scan_player_or_sort_limit && !both_players)
      continue;
    marked_count = mc[scan_player_or_sort_limit];
    for (i = 0; i < marked_count; ++i)
    {
      counter_type = COUNTER_invalid;
      counter_num = 0;
      candidate_card_or_turn_player = marked[scan_player_or_sort_limit][i];
      action = cook_action(scan_player_or_sort_limit, candidate_card_or_turn_player, raw_action,
                           &counter_type, &counter_num);
      if (action == 0xffffffff)
        return effects_applied;
      if (action != ~ACT_KILL_BURY)
      {
        candidate_card_or_turn_player =
            action_on_card_impl(player, card, scan_player_or_sort_limit,
                                candidate_card_or_turn_player, action, counter_type, counter_num);
        effects_applied += candidate_card_or_turn_player;
      }
    }
  }
  return effects_applied;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26675a0; original functions/manipulate_and_damage_all.c:245. */
int manipulate_all(int player, int card, int t_player, int type, int flag1, int subtype,
                        int flag2, int color, int flag3, int id, int flag4, int cc, int flag5,
                        actions_t action)
{
  int result;
  test_definition_t this_test;
  new_default_test_definition(&this_test, type, "");
  this_test.type_flag = (unsigned short)flag1;
  this_test.subtype = subtype;
  this_test.subtype_flag = (unsigned short)flag2;
  this_test.color = color;
  this_test.color_flag = (unsigned short)flag3;
  this_test.id = id;
  this_test.id_flag = (unsigned short)flag4;
  this_test.cmc = cc;
  this_test.cmc_flag = (unsigned short)flag5;
  this_test.type = type;
  result = new_manipulate_all(player, card, t_player, &this_test, action);
  return result;
}
#endif

int manipulate_type(int player, int card, int t_player, type_t type, actions_t action)
{
  test_definition_t this_test;
  new_default_test_definition(&this_test, type, "");
  return new_manipulate_all(player, card, t_player, &this_test, action);
}

int manipulate_auras_enchanting_target(int player, int card, int t_player, int t_card,
                                      test_definition_t *this_test, actions_t raw_action)
{
  int result = 0;
  for (int p = 0; p < 2; ++p)
    for (int c = active_cards_count[p] - 1; c >= 0; --c)
    {
      counter_t counter_type = COUNTER_invalid;
      int counter_num = 0;
      actions_t action = cook_action(p, c, raw_action, &counter_type, &counter_num);
      if (action == (actions_t)-1)
        return result;
      if (action == (actions_t)-2)
        continue;
      card_instance_t *aura = in_play(p, c);
      if (aura && aura->damage_target_player == t_player && aura->damage_target_card == t_card
          && is_what(p, c, TYPE_ENCHANTMENT) && has_subtype(p, c, SUBTYPE_AURA)
          && (!this_test || (new_make_test_in_play(p, c, -1, this_test)
              && (!this_test->not_me || player != p || card != c))))
        result += action_on_card_impl(player, card, p, c, action, counter_type, counter_num);
    }
  return result;
}

int new_damage_all(int player, int card, int targ_player, int dmg, int mode,
                   test_definition_t *this_test)
{
  int player_00;
  int i;
  card_instance_t *instance;
  int damage_card_or_type_check;
  int total_damage;
  int test_score;
  int turn_order_index;

  if (dmg < 1)
  {
    return 0;
  }
  test_score = 0;
  if (this_test != NULL)
  {
    test_score = new_get_test_score(this_test);
  }
  turn_order_index = 0;
  total_damage = 0;
  player_00 = current_turn;
  do
  {
    bool process_player = targ_player == player_00 || targ_player == 2;
    if (process_player)
    {
      i = active_cards_count[player_00] - 1;
      if (i >= 0)
      {
        do
        {
          instance = in_play(player_00, i);
          if (instance != NULL)
          {
            bool damage_permanent = false;
            if (this_test == NULL)
            {
              damage_card_or_type_check = is_what(player_00, i, 2);
              damage_permanent = (damage_card_or_type_check != 0 && (mode & 1U) != 0) ||
                                 ((damage_card_or_type_check = is_what(player_00, i, 0x100),
                                   damage_card_or_type_check != 0) &&
                                  (mode & 0x20U) != 0);
            }
            else
            {
              if (new_make_test_in_play(player_00, i, test_score, this_test) == 0)
              {
                is_what(player_00, i, 2);
                is_what(player_00, i, 0x100);
              }
              else
                damage_permanent = true;
            }
            if (damage_permanent &&
                (card != i || player != player_00 ||
                 ((mode & 4U) == 0 && (this_test == NULL || this_test->not_me != 1))))
            {
              damage_card_or_type_check = damage_creature(player_00, i, dmg, player, card);
              if (-1 < damage_card_or_type_check)
              {
                instance = get_card_instance(player, damage_card_or_type_check);
                if ((mode & 0x10U) != 0)
                {
                  instance->targets[3].card = instance->targets[3].card | 0x20;
                }
                if ((mode & 8U) != 0)
                {
                  instance->targets[3].card = instance->targets[3].card | 0x80;
                }
              }
              total_damage = total_damage + dmg;
            }
          }
          i = i - 1;
        } while (i != -1);
      }
      if ((mode & 2U) != 0)
        damage_player(player_00, dmg, player, card);
    }
    if (process_player || turn_order_index != 0)
    {
      player_00 = current_turn;
      current_turn = player_00;
    }
    turn_order_index = turn_order_index + 1;
    if (turn_order_index == 2)
      return total_damage;
    player_00 = 1 - player_00;
  } while (true);
}

void damage_all(int p, int c, int controller, int amount_damage, int all, int player_too, int keyword, int flag1, int subtype, int flag2, int color, int flag3, int id, int flag4, int cc, int flag5){
	test_definition_t this_test;
	default_test_definition(&this_test, TYPE_CREATURE);
	this_test.subtype = subtype;
	this_test.subtype_flag = flag2;
	this_test.color = color;
	this_test.color_flag = flag3;
	this_test.id = id;
	this_test.id_flag = flag4;
	this_test.cmc = cc;
	this_test.cmc_flag = flag5;
	this_test.keyword = keyword;
	this_test.keyword_flag = flag1;
	new_damage_all(p, c, controller, amount_damage, (1*all)+(2*player_too), &this_test);
}
