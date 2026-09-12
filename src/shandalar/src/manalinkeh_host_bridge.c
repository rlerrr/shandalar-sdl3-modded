#include "magic/src/game_support.h"
#include "magic/src/global_state.h"
#include "magic/src/global_other.h"
#include "magic/src/network.h"

int pick_internal_card_from_list_dialog(char* prompt, int initial_card_id,
                                        int filter);
int card_fireball(int player, int card, event_t event);
int card_lich(int player, int card, event_t event);
int card_spell_blast(int player, int card, event_t event);
int card_zombie_master(int player, int card, event_t event);
typedef int (*manalinkeh_card_function_t)(int player, int card, event_t event);

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

#undef resize_duel_hand_window
void manalinkeh_update_hand_window(HWND hwnd);

void resize_duel_hand_window(void* hwnd)
{
  manalinkeh_update_hand_window((HWND)hwnd);
}

char manalinkeh_legacy_effect_name[51];
char* set_legacy_effect_name_addr = manalinkeh_legacy_effect_name;

/* Legacy-only state with no direct SDL global owner yet. */
int trace_mode;
int must_attack;
int forbid_attack;
int turn_count;
int landsofcolor_controlled[2][8];
int g_saved_ai_modifier;
int g_ai_combat_blocking_card_indices[151];
int g_ai_speculation_spell_fizzled;
int g_deck_rating_multipliers[16];
int opponent_skill;
int LEGACY_EFFECT_CUSTOM;
int LEGACY_EFFECT_ACTIVATED;
int available_slots;
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
int (*charge_mana_addr_of_pre_mana_pool)[8];
int* charge_mana_addr_of_x_value;
int (*charge_mana_addr_of_pay_mana_xbugrw)[8];
int* charge_mana_addr_of_max_x_value;
void* hwnd_HandClass_opponent;
unsigned short cards_drawn_this_turn[2];
int suppress_draw;
char gs_prompt_turnsequence_0[] = "Select attackers";
char gs_prompt_blockerselection_0[] = "Select blockers";
char gs_prompt_specialfephase_5[] = "Draw a card";
char gs_prompt_specialfephase_8[] = "Upkeep";
char gs_prompt_specialfephase_9[] = "Upkeep";
char PROMPT_CHECKFEPHASE_5[] = "Upkeep";
char gs_prompt_grabmana_1[] = "%s X: %d";
char gs_prompt_grabmana_2[] = "%s X: %d (maximum %d)";
char gs_prompt_grabmana_0[] = "%s";
char global_all_purpose_buffer[1024];

extern card_data_t global_cards_data[722];
extern card_ptr_t global_raw_cards_storage[2000];

card_data_t** shandalar_cards_at_7c7000(void)
{
  static card_data_t* pointers[722];
  static int initialized;
  int index;

  if (!initialized)
  {
    for (index = 0; index < 722; ++index)
      pointers[index] = &global_cards_data[index];
    initialized = 1;
  }
  return pointers;
}

card_ptr_t** shandalar_cards_ptr(void)
{
  static card_ptr_t* pointers[2000];
  static int initialized;
  int index;

  if (!initialized)
  {
    for (index = 0; index < 2000; ++index)
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
