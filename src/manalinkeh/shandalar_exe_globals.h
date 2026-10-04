#ifndef SHANDALAR_EXE_GLOBALS_H
#define SHANDALAR_EXE_GLOBALS_H

/* Named host-engine storage used by the statically linked Manalink sources. */
#ifndef MANALINKEH_STATIC
extern int lifeclass_offset_life; /* MOK 0x004d753c */
extern int lifeclass_offset_player_counters; /* MOK 0x004d7540 */
extern int lifeclass_offset_liched; /* MOK 0x004d7544 */
#endif
extern int unknown_charge_mana_004d7cb0; /* MOK 0x004d7cb0 */
extern HDC global_screen_dc; /* MOK 0x004eb7ac */
extern int g_mimicked_effect_csv_ids[]; /* MOK 0x004ec234 */
extern char gs_target_error_elephant_or_mammoth[]; /* MOK 0x004ec749 */
extern char gs_target_error_djinn_or_efreet[]; /* MOK 0x004ec75b */
extern char gs_target_error_summoning_sickness[]; /* MOK 0x004ec769 */
extern int g_opponent_initial_library_index; /* MOK 0x004ed85d */
extern int g_selected_wizard_color; /* MOK 0x004ed861 */
extern int g_recorded_action_count; /* MOK 0x0060a578 */
extern int g_ai_recorded_action_type; /* MOK 0x004ed954 */
extern char TENTATIVE_next_spell_always_castable; /* MOK 0x004ef160 */
extern int g_current_casting_internal_card_id; /* MOK 0x004ef19c */
extern int g_stop_phase; /* MOK 0x004ef1ac */
extern int g_stop_phase_player; /* MOK 0x004ef1b0 */
#ifdef MANALINKEH_STATIC
extern int (__cdecl *pending_killed_card_handler)(int, int); /* MOK 0x004ef1b4 */
#else
extern int pending_killed_card_handler; /* MOK 0x004ef1b4 */
#endif
extern char byte_55CF20[]; /* MOK 0x0055cf20 */
extern CRITICAL_SECTION global_critical_section_for_drawing; /* MOK 0x0056b950 */
extern char gs_prompt_defendwhom_illegal_block[]; /* MOK 0x005b8580 */
extern HBITMAP bmp_poison_pic; /* MOK 0x005b9a64 */
extern int g_saved_ai_modifier; /* MOK 0x005b9b3c */
extern int g_saved_recorded_action_count; /* MOK 0x005bbb44 */
extern int global_saveload_loading; /* MOK 0x005bc5f8 */
extern int ai_block_damage_orders[]; /* MOK 0x006076d8 */
extern int g_ai_combat_blocking_card_indices[]; /* MOK 0x0060775c */
extern int ai_blocker_player; /* MOK 0x00607c2c */
extern int ai_blocker_count; /* MOK 0x00607d54 */
extern int g_ai_attacker_count; /* MOK 0x0060827c */
extern int ai_blocker_cards[]; /* MOK 0x00608304 */
extern HPEN global_pen_palette_5d; /* MOK 0x00608d40 */
extern int g_recorded_action_controller; /* MOK 0x0060a434 */
extern int g_phase_response_window_open; /* MOK 0x0060a43c */
extern int g_previous_stop_phase; /* MOK 0x0060a4ac */
extern int g_mana_color_conversions[2][11]; /* MOK 0x0060a4b4 */
extern int g_recorded_action_phase; /* MOK 0x0060a52c */
extern int g_ai_action_replay_available; /* MOK 0x0060a530 */
extern int g_event_player; /* MOK 0x0060a534 */
extern int g_response_window_serial; /* MOK 0x0060a538 */
extern int g_response_card_type_mask; /* MOK 0x0060a53c */
extern int g_duel_active; /* MOK 0x0060a548 */
extern int g_phase_stop_suppressed; /* MOK 0x0060a54c */
extern int g_target_player_choice; /* MOK 0x0060a550 */
extern int g_response_action_mask; /* MOK 0x0060a554 */
extern int g_previous_stop_phase_player; /* MOK 0x0060a55c */
extern int g_activation_scan_flags; /* MOK 0x0060a560 */
extern int g_target_selection_status_code; /* MOK 0x0060a564 */
extern char global_all_purpose_buffer[]; /* MOK 0x0060a690 */
extern int g_ai_search_result; /* MOK 0x0060e9f8 */
extern int g_ai_search_flags; /* MOK 0x0060e9fc */
extern char path_base[]; /* MOK 0x0060ea00 */
extern int g_ai_decision_code; /* MOK 0x0060ec3c */
extern int g_duel_trace_counter; /* MOK 0x0060ec40 */
extern char global_cardart_path[]; /* MOK 0x0060eda0 */
extern char PROMPT_CHECKFEPHASE_5[]; /* MOK 0x0060eeac */
extern char gs_target_error_power[]; /* MOK 0x00618c1c */
extern CRITICAL_SECTION g_duel_render_lock; /* MOK 0x00620288 */
extern char gs_target_error_toughness[]; /* MOK 0x00620600 */
extern int g_response_processing_flags; /* MOK 0x00620860 */
extern char gs_cuecard_smallcard_is_target[]; /* MOK 0x00620d5c */
extern char gs_target_error_damaged[]; /* MOK 0x006282cc */
extern int g_duel_state_current_card; /* MOK 0x0062852c */
extern char gs_prompt_grabmana_2[]; /* MOK 0x0062853c */
extern int g_stack_prompt_flags[]; /* MOK 0x00628674 */
extern char gs_cuecard_smallcard_damage_to_player[]; /* MOK 0x006286f4 */
extern int g_current_encounter_strength; /* MOK 0x00628798 */
extern int g_deck_rating_multipliers[]; /* MOK 0x00628c1c */
extern int g_duel_state_008cfd20; /* MOK 0x00628c24 */
#ifndef MANALINKEH_STATIC
extern int g_opponent_hand_window_update_flag; /* MOK 0x00628c28 */
#endif
extern int iid_legacy_data_card; /* MOK 0x00628c64 */
extern char gs_target_error_owner[]; /* MOK 0x0062bbbc */
extern int g_current_action_event_code; /* MOK 0x0062bce8 */
extern char gs_cuecard_smallcard_phased[]; /* MOK 0x0062bcf0 */
extern char str_Activation[]; /* MOK 0x0062bd98 */
extern char gs_target_error_blocking[]; /* MOK 0x0062c044 */
extern int g_activation_dispatch_flags; /* MOK 0x0062c180 */
extern HWND global_main_hwnd; /* MOK 0x00715780 */
extern char global_duelsounds_path[]; /* MOK 0x00715788 */
extern int g_encounter_color_match_mode; /* MOK 0x00715890 */
#ifndef MANALINKEH_STATIC
extern void* hwnd_HandClass_player; /* MOK 0x007158c8 */
#endif
extern char gs_prompt_specialfephase_5[]; /* MOK 0x007158d4 */
extern char gs_prompt_specialfephase_6[]; /* MOK 0x00715a04 */
extern int g_ai_search_best_score; /* MOK 0x00715b30 */
extern char gs_target_error_attacking[]; /* MOK 0x00715b3c */
extern HWND g_duel_card_preview_window_hwnd; /* MOK 0x00715ca0 */
extern char gs_localized_cancel[]; /* MOK 0x007281a4 */
extern int g_ai_search_force_pass; /* MOK 0x007282ec */
extern int g_ai_search_target_depth; /* MOK 0x007282f4 */
extern int g_duel_state_008a8de4; /* MOK 0x0072835c */
extern int g_duel_generated_internal_card_id_29; /* MOK 0x00728368 */
extern char gs_cuecard_smallcard_summoning_sickness[]; /* MOK 0x00728374 */
extern char gs_cuecard_smallcard_not_controlled_by_owner[]; /* MOK 0x007283b4 */
extern char gs_prompt_grabmana_1[]; /* MOK 0x00728578 */
extern int g_showlist_smallcard_width; /* MOK 0x007286f8 */
extern char gs_target_error_type[]; /* MOK 0x00728f6c */
extern char gs_target_error_player[]; /* MOK 0x007366d8 */
extern int g_activation_event_flags; /* MOK 0x00736808 */
extern HWND g_duel_player_battlefield_window_hwnd; /* MOK 0x0073680c */
extern char gs_target_error_walls[]; /* MOK 0x007369ac */
extern char gs_target_error_can_untap[]; /* MOK 0x007370c8 */
extern char gs_illegal_target[]; /* MOK 0x0073732c */
extern char gs_done_008b40e0[]; /* MOK 0x007375b0 */
extern char str_Upkeep[]; /* MOK 0x007375c4 */
extern char gs_prompt_checkfephase_2[]; /* MOK 0x00737bc0 */
extern char gs_cuecard_smallcard_cannot_target[]; /* MOK 0x00737e24 */
extern int g_affected_internal_card_id; /* MOK 0x0073825c */
extern int g_duel_modal_action_active; /* MOK 0x00738a10 */
extern int g_ai_search_try_count; /* MOK 0x00738b48 */
extern char gs_target_error_attacking_or_blocking[]; /* MOK 0x00738b4c */
#ifndef MANALINKEH_STATIC
extern HWND hwnd_LifeClass_player; /* MOK 0x00738c7c */
#endif
extern char gs_prompt_action_2[]; /* MOK 0x00738f34 */
extern char gs_target_error_color[]; /* MOK 0x00739060 */
extern int g_current_encounter_color; /* MOK 0x007391a0 */
extern char gs_target_error_cast_resolved[]; /* MOK 0x007392d4 */
extern char gs_illegal_target_format[]; /* MOK 0x0073940c */
extern char gs_target_error_subtype[]; /* MOK 0x0073964c */
extern char gs_target_error_will_untap[]; /* MOK 0x0073977c */
extern char gs_target_error_artifact_creature[]; /* MOK 0x007398ec */
extern char gs_multiblock_creature_008cf040[]; /* MOK 0x00739be4 */
extern HWND g_duel_help_owner_hwnd; /* MOK 0x00739c1c */
extern char gs_prompt_specialfephase_8[]; /* MOK 0x00739c20 */
extern int g_showlist_smallcard_height; /* MOK 0x00739d50 */
extern int g_last_single_card_event_result; /* MOK 0x00739d58 */
extern char gs_cuecard_smallcard_target_cannot_target_again[]; /* MOK 0x00739dd0 */
extern char global_duelart_path[]; /* MOK 0x00739e30 */
extern char gs_target_error_enchanted[]; /* MOK 0x00739f60 */
extern char gs_cardtitle_damage_008cfd30[]; /* MOK 0x0073b538 */
extern char gs_target_error_card_type[]; /* MOK 0x0073b57c */
extern int g_life_gained; /* MOK 0x0073b6ac */
extern int g_nested_trigger_depth; /* MOK 0x00785e60 */
extern char gs_target_error_cannot_target_this_prefixed[]; /* MOK 0x00785e64 */
extern char gs_target_error_cannot_target_this[]; /* MOK 0x00785e65 */
extern char gs_target_error_abilities[]; /* MOK 0x00785f94 */
extern char gs_cuecard_smallcard_damage[]; /* MOK 0x00786230 */
extern char gs_prompt_checkfephase_3[]; /* MOK 0x00786264 */
extern char gs_target_error_controller[]; /* MOK 0x00786398 */
extern char gs_target_error_spell[]; /* MOK 0x007865f8 */
extern int g_adventure_world_state; /* MOK 0x0078676c */
extern char gs_target_error_destroyed[]; /* MOK 0x00786770 */
extern char gs_target_error_attacked[]; /* MOK 0x0078689c */
extern char gs_prompt_grabmana_0[]; /* MOK 0x00786b00 */
extern char gs_target_error_target_player[]; /* MOK 0x00786c30 */
extern int iid_legacy_swap_power_toughness; /* MOK 0x00786d5c */
extern void* hwnd_HandClass_opponent; /* MOK 0x00786dc4 */
extern int g_ai_search_time_limit; /* MOK 0x00786dcc */
extern char g_prevent_normal_card_to_graveyard_effect; /* MOK 0x00786dd4 */
extern char gs_cuecard_smallcard_dying[]; /* MOK 0x00786f08 */
extern char gs_card_into_play_0091c840[]; /* MOK 0x00787108 */
#ifndef MANALINKEH_STATIC
extern int option_ShowPowerToughnessOnCards; /* MOK 0x00787240 */
extern int option_ShowAbilitiesOnCards; /* MOK 0x00787244 */
extern int option_ShowAllCardsSummonSickness; /* MOK 0x00787258 */
extern int option_Layout; /* MOK 0x00787260 */
#endif
extern char gs_target_error_blocked[]; /* MOK 0x0078735c */
extern char gs_target_error_where[]; /* MOK 0x007875bc */
extern char gs_ok_00924800[]; /* MOK 0x0078f0a8 */
extern char gs_cuecard_smallcard_will_untap[]; /* MOK 0x0078f0cc */
extern char gs_target_error_tapped[]; /* MOK 0x0078f10c */
extern int g_ai_recorded_action; /* MOK 0x0078fa30 */
extern char gs_prompt_specialfephase_9[]; /* MOK 0x0078fa70 */
extern char gs_prompt_specialfephase_7[]; /* MOK 0x0078fba8 */
extern char gs_prompt_blockerselection_0[]; /* MOK 0x00790074 */
extern char gs_prompt_turnsequence_0[]; /* MOK 0x00790248 */
extern char gs_target_error_casted[]; /* MOK 0x007a2d7c */
extern int g_ai_speculation_depth; /* MOK 0x007a2fd8 */
extern int g_ai_recorded_choice; /* MOK 0x007a2fe4 */
#ifndef MANALINKEH_STATIC
extern int g_hand_window_card_vertical_offset; /* MOK 0x007a31a4 */
#endif
extern int g_trigger_dispatch_depth; /* MOK 0x007a31ac */
extern int g_phase_was_skipped; /* MOK 0x007a34d0 */
extern int g_ai_speculation_spell_fizzled; /* MOK 0x007a372c */
extern char gs_target_error_basic_land[]; /* MOK 0x007a3730 */
extern int g_duel_state_0093f4b0; /* MOK 0x007a5378 */
extern int g_card_count; /* MOK 0x007a5380 */

/* Interior fields of larger host objects. */
extern int* shandalar_damage_taken_this_turn(void);
#ifndef MANALINKEH_STATIC
extern int* shandalar_network_result(void);
extern unsigned char* shandalar_network_packet_type(void);
#endif

#endif
