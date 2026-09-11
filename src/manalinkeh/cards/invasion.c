#include "manalink.h"

// Functions
int do_kicker(int player, int card, int colorless, int black, int blue, int green, int red, int white){
	int diff = 0;
	if( ! played_for_free(player, card) ){
		diff = MIN(0, cards_ptr[get_id(player, card)]->req_colorless+true_get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, -1));
	}
	if( colorless != -1 ){
		int cless = MAX(0, colorless+diff);
		if( has_mana_multi(player, cless, black, blue, green, red, white) ){
			int choice = do_dialog(player, player, card, -1, -1, " Pay kicker\n Pass", 0);
			if( choice == 0 ){
				charge_mana_multi(player, cless, black, blue, green, red, white);
				if( spell_fizzled != 1 ){
					set_special_flags(player, card, SF_KICKED);
					return 1;
				}
			}
		}
	}
	else{
		int choice = do_dialog(player, player, card, -1, -1, " Pay kicker\n Pass", 0);
		if( choice == 0 ){
			charge_mana(player, COLOR_COLORLESS, -1);
			if( spell_fizzled != 1 ){
				get_card_instance(player, card)->info_slot = x_value;
				if( diff < 0 ){
					get_card_instance(player, card)->info_slot-=diff;
				}
				set_special_flags(player, card, SF_KICKED);
				return 1;
			}
		}
	}

	return 0;
}

int kicker(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white){
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) && ! check_special_flags(player, card, SF_NOT_CAST) ){
			do_kicker(player, card, colorless, black, blue, green, red, white);
		}
	}
	return 0;
}

int kicked(int player, int card){
	if( check_special_flags(player, card, SF_KICKED) ){
		return 1;
	}
	return 0;
}

int generic_split_card(int player, int card, event_t event, int can_play_first_half, int priority_first_half,
						int colorless2, int black2, int blue2, int green2, int red2, int white2, int can_play_second_half, int priority_second_half,
						int can_be_fused, const char *name_first_half, const char *name_second_half
  ){
	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_MODIFY_COST ){
		int c1 = get_updated_casting_cost(player, card, -1, event, colorless2);
		if( has_mana_multi(player, c1, black2, blue2, green2, red2, white2) && can_play_second_half ){
			null_casting_cost(player, card);
			instance->info_slot |= (1<<8);
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( event == EVENT_CAN_CAST ){
		if( (instance->info_slot & (1<<8)) && can_play_second_half){
			return can_play_second_half;
		}
		return can_play_first_half;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( instance->info_slot & (1<<8) ){
			instance->info_slot = (1<<8);
		}
		else{
			instance->info_slot = 0;
		}
		instance->number_of_targets = 0;
		int choice = 1;
		int c1 = get_updated_casting_cost(player, card, -1, event, colorless2);
		int ff = played_for_free(player, card) | is_token(player, card);
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			if( ff || (instance->info_slot & (1<<8)) ){
				if( (ff || has_mana_to_cast_iid(player, event, get_card_instance(player, card)->internal_card_id)) && can_play_first_half){
					if( (ff || has_mana_multi(player, c1, black2, blue2, green2, red2, white2)) && can_play_second_half ){
						if( can_be_fused ){
							int can_fuse = ! not_played_from_hand(player, card);
							if( can_fuse ){
								card_ptr_t* c = cards_ptr[ get_id(player, card)  ];
								int c2 = get_updated_casting_cost(player, card, -1, event, c->req_colorless+colorless2);
								if( ! has_mana_multi(player, c2, c->req_black+black2, c->req_blue+blue2, c->req_green+green2, c->req_red+red2, c->req_white+white2) ){
									can_fuse = 0;
								}
							}
							choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
											name_first_half, can_play_first_half, priority_first_half,
											name_second_half, can_play_second_half, priority_second_half,
											"Fuse", can_fuse, 200);
						}
						else{
							choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
											name_first_half, can_play_first_half, priority_first_half,
											name_second_half, can_play_second_half, priority_second_half);
						}
					}
				}
				else{
					choice = 2;
				}
			}
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			instance->info_slot |= choice;
		}
		if( (instance->info_slot & 1) && (instance->info_slot & 2) ){ // Fuse
			card_ptr_t* c = cards_ptr[ get_id(player, card)  ];
			int c2 = get_updated_casting_cost(player, card, -1, event, c->req_colorless+colorless2);
			charge_mana_multi(player, c2, c->req_black+black2, c->req_blue+blue2, c->req_green+green2, c->req_red+red2, c->req_white+white2);
			if( spell_fizzled != 1 ){
				ff = 1;
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot & 1 ){
			if( !ff && (instance->info_slot & (1<<8)) ){
				charge_mana_from_id(player, card, event, get_id(player, card));
				if( is_x_spell(player, card) ){
					instance->targets[1].card = x_value;
				}
			}
			if( spell_fizzled != 1 ){
				call_card_function(player, card, EVENT_PLAY_FIRST_HALF);
				if( spell_fizzled == 1 ){
					return 0;
				}
			}
		}
		if( instance->info_slot & 2 ){
			if( !ff && (instance->info_slot & (1<<8)) ){
				charge_mana_multi(player, c1, black2, blue2, green2, red2, white2);
				if( colorless2 == -1 ){
					charge_mana(player, COLOR_COLORLESS, -1);
				}
			}
			instance->targets[1].card = x_value;
			if( spell_fizzled != 1 ){
				call_card_function(player, card, EVENT_PLAY_SECOND_HALF);
				if( spell_fizzled == 1 ){
					return 0;
				}
			}
		}
	}
	return 0;
}

static int invasion_apprentices(int player, int card, event_t event, int clr1, int clr2){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0);
	td1.zone = TARGET_ZONE_PLAYERS;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.preferred_controller = player;
	td2.allowed_controller = player;

	target_definition_t td3;
	default_target_definition(player, card, &td3, TYPE_CREATURE);
	td3.preferred_controller = player;

	target_definition_t td4;
	default_target_definition(player, card, &td4, TYPE_CREATURE);


	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( (clr1 == COLOR_BLACK || clr2 == COLOR_BLACK) &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(1), 0, &td1, NULL)
		  ){
			return 1;
		}
		if( (clr1 == COLOR_BLUE || clr2 == COLOR_BLUE) &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(1), 0, &td2, NULL)
		  ){
			return 1;
		}
		if( (clr1 == COLOR_GREEN || clr2 == COLOR_GREEN ) &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_G(1), 0, &td3, NULL)
		  ){
			return 1;
		}
		if( (clr1 == COLOR_RED || clr2 == COLOR_RED ) &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td3, NULL)
		  ){
			return 1;
		}
		if( (clr1 == COLOR_WHITE || clr2 == COLOR_WHITE) &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(1), 0, &td4, NULL)
		  ){
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		instance->info_slot = 0;
		int abilities[5] = {generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(1), 0, &td1, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(1), 0, &td2, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_G(1), 0, &td3, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td3, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(1), 0, &td4, NULL)
							};
		int priorities[5] = {	life[1-player] < 6 && would_validate_arbitrary_target(&td1, 1-player, -1) ? 5 : 15,
								8,
								10,
								5+(10*current_phase == PHASE_AFTER_BLOCKING),
								10
							};

		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL,
							"Target player loses 1 life", abilities[0] && (clr1 == COLOR_BLACK || clr2 == COLOR_BLACK), priorities[0],
							"Put target creature on top of your deck", abilities[1] && (clr1 == COLOR_BLUE || clr2 == COLOR_BLUE), priorities[1],
							"Pump a creature", abilities[2] && (clr1 == COLOR_GREEN || clr2 == COLOR_GREEN), priorities[2],
							"Give first strike", abilities[3] && (clr1 == COLOR_RED || clr2 == COLOR_RED), priorities[3],
							"Tap target creature", abilities[4] && (clr1 == COLOR_WHITE || clr2 == COLOR_WHITE), priorities[4]);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, 0, (choice == 1), (choice == 2), (choice == 3), (choice == 4), (choice == 5)) ){
			instance->number_of_targets = 0;
			if( choice == 1 ){
				pick_target(&td1, "TARGET_PLAYER");
			}
			if( choice == 2 ){
				new_pick_target(&td2, "Select target creature you control.", 0, 1);
			}
			if( choice == 3 ){
				pick_target(&td3, "TARGET_CREATURE");
			}
			if( choice == 4 ){
				pick_target(&td3, "TARGET_CREATURE");
			}
			if( choice == 5 ){
				pick_target(&td4, "TARGET_CREATURE");
			}
			if( spell_fizzled != 1 ){
				instance->number_of_targets = 1;
				instance->info_slot = choice;
				tap_card(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 && valid_target(&td1) ){
			lose_life(instance->targets[0].player, 1);
		}
		if( instance->info_slot == 2 && valid_target(&td2) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		if( instance->info_slot == 3 && valid_target(&td3) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 1);
		}
		if( instance->info_slot == 4 && valid_target(&td3) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
									0, 0, KEYWORD_FIRST_STRIKE, 0);
		}
		if( instance->info_slot == 5 && valid_target(&td4) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

static int mutation_spell(int player, int card, event_t event, int type, const char *buffer){

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, type);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int cmc = get_cmc(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			generate_tokens_by_id(player, card, CARD_ID_SAPROLING, cmc);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, buffer, 1, NULL);
}

static int invasion_masters(int player, int card, event_t event, int clr1, int clr2){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0);
	td1.zone = TARGET_ZONE_PLAYERS;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.preferred_controller = player;

	target_definition_t td3;
	default_target_definition(player, card, &td3, TYPE_CREATURE);


	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( (clr1 == COLOR_BLACK || clr2 == COLOR_BLACK) &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(2), 0, &td1, NULL)
		  ){
			return 1;
		}
		if( (clr1 == COLOR_BLUE || clr2 == COLOR_BLUE) &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(2), 0, &td3, NULL)
		  ){
			return 1;
		}
		if( (clr1 == COLOR_GREEN || clr2 == COLOR_GREEN ) && generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_G(2), 0, NULL, NULL) ){
			return 1;
		}
		if( (clr1 == COLOR_RED || clr2 == COLOR_RED ) &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(2), 0, &td3, NULL)
		  ){
			return 1;
		}
		if( (clr1 == COLOR_WHITE || clr2 == COLOR_WHITE) &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(2), 0, &td2, NULL)
		  ){
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		instance->info_slot = 0;
		int abilities[5] = {generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(2), 0, &td1, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(2), 0, &td3, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_G(2), 0,  NULL, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(2), 0, &td3, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(2), 0, &td2, NULL)
							};
		int priorities[5] = {	life[1-player] < 6 && would_validate_arbitrary_target(&td1, 1-player, -1) ? 5 : 15,
								8,
								current_phase <= PHASE_AFTER_BLOCKING ? 10 : 5,
								8,
								8
							};

		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL,
							"Target player loses 2 life", abilities[0] && (clr1 == COLOR_BLACK || clr2 == COLOR_BLACK), priorities[0],
							"Bounce a creature", abilities[1] && (clr1 == COLOR_BLUE || clr2 == COLOR_BLUE), priorities[1],
							"Pump all your creatures", abilities[2] && (clr1 == COLOR_GREEN || clr2 == COLOR_GREEN), priorities[2],
							"Deal 2 damages", abilities[3] && (clr1 == COLOR_RED || clr2 == COLOR_RED), priorities[3],
							"Give protection", abilities[4] && (clr1 == COLOR_WHITE || clr2 == COLOR_WHITE), priorities[4]);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, 0,
															(choice == 1 ? 2 : 0),
															(choice == 2 ? 2 : 0),
															(choice == 3 ? 2 : 0),
															(choice == 4 ? 2 : 0),
															(choice == 5 ? 2 : 0)
															)
		  ){
			if( choice == 1 ){
				pick_target(&td1, "TARGET_PLAYER");
			}
			if( choice == 2 ){
				pick_target(&td3, "TARGET_CREATURE");
			}
			if( choice == 4 ){
				pick_target(&td3, "TARGET_CREATURE");
			}
			if( choice == 5 ){
				pick_target(&td2, "TARGET_CREATURE");
			}
			if( spell_fizzled != 1 ){
				instance->number_of_targets = 1;
				instance->info_slot = choice;
				tap_card(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 && valid_target(&td1) ){
			lose_life(instance->targets[0].player, 2);
			gain_life(player, 2);
		}
		if( instance->info_slot == 2 && valid_target(&td3) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		if( instance->info_slot == 3 ){
			pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, -1, 2, 2, 0, 0);
		}
		if( instance->info_slot == 4 && valid_target(&td3) ){
			damage_target0(player, card, 2);
		}
		if( instance->info_slot == 5 && valid_target(&td2) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
									0, 0, select_a_protection(player), 0);
		}
	}

	return 0;
}

void spell_can_be_cast_as_instant_by_paying_2(int player, int card, event_t event){
	if( event == EVENT_MODIFY_COST ){
		if( ! can_sorcery_be_played(player, event) ){
			COST_COLORLESS+=2;
		}
		else{
			get_card_instance(player, card)->info_slot = 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( get_card_instance(player, card)->info_slot == 0 && player != AI && has_mana(player, COLOR_ANY, 2) ){
			int choice = do_dialog(player, player, card, -1, -1, " Play as Sorcery\n Play as Instant\n Cancel", 0);
			if( choice == 1 ){
				charge_mana(player, COLOR_COLORLESS, 2);
			}
		}
	}
}

static void invasion_dragon(int player, int card, event_t event, int cless, int black, int blue, int green, int red, int white){

	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE))
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		APNAP(p,{
					if( times_damaged[p] ){
						char buffer[100];
						if( p == player ){
							scnprintf(buffer, 100, "Activate %s (you were damaged)", cards_ptr[get_id(player, card)]->name);
						}
						else{
							scnprintf(buffer, 100, "Activate %s (opponent was damaged)", cards_ptr[get_id(player, card)]->name);
						}
						while( times_damaged[p] && has_mana_multi(player, cless, black, blue, green, red, white) ){
								int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
													buffer, 1, p != player ? 10 : 1,
													"Decline", 5);
								if( choice == 1 ){
									charge_mana_multi(player, cless, black, blue, green, red, white);
									if( spell_fizzled != 1 ){
										SET_BYTE1(instance->info_slot) = p;
										call_card_function(player, card, EVENT_DAMAGE_TRIGGER_ABILITY);
										instance->info_slot = 0;
									}
								}
								times_damaged[p]--;
						}
					}
				};
		);
	}
}


// Cards

int card_absorb(int player, int card, event_t event){

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			gain_life(player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 0, NULL);
}

int card_addle(int player, int card, event_t event){

	/* Addle	|1|B
	 * Sorcery
	 * Choose a color. Target player reveals his or her hand and you choose a card of that color from it. That player discards that card. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int clr = 1<<choose_a_color(player, get_deck_color(player, 1-player));
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			this_test.color = clr;

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			new_effect_coercion(&this_definition, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_aether_rift(int player, int card, event_t event){

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int chosen = get_random_card_in_hand(player);
		if( chosen > -1 ){
			int id = -1;
			if( is_what(player, chosen, TYPE_CREATURE) ){
				id = get_id(player, chosen);
			}
			discard_card(player, chosen);
			if( id > -1 ){
				int result = 0;
				APNAP(p, {
							int choice = 0;
							if( can_pay_life(p, 5) ){
								int ai_choice = 1;
								if( life[p]-5 < 6 ){
									ai_choice = 0;
								}
								char buffer[500];
								card_ptr_t* c = cards_ptr[ id ];
								scnprintf(buffer, 500, " Make %s return\n Pay 5 life", c->name);
								choice = do_dialog(p, player, card, -1, -1, buffer, ai_choice);
							}
							if( choice == 0 ){
								result++;
							}
							else{
								lose_life(p, 5);
								result--;
							}
						};
				);
				if( result == 2 ){
					seek_grave_for_id_to_reanimate(player, card, player, id, REANIMATE_DEFAULT);
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_ancient_spring(int player, int card, event_t event){
	return sac_land_tapped(player, card, event, COLOR_BLUE, COLOR_WHITE, COLOR_BLACK);
}

int card_angelic_shield(int player, int card, event_t event){

	boost_creature_type(player, card, event, -1, 0, 1, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_armadillo_cloack(int player, int card, event_t event){
	if (in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if (instance->damage_target_player >= 0 && instance->damage_target_card >= 0){
			spirit_link_effect(instance->damage_target_player, instance->damage_target_card, event, player);
		}
	}

	return generic_aura(player, card, event, player, 2, 2, KEYWORD_TRAMPLE, 0, 0, 0, 0);
}

int card_artifact_mutation(int player, int card, event_t event){
	/* Artifact Mutation	|R|G
	 * Instant
	 * Destroy target artifact. It can't be regenerated. Put X 1/1 |Sgreen Saproling creature tokens onto the battlefield, where X is that artifact's converted mana cost. */

	return mutation_spell(player, card, event, TYPE_ARTIFACT, "TARGET_ARTIFACT");
}

int card_assault_battery(int player, int card, event_t event){
	/* Assault // Battery	|R|/|3|G	0x200887e
	 * Sorcery // Sorcery
	 * Assault deals 2 damage to any target.
	 * //
	 * Create a 3/3 |Sgreen Elephant creature token. */
	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		return generic_split_card(player, card, event, can_target(&td), 0, MANACOST_XG(3, 1), 1, 0, 0, NULL, NULL);
	}

	if( event == EVENT_PLAY_FIRST_HALF ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		pick_target(&td, "TARGET_ANY");
	}


	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( (instance->info_slot & 1) && valid_target(&td) ){
			damage_creature_or_player(player, card, event, 2);
		}
		if( (instance->info_slot & 2) ){
			generate_token_by_id(player, card, CARD_ID_ELEPHANT);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, can_target(&td), 10, MANACOST_XG(3, 1), 1, 5, 0, "Assault", "Battery");
}

int card_atalya_samite_master(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.extra = damage_card;
	td1.special = TARGET_SPECIAL_DAMAGE_CREATURE;
	td1.required_type = 0;

	card_instance_t *instance = get_card_instance(player, card);

	check_legend_rule(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DAMAGE_PREVENTION_CREATURE | GAA_CAN_TARGET,
												MANACOST_W(1), 0, &td1, NULL);
		if( result ){
			return result;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_W(1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int choice = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_DAMAGE_PREVENTION_CREATURE | GAA_CAN_TARGET,
										MANACOST0, 0, &td1, NULL) )
		{
			int ai_choice = life[player] < 6 ? 1 : 0;
			choice = do_dialog(player, player, card, -1, -1, " Prevent damage to a creature\n Gain life\n Cancel", ai_choice);
		}
		else{
			choice = 1;
		}
		if( choice == 2 ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			charge_mana(player, COLOR_WHITE, -1);
			if( spell_fizzled != 1 ){
				instance->info_slot = x_value;
				if( choice == 0 ){
					if( new_pick_target(&td1, "Select a damage card which targets a creature.", 0, 1 | GS_LITERAL_PROMPT) ){
						instance->targets[1].player = 66+choice;
						tap_card(player, card);
					}
				}
				if( choice == 1 ){
					instance->targets[1].player = 66+choice;
					tap_card(player, card);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->targets[1].player == 66 && valid_target(&td1) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, instance->info_slot);
		}
		if( instance->targets[1].player == 67){
			gain_life(player, instance->info_slot);
		}
	}

	return 0;
}

int card_aura_mutation(int player, int card, event_t event){
	/* Aura Mutation	|G|W
	 * Instant
	 * Destroy target enchantment. Put X 1/1 |Sgreen Saproling creature tokens onto the battlefield, where X is that enchantment's converted mana cost. */

	return mutation_spell(player, card, event, TYPE_ENCHANTMENT, "TARGET_ENCHANTMENT");
}

int card_aura_shards(int player, int card, event_t event){
/*
Aura Shards |1|W|G
Enchantment
Whenever a creature enters the battlefield under your control, you may destroy target artifact or enchantment.
*/
	etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), TYPE_CREATURE, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "DISENCHANT") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_backlash(int player, int card, event_t event){

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
			int dmg = get_power(instance->targets[0].player, instance->targets[0].card);
			damage_player(instance->targets[0].player, dmg, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target untapped creature.", 1, NULL);
}

int card_barrins_spite(int player, int card, event_t event){

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, 0, NULL, NULL, 0, NULL) ){
			td.allowed_controller = 1-player;
			td.preferred_controller = 1-player;
			if( target_available(player, card, &td) > 1 ){
				return 1;
			}
			td.allowed_controller = player;
			td.preferred_controller = player;
			if( target_available(player, card, &td) > 1 ){
				return 1;
			}
			default_target_definition(player, card, &td, TYPE_CREATURE);
		}
	}
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( pick_target(&td, "TARGET_CREATURE") ){
			td.allowed_controller = instance->targets[0].player;
			td.preferred_controller = instance->targets[0].player;
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			if( can_target(&td) ){
				new_pick_target(&td, "TARGET_CREATURE", 1, 1);
			} else {
				cancel = 1;
			}
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}
	if(event == EVENT_RESOLVE_SPELL ){
		int legal_targets = validate_target(player, card, &td, 0) | (2*validate_target(player, card, &td, 1));
		if( legal_targets ){
			if( legal_targets & 1 ){
				if( legal_targets & 2 ){
					int p, c;
					for(p=0; p<2; p++){
						for(c=0; c<active_cards_count[p]; c++){
							if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
								state_untargettable(p, c, 1);
							}
						}
					}
					state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
					state_untargettable(instance->targets[1].player, instance->targets[1].card, 0);

					target_definition_t td2;
					default_target_definition(player, card, &td2, TYPE_CREATURE);
					td2.allowed_controller = instance->targets[0].player;
					td2.preferred_controller = instance->targets[0].player;
					td2.who_chooses = instance->targets[0].player;
					td2.allow_cancel = 0;

					if( new_pick_target(&td2, "Select the creature to sacrifice.", 2, GS_LITERAL_PROMPT) ){
						if( instance->targets[0].player == instance->targets[2].player && instance->targets[0].card == instance->targets[2].card ){
							state_untargettable(instance->targets[1].player, instance->targets[1].card, 1);
							if( can_sacrifice(player, instance->targets[0].player, 1, TYPE_CREATURE, 0) ){
								kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
							}
							else{
								state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
							}
							bounce_permanent(instance->targets[1].player, instance->targets[1].card);
						}
						else{
							state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
							if( can_sacrifice(player, instance->targets[1].player, 1, TYPE_CREATURE, 0) ){
								kill_card(instance->targets[1].player, instance->targets[1].card, KILL_SACRIFICE);
							}
							else{
								state_untargettable(instance->targets[1].player, instance->targets[1].card, 1);
							}
							bounce_permanent(instance->targets[0].player, instance->targets[0].card);
						}
					}
					for(p=0; p<2; p++){
						for(c=0; c<active_cards_count[p]; c++){
							if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
								state_untargettable(p, c, 0);
							}
						}
					}
				}
			}
			else{
				if( can_sacrifice(player, instance->targets[1].player, 1, TYPE_CREATURE, 0) ){
					kill_card(instance->targets[1].player, instance->targets[1].card, KILL_SACRIFICE);
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_benalish_heralds(int player, int card, event_t event){

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XU(3, 1), 0, NULL, NULL);
}

int card_bind2(int player, int card, event_t event)
{
  if (event == EVENT_CAN_CAST)
	return can_counter_activated_ability(player, card, event, NULL);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  get_card_instance(player, card)->number_of_targets = 0;
	  return cast_counter_activated_ability(player, card, 0);
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (resolve_counter_activated_ability(player, card, NULL, 0))
		draw_cards(player, 1);
	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_blazing_specter(int player, int card, event_t event)
{
  //0x4d8fd0

  /* Blazing Specter	|2|B|R
   * Creature - Specter 2/2
   * Flying, haste
   * Whenever ~ deals combat damage to a player, that player discards a card. */

  haste(player, card, event);
  whenever_i_deal_damage_to_a_player_he_discards_a_card(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE, 0);
  return 0;
}

int card_bloodstone_cameo(int player, int card, event_t event){
	return mana_producer(player, card, event);
}

int card_blurred_mangoose(int player, int card, event_t event){
	cannot_be_countered(player, card, event);
	return flash(player, card, event);
}

int card_breaking_wave(int player, int card, event_t event)
{
  /* Breaking Wave	|2|U|U
   * Sorcery
   * You may cast ~ any time you could cast an instant if you pay |2 more to cast it.
   * Simultaneously untap all tapped creatures and tap all untapped creatures. */

	spell_can_be_cast_as_instant_by_paying_2(player, card, event);

  if (event == EVENT_CAN_CAST)
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  char tapped[2][151] = {{0}};
	  int p, c;
	  for (p = 0; p <= 1; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
		  if (in_play(p, c) && is_what(p, c, TYPE_CREATURE))
			tapped[p][c] = is_tapped(p, c) ? 1 : 2;

	  APNAP(pl,
			{
			  for (c = 0; c < active_cards_count[pl]; ++c)
				if (tapped[pl][c] && in_play(pl, c))
				  {
					if (tapped[pl][c] == 1)
					  untap_card(pl, c);
					else
					  tap_card(pl, c);
				  }
			});

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_breath_of_darigaaz(int player, int card, event_t event){

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int dmg = kicked(player, card) ? 4 : 1;
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.keyword = KEYWORD_FLYING;
		this_test.keyword_flag = 1;
		new_damage_all(player, card, ANYBODY, dmg, NDA_PLAYER_TOO, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return kicker(player, card, event, MANACOST_X(2));
}

int card_canopy_surge(int player, int card, event_t event){

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int dmg = kicked(player, card) ? 4 : 1;
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.keyword = KEYWORD_FLYING;
		new_damage_all(player, card, 2, dmg, NDA_PLAYER_TOO, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return kicker(player, card, event, MANACOST_X(2));
}

int card_captain_sisay(int player, int card, event_t event){

	/* Captain Sisay	|2|G|W
	 * Legendary Creature - Human Soldier 2/2
	 * |T: Search your library for a legendary card, reveal that card, and put it into your hand. Then shuffle your library. */

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION  ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t this_test;
		new_default_test_definition(&this_test, 0, "Select a legendary card.");
		this_test.subtype = SUBTYPE_LEGEND;

		card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
		parent->targets[2].card = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DECK_SEARCHER, MANACOST0, 0, NULL, NULL);
}

static int cauldron_dance_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		haste(p, c, event);

		if( eot_trigger(player, card, event) ){
			if( ! check_state(p, c, STATE_OUBLIETTED) ){
				bounce_permanent(p, c);
			}
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_cauldron_dance(int player, int card, event_t event){

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

	if( event == EVENT_CAN_CAST && current_phase > PHASE_MAIN1 && current_phase < PHASE_MAIN2 ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 0, &this_test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 0, &this_test);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected > -1 ){
			int zombo = reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
			if( zombo > -1 ){
				create_targetted_legacy_effect(player, card, &cauldron_dance_legacy, player, zombo);
			}
			this_test.zone = TARGET_ZONE_HAND;
			if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
				int zombo2 = new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
				if( zombo2 > -1  ){
					int legacy = haste_and_remove_at_beginning_of_next_end_step(player, card, player, zombo2, KILL_SACRIFICE);
					get_card_instance(player, legacy)->number_of_targets = 1;
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_charging_troll(int player, int card, event_t event)
{
  //0x1205390

  /* Charging Troll	|2|G|W
   * Creature - Troll 3/3
   * Vigilance
   * |G: Regenerate ~. */

  vigilance(player, card, event);
  return regeneration(player, card, event, MANACOST_G(1));
}

int card_chromatic_sphere(int player, int card, event_t event)
{
  if (event == EVENT_CAN_ACTIVATE)
	return !is_tapped(player, card) && !is_animated_and_sick(player, card) && has_mana(player, COLOR_ANY, 1) && can_produce_mana(player, card);

  if (event == EVENT_ACTIVATE)
	{
	  tap_card(player, card);
	  charge_mana(player, COLOR_COLORLESS, 1);
	  if (spell_fizzled != 1 && produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, 1))
		{
		  kill_card(player, card, KILL_SACRIFICE);
		  draw_a_card(player);
		}
	  else
		untap_card_no_event(player, card);
	}

  return 0;
}

int card_cinder_shade(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return generic_shade(player, card, event, 0, MANACOST_B(1), 1, 1, 0, 0);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_R(1), 0, &td, NULL) ){
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		int choice = instance->info_slot = instance->number_of_targets = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_B(1), 0, NULL, NULL) ){
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_R(1), 0, &td, NULL) ){
				int ai_choice = current_phase == PHASE_AFTER_BLOCKING ? 0 : 1;
				choice = do_dialog(player, player, card, -1, -1, " Pump\n Sac & damage\n Cancel", ai_choice);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 0 ){
			if( charge_mana_for_activated_ability(player, card, MANACOST_B(1)) ){
				instance->info_slot = -1;
			}
		}
		else if( choice == 1 ){
				charge_mana_for_activated_ability(player, card, MANACOST_R(1));
				if( spell_fizzled != 1 && pick_target(&td, "TARGET_CREATURE") ){
					instance->info_slot = get_power(player, card);
					kill_card(player, card, KILL_SACRIFICE);
				}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot < 0 ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 1, 1);
		}
		else{
			if( valid_target(&td) ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, instance->info_slot, player, card);
			}
		}
	}

	return 0;
}

int card_coalition_victory(int player, int card, event_t event){

	if( event == EVENT_RESOLVE_SPELL ){
		int you_win = 0;

		if( count_domain(player, card) == 5 ){
			int c;
			int global_colors = 0;
			for(c=0; c<active_cards_count[player]; c++){
				if( in_play(player, c) && is_what(player, c, TYPE_CREATURE) ){
					global_colors |= get_color(player, c);
					global_colors &= ~COLOR_TEST_COLORLESS;
					if( (global_colors & COLOR_TEST_ANY_COLORED) == COLOR_TEST_ANY_COLORED ){
						you_win = 1;
						break;
					}
				}
			}
		}

		if( you_win ){
			lose_the_game(1-player);
		}

		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_collapsing_borders(int player, int card, event_t event){

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		gain_life(current_turn, count_domain(current_turn, card));
		damage_player(current_turn, 3, player, card);
	}

	return global_enchantment(player, card, event);
}

int card_collective_restraint(int player, int card, event_t event){
	tax_attack(player, card, event );
	return global_enchantment(player, card, event);
}

int card_crosis_the_purger(int player, int card, event_t event){
/*
Crosis, the Purger |3|U|B|R
Legendary Creature — Dragon 6/6
Flying
Whenever Crosis, the Purger deals combat damage to a player, you may pay {2}{B}.
If you do, choose a color, then that player reveals his or her hand and discards all cards of that color.
*/
	check_legend_rule(player, card, event);

	invasion_dragon(player, card, event, MANACOST_XB(2, 1));

	if( event == EVENT_DAMAGE_TRIGGER_ABILITY ){
		int t_player = BYTE1(get_card_instance(player, card)->info_slot);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ANY);
		this_test.color = 1<<choose_a_color(player, get_deck_color(player, t_player));

		ec_definition_t this_definition;
		default_ec_definition(t_player, player, &this_definition);
		this_definition.effect = EC_ALL_WHICH_MATCH_CRITERIA | EC_DISCARD;
		new_effect_coercion(&this_definition, &this_test);
	}

	return 0;
}

int card_crypt_angel(int player, int card, event_t event){
/* CARD_ID_CRYPT_ANGEL	4972
Crypt Angel	|4|B
Creature - Angel 3/3
Flying, protection from |Swhite
When ~ enters the battlefield, return target |Sblue or |Sred creature card from your graveyard to your hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		char msg[100];
		int pos = scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select target %s", COLOR_BLUE));
		scnprintf(msg + pos, 100 - pos, "%s", get_sleighted_color_text(player, card, " or %s creature card.", COLOR_RED));
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, msg);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE | COLOR_TEST_RED);
		if( new_special_count_grave(player, &this_test) && ! graveyard_has_shroud(player) ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 1, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

int card_darigaaz_the_igniter(int player, int card, event_t event){
/*
Darigaaz, the Igniter |3|B|R|G
Legendary Creature — Dragon 6/6
Flying
Whenever Darigaaz, the Igniter deals combat damage to a player, you may pay {2}{R}.
If you do, choose a color, then that player reveals his or her hand and Darigaaz deals damage to the player equal to the number of cards
	of that color revealed this way.
*/
	check_legend_rule(player, card, event);

	invasion_dragon(player, card, event, MANACOST_XR(2, 1));

	if( event == EVENT_DAMAGE_TRIGGER_ABILITY ){
		int t_player = BYTE1(get_card_instance(player, card)->info_slot);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ANY);
		this_test.color = 1<<choose_a_color(player, get_deck_color(player, t_player));
		this_test.zone = TARGET_ZONE_HAND;

		reveal_target_player_hand(t_player);

		int amount = check_battlefield_for_special_card(player, card, t_player, CBFSC_GET_COUNT, &this_test);
		if( amount > 0 ){
			damage_player(t_player, amount, player, card);
		}
	}

	return 0;
}

int card_death_or_glory(int player, int card, event_t event)
{
	if (event == EVENT_RESOLVE_SPELL)
	  {
		/* Move creatures into local dead[] array and obliterate from graveyard, to prevent interactions with creatures that manipulate the graveyard as they
		 * enter the battlefield (such as Gravedigger, Angel of Glory's Rise, Sutured Ghoul, etc.).  This causes problems with Grafdigger's Cage - the creatures
		 * will get moved to the top of their graveyard.  On the other hand, it's only one card.  We could also work around it directly here, by adding another
		 * code path if there's a Cage on the battlefield - don't obliterate cards early, and keep track of the graveyard indices when building piles so the
		 * right ones can be exiled.  Let's wait on that until someone complains.  And at that point, probably better to fix reanimate_all() to work right with
		 * Gravedigger-like cards. */
		int dead[500];
		int dead_count = 0;
		const int* grave = get_grave(player);
		int i;
		for (i = 0; i < 500 && grave[i] != -1;)
		  if (is_what(-1, grave[i], TYPE_CREATURE))
			{
			  dead[dead_count] = grave[i];
			  ++dead_count;
			  obliterate_card_in_grave(player, i);
			}
		  else
			++i;

		if (dead_count > 0)
		  {
			int piles[2][dead_count];

			separate_into_two_piles(player, dead, dead_count, piles);

			int chosen = choose_between_two_piles(1-player, dead_count, piles, AI_MAX_VALUE, "Exile these");

			// Exile the chosen pile
			int any_exiled = 0;
			int* rfg = rfg_ptr[player];
			int rfg_pos = count_rfg(player);
			for (i = 0; i < dead_count; ++i)
			  if (piles[chosen][i] != iid_draw_a_card && piles[chosen][i] != -1)
				{
				  rfg[rfg_pos] = piles[chosen][i];
				  ++rfg_pos;
				  any_exiled = 1;
				}
			if (any_exiled)
			  play_sound_effect(WAV_DESTROY);

			// Now add the non-chosen pile to graveyard and reanimate them, one at a time
			for (i = 0; i < dead_count; ++i)
			  if (piles[1 - chosen][i] != iid_draw_a_card && piles[1 - chosen][i] != -1)
				{
				  int pos = raw_put_iid_on_top_of_graveyard(player, piles[1 - chosen][i]);
				  increase_trap_condition(player, TRAP_CARDS_TO_GY_FROM_ANYWHERE, -1);	// Since not really putting in graveyard
				  reanimate_permanent(player, card, player, pos, REANIMATE_DEFAULT);
				}
		  }

		kill_card(player, card, KILL_DESTROY);
	  }

	return basic_spell(player, card, event);
}

int card_desperate_research(int player, int card, event_t event){
/*
Desperate Research |1|B
Sorcery
Name a card other than a basic land card.
Then reveal the top seven cards of your library and put all of them with that name into your hand. Exile the rest.
*/
	if(event == EVENT_RESOLVE_SPELL ){
		int csvid = -1;
		if( player == AI ){
			test_definition_t test;
			default_test_definition(&test, TYPE_ANY);
			test.subtype = SUBTYPE_BASIC;
			test.subtype_flag = DOESNT_MATCH;
			csvid = ai_name_a_random_csvid(player, card, player, 0, &test);
		}
		else{
			csvid = card_from_list(player, 3, TYPE_ANY, 0, SUBTYPE_BASIC, DOESNT_MATCH, 0, 0, 0, 0, -1, 0);
		}

		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.id = csvid;
		SET_BYTE0(test.qty) = MIN(7, count_deck(player));
		SET_BYTE1(test.qty) = MIN(7, count_deck(player));
		reveal_card_until_you_reveal_test(player, player, &test, RCUYRT_REVEAL_UP_TO_X | RCUYRT_EXILE_REST | RCUYRT_ADD_TEST_TO_HAND);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_dismantling_blow(int player, int card, event_t event){

	/* Dismantling Blow	|2|W
	 * Instant
	 * Kicker |2|U
	 * Destroy target artifact or enchantment.
	 * If ~ was kicked, draw two cards. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
		if( spell_fizzled != 1 ){
			do_kicker(player, card, MANACOST_XU(2, 1));
		}
	}
	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if( kicked(player, card) ){
				draw_cards(player, 2);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_devouring_strossus(int player, int card, event_t event){
/* CARD_ID_DEVOURING_STROSSUS	4979
Devouring Strossus	|5|B|B|B
Creature - Horror 9/9
Flying, trample
At the beginning of your upkeep, sacrifice a creature.
Sacrifice a creature: Regenerate ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return regeneration(player, card, event, MANACOST0);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE | GAA_REGENERATION, MANACOST0, 0, NULL, NULL);
}

int card_distorting_wake(int player, int card, event_t event){
/* CARD_ID_DISTORTING_WAKE	4981
Distorting Wake	|X|U|U|U
Sorcery
Return X target nonland permanents to their owners' hands. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.illegal_type = TYPE_LAND;
	td.allow_cancel = 2; //"Done" button

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, NULL, 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int trgs = instance->number_of_targets = 0;
		int max_targets = has_mana(player, COLOR_ANY, -1) + true_get_updated_casting_cost(player, card, -1, event, -1);
		while( can_target(&td) && trgs < max_targets ){
				if( new_pick_target(&td, "Select target nonland permanent.", trgs, 0 | GS_LITERAL_PROMPT) ){
					state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
					trgs++;
				}
				else{
					break;
				}
		}
		int i;
		for(i=0; i<trgs; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
		}
		int mana_to_charge = trgs + true_get_updated_casting_cost(player, card, -1, event, -1);
		charge_mana(player, COLOR_ANY, mana_to_charge);
		if( spell_fizzled != 1 ){
			instance->info_slot = trgs;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				bounce_permanent(instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

// drake skull cameo -->  bloodstone cameo

int card_dromar_the_banisher(int player, int card, event_t event){
/*
Dromar, the Banisher |3|W|U|B
Legendary Creature — Dragon 6/6
Flying
Whenever Dromar, the Banisher deals combat damage to a player, you may pay {2}{U}.
If you do, choose a color, then return all creatures of that color to their owners' hands.
*/
	check_legend_rule(player, card, event);

	invasion_dragon(player, card, event, MANACOST_XU(2, 1));

	if( event == EVENT_DAMAGE_TRIGGER_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = 1<<choose_a_color(player, get_deck_color(player, 1-player));
		APNAP(p, { new_manipulate_all(player, card, p, &this_test, ACT_BOUNCE); };);
	}

	return 0;
}

int card_elfhame_sanctuary(int player, int card, event_t event){
/*
Elfhame Sanctuary |1|G
Enchantment
At the beginning of your upkeep, you may search your library for a basic land card, reveal that card, and put it into your hand.
If you do, you skip your draw step this turn and shuffle your library.
*/
	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->targets[1].card = 0;
	}

	if( current_turn == player && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) && !is_humiliated(player, card)
	     && reason_for_trigger_controller == player){
		if( event == EVENT_TRIGGER ){
			int trig_mode = player == HUMAN ? RESOLVE_TRIGGER_OPTIONAL : 0;
			if( player == AI && get_card_instance(player, card)->targets[1].card != -1 ){
				test_definition_t test;
				default_test_definition(&test, TYPE_LAND);
				test.zone = TARGET_ZONE_HAND;
				int lands_in_hand = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test);
				if( ! lands_in_hand ){
					if( count_subtype(player, TYPE_LAND, -1) <= 3 || hand_count[player] >= 5 ){
						trig_mode = RESOLVE_TRIGGER_MANDATORY;
					}
				}
			}
			event_result |= trig_mode;
		}
		else if (event == EVENT_RESOLVE_TRIGGER){
				int count = count_upkeeps(current_turn);
				while( count > 0 ){
						/* Ruling 10/4/2004: The "if you do" means "if you search your library". You get to skip your draw step and
						 * shuffle your deck even if you don't find (or choose not to find) a basic land. */
						test_definition_t this_test;
						new_default_test_definition(&this_test, TYPE_LAND, "Select a basic land card.");
						this_test.subtype = SUBTYPE_BASIC;
						int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &this_test);
						get_card_instance(player, card)->targets[1].card = result;
						get_card_instance(player, card)->targets[1].player = 66;
						count--;
				}
		}
	}

	if( get_card_instance(player, card)->targets[1].player == 66 ){
		skip_your_draw_step(player, event);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	return global_enchantment(player, card, event);
}

int card_elvish_champion(int player, int card, event_t event){
/* CARD_ID_ELVISH_CHAMPION	1024
Elvish Champion	|1|G|G
Creature - Elf 2/2
Other Elf creatures get +1/+1 and have |H2forestwalk. (They can't be blocked as long as defending player controls |Ha Forest.) */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( ! affect_me(player, card) && has_creature_type(affected_card_controller, affected_card, SUBTYPE_ELF) ){
			switch( event ){
					case EVENT_POWER:
					case EVENT_TOUGHNESS:
						event_result++;
						break;
					case EVENT_ABILITIES:
						event_result |= get_hacked_walk(player, card, KEYWORD_FORESTWALK);
						break;
					default:
						break;
			}
		}
	}
	return 0;
}

int card_empress_galina(int player, int card, event_t event){
/* CARD_ID_EMPRESS_GALINA	4992
Empress Galina	|3|U|U
Legendary Creature - Merfolk 1/3
|U|U, |T: Gain control of target legendary permanent. (This effect lasts indefinitely.) */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.allowed_controller = 1-player;
	td.preferred_controller = 1-player;
	td.required_subtype = SUBTYPE_LEGEND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td)  ){
			gain_control(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(2), 0, &td, "TARGET_LEGENDARY_PERMANENT");
}

int card_exclude(int player, int card, event_t event){
	/*
	  Exclude |2|U
	  Instant, 2U
	  Counter target creature spell.
	  Draw a card.
	*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, &td, 0) ){
			draw_cards(player, 1);
			card_instance_t *instance= get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
}

int card_fact_or_fiction(int player, int card, event_t event){

	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		ai_modifier += 48;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		effect_fof(player, player, 5, TUTOR_GRAVE);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_fires_of_yavimaya(int player, int card, event_t event){

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( affected_card_controller == player && in_play(affected_card_controller, affected_card) &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
		{
			haste(affected_card_controller, affected_card, event);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_geothermal_crevice(int player, int card, event_t event){
	return sac_land_tapped(player, card, event, COLOR_RED, COLOR_BLACK, COLOR_GREEN);
}

int card_galinas_knight(int player, int card, event_t event){
	/*
	  Galina's Knight |W|U
	  Creature - Merfolk Knight 2/2
	  Protection from red
	*/
	if( ! is_humiliated(player, card) ){
		protection_from_red(player, card, event);
	}
	return 0;
}

int card_ghitu_fire(int player, int card, event_t event){
	/* Ghitu Fire	|X|R	0x20088e2
	 * Sorcery
	 * You may cast ~ as though it had flash if you pay |2 more to cast it.
	 * ~ deals X damage to any target. */
	spell_can_be_cast_as_instant_by_paying_2(player, card, event);

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, "TARGET_ANY", 1, NULL);
}

int card_glimmering_angel(int player, int card, event_t event){
	/*
	  Glimmering Angel |3|W
	  Creature - Angel 2/2
	  Flying
	  {U}: Glimmering Angel gains shroud until end of turn. (It can't be the target of spells or abilities.)
	*/
	return generic_shade(player, card, event, 0, MANACOST_U(1), 0, 0, KEYWORD_SHROUD, 0);
}

int card_global_ruin(int player, int card, event_t event)
{
  // Each player chooses from the lands he or she controls a land of each basic land type, then sacrifices the rest.

  if (event == EVENT_CAN_CAST)
	return 1;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  int subtypes[5] = {SUBTYPE_SWAMP, SUBTYPE_ISLAND, SUBTYPE_FOREST, SUBTYPE_MOUNTAIN, SUBTYPE_PLAINS};

	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_LAND);
	  td.illegal_abilities = 0;

	  card_instance_t* instance = get_card_instance(player, card);

	  int pl, i;
	  char marked[2][151] = {{0}};
	  // Each player targets a land of each type.  Mark them.
	  APNAP(p,
			{
			  td.allowed_controller = td.preferred_controller = td.who_chooses = p;
			  for (i = 0; i < 5; ++i)
				{
				  td.required_subtype = subtypes[i];
				  if (can_target(&td))
					{
					  load_text(0, "WOOD_ELVES");
					  if (select_target(player, card, &td, text_lines[i], &instance->targets[0]))
						{
						  instance->number_of_targets = 0;
						  marked[instance->targets[0].player][instance->targets[0].card] = 1;
						  state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
						}
					}
				}
			});

	  int can_sac[2];

	  // Unmark the chosen lands; mark all other lands.
	  for (pl = 0; pl <= 1; ++pl)
		{
		  for (i = 0; i < active_cards_count[pl]; ++i)
			if (marked[pl][i])
			  {
				marked[pl][i] = 0;
				state_untargettable(pl, i, 0);
			  }
			else if (in_play(pl, i) && is_what(pl, i, TYPE_LAND))
			  marked[pl][i] = 1;

		  can_sac[pl] = can_sacrifice(player, pl, 1, TYPE_LAND, 0);
		}

	  // Each player who can sacrifice sacrifices all marked lands.
	  APNAP(p,
			{
			  if (can_sac[p])
				for (i = 0; i < active_cards_count[p]; ++i)
				  if (marked[p][i])
					kill_card(p, i, KILL_SACRIFICE);
			});

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_hanna_ships_navigator(int player, int card, event_t event){

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ARTIFACT | TYPE_ENCHANTMENT, "Select an artifact or enchantment card.");

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int result = validate_target_from_grave_source(player, card, player, 0);
		if( result > -1 ){
			const int *grave = get_grave(player);
			add_card_to_hand(player, grave[result]);
			remove_card_from_grave(player, result);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST_XUW(1, 1, 1), 0, NULL, NULL,
															0, &this_test, GS_GRAVE_RECYCLER);
}

int card_harsh_judgement(int player, int card, event_t event){

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = 1<<choose_a_color_and_show_legacy(player, card, 1-player, -1);
	}

	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == -1 && damage->damage_target_player == player &&
				damage->damage_source_player != damage->damage_target_player && damage->info_slot > 0 )
			{
				if( instance->targets[3].player & TYPE_SPELL ){
					if( instance->initial_color & get_card_instance(player, card)->info_slot ){
						damage->damage_target_player = damage->damage_source_player;
					}
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_heroes_reunion(int player, int card, event_t event){

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) )
			gain_life(instance->targets[0].player, 7);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

// hunting kavu --> giant trap door spider

int card_irrigation_ditch(int player, int card, event_t event){
	return sac_land_tapped(player, card, event, COLOR_WHITE, COLOR_GREEN, COLOR_BLUE);
}

int card_jade_leech(int player, int card, event_t event){

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card) ){
		if( get_color(affected_card_controller, affected_card) & COLOR_TEST_GREEN ){
			COST_GREEN++;
		}
	}

	return 0;
}

int card_juntu_stakes(int player, int card, event_t event){
	if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && ! is_humiliated(player, card) ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) && get_power(affected_card_controller, affected_card) < 2 ){
			card_instance_t *instance= get_card_instance(affected_card_controller, affected_card);
			instance->untap_status &= ~3;
		}
	}
	return 0;
}

int card_kangee_aerie_keeper(int player, int card, event_t event)
{
  /* Kangee, Aerie Keeper	|2|W|U
   * Legendary Creature - Bird Wizard 2/2
   * Kicker |X|2
   * Flying
   * When ~ enters the battlefield, if it was kicked, put X feather counters on it.
   * Other Bird creatures get +1/+1 for each feather counter on ~. */

  check_legend_rule(player, card, event);

  if (event == EVENT_CAST_SPELL && affect_me(player, card)
	  && has_mana(player, COLOR_ANY, 2)
	  && do_dialog(player, player, card, -1, -1, " Pay kicker\n Pass", has_mana(player, COLOR_ANY, 3) ? 0 : 1) == 0
	  && charge_mana_multi(player, MANACOST_X(2))
	  && charge_mana_multi(player, MANACOST_X(-1)))
	{
		set_x_for_x_spells(player, card, event, x_value);
		set_special_flags(player, card, SF_KICKED);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( kicked(player, card) ){
			add_counters(player, card, COUNTER_FEATHER, get_card_instance(player, card)->info_slot);
		}
	}

	int counters;
	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && ! is_humiliated(player, card) &&
		(counters = count_counters(player, card, COUNTER_FEATHER)))
	{
		boost_subtype(player, card, event, SUBTYPE_BIRD, counters,counters, 0,0, BCT_CONTROLLER_ONLY);
	}

	return 0;
}

int card_kavu_chameleon(int player, int card, event_t event){

	cannot_be_countered(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int clr = 1<<choose_a_color(player, get_deck_color(player, 1-player));
		change_color(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, clr,
					CHANGE_COLOR_SET|CHANGE_COLOR_END_AT_EOT|CHANGE_COLOR_NO_SLEIGHT);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL);
}

int card_kavu_lair(int player, int card, event_t event){
/*
Kavu Lair |2|G
Enchantment
Whenever a creature with power 4 or greater enters the battlefield, its controller draws a card.
*/

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.power = 3;
		this_test.power_flag = F5_POWER_GREATER_THAN_VALUE;
		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(trigger_cause_controller, 1);
	}

	return global_enchantment(player, card, event);
}

int card_kavu_monarch(int player, int card, event_t event){
/*
Kavu Monarch |2|R|R
Creature — Kavu 3/3
Kavu creatures have trample.
Whenever another Kavu enters the battlefield, put a +1/+1 counter on Kavu Monarch.
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.subtype = SUBTYPE_KAVU;
		this_test.not_me = 1;
		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		add_1_1_counter(player, card);
	}

	boost_creature_type(player, card, event, SUBTYPE_KAVU, 0, 0, KEYWORD_TRAMPLE, BCT_INCLUDE_SELF);

	return 0;
}

int card_kavu_titan(int player, int card, event_t event){

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		do_kicker(player, card, MANACOST_XG(2, 1));
	}

	if( kicked(player, card) ){
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);
	}

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card) && kicked(player, card) ){
		event_result |= KEYWORD_TRAMPLE;
	}

	return 0;
}

int card_keldon_necropolis(int player, int card, event_t event){
	/* Keldon Necropolis	""	0x2008914
	 * Legendary Land
	 * |T: Add |C.
	 * |4|R, |T, Sacrifice a creature: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td.allow_cancel = 0;

	card_instance_t *instance = get_card_instance( player, card );

	check_legend_rule(player, card, event);

	if( event == EVENT_ACTIVATE ){
		int choice = instance->number_of_targets = instance->info_slot = 0;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_CREATURE | GAA_CAN_TARGET, MANACOST_XR(5, 1), 0, &td, NULL)
		  ){
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Sac & damage\n Cancel", 1);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		if( choice == 1 ){
			add_state(player, card, STATE_TAPPED);
			if( charge_mana_for_activated_ability(player, card, 4, 0, 0, 0, 1, 0) &&
				sacrifice(player, card, player, 0, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0) &&
				pick_target(&td, "TARGET_ANY")
			  ){
					instance->info_slot = 1;
			}
			else{
				remove_state(player, card, STATE_TAPPED);
				spell_fizzled = 1;
			}
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 ){
			if( valid_target(&td) ){
				damage_target0(player, card, 2);
			}
		}
	}

	return 0;
}

int card_liberate(int player, int card, event_t event){

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	if( player == AI ){
		td.required_state = TARGET_STATE_DESTROYED;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			if( player == AI ){
				regenerate_target(instance->targets[0].player, instance->targets[0].card);
			}
			remove_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	if( player == AI ){
		return generic_spell(player, card, event, GS_REGENERATION | GS_CAN_TARGET, &td, NULL, 1, NULL);
	}
	else{
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature you control.", 1, NULL);
	}
	return 0;
}

int card_malice_spite(int player, int card, event_t event){
	/*
	  Malice |3|B
	  Instant
	  Destroy target nonblack creature. It can't be regenerated.

	  Spite |3|U
	  Instant
	  Counter target noncreature spell.
	*/

	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

		target_definition_t td1;
		counterspell_target_definition(player, card, &td1, 0);
		td1.illegal_type = TYPE_CREATURE;

		generic_split_card(player, card, event, can_target(&td), 10, MANACOST_XU(3, 1), counterspell(player, card, EVENT_CAN_CAST, &td1, 0), 8, 0, NULL, NULL);
	}

	if( event == EVENT_PLAY_FIRST_HALF ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		new_pick_target(&td, "Select target non-black creature.", 0, 1 | GS_LITERAL_PROMPT);
	}

	if( event == EVENT_PLAY_SECOND_HALF ){
		card_instance_t *instance = get_card_instance(player, card);
		instance->targets[0].player = card_on_stack_controller;
		instance->targets[0].card = card_on_stack;
		instance->number_of_targets = 1;
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	target_definition_t td1;
	counterspell_target_definition(player, card, &td1, 0);
	td1.illegal_type = TYPE_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( (instance->info_slot & 1) && valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( (instance->info_slot & 2) && counterspell_validate(player, card, &td1, 0) ){
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, can_target(&td), 10, MANACOST_XU(3, 1), counterspell(player, card, EVENT_CAN_CAST, &td1, 0), 8, 0, "Malice", "Spite");
}

int card_metathran_aerostat(int player, int card, event_t event){
/*
Metathran Aerostat |2|U|U
Creature — Metathran 2/2
Flying
{X}{U}: You may put a creature card with converted mana cost X from your hand onto the battlefield.
	If you do, return Metathran Aerostat to its owner's hand.
*/

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		char buffer[100];
		scnprintf(buffer, 100, "Select a creature card with CMC %d.", instance->info_slot);
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, buffer);
		this_test.cmc = instance->info_slot;
		if( new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test) != -1 ){
			bounce_permanent(instance->parent_controller, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XU(-1, 1), 0, NULL, NULL);
}

int card_meteor_storm(int player, int card, event_t event){
	/* Meteor Storm	|R|G	0x2008928
	 * Enchantment
	 * |2|R|G, Discard two cards at random: ~ deals 4 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_ACTIVATE && hand_count[player] > 1 ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XGR(2, 1, 1), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XGR(2, 1, 1), 0, &td, "TARGET_ANY");
		if( spell_fizzled != 1 ){
			multidiscard(player, 2, 1);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
	}

	return 0;
}

int card_nightscape_apprentice(int player, int card, event_t event){
	/*
	  Nightscape Apprentice |B
	  Creature - Zombie Wizard 1/1
	  {U}, {T}: Put target creature you control on top of its owner's library.
	  {R}, {T}: Target creature gains first strike until end of turn.
	*/
	return invasion_apprentices(player, card, event, COLOR_BLUE, COLOR_RED);
}

int card_nightscape_master(int player, int card, event_t event){
	return invasion_masters(player, card, event, COLOR_BLUE, COLOR_RED);
}

int card_noble_panther(int player, int card, event_t event){
	/*
	  Noble Panther English |1|W|G
	  Creature - Cat 3/3
	  {1}: Noble Panther gains first strike until end of turn.
	*/
	return generic_shade(player, card, event, 0, MANACOST_X(1), 0, 0, KEYWORD_FIRST_STRIKE, 0);
}


int card_obliterate(int player, int card, event_t event)
{
  /* Obliterate	|6|R|R
   * Sorcery
   * ~ can't be countered.
   * Destroy all artifacts, creatures, and lands. They can't be regenerated. */

  cannot_be_countered(player, card, event);
  return card_jokulhaups(player, card, event);
}

int card_opt(int player, int card, event_t event){

	if( event == EVENT_RESOLVE_SPELL){
		scrylike_effect(player, player, 1);
		draw_cards(player, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_ordered_migration(int player, int card, event_t event)
{
  /* Ordered Migration	|3|W|U
   * Sorcery
   * Domain - Put a 1/1 |Sblue Bird creature token with flying onto the battlefield for each basic land type among lands you control. */

	if (event == EVENT_RESOLVE_SPELL)
	{
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_BIRD, &token);
		token.pow = token.tou = 1;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		token.qty = count_domain(player, card);
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_overabundance(int player, int card, event_t event){

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_TAP_CARD && is_what(affected_card_controller, affected_card, TYPE_LAND) && tapped_for_mana_color >= 0 ){
			damage_player(affected_card_controller, 1, player, card);
		}
	}

	return card_mana_flare(player, card, event);
}

int card_overload(int player, int card, event_t event){
/* CARD_ID_OVERLOAD	5055
Overload	|R
Instant
Kicker |2 (You may pay an additional |2 as you cast this spell.)
Destroy target artifact if its converted mana cost is 2 or less. If ~ was kicked, destroy that artifact if its converted mana cost is 5 or less instead. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( pick_target(&td, "TARGET_ARTIFACT") ){
			if( get_cmc(instance->targets[0].player, instance->targets[0].card) > 2 ){
				do_kicker(player, card, MANACOST_X(2));
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			if( ! kicked(player, card) && get_cmc(instance->targets[0].player, instance->targets[0].card) < 3 ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
			if( kicked(player, card) && get_cmc(instance->targets[0].player, instance->targets[0].card) < 5 ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
				}

		  }
	  kill_card(player, card, KILL_DESTROY );
	}
	return 0;
}

int card_phyrexian_altar(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_ALTAR	5057
Phyrexian Altar	|3
Artifact
Sacrifice a creature: Add one mana of any color to your mana pool. */

	if( ! IS_GAA_EVENT(event) && !(event == EVENT_COUNT_MANA && affect_me(player, card)) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) ){
		return new_can_sacrifice_as_cost(player, card, &test);
	}

	if(event == EVENT_ACTIVATE ){
		ai_modifier -= 36;
		if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
			FORCE(produce_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1));
			tapped_for_mana_color = -2;
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( can_use_activated_abilities(player, card) && new_can_sacrifice_as_cost(player, card, &test)){
			declare_mana_available_hex(player, 1, COLOR_TEST_ANY_COLORED);
		}
	}

	return 0;
}

int card_phyrexian_delver(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_DELVER	5059
Phyrexian Delver	|3|B|B
Creature - Zombie 3/2
When ~ enters the battlefield, return target creature card from your graveyard to the battlefield. You lose life equal to that card's converted mana cost.*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) && ! graveyard_has_shroud(player) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
			int result = new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_VALUE, &this_test);
			lose_life(player, get_cmc(player, result));
		}
	}

	return 0;
}

int card_phyrexian_infiltrator(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_INFILTRATOR	5060
Phyrexian Infiltrator	|2|B
Creature - Minion 2/2
|2|U|U: Exchange control of ~ and target creature. (This effect lasts indefinitely.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XU(2, 2), 0, &td, "TARGET_CREATURE");
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XU(2, 2), 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			ai_modifier += (instance->targets[0].player != player ? 48 : -48);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && in_play(instance->parent_controller, instance->parent_card) ){
			if( instance->parent_controller != instance->targets[0].player ){
				exchange_control_of_target_permanents(instance->parent_controller, instance->parent_card,
														instance->parent_controller, instance->parent_card,
														instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_phyrexian_lens(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_LENS	5061
Phyrexian Lens	|3
Artifact
|T, Pay 1 life: Add one mana of any color to your mana pool. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( mana_producer(player, card, event) ){
			return can_pay_life(player, 1);
		}
	}

	if(event == EVENT_ACTIVATE ){
		ai_modifier -= 36;

		produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, 1);
		if( spell_fizzled != 1){
			lose_life(player, 1);
		}
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( can_pay_life(player, 1) ){
			return mana_producer(player, card, event);
		}
	}

	return 0;
}

int card_plague_spitter(int player, int card, event_t event){
/* CARD_ID_PLAGUE_SPITTER	5065
Plague Spitter	|2|B
Creature - Horror 2/2
At the beginning of your upkeep, ~ deals 1 damage to each creature and each player.
When ~ dies, ~ deals 1 damage to each creature and each player. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		APNAP(p, {new_damage_all(player, card, p, 1, NDA_ALL_CREATURES, NULL);});
	}

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		APNAP(p, {new_damage_all(player, card, p, 1, NDA_ALL_CREATURES, NULL);});
	}

	return 0;
}

int card_plague_spores(int player, int card, event_t event)
{
  /* Plague Spores	|4|B|R
   * Sorcery
   * Destroy target non|Sblack creature and target land. They can't be regenerated. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}


	target_definition_t td_creature;
	default_target_definition(player, card, &td_creature, TYPE_CREATURE);
	td_creature.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	target_definition_t td_land;
	default_target_definition(player, card, &td_land, TYPE_LAND);

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td_land) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td_creature, NULL, 1, NULL);
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
		instance->number_of_targets = 0;
		char msg[100];
		scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select target non-%s creature.", COLOR_BLACK));
		if( new_pick_target(&td_creature, msg, 0, 1 | GS_LITERAL_PROMPT) ){
			new_pick_target(&td_land, "TARGET_LAND", 1, 1);
		}
	}

	if (event == EVENT_RESOLVE_SPELL)
	{
		if (validate_target(player, card, &td_creature, 0))
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);

		if (validate_target(player, card, &td_land, 1))
			kill_card(instance->targets[1].player, instance->targets[1].card, KILL_BURY);

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_planar_portal(int player, int card, event_t event){
/* CARD_ID_PLANAR_PORTAL	1116
Planar Portal	|6
Artifact
|6, |T: Search your library for a card and put that card into your hand. Then shuffle your library. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card to tutor.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(6), 0, NULL, NULL);
}

int card_probe(int player, int card, event_t event){
/* CARD_ID_PROBE	5071
Probe	|2|U
Sorcery
Kicker |1|B (You may pay an additional |1|B as you cast this spell.)
Draw three cards, then discard two cards.
If ~ was kicked, target player discards two cards. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		basic_spell(player, card, event);
		do_kicker(player, card, MANACOST_XB(1, 1));
		if( kicked(player, card) ){
			instance->number_of_targets = 0;
			if( can_target(&td) ){
				pick_target(&td, "TARGET_PLAYER");
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		draw_cards(player, 3);
		multidiscard(player, 2, player);
		if( kicked(player, card) && valid_target(&td) ){
			new_multidiscard(instance->targets[0].player, 2, 0, player);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_prohibit(int player, int card, event_t event){
/* CARD_ID_PROHIBIT	5072
Prohibit	|1|U
Instant
Kicker |2 (You may pay an additional |2 as you cast this spell.)
Counter target spell if its converted mana cost is 2 or less. If ~ was kicked, counter that spell if its converted mana cost is 4 or less instead. */

	if( event == EVENT_CAN_CAST ){
		int result = counterspell(player, card, event, NULL, 0);
		if( result ){
			if( player == AI ){
				int cmc = get_cmc(card_on_stack_controller, card_on_stack);
				if( cmc < 5 ){
					if( cmc <=2 || (cmc > 2 && has_mana(player, COLOR_COLORLESS, 2)) ){
						return result;
					}
				}
			}
			else{
				return result;
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		counterspell(player, card, event, NULL, 0);
		if( spell_fizzled != 1 ){
			do_kicker(player, card, MANACOST_X(2));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if (counterspell_validate(player, card, NULL, 0)){
			card_instance_t *instance = get_card_instance(player, card);
			int cmc = get_cmc(instance->targets[0].player, instance->targets[0].card);
			int good = (cmc <=2 || (cmc > 2 && cmc <= 4 && kicked(player, card))) ? 1 : 0;
			if( good ){
				real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_pyre_zombie(int player, int card, event_t event){
	/* Pyre Zombie	|1|B|R	0x2002280
	 * Creature - Zombie 2/1
	 * At the beginning of your upkeep, if ~ is in your graveyard, you may pay |1|B|B. If you do, return ~ to your hand.
	 * |1|R|R, Sacrifice ~: It deals 2 damage to any target. */
	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		if( has_mana_multi(player, MANACOST_XB(1, 2)) ){
			int choice = do_dialog(player, player, card, -1, -1," Return Pyre Zombie to hand\n Pass", 0);
			if( choice == 0 ){
				charge_mana_multi(player, MANACOST_XB(1, 2));
				if( spell_fizzled != 1){
					remove_state(player, card, STATE_INVISIBLE);
					hand_count[player]++;
					return -1;
				}
			}
		}
		return -2;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_XR(1, 2), 0,
									&td, "TARGET_ANY");
}

int card_raging_kavu(int player, int card, event_t event){
/* CARD_ID_RAGING_KAVU	5081
Raging Kavu	|1|R|G
Creature - Kavu 3/1
Flash
Haste */

	haste(player, card, event);
	return flash(player, card, event);
}

int card_reckless_assault(int player, int card, event_t event){
	/* Reckless Assault	|2|B|R	0x2008973
	 * Enchantment
	 * |1, Pay 2 life: ~ deals 1 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 2, &td, "TARGET_ANY");
}

int card_recoil(int player, int card, event_t event){
/* CARD_ID_RECOIL	1554
Recoil	|1|U|B
Instant
Return target permanent to its owner's hand. Then that player discards a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			if( hand_count[1-player] > 0 ){
				discard(instance->targets[0].player, 0, player);
			}
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
}

int card_repulse(int player, int card, event_t event){
/* CARD_ID_REPULSE	1352
Repulse	|2|U
Instant
Return target creature to its owner's hand.
Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			draw_a_card(player);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_restock(int player, int card, event_t event){

	/* Restock	|3|G|G
	 * Sorcery
	 * Return two target cards from your graveyard to your hand. Exile ~. */

	if( event == EVENT_CAN_CAST ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ANY);
		if( generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 0, &this_test)){
			return count_graveyard(player) >= 2;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		select_multiple_cards_from_graveyard(player, player, -1, AI_MAX_VALUE, NULL, 2, &instance->targets[0]);
	}

	if( event == EVENT_RESOLVE_SPELL){
		int i, num_validated = 0;
		for (i = 0; i < 2; ++i){
			int selected = validate_target_from_grave(player, card, player, i);
			if (selected != -1){
				from_grave_to_hand(player, selected, TUTOR_HAND);
				++num_validated;
			}
		}
		if (num_validated == 0){
			spell_fizzled = 1;
			kill_card(player, card, KILL_DESTROY);
		}
		else {
			kill_card(player, card, KILL_EXILE);
		}
  }
  return 0;
}

int card_reviving_vapors(int player, int card, event_t event){
/* CARD_ID_REVIVING_VAPORS	5089
Reviving Vapors	|2|W|U
Instant
Reveal the top three cards of your library and put one of them into your hand. You gain life equal to that card's converted mana cost.
Put all other cards revealed this way into your graveyard. */

	if( event == EVENT_RESOLVE_SPELL){
		int amount = MIN(count_deck(player), 3);
		if( amount ){
			int *deck = deck_ptr[player];
			show_deck(1-player, deck, amount, "Cards revealed by Reviving Vapors", 0, 0x7375B0 );

			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card to add to your hand.");
			this_test.create_minideck = amount;
			this_test.no_shuffle = 1;
			int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 1, AI_MAX_CMC, &this_test);
			amount--;
			if( amount ){
				mill(player, amount);
			}
			gain_life(player, get_cmc(player, result));
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_reya_dawnbringer(int player, int card, event_t event){
/* CARD_ID_REYA_DAWNBRINGER	5091
Reya Dawnbringer	|6|W|W|W
Legendary Creature - Angel 4/6
Flying
At the beginning of your upkeep, you may return target creature card from your graveyard to the battlefield. */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) && ! graveyard_has_shroud(player) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_CMC, &this_test);
		}
	}

	return 0;
}

int card_rith_the_awakener(int player, int card, event_t event){
/* Rith, the Awakener	|3|R|G|W
 * Legendary Creature - Dragon 6/6
 * Flying
 * Whenever ~ deals combat damage to a player, you may pay |2|G.
 * If you do, choose a color, then put a 1/1 |Sgreen Saproling creature token onto the battlefield for each permanent of that color. */

	check_legend_rule(player, card, event);

	invasion_dragon(player, card, event, MANACOST_XG(2, 1));

	if( event == EVENT_DAMAGE_TRIGGER_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = 1<<choose_a_color(player, get_deck_color(player, 1-player));

		int amount = check_battlefield_for_special_card(player, card, ANYBODY, CBFSC_GET_COUNT, &this_test);
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, amount);
	}

	return 0;
}

int card_rout(int player, int card, event_t event){
/* CARD_ID_ROUT	5097
Rout	|3|W|W
Sorcery
You may cast ~ as though it had flash if you pay |2 more to cast it. (You may cast it any time you could cast an instant.)
Destroy all creatures. They can't be regenerated. */

	if( ! is_unlocked(player, card, event, 10) ){ return 0; }
	spell_can_be_cast_as_instant_by_paying_2(player, card, event);
	return card_wrath_of_god(player, card, event);
}

int generic_protection_from(int player, int card, event_t event, color_test_t cols);
int card_sabertooth_nishoba(int player, int card, event_t event){
	/* Sabertooth Nishoba	|4|G|W
	 * Creature - Cat Beast Warrior 5/5
	 * Trample, protection from |Sblue and from |Sred */

	return generic_protection_from(player, card, event, COLOR_TEST_BLUE | COLOR_TEST_RED);
}

int card_saproling_infestation(int player, int card, event_t event){
	/* Saproling Infestation	|1|G
	 * Enchantment
	 * Whenever a player kicks a spell, you put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller ){
		if( check_special_flags(trigger_cause_controller, trigger_cause, SF_KICKED) ||
			check_special_flags(trigger_cause_controller, trigger_cause, SF_KICKED2)
		  ){
			if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, NULL) ){
				generate_token_by_id(player, card, CARD_ID_SAPROLING);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_saproling_symbiosis(int player, int card, event_t event){
	/* Saproling Symbiosis	|3|G
	 * Sorcery
	 * You may cast ~ as though it had flash if you pay |2 more to cast it.
	 * Put a 1/1 |Sgreen Saproling creature token onto the battlefield for each creature you control. */

	spell_can_be_cast_as_instant_by_paying_2(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, count_subtype(player, TYPE_CREATURE, -1));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

// seashell cameo --> bloodstone cameo

int card_shivan_harvest(int player, int card, event_t event){
/* CARD_ID_SHIVAN_HARVEST	5115
Shivan Harvest	|1|R
Enchantment
|1|R, Sacrifice a creature: Destroy target nonbasic land. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = SUBTYPE_NONBASIC;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE | GAA_LITERAL_PROMPT, MANACOST_XR(1, 1),
									0, &td, "Select target nonbasic land.");
}

int card_skizzik(int player, int card, event_t event){
/* CARD_ID_SKIZZIK	5118
Skizzik	|3|R
Creature - Elemental 5/3
Kicker |R (You may pay an additional |R as you cast this spell.)
Trample, haste
At the beginning of the end step, sacrifice ~ unless it was kicked. */

	haste(player, card, event);

	if( event == EVENT_RESOLVE_SPELL && ! kicked(player, card) ){
		haste_and_remove_at_beginning_of_next_end_step(player, card, player, card, KILL_SACRIFICE);
	}

	return kicker(player, card, event, MANACOST_R(1));
}

int card_slay2(int player, int card, event_t event){
	/*
	  Slay |2|B
	  Instant, 2B (3)
	  Destroy target green creature. It can't be regenerated.
	  Draw a card.
	*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target %s creature.", COLOR_GREEN), 1, NULL);
}

int card_sleepers_robe(int player, int card, event_t event)
{
  // original code : 0x4DBC2B

  /* Sleeper's Robe	|U|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has fear.
   * Whenever enchanted creature deals combat damage to an opponent, you may draw a card. */

	int packets = equipped_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_OPPONENT|
												DDBM_TRIGGER_OPTIONAL);
	if( packets )
		draw_cards(player, packets);

	return generic_aura(player, card, event, player, 0, 0, 0,SP_KEYWORD_FEAR, 0, 0, 0);
}

int card_smoldering_tar(int player, int card, event_t event){
/* CARD_ID_SMOLDERING_TAR	5122
Smoldering Tar	|2|B|R
Enchantment
At the beginning of your upkeep, target player loses 1 life.
Sacrifice ~: ~ deals 4 damage to target creature. Activate this ability only any time you could cast a sorcery. */

	if( trigger_condition == TRIGGER_UPKEEP && current_turn == player ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		upkeep_trigger_ability_mode(player, card, event, player, can_target(&td) ? RESOLVE_TRIGGER_MANDATORY : 0);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			lose_life(instance->targets[0].player, 1);
		}

		instance->number_of_targets = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 4, player, card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME | GAA_CAN_SORCERY_BE_PLAYED, MANACOST0, 0,
									&td1, "TARGET_CREATURE");
}

int card_spirit_of_resistance(int player, int card, event_t event){
/* CARD_ID_SPIRIT_OF_RESISTANCE	5126
Spirit of Resistance	|2|W
Enchantment
As long as you control a permanent of each color, prevent all damage that would be dealt to you. */

	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *damage = get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->damage_target_card == -1 && damage->damage_target_player == player &&
			damage->info_slot > 0 )
		{
			int i;
			int clr = 0;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_play(player, i) && is_what(player, i, TYPE_PERMANENT) ){
					clr |= get_color(player, i);
				}
				if( num_bits_set(clr) > 4 ){
					break;
				}
			}
			if( num_bits_set(clr) > 4 ){
				int amount = damage->info_slot;
				prevent_damage(affected_card_controller, affected_card, amount);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_stalking_assassin(int player, int card, event_t event){
/* CARD_ID_STALKING_ASSASSIN	5129
Stalking Assassin	|1|U|B
Creature - Human Assassin 1/1
|3|U, |T: Tap target creature.
|3|B, |T: Destroy target tapped creature. */

	if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td_creature;
		default_target_definition(player, card, &td_creature, TYPE_CREATURE);

		target_definition_t td_tapped_creature;
		default_target_definition(player, card, &td_tapped_creature, TYPE_CREATURE);
		td_tapped_creature.required_state = TARGET_STATE_TAPPED;

		int can_tap = !is_tapped(player, card) && !is_sick(player, card);
		int can_tap_creature = can_tap && can_target(&td_creature) && has_mana_for_activated_ability(player, card, MANACOST_XU(3, 1));
		int can_destroy_creature = can_tap && can_target(&td_tapped_creature) && has_mana_for_activated_ability(player, card, MANACOST_XB(3, 1));

		int choice = DIALOG(player, card, event,
							"Tap a creature", can_tap_creature, 1,
							"Destroy a tapped creature", can_destroy_creature, 2);

		if (event == EVENT_CAN_ACTIVATE){
			return choice;
		} else if (event == EVENT_ACTIVATE){
			instance->number_of_targets = 0;
			if (choice == 1){
				if (current_turn == player){
					ai_modifier -= 48;	// Very relucant to activate to tap during own turn
				} else if (current_phase == PHASE_DISCARD){
					ai_modifier += 24;	// If nothing available to destroy by opponent's end phase, more eager to tap
				}
				if (charge_mana_for_activated_ability(player, card, MANACOST_XU(3, 1)) && pick_target(&td_creature, "TARGET_CREATURE")){
					tap_card(player, card);
				}
			} else if (choice == 2){
				if (charge_mana_for_activated_ability(player, card, MANACOST_XB(3, 1)) && pick_target(&td_creature, "TARGET_CREATURE")){
					tap_card(player, card);
				}
			}
		} else {	// EVENT_RESOLVE_ACTIVATION
			if (choice == 1 && valid_target(&td_creature)){
				tap_card(instance->targets[0].player, instance->targets[0].card);
			} else if (choice == 2 && valid_target(&td_tapped_creature)){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
		}
	}

	if (event == EVENT_ATTACK_RATING && affect_me(player, card)){
		ai_defensive_modifier += 48;
	} else if (event == EVENT_BLOCK_RATING && affect_me(player, card)){
		ai_defensive_modifier -= 48;
	}
	return 0;
}

int card_sterling_grove(int player, int card, event_t event){
/* CARD_ID_STERLING_GROVE	5132
Sterling Grove	|G|W
Enchantment
Other enchantments you control have shroud. (They can't be the targets of spells or abilities.)
|1, Sacrifice ~: Search your library for an enchantment card and reveal that card. Shuffle your library, then put the card on top of it. */

	if( event == EVENT_ABILITIES && in_play(player, card) && ! is_humiliated(player, card) ){
		if( ! affect_me(player, card) && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_ENCHANTMENT) )
		{
			event_result |= KEYWORD_SHROUD;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ENCHANTMENT, "Select an enchantment card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_DECK, 0, AI_MAX_VALUE, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL);
}

int card_stormscape_apprentice(int player, int card, event_t event){
	/*
	  Stormscape Apprentice |U
	  Creature - Human Wizard 1/1
	  {W}, {T}: Tap target creature.
	  {B}, {T}: Target player loses 1 life.
	*/
	return invasion_apprentices(player, card, event, COLOR_BLACK, COLOR_WHITE);
}

int card_stormscape_master(int player, int card, event_t event){
/* CARD_ID_STORMSCAPE_MASTER	5133
Stormscape Master	|2|U|U
Creature - Human Wizard 2/2
|W|W, |T: Target creature gains protection from the color of your choice until end of turn.
|B|B, |T: Target player loses 2 life and you gain 2 life. */

	return invasion_masters(player, card, event, COLOR_BLACK, COLOR_WHITE);
}

int card_sulfur_vent(int player, int card, event_t event){
/* CARD_ID_SULFUR_VENT	5136
Sulfur Vent	""
Land
~ enters the battlefield tapped.
|T: Add |B to your mana pool.
|T, Sacrifice ~: Add |U|R to your mana pool. */

	return sac_land_tapped(player, card, event, COLOR_BLACK, COLOR_RED, COLOR_BLUE);
}

int card_sunscape_apprentice(int player, int card, event_t event){
	/*
	  Sunscape Apprentice |W
	  Creature - Human Wizard 1/1
	  {G}, {T}: Target creature gets +1/+1 until end of turn.
	  {U}, {T}: Put target creature you control on top of its owner's library.
	*/
	return invasion_apprentices(player, card, event, COLOR_BLUE, COLOR_GREEN);
}

int card_sunscape_master(int player, int card, event_t event){
/* CARD_ID_SUNSCAPE_MASTER	5137
Sunscape Master	|2|W|W
Creature - Human Wizard 2/2
|G|G, |T: Creatures you control get +2/+2 until end of turn.
|U|U, |T: Return target creature to its owner's hand. */

	return invasion_masters(player, card, event, COLOR_BLUE, COLOR_GREEN);
}

int card_tangle2(int player, int card, event_t event){
/* CARD_ID_TANGLE	5140
Tangle	|1|G
Instant
Prevent all combat damage that would be dealt this turn.
Each attacking creature doesn't untap during its controller's next untap step. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn == player ? count_attackers(player)*5 : -48);
	}


	if( event == EVENT_RESOLVE_SPELL ){
		fog_effect(player, card);
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.state = STATE_ATTACKING;
		new_manipulate_all(player, card, 2, &this_test, ACT_DOES_NOT_UNTAP);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_tectonic_instability(int player, int card, event_t event){
/* CARD_ID_TECTONIC_INSTABILITY	5141
Tectonic Instability	|2|R
Enchantment
Whenever a land enters the battlefield, tap all lands its controller controls. */

	etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_LAND, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		new_manipulate_all(player, card, trigger_cause_controller, &this_test, ACT_TAP);
	}

	return global_enchantment(player, card, event);
}

int card_teferis_moat(int player, int card, event_t event)
{
/* CARD_ID_TEFERIS_MOAT	5143
Teferi's Moat	|3|W|U
Enchantment
As ~ enters the battlefield, choose a color.
Creatures of the chosen color without flying can't attack you. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = 1<<choose_a_color_and_show_legacy(player, card, 1-player, -1);
	}

	if (event == EVENT_ATTACK_LEGALITY && ! is_humiliated(player, card) ){
		int clr = get_card_instance(player, card)->info_slot;
		if( affected_card_controller != player && clr &&
			!(get_card_instance(affected_card_controller, affected_card)->regen_status & KEYWORD_FLYING) &&
			(get_color(affected_card_controller, affected_card) & clr) )
		{
			event_result = 1;
		}
	}

	return global_enchantment(player, card, event);
}

static const char* targets_a_land_you_control(int who_chooses, int player, int card)
{
  int i;
  card_instance_t* instance = get_card_instance(player, card);

  for (i = 0; i < instance->number_of_targets; ++i)
	if (instance->targets[0].player == who_chooses
		&& is_what(instance->targets[0].player, instance->targets[0].card, TYPE_LAND))
	  return NULL;

  return "targets land you control";
}

int card_teferis_response(int player, int card, event_t event)
{
/* CARD_ID_TEFERIS_RESPONSE	5144
Teferi's Response	|1|U
Instant
Counter target spell or ability an opponent controls that targets a land you control. If a permanent's ability is countered this way, destroy that permanent.
Draw two cards. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

  card_instance_t* instance = get_card_instance(player, card);

  target_definition_t td_effect;
  counter_activated_target_definition(player, card, &td_effect, 0);
  td_effect.allowed_controller = 1-player;
  td_effect.extra = (int32_t)targets_a_land_you_control;
  td_effect.special |= TARGET_SPECIAL_EXTRA_FUNCTION;

  target_definition_t td_spell;
  counterspell_target_definition(player, card, &td_spell, 0);
  td_spell.allowed_controller = 1-player;
  td_spell.extra = (int32_t)targets_a_land_you_control;
  td_spell.special |= TARGET_SPECIAL_EXTRA_FUNCTION;

  if (event == EVENT_CAN_CAST)
	{
	  if (counterspell(player, card, event, &td_spell, 0))
		{
		  instance->info_slot = 1;
		  return 99;
		}
	  if (can_counter_activated_ability(player, card, event, &td_effect))
		{
		  instance->info_slot = 2;
		  return 99;
		}
	  return 0;
	}
  else if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  if (instance->info_slot == 1)
		counterspell(player, card, event, &td_spell, 0);
	  else if (instance->info_slot == 2)
		cast_counter_activated_ability(player, card, 0);
	}
  else if (event == EVENT_RESOLVE_SPELL)
	{
	  if (instance->info_slot == 1)
		{
		  if (counterspell_validate(player, card, &td_spell, 0))
			{
			  set_flags_when_spell_is_countered(player, card, instance->targets[0].player, instance->targets[0].card);
			  kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			  draw_cards(player, 2);
			}
		}
	  else if (instance->info_slot == 2)
		{
		  if (resolve_counter_activated_ability(player, card, &td_effect, 0))
			{
			  card_instance_t* ability = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			  if (is_what(-1, ability->original_internal_card_id, TYPE_PERMANENT)
				  && in_play(ability->parent_controller, ability->parent_card))
				kill_card(ability->parent_controller, ability->parent_card, KILL_DESTROY);

			  draw_cards(player, 2);
			}
		}
	  kill_card(player, card, KILL_DESTROY);
	}
  else
	return counterspell(player, card, event, &td_spell, 0);

  return 0;
}

int card_tek(int player, int card, event_t event){
/*
Tek |5
Artifact Creature — Dragon 2/2
Tek gets +0/+2 as long as you control a Plains, has flying as long as you control an Island, gets +2/+0 as long as you control a Swamp,
	has first strike as long as you control a Mountain, and has trample as long as you control a Forest.
*/
	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES) && affect_me(player, card) ){
			int pow = 0;
			int tou = 0;
			int key = 0;
			CYCLE_ALL_CARDS(p, c, 	{
										if( p == player && in_play(p, c) ){
											if( has_subtype(p, c, SUBTYPE_PLAINS) ){
												tou = 2;
											}
											if( has_subtype(p, c, SUBTYPE_ISLAND) ){
												key |= KEYWORD_FLYING;
											}
											if( has_subtype(p, c, SUBTYPE_SWAMP) ){
												pow = 2;
											}
											if( has_subtype(p, c, SUBTYPE_MOUNTAIN) ){
												key |= KEYWORD_FIRST_STRIKE;
											}
											if( has_subtype(p, c, SUBTYPE_FOREST) ){
												key |= KEYWORD_TRAMPLE;
											}
										}
									};
			);
			switch( event ){
					case EVENT_POWER:
						event_result += pow;
						break;
					case EVENT_TOUGHNESS:
						event_result += tou;
						break;
					case EVENT_ABILITIES:
						event_result |= key;
						break;
					default:
						break;
			}
		}
	}

	return 0;
}

int card_thicket_elemental(int player, int card, event_t event){
/* Thicket Elemental	|3|G|G
 * Creature - Elemental 4/4
 * Kicker |1|G
 * When ~ enters the battlefield, if it was kicked, you may reveal cards from the top of your library until you reveal a creature card.
 * If you do, put that card onto the battlefield and shuffle all other cards revealed this way into your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		reveal_card_until_you_reveal_test(player, player, &test, RCUYRT_PUT_TEST_INTO_PLAY | RCUYRT_SHUFFLE_REST);
	}

	return kicker(player, card, event, MANACOST_XG(1, 1));
}

int card_thornscape_apprentice(int player, int card, event_t event){
	/*
	  Thornscape Apprentice |G
	  Creature - Human Wizard 1/1
	  {R}, {T}: Target creature gains first strike until end of turn.
	  {W}, {T}: Tap target creature.
	*/
	return invasion_apprentices(player, card, event, COLOR_RED, COLOR_WHITE);
}

int card_thornscape_master(int player, int card, event_t event){
/* CARD_ID_THORNSCAPE_MASTER	5148
Thornscape Master	|2|G|G
Creature - Human Wizard 2/2
|R|R, |T: ~ deals 2 damage to target creature.
|W|W, |T: Target creature gains protection from the color of your choice until end of turn. */

	return invasion_masters(player, card, event, COLOR_RED, COLOR_WHITE);
}

int card_thunderscape_apprentice(int player, int card, event_t event){
	/*
	  Thunderscape Apprentice |R
	  Creature - Human Wizard 1/1
	  {B}, {T}: Target player loses 1 life.
	  {G}, {T}: Target creature gets +1/+1 until end of turn.
	*/
	return invasion_apprentices(player, card, event, COLOR_BLACK, COLOR_GREEN);
}

int card_thunderscape_master(int player, int card, event_t event){
/* CARD_ID_THUNDERSCAPE_MASTER	5149
Thunderscape Master	|2|R|R
Creature - Human Wizard 2/2
|B|B, |T: Target player loses 2 life and you gain 2 life.
|G|G, |T: Creatures you control get +2/+2 until end of turn. */

	return invasion_masters(player, card, event, COLOR_BLACK, COLOR_GREEN);
}

// tigereye cameo --> bloodstone cameo

int card_tinder_farm(int player, int card, event_t event){
/* CARD_ID_TINDER_FARM	5152
Tinder Farm	""
Land
~ enters the battlefield tapped.
|T: Add |G to your mana pool.
|T, Sacrifice ~: Add |R|W to your mana pool. */

	return sac_land_tapped(player, card, event, COLOR_GREEN, COLOR_RED, COLOR_WHITE);
}

// trench wurm --> Dwarven miner

int card_treva_the_renewer(int player, int card, event_t event){
/*
Treva, the Renewer |3|W|U|G
Legendary Creature — Dragon 6/6
Flying
Whenever Treva, the Renewer deals combat damage to a player, you may pay {2}{W}.
If you do, choose a color, then you gain 1 life for each permanent of that color.
*/
	check_legend_rule(player, card, event);

	invasion_dragon(player, card, event, MANACOST_XW(2, 1));

	if( event == EVENT_DAMAGE_TRIGGER_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = 1<<choose_a_color(player, get_deck_color(player, 1-player));

		int amount = check_battlefield_for_special_card(player, card, ANYBODY, CBFSC_GET_COUNT, &this_test);
		gain_life(player, amount);
	}

	return 0;
}

int card_tribal_flames(int player, int card, event_t event){
	/* Tribal Flames	|1|R	0x20017c7
	 * Sorcery
	 * Domain - ~ deals X damage to any target, where X is the number of basic land types among lands you control. */
	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL){
		if (valid_target(&td)){
			damage_target0(player, card, count_domain(player, card));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

// troll horn cameo --> mox sapphire

int card_tsabo_tavoc(int player, int card, event_t event){

	/* Tsabo Tavoc	|5|B|R
	 * Legendary Creature - Horror 7/4
	 * First strike, protection from legendary creatures
	 * |B|B, |T: Destroy target legendary creature. It can't be regenerated. */

	check_legend_rule(player, card, event);

	// Protection from legends, an approximation
	if( ! is_humiliated(player, card) ){
		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *source = get_card_instance(affected_card_controller, affected_card);

			if( damage_card == source->internal_card_id && source->info_slot > 0 ){
				if( source->damage_target_player == player && source->damage_target_card == card ){
					if( in_play(source->damage_source_player, source->damage_source_card) &&
						is_what(source->damage_source_player, source->damage_source_card, TYPE_CREATURE) &&
						is_legendary(source->damage_source_player, source->damage_source_card) )
					{
						int amount = source->info_slot;
						prevent_damage(affected_card_controller, affected_card, amount);
					}
				}
			}
		}

		if(event == EVENT_BLOCK_LEGALITY ){
			if(player == attacking_card_controller && card == attacking_card ){
				if( is_legendary(affected_card_controller, affected_card) ){
					event_result = 1;
				}
			}
		}
	}

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_LEGEND;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td)  ){
			card_instance_t* instance = get_card_instance(player, card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST_B(2), 0,
									&td, "Select target Legendary creature.");
}

int card_tsabos_decree(int player, int card, event_t event){

	/* Tsabo's Decree	|5|B
	 * Instant
	 * Choose a creature type. Target player reveals his or her hand and discards all creature cards of that type. Then destroy all creatures of that type that
	 * player controls. They can't be regenerated. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}


	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.subtype = select_a_subtype(player, card);

			reveal_target_player_hand(instance->targets[0].player);

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			this_definition.effect = EC_ALL_WHICH_MATCH_CRITERIA | EC_DISCARD;

			new_effect_coercion(&this_definition, &this_test);
			new_manipulate_all(player, card, instance->targets[0].player, &this_test, KILL_BURY);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

static int tsabo_web_exceptions(int csvid){
	if( csvid == CARD_ID_BOTTOMLESS_VAULT ||
		csvid == CARD_ID_DWARVEN_HOLD ||
		csvid == CARD_ID_HOLLOW_TREES ||
		csvid == CARD_ID_ICATIAN_STORE ||
		csvid == CARD_ID_SAND_SILOS ||
		csvid == CARD_ID_CRYSTAL_QUARRY ||
		csvid == CARD_ID_DARKWATER_CATACOMBS ||
		csvid == CARD_ID_MOSSFIRE_VALLEY ||
		csvid == CARD_ID_SHADOWBLOOD_RIDGE ||
		csvid == CARD_ID_SKYCLOUD_EXPANSE ||
		csvid == CARD_ID_SUNGRASS_PRAIRIE ||
		csvid == CARD_ID_SHIMMERING_GROTTO ||
		csvid == CARD_ID_UNKNOWN_SHORES
	  ){
		return 1;
	}
	return 0;
}

int card_tsabos_web(int player, int card, event_t event){
/* CARD_ID_TSABOS_WEB	5164
Tsabo's Web	|2
Artifact
When ~ enters the battlefield, draw a card.
Each land with an activated ability that isn't a mana ability doesn't untap during its controller's untap step. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && ! is_humiliated(player, card) ){
		if( is_what(affected_card_controller, affected_card, TYPE_LAND) ){
			card_instance_t *instance= get_card_instance(affected_card_controller, affected_card);
			int affected_land = 0;
			if( (cards_data[instance->internal_card_id].extra_ability & EA_ACT_ABILITY) &&
				! tsabo_web_exceptions(get_id(affected_card_controller, affected_card))
			  ){
				affected_land = 1;
			}
			if( ! affected_land ){
				int special_value = cards_data[instance->internal_card_id].cc[2];
				if( special_value == 4 || special_value == 9 ){ //AKA have a activated ability managed via the Rules Engine card.
					affected_land = 1;
				}
			}
			if( affected_land ){
				does_not_untap(affected_card_controller, affected_card, event);
			}
		}
	}

	return 0;
}

int card_twilights_call(int player, int card, event_t event){
/* CARD_ID_TWILIGHTS_CALL	5166
Twilight's Call	|4|B|B
Sorcery
You may cast ~ as though it had flash if you pay |2 more to cast it. (You may cast it any time you could cast an instant.)
Each player returns all creature cards from his or her graveyard to the battlefield. */

	spell_can_be_cast_as_instant_by_paying_2(player, card, event);

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		APNAP(p, {new_reanimate_all(p, -1, p, &this_test, REANIMATE_DEFAULT);});
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_undermine(int player, int card, event_t event){
	/*
	  Undermine |U|U|B
	  Instant
	  Counter target spell. Its controller loses 3 life.
	*/
	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			lose_life(instance->targets[0].player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 0, NULL);
}

int card_urborg_emissary(int player, int card, event_t event){
/*
Urborg Emissary |2|B
Creature — Human Wizard 3/1
Kicker {1}{U} (You may pay an additional {1}{U} as you cast this spell.)
When Urborg Emissary enters the battlefield, if it was kicked, return target permanent to its owner's hand.
*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allowed_controller = 1-player;
		if( player == HUMAN || (player == AI || can_target(&td)) ){
			return kicker(player, card, event, MANACOST_XU(1, 1));
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_urzas_filter(int player, int card, event_t event){
/* CARD_ID_URZAS_FILTER	5172
Urza's Filter	|4
Artifact
Multicolored spells cost up to |2 less to cast. */

	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ANY);
		this_test.color_flag = F3_MULTICOLORED;
		if( new_make_test_in_play(affected_card_controller, affected_card, -1, &this_test) ){
			COST_COLORLESS-=2;
		}
	}

	return 0;
}

int card_urzas_rage(int player, int card, event_t event){
	/* Urza's Rage	|2|R	0x2008a0e
	 * Instant
	 * Kicker |8|R
	 * This spell can't be countered.
	 * ~ deals 3 damage to any target. If this spell was kicked, instead it deals 10 damage to that permanent or player and the damage can't be prevented. */
	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != 1 ){
			state_untargettable(player, card, 1);
			do_kicker(player, card, MANACOST_XR(8,1));
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		state_untargettable(player, card, 0);
		if( valid_target(&td) ){
			int dmg = 3;
			if( kicked(player, card) ){
				dmg = 10;
				int legacy = create_legacy_effect(player, card, &my_damage_cannot_be_prevented);
				get_card_instance(player, legacy)->targets[0].player = player;
				get_card_instance(player, legacy)->targets[0].card = card;
			}
			damage_creature_or_player(player, card, event, dmg);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_utopia_tree(int player, int card, event_t event){
/* CARD_ID_UTOPIA_TREE	1200
Utopia Tree	|1|G
Creature - Plant 0/2
|T: Add one mana of any color to your mana pool. */

	return mana_producer(player, card, event);
}

int card_verdeloth_the_ancient(int player, int card, event_t event){
/* Verdeloth the Ancient	|4|G|G
 * Legendary Creature - Treefolk 4/7
 * Kicker |X
 * Saproling creatures and other Treefolk creatures get +1/+1.
 * When ~ enters the battlefield, if it was kicked, put X 1/1 |Sgreen Saproling creature tokens onto the battlefield. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, get_card_instance(player, card)->info_slot);
	}

	boost_creature_type(player, card, event, SUBTYPE_SAPROLING, 1, 1, 0, BCT_INCLUDE_SELF);

	boost_creature_type(player, card, event, SUBTYPE_TREEFOLK, 1, 1, 0, 0);

	return kicker(player, card, event, MANACOST_X(-1));
}

int card_verduran_emissary(int player, int card, event_t event){
/* CARD_ID_VERDURAN_EMISSARY	5175
Verduran Emissary	|2|G
Creature - Human Wizard 2/3
Kicker |1|R (You may pay an additional |1|R as you cast this spell.)
When ~ enters the battlefield, if it was kicked, destroy target artifact. It can't be regenerated. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		td.allowed_controller = 1-player;
		if( player == HUMAN || (player == AI || can_target(&td)) ){
			return kicker(player, card, event, MANACOST_XR(1, 1));
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_ARTIFACT") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_vicious_kavu(int player, int card, event_t event){
/* CARD_ID_VICIOUS_KAVU	5177
Vicious Kavu	|1|B|R
Creature - Kavu 2/2
Whenever ~ attacks, it gets +2/+0 until end of turn. */

	if (declare_attackers_trigger(player, card, event, 0, player, card)){
		pump_until_eot(player, card, player, card, 2, 0);
	}

	return 0;
}

int card_vigorous_charge(int player, int card, event_t event){
/* CARD_ID_VIGOROUS_CHARGE	5178
Vigorous Charge	|G
Instant
Kicker |W (You may pay an additional |W as you cast this spell.)
Target creature gains trample until end of turn. Whenever that creature deals combat damage this turn, if ~ was kicked, you gain life equal to that damage.*/

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST || event == EVENT_CAN_CHANGE_TARGET || event == EVENT_CHANGE_TARGET){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( pick_target(&td, "TARGET_CREATURE") ){
			if( ! is_token(player, card) ){
				do_kicker(player, card, MANACOST_W(1));
			}
		}
	}
	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			int legacy = pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_TRAMPLE, 0);
			if( kicked(player, card) ){
				card_instance_t* leg = get_card_instance(player, legacy);
				if (leg->targets[4].card == -1){
					leg->targets[4].card = 0;
				}
				leg->targets[4].card |= (player == 0 ? 1 : 2);
			}
		}
		kill_card(player, card, KILL_DESTROY );
	}


	return 0;
}

int card_vile_consumption(int player, int card, event_t event){
/* CARD_ID_VILE_CONSUMPTION	5179
Vile Consumption	|1|U|B
Enchantment
All creatures have "At the beginning of your upkeep, sacrifice this creature unless you pay 1 life." */

	if( upkeep_trigger(player, card, event) ){
		int count = active_cards_count[current_turn]-1;
		while( count > -1 ){
				if( in_play(current_turn, count) && is_what(current_turn, count, TYPE_CREATURE) ){
					int ai_choice = 0;
					int choice = 1;
					if( can_pay_life(current_turn, 1) ){
						if( life[player] < 7 ){
							ai_choice = 1;
						}
						choice = do_dialog(current_turn, current_turn, count, -1, -1, " Pay 1 life\n Sac this", ai_choice);
					}
					if( choice == 0 ){
						lose_life(current_turn, 1);
					}
					else{
						kill_card(current_turn, count, KILL_SACRIFICE);
					}
				}
				count--;
		}
	}

	return global_enchantment(player, card, event);
}

int card_void(int player, int card, event_t event){
/* CARD_ID_VOID	5184
Void	|3|B|R
Sorcery
Choose a number. Destroy all artifacts and creatures with converted mana cost equal to that number.
Then target player reveals his or her hand and discards all nonland cards with converted mana cost equal to the number. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int number = 0;
			int i;
			if( player == AI ){
				int cmc[15];

				for(i=0;i<15; i++){
					cmc[i] = 0;
				}

				i=0;
				for(i=0;i<active_cards_count[1-player]; i++){
					if( in_play(1-player, i) && (is_what(1-player, i, TYPE_CREATURE) || is_what(1-player, i, TYPE_ARTIFACT)) ){
						cmc[get_cmc(1-player, i)]++;
					}
				}

				int most_common_cmc = -1;
				int par = 0;
				i=0;
				for(i=0;i<15; i++){
					if( cmc[i] > 0 && cmc[i] > par ){
						par = cmc[i];
						most_common_cmc = i;
					}
				}
				number = most_common_cmc;
			}

			else{
				number = choose_a_number(player, "Choose a number.", 15);
			}
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE | TYPE_ARTIFACT);
			this_test.cmc = number;

			new_manipulate_all(player, card, 2, &this_test, KILL_DESTROY);

			default_test_definition(&this_test, TYPE_LAND);
			this_test.type_flag = DOESNT_MATCH;
			this_test.cmc = number;

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			this_definition.effect = EC_ALL_WHICH_MATCH_CRITERIA | EC_DISCARD;
			new_effect_coercion(&this_definition, &this_test);

		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_wax_wane(int player, int card, event_t event){
	/*
	  Wax |G
	  Instant
	  Target creature gets +2/+2 until end of turn.

	  Wane |W
	  Instant
	  Destroy target enchantment.
	*/

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_ENCHANTMENT);

		generic_split_card(player, card, event, can_target(&td), 0, MANACOST_W(1), can_target(&td2), 0, 0, NULL, NULL);
	}

	if( event == EVENT_PLAY_FIRST_HALF ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		pick_target(&td, "TARGET_CREATURE");
	}

	if( event == EVENT_PLAY_SECOND_HALF ){
		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_ENCHANTMENT);
		pick_target(&td2, "TARGET_ENCHANTMENT");
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_ENCHANTMENT);

	if(event == EVENT_RESOLVE_SPELL ){
		if( (instance->info_slot & 1) && valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
		if( (instance->info_slot & 2) && valid_target(&td2) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, can_target(&td), 8, MANACOST_W(1), can_target(&td2), 10, 0, "Wax", "Wane");
}

int card_wash_out(int player, int card, event_t event){
/* CARD_ID_WASH_OUT	5188
Wash Out	|3|U
Sorcery
Return all permanents of the color of your choice to their owners' hands. */

	if(event == EVENT_RESOLVE_SPELL ){
		int clr = 1<<choose_a_color(player, get_deck_color(player, 1-player));
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = clr;
		new_manipulate_all(player, card, 2, &this_test, ACT_BOUNCE);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_yawgmoths_agenda(int player, int card, event_t event)
{
  /* Yawgmoth's Agenda	|3|B|B
   * Enchantment
   * You can't cast more than one spell each turn.
   * You may play cards from your graveyard.
   * If a card would be put into your graveyard from anywhere, exile it instead. */

	if_a_card_would_be_put_into_graveyard_from_anywhere_exile_it_instead(player, card, event, player, NULL);

	if (event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && get_specific_storm_count(player) > 0
		&& !is_what(affected_card_controller, affected_card, TYPE_LAND) && ! is_humiliated(player, card) )
		infinite_casting_cost();

	if ((event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE) && ! is_humiliated(player, card)){
		type_t types = TYPE_ANY;
		if (get_specific_storm_count(player) > 0)
			types = TYPE_LAND;

		return can_activate_to_play_cards_from_graveyard(player, card, event, types);
	}

	return global_enchantment(player, card, event);
}
