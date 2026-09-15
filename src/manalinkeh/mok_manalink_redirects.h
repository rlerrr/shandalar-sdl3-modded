#ifndef SHANDALAR_MOK_MANALINK_REDIRECTS_H
#define SHANDALAR_MOK_MANALINK_REDIRECTS_H

/*
 * Source-level equivalents of MOK magic.exe's rel32 jumps into ManalinkEh.
 * Include this before declarations when compiling the surviving magic engine
 * so incompatible prototypes are diagnosed by the compiler.
 */
#define card_multiblocker_hook manalinkeh_card_multiblocker_hook
#define get_protections_from manalinkeh_get_protections_from
#define can_pay_card_mana_cost manalinkeh_compute_and_check_casting_cost /* 0x00402930 -> 0x0200bb14 */
#define charge_mana manalinkeh_charge_mana
#define format_mana_payment_prompt manalinkeh_format_manacost_into_global_allpurpose_buffer /* 0x0042f1f0 -> 0x0200459e */
#define activate_mana_source_for_payment manalinkeh_autotap_mana_source /* 0x0042fed0 -> 0x0200e2c9 */
#define draw_card_for_player manalinkeh_draw_a_card /* 0x00433190 -> 0x02002a28 */
#define resolve_card_on_stack manalinkeh_put_card_on_stack3 /* 0x00433e10 -> 0x0200bad3 */
#define resolve_activated_ability manalinkeh_finalize_activation /* 0x004346e0 -> 0x0200ba5b */
#define resolve_card_immediately manalinkeh_resolve_trigger /* 0x00434800 -> 0x0200b286 */
#define choose_blockers_human manalinkeh_human_assign_blockers /* 0x00434960 -> 0x0200b91b */
#define can_attack manalinkeh_can_attack
#define can_block_attacker_with_landwalk_checks manalinkeh_is_legal_block /* 0x00434f30 -> 0x0200b394 */
#define can_block_attacker_with_abilities manalinkeh_is_legal_block_impl /* 0x00434f90 -> 0x0200b399 */
#define reassess_all_cards_and_mana manalinkeh_recalculate_all_cards_in_play /* 0x004351c0 -> 0x0200b24f */
#define C_get_abilities manalinkeh_get_abilities /* 0x004352d0 -> 0x0200155b */
#define C_dispatch_event_raw manalinkeh_dispatch_event_raw /* 0x00435a40 -> 0x020010dd */
#define dispatch_event_to_single_card manalinkeh_dispatch_event_to_single_card
#define update_phase_display manalinkeh_phase_changed /* 0x00435f20 -> 0x0200b452 */
#define play_sound_effect manalinkeh_play_sound_effect
#define refresh_stack_proxy_card manalinkeh_recopy_card_onto_stack /* 0x00436450 -> 0x0200ba56 */
#define push_card_onto_stack manalinkeh_put_card_or_activation_onto_stack /* 0x00436550 -> 0x0200ba51 */
#define dispatch_trigger manalinkeh_dispatch_trigger
#define get_destroy_if_blocked_combat_flags manalinkeh_check_destroys_if_blocked /* 0x00437700 -> 0x0200b470 */
#define resolve_mana_burn manalinkeh_mana_burn /* 0x0043a060 -> 0x0200b93e */
#define check_duel_finished manalinkeh_is_anyone_dead /* 0x0043a1b0 -> 0x0200e319 */
#define can_activate_mana_source_for_stop_prompt manalinkeh_is_a_tappable_mana_source /* 0x0043a280 -> 0x0200ba65 */
#define activate_mana_source_card manalinkeh_tap_card_for_mana /* 0x0043a2d0 -> 0x0200ba6a */
#define upkeep_phase manalinkeh_upkeep_phase
#define launch_mcu manalinkeh_launch_mcu
#define ante_drawn_card manalinkeh_ante_top_card_of_library /* 0x00458070 -> 0x0200b484 */
#define get_hunting_subtype_name manalinkeh_get_card_or_subtype_name /* 0x004671a0 -> 0x02001d4e */
#define get_card_display_pic_num manalinkeh_get_card_image_number /* 0x00468160 -> 0x02001cb8 */
#define load_startduel_assets2 manalinkeh_load_startduel_assets2
#define load_startduel_assets manalinkeh_load_startduel_assets
#define load_endduel_assets manalinkeh_load_endduel_assets
#define CardTypeFromID manalinkeh_CardTypeFromID /* 0x00471c70 -> 0x0200b77c */
#define C_count_colors_of_lands_in_play manalinkeh_count_colors_of_lands_in_play /* 0x004725c0 -> 0x0200b39e */
#define get_ai_search_elapsed_time manalinkeh_check_timer_for_ai_speculation /* 0x00472d10 -> 0x0200aa07 */
#define is_phase_stop_allowed_for_current_event manalinkeh_current_trigger_or_event_is_forced /* 0x00476f10 -> 0x0200bb37 */
#define kill_creatures_with_lethal_damage manalinkeh_after_damage /* 0x00477340 -> 0x02007127 */
#define dispatch_card_trigger_event manalinkeh_hostabi_dispatch_card_trigger_event /* 0x00477970 -> 0x0200892d */
#define kill_card manalinkeh_kill_card_exe /* 0x004779f0 -> 0x0200bb69 */
#define clear_card_attachments manalinkeh_destroy_attached_auras_and_obliterate_card /* 0x00477d90 -> 0x0200b222 */
#define move_card_to_graveyard manalinkeh_raw_put_card_in_graveyard /* 0x00477ec0 -> 0x0200bb28 */
#define remove_card_from_graveyard manalinkeh_remove_card_from_grave /* 0x00477f30 -> 0x0200bb2d */
#define shuffle_duel_library manalinkeh_shuffle_for_exe /* 0x00479bf0 -> 0x02001d17 */
#define find_internal_card_id_by_csv_id manalinkeh_get_internal_card_id_from_csv_id /* 0x00479df0 -> 0x0200b77c */
#define real_target_available manalinkeh_real_target_available
#define C_real_validate_target manalinkeh_real_validate_target /* 0x0047db90 -> 0x0200a9f8 */
#define C_real_select_target manalinkeh_real_select_target /* 0x0047e9d0 -> 0x0200a9f3 */
#define card_instances_should_be_displayed_identically manalinkeh_card_instances_should_be_displayed_identically
#define card_desperate_sentry manalinkeh_card_desperate_sentry
#define get_special_counters_name manalinkeh_get_special_counters_name
#define sub_495BE0 manalinkeh_sub_495BE0
/*
 * Do not redirect the host's AI snapshots to Manalink's original routines.
 * Those routines copy a contiguous block to hard-coded scratch addresses in
 * magic.exe.  Static and WebAssembly builds have no such address layout; the
 * host implementation snapshots the corresponding named state instead.
 */
#define tap_card_and_dispatch_event manalinkeh_tap_card /* 0x004a3170 -> 0x0200b920 */
#define damage_creature manalinkeh_damage_creature
#define choose_blockers_ai manalinkeh_ai_assign_blockers /* 0x004b18e0 -> 0x0200187b */
#define adjust_two_headed_giant_damage manalinkeh_process_multiblock /* 0x004b3850 -> 0x0200b245 */
#define process_card_enters_play manalinkeh_real_put_into_play /* 0x004b5d90 -> 0x0200e2e7 */
#define can_block_with_landwalk_masks manalinkeh_is_legal_block /* 0x004b9220 -> 0x0200b394 */
#define card_holy_armor manalinkeh_card_holy_armor
#define card_blessing manalinkeh_card_blessing
#define draw_smallcard_special_effect_card manalinkeh_draw_smallcard_special_effect_card
#define draw_smallcard_activation_card manalinkeh_draw_smallcard_activation_card
#define draw_smallcard_normal manalinkeh_draw_smallcard_normal
#define get_card_special_counter_icon manalinkeh_get_counter_type_by_id /* 0x004d4ca0 -> 0x02001d76 */

#endif
