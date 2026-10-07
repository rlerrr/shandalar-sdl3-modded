#include "duel_host.h"
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

int choose_ai_main_phase_action(int player);

int can_target(target_definition_t *td)
{
  return td != NULL;
}

// FUNCTION: MAGIC 0x0048194e
// FUNCTION: SHANDALAR 0x00485458
int select_best_land_target_by_score(int preferred_player, int only_player, int *target_data)
{
  card_instance_t *instance; /* [ebp-0x84] */
  int card;                  /* [ebp-0x80] */
  int mana_colors[8];        /* [ebp-0x7c] */
  unsigned int colors;       /* [ebp-0x5c] */
  int card_colors[8];        /* [ebp-0x58] */
  int current_score;         /* [ebp-0x38] */
  int color;                 /* [ebp-0x34] */
  int best_score;            /* [ebp-0x30] */
  int land_scores[8];        /* [ebp-0x2c] */
  int best_card_color;       /* [ebp-0x0c] */
  int player;                /* [ebp-0x08] */
  int best_land_color;       /* [ebp-0x04] */

  target_data[1] = -1;
  *target_data = target_data[1];

  for (color = 0; color <= 5; ++color)
  {
    card_colors[color] = 0;
    land_scores[color] = card_colors[color];
    mana_colors[color] = land_scores[color];
  }

  for (player = 0; player < 2; ++player)
  {
    if (only_player != -1 && player != only_player)
      continue;

    for (card = 0; card < g_active_cards_count[player]; ++card)
    {
      instance = &PLAYER_CARD_INSTANCE(player, card);
      if (instance->internal_card_id != -1)
      {
        if ((instance->state & (STATE_OUBLIETTED | STATE_INVISIBLE | STATE_IN_PLAY)) == STATE_IN_PLAY)
        {
          if ((global_cards_data[instance->internal_card_id].extra_ability & EA_MANA_SOURCE) != 0)
          {
            for (color = 1; color <= 5; ++color)
            {
              if ((((int)(char)instance->mana_color) & (1 << (unsigned char)color)) != 0)
              {
                ++mana_colors[color];
              }
            }
          }

          if (((int)instance->color & 0x3e) != 0)
          {
            for (color = 1; color <= 5; ++color)
            {
              if (((int)instance->color & (1 << (unsigned char)color)) != 0)
              {
                ++land_scores[color];
              }
            }
          }
        }
        else if (preferred_player == only_player || only_player == -1)
        {
          colors = (unsigned int)(char)global_cards_data[instance->internal_card_id].color;
          for (color = 1; color <= 5; ++color)
          {
            if ((colors & (1 << (unsigned char)color)) != 0)
            {
              ++card_colors[color];
            }
          }
        }
      }
    }
  }

  best_card_color = 0;
  best_land_color = best_card_color;

  for (color = 1; color <= 5; ++color)
  {
    if (land_scores[color] > land_scores[best_land_color])
    {
      best_land_color = color;
    }
    if (card_colors[color] > card_colors[best_card_color])
    {
      best_card_color = color;
    }
  }

  best_score = -1;

  for (player = 0; player < 2; ++player)
  {
    if (only_player != -1 && player != only_player)
      continue;

    for (card = 0; card < g_active_cards_count[player]; ++card)
    {
      instance = &PLAYER_CARD_INSTANCE(player, card);
      if (is_in_play(player, card) != 0)
      {
        if ((global_cards_data[instance->internal_card_id].type & TYPE_LAND) != 0)
        {
          current_score = 0;

          if ((global_cards_data[instance->internal_card_id].extra_ability & EA_ACT_ABILITY) != 0)
          {
            current_score += 2;
            if ((instance->state & STATE_TAPPED) != 0)
            {
              current_score += 3;
            }
          }

          if ((global_cards_data[instance->internal_card_id].extra_ability & EA_MANA_SOURCE) != 0)
          {
            ++current_score;

            colors = (unsigned int)(char)instance->mana_color;
            for (color = 1; color <= 5; ++color)
            {
              if ((colors & (1 << (unsigned char)color)) != 0)
              {
                current_score += ((4 - mana_colors[color]) > 0) ? (4 - mana_colors[color]) : 0;
              }
            }

            if (best_land_color != 0 && (colors & (1 << (unsigned char)best_land_color)) != 0)
            {
              ++current_score;
            }
            if (preferred_player == only_player || only_player == -1)
            {
              if (best_card_color != 0 && (colors & (1 << (unsigned char)best_card_color)) != 0)
              {
                ++current_score;
              }
            }
          }

          if ((global_cards_data[instance->internal_card_id].type & TYPE_CREATURE) != 0)
          {
            ++current_score;
          }

          if (current_score > best_score)
          {
            best_score = current_score;
            *target_data = player;
            target_data[1] = card;
          }
        }
      }
    }
  }
  if (*target_data == -1)
    return 0;
  else
    return 1;
}

// FUNCTION: MAGIC 0x00483e3e
// FUNCTION: SHANDALAR 0x0048794a
int choose_best_card_from_library(int player, unsigned int type_mask)
{
  int best_score;
  int best_slot;
  int current_slot;
  int current_internal_id;
  int current_score;
  int mana_score;
  int abilities;
  int bonus;

  best_score = -999;
  best_slot = -1;

  if (player == -1)
  {
    return best_slot;
  }

  for (current_slot = 0; current_slot < 500 && global_library[player][current_slot] != -1; ++current_slot)
  {
    current_internal_id = global_library[player][current_slot];

    if (type_mask == 0xffffffff || (type_mask & (unsigned int)global_cards_data[current_internal_id].type) != 0)
    {
      current_score = (int)global_cards_data[current_internal_id].power + (int)global_cards_data[current_internal_id].toughness;

      if ((int)(char)global_cards_data[current_internal_id].cc[1] == -1)
      {
        mana_score =
            g_basiclandtypes_controlled[player][7] - (int)(char)global_cards_data[current_internal_id].cc[0] - 1;
      }
      else
      {
        mana_score = (int)(char)global_cards_data[current_internal_id].cc[1];
      }
      mana_score = (int)(char)global_cards_data[current_internal_id].cc[0] + mana_score;

      if (mana_score <= g_basiclandtypes_controlled[player][7])
      {
        current_score += mana_score;
      }

      abilities = (int)global_cards_data[current_internal_id].static_ability;
      while (abilities != 0)
      {
        if ((abilities & 1) != 0)
        {
          ++current_score;
        }
        abilities >>= 1;
      }

      if ((global_cards_data[current_internal_id].extra_ability & 0x1000) != 0)
      {
        bonus = 8 - g_basiclandtypes_controlled[player][7];
        if (bonus < 2)
        {
          bonus = 1;
        }
        current_score += bonus;
      }

      if ((global_cards_data[current_internal_id].extra_ability & 1) != 0)
      {
        current_score += mana_score;
      }

      if ((global_cards_data[current_internal_id].type & TYPE_ARTIFACT) != 0)
      {
        ++current_score;
      }

      if (best_score < current_score)
      {
        best_score = current_score;
        best_slot = current_slot;
      }
    }
  }

  return best_slot;
}

// FUNCTION: MAGIC 0x004a62d7
// FUNCTION: SHANDALAR 0x00558457
int select_from_graveyard_with_dialog(int player,
                                      int *graveyard,
                                      void *available,
                                      int count,
                                       char **prompt,
                                      int a6,
                                       int *selected,
                                      int a8,
                                      int a9)
{
  if (g_duel_ai_mode_state == 1)
  {
    return 1;
  }
  return select_multiple_cards_from_card_list(player, graveyard, 0, available, count, prompt, a6, selected, a8, a9);
}

// FUNCTION: MAGIC 0x004a67bd
// FUNCTION: SHANDALAR 0x00558941
int choose_a_number(int player, char *prompt, int maxnum)
{
  if (duel_host_scripted_input()) return duel_host_choose(DUEL_CHOICE_NUMBER, player, prompt, maxnum, NULL, NULL);
  int chosen;

  if (g_duel_ai_mode_state == 1)
  {
    return maxnum;
  }

  if (player == g_other_player && (g_duel_network_flags & 2) != 0)
  {
    TENTATIVE_wait_for_network_result(player, 0xe);
    maxnum = g_dialog_result_network_packet.result;
  }

  chosen = prompt_for_life_total(player, prompt, maxnum);

  if (player == g_active_player && (g_duel_network_flags & 2) != 0)
  {
    g_dialog_result_network_packet.packet_type = 0xe;
    g_dialog_result_network_packet.result = chosen;
    TENTATIVE_send_network_result(player, 0xe);
  }

  return chosen;
}

// FUNCTION: MAGIC 0x004a686a
// FUNCTION: SHANDALAR 0x005589ed
int choose_a_color(int player, const char *prompt, int use_color_names_instead_of_land, int ai_choice, unsigned int available_colors)
{
  if (duel_host_scripted_input()) return duel_host_choose(DUEL_CHOICE_COLOR, player, prompt, available_colors, NULL, NULL);
  int chosen_color;

  if (g_duel_ai_mode_state == 1)
  {
    return ai_choice;
  }

  if (g_other_player == player && (g_duel_network_flags & 2) != 0)
  {
    TENTATIVE_wait_for_network_result(player, 0xf);
    return g_dialog_result_network_packet.result;
  }

  chosen_color = choose_a_color_dialog(player, prompt, use_color_names_instead_of_land, ai_choice, available_colors);
  if (player == g_active_player && (g_duel_network_flags & 2) != 0)
  {
    g_dialog_result_network_packet.packet_type = '\x0f';
    g_dialog_result_network_packet.result = chosen_color;
    TENTATIVE_send_network_result(player, 0xf);
  }

  return chosen_color;
}

// FUNCTION: MAGIC 0x004b413c
// FUNCTION: SHANDALAR 0x0056a3a7
int select_damage_card_from_list(int player,
                                 int *internal_card_ids,
                                 int *damage_amounts,
                                 int count,
                                 char *title,
                                 int allow_cancel,
                                 char *prompt)
{
  int selected;

  selected = -1;

  if (player == g_other_player && (g_duel_network_flags & 2) != 0)
  {
    TENTATIVE_wait_for_network_result(player, 0x19);
    selected = g_network_result_packet.result;
  }

  if (player == g_active_player && g_duel_ai_mode_state != 1)
  {
    selected = show_damage_assignment_cardlist(internal_card_ids, damage_amounts, count, title, allow_cancel, prompt);
    if ((g_duel_network_flags & 2) != 0)
    {
      g_network_result_packet.result = selected;
      g_network_result_packet.packet_type = 0x19;
      TENTATIVE_send_network_result(player, 0x19);
    }
  }

  return selected;
}

// FUNCTION: MAGIC 0x004b41f2
// FUNCTION: SHANDALAR 0x0056a45f
int select_card_from_graveyard(int player,
                               int *graveyard,
                               int *allowed_cards,
                               int count,
                               const char *prompt,
                               int allow_cancel,
                               char *title)
{
  int result;

  result = -1;

  if (g_other_player == player && (g_duel_network_flags & 2) != 0)
  {
    TENTATIVE_wait_for_network_result(player, 0x19);
    result = g_network_result_packet.result;
  }

  if (g_active_player == player && g_duel_ai_mode_state != 1)
  {
    result = (int)show_selectable_cardlist(graveyard,
                                           allowed_cards,
                                           count,
                                           (void *)prompt,
                                           (unsigned int)allow_cancel,
                                           title);
    if ((g_duel_network_flags & 2) != 0)
    {
      g_network_result_packet.packet_type = 0x19;
      g_network_result_packet.result = result;
      TENTATIVE_send_network_result(player, 0x19);
    }
  }

  return result;
}

#ifndef SHANDALAR
// FUNCTION: MOK 0x0047936d
// FUNCTION: MAGIC 0x004b42aa
int show_deck(int player, int *cards, int count, char *title, int require_selection, char *prompt)
{
  int y_step;
  int selected_index;
  int display_count;
  int y_positions[500];
  int x_positions[500];
  int y;
  int x;
  int valid_count;
  int valid_graveyards[500];
  int i;
  int result;

  if (((player == g_other_player) && ((g_duel_network_flags & 2) != 0)) && g_duel_active != 0)
  {
    TENTATIVE_wait_for_network_result(player, 0x19);
    return g_network_result_packet.result;
  }
  else if (((player == g_other_player) && ((g_duel_network_flags & 2) == 0)) || g_duel_ai_mode_state == 1)
  {
    valid_count = 0;
    for (i = 0; i < count; ++i)
    {
      if (cards[i] != -1)
      {
        valid_graveyards[valid_count] = i;
        ++valid_count;
      }
    }

    g_ai_recorded_choice = internal_rand(valid_count);
    if (player != g_active_player)
    {
      if (g_duel_ai_mode_state == 1)
      {
        record_ai_action_selection();
      }
      else
      {
        replay_ai_action_selection();
      }
    }

    return valid_graveyards[g_ai_recorded_choice];
  }
  else if (g_duel_active == 0)
  {
    ReadPalette("todpal.tr", (char *)0);
    legacy_clear_graphics_page_stub(0, 0);
    SelectPalette(g_duel_screen_dc, g_duel_palette, 0);
    legacy_load_pcx_into_page_no_palette_stub("advfac64.pic");
    legacy_load_pcx_into_page_stub(1, 0, 0, "seedeck.pic", &g_duel_palette_state);
    legacy_blit_graphics_rect_stub(g_duel_state_0057a75c, 0, 0, 0x280, 0x1e0, g_world_map_state, 0, 0, global_screen_width, global_screen_height);

    x = 0;
    display_count = 0;
    for (i = 0; i < count; ++i)
    {
      if (cards[i] != -1 && (i == 0 || cards[i - 1] != cards[i]))
      {
        ++display_count;
      }
    }

    valid_count = (display_count - 1) / 5;
    if (valid_count == 0)
    {
      valid_count = 1;
    }
    y_step = 0x54 / valid_count;

    x = 0x60;
    valid_count = 0;
    y = 0x10;
    for (i = 0; i < count; ++i)
    {
      if (cards[i] != -1 && (i == 0 || cards[i - 1] != cards[i]))
      {
        x_positions[valid_count] = x + 4;
        y_positions[valid_count] = y;
        valid_graveyards[valid_count] = i;
        ++valid_count;
        x += 0x38;
        if (x >= 0x140)
        {
          x = 0x60;
          y += y_step;
        }
      }
    }

    return selected_index;
  }
  else
  {
    result = show_cardlist_if_human(cards, count, title, require_selection, prompt);
    if ((player == g_active_player) && ((g_duel_network_flags & 2) != 0))
    {
      g_network_result_packet.result = result;
      g_network_result_packet.packet_type = 0x19;
      TENTATIVE_send_network_result(player, 0x19);
    }
  }

  return result;
}
#endif

// FUNCTION: MAGIC 0x004bd7d0
// FUNCTION: SHANDALAR 0x004be650
int real_target_available(int *num_valid_targets,
                          target_scan_mode_t target_source_mode,
                          int who_chooses,
                          unsigned int allowed_controller,
                          unsigned int preferred_controller,
                          target_zone_t zone,
                          type_t required_type,
                          type_t illegal_type,
                          keyword_t required_abilities,
                          keyword_t illegal_abilities,
                          color_test_t required_color,
                          color_test_t illegal_color,
                          int extra,
                          subtype_in_card_data_t required_subtype,
                          int required_power,
                          int required_toughness,
                          target_special_t special,
                          target_state_t required_state,
                          target_state_t illegal_state);

// FUNCTION: MAGIC 0x004bdc06
// FUNCTION: SHANDALAR 0x004bea85
unsigned int C_real_validate_target(int tgt_player,
                                    int tgt_card,
                                    char *return_error_str,
                                    int who_chooses,
                                    int allowed_controller,
                                    int preferred_controller,
                                    target_zone_t zone,
                                    type_t required_type,
                                    type_t illegal_type,
                                    keyword_t required_abilities,
                                    keyword_t illegal_abilities,
                                    color_test_t required_color,
                                    color_test_t illegal_color,
                                    int extra,
                                    subtype_in_card_data_t required_subtype,
                                    int power_requirement,
                                    int toughness_requirement,
                                    target_special_t special,
                                    target_state_t required_state,
                                    target_state_t illegal_state);

// FUNCTION: MAGIC 0x004e9c50
// FUNCTION: SHANDALAR 0x00507a50
int select_card_for_action(int player,
                           int arg_2,
                           int player_to_check,
                           unsigned int required_type,
                           unsigned int required_color,
                           char *prompt,
                           int dialog_mode)
{
  if (duel_host_is_headless()) {
    int selected = duel_host_scripted_input()
      ? duel_host_choose(DUEL_CHOICE_PRIORITY, player, prompt, 0, NULL, NULL)
      : choose_ai_main_phase_action(player);
    if (selected == -2 || (!duel_host_scripted_input() && selected == -1)) { g_target_selection_status_code = -2; g_target_player_choice = player; return -1; }
    if (selected < 0 || selected >= g_active_cards_count[player]) duel_host_fail("illegal priority choice");
    g_target_selection_status_code = 0; g_target_player_choice = player;
    return selected;
  }

  unsigned int modal_required_type; /* ebp - 0x20c */
  int stack_card;                   /* ebp - 0x208 */
  int action_msg;                   /* ebp - 0x204 */
  int stack_player;                 /* ebp - 0x200 */
  int candidate_cards[60];          /* ebp - 0x1fc */
  int candidate_count;              /* ebp - 0x10c (reused as random candidate index) */
  target_t selected_target;         /* ebp - 0x108 */
  int action;                       /* ebp - 0x100 (get_current_stack_action() result) */
  int current_card;                 /* ebp - 0xfc */
  int current_player;               /* ebp - 0xf8 */
  int selected_internal_id;         /* ebp - 0xf4 */
  int candidate_players[60];        /* ebp - 0xf0 */

  if (g_spell_fizzled == 1 || (g_duel_ai_mode_state == 1 && g_active_player == player))
  {
    return -1;
  }

  if ((g_ai_search_flags & 1) != 0)
  {
    player_to_check = arg_2;
  }

  if ((player == g_other_player && (g_duel_network_flags & 2) == 0) || g_duel_ai_mode_state == 1 || g_duel_network_state != 0)
  {
    candidate_count = 0;

    for (current_player = 0; current_player < 2; current_player++)
    {
      if (player_to_check != -1 && player_to_check != current_player)
      {
        continue;
      }

      for (current_card = 0; current_card < g_active_cards_count[current_player]; current_card++)
      {
        if (required_type == (unsigned int)-2 || PLAYER_CARD_INSTANCE(current_player, current_card).internal_card_id == -1)
        {
          continue;
        }

        if (((PLAYER_CARD_INSTANCE(current_player, current_card).state & (STATE_OUBLIETTED | STATE_IN_PLAY)) != STATE_IN_PLAY) &&
            !(g_active_player == player && g_duel_network_state != 0))
        {
          continue;
        }

        if (!(((int)required_type <= 0) ||
              ((required_type &
                (unsigned char)global_cards_data[PLAYER_CARD_INSTANCE(current_player, current_card).internal_card_id].type) != 0)))
        {
          continue;
        }

        if (!((required_color == 1 || required_color == 0) ||
              (((int)required_color & (int)PLAYER_CARD_INSTANCE(current_player, current_card).mana_color) != 0)))
        {
          continue;
        }

        candidate_players[candidate_count] = current_player;
        candidate_cards[candidate_count] = current_card;
        candidate_count++;
      }

      if (((int)required_type <= 0) && (required_color == 1 || required_color == 0))
      {
        candidate_players[candidate_count] = current_player;
        candidate_cards[candidate_count] = -1;
        candidate_count++;
      }
    }

    if (candidate_count == 0)
    {
      return -1;
    }

    if (g_active_player == player)
    {
    select_card_for_action_retry:
      candidate_count = internal_rand(candidate_count);
      g_target_player_choice = candidate_players[candidate_count];

      if (g_duel_network_state != 0)
      {
        g_target_selection_status_code = 0;
        if (internal_rand(0x20) == 0 || g_response_selection_in_progress != 0)
        {
          g_target_selection_status_code = (int)0xfffffffeU;
          g_stop_phase = -1;
          g_stop_phase_player = g_stop_phase;
          return -1;
        }

        if (g_active_player != g_target_player_choice)
        {
          goto select_card_for_action_retry;
        }

        selected_internal_id = PLAYER_CARD_INSTANCE(g_target_player_choice, candidate_cards[candidate_count]).internal_card_id;
        if (((global_cards_data[selected_internal_id].type & 1) != 0) &&
            ((PLAYER_CARD_INSTANCE(g_target_player_choice, candidate_cards[candidate_count]).state & STATE_IN_PLAY) != 0))
        {
          goto select_card_for_action_retry;
        }
        if (((global_cards_data[selected_internal_id].type & 2) != 0) &&
            ((PLAYER_CARD_INSTANCE(g_target_player_choice, candidate_cards[candidate_count]).state & STATE_ATTACKING) != 0))
        {
          goto select_card_for_action_retry;
        }

        if (g_current_phase > 0x14 && g_current_phase < 0x1e)
        {
          if ((global_cards_data[selected_internal_id].type & 2) == 0 || (PLAYER_CARD_INSTANCE(g_target_player_choice, candidate_cards[candidate_count]).state & STATE_IN_PLAY) == 0)
          {
            goto select_card_for_action_retry;
          }
          goto select_card_for_action_return_candidate;
        }

        if ((global_cards_data[selected_internal_id].type & 0x4b) == 0 || (PLAYER_CARD_INSTANCE(g_target_player_choice, candidate_cards[candidate_count]).state & STATE_IN_PLAY) != 0)
        {
          goto select_card_for_action_retry;
        }
      }

    select_card_for_action_return_candidate:
      return candidate_cards[candidate_count];
    }

    if (g_duel_ai_mode_state == 1)
    {
      g_ai_recorded_choice = internal_rand(candidate_count);
      g_ai_recorded_action = ((((candidate_players[g_ai_recorded_choice] == 0) - 1) & 0x100) |
                              (candidate_cards[g_ai_recorded_choice] & 0xff) |
                              0x4000);
      record_ai_action_selection();
    }
    else
    {
      replay_ai_action_selection();
      if (g_ai_recorded_choice == 99)
      {
        g_ai_recorded_choice = internal_rand(candidate_count);
      }
    }

    g_target_player_choice = candidate_players[g_ai_recorded_choice];
    return candidate_cards[g_ai_recorded_choice];
  }

  if (required_type != 0 && required_type != 0xff)
  {
    strcpy(g_ui_message_buffer, "");
    action = get_current_stack_action();
    if (action != -1)
    {
      action_msg = ((unsigned int)action >> 0x10) & 0xff;
      stack_player = global_stack_cards[g_stack_size - 1].player;
      stack_card = global_stack_cards[g_stack_size - 1].card;
      format_stack_action_text(g_ui_message_buffer,
                               action_msg,
                               stack_player,
                               stack_card);
    }
  }

  if (required_type == 0 || required_type == 0xff || required_type == (unsigned int)-2)
  {
    modal_required_type = 0xffffffffU;
  }
  else
  {
    modal_required_type = required_type;
  }

  run_target_selection_modal(player,
                             arg_2,
                             prompt,
                             dialog_mode,
                             modal_required_type,
                             required_color,
                             0xffffffff,
                             0xffffffff,
                             &g_target_selection_status_code,
                             &selected_target,
                             0,
                             0);

  strcpy(g_text_lines[0], "");
  if (selected_target.player != -1)
  {
    g_ai_action_replay_available = 0;
  }

  g_target_player_choice = selected_target.player;
  return selected_target.card;
}

// FUNCTION: MAGIC 0x00551638
// FUNCTION: SHANDALAR 0x0049f87b
int select_target_creature_and_store(int player, unsigned int preferred_controller, int card)
{
  target_t selected_target;

  if (preferred_controller == -1)
  {
    preferred_controller = 2;
  }

  if (C_real_select_target(player,
                           2,
                           preferred_controller,
                           TARGET_ZONE_IN_PLAY,
                           TYPE_CREATURE,
                           TYPE_NONE,
                           0,
                           get_protections_from(player, card),
                           0,
                           0,
                           -1,
                           ~SUB_WALL,
                           -1,
                           -1,
                           0,
                           0,
                           0,
                           g_text_lines[0],
                           1,
                           &selected_target) != 0)
  {
    global_card_instances[player][card].targets[(char)global_card_instances[player][card].number_of_targets].card = selected_target.card;
    global_card_instances[player][card].targets[(char)global_card_instances[player][card].number_of_targets].player = selected_target.player;
    ++global_card_instances[player][card].number_of_targets;
    return 1;
  }
  else
  {
    return 0;
  }
}

// FUNCTION: MAGIC 0x005517b4
// FUNCTION: SHANDALAR 0x0049f9f7
int select_target_creature_and_store_without_protection(int player, unsigned int preferred_controller, int card)
{
  target_t selected_target;

  if (preferred_controller == -1)
  {
    preferred_controller = 2;
  }

  if (C_real_select_target(player,
                           2,
                           preferred_controller,
                           TARGET_ZONE_IN_PLAY,
                           TYPE_CREATURE,
                           TYPE_NONE,
                           0,
                           0,
                           COLOR_TEST_0,
                           COLOR_TEST_0,
                           -1,
                           -1,
                           -1,
                           -1,
                           0,
                           0,
                           0,
                           g_text_lines[0],
                           1,
                           &selected_target) != 0)
  {
    SET_TARGET(PLAYER_CARD_INSTANCE(player, card).targets[(char)PLAYER_CARD_INSTANCE(player, card).number_of_targets], selected_target);
    ++PLAYER_CARD_INSTANCE(player, card).number_of_targets;
    return 1;
  }

  return 0;
}

// FUNCTION: MAGIC 0x00551921
// FUNCTION: SHANDALAR 0x0049fb64
int choose_creature_to_sacrifice(int player)
{
  int best_card;
  target_t target;
  int current_card;
  int best_value;
  int internal_card_id;
  int card_value;

  if ((player == g_active_player || (g_duel_network_flags & 2) != 0) && g_duel_ai_mode_state != 1)
  {
    if (!C_real_select_target(player, player, player, TARGET_ZONE_IN_PLAY, TYPE_CREATURE, TYPE_NONE, 0, 0,
                              COLOR_TEST_0, COLOR_TEST_0, -1, ~SUB_WALL, -1, -1, 0, 0, 0,
                              g_text_lines[0], 0, &target))
    {
      best_card = -1;
    }
    else
    {
      best_card = target.card;
    }
  }
  else
  {
    best_card = -1;
    best_value = 0x7fff;
    for (current_card = 0; current_card < g_active_cards_count[player]; ++current_card)
    {
      internal_card_id = PLAYER_CARD_INSTANCE(player, current_card).internal_card_id;
      if (internal_card_id != -1 &&
          (PLAYER_CARD_INSTANCE(player, current_card).state & (STATE_OUBLIETTED | STATE_IN_PLAY)) == STATE_IN_PLAY &&
          (global_cards_data[internal_card_id].type & TYPE_CREATURE) != 0 &&
          PLAYER_CARD_INSTANCE(player, current_card).kill_code != 3 &&
          (PLAYER_CARD_INSTANCE(player, current_card).state & STATE_CANNOT_TARGET) == 0)
      {
        card_value = C_get_abilities(player, current_card, EVENT_POWER, -1) + 2;
        card_value *= C_get_abilities(player, current_card, EVENT_TOUGHNESS, -1) + 2;
        if (card_value < best_value)
        {
          best_value = card_value;
          best_card = current_card;
        }
      }
    }
  }

  if (best_card != -1 && g_duel_ai_mode_state != 1)
  {
    play_sound_effect(WAV_SACRFICE);
  }
  return best_card;
}

// FUNCTION: MAGIC 0x00551b60
// FUNCTION: SHANDALAR 0x0049fda3
int select_target_land_and_store(int player, int preferred_controller, int card)
{
  target_t selected_target;

  if (preferred_controller == -1)
  {
    preferred_controller = 2;
  }

  if (C_real_select_target(player,
                           2,
                           preferred_controller,
                           TARGET_ZONE_IN_PLAY,
                           TYPE_LAND,
                           TYPE_NONE,
                           0,
                           get_protections_from(player, card),
                           0,
                           0,
                           -1,
                           ~SUB_WALL,
                           -1,
                           -1,
                           0,
                           0,
                           0,
                           g_text_lines[0],
                           1,
                           &selected_target) != 0)
  {
    SET_TARGET(PLAYER_CARD_INSTANCE(player, card).targets[PLAYER_CARD_INSTANCE(player, card).number_of_targets], selected_target);
    ++PLAYER_CARD_INSTANCE(player, card).number_of_targets;
    return 1;
  }

  return 0;
}

// FUNCTION: MAGIC 0x00551e49
// FUNCTION: SHANDALAR 0x004a008c
int sacrifice_a_land(int player)
{
  target_t target;

  if (C_real_select_target(player, player, player, TARGET_ZONE_IN_PLAY, TYPE_LAND, TYPE_NONE, 0, 0,
                           COLOR_TEST_0, COLOR_TEST_0, -1, ~SUB_WALL, -1, -1, 0, 0, 0,
                           g_text_lines[0], 0, &target))
  {

    if (g_duel_ai_mode_state != 1)
    {
      play_sound_effect(WAV_SACRFICE);
    }
    kill_card(target.player, target.card, KILL_SACRIFICE);

    return 1;
  }
  return 0;
}

// FUNCTION: MAGIC 0x00551ed7
// FUNCTION: SHANDALAR 0x004a011a
int select_target_artifact_and_store(int player, unsigned int preferred_controller, int card)
{
  target_t selected_target;

  if (preferred_controller == -1)
  {
    preferred_controller = 2;
  }

  if (C_real_select_target(player,
                           2,
                           preferred_controller,
                           TARGET_ZONE_IN_PLAY,
                           TYPE_ARTIFACT,
                           TYPE_NONE,
                           0,
                           get_protections_from(player, card),
                           COLOR_TEST_0,
                           COLOR_TEST_0,
                           -1,
                           ~SUB_WALL,
                           -1,
                           -1,
                           0,
                           0,
                           0,
                           g_text_lines[0],
                           1,
                           &selected_target))
  {
    global_card_instances[player][card].targets[global_card_instances[player][card].number_of_targets].player = selected_target.player;
    global_card_instances[player][card].targets[global_card_instances[player][card].number_of_targets].card = selected_target.card;
    ++global_card_instances[player][card].number_of_targets;

    return 1;
  }
  else
    return 0;
}
