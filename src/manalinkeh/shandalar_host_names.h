#ifndef MANALINKEH_SHANDALAR_HOST_NAMES_H
#define MANALINKEH_SHANDALAR_HOST_NAMES_H

#include <windows.h>

#ifdef MANALINKEH_STATIC
/*
 * These imports have legacy prototypes that do not match the surviving host
 * declarations.  Native x86 tolerated the mismatch at the ABI boundary;
 * WebAssembly requires an exact function type.  Keep the imported call shape
 * and route it through typed adapters supplied by the SDL host.
 */
#define DrawCardBack manalinkeh_hostabi_DrawCardBack
#define DrawSmallCard manalinkeh_hostabi_DrawSmallCard
#define DrawSmallCardTitle manalinkeh_hostabi_DrawSmallCardTitle
#define TENTATIVE_reassess_all_cards manalinkeh_hostabi_TENTATIVE_reassess_all_cards
#define damage_player manalinkeh_hostabi_damage_player
#define draw_standard_counters manalinkeh_hostabi_draw_standard_counters
#define draw_masked_bitmap_left_half_to_rect manalinkeh_hostabi_draw_masked_bitmap_left_half_to_rect
#define process_damage_prevention manalinkeh_hostabi_process_damage_prevention
#define request_duel_display_refresh manalinkeh_hostabi_request_duel_display_refresh
#define should_skip_phase manalinkeh_hostabi_should_skip_phase
#define show_card_preview_if_human manalinkeh_hostabi_show_card_preview_if_human
#define show_duel_card_preview manalinkeh_hostabi_show_duel_card_preview
#endif

/* Legacy Manalink spellings for state now owned by the SDL host. */
extern int g_active_cards_count[2];
extern int g_affected_card;
extern int g_affected_card_controller;
extern int g_ai_speculation_depth;
#ifdef MANALINKEH_STATIC
extern int g_duel_ai_mode_state;
#endif
extern int g_ai_trace_enabled;
extern int g_current_phase;
extern int g_current_turn;
extern int g_current_player;
extern int g_battlefield_extra_ability_flags;
extern int g_damage_card_internal_card_id;
extern int g_stack_proxy_internal_card_id;
extern char g_text_lines[300][300];
extern int g_event_player;
extern int g_activation_event_flags;
extern int g_ai_modifier;
extern int g_land_can_be_played;
extern int g_life[2];
extern int g_mana_charge[8];
extern int g_special_mana_pool[2][8];
extern int g_required_mana_color_mask;
extern int g_max_x_value;
extern int g_trigger_condition;
extern int g_trigger_cause;
extern int g_trigger_cause_controller;
extern int g_stack_size;
extern stack_data_t g_stack_data[32];
extern target_t global_stack_cards[32];
extern target_t global_stack_damage_targets[32];
extern int g_raw_mana_available[2][8];
extern int g_x_value;
extern unsigned int g_card_types_in_play[2];
extern csvid_and_numcards g_initial_library[2][200];
extern int g_attacking_card_controller;
extern int g_attacking_card;
extern int g_card_on_stack_controller;
extern int g_card_on_stack;
extern int TENTATIVE_timestamp_player[500];
extern int TENTATIVE_timestamp_card[500];
extern int g_life_gained;
extern int g_attacking_creature_count;
extern int g_duel_extra_turn_player;
extern int g_duel_state_0093a848;
extern char manalinkeh_cost_scratch[6];
extern char manalinkeh_mana_scratch[8];
extern char manalinkeh_surplus_free_mana[8];
extern char manalinkeh_prevent_normal_graveyard_effect;
extern char manalinkeh_next_spell_always_castable;
extern char global_base_directory[];
extern int g_hand_count_adjustment;
extern int deck[500];
extern int g_duel_run_mode;
extern int g_duel_state_00925bf8;
extern int g_creature_power_by_color[2][8];
extern int g_duel_generated_internal_card_id_06;
extern int g_duel_generated_internal_card_id_0a;
extern int g_duel_generated_internal_card_id_0f;
extern int g_raw_mana_available_hex[2][51];
extern int g_control_aura_special_internal_card_id;
extern int g_duel_generated_internal_card_id_2c;
extern int gs_words_to_hack_count_008cd924;
extern int gs_words_to_sleight_count_00789c34;
extern char gs_words_to_hack_00789c80[6][50][100];
extern char gs_words_to_sleight_00781bd0[6][50][100];
extern void calculate_hand_art_offsets(int, int, int, int, int, int,
                                       int*, int*, int*, int*);
extern char gs_illegal_target_why_power_00781aa0[];
extern char gs_illegal_target_why_toughness_007894b0[];
extern char gs_illegal_target_why_damaged_007911b0[];
extern char gs_illegal_target_why_owner_007aacc0[];
extern char gs_illegal_target_why_blocking_007ab160[];
extern char gs_illegal_target_why_attacking_00896540[];
extern char gs_illegal_target_why_type_008a9a00[];
extern char gs_illegal_target_why_player_008b3110[];
extern char gs_illegal_target_why_walls_008b34c0[];
extern char gs_illegal_target_why_can_untap_008b3be0[];
extern char gs_illegal_target_why_attacking_blocking_008cc850[];
extern char gs_illegal_target_why_color_008ce3c0[];
extern char gs_illegal_target_why_cast_resolved_008ce6a0[];
extern char gs_illegal_target_why_subtype_008cea20[];
extern char gs_illegal_target_why_will_untap_008ceb50[];
extern char gs_illegal_target_why_artifact_creature_008ced40[];
extern char gs_illegal_target_why_enchanted_008cf3c0[];
extern char gs_illegal_target_why_cant_target_this_0091a6e0[];
extern char gs_illegal_target_why_abilities_0091a810[];
extern char gs_illegal_target_why_controller_0091bbe0[];
extern char gs_illegal_target_why_spell_0091be40[];
extern char gs_illegal_target_why_attacked_0091bfc0[];
extern char gs_illegal_target_why_target_player_0091c360[];
extern char gs_illegal_target_why_blocked_0091caa0[];
extern char gs_illegal_target_why_where_0091cd00[];
extern char gs_illegal_target_why_tapped_00925080[];
extern char gs_illegal_target_why_casted_00939050[];
extern char gs_illegal_target_why_basic_land_0093d860[];
extern char gs_illegal_target_why_card_type_008cfde0[];
extern char gs_illegal_target_008b3e50[];
extern char gs_illegal_target_with_reason_008ce7e0[];
extern char gs_cuecard_is_a_target_00789c40[];
extern char gs_cuecard_damage_to_player_007a77d0[];
extern char gs_cuecard_phased_007aae00[];
extern char gs_cuecard_summoning_sickness_008a8e00[];
extern char gs_cuecard_not_controlled_by_owner_008a8e40[];
extern char gs_cuecard_cant_target_this_008b4990[];
extern char gs_cuecard_is_a_target_cant_target_again_008cf230[];
extern char gs_cuecard_damage_n_0091b290[];
extern char gs_cuecard_dying_0091c640[];
extern char gs_cuecard_this_card_will_untap_00925040[];
extern HBITMAP g_life_poison_bitmap;
extern card_data_t global_cards_data[];
extern card_instance_t global_card_instances[2][151];
extern card_instance_t global_displayed_card_instances[2][151];
extern int g_graveyard_card_types[2];
extern int g_basiclandtypes_controlled[2][8];
extern int g_event_result;
extern int g_spell_fizzled;
extern int g_draw_card_placeholder_internal_card_id;
extern int global_ante_cards[2][16];
extern int* shandalar_hand_count(void);
extern card_data_t** shandalar_cards_at_7c7000(void);
extern card_ptr_t** shandalar_cards_ptr(void);
extern int** shandalar_deck_ptr(void);
extern int** shandalar_graveyard_ptr(void);
extern int** shandalar_rfg_ptr(void);
extern int* shandalar_creature_count(void);
extern int* shandalar_artifact_count(void);
extern int* shandalar_enchantment_count(void);
extern int* shandalar_creatures_dead_this_turn(void);
extern int shandalar_is_tapped(int player, int card);
extern int shandalar_choose_a_color(int player, const char* prompt,
                                    int use_color_names_instead_of_land,
                                    int ai_choice, int available_colors);
extern int shandalar_set_centerwindow_txt(const char* text);
extern void shandalar_real_gain_life(int player, int amount);
extern int shandalar_choose_a_card(const char* prompt, int initial_card_id,
                                   int filter);
extern int shandalar_record_ai_action_selection(void);
extern int shandalar_replay_ai_action_selection(void);
extern char* shandalar_opponent_name(void);
extern int* shandalar_option_layout(void);
extern int* shandalar_option_show_power_toughness(void);
extern int* shandalar_option_show_abilities(void);
extern int* shandalar_option_show_all_summoning_sickness(void);
extern char (*shandalar_phase_stoppers(void))[38];
extern void* shandalar_handclass_player(void);
extern void* shandalar_handclass_opponent(void);
extern void* shandalar_lifeclass_player(void);
extern int* shandalar_hand_window_vertical_offset(void);
extern int* shandalar_opponent_hand_update_flag(void);
extern int* shandalar_lifeclass_offset_life(void);
extern int* shandalar_lifeclass_offset_counters(void);
extern int* shandalar_lifeclass_offset_liched(void);
extern int* shandalar_network_result(void);
extern unsigned char* shandalar_network_packet_type(void);
extern int shandalar_get_displayed_damage(unsigned int, unsigned int);
extern int shandalar_get_displayed_kill_code(unsigned int, unsigned int);
extern int shandalar_get_displayed_owned_by_opponent(int, int);
extern state_t shandalar_get_displayed_state(unsigned int, unsigned int);
extern void shandalar_display_mana_burn_dialog(int, int);
extern void shandalar_copy_special_counter_rect(RECT*, RECT*, int);
extern int shandalar_mana_producer_fixed(int, int, event_t, int);
extern int shandalar_regenerate_target(int, int);
extern int shandalar_raw_put_iid_on_top_of_deck(int, int);
extern int shandalar_show_deck_exe(int, const int*, int, const char*, int, int);
extern void shandalar_refresh_unless_ai_speculating(void);
extern int shandalar_redisplay_all(int, int);
extern void shandalar_real_lose_the_game(int);
extern int shandalar_draw_cards(int, int);
extern int* shandalar_ante_setting(void);
extern int is_selected_target_already_attached(int, int, int);
extern int shandalar_card_fireball(int player, int card, event_t event);
extern int shandalar_card_lich(int player, int card, event_t event);
extern int shandalar_card_spell_blast(int player, int card, event_t event);
extern int shandalar_card_zombie_master(int player, int card, event_t event);
extern int shandalar_card_detonate(int player, int card, event_t event);
extern int shandalar_card_whimsy(int player, int card, event_t event);
extern int shandalar_card_winter_blast(int player, int card, event_t event);
extern int card_sea_serpent(int player, int card, event_t event);
extern int card_white_knight(int player, int card, event_t event);
extern int resolve_control_aura_conflict(int player, int card);
extern int shandalar_urzatron_parts(int player);
extern int shandalar_transfer_control(int player, int card);
extern void shandalar_draw_summoning_overlay(HDC dc, RECT* rect);
extern void shandalar_draw_dying_overlay(HDC dc, RECT* rect);
extern int shandalar_draw_oublietted_overlay(HDC dc, RECT* rect);
extern void shandalar_draw_special_counters(HDC dc, const RECT* rect,
                                             int player, int card);
extern HBITMAP shandalar_read_graphics_file(const char* path);
extern void shandalar_redraw_libraries(void);
extern const char* shandalar_legacy_title(int card_id);
extern const char* shandalar_effect_title(int card_id);
extern const char* shandalar_hunting_subtype_name(int subtype);
extern int shandalar_check_mana_multi(int player);

#define active_cards_count g_active_cards_count
#define affected_card g_affected_card
#define affected_card_controller g_affected_card_controller
#define ai_is_speculating g_duel_ai_mode_state
#ifdef MANALINKEH_STATIC
#define MANALINKEH_IS_AI_SPECULATING() \
  (g_duel_ai_mode_state == 1 || g_ai_speculation_depth > 0)
#else
#define MANALINKEH_IS_AI_SPECULATING() (ai_is_speculating == 1)
#endif
#define ai_modifier g_ai_modifier
#define current_phase g_current_phase
#define current_turn g_current_player
#define damage_card g_damage_card_internal_card_id
/* Share the host's activation proxy ID so resolution, cancellation, and the
 * duel UI recognize and remove the copies created by Manalink. */
#define activation_card g_stack_proxy_internal_card_id
#define cards_data global_cards_data
#define raw_mana_available_hex g_raw_mana_available_hex
#define text_lines g_text_lines
#define wndproc_CardClass wndproc_MAGICGAME_CardClass
#define cards_at_7c7000 (shandalar_cards_at_7c7000())
#define cards_ptr (shandalar_cards_ptr())
#define raw_card_instances global_card_instances
#define raw_displayed_card_instances global_displayed_card_instances
#define deck_ptr (shandalar_deck_ptr())
#define graveyard_ptr_mutable (shandalar_graveyard_ptr())
#define rfg_ptr (shandalar_rfg_ptr())
#define hand_count (shandalar_hand_count())
#define creature_count (shandalar_creature_count())
#define artifact_count (shandalar_artifact_count())
#define enchantment_count (shandalar_enchantment_count())
#define event_flags g_battlefield_extra_ability_flags
#define event_result g_event_result
#define cancel g_spell_fizzled
#define spell_fizzled g_spell_fizzled
#define land_can_be_played g_land_can_be_played
#define life g_life
#define max_x_value g_max_x_value
#define PAY_MANA_COLORLESS g_mana_charge[0]
#define PAY_MANA_BLACK g_mana_charge[1]
#define PAY_MANA_BLUE g_mana_charge[2]
#define PAY_MANA_GREEN g_mana_charge[3]
#define PAY_MANA_RED g_mana_charge[4]
#define PAY_MANA_WHITE g_mana_charge[5]
#define PAY_MANA_ARTIFACT g_mana_charge[6]
#define pay_mana_xbugrwaU g_mana_charge
#define mana_pool g_special_mana_pool
#define needed_mana_colors g_required_mana_color_mask
#define raw_mana_available g_raw_mana_available
#define types_of_cards_in_graveyard g_graveyard_card_types
#define basiclandtypes_controlled g_basiclandtypes_controlled
#define iid_draw_a_card g_draw_card_placeholder_internal_card_id
#define ante_cards global_ante_cards
/* MOK 0x00737e1c: trigger owner, despite the surviving host's legacy name.
 * g_event_player (0x0060a534) is separate response/activation context. */
#define reason_for_trigger_controller g_current_turn
#define start_usertime_of_current_thread_in_ms g_ai_trace_enabled
#define trigger_condition g_trigger_condition
#define trigger_cause g_trigger_cause
#define trigger_cause_controller g_trigger_cause_controller
#define stack_size g_stack_size
#define stack_data g_stack_data
#define stack_cards global_stack_cards
#define stack_damage_targets global_stack_damage_targets
#define types_of_cards_on_bf g_card_types_in_play
#define initial_library g_initial_library
#define attacking_card_controller g_attacking_card_controller
#define attacking_card g_attacking_card
#define card_on_stack_controller g_card_on_stack_controller
#define card_on_stack g_card_on_stack
#define timestamp_player TENTATIVE_timestamp_player
#define timestamp_card TENTATIVE_timestamp_card
#define life_gained g_life_gained
#define creatures_dead_this_turn (*shandalar_creatures_dead_this_turn())
#define number_of_attackers_declared g_attacking_creature_count
#define time_walk_flag g_duel_extra_turn_player
#define dword_4EF1B8 g_duel_state_0093a848
#define opponent_name (shandalar_opponent_name())
#define option_Layout (*shandalar_option_layout())
#define option_ShowPowerToughnessOnCards (*shandalar_option_show_power_toughness())
#define option_ShowAbilitiesOnCards (*shandalar_option_show_abilities())
#define option_ShowAllCardsSummonSickness (*shandalar_option_show_all_summoning_sickness())
#define option_PhaseStoppers (shandalar_phase_stoppers())
#define hwnd_HandClass_player (shandalar_handclass_player())
#define hwnd_HandClass_opponent (shandalar_handclass_opponent())
#define hwnd_LifeClass_player (shandalar_lifeclass_player())
#define g_hand_window_card_vertical_offset (*shandalar_hand_window_vertical_offset())
#define g_opponent_hand_window_update_flag (*shandalar_opponent_hand_update_flag())
#define lifeclass_offset_life (*shandalar_lifeclass_offset_life())
#define lifeclass_offset_player_counters (*shandalar_lifeclass_offset_counters())
#define lifeclass_offset_liched (*shandalar_lifeclass_offset_liched())
#define dword_60A4B0 g_hand_count_adjustment
#define dblword_628C10 (*shandalar_network_result())
#define byte_628C0C (*shandalar_network_packet_type())
#define path_base global_base_directory
#define byte_55CF20 manalinkeh_surplus_free_mana
#define calculate_hand_window_layout calculate_hand_art_offsets
#define Deck deck
#define game_type g_duel_run_mode
#define gauntlet_round g_duel_state_00925bf8
#define ante (*shandalar_ante_setting())
#define total_power_of_creatures_by_color g_creature_power_by_color
#define another_copy_attached is_selected_target_already_attached
#define LEGACY_EFFECT_GENERIC g_duel_generated_internal_card_id_0f
#define LEGACY_EFFECT_STONING g_duel_generated_internal_card_id_06
#define LEGACY_EFFECT_ASWAN g_duel_generated_internal_card_id_0a
#define LEGACY_EFFECT_ALADDIN g_control_aura_special_internal_card_id
#define iid_legacy_data_card g_damage_card_internal_card_id
#define iid_legacy_swap_power_toughness g_duel_generated_internal_card_id_2c
#define num_words_to_hack gs_words_to_hack_count_008cd924
#define num_words_to_sleight gs_words_to_sleight_count_00789c34
#define strs_words_to_hack gs_words_to_hack_00789c80
#define strs_words_to_sleight gs_words_to_sleight_00781bd0
#define get_displayed_damage_on_card shandalar_get_displayed_damage
#define get_displayed_kill_code shandalar_get_displayed_kill_code
#define get_displayed_owned_by_opponent shandalar_get_displayed_owned_by_opponent
#define get_displayed_state shandalar_get_displayed_state
#define display_mana_burn_dialog shandalar_display_mana_burn_dialog
#define copy_special_counter_rect shandalar_copy_special_counter_rect
#define mana_producer_fixed shandalar_mana_producer_fixed
#define regenerate_target_exe shandalar_regenerate_target
#define raw_put_iid_on_top_of_deck shandalar_raw_put_iid_on_top_of_deck
#define show_deck_exe shandalar_show_deck_exe
#define call_sub_437E20_unless_ai_is_speculating shandalar_refresh_unless_ai_speculating
#define TENTATIVE_redisplay_all shandalar_redisplay_all
#define real_lose_the_game shandalar_real_lose_the_game
#define draw_cards_exe shandalar_draw_cards
#define urzatron_parts shandalar_urzatron_parts
#define islandhome card_sea_serpent
#define pro_black card_white_knight
#define u4585f0 resolve_control_aura_conflict
#define u4b6860 shandalar_transfer_control
#define blt_summon_pic shandalar_draw_summoning_overlay
#define blt_dying_pic shandalar_draw_dying_overlay
#define draw_oublietted_effect shandalar_draw_oublietted_overlay
#define draw_special_counters shandalar_draw_special_counters
#define read_graphics_file2 shandalar_read_graphics_file
#define redraw_libraries shandalar_redraw_libraries
#define bmp_poison_pic g_life_poison_bitmap
#define check_mana_multi shandalar_check_mana_multi
#define x_value g_x_value
#define choose_a_color_exe shandalar_choose_a_color
#define is_tapped shandalar_is_tapped
#define pay_mana_maximally_satisfied is_mana_cost_paid
#define try_to_pay_for_mana_by_autotapping activate_mana_sources_for_payment
#define put_card_on_stack2 resolve_cast_card
#define damage_creature_or_player deal_damage_to_selected_target
#define set_centerwindow_txt shandalar_set_centerwindow_txt
#define real_gain_life shandalar_real_gain_life
#define choose_a_card shandalar_choose_a_card
#define sub_4378E0 run_target_selection_modal
#define sub_45CEC0 show_spell_window_for_target_selection
#define sub_45CF00 restore_spell_window_after_target_selection
#define sub_47E980 has_pending_damage
#define sub_47EF10 is_basic_land
#define sub_47EF50 basic_land_matches_type_index
#define sub_498F20 shandalar_record_ai_action_selection
#define sub_499050 shandalar_replay_ai_action_selection
#define card_fireball_exe shandalar_card_fireball
#define card_lich_exe shandalar_card_lich
#define card_spell_blast_exe shandalar_card_spell_blast
#define card_zombie_master_exe shandalar_card_zombie_master
#define card_detonate_exe shandalar_card_detonate
#define card_whimsy_exe shandalar_card_whimsy
#define card_winter_blast_exe shandalar_card_winter_blast
#define gs_target_error_power gs_illegal_target_why_power_00781aa0
#define gs_target_error_toughness gs_illegal_target_why_toughness_007894b0
#define gs_target_error_damaged gs_illegal_target_why_damaged_007911b0
#define gs_target_error_owner gs_illegal_target_why_owner_007aacc0
#define gs_target_error_blocking gs_illegal_target_why_blocking_007ab160
#define gs_target_error_attacking gs_illegal_target_why_attacking_00896540
#define gs_target_error_type gs_illegal_target_why_type_008a9a00
#define gs_target_error_player gs_illegal_target_why_player_008b3110
#define gs_target_error_walls gs_illegal_target_why_walls_008b34c0
#define gs_target_error_can_untap gs_illegal_target_why_can_untap_008b3be0
#define gs_target_error_attacking_or_blocking gs_illegal_target_why_attacking_blocking_008cc850
#define gs_target_error_color gs_illegal_target_why_color_008ce3c0
#define gs_target_error_cast_resolved gs_illegal_target_why_cast_resolved_008ce6a0
#define gs_target_error_subtype gs_illegal_target_why_subtype_008cea20
#define gs_target_error_will_untap gs_illegal_target_why_will_untap_008ceb50
#define gs_target_error_artifact_creature gs_illegal_target_why_artifact_creature_008ced40
#define gs_target_error_enchanted gs_illegal_target_why_enchanted_008cf3c0
#define gs_target_error_cannot_target_this gs_illegal_target_why_cant_target_this_0091a6e0
#define gs_target_error_abilities gs_illegal_target_why_abilities_0091a810
#define gs_target_error_controller gs_illegal_target_why_controller_0091bbe0
#define gs_target_error_spell gs_illegal_target_why_spell_0091be40
#define gs_target_error_attacked gs_illegal_target_why_attacked_0091bfc0
#define gs_target_error_target_player gs_illegal_target_why_target_player_0091c360
#define gs_target_error_blocked gs_illegal_target_why_blocked_0091caa0
#define gs_target_error_where gs_illegal_target_why_where_0091cd00
#define gs_target_error_tapped gs_illegal_target_why_tapped_00925080
#define gs_target_error_casted gs_illegal_target_why_casted_00939050
#define gs_target_error_basic_land gs_illegal_target_why_basic_land_0093d860
#define gs_target_error_card_type gs_illegal_target_why_card_type_008cfde0
#define gs_illegal_target gs_illegal_target_008b3e50
#define gs_illegal_target_format gs_illegal_target_with_reason_008ce7e0
#define should_ai_play is_in_play
#define put_card_in_graveyard raw_put_card_in_graveyard
#define COST_COLORLESS manalinkeh_cost_scratch[0]
#define COST_BLACK manalinkeh_cost_scratch[1]
#define COST_BLUE manalinkeh_cost_scratch[2]
#define COST_GREEN manalinkeh_cost_scratch[3]
#define COST_RED manalinkeh_cost_scratch[4]
#define COST_WHITE manalinkeh_cost_scratch[5]
#define MANA_COLORLESS manalinkeh_mana_scratch[0]
#define MANA_BLACK manalinkeh_mana_scratch[1]
#define MANA_BLUE manalinkeh_mana_scratch[2]
#define MANA_GREEN manalinkeh_mana_scratch[3]
#define MANA_RED manalinkeh_mana_scratch[4]
#define MANA_WHITE manalinkeh_mana_scratch[5]
#define MANA_ARTIFACT manalinkeh_mana_scratch[6]
#define MANA_UNKNOWN manalinkeh_mana_scratch[7]
#define byte_786DD4 manalinkeh_prevent_normal_graveyard_effect
#define g_prevent_normal_card_to_graveyard_effect manalinkeh_prevent_normal_graveyard_effect
#define TENTATIVE_next_spell_always_castable manalinkeh_next_spell_always_castable
#define gs_cuecard_smallcard_is_target gs_cuecard_is_a_target_00789c40
#define gs_cuecard_smallcard_damage_to_player gs_cuecard_damage_to_player_007a77d0
#define gs_cuecard_smallcard_phased gs_cuecard_phased_007aae00
#define gs_cuecard_smallcard_summoning_sickness gs_cuecard_summoning_sickness_008a8e00
#define gs_cuecard_smallcard_not_controlled_by_owner gs_cuecard_not_controlled_by_owner_008a8e40
#define gs_cuecard_smallcard_cannot_target gs_cuecard_cant_target_this_008b4990
#define gs_cuecard_smallcard_target_cannot_target_again gs_cuecard_is_a_target_cant_target_again_008cf230
#define gs_cuecard_smallcard_damage gs_cuecard_damage_n_0091b290
#define gs_cuecard_smallcard_dying gs_cuecard_dying_0091c640
#define gs_cuecard_smallcard_will_untap gs_cuecard_this_card_will_untap_00925040

/* Calls made through MOK's EXE veneer from inside the imported sources. */
#define resolve_mana_burn manalinkeh_mana_burn

#endif
