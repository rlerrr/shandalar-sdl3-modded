#include "magic/src/game_support.h"
#include "magic/src/global_state.h"
#include "magic/src/global_other.h"
#include "magic/src/global_strings.h"
#include "magic/src/network.h"
#include "magic/src/carddata_generated.h"
#include "deckdll/src/card_db.h"
#include "platform/file_io.h"
#include <stdlib.h>
#include <string.h>

int pick_internal_card_from_list_dialog(char* prompt, int initial_card_id,
                                        int filter);
int card_fireball(int player, int card, event_t event);
int card_lich(int player, int card, event_t event);
int card_spell_blast(int player, int card, event_t event);
int card_zombie_master(int player, int card, event_t event);
int card_detonate(int player, int card, event_t event);
int card_whimsy(int player, int card, event_t event);
int card_winter_blast(int player, int card, event_t event);
int get_displayed_card_damage_on_card(int player, int card);
int get_displayed_card_kill_code(int player, int card);
int is_displayed_card_owned_by_opponent(int player, int card);
state_t get_displayed_card_state(int player, int card);
void show_mana_burn_dialog(int player, int amount);
void get_special_counter_icons_rect(LPRECT rect, RECT* client_rect, int counters);
int mana_producer_sound_on_resolve(int player, int card, event_t event,
                                   color_t color);
int regenerate_card(int player, int card);
void real_put_on_top_of_deck(int player, int internal_card_id);
int show_deck(int player, int* cards, int count, char* title,
              int require_selection, char* prompt);
void request_duel_display_refresh_if_human(int player, int card,
                                           int unknown1, int unknown2);
unsigned int refresh_duel_display_cache(void);
int manalinkeh_draw_a_card(int player);
unsigned int get_urza_land_types_in_play(int player);
int gain_control(int player, int card);
void draw_summoning_sickness_overlay(HDC dc, RECT* rect);
void draw_dying_overlay(HDC dc, RECT* rect);
void draw_card_ui_flag_20_overlay(HDC dc, RECT* rect);
unsigned int get_displayed_card_special_counter_count(int player, int card);
card_id_t get_displayed_card_id(int player, int card);
int get_card_special_counter_icon(card_id_t card_id);
int draw_special_counter_icons(HDC dc, RECT* rect, int icon, int count);
HBITMAP load_pic(char* filename);
extern int g_duel_selected_card_window;
extern int g_life_window_life_window_long_offset;
extern int g_life_window_poison_window_long_offset;
extern int g_life_window_lich_window_long_offset;
typedef int (*manalinkeh_card_function_t)(int player, int card, event_t event);
/*
 * Mok routed both CardTypeFromID(int) and
 * find_internal_card_id_by_csv_id(card_id_t) to the same Manalink entry.
 * Keep separate C names so compilers that distinguish enum parameters do not
 * see two incompatible declarations for one symbol.
 */
int manalinkeh_CardTypeFromID(int csvid)
{
  return find_internal_card_id_by_csv_id((card_id_t)csvid);
}

/*
 * The DLL build used a NASM shim that also loaded ESI for a few original
 * assembly card handlers.  The static target calls C handlers, so a normal
 * typed call is the appropriate equivalent and keeps NASM out of this build.
 */
int call_card_fn_impl(manalinkeh_card_function_t address, void* instance,
                      int player, int card, event_t event)
{
  (void)instance;
  return address(player, card, event);
}

int shandalar_get_poison_counter(int player)
{
  return player_counters[player].poison_;
}

void shandalar_set_poison_counter(int player, unsigned char value)
{
  player_counters[player].poison_ = value;
}

char manalinkeh_legacy_effect_name[51];
extern char* set_legacy_effect_name_addr;
extern char g_card_rules_text_buffer_00708da8[400];

int shandalar_is_legacy_effect_text_buffer(const char *text)
{
  return text == g_card_rules_text_buffer_00708da8;
}

/* Legacy-only state with no direct SDL global owner yet. */
int trace_mode;
int debug_mode;
int singleplayer_mode;
/* Network match numbering has no owner in the SDL single-process duel. */
int game_number;
int must_attack;
int forbid_attack;
int turn_count;
int landsofcolor_controlled[2][8];
int g_saved_ai_modifier;
int g_ai_speculation_spell_fizzled;
int g_deck_rating_multipliers[16];
int opponent_skill;
int LEGACY_EFFECT_CUSTOM = SHANDALAR_CUSTOM_LEGACY_CARD_IID;
int LEGACY_EFFECT_ACTIVATED = SHANDALAR_ACTIVATED_LEGACY_CARD_IID;
int colors_of_lands_in_play[2];
player_bits_t player_bits[2];
int lands_played;
int unknown62BCEC;
int unknown_charge_mana_004d7cb0;
int ldoubleclicked;
int tapped_for_mana_color = -1;
int cant_be_responded_to;
int stack_phase[32];
int mana_paid[7];
int pumpable_power[2];
int pumpable_toughness[2];
int ai_defensive_modifier;
char manalinkeh_cost_scratch[8];
char manalinkeh_mana_scratch[8];
char manalinkeh_surplus_free_mana[8];
char manalinkeh_prevent_normal_graveyard_effect;
char manalinkeh_next_spell_always_castable;
char ability_tooltip_names_exe[17][50];
int g_mimicked_effect_csv_ids[20] = {
  14, 532, 583, 143, 196, 32, 145, 133, 97, 109,
  59, 245, 425, 256, 745, 260, 208, 460, 240, 869
};
int displayed_x_value;
int target_controller;
int target_card;
unsigned char mana_doesnt_drain_from_pool[2][7];
unsigned char types_that_dont_untap;
int (*charge_mana_addr_of_pre_mana_pool)[8];
int* charge_mana_addr_of_x_value;
int (*charge_mana_addr_of_pay_mana_xbugrw)[8];
int* charge_mana_addr_of_max_x_value;
unsigned short cards_drawn_this_turn[2];
int suppress_draw;
char gs_prompt_turnsequence_0[] = "Select attackers";
char gs_prompt_blockerselection_0[] = "Select blockers";
char gs_prompt_specialfephase_5[] = "Draw a card";
char gs_prompt_specialfephase_8[] = "Upkeep";
char gs_prompt_specialfephase_9[] = "Upkeep";
char PROMPT_CHECKFEPHASE_5[] = "Upkeep";
char gs_prompt_specialfephase_6[] = "Spell cast";
char gs_prompt_specialfephase_7[] = "Card tapped";
char gs_prompt_checkfephase_2[] = "Cast %s?";
char gs_prompt_checkfephase_3[] = "Activate %s?";
char gs_localized_cancel[] = "Cancel";
char gs_prompt_action_2[] = "Choose a target for";
char str_Activation[] = "Activation";
char str_Upkeep[] = "Upkeep";
char str_Draw_a_card[64] = "Draw a card";
char gs_prompt_defendwhom_illegal_block[] = "Illegal block.";
char gs_target_error_cannot_target_this_prefixed[] = ",can't target this";
char gs_target_error_destroyed[] = ",destroyed";
char gs_target_error_djinn_or_efreet[] = ",djinn/efreet";
char gs_target_error_elephant_or_mammoth[] = ",elephant/mammoth";
char gs_target_error_summoning_sickness[] = ",summoning sickness";
char gs_prompt_grabmana_1[] = "%s X: %d";
char gs_prompt_grabmana_2[] = "%s X: %d (maximum %d)";
char gs_prompt_grabmana_0[] = "%s";
char global_all_purpose_buffer[1024];

extern card_data_t global_cards_data[SHANDALAR_CARD_DATA_COUNT];
extern card_ptr_t global_raw_cards_storage[];
extern int global_available_slots;

card_data_t** shandalar_cards_at_7c7000(void)
{
  static card_data_t* pointers[SHANDALAR_CARD_DATA_COUNT];
  static int initialized;
  int index;

  if (!initialized)
  {
    for (index = 0; index < SHANDALAR_CARD_DATA_COUNT; ++index)
      pointers[index] = &global_cards_data[index];
    initialized = 1;
  }
  return pointers;
}

card_ptr_t** shandalar_cards_ptr(void)
{
  static card_ptr_t* pointers[SHANDALAR_CARD_DATABASE_CAPACITY];
  static int initialized;
  int index;

  if (!initialized)
  {
    for (index = 0; index < SHANDALAR_CARD_DATABASE_CAPACITY; ++index)
      pointers[index] = &global_raw_cards_storage[index];
    initialized = 1;
  }
  return pointers;
}

int** shandalar_deck_ptr(void)
{
  static int* pointers[2] = {global_library[0], global_library[1]};
  return pointers;
}

int** shandalar_graveyard_ptr(void)
{
  static int* pointers[2] = {global_graveyard_slots[0], global_graveyard_slots[1]};
  return pointers;
}

int** shandalar_rfg_ptr(void)
{
  static int* pointers[2] = {global_exile[0], global_exile[1]};
  return pointers;
}

/*
 * Small adapters for Manalink references that point inside larger MOK
 * objects.  Keeping these as field accesses avoids manufacturing linker
 * aliases for interior addresses.
 */
int* shandalar_damage_taken_this_turn(void)
{
  return g_duel_summary.player_damage_totals;
}

int* shandalar_network_result(void)
{
  return &g_network_result_packet.result;
}

unsigned char* shandalar_network_packet_type(void)
{
  return (unsigned char*)&g_network_result_packet.packet_type;
}

int* shandalar_hand_count(void)
{
  return g_duel_summary.hand_counts;
}

int* shandalar_creature_count(void)
{
  return g_duel_summary.creature_counts;
}

int* shandalar_artifact_count(void)
{
  return g_duel_summary.artifact_counts;
}

int* shandalar_enchantment_count(void)
{
  return g_duel_summary.enchantment_counts;
}

int* shandalar_creatures_dead_this_turn(void)
{
  return &g_duel_summary.creatures_died;
}

int shandalar_is_tapped(int player, int card)
{
  return (PLAYER_CARD_INSTANCE(player, card).state & STATE_TAPPED) != 0;
}

int shandalar_choose_a_color(int player, const char* prompt,
                             int use_color_names_instead_of_land,
                             int ai_choice, int available_colors)
{
  return choose_a_color(player, prompt, use_color_names_instead_of_land,
                        ai_choice, available_colors);
}

int shandalar_set_centerwindow_txt(const char* text)
{
  display_duel_prompt_text((char*)text);
  return 0;
}

void shandalar_real_gain_life(int player, int amount)
{
  gain_life(player, amount);
}

int shandalar_choose_a_card(const char* prompt, int initial_card_id, int filter)
{
  return pick_internal_card_from_list_dialog((char*)prompt, initial_card_id,
                                              filter);
}

int shandalar_record_ai_action_selection(void)
{
  record_ai_action_selection();
  return 0;
}

int shandalar_replay_ai_action_selection(void)
{
  replay_ai_action_selection();
  return 0;
}

char* shandalar_opponent_name(void)
{
  static char name[100];
  copy_opponent_name_prefix(name);
  return name;
}

int* shandalar_option_layout(void)
{
  return &g_duel_interface_options.layout;
}

int* shandalar_option_show_id_tags(void)
{
  return &g_duel_interface_options.show_id_tags_on_cards;
}

int* shandalar_option_expand_fullcard_text(void)
{
  return &g_duel_interface_options.expand_text_box_on_big_card;
}

int* shandalar_target_pair_packet_value(void)
{
  return &g_target_pair_network_packet.target.player;
}

char* shandalar_target_pair_packet_type(void)
{
  return &g_target_pair_network_packet.packet_type;
}

static void rogue_csv_field(char **cursor, char *output, size_t capacity)
{
  char *p = *cursor;
  int quoted = *p == '"';
  size_t used = 0;
  if (quoted) ++p;
  while (*p && *p != '\r' && *p != '\n') {
    char c = *p++;
    if (quoted && c == '"') {
      if (*p == '"') ++p;
      else { if (*p == ',') ++p; break; }
    } else if (!quoted && c == ',') break;
    if (used + 1 < capacity) output[used++] = c;
  }
  output[used] = 0;
  *cursor = p;
}

int shandalar_select_random_rogue(int difficulty)
{
  typedef struct { int index, difficulty; } RogueChoice;
  SDL_IOStream *file = shandalar_open_file_io("Rogues.csv", "rb");
  RogueChoice *choices = NULL;
  int count = 0, preferred = 0, index = 0, capacity = 0;
  char line[2048];
  if (file) {
    shandalar_io_gets(line, sizeof(line), file); /* CSV column header. */
    while (shandalar_io_gets(line, sizeof(line), file)) {
      char name[200], level[32], quote[300], art[268], *cursor = line;
      rogue_csv_field(&cursor, name, sizeof(name));
      rogue_csv_field(&cursor, level, sizeof(level));
      rogue_csv_field(&cursor, quote, sizeof(quote));
      rogue_csv_field(&cursor, art, sizeof(art));
      if (name[0] && art[0]) {
        if (count == capacity) {
          capacity = capacity ? capacity * 2 : 32;
          RogueChoice *grown = realloc(choices, capacity * sizeof(*choices));
          if (!grown) abort();
          choices = grown;
        }
        choices[count].index = index;
        choices[count].difficulty = atoi(level);
        preferred += choices[count].difficulty == difficulty;
        ++count;
      }
      ++index;
    }
    shandalar_io_close(file);
  }
  if (!count) {
    free(choices);
    fprintf(stderr, "No valid opponents defined in Rogues.csv.\n");
    abort();
  }
  int choice = internal_rand(preferred ? preferred : count), result = -1;
  for (int i = 0; i < count; ++i) {
    if ((!preferred || choices[i].difficulty == difficulty) && choice-- == 0) {
      result = choices[i].index;
      break;
    }
  }
  free(choices);
  return result;
}

int* shandalar_ante_setting(void)
{
  return &g_gauntlet_options.ante;
}

int* shandalar_option_show_power_toughness(void)
{
  return &g_duel_interface_options.show_power_toughness_on_cards;
}

int* shandalar_option_show_abilities(void)
{
  return &g_duel_interface_options.show_abilities_on_cards;
}

int* shandalar_option_show_all_summoning_sickness(void)
{
  return &g_duel_interface_options.show_all_cards_summon_sickness;
}

char (*shandalar_phase_stoppers(void))[38]
{
  return (char (*)[38])g_duel_phase_stop_settings;
}

void* shandalar_handclass_player(void)
{
  return g_duel_full_card_window_hwnd;
}

void* shandalar_handclass_opponent(void)
{
  return g_duel_life_window_hwnd;
}

void* shandalar_lifeclass_player(void)
{
  return g_duel_life_status_window_1_hwnd;
}

int* shandalar_hand_window_vertical_offset(void)
{
  return &g_duel_selected_card_window;
}

int* shandalar_opponent_hand_update_flag(void)
{
  return &g_duel_surface_reset_state;
}

int* shandalar_lifeclass_offset_life(void)
{
  return &g_life_window_life_window_long_offset;
}

int* shandalar_lifeclass_offset_counters(void)
{
  return &g_life_window_poison_window_long_offset;
}

int* shandalar_lifeclass_offset_liched(void)
{
  return &g_life_window_lich_window_long_offset;
}

int shandalar_get_displayed_damage(unsigned int player, unsigned int card)
{
  return get_displayed_card_damage_on_card((int)player, (int)card);
}

int shandalar_get_displayed_kill_code(unsigned int player, unsigned int card)
{
  return get_displayed_card_kill_code((int)player, (int)card);
}

int shandalar_get_displayed_owned_by_opponent(int player, int card)
{
  return is_displayed_card_owned_by_opponent(player, card);
}

state_t shandalar_get_displayed_state(unsigned int player, unsigned int card)
{
  return get_displayed_card_state((int)player, (int)card);
}

void shandalar_display_mana_burn_dialog(int player, int amount)
{
  show_mana_burn_dialog(player, amount);
}

void shandalar_copy_special_counter_rect(RECT* rect, RECT* client_rect,
                                         int counters)
{
  get_special_counter_icons_rect(rect, client_rect, counters);
}

int shandalar_mana_producer_fixed(int player, int card, event_t event,
                                  int color)
{
  return mana_producer_sound_on_resolve(player, card, event, (color_t)color);
}

int shandalar_regenerate_target(int player, int card)
{
  return regenerate_card(player, card);
}

int shandalar_raw_put_iid_on_top_of_deck(int player, int internal_card_id)
{
  real_put_on_top_of_deck(player, internal_card_id);
  return 1;
}

int shandalar_show_deck_exe(int player, const int* cards, int count,
                            const char* title, int require_selection,
                            const char* done_label)
{
  return show_deck(player, (int*)cards, count, (char*)title,
                   require_selection, (char*)done_label);
}

void shandalar_refresh_unless_ai_speculating(void)
{
  if (g_duel_ai_mode_state != 1 && g_ai_speculation_depth <= 0)
    request_duel_display_refresh_if_human(-1, -1, 0, 0);
}

int shandalar_redisplay_all(int unused_player, int unused_card)
{
  (void)unused_player;
  (void)unused_card;
  if (g_duel_ai_mode_state == 1 || g_ai_speculation_depth > 0)
    return 0;
  return (int)refresh_duel_display_cache();
}

void shandalar_real_lose_the_game(int player)
{
  if (player < 0)
  {
    g_life[0] = -99;
    g_life[1] = -99;
  }
  else
  {
    g_life[player] = -99;
  }
}

int shandalar_draw_cards(int player, int amount)
{
  int index;
  int result;

  result = -1;
  for (index = 0; index < amount; ++index)
    result = manalinkeh_draw_a_card(player);
  return result;
}

int shandalar_urzatron_parts(int player)
{
  return (int)get_urza_land_types_in_play(player);
}

int shandalar_transfer_control(int player, int card)
{
  return gain_control(player, card);
}

void shandalar_draw_summoning_overlay(HDC dc, RECT* rect)
{
  draw_summoning_sickness_overlay(dc, rect);
}

void shandalar_draw_dying_overlay(HDC dc, RECT* rect)
{
  draw_dying_overlay(dc, rect);
}

int shandalar_draw_oublietted_overlay(HDC dc, RECT* rect)
{
  draw_card_ui_flag_20_overlay(dc, rect);
  return 0;
}

void shandalar_draw_special_counters(HDC dc, const RECT* rect,
                                      int player, int card)
{
  unsigned int count;
  card_id_t card_id;

  count = get_displayed_card_special_counter_count(player, card);
  if (count == 0)
    return;

  card_id = get_displayed_card_id(player, card);
  if (card_id == -1)
    return;

  draw_special_counter_icons(dc, (RECT*)rect,
                             get_card_special_counter_icon(card_id),
                             (int)count);
}

HBITMAP shandalar_read_graphics_file(const char* path)
{
  return load_pic((char*)path);
}

void shandalar_redraw_libraries(void)
{
  SendMessageA(g_duel_main_window_hwnd, 0x432, 0, 0);
  SendMessageA(g_duel_status_window_hwnd, 0x432, 0, 0);
}

const char* shandalar_legacy_title(int card_id)
{
  const char* title;

  if (card_id < 0 || card_id >= global_available_slots)
    return "";
  title = global_legacy_names[card_id].legacy_title;
  return title != NULL ? title : "";
}

const char* shandalar_effect_title(int card_id)
{
  const char* title;

  if (card_id < 0 || card_id >= global_available_slots)
    return "";
  title = global_legacy_names[card_id].effect_title;
  return title != NULL ? title : "";
}

const char* shandalar_hunting_subtype_name(int subtype)
{
  if (subtype < 0 || subtype >= 212)
    return "";
  return gs_hunting_subtype_name_00926930[subtype];
}

/*
 * Decide whether the declared fixed and flexible mana can satisfy the seven
 * simultaneous MOK cost buckets.  Treating each color independently would
 * double-count flexible mana, so use a small integral max-flow network.
 */
int shandalar_check_mana_multi(int player)
{
  enum { MAX_GROUPS = 57, REQUIREMENTS = 7, MAX_NODES = 70 };
  int capacity[MAX_NODES][MAX_NODES];
  int parent[MAX_NODES];
  int queue[MAX_NODES];
  int group_capacity[MAX_GROUPS];
  unsigned int group_mask[MAX_GROUPS];
  unsigned char group_can_pay_generic[MAX_GROUPS];
  int source;
  int group_base;
  int requirement_base;
  int sink;
  int nodes;
  int groups;
  int color;
  int slot;
  int conversion;
  int source_color;
  int produced_color;
  int amount;
  int total_cost;
  int head;
  int tail;
  int node;
  int next;
  int path_capacity;
  int flow;
  int result;

  memset(capacity, 0, sizeof(capacity));
  memset(group_capacity, 0, sizeof(group_capacity));
  memset(group_mask, 0, sizeof(group_mask));
  memset(group_can_pay_generic, 0, sizeof(group_can_pay_generic));

  groups = 0;
  for (color = COLOR_COLORLESS; color <= COLOR_ARTIFACT; ++color)
  {
    amount = g_raw_mana_available[player][color] +
             g_special_mana_pool[player][color];
    if (amount > 0)
    {
      group_capacity[groups] = amount;
      if (color != COLOR_COLORLESS)
        group_mask[groups] = 1U << color;
      group_can_pay_generic[groups] = color != COLOR_ARTIFACT;

      for (slot = 0; slot < 10 &&
                     g_mana_color_conversions[player][slot] != -1; ++slot)
      {
        conversion = g_mana_color_conversions[player][slot];
        source_color = (unsigned short)conversion;
        produced_color = ((unsigned int)conversion >> 16) & 0xffff;
        if (source_color == color && produced_color < REQUIREMENTS)
          group_mask[groups] |= 1U << produced_color;
      }
      ++groups;
    }
  }

  for (slot = 0; slot < 50 && g_raw_mana_available_hex[player][slot] != -1;
       ++slot)
  {
    conversion = g_raw_mana_available_hex[player][slot];
    amount = (int)((unsigned int)conversion >> 16);
    if (amount > 0)
    {
      group_capacity[groups] = amount;
      group_mask[groups] = (unsigned int)conversion & 0x7f;
      group_can_pay_generic[groups] = 1;
      ++groups;
    }
  }

  source = 0;
  group_base = 1;
  requirement_base = group_base + groups;
  sink = requirement_base + REQUIREMENTS;
  nodes = sink + 1;
  total_cost = 0;

  for (slot = 0; slot < groups; ++slot)
  {
    capacity[source][group_base + slot] = group_capacity[slot];
    for (color = COLOR_BLACK; color <= COLOR_ARTIFACT; ++color)
    {
      if ((group_mask[slot] & (1U << color)) != 0)
        capacity[group_base + slot][requirement_base + color] =
            group_capacity[slot];
    }
    if (group_can_pay_generic[slot])
      capacity[group_base + slot][requirement_base + COLOR_COLORLESS] =
          group_capacity[slot];
  }

  for (color = COLOR_COLORLESS; color <= COLOR_ARTIFACT; ++color)
  {
    amount = (unsigned char)manalinkeh_mana_scratch[color];
    capacity[requirement_base + color][sink] = amount;
    total_cost += amount;
  }

  flow = 0;
  for (;;)
  {
    for (node = 0; node < nodes; ++node)
      parent[node] = -1;
    parent[source] = source;
    queue[0] = source;
    head = 0;
    tail = 1;

    while (head < tail && parent[sink] == -1)
    {
      node = queue[head++];
      for (next = 0; next < nodes; ++next)
      {
        if (parent[next] == -1 && capacity[node][next] > 0)
        {
          parent[next] = node;
          queue[tail++] = next;
        }
      }
    }

    if (parent[sink] == -1)
      break;

    path_capacity = 0x7fffffff;
    for (node = sink; node != source; node = parent[node])
    {
      if (capacity[parent[node]][node] < path_capacity)
        path_capacity = capacity[parent[node]][node];
    }
    for (node = sink; node != source; node = parent[node])
    {
      capacity[parent[node]][node] -= path_capacity;
      capacity[node][parent[node]] += path_capacity;
    }
    flow += path_capacity;
  }

  result = flow == total_cost;
  memset(manalinkeh_mana_scratch, 0, sizeof(manalinkeh_mana_scratch));
  return result;
}

int shandalar_card_fireball(int player, int card, event_t event)
{
  return card_fireball(player, card, event);
}

int shandalar_card_lich(int player, int card, event_t event)
{
  return card_lich(player, card, event);
}

int shandalar_card_spell_blast(int player, int card, event_t event)
{
  return card_spell_blast(player, card, event);
}

int shandalar_card_zombie_master(int player, int card, event_t event)
{
  return card_zombie_master(player, card, event);
}

int shandalar_card_detonate(int player, int card, event_t event)
{
  return card_detonate(player, card, event);
}

int shandalar_card_whimsy(int player, int card, event_t event)
{
  return card_whimsy(player, card, event);
}

int shandalar_card_winter_blast(int player, int card, event_t event)
{
  return card_winter_blast(player, card, event);
}

int get_usertime_of_current_thread_in_ms(void)
{
  return get_duel_thread_time_ms();
}

int shandalar_add_card_to_hand(int player, int internal_card_id)
{
  return add_card_to_hand(player, internal_card_id);
}

int shandalar_create_legacy_effect(int player, int card, int legacy_iid,
                                   int target_player, int target_card)
{
  return create_legacy_effect(player, card, legacy_iid, target_player, target_card);
}

const char* shandalar_legacy_damage_text(int card_id)
{
  if (card_id < 0 || card_id >= global_available_slots) return "";
  const char *text = global_legacy_names[card_id].damage_text;
  return text ? text : "";
}
const char* shandalar_legacy_text(int card_id)
{
  if (card_id < 0 || card_id >= global_available_slots) return "";
  const char *text = global_legacy_names[card_id].legacy_text;
  return text ? text : "";
}
const char* shandalar_effect_text(int card_id)
{
  if (card_id < 0 || card_id >= global_available_slots) return "";
  const char *text = global_legacy_names[card_id].effect_text;
  return text ? text : "";
}

int* shandalar_player_damage_totals(void) { return g_duel_summary.player_damage_totals; }
