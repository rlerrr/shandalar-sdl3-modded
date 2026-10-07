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
#include "duel_host.h"
#include "game_support.h"
#include "magic/src/duel_runtime.h"
#include "global_strings.h"
#include "shared_startup.h"

// FUNCTION: MAGIC 0x0043e18b
// FUNCTION: SHANDALAR 0x00409a0b
int draw_card_for_player(int player);

// FUNCTION: MAGIC 0x0043ec38
// FUNCTION: SHANDALAR 0x0040a4b8
int discard_card_from_hand(int player, int card)
{
  dispatch_event_to_single_card(player, card, EVENT_DISCARD, 1 - player, -1);
  move_card_to_graveyard(player, card);
  PLAYER_CARD_INSTANCE(player, card).internal_card_id = -1;
  request_duel_display_refresh_if_human(player, card, 11, 1);
  if (g_duel_ai_mode_state != 1)
  {
    play_sound_effect(WAV_DISCARD);
  }
  --g_duel_summary.hand_counts[player];
  return 0;
}

// FUNCTION: MAGIC 0x0043ece1
// FUNCTION: SHANDALAR 0x0040a561
int resolve_cast_card(int player, int card)
{
  int force_special_mode;

  force_special_mode = 0;
  if (g_current_action_event_code == 0xd3)
  {
    g_duel_state_00791418 = 1;
    force_special_mode = 1;
  }

  if (!put_card_on_stack(player, card, 0))
  {
  }
  else
  {
    if (!put_card_on_stack(player, card, 1))
    {
    }
    else
    {
      if (!resolve_card_on_stack(player, card))
      {
      }
      else
      {
        if (force_special_mode != 0)
        {
          g_duel_state_00791418 = 0;
        }
        return 1;
      }
    }
  }

  if (force_special_mode != 0)
  {
    g_duel_state_00791418 = 0;
  }
  return 0;
}

// FUNCTION: MAGIC 0x0043edab
// FUNCTION: SHANDALAR 0x0040a62b
int put_card_on_stack(int player, int card, int mode)
{
  int trace_counter;                          // ebp-0x348
  char prompt_text[300];                      // ebp-0x344
  int tmp_mana;                               // ebp-0x218
  char trace_text[500];                       // ebp-0x214
  int saved_unk_008ce508;                     // ebp-0x20
  int saved_max_x_value;                      // ebp-0x1c
  int chosen_x_value;                         // ebp-0x18
  int color_index;                            // ebp-0x14
  int saved_current_casting_internal_card_id; // ebp-0x10
  int internal_card_id;                       // ebp-0xc
  card_data_t *card_data;                     // ebp-0x8
  int saved_unk_008ce4f4;                     // ebp-0x4

  internal_card_id = PLAYER_CARD_INSTANCE(player, card).internal_card_id;
  color_index = (int)single_color_test_bit_to_color_t((int)(char)global_cards_data[internal_card_id].color);
  card_data = &global_cards_data[internal_card_id];

  if (mode != 0)
  {
    goto respond_to_card_on_stack;
  }

  if (TRACE_ENABLED)
  {
    trace_counter = g_duel_trace_counter;
    ++g_duel_trace_counter;
    sprintf(trace_text, "%d: Player #%d is casting %s(%d).\n", trace_counter, player, global_cards_data[PLAYER_CARD_INSTANCE(player, card).internal_card_id].name, card);
    append_to_trace_txt(trace_text);
  }

  if (!can_pay_card_mana_cost(player, player, card))
  {
    return 0;
  }

  g_ai_recorded_action = -1;
  g_adventure_world_state = 0;
  g_max_x_value = -1;

  if ((((unsigned char)card_data->type & 0x3c) != 0 ||
       (int)card_data->id == 0x107 ||
       (int)card_data->id == 0x27) &&
      dispatch_event_to_single_card(player, card, 0x74, 1 - player, -1) == 0)
  {
    return 0;
  }

  g_spell_fizzled = -1;

  saved_current_casting_internal_card_id = g_current_casting_internal_card_id;
  saved_unk_008ce508 = g_current_spell_player;
  saved_unk_008ce4f4 = g_current_spell_card;

  g_current_casting_internal_card_id = internal_card_id;
  g_current_spell_player = player;
  g_current_spell_card = card;

  push_card_onto_stack(player, card, 0x71, player, 0);
  begin_mana_payment_record();

  if (g_adventure_world_state != 0)
  {
  }
  else
  {
    if ((player == g_active_player || (g_duel_network_flags & 2) != 0) && g_duel_ai_mode_state != 1)
    {
      if ((card_data->type & 0x40) != 0)
      {
        g_mana_charge[6] = (int)(char)card_data->cc[0];
        g_mana_charge[6] += (int)(char)card_data->cc[1];
      }
      else
      {
        g_mana_charge[color_index] = (int)(char)card_data->cc[0];
        g_mana_charge[0] += (int)(char)card_data->cc[1];
      }

      charge_mana_w_global_cost_mod(player, card, 0, 0);
    }
    else if ((card_data->type & 0x40) != 0)
    {
      charge_mana_w_global_cost_mod(player, card, 6, (int)(char)card_data->cc[1] + (int)(char)card_data->cc[0]);
    }
    else
    {
      if ((char)card_data->cc[0] != 0)
      {
        g_mana_charge[color_index] = (int)(char)card_data->cc[0];
      }
      if (0 < (char)card_data->cc[1])
      {
        g_mana_charge[0] = (int)(char)card_data->cc[1];
      }

      charge_mana_w_global_cost_mod(player, card, 0, 0);

      if ((char)card_data->cc[1] == -1)
      {
        if (player != g_active_player)
        {
          if (g_duel_ai_mode_state == 1)
          {
            switch (internal_rand(2))
            {
            case 0:
              tmp_mana = has_mana(player, COLOR_ANY, 1);
              if (tmp_mana != 0)
              {
                g_ai_recorded_choice = chosen_x_value = internal_rand(tmp_mana) + 1;
              }
              else
              {
                g_ai_recorded_choice = chosen_x_value = internal_rand(has_mana(player, COLOR_ANY, 1) + 1);
              }
              break;

            case 1:
              g_ai_recorded_choice = chosen_x_value = has_mana(player, COLOR_ANY, 1);
              if (g_life[0] < g_ai_recorded_choice && internal_rand(3) == 0)
              {
                g_ai_recorded_choice = chosen_x_value = g_life[0];
              }
              if (g_max_x_value != -1 && g_max_x_value < g_ai_recorded_choice)
              {
                g_ai_recorded_choice = chosen_x_value = g_max_x_value;
              }
              break;
            }

            record_ai_action_selection();
          }
          else
          {
            replay_ai_action_selection();
            chosen_x_value = g_ai_recorded_choice;
          }

          saved_max_x_value = g_max_x_value;
          g_max_x_value = chosen_x_value;
          charge_mana(player, 0, -1);
          g_max_x_value = saved_max_x_value;
        }
        else
        {
          saved_max_x_value = g_max_x_value;
          g_max_x_value = has_mana(player, COLOR_ANY, 1);
          charge_mana(player, 0, -1);
          g_max_x_value = saved_max_x_value;
        }

        if (g_x_value == 0)
        {
          g_ai_modifier -= 100;
        }
      }
    }
  }

  g_max_x_value = -1;

  if (internal_card_id != -1)
  {
    g_current_casting_player = player;
    g_duel_state_008b2880 = 1;
    --g_duel_summary.hand_counts[player];

    if ((card_data->type & 2) != 0)
    {
      ++g_duel_summary.creature_counts[player];
    }
    if ((card_data->type & 0x40) != 0)
    {
      ++g_duel_summary.artifact_counts[player];
    }
    if ((card_data->type & 4) != 0)
    {
      ++g_duel_summary.enchantment_counts[player];
    }

    g_card_types_in_play[player] |= card_data->type;
    PLAYER_CARD_INSTANCE(player, card).state |= STATE_INVISIBLE;
    g_land_can_be_played |= LCBP_SPELL_BEING_PLAYED;

    if (player == g_active_player || (g_duel_network_flags & 2) != 0 || g_duel_ai_mode_state != 1)
    {
      PLAYER_CARD_INSTANCE(player, card).state |= STATE_SUMMONSICK_BOTH;
    }
    else
    {
      PLAYER_CARD_INSTANCE(player, card).state |= STATE_SUMMONSICK_NOATTACK;
    }

    g_current_casting_internal_card_id = saved_current_casting_internal_card_id;
    g_current_spell_player = saved_unk_008ce508;
    g_current_spell_card = saved_unk_008ce4f4;

    if (g_spell_fizzled == 1)
    {
      end_mana_payment_record();
      goto finish_put_card_on_stack;
    }

    if (dispatch_event(player, card, 0x6c) != 0)
    {
      g_spell_fizzled = 1;
    }
    if (g_spell_fizzled == 1)
    {
      restore_recorded_mana_to_pool(player);
    }
    end_mana_payment_record();

    if (g_spell_fizzled == 1)
    {
      goto finish_put_card_on_stack;
    }

    if (player == g_other_player && (g_duel_network_flags & 2) == 0 && g_duel_ai_mode_state == 1 && card_data->cc[1] == -1)
    {
      g_ai_modifier -= count_colored_cards_in_hand(player) * 12;
    }
    return 1;

  respond_to_card_on_stack:
    saved_current_casting_internal_card_id = g_current_casting_internal_card_id;
    saved_unk_008ce508 = g_current_spell_player;
    saved_unk_008ce4f4 = g_current_spell_card;

    g_current_casting_internal_card_id = internal_card_id;
    g_current_spell_player = player;
    g_current_spell_card = card;

    if ((card_data->type & 1) == 0)
    {
      refresh_stack_proxy_card(1);
    }

    if ((card_data->type != 1) &&
        ((card_data->type != 0x20) || ((card_data->extra_ability & EA_MANA_SOURCE) == 0)))
    {
      if (g_duel_state_007a7d78 != 0)
      {
        g_duel_state_007a7d78 |= 2;
      }

      sprintf(g_ui_message_buffer, (const char *)gs_trying_to_cast_007a79c0, get_displayed_card_name(g_current_spell_player, g_current_spell_card));
      allow_response(-2, g_current_phase, g_ui_message_buffer, 0xd3);
      if ((g_duel_state_007a7d78 & 2) != 0)
      {
        request_duel_display_refresh_if_human(player, card, 2, 1);
        Sleep(3000);
      }
    }

    g_current_casting_internal_card_id = saved_current_casting_internal_card_id;
    g_current_spell_player = saved_unk_008ce508;
    g_current_spell_card = saved_unk_008ce4f4;

    if (PLAYER_CARD_INSTANCE(player, card).internal_card_id == -1)
    {
      g_spell_fizzled = 1;
    }

    g_land_can_be_played &= ~LCBP_SPELL_BEING_PLAYED;
    *(unsigned int *)&PLAYER_CARD_INSTANCE(player, card).state |= (((unsigned int)player < 1 ? 0 : 0x400000) | 0x80);

    if (g_duel_ai_mode_state != 1)
    {
      set_stack_damage_targets();
    }

    if (g_spell_fizzled == 1)
    {
      goto finish_put_card_on_stack;
    }

    if (player == g_other_player && g_duel_ai_mode_state != 1 && g_battlefield_ui_flags == 0 && (((unsigned char)card_data->type & 0x7e) != 0))
    {
      LoadTextSectionLines(global_ui_strings_filename, "PROMPT_CAST1");
      sprintf(prompt_text, g_text_lines[0], g_duel_message_text);
      if ((char)card_data->cc[1] == -1)
      {
        sprintf(prompt_text, g_text_lines[1], g_duel_message_text, g_x_value);
      }

      if (PLAYER_CARD_INSTANCE(player, card).number_of_targets != 0)
      {
        if (PLAYER_CARD_INSTANCE(player, card).number_of_targets == 1)
        {
          raw_do_dialog(player,
                        card,
                        PLAYER_CARD_INSTANCE(player, card).targets[0].player,
                        PLAYER_CARD_INSTANCE(player, card).targets[0].card,
                        prompt_text,
                        0);
        }
        else
        {
          raw_do_dialog(player, card, -1, -1, prompt_text, 0);
        }
      }
      else if (0)
      {
        // Unreachable in practice, but present in the original binary.
        raw_do_dialog(player, card, (g_ai_recorded_action >> 8), (g_ai_recorded_action & 0xff), prompt_text, 0);
      }
      else
      {
        raw_do_dialog(player, card, -1, -1, prompt_text, 0);
      }
    }

    if (g_duel_ai_mode_state != 1)
    {
      request_duel_display_refresh_if_human(player, card, 2, 1);
    }
  }
finish_put_card_on_stack:
  if (g_spell_fizzled == 1)
  {
    ++g_duel_summary.hand_counts[player];

    if ((card_data->type & 2) != 0)
    {
      --g_duel_summary.creature_counts[player];
    }
    if ((card_data->type & 0x40) != 0)
    {
      --g_duel_summary.artifact_counts[player];
    }
    if ((card_data->type & 4) != 0)
    {
      --g_duel_summary.enchantment_counts[player];
    }

    *(unsigned int *)&PLAYER_CARD_INSTANCE(player, card).state &= 0xffffff5d;
    *(unsigned int *)&PLAYER_CARD_INSTANCE(player, card).state &= 0xfffcffff;

    if (player == g_other_player && (g_duel_network_flags & 2) == 0)
    {
      g_recorded_action_cursor = 1;
    }
    g_spell_fizzled = 0;
    obliterate_top_card_of_stack();
    g_land_can_be_played &= ~LCBP_SPELL_BEING_PLAYED;
    return 0;
  }

  TENTATIVE_set_timestamps(player, card);
  return 1;
}

// FUNCTION: MAGIC 0x0043f9ed
// FUNCTION: SHANDALAR 0x0040b26d
int resolve_card_on_stack(int player, int card);

// FUNCTION: MAGIC 0x0043ff32
// FUNCTION: SHANDALAR 0x0040b7b2
int activate(int who_activates, int player, int card)
{
  int upkeep_uses_mana_source;
  int untap_color;
  int upkeep_total;
  char upkeep_prompt[600];
  int has_upkeep_costs;
  int i;
  int upkeep_dialog_ai_choice;
  char upkeep_opt0[300];
  char upkeep_opt1[300];
  card_instance_t *instance;
  char prompt[300];
  int result;
  int who_pays; /* also who chooses in do_dialog */
  int was_tapped;
  char trace[500];

  result = 1;

  instance = &PLAYER_CARD_INSTANCE(player, card);
  who_pays = player;

  if ((g_duel_network_flags & 2) != 0)
  {
    int trace_counter;

    trace_counter = g_duel_trace_counter;
    ++g_duel_trace_counter;
    sprintf(trace,
            "%d: Player #%d is tapping %s(%d).\n",
            trace_counter,
            player,
            global_cards_data[PLAYER_CARD_INSTANCE(player, card).internal_card_id].name,
            card);
    append_to_trace_txt(trace);
  }
  if (g_current_phase == EVENT_UPKEEP_PHASE)
  {
    who_pays = g_event_player;
  }
  if (who_pays == g_other_player && (g_duel_network_flags & 2) == 0 && g_duel_ai_mode_state != 1)
  {
    g_ai_recorded_action = -1;
    dispatch_event_to_single_card(player, card, EVENT_GET_SELECTED_CARD, 1 - player, -1);

    LoadTextSectionLines(global_ui_strings_filename, "PROMPT_TAP1");
    sprintf(prompt, g_text_lines[0], g_duel_message_text);

    if (card_activation_uses_x(player, card) != 0)
    {
      sprintf(prompt, g_text_lines[1], g_duel_message_text, g_loaded_recorded_action_code);
    }
    else if (((global_cards_data[instance->internal_card_id].extra_ability & (EA_INF_POWER | EA_INF_TOUGHNESS)) != 0) &&
             g_current_player == player && g_loaded_recorded_action_code != 0)
    {
      sprintf(prompt, g_text_lines[2], g_duel_message_text, g_loaded_recorded_action_code);
    }

    if (instance->number_of_targets != 0)
    {
      if (instance->number_of_targets == 1)
      {
        raw_do_dialog(player, card, instance->targets[0].player, instance->targets[0].card, prompt, 0);
      }
      else
      {
        raw_do_dialog(player, card, -1, -1, prompt, 0);
      }
    }
    else
    {
      if (g_ai_recorded_action != -1)
      {
        /* Unreachable in practice, but present in the original binary. */
        raw_do_dialog(player, card, (g_ai_recorded_action >> 8), (g_ai_recorded_action & 0xff), prompt, 0);
      }
      else
      {
        raw_do_dialog(player, card, -1, -1, prompt, 0);
      }
    }
  }

  push_card_onto_stack(player, card, EVENT_RESOLVE_ACTIVATION, player, 0);
  g_upkeep_payment_completed = 0;

  if ((instance->upkeep_flags & UPKEEP_UPKEEP_TRIGGER) != 0)
  {
    has_upkeep_costs = 1;
    upkeep_total = 0;

    for (i = 0; i < 7; i++)
    {
      g_mana_charge[i] = ((char *)&instance->upkeep_colorless)[i];

      if (i > 0)
      {
        if (has_mana(player, (color_t)i, g_mana_charge[i]) == 0)
        {
          has_upkeep_costs = 0;
        }
      }

      upkeep_total += g_mana_charge[i];
    }

    if (has_mana(who_pays, COLOR_ANY, upkeep_total) == 0)
    {
      has_upkeep_costs = 0;
    }

    if (has_upkeep_costs != 0)
      upkeep_dialog_ai_choice = 0;
    else
      upkeep_dialog_ai_choice = 1;

    load_text(global_ui_strings_filename, "PROMPT_PAYUPKEEP");
    strcpy(upkeep_opt0, g_text_lines[0]);
    strcpy(upkeep_opt1, g_text_lines[1]);
    sprintf(upkeep_prompt, " %s\n %s", upkeep_opt0, upkeep_opt1);

    if ((instance->upkeep_flags & UPKEEP_UPKEEP_NODIALOG) != 0 ||
        do_dialog(who_pays, player, card, -1, -1, upkeep_prompt, upkeep_dialog_ai_choice) == 0)
    {
      if (has_upkeep_costs == 0)
      {
        goto upkeep_cleanup;
      }

      charge_mana(who_pays, 0, 0);
      if (g_spell_fizzled == 0)
      {
        dispatch_event_to_single_card(player, card, EVENT_UPKEEP_PHASE, 1 - player, -1);
        if (g_last_single_card_event_result == 0)
        {
          instance->upkeep_flags |= UPKEEP_UPKEEP_UNPAID;
        }
      }
    }

    if (g_spell_fizzled != 0)
    {
      g_spell_fizzled = 0;
      obliterate_top_card_of_stack();
      return 0;
    }

  upkeep_cleanup:
    for (i = 0; i < 7; i++)
    {
      g_mana_charge[i] = 0;
    }

    instance->upkeep_flags &= ~UPKEEP_UPKEEP_TRIGGER;
    instance->upkeep_flags |= UPKEEP_UPKEEP_CANTPAY;

    refresh_stack_proxy_card(1);
    return 1;
  }

  if ((instance->upkeep_flags & UPKEEP_UNTAP_TRIGGER) != 0 && can_pay_untap_cost(player, card) != 0)
  {
    for (untap_color = 0; untap_color < 7; ++untap_color)
    {
      g_mana_charge[untap_color] = (int)(char)instance->mana_to_untap[untap_color];
    }

    charge_mana(player, 0, 0);
    if (g_spell_fizzled == 0)
    {
      dispatch_event_to_single_card(player, card, EVENT_UNTAP_PHASE, 1 - player, -1);
      if (g_last_single_card_event_result == 0)
      {
        instance->upkeep_flags |= UPKEEP_UNTAP_PAID;
      }
    }

    if (g_spell_fizzled != 0)
    {
      g_spell_fizzled = 0;
      obliterate_top_card_of_stack();
      return 0;
    }

    instance->upkeep_flags &= ~UPKEEP_UNTAP_TRIGGER;
    instance->upkeep_flags |= UPKEEP_UNTAP_DONE;
    refresh_stack_proxy_card(1);
    return 1;
  }

  if (dispatch_event(player, card, EVENT_UNKNOWN80) != 0)
  {
    obliterate_top_card_of_stack();
    result = 0;
  }
  else
  {
    if ((global_cards_data[instance->internal_card_id].extra_ability & EA_MANA_SOURCE) != 0)
    {
      g_produced_mana_color = -1;
    }

    begin_mana_payment_record();

    was_tapped = instance->state & STATE_TAPPED;
    dispatch_event_to_single_card(player, card, EVENT_ACTIVATE, 1 - player, -1);

    if ((g_duel_network_flags & 2) != 0 && g_other_player == who_activates && g_battlefield_ui_flags == 0 &&
        instance->internal_card_id != g_draw_card_placeholder_internal_card_id)
    {
      LoadTextSectionLines(global_ui_strings_filename, "PROMPT_TAP1");
      sprintf(prompt, g_text_lines[0], g_duel_message_text);

      if (card_activation_uses_x(player, card) != 0)
      {
        sprintf(prompt, g_text_lines[1], g_duel_message_text, g_x_value);
      }
      else if (((global_cards_data[instance->internal_card_id].extra_ability & (EA_INF_POWER | EA_INF_TOUGHNESS)) != 0) &&
               g_current_player == player && g_x_value != 0)
      {
        sprintf(prompt, g_text_lines[2], g_duel_message_text, g_x_value);
      }

      if (instance->number_of_targets == 1)
      {
        raw_do_dialog(player, card, instance->targets[0].player, instance->targets[0].card, prompt, 0);
      }
      else
      {
        raw_do_dialog(player, card, -1, -1, prompt, 0);
      }
    }

    if (g_spell_fizzled == 1)
    {
      restore_recorded_mana_to_pool(player);
      obliterate_top_card_of_stack();
      result = 0;
    }

    end_mana_payment_record();

    if (result != 0)
    {
      if (was_tapped == 0 && (instance->state & STATE_TAPPED) != 0)
      {
        dispatch_event(player, card, EVENT_TAP_CARD);
      }
      else
      {
        dispatch_event(player, card, EVENT_PLAY_ABILITY);
      }

      if (instance->internal_card_id != -1)
      {
        upkeep_uses_mana_source = global_cards_data[instance->internal_card_id].extra_ability & EA_MANA_SOURCE;
      }
      else
      {
        upkeep_uses_mana_source = global_cards_data[instance->original_internal_card_id].extra_ability & EA_MANA_SOURCE;
      }

      if (upkeep_uses_mana_source != 0 && g_produced_mana_color != -1)
      {
        refresh_stack_proxy_card(0);
      }
      else
      {
        refresh_stack_proxy_card(1);
      }

      if (g_duel_ai_mode_state != 1)
      {
        if ((global_cards_data[instance->internal_card_id].extra_ability & EA_MANA_SOURCE) != 0 &&
            g_produced_mana_color != -1)
        {
          play_sound_effect(0x12);
        }
        else
        {
          play_sound_effect(0x26);
        }
      }

      if (g_duel_ai_mode_state != 1)
      {
        set_stack_damage_targets();
      }
    }
  }

  if (result == 0)
  {
    g_upkeep_payment_completed = 0;
  }

  return result;
}

// FUNCTION: MAGIC 0x0044096f
// FUNCTION: SHANDALAR 0x0040c1ef
int resolve_activated_ability(int player, int card);

// FUNCTION: MAGIC 0x00440ad2
// FUNCTION: SHANDALAR 0x0040c352
void resolve_card_immediately(int player, int card, int arg_3);

// FUNCTION: MAGIC 0x00440c61
// FUNCTION: SHANDALAR 0x0040c4e1
int is_card_pending_resolution(int player, int card)
{
  if (PLAYER_CARD_INSTANCE(player, card).internal_card_id == -1)
  {
    return 0;
  }

  return ((PLAYER_CARD_INSTANCE(player, card).state & (STATE_IN_PLAY | STATE_INVISIBLE)) != 0) ? 0 : 1;
}

// FUNCTION: MAGIC 0x0044125c
// FUNCTION: SHANDALAR 0x0040cadc
int can_attack(int player, int card);

// FUNCTION: MAGIC 0x00441514
// FUNCTION: SHANDALAR 0x0040cd94
int has_vigilance(int player, int card)
{
  if (PLAYER_CARD_INSTANCE(player, card).state & STATE_VIGILANCE)
    return 1;

  return 0;
}

// FUNCTION: MAGIC 0x004418c6
// FUNCTION: SHANDALAR 0x0040d146
int can_block_attacker_with_abilities(int blocker_player,
                                      int blocker_card,
                                      int attacker_player,
                                      int attacker_card,
                                      unsigned int attacker_abilities,
                                      unsigned int land_bits);

// FUNCTION: MAGIC 0x00441d78
// FUNCTION: SHANDALAR 0x0040d614
void reassess_all_cards_and_mana(void);

// FUNCTION: MAGIC 0x00441f16
// FUNCTION: SHANDALAR 0x0040d7b2
int C_get_abilities(int player, int card, event_t event, int new_attacking_card);

// FUNCTION: MAGIC 0x004427d9
// FUNCTION: SHANDALAR 0x0040e073
int count_colored_cards_in_hand(int player)
{
  int count;
  int card;
  int internal_card_id;

  count = 0;
  for (card = 0; card < g_active_cards_count[player]; ++card)
  {
    internal_card_id = PLAYER_CARD_INSTANCE(player, card).internal_card_id;
    if (internal_card_id != -1)
    {
      if ((PLAYER_CARD_INSTANCE(player, card).state & STATE_IN_PLAY) == 0)
      {
        if ((global_cards_data[internal_card_id].type & 1) != 0)
        {
          if ((char)global_cards_data[internal_card_id].color != 0)
          {
            ++count;
          }
        }
      }
    }
  }

  return count;
}

// FUNCTION: MAGIC 0x004428be
// FUNCTION: SHANDALAR 0x0040e158
int dispatch_event(int player, int card, event_t event)
{
  int result;                   // ebp-0x8
  trigger_t saved_trigger_cond; // ebp-0x4

  push_affected_card_stack();

  g_event_result = 0;
  g_affected_card_controller = player;
  g_affected_card = card;
  g_attacking_card_controller = 1 - player;
  g_attacking_card = -1;

  saved_trigger_cond = g_trigger_condition;
  if (event != EVENT_TRIGGER && event != EVENT_RESOLVE_TRIGGER)
  {
    g_trigger_condition = -1;
  }

  C_dispatch_event_raw(event);

  g_affected_card_controller = -1;
  g_trigger_condition = saved_trigger_cond;

  result = g_event_result;
  pop_affected_card_stack();
  return result;
}

// FUNCTION: MAGIC 0x0044295b
// FUNCTION: SHANDALAR 0x0040e1f5
void C_dispatch_event_raw(event_t event);

// FUNCTION: MAGIC 0x00442b42
// FUNCTION: SHANDALAR 0x0040e3dc
int dispatch_event_to_single_card(int player,
                                  int card,
                                  event_t event,
                                  int new_attacking_card_controller,
                                  int new_attacking_card);

// FUNCTION: MAGIC 0x00442dac
// FUNCTION: SHANDALAR 0x0040e646
void push_affected_card_stack(void)
{
  if (g_affected_card_context_depth >= 0x20)
    return;

  g_affected_card_context_stack[g_affected_card_context_depth][0] = g_affected_card_controller;
  g_affected_card_context_stack[g_affected_card_context_depth][1] = g_affected_card;
  g_affected_card_context_stack[g_affected_card_context_depth][2] = g_affected_internal_card_id;
  g_affected_card_context_stack[g_affected_card_context_depth][3] = g_affected_card_color;
  g_affected_card_context_stack[g_affected_card_context_depth][4] = g_attacking_card_controller;
  g_affected_card_context_stack[g_affected_card_context_depth][5] = g_attacking_card;
  g_affected_card_context_stack[g_affected_card_context_depth][6] = g_event_result;
  g_affected_card_context_depth += 1;
}

// FUNCTION: MAGIC 0x00442e62
// FUNCTION: SHANDALAR 0x0040e6fc
void pop_affected_card_stack(void)
{
  if (g_affected_card_context_depth > 0)
  {
    g_affected_card_context_depth -= 1;
  }

  g_affected_card_controller = g_affected_card_context_stack[g_affected_card_context_depth][0];
  g_affected_card = g_affected_card_context_stack[g_affected_card_context_depth][1];
  g_affected_internal_card_id = g_affected_card_context_stack[g_affected_card_context_depth][2];
  g_affected_card_color = g_affected_card_context_stack[g_affected_card_context_depth][3];
  g_attacking_card_controller = g_affected_card_context_stack[g_affected_card_context_depth][4];
  g_attacking_card = g_affected_card_context_stack[g_affected_card_context_depth][5];
  g_event_result = g_affected_card_context_stack[g_affected_card_context_depth][6];
}

// FUNCTION: MAGIC 0x00443898
// FUNCTION: SHANDALAR 0x0040f132
unsigned int get_current_stack_action(void)
{
  if (g_stack_size == 0)
  {
    return 0xffffffffU;
  }

  return (unsigned int)g_duel_state_table_0093917c[g_stack_size];
}

// FUNCTION: MAGIC 0x004438cb
// FUNCTION: SHANDALAR 0x0040f165
void refresh_stack_proxy_card(int prompt_flag);

// FUNCTION: MAGIC 0x00443ee2
// FUNCTION: SHANDALAR 0x0040f77c
void push_card_onto_stack(int player, int card, int event, int extra, int prompt);

// FUNCTION: MAGIC 0x00444443
// FUNCTION: SHANDALAR 0x0040fcdd
int set_stack_damage_targets(void)
{
  int card;
  int stack_index;
  int player;

  for (stack_index = 0; stack_index < g_stack_size; ++stack_index)
  {
    player = global_stack_cards[stack_index].player;
    card = global_stack_cards[stack_index].card;
    global_stack_damage_targets[stack_index].player =
        (int)(char)PLAYER_CARD_INSTANCE(player, card).damage_target_player;
    global_stack_damage_targets[stack_index].card =
        PLAYER_CARD_INSTANCE(player, card).damage_target_card;
  }
  return 0;
}

// FUNCTION: MAGIC 0x00444505
// FUNCTION: SHANDALAR 0x0040fd9f
int resolve_top_card_on_stack(void)
{
  int player;           /* [ebp-0xc] */
  int current_internal; /* [ebp-0x8] */
  int card;             /* [ebp-0x4] */

  if (g_stack_size <= 0)
  {
    return 0;
  }

  --g_stack_size;
  player = global_stack_cards[g_stack_size].player;
  card = global_stack_cards[g_stack_size].card;
  current_internal = PLAYER_CARD_INSTANCE(player, card).internal_card_id;
  if (current_internal == g_stack_proxy_internal_card_id)
  {
    current_internal = PLAYER_CARD_INSTANCE(player, card).original_internal_card_id;
  }
  if (PLAYER_CARD_INSTANCE(player, card).internal_card_id != -1)
  {
    if ((((unsigned int)g_stack_data[g_stack_size] >> 0x10) & 0xff) == 0x7e)
    {
      dispatch_card_trigger_event(player,
                                  card,
                                  ((unsigned int)g_stack_data[g_stack_size] >> 0x10) & 0xff,
                                  ((int)g_stack_data[g_stack_size]) >> 0x18);
    }
    else
    {
      if ((PLAYER_CARD_INSTANCE(player, card).upkeep_flags & 8) != 0)
      {
        if ((PLAYER_CARD_INSTANCE(player, card).upkeep_flags & 0x200) == 0)
        {
          dispatch_event_to_single_card(player, card, EVENT_UPKEEP_COSTS_UNPAID, 1 - player, -1);
          if (PLAYER_CARD_INSTANCE(player, card).internal_card_id != -1 &&
              (PLAYER_CARD_INSTANCE(player, card).upkeep_flags & 2) != 0)
          {
            kill_card(PLAYER_CARD_INSTANCE(player, card).parent_controller,
                      PLAYER_CARD_INSTANCE(player, card).parent_card,
                      KILL_BURY);
          }
        }
        PLAYER_CARD_INSTANCE(PLAYER_CARD_INSTANCE(player, card).parent_controller,
                             PLAYER_CARD_INSTANCE(player, card).parent_card)
            .upkeep_flags &= ~0x208;
        PLAYER_CARD_INSTANCE(PLAYER_CARD_INSTANCE(player, card).parent_controller,
                             PLAYER_CARD_INSTANCE(player, card).parent_card)
            .upkeep_flags |= 4;
      }
      else
      {
        if ((PLAYER_CARD_INSTANCE(player, card).upkeep_flags & 0x80) != 0)
        {
          if ((PLAYER_CARD_INSTANCE(player, card).upkeep_flags & 0x40) != 0)
          {
            /* MOK 0x436854: honor effects that observe every card untapping. */
            manalinkeh_untap_card(PLAYER_CARD_INSTANCE(player, card).parent_controller,
                                PLAYER_CARD_INSTANCE(player, card).parent_card);
          }
          PLAYER_CARD_INSTANCE(PLAYER_CARD_INSTANCE(player, card).parent_controller,
                               PLAYER_CARD_INSTANCE(player, card).parent_card)
              .upkeep_flags &= ~0x80;
        }
        else
        {
          dispatch_event_to_single_card(player,
                                        card,
                                        ((unsigned int)g_stack_data[g_stack_size] >> 0x10) & 0xff,
                                        1 - player,
                                        -1);
        }
      }
    }
    if (PLAYER_CARD_INSTANCE(player, card).internal_card_id == g_stack_proxy_internal_card_id)
    {
      kill_card(player, card, KILL_REMOVE);
    }
  }
  global_stack_cards[g_stack_size].player = -1;
  reassess_all_cards_and_mana();
  if (g_nested_trigger_depth <= 1 && ((g_land_can_be_played & LCBP_REGENERATION) == 0 || g_stack_size == 0))
  {
    process_damage_prevention(g_current_player);
    regenerate_or_graveyard_triggers();
  }

  return 0;
}

// FUNCTION: MAGIC 0x00444b74
// FUNCTION: SHANDALAR 0x0041040d
int obliterate_top_card_of_stack(void)
{
  int card;
  int current_internal;
  int player;

  if (g_stack_size <= 0)
  {
    return 0;
  }

  --g_stack_size;
  player = global_stack_cards[g_stack_size].player;
  card = global_stack_cards[g_stack_size].card;
  current_internal = PLAYER_CARD_INSTANCE(player, card).internal_card_id;
  if (g_stack_proxy_internal_card_id == current_internal)
  {
    PLAYER_CARD_INSTANCE(player, card).internal_card_id = -1;
  }
  global_stack_cards[g_stack_size].player = -1;

  return 0;
}

// FUNCTION: MAGIC 0x00444c43
// FUNCTION: SHANDALAR 0x004104db
char *format_stack_action_text(char *out, int msg, int player, int card)
{
  char tmp[300];
  char *prefix;

  if (out == (char *)0)
  {
    return (char *)0;
  }

  if (msg == 0x71)
  {
    prefix = gs_casting_008b4850;
  }
  else if (msg == 0x72)
  {
    prefix = gs_activating_007912e0;
  }
  else if (msg == 0x7e)
  {
    prefix = gs_processing_008ce290;
  }
  else
  {
    prefix = "";
  }

  if (player >= 0 && card >= 0)
  {
    sprintf(tmp, "%s: %s", prefix, get_displayed_card_name(player, card));
  }
  else
  {
    strcpy(tmp, prefix);
  }

  strcpy(out, tmp);
  return out;
}
// FUNCTION: MAGIC 0x00444d1f
// FUNCTION: SHANDALAR 0x004105b7
void reset_trigger_dispatch_state(void)
{
  g_trigger_dispatch_depth = g_max_trigger_dispatch_depth = 0;
  g_regenerate_or_graveyard_triggers_in_progress = 0;
}

// FUNCTION: MAGIC 0x00444d48
// FUNCTION: SHANDALAR 0x004105e0
int allow_response(int response_player, int phase, char *prompt, int event_code)
{
  int saved_allow_response_flag = g_phase_stop_suppressed;

  if (g_stack_size == 0)
  {
    if (should_skip_phase(g_current_player) != 0 && response_player != -2 && g_duel_ai_mode_state != 1)
    {
      g_phase_stop_suppressed = 1;
    }
  }

  {
    int result;

    do
    {
      g_response_action_taken = 0;
      result = allow_response_once(response_player, phase, prompt, event_code);
    } while (g_response_action_taken != 0 && g_stack_size <= 0);

    if (g_stack_size == 0)
    {
      if (g_duel_ai_mode_state != 1)
      {
        g_phase_stop_suppressed = 0;
      }
      {
        g_duel_phase_stop_settings[g_current_player].phase_flags[g_current_phase] = (int)(char)g_duel_phase_stop_settings[g_current_player].phase_flags[g_current_phase] & ~PHASE_STOP_SUPPRESSED;
      }
    }
    else if (g_duel_ai_mode_state != 1)
    {
      g_phase_stop_suppressed = saved_allow_response_flag;
    }

    return result;
  }
}

// FUNCTION: MAGIC 0x00444e5f
// FUNCTION: SHANDALAR 0x004106f7
int allow_response_once(int response_player, int phase, char *prompt, int event_code)
{
  /* Stack layout is sensitive here; keep locals in one struct to match /Od codegen. */
  int saved_DAT_007aadec;          /* ebp-0xa4 */
  int saved_unk_008b3270;          /* ebp-0xa0 */
  unsigned int dispatch_result;    /* ebp-0x9c */
  int result;                      /* ebp-0x98 */
  int saved_DAT_00777aa8_in_loop;  /* ebp-0x94 */
  int saved_DAT_00777aa8;          /* ebp-0x90 */
  int saved_phase_stop_suppressed; /* ebp-0x8c */
  char prompt_copy[128];           /* ebp-0x88 */
  int saved_trigger_condition;     /* ebp-0x8 */
  int saved_DAT_00561390;          /* ebp-0x4 */

  saved_trigger_condition = g_trigger_condition;
  saved_DAT_007aadec = g_current_action_event_code;
  g_trigger_condition = -1;
  saved_unk_008b3270 = g_activation_event_flags;

  ++g_trigger_dispatch_depth;
  if (g_trigger_dispatch_depth == 1)
  {
    g_max_trigger_dispatch_depth = 0;
  }
  else if (g_max_trigger_dispatch_depth < g_trigger_dispatch_depth && phase != 0x8e && phase != 0x70 && phase != 0xd3)
  {
    g_max_trigger_dispatch_depth = g_trigger_dispatch_depth;
  }

  result = 0;
  g_current_action_event_code = event_code;
  saved_DAT_00777aa8 = g_ai_decision_parameter;
  saved_DAT_00561390 = g_affected_card_context_flags;

  if (response_player == -2 && should_skip_phase(g_current_player) == 0)
  {
    g_affected_card_context_flags = 1;
  }
  else
  {
    g_affected_card_context_flags = 0;
  }

  if ((g_duel_network_flags & 2) == 0)
  {
    if (should_skip_phase(g_current_player) == 0 &&
        ((((int)(char)g_duel_phase_stop_settings[g_current_player].phase_flags[phase] & PHASE_STOP_ENABLED) != 0) ||
         (g_current_player == g_stop_phase_player && g_stop_phase == phase)))
    {
      g_ai_decision_parameter = 1;
    }
    else
    {
      g_ai_decision_parameter = 0;
    }
  }

  if (response_player > -1)
  {
    if (g_ai_decision_parameter == 0)
    {
      goto cleanup;
    }
  }

  strcpy(prompt_copy, prompt);

  if (g_current_phase == PHASE_UPKEEP && g_trigger_dispatch_depth == 1)
  {
    g_event_player = g_current_player;
    reset_upkeep_costs();
  }

  while (1)
  {
  restart_response_window:
    do
    {
      g_ai_decision_parameter = can_stop_for_phase(1);

      if ((g_duel_network_flags & 2) != 0)
      {
        g_response_processing_flags = 1;
      }
      else
      {
        if (g_current_player != 0)
        {
          if (response_player > -1)
          {
            g_response_processing_flags = 0;
          }
          else
          {
            g_response_processing_flags = 2;
          }
        }
        else
        {
          g_response_processing_flags = 1;
        }
      }

      if (g_trigger_dispatch_depth <= 1)
      {
        g_response_action_mask = 0xffffffff;
      }

      g_spell_fizzled = 0;
      g_activation_event_flags = g_spell_fizzled;

      dispatch_result = (unsigned int)process_response_actions(g_current_player, prompt_copy);
      if (dispatch_result != 0)
      {
        g_response_action_taken = 1;
      }

      if (((g_active_player == g_current_player) ||
           (g_other_player == g_current_player && (g_duel_network_flags & 2) != 0)) &&
          dispatch_result != 0 && g_duel_ai_mode_state != 1)
      {
        result = 1;
      }

      if (g_trigger_dispatch_depth < g_max_trigger_dispatch_depth && g_stack_size >= 0)
      {
        dispatch_result = 0;
      }

    } while (dispatch_result != 0 || ((g_activation_event_flags & 1) != 0 && g_trigger_dispatch_depth == 1));

    if (g_current_phase == PHASE_UPKEEP && g_trigger_dispatch_depth == 1)
    {
      g_event_player = 1 - g_current_player;
      reset_upkeep_costs();
    }

    while (1)
    {
      saved_DAT_00777aa8_in_loop = g_ai_decision_parameter;
      saved_phase_stop_suppressed = g_phase_stop_suppressed;

      if ((g_duel_network_flags & 2) != 0)
      {
        g_response_processing_flags = 1;
        g_phase_stop_suppressed = 0;

        if (g_stack_size == 0 && should_skip_phase(1 - g_current_player) != 0 && response_player != -2)
        {
          g_phase_stop_suppressed = 1;
        }

        g_ai_decision_parameter = can_stop_for_phase(0);
      }
      else
      {
        if (g_current_player != 0)
        {
          g_response_processing_flags = 1;
        }
        else
        {
          if (response_player > -1)
          {
            g_response_processing_flags = 0;
          }
          else
          {
            g_response_processing_flags = 2;
          }
        }
      }

      if (g_trigger_dispatch_depth <= 1)
      {
        g_response_action_mask = 0xffffffff;
      }

      g_spell_fizzled = 0;
      g_activation_event_flags = g_spell_fizzled;

      if ((g_trigger_dispatch_depth < g_max_trigger_dispatch_depth && g_stack_size >= 0))
      {
        goto cleanup;
      }

      dispatch_result = (unsigned int)process_response_actions(1 - g_current_player, prompt_copy);
      g_ai_decision_parameter = saved_DAT_00777aa8_in_loop;
      g_phase_stop_suppressed = saved_phase_stop_suppressed;
      if (dispatch_result != 0)
      {
        g_response_action_taken = 1;

        if (g_duel_ai_mode_state != 1)
        {
          goto restart_response_window;
        }

        if (g_active_player == g_current_player)
        {
          goto restart_response_window;
        }
        if ((g_duel_network_flags & 2) != 0)
        {
          goto restart_response_window;
        }
        if ((g_activation_event_flags & 1) != 0)
        {
          if (g_trigger_dispatch_depth == 1)
          {
            goto restart_response_window;
          }
        }
        goto cleanup;
      }
      else
      {
        goto cleanup;
      }
    }
  }

cleanup:
  --g_trigger_dispatch_depth;
  if (g_trigger_dispatch_depth != 0)
  {
    g_response_action_mask = 0xffffffff;
  }
  else
  {
    g_response_card_type_mask = 0xffffffff;
    g_response_action_mask = g_response_card_type_mask;
    if (g_nested_trigger_depth == 0)
    {
      g_response_window_serial = 0;
      if (g_duel_ai_mode_state != 1)
      {
        g_phase_stop_suppressed = 0;
      }
    }
  }

  g_ai_decision_parameter = saved_DAT_00777aa8;
  g_affected_card_context_flags = saved_DAT_00561390;
  g_activation_event_flags = saved_unk_008b3270;
  g_trigger_condition = saved_trigger_condition;
  g_current_action_event_code = saved_DAT_007aadec;

  if (result != 0)
  {
    g_ai_action_replay_available = 0;
  }

  return result;
}

// FUNCTION: MAGIC 0x0044541f
// FUNCTION: SHANDALAR 0x00410cb8
int can_stop_for_phase(int use_current_player_stops)
{
  int can_respond;

  can_respond = 0;
  if (use_current_player_stops != 0)
  {
    if (g_current_player == g_active_player &&
        (((int)(char)g_duel_phase_stop_settings[g_current_player].phase_flags[g_current_phase]) & PHASE_STOP_ENABLED) != 0)
    {
      can_respond = 1;
    }

    if (g_current_player == g_other_player &&
        (((int)(char)g_duel_phase_stop_settings[g_current_player].phase_flags[g_current_phase]) & PHASE_STOP_OPPONENT) != 0)
    {
      can_respond = 1;
    }

    if (g_current_player == g_active_player)
    {
      if (g_current_player == g_stop_phase_player && g_current_phase == g_stop_phase)
      {
        can_respond = 1;
      }
    }
    else if (g_current_player == g_previous_stop_phase_player && g_previous_stop_phase == g_current_phase)
    {
      can_respond = 1;
    }
    if (should_skip_phase(g_current_player) == 0 && can_respond != 0)
    {
      return 1;
    }
    return 0;
  }

  /* use_current_player_stops == 0 */
  if (g_current_player == g_active_player &&
      (((int)(char)g_duel_phase_stop_settings[g_current_player].phase_flags[g_current_phase]) & PHASE_STOP_OPPONENT) != 0)
  {
    can_respond = 1;
  }

  if (g_current_player == g_other_player &&
      (((int)(char)g_duel_phase_stop_settings[g_current_player].phase_flags[g_current_phase]) & PHASE_STOP_ENABLED) != 0)
  {
    can_respond = 1;
  }

  if (g_current_player == g_other_player)
  {
    if (g_current_player == g_stop_phase_player && g_current_phase == g_stop_phase)
    {
      can_respond = 1;
    }
  }
  else if (g_current_player == g_previous_stop_phase_player && g_previous_stop_phase == g_current_phase)
  {
    can_respond = 1;
  }

  if (should_skip_phase(1 - g_current_player) == 0 && can_respond != 0)
  {
    return 1;
  }
  return 0;
}

// FUNCTION: MAGIC 0x00445663
// FUNCTION: SHANDALAR 0x00410efc
int dispatch_trigger_twice_once_with_each_player_as_reason(int reason_for_trig, trigger_t trig, const char *prompt, int a4)
{
  dispatch_trigger(reason_for_trig, trig, prompt, a4);
  dispatch_trigger(1 - reason_for_trig, trig, prompt, a4);
  return 1;
}

// FUNCTION: MAGIC 0x004456ad
// FUNCTION: SHANDALAR 0x00410f46
int dispatch_trigger(int player, trigger_t trig, const char *prompt, int TENTATIVE_allow_response);

// FUNCTION: MAGIC 0x00445b56
// FUNCTION: SHANDALAR 0x004113ef
int can_pay_untap_cost(int player, int card)
{
  int total_paid;
  color_t current_color;

  if ((PLAYER_CARD_INSTANCE(player, card).state & STATE_TAPPED) == 0)
  {
    return 0;
  }

  total_paid = 0;
  current_color = COLOR_BLACK;
  for (; current_color < 7; ++current_color)
  {
    if (PLAYER_CARD_INSTANCE(player, card).mana_to_untap[current_color] != 0)
    {
      if (has_mana(player, current_color,
                   PLAYER_CARD_INSTANCE(player, card).mana_to_untap[current_color]) == 0)
      {
        return 0;
      }
      total_paid += PLAYER_CARD_INSTANCE(player, card).mana_to_untap[current_color];
    }
  }

  if (has_mana(player, COLOR_ANY, PLAYER_CARD_INSTANCE(player, card).mana_to_untap[0] + total_paid) == 0)
  {
    return 0;
  }

  dispatch_event_to_single_card(player, card, EVENT_CHECK_UNTAP_PAYMENT, 1 - player, -1);
  if (g_last_single_card_event_result != 0)
    return 0;

  return 1;
}

// FUNCTION: MAGIC 0x004460d3
// FUNCTION: SHANDALAR 0x0041196b
void reset_upkeep_costs(void)
{
  int player;
  int card;
  int i;

  for (player = 0; player < 2; ++player)
  {
    for (card = 0; card < g_active_cards_count[player]; ++card)
    {
      if ((PLAYER_CARD_INSTANCE(player, card).upkeep_flags & UPKEEP_UPKEEP_DONE) == 0)
      {
        if (is_in_play(player, card))
        {
          for (i = 0; i < 7; ++i)
          {
            PLAYER_CARD_INSTANCE(player, card).mana_to_untap[i] = 0;
            PLAYER_CARD_INSTANCE(player, card).upkeep_cost[i] = PLAYER_CARD_INSTANCE(player, card).mana_to_untap[i];
          }

          PLAYER_CARD_INSTANCE(player, card).upkeep_flags = 0;
          dispatch_event(player, card, 0x85);
          dispatch_event(player, card, 0x84);
        }
      }
    }
  }
}

// FUNCTION: MAGIC 0x00447f80
int get_duel_time_units(void)
{
  int ticks = GetTickCount();
  int diff = ticks - g_affected_card_context_flags_0093f4b4 - g_duel_tick_adjustment;
  int result = diff / 0x37;
  return result;
}

// FUNCTION: MAGIC 0x004482f3
// FUNCTION: SHANDALAR 0x00451e70
int copy_mana_pool_to_display(void)
{
  if (g_duel_ai_mode_state != 1)
  {
    EnterCriticalSection(&g_duel_render_lock);
    memcpy(g_duel_cached_raw_mana_player_0, g_raw_mana_available[0],
           sizeof(g_duel_cached_raw_mana_player_0));
    memcpy(g_duel_cached_raw_mana_player_1, g_raw_mana_available[1],
           sizeof(g_duel_cached_raw_mana_player_1));
    LeaveCriticalSection(&g_duel_render_lock);
    SendMessageA(g_duel_active_popup_window, 0x432, 0, 0);
    SendMessageA(g_duel_last_active_window, 0x432, 0, 0);
  }

  return 0;
}

// FUNCTION: MAGIC 0x0044aa01
// FUNCTION: SHANDALAR 0x00403381
int should_skip_phase(int player)
{
  if ((g_duel_network_flags & 2) != 0)
  {
    if (g_stop_phase == -1)
    {
      if (g_previous_stop_phase == -1)
      {
        return 0;
      }
    }

    if (g_duel_ai_mode_state == 1)
    {
      return 0;
    }

    return 0;

    /* Dead code (kept for matching): MSVC /Od still emits this after the return. */
    if (g_previous_stop_phase == g_current_phase && g_current_player == g_previous_stop_phase_player)
    {
      return 0;
    }

    if (g_stop_phase == g_current_phase && g_current_player == g_stop_phase_player)
    {
      return 0;
    }

    if (g_previous_stop_phase == -1 && player == g_other_player)
    {
      if (((char)g_duel_phase_stop_settings[g_current_player].phase_flags[g_current_phase] & PHASE_STOP_OPPONENT) != 0)
      {
        return 0;
      }
    }

    if (g_stop_phase == -1 && player == g_active_player)
    {
      if (((char)g_duel_phase_stop_settings[g_current_player].phase_flags[g_current_phase] & PHASE_STOP_ENABLED) != 0)
      {
        return 0;
      }
    }

    if (!(g_stop_phase == g_current_phase && g_current_player == g_stop_phase_player))
    {
      if (g_previous_stop_phase == -1)
      {
        return 1;
      }
    }

    if (!(g_previous_stop_phase == g_current_phase && g_current_player == g_previous_stop_phase_player))
    {
      if (g_stop_phase == -1)
      {
        return 1;
      }
    }

    if (g_trigger_condition != -1 && g_stop_phase == g_current_phase)
    {
      return 1;
    }

    if (g_trigger_condition != -1 && g_previous_stop_phase == g_current_phase)
    {
      return 1;
    }

    return 0;
  }

  if (g_stop_phase == -1)
  {
    return 0;
  }
  if (g_duel_ai_mode_state == 1)
  {
    return 0;
  }

  if (g_stop_phase > g_current_phase)
  {
    return 1;
  }
  if (g_current_player != g_stop_phase_player)
  {
    return 1;
  }

  if (g_trigger_condition != -1)
  {
    if (g_stop_phase == g_current_phase)
    {
      return 1;
    }
  }

  if (g_stop_phase < g_current_phase)
  {
    if (g_current_player == g_stop_phase_player)
    {
      g_stop_phase_player = -1;
      g_stop_phase = g_stop_phase_player;
    }
  }

  return 0;
}

// FUNCTION: MAGIC 0x0044ac96
// FUNCTION: SHANDALAR 0x00403615
int player_can_stop_at_phase(int player, phase_t phase)
{
  phase_t mapped_phase; /* ebp-0x8 */
  int result;           /* ebp-0x4 */

  result = 0;

  if (g_duel_ai_mode_state == 1)
  {
    return 0;
  }

  if (g_phase_stop_suppressed != 0)
  {
    return 0;
  }

  if (is_phase_stop_allowed_for_current_event() == 0)
  {
    return 0;
  }

  if (g_phase_response_window_open == 0)
  {
    return 0;
  }

  if (phase >= PHASE_BEGIN_UPKEEP && phase <= PHASE_UNKOWN5)
  {
    mapped_phase = PHASE_UPKEEP;
  }
  else if (phase >= PHASE_CLEANUP2 && phase <= PHASE_DAMAGE_PREVENTION)
  {
    mapped_phase = PHASE_CLEANUP2;
  }
  else
  {
    mapped_phase = phase;
  }

  if ((g_duel_network_flags & 2) != 0)
  {
    if (g_current_player == player)
    {
      if ((g_active_player == player &&
           (((int)(char)g_duel_phase_stop_settings[g_active_player].phase_flags[g_current_phase]) & PHASE_STOP_ENABLED) != 0) ||
          (g_other_player == player &&
           (((int)(char)g_duel_phase_stop_settings[g_other_player].phase_flags[g_current_phase]) & PHASE_STOP_OPPONENT) != 0))
      {
        result = 1;
      }
    }
    else
    {
      if ((g_other_player == player &&
           (((int)(char)g_duel_phase_stop_settings[g_active_player].phase_flags[g_current_phase]) & PHASE_STOP_OPPONENT) != 0) ||
          (g_active_player == player &&
           (((int)(char)g_duel_phase_stop_settings[g_other_player].phase_flags[g_current_phase]) & PHASE_STOP_ENABLED) != 0))
      {
        result = 1;
      }
    }
  }
  else
  {
    if (g_stop_phase == mapped_phase)
    {
      result = 1;
    }

    if (g_stop_phase == -1 && g_active_player == player &&
        (((int)(char)g_duel_phase_stop_settings[g_current_player].phase_flags[mapped_phase]) & PHASE_STOP_ENABLED) != 0)
    {
      result = 1;
    }
  }

  return result;
}

// FUNCTION: MAGIC 0x0044af2d
// FUNCTION: SHANDALAR 0x004038a8
int prompt_stop_phase_anyway(phase_t phase)
{
  if (g_duel_ai_mode_state != 1 &&
      (((int)(char)g_duel_phase_stop_settings[g_current_player].phase_flags[phase] & (PHASE_STOP_ENABLED | PHASE_STOP_OPPONENT)) != 0 ||
       (phase == g_stop_phase && g_current_player == g_stop_phase_player) ||
       (phase == g_previous_stop_phase && g_current_player == g_previous_stop_phase_player)))
  {
    load_text(global_ui_strings_filename, "PROMPT_STOPANYWAY");

    if (phase == 4)
    {
      strcpy(g_prompt_stop_phase_anyway_text, g_text_lines[1]);
    }
    else if (phase == 1)
    {
      strcpy(g_prompt_stop_phase_anyway_text, g_text_lines[2]);
    }
    else if (phase == 10)
    {
      strcpy(g_prompt_stop_phase_anyway_text, g_text_lines[3]);
    }
    else if (phase == 0x14)
    {
      strcpy(g_prompt_stop_phase_anyway_text, g_text_lines[4]);
    }
    else if (phase == 0x1f)
    {
      strcpy(g_prompt_stop_phase_anyway_text, g_text_lines[5]);
    }
    else if (phase == 0x22)
    {
      strcpy(g_prompt_stop_phase_anyway_text, g_text_lines[6]);
    }
    else if (phase == 0x19)
    {
      strcpy(g_prompt_stop_phase_anyway_text, g_text_lines[7]);
    }
    else if (phase == 0x1a)
    {
      strcpy(g_prompt_stop_phase_anyway_text, g_text_lines[8]);
    }
    else if (phase == 0x1b)
    {
      strcpy(g_prompt_stop_phase_anyway_text, g_text_lines[8]);
    }
    else
    {
      strcpy(g_prompt_stop_phase_anyway_text, g_text_lines[0]);
    }

    if ((g_duel_network_flags & 2) != 0)
    {
      if (g_active_player == g_current_player)
      {
        if (((int)(char)g_duel_phase_stop_settings[g_current_player].phase_flags[phase] & PHASE_STOP_ENABLED) != 0 ||
            (phase == g_stop_phase && g_current_player == g_stop_phase_player))
        {
          prompt_player_for_stop_action(g_active_player, g_prompt_stop_phase_anyway_text);
        }
        if (((int)(char)g_duel_phase_stop_settings[g_current_player].phase_flags[phase] & PHASE_STOP_OPPONENT) != 0 ||
            (phase == g_previous_stop_phase && g_current_player == g_previous_stop_phase_player))
        {
          prompt_player_for_stop_action(g_other_player, g_prompt_stop_phase_anyway_text);
        }
      }
      else
      {
        if (((int)(char)g_duel_phase_stop_settings[g_current_player].phase_flags[phase] & PHASE_STOP_OPPONENT) != 0 ||
            (phase == g_previous_stop_phase && g_current_player == g_previous_stop_phase_player))
        {
          prompt_player_for_stop_action(g_other_player, g_prompt_stop_phase_anyway_text);
        }
        if (((int)(char)g_duel_phase_stop_settings[g_current_player].phase_flags[phase] & PHASE_STOP_ENABLED) != 0 ||
            (phase == g_stop_phase && g_current_player == g_stop_phase_player))
        {
          prompt_player_for_stop_action(g_active_player, g_prompt_stop_phase_anyway_text);
        }
      }
    }
    else
    {
      prompt_player_for_stop_action(g_active_player, g_prompt_stop_phase_anyway_text);
    }
  }

  return -1;
}

// FUNCTION: MAGIC 0x0044b2b3
// FUNCTION: SHANDALAR 0x00403c34
void prompt_player_for_stop_action(int player, char *prompt)
{
  int selected_card;
  char prompt_copy[100];
  int done;

  strcpy(prompt_copy, prompt);
  done = 0;

  while (done == 0)
  {
    if (g_active_player == g_current_player)
    {
      g_stop_phase = g_stop_phase_player = -1;
    }
    else
    {
      g_previous_stop_phase = g_previous_stop_phase_player = -1;
    }

    selected_card = select_card_for_action(player, player, player, 0xff, 0, prompt_copy, 2);
    switch (g_target_selection_status_code)
    {
    case 0:
      if (selected_card == -1)
      {
      }
      else
      {
        if (can_activate_mana_source_for_stop_prompt(g_target_player_choice, selected_card) != 0)
        {
          activate_mana_source_card(g_target_player_choice, selected_card);
        }
        else
        {
        }
      }
      break;

    case -2:
      done = 1;
      break;

    case -3:
      break;
    }
  }
}

// FUNCTION: MAGIC 0x0044b646
// FUNCTION: SHANDALAR 0x00403fc7
int contains_player_card_pair(int *card_pairs, int card_pair_count, int player, int card)
{
  int card_pair_index;
  int result;

  result = 0;
  for (card_pair_index = 0; card_pair_index < card_pair_count; card_pair_index = card_pair_index + 1)
  {
    if ((card_pairs[card_pair_index * 2] == player) &&
        (card_pairs[card_pair_index * 2 + 1] == card))
    {
      result = 1;
    }
  }
  return result;
}

// FUNCTION: MOK 0x00459AA0
// FUNCTION: MAGIC 0x00485060
// FUNCTION: SHANDALAR 0x00488b70
int dispatch_function_to_all_cards_in_play(int player, int card, in_play_card_callback_t callback, int who_to_check)
{
  int saved_event_result;
  int callback_result;
  int test_player;
  int test_card;
  int result;

  saved_event_result = g_event_result;
  callback_result = 0;
  g_event_result = -1;
  for (test_player = player;
       test_player > -1 && test_player < 2 && callback_result == 0;
       test_player += (player == 0) ? 1 : -1)
  {
    if (who_to_check == -1 || test_player == who_to_check)
    {
      for (test_card = 0; test_card < g_active_cards_count[test_player] && callback_result == 0; ++test_card)
      {
        if (is_in_play(test_player, test_card))
        {
          callback_result = callback(player,
                                     card,
                                     test_player,
                                     test_card,
                                     global_card_instances[test_player][test_card].internal_card_id);
        }
      }
    }
  }

  result = g_event_result;
  g_event_result = saved_event_result;
  return result;
}

// FUNCTION: MAGIC 0x004ad7e0
// FUNCTION: SHANDALAR 0x0046a7c0
int process_response_actions(int reason_for_trigger_controller, const char *prompt)
{
  int selection_done;           // local_29c
  char prompt_format[300];      // local_29c
  unsigned int selected_card;   // local_170
  int saved_unk_00742f60;       // local_16c
  unsigned int maybe_card;      // local_168
  int some_flag;                // local_164
  int response_code;            // local_160
  unsigned int card;            // local_15c
  int saved_land_can_be_played; // local_158
  unsigned int response_mask;   // local_154
  int player;                   // local_150
  unsigned int result_flags;    // local_14c
  char prompt_copy[300];        // local_148
  int internal_card_id;         // local_1c
  int interrupts_available;     // local_18
  int selected_player;          // local_14
  int can_respond;              // local_10
  int maybe_player;             // local_c
  int num_candidates;           // local_8

  if (g_life[g_active_player] <= 0)
  {
    g_duel_phase_stop_settings[g_current_player].phase_flags[g_current_phase] =
        (unsigned char)(((int)(char)g_duel_phase_stop_settings[g_current_player].phase_flags[g_current_phase]) | PHASE_STOP_SUPPRESSED);
  }

  interrupts_available = 0;
  if (g_duel_ai_mode_state != 1 && g_stop_phase_player == g_current_player && g_current_phase == g_stop_phase)
  {
    g_phase_stop_suppressed = 0;
  }

  can_respond = player_can_stop_at_phase(reason_for_trigger_controller, g_current_phase);

  strcpy(prompt_copy, prompt);

  saved_land_can_be_played = g_land_can_be_played;
  saved_unk_00742f60 = g_event_player;
  selected_player = reason_for_trigger_controller;
  g_event_player = selected_player;
  player = g_event_player;

  g_response_selection_in_progress = 1;
  g_response_card_type_mask = g_response_action_mask & 0x30;
  g_response_action_selected = 0;

  ++g_response_window_depth;
  if (g_response_window_serial > g_response_window_depth)
  {
    g_response_window_serial = 0;
  }

  if ((g_duel_network_flags & 2) == 0)
  {
    g_response_activation_phase_mask = 2;
    g_response_source_phase_mask = 0x20;

    if (g_current_phase >= PHASE_DECLARE_BLOCKERS)
    {
      g_response_activation_phase_mask = 4;
      g_response_source_phase_mask = 0x40;
    }
    if (g_current_phase <= PHASE_MAIN1)
    {
      g_response_activation_phase_mask = 1;
      g_response_source_phase_mask = 0x10;
    }
    if (g_current_phase >= PHASE_MAIN2)
    {
      g_response_activation_phase_mask = 8;
      g_response_source_phase_mask = (int)0xffffff80;
    }
    if (g_current_phase == PHASE_DISCARD)
    {
      g_response_activation_phase_mask = 0xf;
      g_response_source_phase_mask = (int)0xfffffff0;
    }
    if (g_other_player == reason_for_trigger_controller && global_stack_cards[0].player == g_active_player)
    {
      g_response_activation_phase_mask = 0xf;
      g_response_source_phase_mask = (int)0xfffffff0;
    }
  }
  else
  {
    g_response_activation_phase_mask = 0xf;
    g_response_source_phase_mask = (int)0xfffffff0;
  }

  if ((g_duel_network_flags & 2) == 0)
  {
    if (g_duel_ai_mode_state == 1 || g_ai_action_replay_available != 0 ||
        (g_trigger_condition != -1 && g_current_turn == g_other_player) || g_current_action_event_code == 4 ||
        (g_other_player == g_event_player && (g_land_can_be_played & LCBP_REGENERATION) != 0))
    {
      selected_card = choose_response_card(player);
      selected_player = g_target_player_choice;
    }
    else
    {
      selected_card = 0xffffffffU;
    }

    result_flags = 0;

    if (selected_card != 0xffffffffU &&
        dispatch_card_trigger_event(selected_player, (int)selected_card, 0x7d, player) == 2)
    {
      resolve_card_immediately(selected_player, (int)selected_card, player);
      selected_card = 0xffffffffU;
      TENTATIVE_reassess_all_cards(0, 0xff);
    }

    if (selected_card != 0xffffffffU)
    {
      internal_card_id = PLAYER_CARD_INSTANCE(selected_player, (int)selected_card).internal_card_id;
      g_selected_response_internal_card_id = internal_card_id;

      if (is_in_play(selected_player, (int)selected_card))
      {
        if (PLAYER_CARD_INSTANCE(selected_player, (int)selected_card).unknown0x14 == (unsigned int)g_trigger_condition &&
            pending_killed_card_handler != NULL && g_trigger_condition != -1)
        {
          if (pending_killed_card_handler != NULL)
          {
            pending_killed_card_handler(selected_player, (int)selected_card);
          }
        }
        else
        {
          dispatch_event_to_single_card(selected_player, (int)selected_card, 0x73, 1 - selected_player, -1);
          if (activate(player, selected_player, (int)selected_card))
          {
            resolve_activated_ability(selected_player, (int)selected_card);
          }
          g_spell_fizzled = 0;
        }

        result_flags = 1;
        TENTATIVE_reassess_all_cards(0, 0xff);
      }
      else
      {
        if (resolve_cast_card(selected_player, (int)selected_card))
        {
          if (global_cards_data[internal_card_id].type == 0x20)
          {
            g_response_card_type_mask = g_response_action_mask & 0x20;
          }

          result_flags = 1;
          TENTATIVE_reassess_all_cards(0, 0xff);

          if (g_duel_ai_mode_state != 1 && internal_rand(3) == 0)
          {
            show_opponent_taunt("Didn't expect that, did ya?");
          }
        }
      }
    }
  }
  else
  {
    result_flags = 0;
  }

  num_candidates = response_mask = 0;
  maybe_card = 0xffffffffU;

  if (g_duel_ai_mode_state != 1 ||
      (g_trigger_condition != -1 &&
       (g_current_turn == g_active_player || (g_current_turn == g_other_player && (g_duel_network_flags & 2) != 0))))
  {
    if (((g_current_turn == g_active_player) || (g_current_turn == g_other_player && (g_duel_network_flags & 2) != 0)) &&
        g_trigger_condition != -1)
    {
      for (player = 0; player <= 1; ++player)
      {
        for (card = 0; (int)card < g_active_cards_count[player]; ++card)
        {
          if (is_in_play(player, (int)card))
          {
            response_code = dispatch_card_trigger_event(player, (int)card, 0x7d, reason_for_trigger_controller);
            if (response_code)
            {
              if (response_code == 2)
              {
                maybe_card = card;
                maybe_player = player;
                selected_player = player;
                ++num_candidates;
              }
              else
              {
                response_mask |= 1U << (unsigned char)response_code;
              }
            }
          }

          if ((g_response_processing_flags & 1) != 0 &&
              PLAYER_CARD_INSTANCE(player, (int)card).unknown0x14 == (unsigned int)g_trigger_condition &&
              g_current_turn == player && g_trigger_condition != -1 &&
              PLAYER_CARD_INSTANCE(player, (int)card).internal_card_id != -1)
          {
            response_mask |= 4;
            maybe_card = card;
            maybe_player = player;
            selected_player = player;
            ++num_candidates;
          }
        }
      }
    }

    if (maybe_card == 0xffffffffU)
    {
      if (g_duel_ai_mode_state != 1)
      {
        if (g_trigger_condition == 0xca && (char)g_duel_phase_stop_settings[g_current_player].phase_flags[PHASE_UPKEEP] == 0)
        {
          g_response_processing_flags = 0;
        }
        if (g_trigger_condition == 0xce && (char)g_duel_phase_stop_settings[g_current_player].phase_flags[PHASE_DRAW] == 0)
        {
          g_response_processing_flags = 0;
        }
      }
    }

    if (maybe_card != 0xffffffffU || g_duel_ai_mode_state != 1)
    {
      if (maybe_card != 0xffffffffU || response_mask != 0 || ((g_response_processing_flags & 1) != 0 && g_response_action_mask != 0))
      {
        if ((g_duel_network_flags & 2) == 0)
        {
          player = g_active_player;
        }
        else
        {
          player = reason_for_trigger_controller;
        }

        g_response_processing_flags = g_activation_event_flags = 0;
        some_flag = 0;

        for (card = 0; (int)card < g_active_cards_count[player]; ++card)
        {
          if (PLAYER_CARD_INSTANCE(player, (int)card).internal_card_id != -1 &&
              ((PLAYER_CARD_INSTANCE(player, (int)card).upkeep_flags & 1) != 0 ||
               (PLAYER_CARD_INSTANCE(player, (int)card).upkeep_flags & 0x10) != 0 ||
               (!is_nonactivated_mana_source(player, (int)card) && player == g_event_player)))
          {
            response_code = get_card_response_action_type(player, (int)card);

            if (((response_code >= 2) ||
                 (((g_ai_decision_parameter != 0 || player != g_current_player) && (response_code & 2) != 0) ||
                  (g_activation_event_flags & 2) != 0)) &&
                ((g_current_turn != player || g_trigger_condition == -1) ||
                 (response_code != 2 || (g_activation_event_flags & 2) != 0)))
            {
              if ((g_activation_event_flags & 2) == 0 && response_code != 2)
              {
                if (response_code == 2)
                {
                  response_mask |= 4;
                }
                else
                {
                  response_mask |= 2;
                }
              }
              else
              {
                maybe_card = card;
                maybe_player = player;
                ++num_candidates;
              }

              g_activation_event_flags &= ~2;
            }
          }
        }

        if (g_current_action_event_code == 4)
        {
          for (card = 0; (int)card < g_active_cards_count[1 - player]; ++card)
          {
            if (PLAYER_CARD_INSTANCE(1 - player, (int)card).internal_card_id != -1 &&
                ((PLAYER_CARD_INSTANCE(1 - player, (int)card).upkeep_flags & 1) != 0 ||
                 (PLAYER_CARD_INSTANCE(1 - player, (int)card).upkeep_flags & 0x10) != 0 ||
                 !is_nonactivated_mana_source(1 - player, (int)card)))
            {
              response_code = get_card_response_action_type(1 - player, (int)card);
              if ((g_activation_event_flags & 2) != 0)
              {
                maybe_card = card;
                maybe_player = 1 - player;
                ++num_candidates;

                g_activation_event_flags &= ~2;

                if (response_code == 2)
                {
                  response_mask |= 4;
                }
                else
                {
                  response_mask |= 2;
                }

                if ((g_duel_network_flags & 2) == 0)
                {
                  break;
                }
              }
            }
          }
        }

        if ((response_mask & 2) != 0)
        {
          interrupts_available = 1;
        }
      }
    }
  }

  if (interrupts_available || can_respond || num_candidates)
  {
    strcpy(prompt_format, gs_triggered_effects_009396a0);

    if ((g_response_card_type_mask & 0x10) != 0 && g_current_spell_player == -1)
    {
      strcpy(prompt_format, gs_fast_effects_00926520);
    }
    else if ((g_response_card_type_mask & 0x20) != 0)
    {
      if (g_duel_state_007a7d78 != 0)
      {
        g_duel_state_007a7d78 = 1;
      }
      strcpy(prompt_format, gs_interrupts_0093a850);
    }

    if (g_trigger_condition != -1)
    {
      strcpy(prompt_format, gs_triggered_effects_009396a0);
    }

    sprintf(g_ui_message_buffer, prompt_format, prompt_copy);

    if (g_phase_stop_suppressed == 0 || (response_mask & 2) != 0)
    {
      if (((can_respond == 0 && (g_ai_decision_parameter == 0 || g_trigger_condition != -1)) &&
           (interrupts_available == 0 || g_trigger_condition == -1) &&
           (g_affected_card_context_flags == 0 || (response_mask & 6) == 0)) &&
          ((num_candidates <= (int)!(response_mask & 2) && (response_mask & 4) == 0) ||
           ((some_flag == 0 && should_skip_phase(reason_for_trigger_controller) != 0) || g_trigger_condition == 0xd6 ||
            (maybe_card != 0xffffffffU && g_response_window_serial == g_response_window_depth))))
      {
        selected_card = maybe_card;
        selected_player = maybe_player;
        g_response_action_selected = 1;
      }
      else
      {
        if (g_duel_ai_mode_state != 1)
        {
          g_phase_was_skipped = 1;
        }

        g_response_window_serial = 0;

        selection_done = g_response_window_serial;
        while (!selection_done)
        {
          if (g_duel_ai_mode_state == 1)
          {
            selected_card = maybe_card;
            selection_done = 1;
            g_target_selection_status_code = -1;
          }
          else
          {
            if ((g_duel_network_flags & 2) == 0)
            {
              player = g_active_player;
            }
            else
            {
              player = reason_for_trigger_controller;
            }

            if (g_active_player == player)
            {
              g_stop_phase = g_stop_phase_player = -1;
            }
            else
            {
              g_previous_stop_phase = g_previous_stop_phase_player = -1;
            }

            selected_card = select_card_for_action(player, -1, player, 0xff, 0, g_ui_message_buffer, 2);
            selected_player = g_target_player_choice;
            if ((int)selected_card >= 0)
            {
              g_duel_phase_stop_settings[g_current_player].phase_flags[g_current_phase] =
                  (unsigned char)(((int)(char)g_duel_phase_stop_settings[g_current_player].phase_flags[g_current_phase]) | PHASE_STOP_SUPPRESSED);
            }
          }

          switch (g_target_selection_status_code)
          {
          case 0:
            if (selected_player != -1 && selected_card != 0xffffffffU)
            {
              selection_done = 1;
            }
            else if (selected_player != -1 && selected_card == 0xffffffffU)
            {
              selection_done = 0;
            }
            break;

          case -3:
            selection_done = 0;
            break;

          case -2:
            selection_done = 1;
            selected_card = 0xffffffffU;

            g_activation_dispatch_flags &= ~2;
            if (g_trigger_condition != -1)
            {
              g_target_selection_status_code = 0;
            }

            if (num_candidates != 0)
            {
              g_response_window_serial = g_response_window_depth;
              g_response_action_selected = 1;
              if (g_duel_ai_mode_state != 1)
              {
                g_phase_stop_suppressed = 1;
              }
            }
            break;
          }
        }
      }
    }
    else
    {
      selected_card = maybe_card;
      selected_player = maybe_player;
      if (maybe_card != 0xffffffffU)
      {
        g_response_action_selected = 1;
      }
    }

    strcpy(g_ui_message_buffer, "");

    if ((g_duel_network_flags & 2) == 0)
    {
      player = g_active_player;
    }
    else
    {
      player = reason_for_trigger_controller;
    }

    if (selected_card != 0xffffffffU &&
        (selected_player == player ||
         (((PLAYER_CARD_INSTANCE(selected_player, (int)selected_card).state & STATE_IN_PLAY) != 0 &&
           dispatch_card_trigger_event(selected_player, (int)selected_card, 0x7d, player) != 0) ||
          g_current_action_event_code == 4)))
    {
      internal_card_id = PLAYER_CARD_INSTANCE(selected_player, (int)selected_card).internal_card_id;

      if (get_card_response_action_type(selected_player, (int)selected_card) != 0)
      {
        if (is_in_play(selected_player, (int)selected_card))
        {
          if (dispatch_card_trigger_event(selected_player, (int)selected_card, 0x7d, player) != 0)
          {
            resolve_card_immediately(selected_player, (int)selected_card, player);
          }
          else
          {
            if (PLAYER_CARD_INSTANCE(selected_player, (int)selected_card).unknown0x14 == (unsigned int)g_trigger_condition &&
                pending_killed_card_handler != NULL && g_trigger_condition != -1)
            {
              if (pending_killed_card_handler != NULL)
              {
                pending_killed_card_handler(selected_player, (int)selected_card);
              }
            }
            else
            {
              if (activate(player, selected_player, (int)selected_card))
              {
                resolve_activated_ability(selected_player, (int)selected_card);

                if (g_spell_fizzled != 1 && g_duel_ai_mode_state != 1)
                {
                  play_sound_effect(0x26);
                }
              }

              g_spell_fizzled = 0;
            }
          }
        }
        else
        {
          resolve_cast_card(selected_player, (int)selected_card);
          if (global_cards_data[internal_card_id].type == 0x20)
          {
            g_response_card_type_mask = g_response_action_mask & 0x20;
          }

          if (internal_rand(3) == 0)
          {
            show_opponent_taunt("I knew that was coming.");
          }
        }

        TENTATIVE_reassess_all_cards(0, 0xff);
        result_flags |= 2;
      }
      else
      {
        if (can_activate_mana_source_for_stop_prompt(selected_player, (int)selected_card) != 0 && selected_player == reason_for_trigger_controller)
        {
          activate_mana_source_card(selected_player, (int)selected_card);
        }
      }

      result_flags |= 2;
    }

    g_response_processing_flags = 1;
    g_response_action_selected = 0;
  }

  if (result_flags == 0)
  {
    g_response_card_type_mask = g_response_action_mask & 0x30;
  }

  g_response_selection_in_progress = 0;
  g_event_player = saved_unk_00742f60;
  g_land_can_be_played = saved_land_can_be_played;
  --g_response_window_depth;

  return (int)result_flags;
}

// FUNCTION: MAGIC 0x004aec05
// FUNCTION: SHANDALAR 0x0046bbe0
unsigned int choose_response_card(int player)
{
  unsigned int candidates[20];
  int response_flags;   /* local_18 */
  unsigned int card;    /* local_14 */
  int other_player;     /* local_10 */
  int internal_card_id; /* local_c */
  int num_candidates;   /* local_8 */

  num_candidates = 0;

  if (g_trigger_condition != -1 && g_current_turn == g_other_player)
  {
    for (other_player = 0; other_player < 2; ++other_player)
    {
      for (card = 0; (int)card < g_active_cards_count[other_player]; ++card)
      {
        if (is_in_play(other_player, (int)card) &&
            dispatch_card_trigger_event(other_player, (int)card, 0x7d, player) == 2)
        {
          g_activation_dispatch_flags = 4;
          g_target_player_choice = other_player;
          return card;
        }

        if (player == other_player &&
            (int)PLAYER_CARD_INSTANCE(other_player, (int)card).unknown0x14 == g_trigger_condition &&
            g_current_turn == other_player && g_trigger_condition != -1 &&
            PLAYER_CARD_INSTANCE(other_player, (int)card).internal_card_id != -1)
        {
          g_activation_dispatch_flags |= 4;
          ++g_duel_state_current_card;
          g_target_player_choice = other_player;
          return card;
        }
      }
    }
  }

  if (((g_response_processing_flags & 2) == 0) || (player == g_active_player) || (g_response_action_mask == 0))
  {
    return 0xffffffffU;
  }

  g_target_player_choice = player;

  for (card = 0; (int)card < g_active_cards_count[player]; ++card)
  {
    internal_card_id = PLAYER_CARD_INSTANCE(player, (int)card).internal_card_id;
    if (internal_card_id == -1)
      continue;

    response_flags = (int)get_card_response_action_type(player, (int)card);
    if (response_flags != 0)
    {
      candidates[num_candidates] = card;
      ++num_candidates;

      if (response_flags == 2)
      {
        return card;
      }
    }
  }

  if (g_current_action_event_code == 4)
  {
    for (card = 0; (int)card < g_active_cards_count[1 - player]; ++card)
    {
      internal_card_id = PLAYER_CARD_INSTANCE(1 - player, (int)card).internal_card_id;
      if (internal_card_id == -1)
        continue;

      response_flags = (int)get_card_response_action_type(1 - player, (int)card);
      if (response_flags != 0 && response_flags == 2)
      {
        g_target_player_choice = 1 - player;
        return card;
      }
    }
  }

  if (g_current_action_event_code == 4)
  {
    return 0xffffffffU;
  }

  if (duel_host_scripted_input())
  {
    int selected = duel_host_choose(DUEL_CHOICE_PRIORITY, player, "Opponent response", num_candidates, (const int*)candidates, NULL);
    if (selected == -2) return 0xffffffffU;
    for (int n = 0; n < num_candidates; ++n)
      if (candidates[n] == (unsigned int)selected) return candidates[n];
    duel_host_fail("illegal opponent response");
  }

  candidates[num_candidates] = 0xffffffffU;
  ++num_candidates;

  if (g_duel_ai_mode_state == 1)
  {
    if (internal_rand(2) != 0 && get_recorded_action_count() != 0)
    {
      g_ai_recorded_choice = num_candidates - 1;
    }
    else
    {
      g_ai_recorded_choice = internal_rand(num_candidates);
    }

    if (g_ai_search_force_pass != 0)
    {
      g_ai_recorded_choice = num_candidates - 1;
      if (g_ai_search_force_pass == 1)
      {
        g_ai_search_force_pass = -1;
      }
    }

    if (candidates[g_ai_recorded_choice] == 0xffffffffU)
      g_ai_recorded_action = -1;
    else
      g_ai_recorded_action = ((PLAYER_CARD_INSTANCE(player, (int)candidates[g_ai_recorded_choice]).state & STATE_IN_PLAY) ? 0x2000 : 0x1000) | candidates[g_ai_recorded_choice] | ((player != 0) ? 0x100 : 0);

    g_ai_recorded_action_type = 4;
    record_ai_action_selection();
  }
  else
  {
    g_ai_recorded_action_type = 4;
    replay_ai_action_selection();

    if (g_ai_recorded_choice < 0 || num_candidates <= g_ai_recorded_choice)
    {
      g_ai_recorded_choice = num_candidates - 1;
    }
  }

  if (candidates[g_ai_recorded_choice] != 0xffffffffU)
  {
    if (g_duel_state_008a8de4 >= 0x10)
    {
      --g_duel_state_008a8de4;
    }

    g_nested_trigger_stack[g_duel_state_008a8de4] =
        PLAYER_CARD_INSTANCE(player, (int)candidates[g_ai_recorded_choice]).internal_card_id;
    g_ai_action_score_table[g_duel_state_008a8de4] = candidates[g_ai_recorded_choice];
    ++g_duel_state_008a8de4;
  }

  return candidates[g_ai_recorded_choice];
}

// FUNCTION: MAGIC 0x004af0ec
// FUNCTION: SHANDALAR 0x0046c0c8
int get_card_response_action_type(int player, int card)
{
  int tmp;                 /* ebp-0x0c */
  int response_controller; /* ebp-0x08 */
  int internal_card_id;    /* ebp-0x04 */

  if ((g_duel_network_flags & 2) != 0)
  {
    response_controller = player;
  }
  else
  {
    response_controller = g_active_player;
  }

  internal_card_id = PLAYER_CARD_INSTANCE(player, card).internal_card_id;

  if ((PLAYER_CARD_INSTANCE(player, card).state & STATE_IN_PLAY) != 0)
  {
    if ((PLAYER_CARD_INSTANCE(player, card).state & STATE_OUBLIETTED) != 0)
    {
      return 0;
    }

    if ((int)PLAYER_CARD_INSTANCE(player, card).unknown0x14 == g_trigger_condition)
    {
      if (g_trigger_condition != -1)
      {
        if (g_current_turn == player)
        {
          g_activation_dispatch_flags |= 4;
          g_duel_state_current_card += 1;
          return 2;
        }
        else
        {
          return 0;
        }
      }
    }

    if (g_trigger_condition != -1)
    {
      tmp = dispatch_card_trigger_event(player, card, 0x7d, player);
      if (tmp != 0)
      {
        g_activation_dispatch_flags |= 1 << ((char)tmp);
        g_duel_state_current_card += 1;

        if (tmp >= 2)
        {
          return 2;
        }
        else
        {
          return 3;
        }
      }
      return 0;
    }

    if (((PLAYER_CARD_INSTANCE(player, card).upkeep_flags & 1) == 0 &&
         (global_cards_data[internal_card_id].extra_ability & 1) != 0 &&
         (g_response_card_type_mask & 0x10) != 0) ||
        ((global_cards_data[internal_card_id].extra_ability & 2) != 0 &&
         (g_response_card_type_mask & 0x20) != 0))
    {
      if (g_duel_state_00791418 != 0 &&
          (global_cards_data[internal_card_id].extra_ability & 2) == 0)
      {
        return 0;
      }

      if ((g_land_can_be_played & LCBP_DAMAGE_PREVENTION) == 0 ||
          ((g_land_can_be_played & LCBP_DAMAGE_PREVENTION) != 0 &&
           (global_cards_data[internal_card_id].extra_ability & 0x1004) != 0) ||
          (g_current_action_event_code == 0xd3 &&
           (global_cards_data[internal_card_id].extra_ability & 0x4000) != 0))
      {
        g_activation_event_flags &= ~2;

        if (((response_controller == player ||
              (g_response_source_phase_mask & (int)(char)global_cards_data[internal_card_id].act_phases) != 0) &&
             (PLAYER_CARD_INSTANCE(player, card).state & STATE_INVISIBLE) == 0) &&
            dispatch_event_to_single_card(player, card, 0x73, 1 - player, -1) != 0)
        {

          if ((g_activation_event_flags & 2U) != 0)
          {
            g_activation_dispatch_flags |= 4;
            return 2;
          }

          g_activation_dispatch_flags |= 2;
          return 3;
        }
      }
    }

    if (g_current_action_event_code == 4 && (PLAYER_CARD_INSTANCE(player, card).upkeep_flags & 1) != 0)
    {
      g_activation_event_flags |= 3;
      g_activation_dispatch_flags |= 4;
      return 2;
    }

    if (g_current_action_event_code == 4 && player == g_event_player &&
        (PLAYER_CARD_INSTANCE(player, card).upkeep_flags & 0x10) != 0 &&
        (PLAYER_CARD_INSTANCE(player, card).upkeep_flags & 0x88) == 0 &&
        can_pay_untap_cost(player, card) != 0)
    {
      g_activation_dispatch_flags |= 2;

      if (g_other_player == player && (g_duel_network_flags & 2) == 0)
      {
        return 2;
      }
      else
      {
        return 3;
      }
    }
  }
  else
  {
    if ((PLAYER_CARD_INSTANCE(player, card).state & (STATE_INVISIBLE | STATE_SUMMONSICK)) != 0)
    {
      return 0;
    }

    if (g_trigger_condition != -1 &&
        global_cards_data[internal_card_id].code_pointer != card_death_ward)
    {
      return 0;
    }

    if (g_duel_state_00791418 != 0 && ((global_cards_data[internal_card_id].type & 0x20) == 0))
    {
      return 0;
    }

    if ((g_response_card_type_mask & (unsigned char)global_cards_data[internal_card_id].type) != 0 &&
        can_pay_card_mana_cost(player, player, card) != 0 &&
        ((g_land_can_be_played & LCBP_DAMAGE_PREVENTION) == 0 ||
         ((g_land_can_be_played & LCBP_DAMAGE_PREVENTION) != 0 &&
          (global_cards_data[internal_card_id].extra_ability & 0x1004) != 0) ||
         (g_current_action_event_code == 0xd3 &&
          (global_cards_data[internal_card_id].extra_ability & 0x2000) != 0)) &&
        ((response_controller == player ||
          (g_response_activation_phase_mask & (int)(char)global_cards_data[internal_card_id].act_phases) != 0) &&
         dispatch_event_to_single_card(player, card, 0x74, 1 - player, -1) != 0))
    {
      return 3;
    }
  }

  return 0;
}

// FUNCTION: MAGIC 0x004af6f4
// FUNCTION: SHANDALAR 0x0046c6cf
int is_phase_stop_allowed_for_current_event(void);

// FUNCTION: MAGIC 0x004afa4b
// FUNCTION: SHANDALAR 0x0046ca26
void process_damage_prevention(int player)
{
  int did_setup;    /* ebp-0x18 */
  int test_card;    /* ebp-0x14 */
  int test_player;  /* ebp-0x10 */
  int best_score;   /* ebp-0x0c */
  int saved_777854; /* ebp-0x08 */
  int temp;         /* ebp-0x04 */

  if ((g_land_can_be_played & LCBP_PENDING_DAMAGE_CARDS) == 0)
  {
    return;
  }
  g_land_can_be_played &= ~LCBP_PENDING_DAMAGE_CARDS;
  g_land_can_be_played |= LCBP_DAMAGE_PREVENTION;
  TENTATIVE_reassess_all_cards(0, 0xff);

  for (test_player = 0; test_player < 2; ++test_player)
  {
    for (test_card = 0; test_card < g_active_cards_count[test_player]; ++test_card)
    {
      if (PLAYER_CARD_INSTANCE(test_player, test_card).internal_card_id == g_damage_card_internal_card_id &&
          is_in_play(test_player, test_card) != 0 &&
          (PLAYER_CARD_INSTANCE(test_player, test_card).state & STATE_TAPPED) == 0)
      {
        dispatch_event(test_player, test_card, 0x21);
      }
    }
  }

  did_setup = 0;
setup:
  if (((g_duel_network_flags & 2) == 0) && g_duel_ai_mode_state != 1 && g_ai_action_replay_available == 0)
  {
    start_ai_decision_search(9, 0xf);
    best_score = -99999;
    did_setup = 1;
  }

after_setup:
  if (g_ai_decision_code == 9 && did_setup)
  {
    reset_ai_search_trial_state();
    g_ai_side_score_player_1 = 0;
    g_ai_side_score_player_0 = g_ai_side_score_player_1;
    g_duel_state_008a8de4 = g_ai_side_score_player_0;
    g_ai_modifier = g_duel_state_008a8de4;
  }

  if (allow_response(-2, -1, gs_damage_prevention_00939200, 0x8e) != 0)
  {
    goto setup;
  }

  C_dispatch_event_raw(EVENT_DAMAGE_PREVENTION);
  if ((g_battlefield_extra_ability_flags & 0x00040000) != 0)
  {
    dispatch_trigger_twice_once_with_each_player_as_reason(g_current_player, TRIGGER_END_DAMAGE_PREV, gs_end_damage_prevention_00789740, 0);
  }

  for (test_player = 0; test_player < 2; ++test_player)
  {
    for (test_card = 0; test_card < g_active_cards_count[test_player]; ++test_card)
    {
      if (PLAYER_CARD_INSTANCE(test_player, test_card).internal_card_id == g_damage_card_internal_card_id &&
          is_in_play(test_player, test_card) != 0 &&
          (PLAYER_CARD_INSTANCE(test_player, test_card).state & STATE_TAPPED) == 0)
      {
        dispatch_event(test_player, test_card, 0x6e);
      }
    }
  }

  dispatch_trigger_twice_once_with_each_player_as_reason(g_current_player, TRIGGER_DEAL_DAMAGE, gs_damage_dealing_00777ab0, 0);

  for (test_player = 0; test_player < 2; ++test_player)
  {
    for (test_card = 0; test_card < g_active_cards_count[test_player]; ++test_card)
    {
      if (PLAYER_CARD_INSTANCE(test_player, test_card).internal_card_id == g_damage_card_internal_card_id &&
          is_in_play(test_player, test_card) != 0)
      {
        if ((PLAYER_CARD_INSTANCE(test_player, test_card).state & STATE_TAPPED) != 0)
        {
          kill_card(test_player, test_card, 1);
        }
        else
        {
          g_land_can_be_played |= LCBP_PENDING_DAMAGE_CARDS;
        }
      }
    }
  }

  kill_creatures_with_lethal_damage();
  g_land_can_be_played &= ~LCBP_DAMAGE_PREVENTION;

  if (g_duel_ai_mode_state == 1 && did_setup != 0 && g_ai_decision_code == 9)
  {
    regenerate_or_graveyard_triggers();
    temp = g_ai_modifier + ai_opinion_of_gamestate(g_other_player);
    if (temp > best_score)
    {
      best_score = temp;
      save_recorded_ai_actions();
      saved_777854 = g_ai_search_flags;
    }
    if (g_ai_search_target_depth == 999)
    {
      g_ai_search_target_depth = -1;
    }
    g_ai_search_force_pass = 0;
#ifdef SHANDALAR
    if ((g_game_time_scale * g_ai_search_time_limit) / 5 < GetUiTickCount())
#else
    if (get_duel_time_units_if_available() > (g_game_time_scale * g_ai_search_time_limit) / 5)
#endif
    {
      g_duel_ai_mode_state = 0;
      g_ai_search_target_depth = -1;
      g_ai_search_flags = saved_777854;
    }
    g_land_can_be_played |= LCBP_DAMAGE_PREVENTION;
    goto after_setup;
  }

  if (g_duel_ai_mode_state != 1 && did_setup != 0)
  {
    g_ai_action_replay_available = 0;
  }
}

// FUNCTION: MAGIC 0x004aff25
// FUNCTION: SHANDALAR 0x0046cf00
void kill_creatures_with_lethal_damage(void);

// FUNCTION: MAGIC 0x004b082f
// FUNCTION: SHANDALAR 0x0046d80a
int dispatch_card_trigger_event(int player, int card, event_t event, int extra);

// FUNCTION: MAGIC 0x004c0a36
// FUNCTION: SHANDALAR 0x004ecec6
int ClampIntToRange(int a1, int a2, int a3)
{
  if (a1 < a2)
  {
    a1 = a2;
  }
  if (a3 < a1)
  {
    a1 = a3;
  }
  return a1;
}

// FUNCTION: MAGIC 0x004e1b6f
int get_duel_time_units_if_available(void)
{
  return get_duel_time_units();
}

// FUNCTION: MAGIC 0x004e1c81
int legacy_clear_graphics_page_stub(int page, int color)
{
}

// FUNCTION: MOK 0x0044e050
// FUNCTION: MAGIC 0x004e1d6d
int legacy_blit_graphics_rect_stub(int dst, int dst_x, int dst_y, int width, int height,
                                   int src, int src_x, int src_y, int src_width, int src_height)
{
  return 0;
}

// FUNCTION: MAGIC 0x004e1d91
int legacy_load_pcx_into_page_stub(int page, int x, int y, const char *filename, void *palette)
{
  return 1;
}

// FUNCTION: MAGIC 0x004e1da6
int legacy_load_pcx_into_page_no_palette_stub(const char *filename)
{
  return 1;
}

// FUNCTION: MAGIC 0x0055117d
// FUNCTION: SHANDALAR 0x0049f3c0
void dispatch_three_arg_callback_to_cards_in_play(int(__cdecl *callback)(int, int, int), int who_to_check)
{
  int test_card;
  int test_player;

  for (test_player = 0; test_player < 2; ++test_player)
  {
    if (who_to_check == -1 || test_player == who_to_check)
    {
      for (test_card = 0; test_card < g_active_cards_count[test_player]; ++test_card)
      {
        if (is_in_play(test_player, test_card))
        {
          callback(test_player,
                   test_card,
                   global_card_instances[test_player][test_card].internal_card_id);
        }
      }
    }
  }
}

// FUNCTION: MAGIC 0x005513d7
// FUNCTION: SHANDALAR 0x0049f61a
void add_special_counters(int player, int card, int amount)
{
  if ((PLAYER_CARD_INSTANCE(player, card).special_counters & 0xffu) == 0xffu)
    return;

  PLAYER_CARD_INSTANCE(player, card).special_counters =
      ((PLAYER_CARD_INSTANCE(player, card).special_counters + amount) & 0xff) |
      (PLAYER_CARD_INSTANCE(player, card).special_counters & 0xffffff00);

  if (g_duel_ai_mode_state != 1)
  {
    play_sound_effect(ICON_HASTE);
  }
}
