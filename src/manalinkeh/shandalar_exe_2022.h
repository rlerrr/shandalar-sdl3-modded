#ifndef SHANDALAR_EXE_2022_H
#define SHANDALAR_EXE_2022_H

/* Typed source references resolved from the MOK annotations. */



BOOL DrawBitmapToRect(HDC dst_dc, const RECT *dst_rect, HBITMAP bitmap);

extern int ai_attacker_blocker_counts[16];
extern int ai_candidate_block_assignments[16];
void apply_displayed_card_hack_words(int player, int card, char *text);
void apply_displayed_card_sleight_words(int player, int card, char *text);



int begin_mana_payment_record(void);
int send_arena_network_choice(int player, int choice);

int charge_mana_w_global_cost_mod(int player, int card, int color, int amount);
extern int combat_damage_attacker_abilities[16];
extern int combat_damage_attacker_cards[16];
extern int combat_damage_attacker_count;
extern int combat_damage_attacker_damage[16];
extern int combat_damage_attacker_toughness[16];
extern int combat_damage_blocker_abilities[16];
extern int combat_damage_blocker_damage[16];
extern int combat_damage_blocker_toughness[16];
extern int combat_eval_abilities;
extern int combat_eval_power;
extern int combat_eval_toughness;



int count_colored_cards_in_hand(int player);
extern int defending_cop_available_mana;
extern int defending_cop_color_mask;
void draw_card_highlight_frame(HDC dc, RECT *rect, int highlighted);
void draw_card_list_count(HDC dc, RECT *rect, int value);

void draw_displayed_card_overlaid_full_card(HDC dc, RECT *rect, int player, int card);
unsigned int draw_displayed_full_card(HDC dc, RECT *rect, card_ptr_t *raw_card, int player, int card,
                                      int draw_mode, int expand_text_box);
void draw_duel_face_window(HDC dc, RECT *rect, int player);
void draw_id_tag(HDC dc, RECT *rect, int player, int card, int enabled);


void draw_owner_draw_button_centered(DRAWITEMSTRUCT *draw_item, HBRUSH brush, HPEN pen1, HPEN pen2, COLORREF color, int draw_focus);

unsigned int draw_target_canttarget(HDC dc, RECT *rect, int is_target, int can_target);
void draw_will_untap_overlay(HDC dc, RECT *rect, unsigned int untap_status);
int end_mana_payment_record(void);
extern unsigned int g_ai_attacker_ability_counts[7];
extern int g_ai_attacker_power[16];
extern int g_ai_attacker_toughness[16];
extern unsigned int g_ai_blocker_ability_counts[7];
extern int g_ai_blocker_damage[16];
extern int g_ai_blocker_power[16];
extern int g_ai_combat_best_score;
extern int g_ai_combat_candidate_cards[16];
extern int g_ai_current_attacker;
extern int g_ai_destroy_if_blocked_attacker_mask;
extern HINSTANCE g_app_instance;
extern int g_card_on_stack;
extern int g_card_on_stack_controller;
extern CRITICAL_SECTION g_card_render_lock;
extern BITMAPINFO g_duel_backbuffer_bmi;
extern int g_duel_generated_internal_card_id_24;
extern char g_duel_message_text[100];
extern int g_duel_state_00791418;
extern int g_duel_state_007a7d78;
extern void *g_shared_offscreen_bits;
extern HDC g_shared_offscreen_dc;
extern HWND g_show_card_list_last_preview_hwnd;
extern int g_showlist_card_count_window_long_offset;
extern int g_showlist_card_csvid_window_long_offset;
extern int g_showlist_card_show_count_window_long_offset;
extern int g_showlist_card_width;
extern int g_showlist_scroll_max;
int get_displayed_card_color_flags(int player, int card);
void get_displayed_card_counter_power_toughness(int player, int card, int *power, int *toughness);



unsigned int get_displayed_card_special_counters(int player, int card);






extern char global_all_purpose_buffer[1024];
extern HWND global_main_hwnd;
extern char gs_illus_00789130[200];
extern char gs_prompt_action_2[];
extern char gs_trying_to_cast_007a79c0[300];
int handle_button_palette_message(HWND hwnd, UINT msg, HWND wparam, LPARAM lparam);
void kill_card(int player, int card, kill_t kill_mode);

int player_has_legal_attacker(int player);

int record_paid_mana(int color, int amount);

void replace_text_token(char *text, char *search, int case_sensitive, char *replace);
void reset_duel_globals(void);

int restore_recorded_mana_to_pool(int player);
void rotate_dib_rect_clockwise(BITMAPINFO *bmi, unsigned char *bits, RECT *rect);

int score_ai_attacker_candidate(int player, int card);
void set_global_base_directory(char *path);


extern int untapped_royal_assassin_count;

extern int g_ai_attacker_scores[16];
extern int g_ai_combat_value_weights[4];
extern HWND g_duel_window_hwnd;
extern int g_duel_dialog_refresh_state, g_duel_tick_adjustment;
extern int g_full_card_card_id_window_long_offset, g_full_card_player_window_long_offset;
extern int g_full_card_card_window_long_offset, g_full_card_counters_window_long_offset;
extern int g_full_card_color_window_long_offset;
extern char gs_multiblock_shadow_blocker_00925e70[];
int* shandalar_option_show_id_tags(void);
int* shandalar_option_expand_fullcard_text(void);
int* shandalar_target_pair_packet_value(void);
char* shandalar_target_pair_packet_type(void);
void draw_card_debug_id_tag(HDC, RECT*, int, int);
int shandalar_big_card_dialog_mode(void);
LRESULT CALLBACK wndproc_MAGICGAME_FullCardClass(HWND, UINT, WPARAM, LPARAM);

extern HWND g_duel_window_hwnd;
extern int g_duel_dialog_refresh_state, g_duel_tick_adjustment;
extern int g_full_card_card_id_window_long_offset, g_full_card_player_window_long_offset;
extern int g_full_card_card_window_long_offset, g_full_card_counters_window_long_offset;
extern int g_full_card_color_window_long_offset;
extern char gs_multiblock_shadow_blocker_00925e70[];
int* shandalar_option_show_id_tags(void);
int* shandalar_option_expand_fullcard_text(void);
int* shandalar_target_pair_packet_value(void);
char* shandalar_target_pair_packet_type(void);
void draw_card_debug_id_tag(HDC, RECT*, int, int);
LRESULT CALLBACK wndproc_MAGICGAME_FullCardClass(HWND, UINT, WPARAM, LPARAM);

extern ai_combat_eval_entry_t ai_combat_eval_table[2][151];
LRESULT CALLBACK wndproc_ShowListCard(HWND, UINT, WPARAM, LPARAM);
extern int g_shandalar_difficulty;
#endif
