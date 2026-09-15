#include "magic/src/game_support.h"
#include "magic/src/global_state.h"
#include "drawcardlib/src/autoproto.h"

int draw_standard_counters(HDC dc, RECT* rect, int power, int m1m1,
                           int p1p1, int loyalty, int charge);
int draw_masked_bitmap_left_half_to_rect(HDC dc, RECT* rect, HBITMAP bitmap);
void get_displayed_card_standard_counters(int player, int card,
                                          unsigned int* counter_power,
                                          unsigned int* counter_m1m1,
                                          unsigned int* counter_p1p1,
                                          unsigned int* counter_p0p1,
                                          unsigned int* counter_p1p0);
int manalinkeh_dispatch_trigger_to_one_card(int player, int card,
                                             event_t event);

int manalinkeh_hostabi_dispatch_card_trigger_event(int player, int card,
                                                    event_t event, int extra)
{
  (void)extra;
  return manalinkeh_dispatch_trigger_to_one_card(player, card, event);
}

void manalinkeh_hostabi_TENTATIVE_reassess_all_cards(void)
{
  TENTATIVE_reassess_all_cards(0, 0xff);
}

int manalinkeh_hostabi_damage_player(int target_player, int amount,
                                     int source_player, int source_card)
{
  return damage_creature(target_player, -1, amount, source_player, source_card);
}

void manalinkeh_hostabi_process_damage_prevention(void)
{
  process_damage_prevention(g_current_turn);
}

void manalinkeh_hostabi_request_duel_display_refresh(void)
{
  if (g_duel_ai_mode_state != 1 && g_ai_speculation_depth <= 0)
    request_duel_display_refresh(-1, -1, 0, 0);
}

int manalinkeh_hostabi_should_skip_phase(void)
{
  return should_skip_phase(g_current_turn);
}

int manalinkeh_hostabi_show_card_preview_if_human(int internal_card_id)
{
  show_card_preview_if_human((unsigned int)internal_card_id, 0, 0, 1);
  return 0;
}

int manalinkeh_hostabi_show_duel_card_preview(int internal_card_id, int player,
                                              int card)
{
  show_duel_card_preview((unsigned int)internal_card_id, player, card, 0, 0, 1);
  return 0;
}

int manalinkeh_hostabi_DrawCardBack(HDC dc, const RECT* rect, int mode)
{
  (void)mode;
  DrawCardBack(dc, (RECT*)rect);
  return 0;
}

int manalinkeh_hostabi_DrawSmallCard(HDC dc, const RECT* rect,
                                     const card_ptr_t* card, int version,
                                     int mode, int player, int card_index)
{
  (void)player;
  (void)card_index;
  DrawSmallCard(dc, (RECT*)rect, (card_ptr_t*)card, version, mode);
  return 0;
}

int manalinkeh_hostabi_DrawSmallCardTitle(HDC dc, const RECT* rect,
                                          const char* title, int color,
                                          int controlled_by_owner,
                                          const card_ptr_t* card, int player,
                                          int card_index)
{
  (void)card;
  (void)player;
  (void)card_index;
  DrawSmallCardTitle(dc, (RECT*)rect, (char*)title, color,
                     controlled_by_owner);
  return 0;
}

void manalinkeh_hostabi_draw_masked_bitmap_left_half_to_rect(HDC dc,
                                                             RECT* rect,
                                                             HBITMAP bitmap)
{
  (void)draw_masked_bitmap_left_half_to_rect(dc, rect, bitmap);
}

void manalinkeh_hostabi_draw_standard_counters(HDC dc, const RECT* rect,
                                               int player, int card)
{
  unsigned int power;
  unsigned int m1m1;
  unsigned int p1p1;
  unsigned int p0p1;
  unsigned int p1p0;

  get_displayed_card_standard_counters(player, card, &power, &m1m1, &p1p1,
                                       &p0p1, &p1p0);
  (void)draw_standard_counters(dc, (RECT*)rect, (int)power, (int)m1m1,
                               (int)p1p1, (int)p0p1, (int)p1p0);
}
