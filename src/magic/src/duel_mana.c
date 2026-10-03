#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <windows.h>

#include "cardartlib/src/assert.h"
#include "cardartlib/src/palette.h"
#include "deckdll/src/card_db.h"
#include "deckdll/src/magsnd.h"
#include "deckdll/src/shared_resources.h"
#include "drawcardlib/Drawcardlib.h"
#include "duel_engine.h"
#include "game_support.h"
#include "magic/src/duel_runtime.h"
#include "global_strings.h"
#include "shared_startup.h"

/* A second click on a pending spell/activation requests automatic payment;
 * it must not cast that spell or activate that ability again. */
static int mana_source_is_on_stack(int player, int card)
{
  int index;
  int stack_card;

  for (index = 0; index < g_stack_size; ++index)
  {
    if (global_stack_cards[index].player != player)
      continue;
    stack_card = global_stack_cards[index].card;
    if (stack_card == card)
      return 1;
    if (PLAYER_CARD_INSTANCE(player, stack_card).internal_card_id == g_stack_proxy_internal_card_id &&
        PLAYER_CARD_INSTANCE(player, stack_card).parent_controller == player &&
        PLAYER_CARD_INSTANCE(player, stack_card).parent_card == card)
      return 1;
  }
  return 0;
}

int select_target(int player, int card, target_definition_t *td, const char *prompt, target_t *ret_location)
{
  (void)player;
  (void)card;
  (void)td;
  (void)prompt;
  if (ret_location != NULL)
  {
    ret_location->player = -1;
    ret_location->card = -1;
  }
  return 0;
}

// FUNCTION: MOK 0x0042de60
// FUNCTION: MAGIC 0x00432f00
// FUNCTION: SHANDALAR 0x004143e0
int charge_mana(int player, color_t color, int amount);

// FUNCTION: MAGIC 0x0043410a
// FUNCTION: SHANDALAR 0x004155ee
void auto_pay_colored_mana(int player, int *mana_paid_by_color, int *total_mana_paid, int max_colorless)
{
  unsigned short produced_color;
  unsigned int special_mana;
  int slot;
  int color;

  for (color = 0; color < 7; ++color)
  {
    while (g_mana_charge[color] > 0 && g_raw_mana_available[player][color] > 0)
    {
      spend_mana_for_cost(g_mana_charge, color, 1, (int *)0, 0, player, color, mana_paid_by_color, total_mana_paid);
    }
  }

  while (g_mana_charge[6] > 0 && g_raw_mana_available[player][0] > 0 && max_colorless < g_mana_charge[6])
  {
    spend_mana_for_cost(g_mana_charge, 6, 1, (int *)0, 0, player, 0, mana_paid_by_color, total_mana_paid);
  }

  slot = 0;
  while (slot < 10 && g_mana_color_conversions[player][slot] != -1)
  {
    produced_color = (unsigned short)g_mana_color_conversions[player][slot];
    special_mana = (unsigned int)g_mana_color_conversions[player][slot];
    while (g_raw_mana_available[player][produced_color] > 0 && g_mana_charge[special_mana >> 16] > 0)
    {
      spend_mana_for_cost(
          g_mana_charge, special_mana >> 16, 1, (int *)0, 0, player, produced_color, mana_paid_by_color, total_mana_paid);
    }
    slot = slot + 1;
  }
}

// FUNCTION: MAGIC 0x004342b3
// FUNCTION: SHANDALAR 0x00415794
void auto_pay_variable_mana(int player,
                            int *mana_paid_by_color,
                            int *total_mana_paid,
                            int max_colorless,
                            int *special_mana_override,
                            int max_x)
{
  unsigned short produced_color;
  unsigned int special_mana;
  int slot;
  int color;

  for (color = 0; color < 7; ++color)
  {
    if (g_mana_charge[color] == -1)
    {
      while (g_raw_mana_available[player][color] > 0 && (g_x_value < max_x || max_x == -1))
      {
        spend_mana_for_cost(
            g_mana_charge, color, 1, special_mana_override, max_x, player, color, mana_paid_by_color, total_mana_paid);
      }
    }
  }

  if (g_mana_charge[6] == -1)
  {
    while (g_raw_mana_available[player][0] > 0 && max_colorless < g_mana_charge[6] &&
           (g_x_value < max_x || max_x == -1))
    {
      spend_mana_for_cost(
          g_mana_charge, 6, 1, special_mana_override, max_x, player, 0, mana_paid_by_color, total_mana_paid);
    }
  }

  slot = 0;
  while (slot < 10 && g_mana_color_conversions[player][slot] != -1)
  {
    produced_color = (unsigned short)g_mana_color_conversions[player][slot];
    special_mana = (unsigned int)g_mana_color_conversions[player][slot];
    if (g_mana_charge[special_mana >> 16] == -1)
    {
      while (g_raw_mana_available[player][produced_color] > 0 && (g_x_value < max_x || max_x == -1))
      {
        spend_mana_for_cost(g_mana_charge,
                            special_mana >> 16,
                            1,
                            special_mana_override,
                            max_x,
                            player,
                            produced_color,
                            mana_paid_by_color,
                            total_mana_paid);
      }
    }
    slot = slot + 1;
  }
}

// FUNCTION: MAGIC 0x004344b0
// FUNCTION: SHANDALAR 0x0041598e
void auto_pay_generic_mana(int player,
                           int *mana_paid_by_color,
                           int *total_mana_paid,
                           int max_colorless,
                           int *special_mana_override,
                           int max_x)
{
  int color;

  if (g_mana_charge[COLOR_ARTIFACT] != 0)
  {
    for (color = 0; color < 7; ++color)
    {
      if (g_mana_charge[COLOR_ARTIFACT] > 0)
      {
        while (g_raw_mana_available[player][color] > 0 && max_colorless < g_mana_charge[COLOR_ARTIFACT])
        {
          spend_mana_for_cost(g_mana_charge, COLOR_ARTIFACT, 1, (int *)0, 0, player, color, mana_paid_by_color, total_mana_paid);
        }
      }
      else
      {
        while (g_raw_mana_available[player][color] > 0 && g_mana_charge[COLOR_ARTIFACT] == -1 &&
               (max_x == -1 || max_colorless < max_x - g_x_value))
        {
          spend_mana_for_cost(g_mana_charge,
                              COLOR_ARTIFACT,
                              1,
                              special_mana_override,
                              max_x,
                              player,
                              color,
                              mana_paid_by_color,
                              total_mana_paid);
        }
      }
    }
  }

  if (g_mana_charge[COLOR_COLORLESS] != 0)
  {
    for (color = 0; color < 7; ++color)
    {
      if (color != COLOR_ARTIFACT)
      {
        if (g_mana_charge[COLOR_COLORLESS] == -1)
        {
          while (g_raw_mana_available[player][color] > 0 && (g_x_value < max_x || max_x == -1))
          {
            spend_mana_for_cost(g_mana_charge,
                                COLOR_COLORLESS,
                                1,
                                special_mana_override,
                                max_x,
                                player,
                                color,
                                mana_paid_by_color,
                                total_mana_paid);
          }
        }
        else
        {
          while (g_mana_charge[COLOR_COLORLESS] > 0 && g_raw_mana_available[player][color] > 0)
          {
            spend_mana_for_cost(g_mana_charge,
                                COLOR_COLORLESS,
                                1,
                                (int *)0,
                                0,
                                player,
                                color,
                                mana_paid_by_color,
                                total_mana_paid);
          }
        }
      }
    }
  }
}

// FUNCTION: MAGIC 0x004346ab
// FUNCTION: SHANDALAR 0x00415b89
int is_mana_cost_paid(int *mana_cost, int required_colorless, int max_x)
{
  int color;

  for (color = 0; color < 7; color++)
  {
    if (mana_cost[color] > 0)
    {
      return 0;
    }

    if (mana_cost[color] == -1 && max_x != -1 && required_colorless < max_x)
    {
      return 0;
    }

    if (mana_cost[color] == -1 && max_x == -1)
    {
      return 0;
    }
  }

  return 1;
}

// FUNCTION: MAGIC 0x00434746
// FUNCTION: SHANDALAR 0x00415c24
unsigned int get_required_mana_color_mask(int *mana)
{
  int color;
  unsigned int result;

  result = 0;
  for (color = 0; color < 7; ++color)
  {
    if (mana[color] != 0)
    {
      result = result | (1 << (unsigned char)color);
    }
  }

  return result;
}

// FUNCTION: MAGIC 0x0043479e
// FUNCTION: SHANDALAR 0x00415c7c
int format_mana_payment_prompt(char *prompt, int *mana_cost, int x_paid, int max_x);

// FUNCTION: MAGIC 0x00434af9
// FUNCTION: SHANDALAR 0x00415fd7
int activate_mana_sources_for_payment(int player,
                                      int *mana_paid_by_color,
                                      int *total_mana_paid,
                                      int activation_flags,
                                      int include_special_mana)
{
  int special_color;
  int produced_color;
  int scratch;
  int dual_mana_slot;
  int max_colorless;
  int color_scratch;
  int color;
  int can_activate;
  int current_card;
  int any_variable_costs;

  any_variable_costs = 0;
  for (color = 0; color < 7; ++color)
  {
    if (g_mana_charge[color] == -1)
    {
      any_variable_costs = 1;
    }
  }

  if (player == g_other_player && (g_duel_network_flags & 2) != 0)
  {
    current_card = 0;
    while (current_card != -1)
    {
      TENTATIVE_wait_for_network_result(player, 0x10);
      current_card = g_network_result_packet.result;
      if (current_card != -1 && activate_mana_source_for_payment(player, current_card) != 0)
      {
        max_colorless = has_mana(player, COLOR_ARTIFACT, 1) - has_mana(player, COLOR_ANY, 1);
        auto_pay_colored_mana(player, mana_paid_by_color, total_mana_paid, max_colorless);
        auto_pay_variable_mana(player, mana_paid_by_color, total_mana_paid, max_colorless, &g_x_value, g_max_x_value);
        auto_pay_generic_mana(player, mana_paid_by_color, total_mana_paid, max_colorless, &g_x_value, g_max_x_value);
      }
    }
    copy_mana_pool_to_display();
  }
  else
  {
    if (is_mana_cost_paid(g_mana_charge, g_x_value, g_max_x_value) == 0)
    {
      for (current_card = 0; current_card < g_active_cards_count[player]; ++current_card)
      {
        if (can_activate_mana_source(player, current_card, activation_flags) != 0 &&
            dispatch_event_to_single_card(player, current_card, EVENT_CAN_ACTIVATE, 1 - player, -1) != 0)
        {
          can_activate = 0;
          for (color = 0; color < 7; ++color)
          {
            if (g_mana_charge[color] > 0 && (((unsigned int)(1 << (unsigned char)color) & (unsigned int)(unsigned char)PLAYER_CARD_INSTANCE(player, current_card).mana_color) != 0))
            {
              can_activate = 1;
            }
            else if (g_mana_charge[color] == -1 && g_x_value < g_max_x_value && g_max_x_value != -1 && (((unsigned int)(1 << (unsigned char)color) & (unsigned int)(unsigned char)PLAYER_CARD_INSTANCE(player, current_card).mana_color) != 0))
            {
              can_activate = 1;
            }
          }
          if (include_special_mana != 0 && !can_activate)
          {
            dual_mana_slot = 0;
            while (dual_mana_slot < 10 && g_mana_color_conversions[player][dual_mana_slot] != -1)
            {
              produced_color = (unsigned short)g_mana_color_conversions[player][dual_mana_slot];
              if (g_mana_charge[g_mana_color_conversions[player][dual_mana_slot] >> 16] > 0 && (((unsigned int)(1 << produced_color) & (unsigned int)(unsigned char)PLAYER_CARD_INSTANCE(player, current_card).mana_color) != 0))
              {
                can_activate = 1;
              }
              else if (g_mana_charge[g_mana_color_conversions[player][dual_mana_slot] >> 16] == -1 && g_x_value < g_max_x_value && g_max_x_value != -1 && (((unsigned int)(1 << produced_color) & (unsigned int)(unsigned char)PLAYER_CARD_INSTANCE(player, current_card).mana_color) != 0))
              {
                can_activate = 1;
              }
              ++dual_mana_slot;
            }
          }
          if (can_activate)
          {
            if ((g_duel_network_flags & 2) != 0)
            {
              g_network_result_packet.packet_type = 0x10;
              g_network_result_packet.result = current_card;
              TENTATIVE_send_network_result(player, 0x10);
              Sleep(0x32);
            }
            if (activate_mana_source_for_payment(player, current_card) != 0)
            {
              max_colorless = has_mana(player, COLOR_ARTIFACT, 1) - has_mana(player, COLOR_ANY, 1);
              auto_pay_colored_mana(player, mana_paid_by_color, total_mana_paid, max_colorless);
              auto_pay_variable_mana(player, mana_paid_by_color, total_mana_paid, max_colorless, &g_x_value, g_max_x_value);
              auto_pay_generic_mana(player, mana_paid_by_color, total_mana_paid, max_colorless, &g_x_value, g_max_x_value);
            }
          }
        }
      }
    }

    if (is_mana_cost_paid(g_mana_charge, g_x_value, g_max_x_value) == 0 &&
        (g_mana_charge[0] != 0 || g_mana_charge[6] != 0))
    {
      for (current_card = 0; current_card < g_active_cards_count[player]; ++current_card)
      {
        if (can_activate_mana_source(player, current_card, activation_flags) != 0 &&
            dispatch_event_to_single_card(player, current_card, EVENT_CAN_ACTIVATE, 1 - player, -1) != 0)
        {
          can_activate = 0;
          if (g_mana_charge[0] < 1)
          {
            if (g_mana_charge[0] == -1 && g_x_value < g_max_x_value && g_max_x_value != -1)
            {
              can_activate = 1;
            }
            else if (g_mana_charge[6] < 1)
            {
              if (g_mana_charge[6] == -1 && g_x_value < g_max_x_value && g_max_x_value != -1)
              {
                can_activate = 1;
              }
            }
            else
            {
              can_activate = 1;
            }
          }
          else
          {
            can_activate = 1;
          }
          if (PLAYER_CARD_INSTANCE(player, current_card).mana_color == '@' && g_mana_charge[6] == 0)
          {
            can_activate = 0;
          }
          if (can_activate)
          {
            if ((g_duel_network_flags & 2) != 0)
            {
              g_network_result_packet.packet_type = 0x10;
              g_network_result_packet.result = current_card;
              TENTATIVE_send_network_result(player, 0x10);
              Sleep(0x32);
            }
            if (activate_mana_source_for_payment(player, current_card) != 0)
            {
              max_colorless = has_mana(player, COLOR_ARTIFACT, 1) - has_mana(player, COLOR_ANY, 1);
              auto_pay_colored_mana(player, mana_paid_by_color, total_mana_paid, max_colorless);
              auto_pay_variable_mana(player, mana_paid_by_color, total_mana_paid, max_colorless, &g_x_value, g_max_x_value);
              auto_pay_generic_mana(player, mana_paid_by_color, total_mana_paid, max_colorless, &g_x_value, g_max_x_value);
            }
          }
        }
      }
    }

    if (is_mana_cost_paid(g_mana_charge, g_x_value, g_max_x_value) == 0 &&
        any_variable_costs && g_max_x_value == -1)
    {
      for (current_card = 0; current_card < g_active_cards_count[player]; ++current_card)
      {
        if (can_activate_mana_source(player, current_card, activation_flags) != 0 &&
            dispatch_event_to_single_card(player, current_card, EVENT_CAN_ACTIVATE, 1 - player, -1) != 0)
        {
          can_activate = 0;
          for (color = 1; color < 7; ++color)
          {
            if (g_max_x_value == -1 && g_mana_charge[color] == -1 && (((unsigned int)(1 << (unsigned char)color) & (unsigned int)(unsigned char)PLAYER_CARD_INSTANCE(player, current_card).mana_color) != 0))
            {
              can_activate = 1;
            }
            else if (g_max_x_value == -1 && g_mana_charge[0] == -1)
            {
              can_activate = 1;
            }
            else if (g_max_x_value == -1 && g_mana_charge[6] == -1)
            {
              can_activate = 1;
            }
          }
          if (PLAYER_CARD_INSTANCE(player, current_card).mana_color == '@' && g_mana_charge[6] != -1)
          {
            can_activate = 0;
          }
          if (include_special_mana != 0 && !can_activate)
          {
            dual_mana_slot = 0;
            while (dual_mana_slot < 10 && g_mana_color_conversions[player][dual_mana_slot] != -1)
            {
              produced_color = (unsigned short)g_mana_color_conversions[player][dual_mana_slot];
              special_color = ((unsigned int)g_mana_color_conversions[player][dual_mana_slot] >> 16) & 0xffff;
              if (g_max_x_value == -1 && g_mana_charge[special_color] == -1 && (((unsigned int)(1 << produced_color) & (unsigned int)(unsigned char)PLAYER_CARD_INSTANCE(player, current_card).mana_color) != 0))
              {
                can_activate = 1;
              }
              ++dual_mana_slot;
            }
          }
          if (can_activate)
          {
            if ((g_duel_network_flags & 2) != 0)
            {
              g_network_result_packet.packet_type = 0x10;
              g_network_result_packet.result = current_card;
              TENTATIVE_send_network_result(player, 0x10);
              Sleep(0x32);
            }
            if (activate_mana_source_for_payment(player, current_card) != 0)
            {
              max_colorless = has_mana(player, COLOR_ARTIFACT, 1) - has_mana(player, COLOR_ANY, 1);
              auto_pay_colored_mana(player, mana_paid_by_color, total_mana_paid, max_colorless);
              auto_pay_variable_mana(player, mana_paid_by_color, total_mana_paid, max_colorless, &g_x_value, g_max_x_value);
              auto_pay_generic_mana(player, mana_paid_by_color, total_mana_paid, max_colorless, &g_x_value, g_max_x_value);
            }
          }
        }
      }
    }

    if ((g_duel_network_flags & 2) != 0)
    {
      g_network_result_packet.packet_type = 0x10;
      g_network_result_packet.result = -1;
      TENTATIVE_send_network_result(player, 0x10);
    }
    copy_mana_pool_to_display();
  }

  return 1;
}

// FUNCTION: MAGIC 0x00435643
// FUNCTION: SHANDALAR 0x00416b1c
int can_activate_mana_source(int player, int card, int activation_flags)
{
  int csvid;
  int result;
  int internal_card_id;

  if (mana_source_is_on_stack(player, card))
    return 0;

  internal_card_id = PLAYER_CARD_INSTANCE(player, card).internal_card_id;
  csvid = global_cards_data[internal_card_id].id;
  result = 1;

  if ((PLAYER_CARD_INSTANCE(player, card).state & (STATE_OUBLIETTED | STATE_IN_PLAY)) == STATE_IN_PLAY && (global_cards_data[internal_card_id].extra_ability & 0x1000) != 0 && (global_cards_data[internal_card_id].extra_ability & 0x10000) == 0)
  {
    if ((activation_flags & 1) && (internal_card_id <= 4 || csvid == 0x366 || PLAYER_CARD_INSTANCE(player, card).mana_color == '@'))
    {
      result = 0;
    }
    if ((activation_flags & 2) && (global_cards_data[internal_card_id].type & TYPE_LAND) != 0 && internal_card_id > 4 && csvid != 0x366 && PLAYER_CARD_INSTANCE(player, card).mana_color != '@')
    {
      result = 0;
    }
    if ((activation_flags & 4) && (PLAYER_CARD_INSTANCE(player, card).state & STATE_NO_AUTO_TAPPING) != 0)
    {
      result = 0;
    }
    if ((activation_flags & 8) && (global_cards_data[internal_card_id].type & TYPE_ARTIFACT) != 0)
    {
      result = 0;
    }
    if ((activation_flags & 0x10) && (global_cards_data[internal_card_id].type & TYPE_CREATURE) != 0)
    {
      result = 0;
    }
  }
  else
  {
    result = 0;
  }

  return result;
}

// FUNCTION: MAGIC 0x00435881
// FUNCTION: SHANDALAR 0x00416d5a
int activate_mana_source_for_payment(int player, int card);

// FUNCTION: MAGIC 0x00435b18
// FUNCTION: SHANDALAR 0x00416ff1
void spend_mana_for_cost(int *mana_cost,
                         int cost_color,
                         int amount,
                         int *special_mana,
                         int max_x,
                         int player,
                         int mana_color,
                         int *mana_paid_by_color,
                         int *total_mana_paid)
{
  (void)max_x;

  unproduce_mana(player, mana_color, amount);
  if (mana_cost[cost_color] == -1)
  {
    *special_mana += amount;
  }
  else
  {
    mana_cost[cost_color] -= amount;
  }
  mana_paid_by_color[mana_color] += amount;
  record_paid_mana(mana_color, amount);
  *total_mana_paid += amount;
}

// FUNCTION: MAGIC 0x00435b88
// FUNCTION: SHANDALAR 0x00417061
int get_mana_payment_amount(int *mana_cost,
                            int cost_color,
                            int *mana_available,
                            int mana_color,
                            int allow_multi_pay,
                            int max_x,
                            int x_paid)
{
  int amount;

  if (allow_multi_pay != 0)
  {
    if (mana_cost[cost_color] == -1)
    {
      if (max_x == -1)
      {
        amount = mana_available[mana_color];
      }
      else
      {
        amount = mana_available[mana_color] < max_x - x_paid ? mana_available[mana_color] : max_x - x_paid;
      }
    }
    else
    {
      amount = mana_cost[cost_color] >= mana_available[mana_color] ? mana_available[mana_color] : mana_cost[cost_color];
    }
  }
  else
  {
    amount = 1;
  }

  return amount;
}

// FUNCTION: MAGIC 0x00435c1f
// FUNCTION: SHANDALAR 0x004170f8
int begin_mana_payment_record(void)
{
  int color;

  if (g_mana_payment_record_depth < 10)
  {
    for (color = 0; color < 7; ++color)
    {
      g_recorded_mana_payments[g_mana_payment_record_depth][color] = 0;
    }
    ++g_mana_payment_record_depth;
    return 1;
  }
  else
  {
    return 0;
  }
}

// FUNCTION: MAGIC 0x00435c91
// FUNCTION: SHANDALAR 0x0041716b
int record_paid_mana(int color, int amount)
{
  if (0 < g_mana_payment_record_depth)
  {
    g_recorded_mana_payments[g_mana_payment_record_depth - 1][color] += amount;
    return 1;
  }
  else
    return 0;
}

// FUNCTION: MAGIC 0x00435cdd
// FUNCTION: SHANDALAR 0x004171b7
int end_mana_payment_record(void)
{
  if (g_mana_payment_record_depth > 0)
  {
    --g_mana_payment_record_depth;
    return 1;
  }
  else
  {
    return 0;
  }
}

// FUNCTION: MAGIC 0x00435d11
// FUNCTION: SHANDALAR 0x004171eb
int restore_recorded_mana_to_pool(int player)
{
  int color;

  if (g_mana_payment_record_depth > 0)
  {
    for (color = 0; color < 7; ++color)
    {
      g_raw_mana_available[player][color] += g_recorded_mana_payments[g_mana_payment_record_depth - 1][color];
      g_raw_mana_available[player][7] += g_recorded_mana_payments[g_mana_payment_record_depth - 1][color];
    }

    return 1;
  }
  else
  {
    return 0;
  }
}

// FUNCTION: MAGIC 0x00435db0
// FUNCTION: SHANDALAR 0x0041728a
int refund_paid_mana(int *mana_paid_by_color)
{
  int color;

  if (g_mana_payment_record_depth > 0)
  {
    for (color = 0; color < 7; ++color)
    {
      g_recorded_mana_payments[g_mana_payment_record_depth - 1][color] -= mana_paid_by_color[color];
    }

    return 1;
  }
  else
  {
    return 0;
  }
}

// FUNCTION: MOK 0x004302c0
// FUNCTION: MAGIC 0x00435e27
// FUNCTION: SHANDALAR 0x00417301
int charge_mana_w_global_cost_mod(int player, int card, int color, int amount)
{
  int result;

  g_mana_charge[0] += g_mana_cost_modifiers_by_color[single_color_test_bit_to_color_t(PLAYER_CARD_INSTANCE(player, card).color)];
  result = charge_mana(player, color, amount) - g_mana_cost_modifiers_by_color[single_color_test_bit_to_color_t(PLAYER_CARD_INSTANCE(player, card).color)];
  if (g_spell_fizzled == 1)
  {
    return 0;
  }
  return result;
}

// FUNCTION: MAGIC 0x0043fdb3
// FUNCTION: SHANDALAR 0x0040b633
int can_pay_card_mana_cost(int player, int target_player, int target_card);

// FUNCTION: MAGIC 0x00442cf9
// FUNCTION: SHANDALAR 0x0040e593
int is_nonactivated_mana_source(int player, int card)
{
  int result;

  if ((global_cards_data[PLAYER_CARD_INSTANCE(player, card).internal_card_id].extra_ability & EA_MANA_SOURCE) != 0)
  {
    if ((global_cards_data[PLAYER_CARD_INSTANCE(player, card).internal_card_id].extra_ability & EA_ACT_ABILITY) != 0)
    {
      result = 0;
    }
    else
    {
      result = 1;
    }
  }
  else
  {
    result = 0;
  }

  return result;
}

// FUNCTION: MAGIC 0x00445f61
// FUNCTION: SHANDALAR 0x004117fa
int card_activation_uses_x(int player, int card)
{
  int internal_card_id;

  if (player == -1 || card == -1)
  {
    return 0;
  }

  internal_card_id = PLAYER_CARD_INSTANCE(player, card).internal_card_id;
  if (g_stack_proxy_internal_card_id == internal_card_id)
  {
    internal_card_id = PLAYER_CARD_INSTANCE(player, card).original_internal_card_id;
  }

  if (internal_card_id == -1)
  {
    return 0;
  }

  if ((global_cards_data[internal_card_id].extra_ability & EA_ACT_USE_X) != 0)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

// FUNCTION: MAGIC 0x0044b3d4
// FUNCTION: SHANDALAR 0x00403d55
void resolve_mana_burn(void);

// FUNCTION: MAGIC 0x0044b6af
// FUNCTION: SHANDALAR 0x00404030
int can_activate_mana_source_for_stop_prompt(int player, int card);

// FUNCTION: MAGIC 0x0044b7d4
// FUNCTION: SHANDALAR 0x00404155
void activate_mana_source_card(int player, int card);

// FUNCTION: MAGIC 0x004a6d0e
// FUNCTION: SHANDALAR 0x00558e90
void count_mana(void)
{
  card_instance_t *instance;
  int current_card;
  int player;

  for (current_card = 0; current_card <= 7; ++current_card)
  {
    g_mana_cost_modifiers_by_color[current_card] = 0;
    g_special_mana_pool[1][current_card] = g_mana_cost_modifiers_by_color[current_card];
    g_special_mana_pool[0][current_card] = g_special_mana_pool[1][current_card];
  }

  g_raw_mana_available_hex[1][0] = -1;
  g_raw_mana_available_hex[0][0] = g_raw_mana_available_hex[1][0];
  g_mana_color_conversions[1][0] = -1;
  g_mana_color_conversions[0][0] = g_mana_color_conversions[1][0];
  for (player = 0; player < 2; ++player)
  {
    instance = global_card_instances[player];
    for (current_card = 0; current_card < g_active_cards_count[player];
         ++current_card, ++instance)
    {
      if (instance->internal_card_id == -1)
      {
        continue;
      }

      if ((global_cards_data[instance->internal_card_id].extra_ability & 0x1000) != 0 && (instance->state & (STATE_OUBLIETTED | STATE_TAPPED | STATE_IN_PLAY)) == STATE_IN_PLAY)
      {
        dispatch_event(player, current_card, EVENT_COUNT_MANA);
      }
      if (global_cards_data[instance->internal_card_id].id == 0xee && (instance->state & (STATE_OUBLIETTED | STATE_TAPPED | STATE_IN_PLAY)) == STATE_IN_PLAY)
      {
        dispatch_event_to_single_card(player, current_card, EVENT_COUNT_MANA, -1, -1);
      }
      if (global_cards_data[instance->internal_card_id].id == 100 && (instance->state & (STATE_OUBLIETTED | STATE_IN_PLAY)) == STATE_IN_PLAY)
      {
        dispatch_event_to_single_card(player, current_card, EVENT_COUNT_MANA, -1, -1);
      }
    }
  }
  if ((g_battlefield_extra_ability_flags & 0x08000000) != 0)
  {
    for (player = 0; player < 2; ++player)
    {
      instance = global_card_instances[player];
      for (current_card = 0; current_card < g_active_cards_count[player];
           ++current_card, ++instance)
      {
        if (instance->internal_card_id == -1)
        {
          continue;
        }

        if (global_cards_data[instance->internal_card_id].code_pointer == card_fellwar_stone && (instance->state & (STATE_OUBLIETTED | STATE_TAPPED | STATE_IN_PLAY)) == STATE_IN_PLAY)
        {
          dispatch_event_to_single_card(player, current_card, EVENT_VARIABLE_MANA_SRC, -1, -1);
        }
      }
    }
  }
}
// FUNCTION: MAGIC 0x004a6faa
// FUNCTION: SHANDALAR 0x0055912c
void C_count_colors_of_lands_in_play(void);

// FUNCTION: MAGIC 0x004eab5c
// FUNCTION: SHANDALAR 0x004412ec
void declare_mana_available_hex(int player, color_test_t colors, int amount)
{
  int i;

  if ((int)colors > 0)
  {
    for (i = 0; i < 0x32; ++i)
    {
      if (g_raw_mana_available_hex[player][i] == -1)
      {
        g_raw_mana_available_hex[player][i] = (amount << 16) | colors;
        g_raw_mana_available_hex[player][i + 1] = -1;
        break;
      }
    }
  }
}

// FUNCTION: MAGIC 0x004eac0c
// FUNCTION: SHANDALAR 0x0044139c
void undeclare_mana_available_hex(int player, color_test_t color, int amount)
{
  int j;     /* [ebp-0x0c] */
  int i;     /* [ebp-0x08] */
  int found; /* [ebp-0x04] */

  found = 0;
  i = 0;

  while (i < 0x32)
  {
    if (g_raw_mana_available_hex[player][i] == -1)
    {
      break;
    }

    if (found != 0)
    {
      break;
    }

    if (g_raw_mana_available_hex[player][i] == ((amount << 16) | color))
    {
      found = 1;
      j = i;
      while (j < 0x31)
      {
        g_raw_mana_available_hex[player][j] = g_raw_mana_available_hex[player][j + 1];
        ++j;
      }
    }

    ++i;
  }
}

// FUNCTION: MAGIC 0x004eaceb
// FUNCTION: SHANDALAR 0x0044147b
void declare_mana_color_conversion(int player, int color_to_produce, int color_to_consume)
{
  int slot;
  int found;

  if (color_to_produce > 0 && color_to_consume > 0)
  {
    found = 0;
    slot = 0;
    for (; slot < 10 && !found; ++slot)
    {
      if (g_mana_color_conversions[player][slot] == -1)
      {
        found = 1;
        g_mana_color_conversions[player][slot] = MAKELONG(color_to_produce, color_to_consume);
        g_mana_color_conversions[player][slot + 1] = -1;
      }
    }
  }
}

// FUNCTION: MAGIC 0x004eada9
// FUNCTION: SHANDALAR 0x00441539
int declare_mana_available(int player, color_t color, int amount)
{
  g_special_mana_pool[player][color] += amount;
  g_special_mana_pool[player][COLOR_ANY] += amount;
  return g_special_mana_pool[player][color];
}

// FUNCTION: MAGIC 0x004eadeb
// FUNCTION: SHANDALAR 0x0044157b
int undeclare_mana_available(int player, color_t color, int amount)
{
  g_special_mana_pool[player][color] -= amount;
  g_special_mana_pool[player][COLOR_ANY] -= amount;
  return g_special_mana_pool[player][color];
}

// FUNCTION: MAGIC 0x004eae35
// FUNCTION: SHANDALAR 0x004415c5
int produce_mana(int player, color_t color, int amount)
{
  g_raw_mana_available[player][color] += amount;
  g_raw_mana_available[player][7] += amount;
  return g_raw_mana_available[player][color];
}

// FUNCTION: MAGIC 0x004eae77
// FUNCTION: SHANDALAR 0x00441607
int unproduce_mana(int player, int color, int amount)
{
  g_raw_mana_available[player][color] -= amount;
  g_raw_mana_available[player][7] -= amount;
  return g_raw_mana_available[player][color];
}

// FUNCTION: MAGIC 0x004eaec1
// FUNCTION: SHANDALAR 0x00441651
int undeclare_mana_available_and_produce_it(int player, color_t color, int amount)
{
  undeclare_mana_available(player, color, amount);
  produce_mana(player, color, amount);
  return g_raw_mana_available[player][color];
}

// FUNCTION: MOK 0x0049d510
// FUNCTION: MAGIC 0x004eaf09
// FUNCTION: SHANDALAR 0x00441699
int has_mana(int player, color_t color, int amount)
{
  int result;                   // ebp-0x2c
  int available_mana;           // ebp-0x28
  unsigned int interchangeable; // ebp-0x24
  color_t effective_color;      // ebp-0x20
  int current_color;            // ebp-0x1c
  unsigned int mana_color;      // ebp-0x18
  int special_mana;             // ebp-0x14
  int artifact_mana;            // ebp-0x10
  int xmana;                    // ebp-0xc
  int artifact_special_mana;    // ebp-0x8
  int mana_source_color;        // ebp-0x4 (only low byte used)

  if (color == COLOR_ARTIFACT)
  {
    effective_color = COLOR_ANY;
  }
  else if (color == COLOR_COLORLESS)
  {
    effective_color = COLOR_ANY;
  }
  else
  {
    effective_color = color;
  }

  if (amount == 0)
  {
    return 1;
  }

  interchangeable = 0;
  for (current_color = 0; current_color < 10 && g_mana_color_conversions[player][current_color] != -1; ++current_color)
  {
    mana_source_color = (unsigned short)g_mana_color_conversions[player][current_color];
    mana_color = HIWORD(g_mana_color_conversions[player][current_color]);
    if (effective_color == (color_t)mana_color)
    {
      interchangeable |= 1 << (unsigned char)mana_source_color;
    }
  }

  available_mana = g_raw_mana_available[player][effective_color];
  for (current_color = COLOR_COLORLESS; current_color < 7; ++current_color)
  {
    if (effective_color != (color_t)current_color && (interchangeable & (1 << (unsigned char)current_color)) != 0)
    {
      available_mana += g_raw_mana_available[player][current_color];
    }
  }

  artifact_mana = g_raw_mana_available[player][COLOR_ARTIFACT];

  special_mana = g_special_mana_pool[player][effective_color];
  for (current_color = COLOR_COLORLESS; current_color < 7; ++current_color)
  {
    if (effective_color != (color_t)current_color && (interchangeable & (1 << (unsigned char)current_color)) != 0)
    {
      special_mana += g_special_mana_pool[player][current_color];
    }
  }

  xmana = 0;
  for (current_color = 0; current_color < 0x32 && g_raw_mana_available_hex[player][current_color] != -1; ++current_color)
  {
    if (effective_color == COLOR_ANY)
    {
      xmana += g_raw_mana_available_hex[player][current_color] >> 16;
    }
    else if ((g_raw_mana_available_hex[player][current_color] & (1 << (unsigned char)effective_color)) != 0)
    {
      xmana += g_raw_mana_available_hex[player][current_color] >> 16;
    }
    else
    {
      if ((interchangeable & g_raw_mana_available_hex[player][current_color]) != 0)
      {
        xmana += g_raw_mana_available_hex[player][current_color] >> 16;
      }
    }
  }

  artifact_special_mana = g_special_mana_pool[player][COLOR_ARTIFACT];

  if (color == COLOR_ANY || color == COLOR_COLORLESS)
  {
    result = ((available_mana - artifact_mana) + special_mana - artifact_special_mana) + xmana;
  }
  else
  {
    result = (available_mana + special_mana) + xmana;
  }

  if (result >= amount)
  {
    return result;
  }
  else
  {
    return 0;
  }
}

// FUNCTION: MOK 0x0049d710
// FUNCTION: MAGIC 0x004eb23d
// FUNCTION: SHANDALAR 0x004419ca
int has_mana_w_global_cost_mod(int player, int card, color_t color, int amount)
{
  int result;

  result = 0;

  if (amount == 0)
  {
    if (g_mana_cost_modifiers_by_color[single_color_test_bit_to_color_t(PLAYER_CARD_INSTANCE(player, card).color)] > 0)
    {
      return has_mana(
          player, 7, g_mana_cost_modifiers_by_color[single_color_test_bit_to_color_t(PLAYER_CARD_INSTANCE(player, card).color)]);
    }

    return 1;
  }
  else
  {
    result = has_mana(player, color, amount);
    if (result != 0)
    {
      if (g_mana_cost_modifiers_by_color[single_color_test_bit_to_color_t(PLAYER_CARD_INSTANCE(player, card).color)] > 0)
      {
        return has_mana(
            player,
            7,
            g_mana_cost_modifiers_by_color[single_color_test_bit_to_color_t(PLAYER_CARD_INSTANCE(player, card).color)] + amount);
      }
      else
      {
        return result;
      }
    }
  }

  return 0;
}
