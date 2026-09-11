// -*- c-basic-offset:2 -*-
#include "manalink.h"

/***** Functions *****/

static void modify_casting_cost_for_emerge(int player, int card, event_t event, int cless, int black, int blue, int green, int red, int white){
// "You may cast this spell by sacrificing a creature and paying the emerge cost reduced by that creature's converted mana cost.

	if( event == EVENT_MODIFY_COST ){
		int null_cost = 0;
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		if( new_can_sacrifice_as_cost(player, card, &test) ){
			int c1 = get_updated_casting_cost(player, card, -1, event, cless);
			int i;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_play(player, i) && is_what(player, i, TYPE_CREATURE) ){
					int c2 = MIN(c1-get_cmc(player, i), 0);
					if( has_mana_multi(player, c2, black, blue, green, red, white) ){
						null_cost = 1;
						break;
					}
				}
			}
		}
		if( null_cost ){
			null_casting_cost(player, card);
		}
		get_card_instance(player, card)->info_slot = null_cost;
	}
}

static int pay_cost_for_emerge(int player, int card, event_t event, int cless, int black, int blue, int green, int red, int white){
	int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM, DLG_NO_CANCEL,
						"Pay Emerge cost", 1, 10,
						"Play normally", 1, 5);
	if( ! choice ){
		return 0;
	}

	if( choice == 1 ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (sac){
			int c1 = get_updated_casting_cost(player, card, -1, event, cless);
			int c2 = MIN(c1-get_cmc(BYTE2(sac), BYTE3(sac)), 0);
			charge_mana_multi(player, c2, black, blue, green, red, white);
			if( spell_fizzled != 1 ){
				int foul_emissary_flag = get_id(BYTE2(sac), BYTE3(sac)) == CARD_ID_FOUL_EMISSARY ? 1 : 0;
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				if( foul_emissary_flag ){
					generate_token_by_id(player, card, CARD_ID_ELDRAZI_HORROR);
				}
				return 1;
			}
			else{
				state_untargettable(BYTE2(sac), BYTE3(sac), 0);
				return 0;
			}
		}
		else{
			return 0;
		}
	}

	if( choice == 2 ){
		charge_mana_from_id(player, -1, event, get_id(player, card));
		if( spell_fizzled != 1 ){
			return 1;
		}
		else{
			return 0;
		}
	}

	if( ! choice ){
		spell_fizzled = 1;
		return 0;
	}

	return 0;
}

/***** Cards *****/

/*** Colorless ***/

int card_abundant_maw(int player, int card, event_t event)
{
/* Abundant Maw	|8	0x200eed1
 * Creature - Eldrazi Leech 6/4
 * Emerge |6|B
 * When you cast ~, target opponent loses 3 life and you gain 3 life. */

	modify_casting_cost_for_emerge(player, card, event, MANACOST_XB(6,1));

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) ){
			if( ! played_for_free(player, card) && get_card_instance(player, card)->info_slot == 1 ){
				if( ! pay_cost_for_emerge(player, card, event, MANACOST_XB(6,1)) ){
					spell_fizzled = 1;
				}
			}
			if( spell_fizzled != 1 ){
				lose_life(1-player, 3);
				gain_life(player, 3);
			}
		}
	}

	return 0;
}

int card_decimator_of_the_provinces(int player, int card, event_t event)
{
/* Decimator of the Provinces	|10	0x200eed6
 * Creature - Eldrazi Boar 7/7
 * Emerge |6|G|G|G
 * When you cast ~, creatures you control get +2/+2 and gain trample until end of turn.
 * Trample, haste */

	modify_casting_cost_for_emerge(player, card, event, MANACOST_XG(6,3));

	haste(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) ){
			if( ! played_for_free(player, card) && get_card_instance(player, card)->info_slot == 1 ){
				if( ! pay_cost_for_emerge(player, card, event, MANACOST_XG(6,3)) ){
					spell_fizzled = 1;
				}
			}
			if( spell_fizzled != 1 ){
				pump_subtype_until_eot(player, card, player, -1, 2, 2, KEYWORD_TRAMPLE, 0);
			}
		}
	}

	return 0;
}

int card_distended_mindbender(int player, int card, event_t event)
{
/* Distended Mindbender	|8	0x200eedb
 * Creature - Eldrazi Insect 5/5
 * Emerge |5|B|B
 * When you cast ~, target opponent reveals his or her hand.
 * You choose from it a nonland card with converted mana cost 3 or less and a card with converted mana cost 4 or greater.
 * That player discards those cards. */

	modify_casting_cost_for_emerge(player, card, event, MANACOST_XB(5,2));

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) ){
			if( ! played_for_free(player, card) && get_card_instance(player, card)->info_slot == 1 ){
				if( ! pay_cost_for_emerge(player, card, event, MANACOST_XB(5,2)) ){
					spell_fizzled = 1;
				}
			}
			if( spell_fizzled != 1 ){
				target_definition_t td2;
				default_target_definition(player, card, &td2, 0 );
				td2.zone = TARGET_ZONE_PLAYERS;
				if( would_validate_arbitrary_target(&td2, 1-player, -1) ){
					reveal_target_player_hand(1-player);
					int selected[2] = {-1};
					test_definition_t test;
					test.zone = TARGET_ZONE_HAND;
					new_default_test_definition(&test, TYPE_LAND, "Select a nonland card with converted mana cost 3 or less.");
					test.type_flag = DOESNT_MATCH;
					test.cmc = 4;
					test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
					if( check_battlefield_for_special_card(player, card, 1-player, CBFSC_CHECK_ONLY, &test) ){
						selected[0] = new_select_a_card(player, 1-player, TUTOR_FROM_HAND, 1, AI_MAX_VALUE, -1, &test);
					}
					new_default_test_definition(&test, TYPE_LAND, "Select a card with converted mana cost 4 or greater.");
					test.type_flag = DOESNT_MATCH;
					test.cmc = 3;
					test.cmc_flag = F5_CMC_GREATER_THAN_VALUE;
					if( check_battlefield_for_special_card(player, card, 1-player, CBFSC_CHECK_ONLY, &test) ){
						selected[1] = new_select_a_card(player, 1-player, TUTOR_FROM_HAND, 1, AI_MAX_VALUE, -1, &test);
					}
					int i;
					for(i=0; i<2; i++){
						if( selected[i] != -1 ){
							new_discard_card(1-player, selected[i], 0, player);
						}
					}
				}
			}
		}
	}

	return 0;
}


int card_drownyard_behemoth(int player, int card, event_t event)
{
/* Drownyard Behemoth	|9	0x200eee0
 * Creature - Eldrazi Crab 5/7
 * Flash
 * Emerge |7|U
 * ~ has hexproof as long as it entered the battlefield this turn. */

	modify_casting_cost_for_emerge(player, card, event, MANACOST_XU(7,1));

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) ){
			if( ! played_for_free(player, card) && get_card_instance(player, card)->info_slot == 1 ){
				if( ! pay_cost_for_emerge(player, card, event, MANACOST_XU(7,1)) ){
					spell_fizzled = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot |= 2;
	}

	if( event == EVENT_ABILITIES && in_play(player, card) && ! is_humiliated(player, card) &&
		(get_card_instance(player, card)->info_slot & 2) )
	{
		hexproof(player, card, event);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->info_slot = 0;
	}

	return 0;
}

int card_elder_deep_fiend(int player, int card, event_t event)
{
/* Elder Deep-Fiend	|8	0x200eee5
 * Creature - Eldrazi Octopus 5/6
 * Flash
 * Emerge |5|U|U
 * When you cast ~, tap up to four target permanents. */

	modify_casting_cost_for_emerge(player, card, event, MANACOST_XU(5, 2));

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) ){
			if( ! played_for_free(player, card) && get_card_instance(player, card)->info_slot == 1 ){
				if( ! pay_cost_for_emerge(player, card, event, MANACOST_XU(5, 2)) ){
					spell_fizzled = 1;
				}
			}
			if( spell_fizzled != 1 ){
				target_definition_t td2;
				default_target_definition(player, card, &td2, TYPE_PERMANENT );
				td2.allow_cancel = 2; //"Done" button

				card_instance_t *instance = get_card_instance(player, card);

				int trgs = 0;
				while( can_target(&td2) && trgs < 4 ){
						if( new_pick_target(&td2, "TARGET_PERMANENT", trgs, 0) ){
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
					tap_card(instance->targets[i].player, instance->targets[i].card);
				}

				instance->number_of_targets = 0;
			}
		}
	}

	return flash(player, card, event);
}

/* Emrakul, the Promised End	|13	0x000000
 * Legendary Creature - Eldrazi 13/13
 * ~ costs |1 less to cast for each card type among cards in your graveyard.
 * When you cast Emrakul, you gain control of target opponent during that player's next turn. After that turn, that player takes an extra turn.
 * Flying, trample, protection from instants */

int card_eternal_scourge(int player, int card, event_t event)
{
/* Eternal Scourge	|3	0x200eeea
 * Creature - Eldrazi Horror 3/3
 * You may cast ~ from exile.
 * When ~ becomes the target of a spell or ability an opponent controls, exile ~. */
	if (in_play(player, card) && !is_humiliated(player, card) &&
		becomes_target_of_spell_or_effect(player, card, event, player, card, ANYBODY))
	{
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_it_of_the_horrid_swarm(int player, int card, event_t event)
{
/* It of the Horrid Swarm	|8	0x200eee
 * Creature - Eldrazi Insect 4/4
 * Emerge |6|G
 * When you cast ~, put two 1/1 |Sgreen Insect creature tokens onto the battlefield. */
	modify_casting_cost_for_emerge(player, card, event, MANACOST_XG(6,1));

	haste(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) ){
			if( ! played_for_free(player, card) && get_card_instance(player, card)->info_slot == 1 ){
				if( ! pay_cost_for_emerge(player, card, event, MANACOST_XG(6,1)) ){
					spell_fizzled = 1;
				}
			}
			if( spell_fizzled != 1 ){
				token_generation_t token;
				default_token_definition(player, card, CARD_ID_INSECT, &token);
				token.pow = token.tou = 1;
				token.key_plus = KEYWORD_FLYING;
				token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
				token.qty = 2;
				generate_token(&token);
			}
		}
	}

	return 0;
}

int card_lashweed_lurker(int player, int card, event_t event)
{
/* Lashweed Lurker	|8	0x200eef4
 * Creature - Eldrazi Horror 5/4
 * Emerge |5|G|U
 * When you cast ~, you may put target nonland permanent on top of its owner's library. */

	modify_casting_cost_for_emerge(player, card, event, MANACOST_XGU(5, 1, 1));

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) ){
			if( ! played_for_free(player, card) && get_card_instance(player, card)->info_slot == 1 ){
				if( ! pay_cost_for_emerge(player, card, event, MANACOST_XGU(5, 1, 1)) ){
					spell_fizzled = 1;
				}
			}
			if( spell_fizzled != 1 ){
				target_definition_t td2;
				default_target_definition(player, card, &td2, TYPE_PERMANENT );
				td2.illegal_type = TYPE_LAND;

				card_instance_t *instance = get_card_instance(player, card);

				if( new_pick_target(&td2, "Select target nonland permanent.", 0, GS_LITERAL_PROMPT) ){
					put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
				}

				instance->number_of_targets = 0;
			}
		}
	}

	return 0;
}

int card_mockery_of_nature(int player, int card, event_t event)
{
/* Mockery of Nature	|9	0x200eef9
 * Creature - Eldrazi Beast 6/5
 * Emerge |7|G
 * When you cast ~, you may destroy target artifact or enchantment. */

	modify_casting_cost_for_emerge(player, card, event, MANACOST_XG(7, 1));

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) ){
			if( ! played_for_free(player, card) && get_card_instance(player, card)->info_slot == 1 ){
				if( ! pay_cost_for_emerge(player, card, event, MANACOST_XG(7, 1)) ){
					spell_fizzled = 1;
				}
			}
			if( spell_fizzled != 1 ){
				target_definition_t td2;
				default_target_definition(player, card, &td2, TYPE_ARTIFACT | TYPE_ENCHANTMENT );

				card_instance_t *instance = get_card_instance(player, card);

				if( new_pick_target(&td2, "DISENCHANT", 0, 0) ){
					kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
				}

				instance->number_of_targets = 0;
			}
		}
	}

	return 0;
}

int card_vexing_scuttler(int player, int card, event_t event)
{
/* Vexing Scuttler	|8	0x200eefe
 * Creature - Eldrazi Crab 4/5
 * Emerge |6|U
 * When you cast ~, you may return target instant or sorcery card from your graveyard to your hand. */

	modify_casting_cost_for_emerge(player, card, event, MANACOST_XU(6, 1));

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) ){
			if( ! played_for_free(player, card) && get_card_instance(player, card)->info_slot == 1 ){
				if( ! pay_cost_for_emerge(player, card, event, MANACOST_XU(6, 1)) ){
					spell_fizzled = 1;
				}
			}
			if( spell_fizzled != 1 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_SPELL, "Select an instant or sorcery card.");
				if( new_special_count_grave(player, &test) && ! graveyard_has_shroud(player) ){
					new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_CMC, &test);
				}
			}
		}
	}

	return 0;
}

int card_wretched_gryff(int player, int card, event_t event)
{
/* Wretched Gryff	|7	0x200ef03
 * Creature - Eldrazi Hippogriff 3/4
 * Emerge |5|U
 * When you cast ~, draw a card.
 * Flying */

	modify_casting_cost_for_emerge(player, card, event, MANACOST_XU(5, 1));

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) ){
			if( ! played_for_free(player, card) && get_card_instance(player, card)->info_slot == 1 ){
				if( ! pay_cost_for_emerge(player, card, event, MANACOST_XU(5, 1)) ){
					spell_fizzled = 1;
				}
			}
			if( spell_fizzled != 1 ){
				draw_cards(player, 1);
			}
		}
	}

	return 0;
}

/*** White ***/

int card_blessed_alliance(int player, int card, event_t event){
/* Blessed Alliance	|1|W	0x200ef08
 * Instant
 * Escalate |2
 * Choose one or more -
 * * Target player gains 4 life.
 * * Untap up to two target creatures.
 * * Target opponent sacrifices an attacking creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum
	{
		CHOICE_GAIN_LIFE 	= 1<<0,
		CHOICE_UNTAP 		= 1<<1,
		CHOICE_OPP_SAC 		= 1<<2,
		CHOICE_MASK = CHOICE_GAIN_LIFE | CHOICE_UNTAP | CHOICE_OPP_SAC,
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, 0);
	td2.zone = TARGET_ZONE_PLAYERS;

	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);
	test.state = STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int i;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			while( (instance->info_slot & CHOICE_MASK) != CHOICE_MASK ){
					int can_untap = !(instance->info_slot & CHOICE_UNTAP) && can_target(&td) ? 1 : 0;
					int can_target_opp = !(instance->info_slot & CHOICE_OPP_SAC) && would_validate_arbitrary_target(&td2, 1-player, -1) ? 1 : 0;
					if( player == AI && ! new_can_sacrifice(player, card, 1-player, &test) ){
						can_target_opp = 0;
					}
					int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
										"Target player gains 4 life", !(instance->info_slot & CHOICE_GAIN_LIFE), 5,
										"Untap up to two target creatures", can_untap, 10,
										"Target opponent sacrifices an attacking creature", can_target_opp, 15);

					if( ! choice ){
						spell_fizzled = 1;
						return 0;
					}
					instance->info_slot |= choice;
					if( has_mana(player, COLOR_ANY, 2) ){
						int choice2 = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_NO_CANCEL,
											"Escalate", 1, 10,
											"Decline", 1, 1);
						if( choice2 == 1 ){
							charge_mana(player, COLOR_ANY, 2);
							if( spell_fizzled == 1 ){
								spell_fizzled = 0;
								break;
							}
						}
						else{
							break;
						}
					}
					else{
						break;
					}
			}
		}
		int base_target = 0;
		if( instance->info_slot & CHOICE_UNTAP ){
			for(i=0; i<2; i++){
				if( new_pick_target(&td, "TARGET_CREATURE", base_target, 0) ){
					state_untargettable(instance->targets[base_target].player, instance->targets[base_target].card, 1);
					base_target++;
				}
				else{
					break;
				}
			}
			for(i=0; i<instance->number_of_targets; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			}
		}
		if( instance->info_slot & CHOICE_OPP_SAC ){
			instance->targets[base_target].player = 1-player;
			instance->targets[base_target].card = -1;
			instance->number_of_targets++;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int base_target = 0;
		if( instance->info_slot & CHOICE_GAIN_LIFE ){
			gain_life(player, 4);
		}
		if( instance->info_slot & CHOICE_UNTAP ){
			int i;
			for(i=0; i<instance->number_of_targets; i++){
				if( instance->targets[i].card != -1 ){
					if( validate_target(player, card, &td, i) ){
						untap_card(instance->targets[i].player, instance->targets[i].card);
					}
					base_target++;
				}
			}
		}
		if( instance->info_slot & CHOICE_OPP_SAC ){
			if( validate_target(player, card, &td2, base_target) ){
				new_sacrifice(player, card, 1-player, SAC_NO_CANCEL | SAC_CAUSED, &test);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_borrowed_grace(int player, int card, event_t event){
/* Borrowed Grace	|2|W	0x200ef0d
 * Instant
 * Escalate |1|W
 * Choose one or both -
 * * Creatures you control get +2/+0 until end of turn.
 * * Creatures you control get +0/+2 until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum
	{
		CHOICE_2_0 	= 1<<0,
		CHOICE_0_2 	= 1<<1,
		CHOICE_MASK = CHOICE_2_0 | CHOICE_0_2,
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			while( (instance->info_slot & CHOICE_MASK) != CHOICE_MASK ){
					int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
										"+2/+0 until end of turn", !(instance->info_slot & CHOICE_2_0), 10,
										"+0/+2 until end of turn", !(instance->info_slot & CHOICE_0_2), 5);

					if( ! choice ){
						spell_fizzled = 1;
						return 0;
					}
					instance->info_slot |= choice;
					if( has_mana_multi(player, MANACOST_XW(1, 1)) ){
						int choice2 = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_NO_CANCEL,
											"Escalate", 1, 10,
											"Decline", 1, 1);
						if( choice2 == 1 ){
							charge_mana_multi(player, MANACOST_XW(1, 1));
							if( spell_fizzled == 1 ){
								spell_fizzled = 0;
								break;
							}
						}
						else{
							break;
						}
					}
					else{
						break;
					}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int pow = (instance->info_slot & CHOICE_2_0) ? 2 : 0;
		int tou = (instance->info_slot & CHOICE_0_2) ? 2 : 0;
		pump_subtype_until_eot(player, card, player, -1, pow, tou, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_bruna_the_fading_light(int player, int card, event_t event){
/* Bruna, the Fading Light	|5|W|W	0x200ef12
 * Legendary Creature - Angel Horror 5/7
 * When you cast ~, you may return target Angel or Human creature card from your graveyard to the battlefield.
 * Flying, vigilance
 */
	check_legend_rule(player, card, event);
	vigilance(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) && ! graveyard_has_shroud(player) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select an Angel or Human creature card.");
			test.subtype = SUBTYPE_ANGEL;
			test.sub2 = SUBTYPE_HUMAN;
			test.subtype_flag = F2_MULTISUBTYPE;
			if( new_special_count_grave(player, &test) ){
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
			}
		}
	}

	return 0;
}
// Melds with:
int card_gisela_the_broken_blade(int player, int card, event_t event){
 /*
 * Gisela, the Broken Blade	|2|W|W	0x200ef17
 * Legendary Creature - Angel Horror 4/3
 * Flying, first strike, lifelink
 * At the beginning of your end step, if you both own and control ~ and a creature named Bruna, the Fading Light,
 * exile them, then meld them into Brisela, Voice of Nightmares.
*/
	check_legend_rule(player, card, event);
	lifelink(player, card, event);

	if( trigger_condition == TRIGGER_EOT && affect_me(player, card) && reason_for_trigger_controller == player ){
		int done = 0;
		int i;
		for(i=0; i<active_cards_count[player] && done != 3; i++){
			if( in_play(player, i) && get_owner(player, i) == player ){
				if( get_card_name(player, i) == CARD_ID_BRUNA_THE_FADING_LIGHT ){
					done |= 1;
				}
				if( get_card_name(player, i) == CARD_ID_GISELA_THE_BROKEN_BLADE ){
					done |= 2;
				}
			}
		}
		if( eot_trigger_mode(player, card, event, player, (done == 3 ? RESOLVE_TRIGGER_MANDATORY : 0)) ){
			done = 0;
			int marked[2] = {-1, -1};
			for(i=0; i<active_cards_count[player] && done != 3; i++){
				if( in_play(player, i) && get_owner(player, i) == player ){
					if( get_card_name(player, i) == CARD_ID_BRUNA_THE_FADING_LIGHT ){
						done |= 1;
						marked[0] = i;
					}
					if( get_card_name(player, i) == CARD_ID_GISELA_THE_BROKEN_BLADE ){
						done |= 2;
						marked[1] = i;
					}
				}
			}
			if( done == 3 ){
				for(i=0; i<2; i++){
					kill_card(player, marked[i], KILL_EXILE);
				}
				int card_added = add_card_to_hand(player, get_internal_card_id_from_csv_id(CARD_ID_BRISELA_VOICE_OF_NIGHTMARES));
				put_into_play(player, card_added);
				if( check_rfg( player, CARD_ID_GISELA_THE_BROKEN_BLADE ) && check_rfg( player, CARD_ID_BRUNA_THE_FADING_LIGHT ) ){
			        remove_card_from_rfg(player, CARD_ID_GISELA_THE_BROKEN_BLADE);
					remove_card_from_rfg(player, CARD_ID_BRUNA_THE_FADING_LIGHT);
				}
			}
		}
	}

	return 0;
}
// Meld into:
int card_brisela_voice_of_nightmare(int player, int card, event_t event){
/* Brisela, Voice of Nightmares	""	0x200ef1c
 * Legendary Creature - Eldrazi Angel 9/10
 * Flying, first strike, vigilance, lifelink
 * Your opponents can't cast spells with converted mana cost 3 or less.
 */

	check_legend_rule(player, card, event);
	lifelink(player, card, event);
	vigilance(player, card, event);

	/*if( trigger_condition == TRIGGER_LEAVE_PLAY && put_on_top_of_deck(player, card) ){
		if(event == EVENT_RESOLVE_TRIGGER){
	    obliterate_card(player, card);
	    put_on_top_of_deck(player,  get_internal_card_id_from_csv_id(CARD_ID_GISELA_THE_BROKEN_BLADE));
	    put_on_top_of_deck(player, get_internal_card_id_from_csv_id(CARD_ID_BRUNA_THE_FADING_LIGHT));
		}
	}  
		put_on_bottom_of_deck(instance->targets[0].player, instance->targets[0].card);
        int card_added = add_card_to_hand(player, get_internal_card_id_from_csv_id(CARD_ID_GISELA_THE_BROKEN_BLADE));
		int card_added2 = add_card_to_hand(player, get_internal_card_id_from_csv_id(CARD_ID_BRUNA_THE_FADING_LIGHT));
		put_into_play(player, card_added);
		put_into_play(player, card_added2);
		obliterate_card(player, card);*/

	if (event == EVENT_MODIFY_COST_GLOBAL)
	{
	  if ((affected_card_controller == -1 ? event_result : affected_card_controller) == 1-player
		  && !is_what(affected_card_controller, affected_card, TYPE_LAND) && get_cmc(affected_card_controller, affected_card) <= 3 &&
		  !is_humiliated(player, card))
		  infinite_casting_cost();
	  return 0;
	}

	return 0;
}

int card_choking_restraints(int player, int card, event_t event)
{
  /* Choking Restraints	|2|W	0x20024b5
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature can't attack or block.
   * |3|W|W, Sacrifice ~: Exile enchanted creature. */

  if (IS_GAA_EVENT(event))
	{
	  if (event == EVENT_RESOLVE_ACTIVATION)
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  int p = inst->damage_target_player;
		  int c = inst->damage_target_card;
		  if (p >= 0 && in_play(p, c))
			kill_card(p, c, KILL_EXILE);
		}

	  // Not GAA_REGENERATION, that checks if {player,card} can regenerate *itself*
	  return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XW(3,2), 0, NULL, NULL);
	}

  return card_pacifism(player, card, event);
}

int card_collective_effort(int player, int card, event_t event){
/* Collective Effort	|1|W|W	0x200ef21
 * Sorcery
 * Escalate-Tap an untapped creature you control.
 * Choose one or more -
 * * Destroy target creature with power 4 or greater.
 * * Destroy target enchantment.
 * * Put a +1/+1 counter on each creature target player controls. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum
	{
		CHOICE_KILL_CRIT 	= 1<<0,
		CHOICE_KILL_ENCH 	= 1<<1,
		CHOICE_PUMP_CRIT 	= 1<<2,
		CHOICE_MASK = CHOICE_KILL_CRIT | CHOICE_KILL_ENCH | CHOICE_PUMP_CRIT,
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.power_requirement = 4 | TARGET_PT_GREATER_OR_EQUAL;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_ENCHANTMENT);

	target_definition_t td3;
	default_target_definition(player, card, &td3, 0);
	td3.zone = TARGET_ZONE_PLAYERS;
	td3.preferred_controller = player;

	target_definition_t td4;
	default_target_definition(player, card, &td4, TYPE_CREATURE );
	td4.illegal_state = TARGET_STATE_TAPPED;
	td4.allowed_controller = td4.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return can_target(&td) || can_target(&td2) || can_target(&td3) ? 1 : 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int i;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			int to_tap[2] = {-1};
			while( (instance->info_slot & CHOICE_MASK) != CHOICE_MASK ){
					int can_kill_crit = !(instance->info_slot & CHOICE_KILL_CRIT) && can_target(&td) ? 1 : 0;
					int can_kill_ench = !(instance->info_slot & CHOICE_KILL_ENCH) && can_target(&td2) ? 1 : 0;
					int can_pump = !(instance->info_slot & CHOICE_PUMP_CRIT) && can_target(&td3) ? 1 : 0;
					int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
										"Destroy creature with POW >= 4", can_kill_crit, 15,
										"Destroy enchantment", can_kill_ench, 10,
										"+1/+1 to target player's crits", can_pump, 5);

					if( ! choice ){
						spell_fizzled = 1;
						return 0;
					}
					instance->info_slot |= choice;
					if( can_target(&td4) ){
						int choice2 = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_NO_CANCEL,
											"Escalate", 1, 10,
											"Decline", 1, 1);
						if( choice2 == 1 ){
							if( new_pick_target(&td4, "Select a creature you control to tap.", 0, GS_LITERAL_PROMPT) ){
								if( to_tap[0] == -1 ){
									to_tap[0] = instance->targets[0].card;
								}
								else{
									to_tap[1] = instance->targets[0].card;
								}
								state_untargettable(player, instance->targets[0].card, 1);
							}
							else{
								break;
							}
						}
						else{
							break;
						}
					}
					else{
						break;
					}
			}
			for(i=0; i<2; i++){
				if( to_tap[i] != -1 ){
					state_untargettable(player, instance->targets[i].card, 0);
					tap_card(player, instance->targets[i].card);
				}
			}
			instance->number_of_targets = 0;
		}
		int base_target = 0;
		if( instance->info_slot & CHOICE_KILL_CRIT ){
			if( new_pick_target(&td, "TARGET_CREATURE", base_target, 1) ){
				base_target++;
			}
			else{
				return 0;
			}
		}
		if( instance->info_slot & CHOICE_KILL_ENCH ){
			if( new_pick_target(&td2, "TARGET_ENCHANTMENT", base_target, 1) ){
				base_target++;
			}
			else{
				return 0;
			}
		}
		if( instance->info_slot & CHOICE_PUMP_CRIT  ){
			new_pick_target(&td3, "TARGET_PLAYER", base_target, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int base_target = 0;
		if( instance->info_slot & CHOICE_KILL_CRIT ){
			if( validate_target(player, card, &td, base_target) ){
				kill_card(instance->targets[base_target].player, instance->targets[base_target].card, KILL_DESTROY);
			}
			base_target++;
		}
		if( instance->info_slot & CHOICE_KILL_ENCH ){
			if( validate_target(player, card, &td2, base_target) ){
				kill_card(instance->targets[base_target].player, instance->targets[base_target].card, KILL_DESTROY);
			}
			base_target++;
		}
		if( instance->info_slot & CHOICE_PUMP_CRIT ){
			if( validate_target(player, card, &td3, base_target) ){
				manipulate_type(player, card, instance->targets[base_target].player, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_courageous_outrider(int player, int card, event_t event)
{
  /* Courageous Outrider	|3|W	0x200a9e4
   * Creature - Human Scout 3/4
   * When ~ enters the battlefield, look at the top four cards of your library. You may reveal a Human card from among them and put it into your hand. Put the rest on the bottom of your library in any order. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
	  int to_reveal = count_deck(player);
	  to_reveal = MIN(4, to_reveal);

	  if (to_reveal > 0)
		{
		  test_definition_t test;
		  new_default_test_definition(&test, 0, "Select a Human card.");
		  test.subtype = SUBTYPE_HUMAN;
		  test.subtype_flag = MATCH;
		  test.create_minideck = to_reveal;
		  test.no_shuffle = 1;

		  int card_added = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
		  if (card_added != -1)
			--to_reveal;
		  if (to_reveal > 0)
			put_top_x_on_bottom(player, player, to_reveal);
		}
	}

  return 0;
}

/* Dawn Gryff	|2|W	=>vanilla	0x401000
 * Creature - Hippogriff 2/2
 * Flying */

int card_deploy_the_gatewatch(int player, int card, event_t event)
{
/* Deploy the Gatewatch	|4|W|W	0x200ef26
 * Sorcery
 * Look at the top seven cards of your library. Put up to two planeswalker cards from among them onto the battlefield.
 * Put the rest on the bottom of your library in a random order. */

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = MIN(7, count_deck(player));
		if( ! amount ){
			kill_card(player, card, KILL_DESTROY);
			return 0;
		}

		test_definition_t test;
		new_default_test_definition(&test, TARGET_TYPE_PLANESWALKER, "Select a planeswalker card.");
		test.qty = 2;
		test.no_shuffle = 1;
		test.create_minideck = amount;

		int rest = amount-new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &test);

		if( rest ){
			put_top_x_on_bottom_in_random_order(player, amount);
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_desperate_sentry(int player, int card, event_t event)
{
  /* Desperate Sentry	|2|W	0x200e2ce
   * Creature - Human Soldier 1/2
   * When ~ dies, put a 3/2 colorless Eldrazi Horror creature token onto the battlefield.
   * Delirium - ~ gets +3/+0 as long as there are four or more card types among cards in your graveyard. */

  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	generate_token_by_id(player, card, CARD_ID_ELDRAZI_HORROR);

  if (event == EVENT_POWER && affect_me(player, card) && delirium(player) && !is_humiliated(player, card))
	event_result += 3;

  return 0;
}

int card_drogskol_shieldmate(int player, int card, event_t event)
{
  /* Drogskol Shieldmate	|2|W	0x200ec42
   * Creature - Spirit Soldier 2/3
   * Flash
   * When ~ enters the battlefield, other creatures you control get +0/+1 until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.not_me = 1;

	  pump_creatures_until_eot(player, card, player, 0, 0,1, 0,0, &test);
	}

  return flash(player, card, event);
}

int card_extricator_of_sin(int player, int card, event_t event)
{
/* Extricator of Sin	|2|W	0x200ef2b
 * Creature - Human Cleric 0/3
 * When ~ enters the battlefield, you may sacrifice another permanent. If you do, put a 3/2 colorless Eldrazi Horror creature token onto the battlefield.
 * Delirium - At the beginning of your upkeep, if there are four or more card types among cards in your graveyard, transform ~.
 */
	double_faced_card(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a permanent to sacrifice.");
		test.not_me = 1;

		if( new_sacrifice(player, card, player, SAC_DONE, &test) ){
			generate_token_by_id(player, card, CARD_ID_ELDRAZI_HORROR);
		}
	}

	if( eot_trigger_mode(player, card, event, player, delirium(player) ? RESOLVE_TRIGGER_MANDATORY : 0) ){
		transform(player, card);
	}

	return 0;
}
// --TRANSFORM--
int card_extricator_of_flesh(int player, int card, event_t event)
{
 /* Extricator of Flesh	""	0x200ef30
 * Creature - Eldrazi Horror 3/5
 * Eldrazi you control have vigilance.
 * |2, |T, Sacrifice a non-Eldrazi creature: Put a 3/2 colorless Eldrazi Horror creature token onto the battlefield. */

	if ( event == EVENT_ABILITIES && in_play(player, card) && ! is_humiliated(player, card) )
	{
		if( has_subtype(affected_card_controller, affected_card, SUBTYPE_ELDRAZI) ){
			vigilance(affected_card_controller, affected_card, event);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a non-Eldrazi creature to sacrifice.");
	test.subtype = SUBTYPE_ELDRAZI;

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL) ){
			return new_can_sacrifice_as_cost(player, card, &test);
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			if( sac ){
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				tap_card(player, card);
			}
			else{
				cancel = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_ELDRAZI_HORROR);
	}

	return 0;
}

int card_faith_unbroken(int player, int card, event_t event)
{
  /* Faith Unbroken	|3|W	0x200ec47
   * Enchantment - Aura
   * Enchant creature you control
   * When ~ enters the battlefield, exile target creature an opponent controls until ~ leaves the battlefield.
   * Enchanted creature gets +2/+2. */

  return_from_oblivion(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = 1-player;
	  td.allow_cancel = 0;

	  card_instance_t* inst = get_card_instance(player, card);

	  if (can_target(&td) && pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS"))
		obliviation(player, card, inst->targets[0].player, inst->targets[0].card);

	  inst->number_of_targets = 0;
	}

  // generic_aura() contains a bletcherous hack for "Enchant creature you control"
  return generic_aura(player, card, event, player, 2,2, 0,0, 0,0,0);
}

/* Faithbearer Paladin	|4|W	=>m10.c:card_child_of_night	0x200415c
 * Creature - Human Knight 3/4
 * Lifelink */

/* Fiend Binder	|3|W	=>m14.c:card_master_of_diversion	0x200aa8e
 * Creature - Human Soldier 3/2
 * Whenever ~ attacks, tap target creature defending player controls. */

int card_geist_of_the_lonely_vigil(int player, int card, event_t event)
{
  /* Geist of the Lonely Vigil	|1|W	0x200ec4c
   * Creature - Spirit Cleric 2/3
   * Defender, flying
   * Delirium - ~ can attack as though it didn't have defender as long as there are four or more card types among cards in your graveyard. */

  if (event == EVENT_ABILITIES && affect_me(player, card) && delirium(player) && !is_humiliated(player, card))
	add_status(player, card, STATUS_WALL_CAN_ATTACK);

  return 0;
}

/* Give No Ground	|3|W	0x000000
 * Instant
 * Target creature gets +2/+6 until end of turn and can block any number of creatures this turn. */

/* Guardian of Pilgrims	|1|W	=>lorwyn.c:card_kinsbaille_skirmisher	0x2007979
 * Creature - Spirit Cleric 2/2
 * When ~ enters the battlefield, target creature gets +1/+1 until end of turn. */

int card_ironclad_slayer(int player, int card, event_t event)
{
  /* Ironclad Slayer	|2|W	0x200ec51
   * Creature - Human Warrior 3/2
   * When ~ enters the battlefield, you may return target Aura or Equipment card from your graveyard to your hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
	  test_definition_t test;
	  new_default_test_definition(&test, 0, "Select target Aura or Equipment card.");
	  test.subtype = SUBTYPE_AURA;
	  test.sub2 = SUBTYPE_EQUIPMENT;
	  test.subtype_flag = F2_MULTISUBTYPE;

	  new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
	}

  return 0;
}

/* Ironwright's Cleansing	|2|W	=>scars_of_mirrodin.c:card_revoke_existence	0x2004391
 * Sorcery
 * Exile target artifact or enchantment. */

int card_lone_rider(int player, int card, event_t event)
{
  /* Lone Rider	|1|W	0x200ec56
   * Creature - Human Knight 1/1
   * First strike, lifelink
   * At the beginning of the end step, if you gained 3 or more life this turn, transform ~. */

  double_faced_card(player, card, event);

  lifelink(player, card, event);

  if (trigger_condition == TRIGGER_EOT && affect_me(player, card)
	  && get_trap_condition(player, TRAP_LIFE_GAINED) >= 3
	  && eot_trigger(player, card, event))
	transform(player, card);

  return 0;
}
// --TRANSFORM--
/* It That Rides as One	""	=>m10.c:card_child_of_night	0x200415c
 * Creature - Eldrazi Horror 4/4
 * First strike, trample, lifelink */

int card_long_road_home(int player, int card, event_t event)
{
  /* Long Road Home	|1|W	0x200ec5b
   * Instant
   * Exile target creature. At the beginning of the next end step, return that card to the battlefield under its owner's control with a +1/+1 counter on it. */

  /* As with Otherworldly Journey (which is identical except that it's Arcane), the +1/+1 counter is implemented by a
   * hack in remove_until_eot(). */
  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = player;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  remove_until_eot(player, card, inst->targets[0].player, inst->targets[0].card);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

/* Lunarch Mantle	|1|W	0x000000
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+2 and has "|1, Sacrifice a permanent: This creature gains flying until end of turn." */

/* Peace of Mind	|1|W	=>exodus.c:card_peace_of_mind	0x20036c6
 * Enchantment
 * |W, Discard a card: You gain 3 life. */

int card_providence(int player, int card, event_t event)
{
  /* Providence	|5|W|W	0x200ec60
   * Sorcery
   * You may reveal this card from your opening hand. If you do, at the beginning of the first upkeep, your life total becomes 26.
   * Your life total becomes 26. */

  // First ability approximated with the Rules Engine.

  if (event == EVENT_RESOLVE_SPELL)
	{
	  set_life_total(player, 26);
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_repel_the_abominable(int player, int card, event_t event)
{
  /* Repel the Abominable	|1|W	0x200ec65
   * Instant
   * Prevent all damage that would be dealt this turn by non-Human sources. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  fog_special2(player, card, ANYBODY, FOG_BY_ANY_TYPE | FOG_BY_NON_SUBTYPE, SUBTYPE_HUMAN);
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_sanctifier_of_souls(int player, int card, event_t event)
{
  /* Sanctifier of Souls	|3|W	0x200ec6a
   * Creature - Human Cleric 2/3
   * Whenever another creature enters the battlefield under your control, ~ gets +1/+1 until end of turn.
   * |2|W, Exile a creature card from your graveyard: Put a 1/1 |Swhite Spirit creature token with flying onto the battlefield. */

	if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card))
	{
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "");
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		pump_until_eot_merge_previous(player, card, player, card, 1,1);
	}

  if (event == EVENT_CAN_ACTIVATE)
	return CAN_ACTIVATE(player, card, MANACOST_XW(2,1)) && any_in_graveyard_by_type(player, TYPE_CREATURE);

  if (event == EVENT_ACTIVATE)
	{
	  charge_mana_for_activated_ability(player, card, MANACOST_XW(2,1));
	  if (cancel == 1)
		return 0;

	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card to exile.");
	  int selected = new_select_a_card(player, player, TUTOR_FROM_GRAVE, 0, AI_MIN_VALUE, -1, &test);
	  if (selected == -1)
		{
		  cancel = 1;
		  return 0;
		}

	  rfg_card_from_grave(player, selected);
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  token_generation_t token;
	  default_token_definition(player, card, CARD_ID_SPIRIT, &token);
	  token.key_plus = KEYWORD_FLYING;
	  token.color_forced = COLOR_TEST_WHITE;
	  generate_token(&token);
	}

  return 0;
}

/* Selfless Spirit	|1|W	=>alara_reborn.c:card_dauntless_escort	0x20083ce
 * Creature - Spirit Cleric 2/1
 * Flying
 * Sacrifice ~: Creatures you control gain indestructible until end of turn. */

static int is_aura_or_equipment(int player, int card){
	if( is_what(player, card, TYPE_ENCHANTMENT) && has_subtype(player, card, SUBTYPE_AURA) ){
		return 1;
	}
	if( is_what(player, card, TYPE_ARTIFACT) && has_subtype(player, card, SUBTYPE_EQUIPMENT) ){
		return 1;
	}
	return 0;
}

int card_sigardas_aid(int player, int card, event_t event){
/* Sigarda's Aid	|W	0x200ef35
 * Enchantment
 * You may cast Aura and Equipment spells as though they had flash.
 * Whenever an Equipment enters the battlefield under your control, you may attach it to target creature you control. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && reason_for_trigger_controller == player ){
		if( is_aura_or_equipment(trigger_cause_controller, trigger_cause) ){
			etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_PERMANENT, NULL, player, card);
		}
	}

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(trigger_cause_controller, trigger_cause, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;

		if( can_target(&td) && new_pick_target(&td, "Attaching equipment: select target creature.", 0, GS_LITERAL_PROMPT)){
			card_instance_t *equip = get_card_instance(trigger_cause_controller, trigger_cause);
			equip_target_creature(trigger_cause_controller, trigger_cause,
			equip->targets[0].player, equip->targets[0].card);
			equip->number_of_targets = 0;
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_hand(player, i) && ! is_what(player, i, TYPE_LAND) && is_aura_or_equipment(player, i) &&
				has_mana_to_cast_iid(player, event, get_card_instance(player, i)->internal_card_id)
			  ){
				int tr = can_legally_play_card_in_hand(player, i);
				if( tr ){
					if( tr == 99 ){
						return 99;
					}
					return 1;
				}
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int playable[2][hand_count[player]];
		int pc = 0;
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_hand(player, i) && ! is_what(player, i, TYPE_LAND) && is_aura_or_equipment(player, i) &&
				has_mana_to_cast_iid(player, event, get_card_instance(player, i)->internal_card_id)
			  ){
				if( can_legally_play_card_in_hand(player, i) )
				{
					playable[0][pc] = get_card_instance(player, i)->internal_card_id;
					playable[1][pc] = i;
					pc++;
				}
			}
		}

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a Aura or Equipment to play.");

		int selected = select_card_from_zone(player, player, playable[0], pc, 0, AI_MAX_VALUE, -1, &this_test);
		if( selected != -1 ){
			if( charge_mana_from_id(player, -1, event, cards_data[playable[0][selected]].id) ){
				play_card_in_hand_for_free(player, playable[1][selected]);
				cant_be_responded_to = 1;	// The spell will be respondable to, but this (fake) activation won't
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		else{
			spell_fizzled = 1;
			return 0;
		}
	}

	return global_enchantment(player, card, event);
}

/* Sigardian Priest	|1|W	=>innistrad.c:card_avacynian_priest	0x2004823
 * Creature - Human Cleric 1/2
 * |1, |T: Tap target non-Human creature. */

int card_spectral_reserves(int player, int card, event_t event)
{
  /* Spectral Reserves	|3|W	0x200ec6f
   * Sorcery
   * Put two 1/1 |Swhite Spirit creature tokens with flying onto the battlefield. You gain 2 life. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  token_generation_t token;
	  default_token_definition(player, card, CARD_ID_SPIRIT, &token);
	  token.qty = 2;
	  token.key_plus = KEYWORD_FLYING;
	  token.color_forced = COLOR_TEST_WHITE;
	  generate_token(&token);

	  gain_life(player, 2);

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_steadfast_cathar(int player, int card, event_t event)
{
  /* Steadfast Cathar	|1|W	0x200ec74
   * Creature - Human Soldier 2/1
   * Whenever ~ attacks, it gets +0/+2 until end of turn. */

  return when_attacks_pump_self(player, card, event, 0,2);
}

int card_subjugator_angel(int player, int card, event_t event)
{
  /* Subjugator Angel	|4|W|W	0x200ec88
   * Creature - Angel 4/3
   * Flying
   * When ~ enters the battlefield, tap all creatures your opponents control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		new_manipulate_all(player, card, 1-player, &test, ACT_TAP);
	}

	return 0;
}

int card_thalia_heretic_cathar(int player, int card, event_t event)
{
  /* Thalia, Heretic Cathar	|2|W	0x200ec8d
   * Legendary Creature - Human Soldier 3/2
   * First strike
   * Creatures and nonbasic lands your opponents control enter the battlefield tapped. */

  check_legend_rule(player, card, event);

  if (event == EVENT_CAST_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_LAND, "");
	  test.subtype = SUBTYPE_BASIC;
	  test.subtype_flag = DOESNT_MATCH;

	  permanents_enters_battlefield_tapped(player, card, event, 1-player, TYPE_CREATURE, &test);
	}

  return 0;
}

int card_thalias_lancers(int player, int card, event_t event)
{
  /* Thalia's Lancers	|3|W|W	0x200ec92
   * Creature - Human Knight 4/4
   * First strike
   * When ~ enters the battlefield, you may search your library for a legendary card, reveal it, put it into your hand, then shuffle your library. */


	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
	  test_definition_t test;
	  new_default_test_definition(&test, 0, "Select a legendary card.");
	  test.subtype = SUBTYPE_LEGEND;
	  test.subtype_flag = MATCH;

	  new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
	}

  return 0;
}

int card_thraben_standard_bearer(int player, int card, event_t event)
{
  /* Thraben Standard Bearer	|W	0x200ec97
   * Creature - Human Soldier 1/1
   * |1|W, |T, Discard a card: Put a 1/1 |Swhite Human Soldier creature token onto the battlefield. */

  if (event == EVENT_RESOLVE_ACTIVATION)
	generate_token_by_id(player, card, CARD_ID_HUMAN_SOLDIER);

  return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD, MANACOST_XW(1,1), 0, NULL, NULL);
}

/*** Blue ***/

int card_advanced_stitchwing(int player, int card, event_t event)
{
/* Advanced Stitchwing	|3|U|U	0x200ef3a
 * Creature - Zombie Horror 3/4
 * Flying
 * |2|U, Discard two cards: Return ~ from your graveyard to the battlefield tapped. */

	if( event == EVENT_GRAVEYARD_ABILITY && has_mana_multi(player, MANACOST_XU(2, 1)) ){
		if( hand_count[player] >= 2 ){
			return GA_RETURN_TO_PLAY_MODIFIED;
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		charge_mana_multi(player, MANACOST_XU(2, 1));
		if( spell_fizzled != 1 ){
			new_multidiscard(player, 2, 0, player);
			return GAPAID_REMOVE;
		}
	}

	if( event == EVENT_RETURN_TO_PLAY_FROM_GRAVE_MODIFIED ){
		add_state(player, card, STATE_TAPPED);
	}

	return 0;
}

int card_chilling_grasp(int player, int card, event_t event)
{
  /* Chilling Grasp	|2|U	0x200ec9c
   * Instant
   * Tap up to two target creatures. Those creatures don't untap during their controller's next untap step.
   * Madness |3|U */

  if (IS_GS_EVENT(player, card, event))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);

	  if (event == EVENT_RESOLVE_SPELL)
		{
		  card_instance_t* inst = get_card_instance(player, card);

		  int i;
		  for (i = 0; i < inst->number_of_targets; ++i)
			if (validate_target(player, card, &td, i))
			  does_not_untap_effect(player, card, inst->targets[i].player, inst->targets[i].card, EDNT_TAP_TARGET, 1);

		  kill_card(player, card, KILL_DESTROY);
		}

	  return generic_spell(player, card, event, GS_CAN_TARGET | GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 2, NULL);
	}

  return madness(player, card, event, MANACOST_XU(3,1));
}

int card_coax_from_the_blind_eternities(int player, int card, event_t event)
{
/* Coax from the Blind Eternities	|2|U	0x200ef3f
 * Sorcery
 * You may choose an Eldrazi card you own from outside the game or in exile, reveal that card, and put it into your hand. */
	if ( event == EVENT_RESOLVE_SPELL )
	{
		int iid = -1;
		if( player == HUMAN ){
			iid = card_from_list(player, 4, TYPE_ANY, 0, SUBTYPE_ELDRAZI, 0, 0, 0, 0, 0, -1, 0);
		}
		else{
			test_definition_t test;
			default_test_definition(&test, TYPE_ANY);
			test.subtype = SUBTYPE_ELDRAZI;
			int selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_MAX_VALUE, -1, &test);
			iid = selected != -1 ? deck_ptr[player][selected] : -1;
		}
		if( iid > -1 ){
			add_card_to_hand(player, iid);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/* Contingency Plan	|1|U	=>khans_of_tarkir.c:card_taigams_scheming	0x200d00e
 * Sorcery
 * Look at the top five cards of your library. Put any number of them into your graveyard and the rest back on top of your library in any order. */

/* Convolute	|2|U	=>ravnica.c:card_convolute	0x20066a5
 * Instant
 * Counter target spell unless its controller pays |4. */

int card_curious_humunculus(int player, int card, event_t event)
{
/* Curious Homunculus	|1|U	0x200ef44
 * Creature - Homunculus 1/1
 * |T: Add |C to your mana pool. Spend this mana only to cast an instant or sorcery spell.
 * At the beginning of your upkeep, if there are three or more instant and/or sorcery cards in your graveyard, transform ~.
 */

	// The mana ability can't be coded as we haven't any way right now to restrict mana usage

	double_faced_card(player, card, event);

	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_SPELL);
		if( upkeep_trigger_mode(player, card, event, new_special_count_grave(player, &test) >= 3 ? RESOLVE_TRIGGER_MANDATORY : 0)){
			transform(player, card);
		}
	}

	return 0;
}
// --TRANSFORM--
int card_voracious_reader(int player, int card, event_t event)
{
/* Voracious Reader	""	0x200ef49
 * Creature - Eldrazi Homunculus 3/4
 * Prowess
 * Instant and sorcery spells you cast cost |1 less to cast. */

	prowess(player, card, event);

	if (event == EVENT_MODIFY_COST_GLOBAL)
	{
	  if ((affected_card_controller == -1 ? event_result : affected_card_controller) == player
		  && is_what(affected_card_controller, affected_card, TYPE_SPELL) && !is_humiliated(player, card))
		COST_COLORLESS -= 1;
	  return 0;
	}

	return 0;
}

int card_displace(int player, int card, event_t event)
{
  /* Displace	|2|U	0x200eca1
   * Instant
   * Exile up to two target creatures you control, then return those cards to the battlefield under their owner's control. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = td.preferred_controller = player;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* inst = get_card_instance(player, card);

	  /* Yes, they should all be exiled, then all be returned, instead of interspersing the exiling and returning like
	   * this.  On the other hand, this makes any etb/ltb triggers they have see the other target like they should.
	   * Ghostly Flicker has a similar issue. */
	  int i;
	  for (i = 0; i < inst->number_of_targets; ++i)
		if (validate_target(player, card, &td, i))
		  blink_effect(inst->targets[i].player, inst->targets[i].card, 0);

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET | GS_OPTIONAL_TARGET, &td, "ASHNODS_BATTLEGEAR", 2, NULL);
}

int card_docent_of_perfection(int player, int card, event_t event)
{
  /* Docent of Perfection	|3|U|U	0x200eca6
   * Creature - Insect Horror 5/4
   * Flying
   * Whenever you cast an instant or sorcery spell, put a 1/1 |Sblue Human Wizard creature token onto the battlefield. Then if you control three or more Wizards, transform ~. */

  double_faced_card(player, card, event);

  if (specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY,
							TYPE_SPELL,MATCH, 0,0, 0,0, 0,0, -1,0))
	{
	  generate_token_by_id(player, card, CARD_ID_HUMAN_WIZARD);
	  if (count_subtype(player, TYPE_PERMANENT, SUBTYPE_WIZARD) >= 3)
		transform(player, card);
	}

  return 0;
}
// --TRANSFORM--
int card_final_iteration(int player, int card, event_t event)
{
  /* Final Iteration	""	0x200ecab
   * Creature - Eldrazi Insect 6/5
   * Flying
   * Wizards you control get +2/+1 and have flying.
   * Whenever you cast an instant or sorcery spell, put a 1/1 |Sblue Human Wizard creature token onto the battlefield. */

  boost_creature_type(player, card, event, SUBTYPE_WIZARD, 2,1, KEYWORD_FLYING, BCT_CONTROLLER_ONLY|BCT_INCLUDE_SELF);

  if (specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY,
							TYPE_SPELL,MATCH, 0,0, 0,0, 0,0, -1,0))
	generate_token_by_id(player, card, CARD_ID_HUMAN_WIZARD);

  return 0;
}

/* Drag Under	|2|U	=>invasion.c:card_repulse	0x200115a
 * Sorcery
 * Return target creature to its owner's hand.
 * Draw a card. */

int card_enlightened_maniac(int player, int card, event_t event)
{
  /* Enlightened Maniac	|3|U	0x200ecb0
   * Creature - Human 0/2
   * When ~ enters the battlefield, put a 3/2 colorless Eldrazi Horror creature token onto the battlefield. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
		generate_token_by_id(player, card, CARD_ID_ELDRAZI_HORROR);

  return 0;
}

/* Exultant Cultist	|2|U	=>scars_of_mirrodin.c:card_darkslick_drake	0x2004201
 * Creature - Human Wizard 2/2
 * When ~ dies, draw a card. */

int card_fogwalker(int player, int card, event_t event)
{
  /* Fogwalker	|1|U	0x200ecb5
   * Creature - Spirit 1/3
   * Skulk
   * When ~ enters the battlefield, target creature an opponent controls doesn't untap during its controller's next untap step. */

  skulk(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = 1-player;
	  td.allow_cancel = 0;
	  if (IS_AI(player))
		{
		  td.required_state = TARGET_STATE_TAPPED;
		  if (!can_target(&td))
			td.required_state = 0;
		}

	  card_instance_t* inst = get_card_instance(player, card);
	  inst->number_of_targets = 0;
	  if (can_target(&td) && pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS"))
		does_not_untap_effect(player, card, inst->targets[0].player, inst->targets[0].card, 0, 1);
	}

  return 0;
}

int card_fortunes_favor(int player, int card, event_t event)
{
/* Fortune's Favor	|3|U	0x200ef4e
 * Instant
 * Target opponent looks at the top four cards of your library and separates them into a face-down pile and a face-up pile. Put one pile into your hand and the other into your graveyard. */

	if (event == EVENT_RESOLVE_SPELL ){
		int max = MIN(count_deck(player), 4);
		if( max ){
			int face_up_pile[4];
			int fup_count = 0;
			int face_down_pile[4];
			int fdp_count = 0;

			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a card for the face-down pile.");
			test.no_shuffle = 1;
			int ai_mode = AI_MAX_VALUE;
			int ai_count = 2;
			while( max ){
					test.create_minideck = max;
					int selected = new_select_a_card(1-player, player, TUTOR_FROM_DECK, 0, ai_mode, -1, &test);
					if( selected != -1 ){
						face_down_pile[fdp_count] = deck_ptr[player][selected];
						remove_card_from_deck(player, selected);
						fdp_count++;
						max--;
						if( ai_mode == AI_MAX_VALUE ){
							ai_mode = AI_MIN_VALUE;
						}
						if( player == AI ){
							ai_count++;
							if( ai_count >= max / 2 ){
								break;
							}
						}
					}
					else{
						break;
					}
			}
			while( max ){
					face_up_pile[fup_count] = deck_ptr[player][0];
					remove_card_from_deck(player, 0);
					fup_count++;
					max--;
			}

			int will_choose_the_face_up_pile = fup_count == 0 ? 0 : 1;
			if( will_choose_the_face_up_pile ){
				if( player == HUMAN ){
					new_default_test_definition(&test, TYPE_ANY, "Click on a card to select the face-up pile.");
					if( select_card_from_zone(player, player, face_up_pile, fup_count, 0, AI_MAX_CMC, -1, &test) == -1 ){
						will_choose_the_face_up_pile = 0;
					}
				}
				else{
					if( fdp_count > fup_count ){
						int total_value = 0;
						int c;
						for(c = 0; c < fup_count; c++){
							total_value += get_base_value(-1, face_up_pile[c]);
						}
						if( total_value < 150 ){
							will_choose_the_face_up_pile = 0;
						}
					}
					if( fdp_count == fup_count ){
						if( internal_rand(100)+1 >= 75 ){
							will_choose_the_face_up_pile = 0;
						}
					}
					if( fdp_count < fup_count ){
						int total_value = 0;
						int c;
						for(c = 0; c < fup_count; c++){
							total_value += get_base_value(-1, face_up_pile[c]);
						}
						if( total_value < 100 ){
							will_choose_the_face_up_pile = 0;
						}
					}
				}
			}
			if( will_choose_the_face_up_pile ){
				show_deck(1-player, face_up_pile, fup_count, "Opponent chosen the face-up pile.", 0, 0x7375B0 );
			}
			else{
				show_deck(1-player, face_down_pile, fdp_count, "Opponent chosen the face-down pile.", 0, 0x7375B0 );
			}

			int i;
			for(i=0; i<fup_count; i++){
				if( will_choose_the_face_up_pile ){
					add_card_to_hand(player, face_up_pile[i]);
				}
				if( ! will_choose_the_face_up_pile ){
					raw_put_iid_on_top_of_deck(player, face_up_pile[i]);
				}
			}
			for(i=0; i<fdp_count; i++){
				if( ! will_choose_the_face_up_pile ){
					add_card_to_hand(player, face_down_pile[i]);
				}
				if( will_choose_the_face_up_pile ){
					raw_put_iid_on_top_of_deck(player, face_down_pile[i]);
				}
			}
			if( will_choose_the_face_up_pile ){
				mill(player, fdp_count);
			}
			else{
				mill(player, fup_count);
			}
 		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_geist_of_the_archives(int player, int card, event_t event)
{
  /* Geist of the Archives	|2|U	0x200ecba
   * Creature - Spirit 0/4
   * Defender
   * At the beginning of your upkeep, scry 1. */

  upkeep_trigger_ability(player, card, event, player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	scry(player, 1);

  return 0;
}

int card_grizzled_angler(int player, int card, event_t event)
{
  /* Grizzled Angler	|2|U	0x200ecbf
   * Creature - Human 2/3
   * |T: Put the top two cards of your library into your graveyard. Then if there is a colorless creature card in your graveyard, transform ~. */

  double_faced_card(player, card, event);

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  mill(player, 2);

	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.color = COLOR_TEST_ANY_COLORED;
	  test.color_flag = DOESNT_MATCH;

	  card_instance_t* inst = get_card_instance(player, card);
	  if (in_play(inst->parent_controller, inst->parent_card)
		  && !check_special_flags3(inst->parent_controller, inst->parent_card, SF3_CARD_IS_FLIPPED)
		  && new_special_count_grave(player, &test))
		transform(inst->parent_controller, inst->parent_card);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}
// --TRANSFORM--
int card_grisly_anglerfish(int player, int card, event_t event)
{
  /* Grisly Anglerfish	""	0x200ecc4
   * Creature - Eldrazi Fish 4/5
   * |6: Creatures your opponents control attack this turn if able. */

  if (event == EVENT_RESOLVE_ACTIVATION)
	pump_subtype_until_eot(player, card, 1-player, -1, 0,0, 0,SP_KEYWORD_MUST_ATTACK);

  return generic_activated_ability(player, card, event, GAA_NONE, MANACOST_X(6), 0, NULL, NULL);
}

int card_identity_thief(int player, int card, event_t event)
{
/* Identity Thief	|2|U|U	0x200ef53
 * Creature - Shapeshifter 0/3
 * Whenever ~ attacks, you may exile another target nontoken creature. If you do, ~ becomes a copy of that creature until end of turn.
 * Return the exiled card to the battlefield under its owner's control at the beginning of the next end step. */

	if( event == EVENT_CHANGE_TYPE && affect_me(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->dummy3 > 0 ){
			event_result = instance->dummy3;
		}
	}

	store_attackers(player, card, event, RESOLVE_TRIGGER_AI(player), player, card, NULL);

	if (xtrigger_condition() == XTRIGGER_ATTACKING && affect_me(player, card) && reason_for_trigger_controller == player)
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.illegal_type = TARGET_TYPE_TOKEN;
		td.special = TARGET_SPECIAL_NOT_ME;

		card_instance_t *instance = get_card_instance(player, card);
		instance->number_of_targets = 0;

		if( can_target(&td) && resolve_declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
			if( new_pick_target(&td, "Select another target nontoken creature.", 0, GS_LITERAL_PROMPT) ){
				instance->dummy3 = get_card_instance(instance->targets[0].player, instance->targets[0].card)->internal_card_id;
				remove_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->dummy3 = 0;
	}

	return 0;
}

int card_imprisoned_by_the_moon(int player, int card, event_t event ){
/* Imprisoned in the Moon	|2|U	0x200ef58
 * Enchantment - Aura
 * Enchant creature, land, or planeswalker
 * Enchanted permanent is a colorless land with "|T: Add |C to your mana pool" and loses all other card types and abilities. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_CHANGE_TYPE && affect_me(p, c) ){
			get_card_instance(p, c)->targets[1].card = event_result;
			if( instance->targets[1].player == -1 ){
				instance->targets[1].player = get_internal_card_id_from_csv_id(CARD_ID_WASTES);
			}
			event_result = instance->targets[1].player;
		}

		/* 7/13/2016: The permanent will keep any supertypes it previously had. Notably, if Imprisoned in the Moon is enchanting a
		 * legendary permanent, that permanent will continue to be legendary. */
		if( event == EVENT_STATIC_EFFECTS && affect_me(p, c) ){
			if( get_card_instance(p, c)->targets[1].card > -1 ){
				if( is_legendary(-1, get_card_instance(p, c)->targets[1].card) ){
					check_legend_rule(p, c, event);
				}
			}
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_LAND | TARGET_TYPE_PLANESWALKER);

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target creature, land, or planeswalker.");
}

int card_ingenious_skaab(int player, int card, event_t event)
{
  /* Ingenious Skaab	|2|U	0x200ecc9
   * Creature - Zombie Horror 2/3
   * Prowess
   * |U: ~ gets +1/-1 until end of turn. */

  prowess(player, card, event);

  return generic_shade_merge_pt(player, card, event, 0, MANACOST_U(1), 1,-1);
}

/* Laboratory Brute	|3|U	=>innistrad.c:card_armored_skaab	0x2004819
 * Creature - Zombie Horror 3/3
 * When ~ enters the battlefield, put the top four cards of your library into your graveyard. */

int card_lunar_force(int player, int card, event_t event)
{
  /* Lunar Force	|2|U	0x200ecce
   * Enchantment
   * When an opponent casts a spell, sacrifice ~ and counter that spell. */

  if (new_specific_spell_played(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, NULL))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  target_t spl = inst->targets[1];	// struct copy
	  kill_card(player, card, KILL_SACRIFICE);
	  kill_card(spl.player, spl.card, KILL_SACRIFICE);
	}

  return global_enchantment(player, card, event);
}

int card_mausoleum_wanderer(int player, int card, event_t event)
{
  /* Mausoleum Wanderer	|U	0x200ecd3
   * Creature - Spirit 1/1
   * Flying
   * Whenever another Spirit enters the battlefield under your control, ~ gets +1/+1 until end of turn.
   * Sacrifice ~: Counter target instant or sorcery spell unless its controller pays |X, where X is ~'s power. */

	if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card))
	{
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "");
		this_test.subtype = SUBTYPE_SPIRIT;
		this_test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &this_test, player, card);
	}


	if( event == EVENT_ETB_ABILITY ){
		pump_until_eot_merge_previous(player, card, player, card, 1, 1);
	}

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  counterspell_target_definition(player, card, &td, TYPE_SPELL);

  if (event == EVENT_ACTIVATE)
	{
	  int pow = get_power(player, card);
	  if (pow <= 0 || has_mana(card_on_stack_controller, COLOR_COLORLESS, pow))
		ai_modifier -= 20;
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	counterspell_resolve_unless_pay_x(player, card, &td, 0, get_power(player, card));

  return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME|GAA_SPELL_ON_STACK, MANACOST0, 0, &td, NULL);
}

int card_minds_dilatation(int player, int card, event_t event)
{
/* Mind's Dilation	|5|U|U	0x200ef5d
 * Enchantment
 * Whenever an opponent casts his or her first spell each turn, that player exiles the top card of his or her library.
 * If it's a nonland card, you may cast it without paying its mana cost. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && reason_for_trigger_controller == player)
	{
		if( trigger_cause_controller == 1-player && get_specific_storm_count(1-player) == 1 && deck_ptr[1-player][0] != -1 ){
			if( new_specific_spell_played(player, card, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL) ){
				show_deck(player, deck_ptr[1-player], 1, "This card will be exiled by Mind's Dilatation.", 0, 0x7375B0 );
				int iid = deck_ptr[1-player][0];
				rfg_top_card_of_deck(1-player);
				if( can_legally_play_iid(player, iid) && ! is_what(-1, iid, TYPE_LAND) ){
					int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM, DLG_NO_CANCEL, DLG_FULLCARD_ID(iid),
										"Play this card", 1, 10,
										"Decline", 1, 1);
					if( choice == 1 ){
						play_card_in_exile_for_free(player, 1-player, cards_data[iid].id);
					}
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_nebelgast_herald(int player, int card, event_t event)
{
  /* Nebelgast Herald	|2|U	0x200ecd8
   * Creature - Spirit 2/1
   * Flash
   * Flying
   * Whenever ~ or another Spirit enters the battlefield under your control, tap target creature an opponent controls. */

	if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) )
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_SPIRIT;
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.allow_cancel = 0;

		card_instance_t* inst = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS") )
		{
			tap_card(inst->targets[0].player, inst->targets[0].card);
		}
		inst->number_of_targets = 0;
	}

	return flash(player, card, event);
}

int card_niblis_of_frost(int player, int card, event_t event)
{
  /* Niblis of Frost	|2|U|U	0x200ecdd
   * Creature - Spirit 3/3
   * Flying
   * Prowess
   * Whenever you cast an instant or sorcery spell, tap target creature an opponent controls. That creature doesn't untap during its controller's next untap step. */

  if (prowess_mode(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	{
	  alternate_legacy_text(1, player, pump_until_eot_merge_previous(player, card, player, card, 1, 1));
	  if (is_what(trigger_cause_controller, trigger_cause, TYPE_SPELL))
		{
		  target_definition_t td;
		  default_target_definition(player, card, &td, TYPE_CREATURE);
		  td.allowed_controller = 1-player;
		  td.allow_cancel = 0;

		  card_instance_t* inst = get_card_instance(player, card);
		  inst->number_of_targets = 0;
		  if (can_target(&td) && pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS"))
			{
			  tap_card(inst->targets[0].player, inst->targets[0].card);
			  alternate_legacy_text(2, player,
									does_not_untap_effect(player, card,
														  inst->targets[0].player, inst->targets[0].card, 0, 1));
			}
		}
	}

  return 0;
}

int card_scour_the_laboratory(int player, int card, event_t event)
{
  /* Scour the Laboratory	|4|U|U	0x200ece2
   * Instant
   * Delirium - ~ costs |2 less to cast if there are four or more card types among cards in your graveyard.
   * Draw three cards. */

  if (event == EVENT_MODIFY_COST && delirium(player))
	COST_COLORLESS -= 2;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  draw_cards(player, 3);
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_spontaneous_mutation(int player, int card, event_t event)
{
  /* Spontaneous Mutation	|U	0x200ece7
   * Enchantment - Aura
   * Flash
   * Enchant creature
   * Enchanted creature gets -X/-0, where X is the number of cards in your graveyard. */

  if (event == EVENT_CHECK_PUMP && !(in_play(player, card)))
	return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, -count_graveyard(player),0, 0,0);

  int pow;
  card_instance_t* inst;
  if (event == EVENT_POWER
	  && (inst = in_play(player, card))
	  && affect_me(inst->damage_target_player, inst->damage_target_card))
	pow = -count_graveyard(player);
  else
	pow = -1;

  return generic_aura(player, card, event, 1-player, pow,0, 0,0, 0,0,0);
}

/* Summary Dismissal	|2|U|U	0x000000
 * Instant
 * Exile all other spells and counter all abilities. */

int card_take_inventory(int player, int card, event_t event)
{
  /* Take Inventory	|1|U	0x200ecec
   * Sorcery
   * Draw a card, then draw cards equal to the number of cards named ~ in your graveyard. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  draw_cards(player, 1 + count_graveyard_by_id(player, CARD_ID_TAKE_INVENTORY));
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

/* Tattered Haunter	|1|U	=>m12.c:card_skywinder_drake	0x2004049
 * Creature - Spirit 2/1
 * Flying
 * ~ can block only creatures with flying. */

/* Turn Aside	|U	=>scars_of_mirrodin.c:card_turn_aside	0x2004431
 * Instant
 * Counter target spell that targets a permanent you control. */

int card_unsubstantiate(int player, int card, event_t event)
{
/* Unsubstantiate	|1|U	0x200ef62
 * Instant
 * Return target spell or creature to its owner's hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td1;
	counterspell_target_definition(player, card, &td1, TYPE_ANY);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( counterspell(player, card, event, &td1, 0)  ){
			return 99;
		}
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->info_slot = 0;
		if( counterspell(player, card, EVENT_CAN_CAST, &td1, 0)  ){
			instance->info_slot = 66;
			return counterspell(player, card, event, &td1, 0);
		}
		else{
			generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
			if( spell_fizzled != 1 ){
				instance->info_slot = 67;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int good = 1*(instance->info_slot == 66 && counterspell_validate(player, card, &td1, 0)) |
				   2*(instance->info_slot == 67 && valid_target(&td));
		if( good ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_wharf_infiltrator(int player, int card, event_t event)
{
  /* Wharf Infiltrator	|1|U	0x200ecf1
   * Creature - Human Horror 1/1
   * Skulk
   * Whenever ~ deals combat damage to a player, you may draw a card. If you do, discard a card.
   * Whenever you discard a creature card, you may pay |2. If you do, put a 3/2 colorless Eldrazi Horror creature token onto the battlefield. */

  skulk(player, card, event);

  if (damage_dealt_by_me(player, card, event,
						 DDBM_TRIGGER_OPTIONAL | DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE)
	  && can_draw_cards_as_cost(player, 1))
	{
	  draw_a_card(player);
	  discard(player, 0, player);
	}

  if (trigger_condition == TRIGGER_DISCARD && affect_me(player, card)
	  && is_what(trigger_cause_controller, trigger_cause, TYPE_CREATURE)
	  && discard_trigger(player, card, event, player, RESOLVE_TRIGGER_AI(player), 0)
	  && charge_mana_while_resolving(player, card, event, player, COLOR_COLORLESS, 2))
	generate_token_by_id(player, card, CARD_ID_ELDRAZI_HORROR);

  return 0;
}

/*** Black ***/

int card_boon_of_emrakul(int player, int card, event_t event)
{
  /* Boon of Emrakul	|2|B	0x200ecf6
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +3/-3. */

  return generic_aura(player, card, event, ANYBODY, 3,-3, 0,0, 0,0,0);
}

int card_borrowed_malevolence(int player, int card, event_t event){
/* Borrowed Malevolence	|B	0x200ef67
 * Instant
 * Escalate |2
 * Choose one or both -
 * * Target creature gets +1/+1 until end of turn.
 * * Target creature gets -1/-1 until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum
	{
		CHOICE_PUMP 	= 1<<0,
		CHOICE_WEAKEN 	= 1<<1,
		CHOICE_MASK = CHOICE_PUMP | CHOICE_WEAKEN,
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return can_target(&td) || can_target(&td2) ? 1 : 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			while( (instance->info_slot & CHOICE_MASK) != CHOICE_MASK ){
					int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
										"Target gets +1/+1 until eot.", !(instance->info_slot & CHOICE_PUMP), 5,
										"Target gets -1/-1 until eot.", !(instance->info_slot & CHOICE_WEAKEN), 10);

					if( ! choice ){
						spell_fizzled = 1;
						return 0;
					}
					instance->info_slot |= choice;
					if( has_mana(player, COLOR_ANY, 2) ){
						int choice2 = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_NO_CANCEL,
											"Escalate", 1, 10,
											"Decline", 1, 1);
						if( choice2 == 1 ){
							charge_mana(player, COLOR_ANY, 2);
							if( spell_fizzled == 1 ){
								spell_fizzled = 0;
								break;
							}
						}
						else{
							break;
						}
					}
					else{
						break;
					}
			}
		}
		int base_target = 0;
		if( instance->info_slot & CHOICE_PUMP ){
			if( new_pick_target(&td, "Select target creature (+1/+1)", base_target, 1 | GS_LITERAL_PROMPT) ){
				base_target++;
			}
			else{
				return 0;
			}
		}
		if( instance->info_slot & CHOICE_WEAKEN ){
			if( ! new_pick_target(&td2, "Select target creature (-1/-1)", base_target, 1 | GS_LITERAL_PROMPT) ){
				return 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int base_target = 0;
		if( instance->info_slot & CHOICE_PUMP ){
			if( validate_target(player, card, &td, base_target) ){
				pump_until_eot(player, card, instance->targets[base_target].player, instance->targets[base_target].card, 1, 1);
			}
			base_target++;
		}
		if( instance->info_slot & CHOICE_WEAKEN ){
			if( validate_target(player, card, &td2, base_target) ){
				pump_until_eot(player, card, instance->targets[base_target].player, instance->targets[base_target].card, -1, -1);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_cemetery_recruitment(int player, int card, event_t event)
{
  /* Cemetery Recruitment	|1|B	0x200ecfb
   * Sorcery
   * Return target creature card from your graveyard to your hand. If it's a Zombie card, draw a card. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  test_definition_t test;
  new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");

  if (event == EVENT_RESOLVE_SPELL)
	{
	  int pos = validate_target_from_grave_source(player, card, player, 0);
	  if (pos != -1)
		{
		  int zombie = has_subtype(-1, get_grave(player)[pos], SUBTYPE_ZOMBIE);
		  from_grave_to_hand(player, pos, TUTOR_HAND);
		  if (zombie)
			draw_a_card(player);
		}

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
}

int card_certain_death(int player, int card, event_t event)
{
  /* Certain Death	|5|B	0x200ed00
   * Sorcery
   * Destroy target creature. Its controller loses 2 life and you gain 2 life. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  kill_card(inst->targets[0].player, inst->targets[0].card, KILL_DESTROY);
		  lose_life(inst->targets[0].player, 2);
		  gain_life(player, 2);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_collective_brutality(int player, int card, event_t event){
/* Collective Brutality	|1|B	0x200ef6c
 * Sorcery
 * Escalate-Discard a card.
 * Choose one or more -
 * * Target opponent reveals his or her hand. You choose an instant or sorcery card from it. That player discards that card.
 * * Target creature gets -2/-2 until end of turn.
 * * Target opponent loses 2 life and you gain 2 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum
	{
		CHOICE_DURESS 		= 1<<0,
		CHOICE_WEAKEN 		= 1<<1,
		CHOICE_SUCK_LIFE 	= 1<<2,
		CHOICE_MASK = CHOICE_DURESS | CHOICE_WEAKEN | CHOICE_SUCK_LIFE,
	};

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return would_validate_arbitrary_target(&td, 1-player, -1) || can_target(&td2) ? 1 : 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int i;
		int to_discard[2];
		int td_count = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			int hand_backup[2][hand_count[player]];
			int hb_count = 0;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_hand(player, i) ){
					hand_backup[0][hb_count] = get_original_internal_card_id(player, i);
					hand_backup[1][hb_count] = i;
					hb_count++;
				}
			}
			while( (instance->info_slot & CHOICE_MASK) != CHOICE_MASK ){
					int abils[3] ={ (!(instance->info_slot & CHOICE_DURESS) && would_validate_arbitrary_target(&td, 1-player, -1)),
									(!(instance->info_slot & CHOICE_WEAKEN) && can_target(&td2)),
									(!(instance->info_slot & CHOICE_SUCK_LIFE) && would_validate_arbitrary_target(&td, 1-player, -1))
					};
					int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
										"Opponent discard an instant or sorcery card", abils[0], 15,
										"Target creature gets -2/-2", abils[1], 10,
										"Suck 2 life", !(instance->info_slot & CHOICE_SUCK_LIFE), life[1-player] <= 2 ? 20 : 5);

					if( ! choice ){
						spell_fizzled = 1;
						return 0;
					}
					instance->info_slot |= choice;
					if( hand_count[player] > td_count ){
						int choice2 = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_NO_CANCEL,
											"Escalate", 1, 10,
											"Decline", 1, 1);
						if( choice2 == 1 ){
							test_definition_t test;
							new_default_test_definition(&test, TYPE_ANY, "Select a card to discard.");
							test.id = cards_data[904].id;
							test.id_flag = DOESNT_MATCH;
							int selected = select_card_from_zone(player, player, hand_backup[0], hb_count, 0, AI_MIN_VALUE, -1, &test);
							if( selected != -1 ){
								to_discard[td_count] = hand_backup[1][selected];
								hand_backup[0][selected] = 904;
								td_count++;
							}
							else{
								break;
							}
						}
						else{
							break;
						}
					}
					else{
						break;
					}
			}
		}
		int base_target = 0;
		if( instance->info_slot & CHOICE_DURESS ){
			instance->targets[0].player = 1-player;
			instance->targets[0].card = -1;
			instance->number_of_targets = 1;
			base_target++;
		}
		if( instance->info_slot & CHOICE_WEAKEN ){
			new_pick_target(&td, "Select target creature (-2/-2).", base_target, 1 | GS_LITERAL_PROMPT);
			base_target++;
		}
		if( instance->info_slot & CHOICE_SUCK_LIFE ){
			instance->targets[base_target].player = 1-player;
			instance->targets[base_target].card = -1;
			instance->number_of_targets++;
		}
		if( spell_fizzled != 1 ){
			for(i=0; i<td_count; i++){
				new_discard_card(player, to_discard[i], 0, player);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int base_target = 0;
		if( instance->info_slot & CHOICE_DURESS ){
			if( validate_target(player, card, &td, base_target) ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_SPELL, "Select an instant or sorcery card.");
				int selected = new_select_a_card(player, 1-player, TUTOR_FROM_HAND, 1, AI_MAX_VALUE, -1, &test);
				if( selected != -1 ){
					new_discard_card(1-player, selected, 0, player);
				}
			}
			base_target++;
		}
		if( instance->info_slot & CHOICE_WEAKEN ){
			if( validate_target(player, card, &td2, base_target) ){
				pump_until_eot(player, card, instance->targets[base_target].player, instance->targets[base_target].card, -2, -2);
			}
			base_target++;
		}
		if( instance->info_slot & CHOICE_SUCK_LIFE ){
			if( validate_target(player, card, &td, base_target) ){
				lose_life(instance->targets[base_target].player, 2);
				gain_life(player, 2);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_cryptbreaker(int player, int card, event_t event)
{
  /* Cryptbreaker	|B	0x200ed05
   * Creature - Zombie 1/1
   * |1|B, |T, Discard a card: Put a 2/2 |Sblack Zombie creature token onto the battlefield.
   * Tap three untapped Zombies you control: You draw a card and you lose 1 life. */

  if (!IS_ACTIVATING(event))
	return 0;

  target_definition_t zombie;
  base_target_definition(player, card, &zombie, TYPE_PERMANENT);
  zombie.allowed_controller = zombie.preferred_controller = player;
  zombie.required_subtype = SUBTYPE_ZOMBIE;
  zombie.illegal_state = TARGET_STATE_TAPPED;

  int can_token = hand_count[player] >= 1;
  int can_draw = target_available(player, card, &zombie) >= 3;

  enum
  {
	CHOICE_TOKEN = 1,
	CHOICE_DRAW
  } choice = DIALOG(player, card, event,
					DLG_RANDOM,
					"Zombie token",			can_token,	2,	DLG_MANA(MANACOST_XB(1,1)), DLG_TAP,
					"Draw and lose life",	can_draw,	3,	DLG_MANA(MANACOST0));

  if (event == EVENT_CAN_ACTIVATE)
	return choice && can_use_activated_abilities(player, card);
  else if (event == EVENT_ACTIVATE)
	switch (choice)
	  {
		case CHOICE_TOKEN:
		  discard(player, 0, player);
		  break;

		case CHOICE_DRAW:
		  if (!tapsubtype_ability(player, card, 3, &zombie))
			cancel = 1;
		  break;
	  }
  else	// event == EVENT_ACTIVATION
	switch (choice)
	  {
		case CHOICE_TOKEN:
		  generate_token_by_id(player, card, CARD_ID_ZOMBIE);
		  break;

		case CHOICE_DRAW:
		  draw_a_card(player);
		  lose_life(player, 1);
		  break;
	  }

  return 0;
}

int card_dark_salvation(int player, int card, event_t event)
{
/* Dark Salvation	|X|X|B	0x200ef71
 * Sorcery
 * Target player puts X 2/2 |Sblack Zombie creature tokens onto the battlefield,
 * then up to one target creature gets -1/-1 until end of turn for each Zombie that player controls. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE );
	td2.allow_cancel = 2;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int amount = charge_mana_for_double_x(player, COLOR_ANY);
		if( spell_fizzled != 1 ){
			set_x_for_x_spells(player, card, event, amount/2);
			set_special_flags2(player, card, SF2_X_SPELL);
		}
		if( new_pick_target(&td, "TARGET_PLAYER", 0, 1) ){
			new_pick_target(&td2, "TARGET_CREATURE", 1, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
			token.pow = token.tou = 2;
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
			token.qty = instance->info_slot;
			generate_token(&token);

			if( instance->number_of_targets == 2 && validate_target(player, card, &td2, 1) ){
				int amount = count_subtype(instance->targets[0].player, TYPE_PERMANENT, SUBTYPE_ZOMBIE);
				pump_until_eot(player, card, instance->targets[1].player, instance->targets[1].card, -amount, -amount);
			}
		}
	}

	return 0;
}

int card_dusk_feaster(int player, int card, event_t event)
{
  /* Dusk Feaster	|5|B|B	0x200ed0a
   * Creature - Vampire 4/5
   * Delirium - ~ costs |2 less to cast if there are four or more card types among cards in your graveyard.
   * Flying */

  if (event == EVENT_MODIFY_COST && delirium(player))
	COST_COLORLESS -= 2;

  return 0;
}

/* Gavony Unhallowed	|3|B	=>innistrad.c:card_unruly_mob	0x2004aad
 * Creature - Zombie 2/4
 * Whenever another creature you control dies, put a +1/+1 counter on ~. */

int card_graf_harvest(int player, int card, event_t event)
{
/* Graf Harvest	|B	0x200ef76
 * Enchantment
 * Zombies you control have menace.
 * |3|B, Exile a creature card from your graveyard: Put a 2/2 |Sblack Zombie creature token onto the battlefield. */

	if( event == EVENT_ABILITIES && in_play(player, card) && ! is_humiliated(player, card) )
	{
		if( affected_card_controller == player && has_subtype(affected_card_controller, affected_card, SUBTYPE_ZOMBIE) ){
			menace(affected_card_controller, affected_card, event);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_XB(3, 1), 0, NULL, NULL) ){
			return new_special_count_grave(player, &test) ? 1 : 0;
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_XB(3, 1)) ){
			if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &test) == -1 ){
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		token_generation_t token;
		default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_ZOMBIE, &token);
		token.pow = token.tou = 2;
		token.color_forced = get_sleighted_color_test(instance->parent_controller, instance->parent_card, COLOR_TEST_BLACK);
		generate_token(&token);
	}

	return 0;
}

int card_graf_rats(int player, int card, event_t event){
/* Graf Rats	|1|B	0x200ef7b
 * Creature - Rat 2/1
 * At the beginning of combat on your turn, if you both own and control ~ and a creature named Midnight Scavengers,
 * exile them, then meld them into Chittering Host.*/

	if( beginning_of_combat(player, card, event, player, card) ){
		int done = 0;
		int marked[2] = {-1, -1};
		int i;
		for(i=0; i<active_cards_count[player] && done != 3; i++){
			if( in_play(player, i) && get_owner(player, i) == player ){
				if( get_card_name(player, i) == CARD_ID_GRAF_RATS ){
					done |= 1;
					marked[0] = i;
				}
				if( get_card_name(player, i) == CARD_ID_MIDNIGHT_SCAVENGERS ){
					done |= 2;
					marked[1] = i;
				}
			}
		}
		if( done == 3 ){
			for(i=0; i<2; i++){
				kill_card(player, marked[i], KILL_EXILE);
			}
			int card_added = add_card_to_hand(player, get_internal_card_id_from_csv_id(CARD_ID_CHITTERING_HOST));
			put_into_play(player, card_added);
		}
	}

	return 0;
}
// --MELD WITH--
int card_midnight_scavengers(int player, int card, event_t event){
 /* Midnight Scavengers	|4|B	0x200ef80
 * Creature - Human Rogue 3/3
 * When ~ enters the battlefield, you may return target creature card with converted mana cost 3 or less from your graveyard to your hand.*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if(	! graveyard_has_shroud(player) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card with converted mana cost 3 or less.");
			test.cmc = 4;
			test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
			if( new_special_count_grave(player, &test) ){
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_CMC, &test);
			}
		}
	}

	return 0;
}
// --MELD TO--
int card_chittering_host(int player, int card, event_t event){
 /* Chittering Host	""	0x200ef85
 * Creature - Eldrazi Horror 5/6
 * Haste
 * Menace
 * When ~ enters the battlefield, other creatures you control get +1/+0 and gain menace until end of turn. */

	haste(player, card, event);

	menace(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.not_me = 1;
		pump_creatures_until_eot(player, card, player, 0, 1, 0, 0, SP_KEYWORD_MENACE, &test);
	}

	return 0;
}

int card_haunted_dead(int player, int card, event_t event)
{
/* Haunted Dead	|3|B	0x200ef8a
 * Creature - Zombie 2/2
 * When ~ enters the battlefield, put a 1/1 |Swhite Spirit creature token with flying onto the battlefield.
 * |1|B, Discard two cards: Return ~ from your graveyard to the battlefield tapped. */

	if( event == EVENT_GRAVEYARD_ABILITY && has_mana_multi(player, MANACOST_XB(1, 1)) ){
		if( hand_count[player] >= 2 ){
			return GA_RETURN_TO_PLAY_MODIFIED;
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		charge_mana_multi(player, MANACOST_XB(1, 1));
		if( spell_fizzled != 1 ){
			new_multidiscard(player, 2, 0, player);
			return GAPAID_REMOVE;
		}
	}

	if( event == EVENT_RETURN_TO_PLAY_FROM_GRAVE_MODIFIED ){
		add_state(player, card, STATE_TAPPED);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SPIRIT, &token);
		token.pow = token.tou = 1;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		token.key_plus = KEYWORD_FLYING;
		generate_token(&token);
	}

	return 0;
}

int card_liliana_the_last_hope(int player, int card, event_t event){
/* Liliana, the Last Hope	|1|B|B	0x200ef8f
 * Planeswalker - Liliana (3)
 * +1: Up to one target creature gets -2/-1 until your next turn.
 * -2: Put the top two cards of your library into your graveyard, then you may return a creature card from your graveyard to your hand.
 * -7: You get an emblem with "At the beginning of your end step, put X 2/2 |Sblack Zombie creature tokens onto the battlefield, where X is two plus the number of Zombies you control." */

	if( ! IS_GAA_EVENT(event) ){
		return planeswalker(player, card, event, 3);
	}

	enum{
		CHOICE_WEAKEN = 1,
		CHOICE_MILL,
		CHOICE_EMBLEM
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int priority_mill = 3*count_graveyard_by_type(player, TYPE_CREATURE);

		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
							"Weaken a creature", 	1, 10, 				+1,
							"Mill & Raise Dead", 	1, priority_mill, 	-2,
							"Emblem", 				1, 15, 				-7);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_WEAKEN ){
			new_pick_target(&td, "TARGET_CREATURE", 0, 0);
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		if( choice == CHOICE_WEAKEN ){
				if( instance->number_of_targets && can_target(&td) && valid_target(&td)){
				pump_ability_t pump;
				default_pump_ability_definition(player, card, &pump, -2, -1, 0, 0);
				pump.paue_flags = PAUE_END_AT_THE_BEGINNING_OF_YOUR_NEXT_TURN;
				pump.eot_removal_method = KILL_DESTROY;
				pump_ability(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, &pump);
			}
		}
		if( choice == CHOICE_MILL ){
			mill(player, 2);
			if( count_graveyard(player) ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
			}
		}
		if( choice == CHOICE_EMBLEM ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_LILIANA_THE_LAST_HOPES_EMBLEM);
		}
	}

	return planeswalker(player, card, event, 3);
}

int card_liliana_the_last_hopes_emblem(int player, int card, event_t event){
/* Liliana, the Last Hope's Emblem	""	0x200ef94
 * Emblem - Liliana
 * At the beginning of your end step, put X 2/2 |Sblack Zombie creature tokens onto the battlefield,
 * where X is two plus the number of Zombies you control. */

	if( current_turn == player && eot_trigger(player, card, event) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
		token.pow = token.tou = 2;
		token.color_forced = COLOR_TEST_BLACK;
		token.qty = 2+count_subtype(player, TYPE_PERMANENT, SUBTYPE_ZOMBIE);
		generate_token(&token);
	}

	return 0;
}

int card_lilianas_elite(int player, int card, event_t event)
{
  /* Liliana's Elite	|2|B	0x200ed0f
   * Creature - Zombie 1/1
   * ~ gets +1/+1 for each creature card in your graveyard. */

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && !is_humiliated(player, card))
	event_result += count_graveyard_by_type(player, TYPE_CREATURE);

  return 0;
}

int card_markov_crusader(int player, int card, event_t event)
{
  /* Markov Crusader	|4|B	0x200ed14
   * Creature - Vampire Knight 4/3
   * Lifelink
   * ~ has haste as long as you control another Vampire. */

  lifelink(player, card, event);

  if (event == EVENT_ABILITIES && affect_me(player, card) && !is_humiliated(player, card))
	{
	  int c;
	  for (c = 0; c < active_cards_count[player]; ++c)
		if (in_play(player, c) && c != card && has_subtype(player, c, SUBTYPE_VAMPIRE))
		  {
			haste(player, card, event);
			break;
		  }
	}

  return 0;
}

/* Murder	|1|B|B	=>m13.c:card_murder2	0x20067ea
 * Instant
 * Destroy target creature. */

int card_noosegraf_mob(int player, int card, event_t event)
{
  /* Noosegraf Mob	|4|B|B	0x200ed19
   * Creature - Zombie 0/0
   * ~ enters the battlefield with five +1/+1 counters on it.
   * Whenever a player casts a spell, remove a +1/+1 counter from ~. If you do, put a 2/2 |Sblack Zombie creature token onto the battlefield. */

  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 5);

  if (specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, 0,0, 0,0, 0,0, 0,0, -1,0)
	  && count_1_1_counters(player, card) >= 1)
	{
		remove_1_1_counter(player, card);

		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
		token.pow = token.tou = 2;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		generate_token(&token);
	}

  return 0;
}

int card_oath_of_liliana(int player, int card, event_t event)
{
/* Oath of Liliana	|2|B	0x200ef99
 * Legendary Enchantment
 * When ~ enters the battlefield, each opponent sacrifices a creature.
 * At the beginning of each end step, if a planeswalker entered the battlefield under your control this turn,
 * put a 2/2 |Sblack Zombie creature token onto the battlefield. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_sacrifice(player, card, 1-player, SAC_CAUSED | SAC_NO_CANCEL, &this_test);
	}

	if( trigger_condition == TRIGGER_EOT && affect_me(player, card) && reason_for_trigger_controller == player ){
		int done = get_trap_condition(player, TRAP_PLANESWALKERS_PLAYED) + get_trap_condition(1-player, TRAP_PLANESWALKERS_PLAYED);
		if( done && eot_trigger(player, card, event) ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
			token.pow = token.tou = 2;
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
			generate_token(&token);
		}
	}

	return global_enchantment(player, card, event);
}

int card_olivias_dragoon(int player, int card, event_t event)
{
  /* Olivia's Dragoon	|1|B	0x200ed1e
   * Creature - Vampire Berserker 2/2
   * Discard a card: ~ gains flying until end of turn. */

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (in_play(inst->parent_controller, inst->parent_card))
		pump_ability_until_eot_no_repeat(player, card, inst->parent_controller, inst->parent_card, KEYWORD_FLYING,0);
	}

  return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
}

int card_prying_questions(int player, int card, event_t event)
{
  /* Prying Questions	|2|B	0x200ed23
   * Sorcery
   * Target opponent loses 3 life and puts a card from his or her hand on top of his or her library. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.allowed_controller = 1-player;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  int oppo = get_card_instance(player, card)->targets[0].player;

		  lose_life(oppo, 3);

		  if (hand_count[oppo] >= 1)
			{
			  test_definition_t test;
			  new_default_test_definition(&test, TYPE_ANY, "Select a card to put on top.");
			  new_global_tutor(oppo, oppo, TUTOR_FROM_HAND, TUTOR_DECK, 1, AI_MIN_VALUE, &test);
			}
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, "TARGET_OPPONENT", 1, NULL);
}

/* Rise from the Grave	|4|B	=>m11.c:card_rise_from_the_grave	0x2002735
 * Sorcery
 * Put target creature card from a graveyard onto the battlefield under your control. That creature is a |Sblack Zombie in addition to its other colors and types. */

int card_ruthless_disposal(int player, int card, event_t event){
/* Ruthless Disposal	|4|B	0x200ef9e
 * Sorcery
 * As an additional cost to cast ~, discard a card and sacrifice a creature.
 * Two target creatures each get -13/-13 until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_CAST ){
		int rvalue = generic_spell(player, card, event, GS_SAC_CREATURE_AS_COST, NULL, NULL, 0, NULL);
		if( rvalue && hand_count[player] > 1 ){
			return target_available(player, card, &td) >= 2 ? 1 : 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if( sac){
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
			new_default_test_definition(&test, TYPE_ANY, "Select a card to discard.");
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &test);
			if( selected != -1 ){
				if( new_pick_target(&td, "TARGET_CREATURE", 0, 1) ){
					state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
					new_pick_target(&td, "TARGET_CREATURE", 1, 1);
					state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
				}
			}
			else{
				spell_fizzled = 1;
			}
			if( spell_fizzled != 1 ){
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				new_discard_card(player, selected, 0, player);
			}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if(event == EVENT_RESOLVE_SPELL){
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				pump_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, -13, -13);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_skirsdag_supplicant(int player, int card, event_t event)
{
  /* Skirsdag Supplicant	|2|B	0x200ed28
   * Creature - Human Cleric 2/3
   * |B, |T, Discard a card: Each player loses 2 life. */

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  lose_life(current_turn, 2);
	  lose_life(1-current_turn, 2);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD, MANACOST_B(1), 0, NULL, NULL);
}

int card_strange_augmentation(int player, int card, event_t event)
{
  /* Strange Augmentation	|B	0x200ed2d
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+1.
   * Delirium - Enchanted creature gets an additional +2/+2 as long as there are four or more card types among cards in your graveyard. */

  int pt;
  card_instance_t* inst;
  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS)
	  && (inst = in_play(player, card))
	  && affect_me(inst->damage_target_player, inst->damage_target_card)
	  && delirium(player))
	pt = 3;
  else
	pt = 1;

  return generic_aura(player, card, event, player, pt,pt, 0,0, 0,0,0);
}

int card_stromkirk_condemned(int player, int card, event_t event)
{
  /* Stromkirk Condemned	|B|B	0x200ed32
   * Creature - Vampire Horror 2/2
   * Discard a card: Vampires you control get +1/+1 until end of turn. Activate this ability only once each turn. */
  // Used externally
  if (event == EVENT_CHECK_PUMP && card_stromkirk_condemned(player, card, EVENT_CAN_ACTIVATE))
	{
	  pumpable_power[player] += 1;
	  pumpable_toughness[player] += 1;
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, 0, "");
	  test.subtype = SUBTYPE_VAMPIRE;
	  test.subtype_flag = MATCH;

	  pump_creatures_until_eot_merge_pt(player, card, player, 1,1, &test);
	}

  return generic_activated_ability(player, card, event, GAA_DISCARD | GAA_ONCE_PER_TURN, MANACOST0, 0, NULL, NULL);
}

/* Succumb to Temptation	|1|B|B	=>fifth_dawn.c:card_nights_whispers	0x200cd39
 * Instant
 * You draw two cards and you lose 2 life. */

int card_thraben_foulbloods(int player, int card, event_t event)
{
/* Thraben Foulbloods	|2|B	0x200efa3
 * Creature - Zombie Hound 3/2
 * Delirium - ~ gets +1/+1 and has menace as long as there are four or more card types among cards in your graveyard. */

	if( affect_me(player, card) && in_play(player, card) && ! is_humiliated(player, card) )
	{
		switch( event ){
				case EVENT_POWER:
				case EVENT_TOUGHNESS:
					event_result += delirium(player);
					break;
				case EVENT_ABILITIES:
				{
					if( delirium(player) ){
						menace(player, card, event);
					}
					break;
				}
				default:
					break;
		}
	}

	return 0;
}

int card_tree_of_perdition(int player, int card, event_t event)
{
  /* Tree of Perdition	|3|B	0x200ed37
   * Creature - Plant 0/13
   * Defender
   * |T: Exchange target opponent's life total with ~'s toughness. */

  // Deliberately not !is_humiliated() - the exchange is unaffected even if this card later loses its abilities
  if (event == EVENT_TOUGHNESS && affect_me(player, card))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (HIWORD(inst->targets[1].card) == CARD_ID_TREE_OF_PERDITION)
		event_result = SLOWORD(inst->targets[1].card);
	}

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.allowed_controller = 1-player;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  card_instance_t* parent = in_play(inst->parent_controller, inst->parent_card);
	  if (parent)
		{
		  int tgh = get_toughness(inst->parent_controller, inst->parent_card);
		  parent->targets[1].card = (CARD_ID_TREE_OF_PERDITION << 16) | LOWORD(life[inst->targets[0].player]);
		  set_life_total(inst->targets[0].player, tgh);
		}
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_ONLY_TARGET_OPPONENT,
								   MANACOST0, 0, &td, "TARGET_OPPONENT");
}

/* Vampire Cutthroat	|B	=>shadows_over_innistrad.c:card_farbog_revenant	0x200e931
 * Creature - Vampire Rogue 1/1
 * Skulk
 * Lifelink */

int card_voldaren_pariah(int player, int card, event_t event)
{
  /* Voldaren Pariah	|3|B|B	0x200ed41
   * Creature - Vampire Horror 3/3
   * Flying
   * Sacrifice three other creatures: Transform ~.
   * Madness |B|B|B */

  double_faced_card(player, card, event);

  if (IS_ACTIVATING(event))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "Select another creature to sacrifice.");
	  test.not_me = 1;
	  test.qty = 3;

	  if (event == EVENT_CAN_ACTIVATE)
		return CAN_ACTIVATE0(player, card) && new_can_sacrifice_as_cost(player, card, &test);

	  if (event == EVENT_ACTIVATE && charge_mana_for_activated_ability(player, card, MANACOST0))
		new_sacrifice(player, card, player, SAC_ALL_OR_NONE|SAC_AS_COST, &test);

	  if (event == EVENT_RESOLVE_ACTIVATION)
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  if (in_play(inst->parent_controller, inst->parent_card)
			  && !check_special_flags3(inst->parent_controller, inst->parent_card, SF3_CARD_IS_FLIPPED))
			transform(inst->parent_controller, inst->parent_card);
		}
	}

  return madness(player, card, event, MANACOST_B(3));
}
// --TRANSFORM--
int card_abolisher_of_bloodlines(int player, int card, event_t event)
{
  /* Abolisher of Bloodlines	""	0x200ed46
   * Creature - Eldrazi Vampire 6/5
   * Flying
   * When this creature transforms into ~, target opponent sacrifices three creatures. */

  if (event == EVENT_TRANSFORMED && target_opponent(player, card))
	impose_sacrifice(player, card, 1-player, 3, TYPE_CREATURE,MATCH, 0,0, 0,0, 0,0, -1,0);

  return 0;
}

/* Wailing Ghoul	|1|B	=>dark_ascension.c:card_screeching_skaab	0x2004e77
 * Creature - Zombie 1/3
 * When ~ enters the battlefield, put the top two cards of your library into your graveyard. */

/* Weirded Vampire	|3|B	=>shadows_over_innistrad.c:card_twins_of_maurer_estate	0x200e9cc
 * Creature - Vampire Horror 3/3
 * Madness |2|B */

int card_whispers_of_emrakul(int player, int card, event_t event)
{
  /* Whispers of Emrakul	|1|B	0x200ed4b
   * Sorcery
   * Target opponent discards a card at random.
   * Delirium - If there are four or more card types among cards in your graveyard, that player discards two cards at random instead. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.allowed_controller = 1-player;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		new_multidiscard(get_card_instance(player, card)->targets[0].player,
						 delirium(player) ? 2 : 1, DISC_RANDOM, player);

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, "TARGET_OPPONENT", 1, NULL);
}

/*** Red ***/

int card_abandon_reason(int player, int card, event_t event)
{
  /* Abandon Reason	|2|R	0x200ed50
   * Instant
   * Up to two target creatures each get +1/+0 and gain first strike until end of turn.
   * Madness |1|R */

  if (event == EVENT_CHECK_PUMP)
	return vanilla_instant_pump(player, card, event, player, player, 1,0, KEYWORD_FIRST_STRIKE,0);

  if (IS_GS_EVENT(player, card, event))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.preferred_controller = player;

	  if (event == EVENT_RESOLVE_SPELL)
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  int i;
		  for (i = 0; i < inst->number_of_targets; ++i)
			if (validate_target(player, card, &td, i))
			  pump_ability_until_eot(player, card, inst->targets[i].player, inst->targets[i].card,
									 1,0, KEYWORD_FIRST_STRIKE,0);

		  kill_card(player, card, KILL_DESTROY);
		}

	  return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 2, NULL);
	}

  return madness(player, card, event, MANACOST_XR(1,1));
}

int card_alchemists_greeting(int player, int card, event_t event)
{
  /* Alchemist's Greeting	|4|R	0x200ed55
   * Sorcery
   * ~ deals 4 damage to target creature.
   * Madness |1|R */

  if (IS_GS_EVENT(player, card, event))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);

	  if (event == EVENT_RESOLVE_SPELL)
		{
		  if (valid_target(&td))
			damage_target0(player, card, 4);

		  kill_card(player, card, KILL_DESTROY);
		}

	  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

  return madness(player, card, event, MANACOST_XR(1,1));
}

static void damage_blocked_blocking_creature_and_controller(int player, int card, int t_player, int t_card)
{
	damage_creature(t_player, t_card, 3, player, card);
	damage_player(t_player, 3, player, card);
}

int card_assembled_alphas(int player, int card, event_t event)
{
/* Assembled Alphas	|5|R	0x200efa8
 * Creature - Wolf 5/5
 * Whenever ~ blocks or becomes blocked by a creature, ~ deals 3 damage to that creature and 3 damage to that creature's controller. */

	if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card)){
		if (player == current_turn){
			for_each_creature_blocking_me(player, card, damage_blocked_blocking_creature_and_controller, player, card);
		}

		if (player == 1-current_turn){
			for_each_creature_blocked_by_me(player, card, damage_blocked_blocking_creature_and_controller, player, card);
		}
	}
	return 0;
}

int card_bedlam_reveler(int player, int card, event_t event)
{
  /* Bedlam Reveler	|6|R|R	0x200ed5a
   * Creature - Devil Horror 3/4
   * ~ costs |1 less to cast for each instant and sorcery card in your graveyard.
   * Prowess
   * When ~ enters the battlefield, discard your hand, then draw three cards. */

	if (event == EVENT_MODIFY_COST)
		COST_COLORLESS -= count_graveyard_by_type(player, TYPE_SPELL);

	prowess(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		discard_all(player);
		draw_cards(player, 3);
	}

	return 0;
}

int card_blood_mist(int player, int card, event_t event)
{
  /* Blood Mist	|3|R	0x200ed5f
   * Enchantment
   * At the beginning of combat on your turn, target creature you control gains double strike until end of turn. */

  if (event == EVENT_MUST_ATTACK && current_turn == player && !is_humiliated(player, card))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = td.preferred_controller = player;

	  card_instance_t* inst = get_card_instance(player, card);
	  inst->number_of_targets = 0;
	  if (can_target(&td) && pick_target(&td, "ASHNODS_BATTLEGEAR"))	// "Select target creature you control"
		pump_ability_until_eot(player, card, inst->targets[0].player, inst->targets[0].card,
							   0,0, KEYWORD_DOUBLE_STRIKE,0);
	}

  return global_enchantment(player, card, event);
}

/* Bold Impaler	|R	=>battle_for_zendikar.c:card_lavastep_raider	0x200e08f
 * Creature - Vampire Knight 1/2
 * |2|R: ~ gets +2/+0 until end of turn. */

int card_borrowed_hostility(int player, int card, event_t event){
/* Borrowed Hostility	|R	0x200efad
 * Instant
 * Escalate |3
 * Choose one or both -
 * * Target creature gets +3/+0 until end of turn.
 * * Target creature gains first strike until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum
	{
		CHOICE_PUMP 			= 1<<0,
		CHOICE_FIRST_STRIKE 	= 1<<1,
		CHOICE_MASK = CHOICE_PUMP | CHOICE_FIRST_STRIKE,
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return can_target(&td) ? 1 : 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			while( (instance->info_slot & CHOICE_MASK) != CHOICE_MASK ){
					int priority_fs = current_turn == 1-player ? 15 : 5;
					int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
										"Target gets +3/+0 until EOT.", !(instance->info_slot & CHOICE_PUMP), 10,
										"Target gains First Strike until EOT.", !(instance->info_slot & CHOICE_FIRST_STRIKE), priority_fs);

					if( ! choice ){
						spell_fizzled = 1;
						return 0;
					}
					instance->info_slot |= choice;
					if( has_mana(player, COLOR_ANY, 3) ){
						int choice2 = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_NO_CANCEL,
											"Escalate", 1, 10,
											"Decline", 1, 1);
						if( choice2 == 1 ){
							charge_mana(player, COLOR_ANY, 3);
							if( spell_fizzled == 1 ){
								spell_fizzled = 0;
								break;
							}
						}
						else{
							break;
						}
					}
					else{
						break;
					}
			}
		}
		int base_target = 0;
		if( instance->info_slot & CHOICE_PUMP ){
			if( new_pick_target(&td, "Select target creature (+3/+0)", base_target, 1 | GS_LITERAL_PROMPT) ){
				base_target++;
			}
			else{
				return 0;
			}
		}
		if( instance->info_slot & CHOICE_FIRST_STRIKE ){
			if( ! new_pick_target(&td, "Select target creature (First Strike)", base_target, 1 | GS_LITERAL_PROMPT) ){
				return 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int base_target = 0;
		if( instance->info_slot & CHOICE_PUMP ){
			if( validate_target(player, card, &td, base_target) ){
				pump_until_eot(player, card, instance->targets[base_target].player, instance->targets[base_target].card, 3, 0);
			}
			base_target++;
		}
		if( instance->info_slot & CHOICE_FIRST_STRIKE ){
			if( validate_target(player, card, &td, base_target) ){
				pump_ability_until_eot(player, card, instance->targets[base_target].player, instance->targets[base_target].card,
										0, 0, KEYWORD_FIRST_STRIKE, 0);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

/* Brazen Wolves	|2|R	=>portal_1_2_3k.c:card_lurking_nightstalker	0x200968e
 * Creature - Wolf 2/3
 * Whenever ~ attacks, it gets +2/+0 until end of turn. */

int card_collective_defiance(int player, int card, event_t event){
	/* Collective Defiance	|1|R|R	0x200efb2
	 * Sorcery
	 * Escalate |1
	 * Choose one or more -
	 * * Target player discards all the cards in their hand, then draws that many cards.
	 * * ~ deals 4 damage to target creature.
	 * * ~ deals 3 damage to target opponent or planeswalker. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum
	{
		CHOICE_DISCARD_DRAW 		= 1<<0,
		CHOICE_DAMAGE_CREATURE		= 1<<1,
		CHOICE_DAMAGE_OPP			= 1<<2,
		CHOICE_MASK = CHOICE_DISCARD_DRAW | CHOICE_DAMAGE_CREATURE | CHOICE_DAMAGE_OPP,
	};

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE );

	target_definition_t td3;
	opponent_or_pw_target_definition(player, card, &td3);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return can_target(&td) | can_target(&td2) | would_validate_arbitrary_target(&td3, 1-player, -1) ? 1 : 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			while( (instance->info_slot & CHOICE_MASK) != CHOICE_MASK ){
					int abils[3]={	!(instance->info_slot & CHOICE_DISCARD_DRAW) && can_target(&td),
									!(instance->info_slot & CHOICE_DAMAGE_CREATURE) && can_target(&td2),
									!(instance->info_slot & CHOICE_DAMAGE_OPP) && can_target(&td3)
					};
					int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
										"Target player discard & draw", abils[0], 5,
										"4 damage to target creature", abils[1], 15,
										"3 damage to target opponent or planeswalker", abils[2], life[1-player] <= 3 ? 20 : 10);

					if( ! choice ){
						spell_fizzled = 1;
						return 0;
					}
					instance->info_slot |= choice;
					if( has_mana(player, COLOR_ANY, 1) ){
						int choice2 = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_NO_CANCEL,
											"Escalate", 1, 10,
											"Decline", 1, 1);
						if( choice2 == 1 ){
							charge_mana(player, COLOR_ANY, 1);
							if( spell_fizzled == 1 ){
								spell_fizzled = 0;
								break;
							}
						}
						else{
							break;
						}
					}
					else{
						break;
					}
			}
		}
		int base_target = 0;
		if( instance->info_slot & CHOICE_DISCARD_DRAW ){
			if( new_pick_target(&td, "TARGET_PLAYER", base_target, 1) ){
				base_target++;
			}
			else{
				return 0;
			}
		}
		if( instance->info_slot & CHOICE_DAMAGE_CREATURE ){
			if( new_pick_target(&td2, "TARGET_CREATURE", base_target, 1) ){
				base_target++;
			}
			else{
				return 0;
			}
		}
		if( instance->info_slot & CHOICE_DAMAGE_OPP ){
			if( new_pick_target(&td2, "TARGET_OPPONENT_OR_PLANESWALKER", base_target, 1) ){
				base_target++;
			}
			else{
				return 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int base_target = 0;
		if( instance->info_slot & CHOICE_DISCARD_DRAW ){
			if( validate_target(player, card, &td, base_target) ){
				int amount = hand_count[instance->targets[base_target].player];
				new_discard_all(instance->targets[base_target].player, player);
				draw_cards(instance->targets[base_target].player, amount);
			}
			base_target++;
		}
		if( instance->info_slot & CHOICE_DAMAGE_CREATURE ){
			if( validate_target(player, card, &td2, base_target) ){
				damage_creature(instance->targets[base_target].player, instance->targets[base_target].card, 4, player, card);
			}
			base_target++;
		}
		if( instance->info_slot & CHOICE_DAMAGE_OPP ){
			if( validate_target(player, card, &td3, base_target) ){
				damage_creature(instance->targets[base_target].player, instance->targets[base_target].card, 3, player, card);
			}
			base_target++;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int fx_produce_mana_your_next_main_phase_this_turn(int player, int card, event_t event)
{
  /* player: who gets mana
   * targets[1].player: amt of colorless
   * targets[1].card: amt of black
   * targets[2].player: amt of blue
   * targets[2].card: amt of green
   * targets[3].player: amt of red
   * targets[3].card: amt of white */

  if (event == EVENT_PHASE_CHANGED
	  && current_turn == player
	  && (current_phase == PHASE_MAIN1 || current_phase == PHASE_MAIN2))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  produce_mana_multi(player,
						 inst->targets[1].player, inst->targets[1].card,
						 inst->targets[2].player, inst->targets[2].card,
						 inst->targets[3].player, inst->targets[3].card);
	  kill_card(player, card, KILL_EXILE);
	}

  if (event == EVENT_CLEANUP)
	kill_card(player, card, KILL_EXILE);

  return 0;
}
void produce_mana_your_next_main_phase_this_turn(int player, int card, int x, int b, int u, int g, int r, int w)
{
  if (current_turn != player)
	return;
  int c = create_legacy_effect(player, card, &fx_produce_mana_your_next_main_phase_this_turn);
  if (c == -1)
	return;
  card_instance_t* fx = get_card_instance(player, c);
  fx->targets[1].player = x;
  fx->targets[1].card = b;
  fx->targets[2].player = u;
  fx->targets[2].card = g;
  fx->targets[3].player = r;
  fx->targets[3].card = w;
}

int card_conduit_of_storms(int player, int card, event_t event)
{
  /* Conduit of Storms	|2|R	0x200ed64
   * Creature - Werewolf Horror 2/3
   * Whenever ~ attacks, add |R to your mana pool at the beginning of your next main phase this turn.
   * |3|R|R: Transform ~. */

  double_faced_card(player, card, event);

  if (declare_attackers_trigger(player, card, event, 0, player, card))
	produce_mana_your_next_main_phase_this_turn(player, card, MANACOST_R(1));

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (in_play(inst->parent_controller, inst->parent_card)
		  && !check_special_flags3(inst->parent_controller, inst->parent_card, SF3_CARD_IS_FLIPPED))
		transform(inst->parent_controller, inst->parent_card);
	}

  return generic_activated_ability(player, card, event, 0, MANACOST_XR(3,2), 0, NULL, NULL);
}
// --TRANSFORM--
int card_conduit_of_emrakul(int player, int card, event_t event)
{
  /* Conduit of Emrakul	""	0x200ed69
   * Creature - Eldrazi Werewolf 5/4
   * Whenever ~ attacks, add |C|C to your mana pool at the beginning of your next main phase this turn. */

  if (declare_attackers_trigger(player, card, event, 0, player, card))
	produce_mana_your_next_main_phase_this_turn(player, card, MANACOST_X(2));

  return 0;
}

/* Deranged Whelp	|1|R	= gatecrash.c:card_ripscale_predators
 * Creature - Wolf 2/1
 * Menace */

int card_distemper_of_the_blood(int player, int card, event_t event)
{
  /* Distemper of the Blood	|1|R	0x200ed3c
   * Sorcery
   * Target creature gets +2/+2 and gains trample until end of turn.
   * Madness |R */

  // Not an instant, but this is a less cumbersome API than vanilla_pump.  It won't ever see EVENT_CHECK_PUMP events.
  if (IS_GS_EVENT(player, card, event))
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 2,2, KEYWORD_TRAMPLE,0);

  return madness(player, card, event, MANACOST_R(1));
}

/* Falkenrath Reaver	|1|R	=>vanilla	0x401000
 * Creature - Vampire 2/2 */

int card_furyblade_vampire(int player, int card, event_t event)
{
  /* Furyblade Vampire	|1|R	0x200ed6e
   * Creature - Vampire Berserker 1/2
   * Trample
   * At the beginning of combat on your turn, you may discard a card. If you do, ~ gets +3/+0 until end of turn. */

  if (event == EVENT_MUST_ATTACK && current_turn == player && hand_count[player] >= 1 && !is_humiliated(player, card))
	{
	  test_definition_t this_test;
	  default_test_definition(&this_test, 0);
	  int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
	  if (selected != -1)
		{
		  discard_card(player, selected);
		  pump_until_eot_merge_previous(player, card, player, card, 3,0);
		}
	}

  return 0;
}

int card_galvanic_bombardment(int player, int card, event_t event)
{
  /* Galvanic Bombardment	|R	0x200ed73
   * Instant
   * ~ deals X damage to target creature, where X is 2 plus the number of cards named ~ in your graveyard. */

  if (event == EVENT_CHECK_PUMP && can_play_iid(player, event, get_card_instance(player, card)->internal_card_id))
	pumpable_toughness[1-player] -= 2 + count_graveyard_by_id(player, get_id(player, card));

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		damage_target0(player, card, 2 + count_graveyard_by_id(player, get_id(player, card)));

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_harmless_offering(int player, int card, event_t event)
{
  /* Harmless Offering	|2|R	0x200ed78
   * Sorcery
   * Target opponent gains control of target permanent you control. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_PERMANENT);
  td.allowed_controller = td.preferred_controller = player;

  if (event == EVENT_CAN_CAST)
	return opponent_is_valid_target(player, card) && can_target(&td);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  inst->number_of_targets = 0;
	  if (target_opponent(player, card))
		new_pick_target(&td, "TARGET_PERMANENT_YOU_CONTROL", 1, 1);
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (opponent_is_valid_target(player, card) && validate_target(player, card, &td, 1))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  give_control(player, card, inst->targets[1].player, inst->targets[1].card);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_impetuous_devils(int player, int card, event_t event)
{
/* Impetuous Devils	|2|R|R	0x200efb7
 * Creature - Devil 6/1
 * Trample, haste
 * When ~ attacks, up to one target creature defending player controls blocks it this combat if able.
 * At the beginning of the end step, sacrifice ~. */

	store_attackers(player, card, event, 0, player, card, NULL);

	if (xtrigger_condition() == XTRIGGER_ATTACKING && affect_me(player, card) && reason_for_trigger_controller == player){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;

		card_instance_t* instance = get_card_instance(player, card);

		if(can_target(&td) && is_attacking(player, card) ){
			if( resolve_declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
				instance->number_of_targets = 0;
				if( pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS") ){
					pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
											0, 0, 0, SP_KEYWORD_MUST_BLOCK);
				}
			}
		}
		instance->info_slot = 0;
	}

	return card_ball_lightning(player, card, event);
}

/* Incendiary Flow	|1|R	=>champions_of_kamigawa.c:card_yamabushis_flame	0x2006173
 * Sorcery
 * ~ deals 3 damage to any target. If a creature dealt damage this way would die this turn, exile it instead. */

int card_insatiable_gorgers(int player, int card, event_t event)
{
  /* Insatiable Gorgers	|2|R|R	0x200ed7d
   * Creature - Vampire Berserker 5/3
   * ~ attacks each combat if able.
   * Madness |3|R */

  if (event == EVENT_MUST_ATTACK && current_turn == player && !is_humiliated(player, card))
	attack_if_able(player, card, event);

  return madness(player, card, event, MANACOST_XR(3,1));
}

int card_make_mischief(int player, int card, event_t event)
{
  /* Make Mischief	|2|R	0x200ed82
   * Sorcery
   * ~ deals 1 damage to any target. Create a 1/1 |Sred Devil creature token. It has "When this creature dies, it deals 1 damage to any target." */
  if (event == EVENT_CHECK_PUMP && can_play_iid(player, event, get_card_instance(player, card)->internal_card_id))
	pumpable_toughness[1-player] -= 1;

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  damage_target0(player, card, 1);
		  generate_token_by_id(player, card, CARD_ID_DEVIL);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

/* Mirrorwing Dragon	|3|R|R	0x000000
 * Creature - Dragon 4/5
 * Flying
 * Whenever a player casts an instant or sorcery spell that targets only ~, that player copies that spell for each other creature he or she controls that the spell could target. Each copy targets a different one of those creatures. */

int card_nahiris_wrath(int player, int card, event_t event){
/* Nahiri's Wrath	|2|R	0x200efbc
 * Sorcery
 * As an additional cost to cast ~, discard X cards.
 * ~ deals damage equal to the total converted mana cost of the discarded cards to each of up to X target creatures and/or planeswalkers. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( hand_count[player] > 1 ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
		}
		int i = 0;
		int max_targets = ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ? hand_count[player] : instance->info_slot;
		while( can_target(&td) && i < max_targets ){
				if( new_pick_target(&td, "Select target creature or planeswalker.", i, GS_LITERAL_PROMPT) ){
					state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
					i++;
				}
				else{
					break;
				}
		}
		int k;
		for(k=0; k<i; k++){
			state_untargettable(instance->targets[k].player, instance->targets[k].card, 1);
		}
		new_multidiscard(player, i, 0, player);
		instance->info_slot = 1;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int k;
		for(k=0; k<instance->info_slot; k++){
			damage_creature(instance->targets[k].player, instance->targets[k].card, instance->info_slot, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int fx_otherworldly_outburst(int player, int card, event_t event)
{
  card_instance_t* inst = get_card_instance(player, card);

  if (event == EVENT_POWER && affect_me(inst->damage_target_player, inst->damage_target_card))
	event_result += 1;

  if (graveyard_from_play(inst->damage_target_player, inst->damage_target_card, event))
	{
	  generate_token_by_id(player, card, CARD_ID_ELDRAZI_HORROR);
	  kill_card(player, card, KILL_EXILE);
	}

  if (event == EVENT_CLEANUP)
	kill_card(player, card, KILL_EXILE);

  return 0;
}
int card_otherworldly_outburst(int player, int card, event_t event)
{
  /* Otherworldly Outburst	|R	0x200ed87
   * Instant
   * Target creature gets +1/+0 until end of turn. When that creature dies this turn, put a 3/2 colorless Eldrazi Horror creature token onto the battlefield. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.preferred_controller = player;

	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  create_targetted_legacy_effect(player, card, &fx_otherworldly_outburst,
										 inst->targets[0].player, inst->targets[0].card);
		}
	  kill_card(player, card, KILL_DESTROY);
	  return 0;
	}

  return vanilla_instant_pump(player, card, event, ANYBODY, player, 1,0, 0,0);
}

/* Prophetic Ravings	|R	0x000000
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature has haste and "|T, Discard a card: Draw a card." */

int card_savage_alliance(int player, int card, event_t event){
/* Savage Alliance	|2|R	0x200efc1
 * Instant
 * Escalate |1
 * Choose one or more -
 * * Creatures target player controls gain trample until end of turn.
 * * ~ deals 2 damage to target creature.
 * * ~ deals 1 damage to each creature target opponent controls. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum
	{
		CHOICE_TRAMPLE 			= 1<<0,
		CHOICE_DAMAGE_CREATURE	= 1<<1,
		CHOICE_DAMAGE_ALL_CRIT	= 1<<2,
		CHOICE_MASK = CHOICE_TRAMPLE | CHOICE_DAMAGE_CREATURE | CHOICE_DAMAGE_ALL_CRIT,
	};

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE );

	target_definition_t td3;
	default_target_definition(player, card, &td3, 0 );
	td3.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return can_target(&td) | can_target(&td2) | would_validate_arbitrary_target(&td3, 1-player, -1) ? 1 : 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			while( (instance->info_slot & CHOICE_MASK) != CHOICE_MASK ){
					int abils[3]={	!(instance->info_slot & CHOICE_TRAMPLE) && can_target(&td),
									!(instance->info_slot & CHOICE_DAMAGE_CREATURE) && can_target(&td2),
									!(instance->info_slot & CHOICE_DAMAGE_ALL_CRIT) && would_validate_arbitrary_target(&td3, 1-player, -1)
					};
					int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
										"Creatures gains trample", abils[0], 5,
										"2 damage to target creature", abils[1], 10,
										"1 damage to all opponent's crits", abils[2], 15);

					if( ! choice ){
						spell_fizzled = 1;
						return 0;
					}
					instance->info_slot |= choice;
					if( has_mana(player, COLOR_ANY, 1) ){
						int choice2 = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_NO_CANCEL,
											"Escalate", 1, 10,
											"Decline", 1, 1);
						if( choice2 == 1 ){
							charge_mana(player, COLOR_ANY, 1);
							if( spell_fizzled == 1 ){
								spell_fizzled = 0;
								break;
							}
						}
						else{
							break;
						}
					}
					else{
						break;
					}
			}
		}
		int base_target = 0;
		if( instance->info_slot & CHOICE_TRAMPLE ){
			if( new_pick_target(&td, "TARGET_PLAYER", base_target, 1) ){
				base_target++;
			}
			else{
				return 0;
			}
		}
		if( instance->info_slot & CHOICE_DAMAGE_CREATURE ){
			if( new_pick_target(&td2, "TARGET_CREATURE", base_target, 1) ){
				base_target++;
			}
			else{
				return 0;
			}
		}
		if( instance->info_slot & CHOICE_DAMAGE_ALL_CRIT ){
			instance->targets[base_target].player = 1-player;
			instance->targets[base_target].card = -1;
			instance->number_of_targets++;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int base_target = 0;
		if( instance->info_slot & CHOICE_TRAMPLE ){
			if( validate_target(player, card, &td, base_target) ){
				pump_subtype_until_eot(player, card, instance->targets[base_target].player, -1, 0, 0, KEYWORD_TRAMPLE, 0);
			}
			base_target++;
		}
		if( instance->info_slot & CHOICE_DAMAGE_CREATURE ){
			if( validate_target(player, card, &td2, base_target) ){
				damage_creature(instance->targets[base_target].player, instance->targets[base_target].card, 2, player, card);
			}
			base_target++;
		}
		if( instance->info_slot & CHOICE_DAMAGE_ALL_CRIT ){
			if( validate_target(player, card, &td3, base_target) ){
				new_damage_all(player, card, instance->targets[base_target].player, 1, NDA_ALL_CREATURES, NULL);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_shreds_of_sanity(int player, int card, event_t event)
{
/* Shreds of Sanity	|2|R	0x200efc6
 * Sorcery
 * Return up to one target instant card and up to one target sorcery card from your graveyard to your hand, then discard a card. Exile ~. */


	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! graveyard_has_shroud(player) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_INSTANT, "Select target instant card.");
			test.id = cards_data[iid_draw_a_card].id;
			test.id_flag = DOESNT_MATCH;
			int base_target = 0;
			int iid = -1;
			int selected = new_select_target_from_grave(player, card, player, 0, AI_MAX_VALUE, &test, base_target);
			if( selected != -1 ){
				base_target++;
				iid = turn_card_in_grave_face_down(player, selected);
			}

			test_definition_t test2;
			new_default_test_definition(&test2, TYPE_SORCERY, "Select target sorcery card.");
			test2.id = cards_data[iid_draw_a_card].id;
			test2.id_flag = DOESNT_MATCH;
			new_select_target_from_grave(player, card, player, 0, AI_MAX_VALUE, &test2, base_target);

			if( iid != -1 ){
				turn_card_in_grave_face_up(player, selected, iid);
			}

			get_card_instance(player, card)->info_slot = base_target;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			int selected = validate_target_from_grave(player, card, player, i);
			if( selected != -1 ){
				from_grave_to_hand(player, selected, TUTOR_HAND);
			}
		}
		discard(player, 0, player);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_smoldering_werewolf(int player, int card, event_t event)
{
  /* Smoldering Werewolf	|2|R|R	0x200ed8c
   * Creature - Werewolf Horror 3/2
   * When ~ enters the battlefield, it deals 1 damage to each of up to two target creatures.
   * |4|R|R: Transform ~. */

  double_faced_card(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allow_cancel = 3;

	  card_instance_t* inst = get_card_instance(player, card);
	  inst->number_of_targets = 0;
	  pick_up_to_n_targets(&td, "TARGET_CREATURE", 2);
	  if (cancel == 1)
		inst->number_of_targets = 0;
	  else
		{
		  int i;
		  for (i = 0; i < inst->number_of_targets; ++i)
			damage_creature(inst->targets[i].player, inst->targets[i].card, 1, player, card);
		}
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (in_play(inst->parent_controller, inst->parent_card)
		  && !check_special_flags3(inst->parent_controller, inst->parent_card, SF3_CARD_IS_FLIPPED))
		transform(inst->parent_controller, inst->parent_card);
	}

  return generic_activated_ability(player, card, event, 0, MANACOST_XR(4,2), 0, NULL, NULL);
}
// --TRANSFORM--
int card_erupting_dreadwolf(int player, int card, event_t event)
{
  /* Erupting Dreadwolf	""	0x200ed91
   * Creature - Eldrazi Werewolf 6/4
   * Whenever ~ attacks, it deals 2 damage to any target. */
  if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	  td.allow_cancel = 0;

	  card_instance_t* inst = get_card_instance(player, card);
	  inst->number_of_targets = 0;
	  if (can_target(&td) && pick_target(&td, "TARGET_ANY"))
		damage_target0(player, card, 2);
	}

  return 0;
}

int card_spreading_flames(int player, int card, event_t event)
{
  /* Spreading Flames	|6|R	0x200ed96
   * Instant
   * ~ deals 6 damage divided as you choose among any number of target creatures. */

  if (!IS_CASTING(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  if (event == EVENT_CAN_CAST)
	return can_target(&td);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  card_instance_t* inst = get_card_instance(player, card);

	  int i;
	  for (i = 0; i < 6; ++i)
		if (new_pick_target(&td, "TARGET_CREATURE", -1, 1))
		  add_state(inst->targets[i].player, inst->targets[i].card, STATE_TARGETTED);
		else
		  break;

	  for (i = 0; i < inst->number_of_targets; ++i)
		remove_state(inst->targets[i].player, inst->targets[i].card, STATE_TARGETTED);

	  if (cancel == 1)
		inst->number_of_targets = 0;
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  divide_damage(player, card, &td);
	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_stensia_banquet(int player, int card, event_t event)
{
  /* Stensia Banquet	|2|R	0x200ed9b
   * Sorcery
   * ~ deals damage to target opponent or planeswalker equal to the number of Vampires you control.
   * Draw a card. */
  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  opponent_or_pw_target_definition(player, card, &td);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  int dmg = count_subtype(player, TYPE_PERMANENT, SUBTYPE_VAMPIRE);
		  if (dmg > 0)
			damage_target0(player, card, dmg);

		  draw_a_card(player);
		}

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_OPPONENT_OR_PLANESWALKER", 1, NULL);
}

int card_stensia_innkeeper(int player, int card, event_t event)
{
  /* Stensia Innkeeper	|3|R	0x200eda0
   * Creature - Vampire 3/3
   * When ~ enters the battlefield, tap target land an opponent controls. That land doesn't untap during its controller's next untap step. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_LAND);
	  td.allowed_controller = 1-player;
	  td.allow_cancel = 0;

	  card_instance_t* inst = get_card_instance(player, card);
	  inst->number_of_targets = 0;
	  if (can_target(&td) && pick_next_target_noload(&td, "Select target land an opponent controls."))
		does_not_untap_effect(player, card, inst->targets[0].player, inst->targets[0].card, EDNT_TAP_TARGET, 1);
	}

  return 0;
}

int card_stromkirk_occultist(int player, int card, event_t event)
{
/* Stromkirk Occultist	|2|R	0x200efcb
 * Creature - Vampire Horror 3/2
 * Trample
 * Whenever ~ deals combat damage to a player, exile the top card of your library. Until end of turn, you may play that card.
 * Madness |1|R */

	if ( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER) )
	{
		if( deck_ptr[player][0] != -1 ){
			int csvid = cards_data[deck_ptr[player][0]].id;
			rfg_top_card_of_deck(player);
			create_may_play_card_from_exile_effect(player, card, player, csvid, MPCFE_UNTIL_EOT);
		}
	}

	return madness(player, card, event, MANACOST_XR(1, 1));
}

int card_thermo_alchemist(int player, int card, event_t event)
{
  /* Thermo-Alchemist	|1|R	0x200eda5
   * Creature - Human Shaman 0/3
   * Defender
   * |T: ~ deals 1 damage to each opponent.
   * Whenever you cast an instant or sorcery spell, untap ~. */

  if (specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY,
							TYPE_SPELL,MATCH, 0,0, 0,0, 0,0, -1,0))
	untap_card(player, card);

  if (event == EVENT_RESOLVE_ACTIVATION)
	damage_creature(1-player, -1, 1, player, card);

  return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_vildin_pack_outcast(int player, int card, event_t event)
{
  /* Vildin-Pack Outcast	|4|R	0x200edaa
   * Creature - Werewolf Horror 4/4
   * Trample
   * |R: ~ gets +1/-1 until end of turn.
   * |5|R|R: Transform ~. */

  double_faced_card(player, card, event);

  if (event == EVENT_POW_BOOST)
	return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1,-1);

  if (!IS_ACTIVATING(event))
	return 0;

  enum
  {
	CHOICE_PUMP = 1,
	CHOICE_TRANSFORM
  } choice = DIALOG(player, card, event,
					DLG_RANDOM,
					"+1/-1",	1,	1,	DLG_MANA(MANACOST_R(1)),
					"Transform",1,	4,	DLG_MANA(MANACOST_XR(5,2)));

  if (event == EVENT_CAN_ACTIVATE)
	return choice && can_use_activated_abilities(player, card);
  else if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (in_play(inst->parent_controller, inst->parent_card))
		switch (choice)
		  {
			case CHOICE_PUMP:
			  pump_until_eot_merge_previous(inst->parent_controller, inst->parent_card, inst->parent_controller, inst->parent_card, 1,-1);
			  break;

			case CHOICE_TRANSFORM:
			  if (!check_special_flags3(inst->parent_controller, inst->parent_card, SF3_CARD_IS_FLIPPED))
				transform(inst->parent_controller, inst->parent_card);
			  break;
		  }
	}

  return 0;
}
// --TRANSFORM--
/* Dronepack Kindred	""	=>legends.c:card_wall_of_opposition	0x200c519
 * Creature - Eldrazi Werewolf 5/7
 * Trample
 * |1: ~ gets +1/+0 until end of turn. */

int card_weaver_of_lightning(int player, int card, event_t event)
{
  /* Weaver of Lightning	|2|R	0x200edaf
   * Creature - Human Shaman 1/4
   * Reach
   * Whenever you cast an instant or sorcery spell, ~ deals 1 damage to target creature an opponent controls. */

  if (specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY,
							TYPE_SPELL,MATCH, 0,0, 0,0, 0,0, -1,0))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = 1-player;
	  td.allow_cancel = 0;

	  get_card_instance(player, card)->number_of_targets = 0;
	  if (can_target(&td) && pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS"))
		damage_target0(player, card, 1);
	}

  return 0;
}

/*** Green ***/

int card_backwoods_survivalists(int player, int card, event_t event)
{
  /* Backwoods Survivalists	|3|G	0x200edb4
   * Creature - Human Warrior 4/3
   * Delirium - ~ gets +1/+1 and has trample as long as there are four or more card types among cards in your graveyard. */

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card)
	  && delirium(player) && !is_humiliated(player, card))
	event_result += 1;

  if (event == EVENT_ABILITIES && affect_me(player, card) && delirium(player) && !is_humiliated(player, card))
	event_result |= KEYWORD_TRAMPLE;

  return 0;
}

int card_bloodbriar(int player, int card, event_t event)
{
  /* Bloodbriar	|2|G	0x200edb9
   * Creature - Plant Elemental 2/3
   * Whenever you sacrifice another permanent, put a +1/+1 counter on ~. */

  if (event == EVENT_GRAVEYARD_FROM_PLAY
	  && !affect_me(player, card)
	  && get_card_instance(affected_card_controller, affected_card)->kill_code == KILL_SACRIFICE)
	count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);

  int n = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (n > 0)
	add_1_1_counters(player, card, n);

  return 0;
}

int card_clear_shot(int player, int card, event_t event)
{
  /* Clear Shot	|2|G	0x200edbe
   * Instant
   * Target creature you control gets +1/+1 until end of turn. It deals damage equal to its power to target creature you don't control. */

  if (event == EVENT_CHECK_PUMP && can_play_iid(player, event, get_card_instance(player, card)->internal_card_id))
	{
	  pumpable_power[player] += 1;
	  pumpable_toughness[player] += 1;
	}

  if (!IS_CASTING(player, card, event))
	return 0;

  target_definition_t td_control;
  default_target_definition(player, card, &td_control, TYPE_CREATURE);
  td_control.allowed_controller = td_control.preferred_controller = player;

  target_definition_t td_dont;
  default_target_definition(player, card, &td_dont, TYPE_CREATURE);
  td_dont.allowed_controller = td_dont.preferred_controller = 1-player;

  if (event == EVENT_CAN_CAST)
	return can_target(&td_control) && can_target(&td_dont);

  card_instance_t* inst = get_card_instance(player, card);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  inst->number_of_targets = 0;
	  if (new_pick_target(&td_control, "ASHNODS_BATTLEGEAR", 0, 1))
		new_pick_target(&td_dont, "Select target creature you don't control.", 1, 1 | GS_LITERAL_PROMPT);
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (validate_target(player, card, &td_control, 0))
		{
		  pump_until_eot(player, card, inst->targets[0].player, inst->targets[0].card, 1,1);
		  if (validate_target(player, card, &td_dont, 1))
			damage_creature(inst->targets[1].player, inst->targets[1].card,
							get_power(inst->targets[0].player, inst->targets[0].card),
							inst->targets[0].player, inst->targets[0].card);
		}

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_crop_sigil(int player, int card, event_t event)
{
/* Crop Sigil	|G	0x200efd0
 * Enchantment
 * At the beginning of your upkeep, you may put the top card of your library into your graveyard.
 * Delirium - |2|G, Sacrifice ~: Return up to one target creature card and up to one target land card from your graveyard to your hand.
 * 		Activate this ability only if there are four or more card types among cards in your graveyard. */

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		mill(player, 1);
	}

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_CAN_ACTIVATE && delirium(player) ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XG(2, 1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_XG(2, 1)) ){
			if( ! graveyard_has_shroud(player) ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_INSTANT, "Select target creature card.");
				test.id = cards_data[iid_draw_a_card].id;
				test.id_flag = DOESNT_MATCH;
				int base_target = 0;
				int iid = -1;
				int selected = new_select_target_from_grave(player, card, player, 0, AI_MAX_VALUE, &test, base_target);
				if( selected != -1 ){
					base_target++;
					iid = turn_card_in_grave_face_down(player, selected);
				}

				test_definition_t test2;
				new_default_test_definition(&test2, TYPE_SORCERY, "Select target land card.");
				test2.id = cards_data[iid_draw_a_card].id;
				test2.id_flag = DOESNT_MATCH;
				new_select_target_from_grave(player, card, player, 0, AI_MAX_VALUE, &test2, base_target);

				if( iid != -1 ){
					turn_card_in_grave_face_up(player, selected, iid);
				}

				get_card_instance(player, card)->info_slot = base_target;
			}
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int i;
		for(i=0; i<2; i++){
			int selected = validate_target_from_grave(player, card, player, i);
			if( selected != -1 ){
				from_grave_to_hand(player, selected, TUTOR_HAND);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_crossroads_consecrator(int player, int card, event_t event)
{
  /* Crossroads Consecrator	|G	0x200edc3
   * Creature - Human Cleric 1/2
   * |G, |T: Target attacking Human gets +1/+1 until end of turn. */

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.required_subtype = SUBTYPE_HUMAN;
  td.required_state = TARGET_STATE_ATTACKING;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  pump_until_eot_merge_previous(player, card, inst->targets[0].player, inst->targets[0].card, 1,1);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT,
								   MANACOST_G(1), 0, &td, "Select target attacking Human.");
}

int card_eldritch_evolution(int player, int card, event_t event){
/* Eldritch Evolution	|1|G|G	0x200efd5
 * Sorcery
 * As an additional cost to cast ~, sacrifice a creature.
 * Search your library for a creature card with converted mana cost X or less, where X is 2 plus the sacrificed creature's converted mana
 * cost. Put that card onto the battlefield, then shuffle your library. Exile ~. */

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_SAC_CREATURE_AS_COST, NULL, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			if (!sac){
				cancel = 1;
				return 0;
			}
			get_card_instance(player, card)->info_slot = get_cmc(BYTE2(sac), BYTE3(sac))+2;
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
	}


	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.cmc = get_card_instance(player, card)->info_slot+1;
		test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		scnprintf(test.message, 100, "Select a creature card with CMC %d or less.", get_card_instance(player, card)->info_slot);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &test);

		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_emrakuls_evangel(int player, int card, event_t event){
/* Emrakul's Evangel	|2|G	0x200efda
 * Creature - Human Horror 3/2
 * |T, Sacrifice ~ and any number of other non-Eldrazi creatures: Put a 3/2 colorless Eldrazi Horror creature token onto the battlefield for each creature sacrificed this way. */

	if(event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE){
		get_card_instance(player, card)->info_slot = 0;
		state_untargettable(player, card, 1);

		int max_sacrifices = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && c != card && is_what(player, c, TYPE_CREATURE) ){
				if( can_sacrifice_this_as_cost(player, c) ){
					max_sacrifices++;
				}
			}
		}

		int sacced[max_sacrifices];
		int sac_count = 0;

		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

		while( sac_count < max_sacrifices ){
				int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
				if (!sac){
					break;
				}
				sacced[sac_count] = BYTE3(sac);
				sac_count++;
		}
		for(c=0; c<sac_count; c++){
			state_untargettable(player, sacced[c], 0);
			kill_card(player, sacced[c], KILL_SACRIFICE);
		}
		state_untargettable(player, card, 0);
		add_state(player, card, STATE_TAPPED);
		get_card_instance(player, card)->info_slot = sac_count+1;
		kill_card(player, card, KILL_SACRIFICE);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_tokens_by_id(player, card, CARD_ID_ELDRAZI_HORROR, get_card_instance(player, card)->info_slot);
	}

	return 0;
}

int card_emrakuls_influence(int player, int card, event_t event)
{
  /* Emrakul's Influence	|2|G|G	0x200edc8
   * Enchantment
   * Whenever you cast an Eldrazi creature spell with converted mana cost 7 or greater, draw two cards. */

  if (specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE,MATCH,
							SUBTYPE_ELDRAZI,MATCH, 0,0, 0,0, 6,F5_CMC_GREATER_THAN_VALUE))
	draw_cards(player, 2);

  return global_enchantment(player, card, event);
}

int card_foul_emissary(int player, int card, event_t event)
{
/* Foul Emissary	|2|G	0x200efdf
 * Creature - Human Horror 1/1
 * When ~ enters the battlefield, look at the top four cards of your library.
 * You may reveal a creature card from among them and put it into your hand. Put the rest on the bottom of your library in any order.
 * When you sacrifice ~ while casting a spell with emerge, put a 3/2 colorless Eldrazi Horror creature token onto the battlefield. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int amount = MIN(count_deck(player), 4);
		if( amount ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
			test.create_minideck = amount;
			test.no_shuffle = 1;

			int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
			if( result > -1 ){
				reveal_card(player, card, player, result);
				amount--;
			}
			if( amount ){
				put_top_x_on_bottom(player, player, amount);
			}
		}
	}

	return 0;
}

int card_gnarlwood_dryad(int player, int card, event_t event)
{
  /* Gnarlwood Dryad	|G	0x200edcd
   * Creature - Dryad Horror 1/1
   * Deathtouch
   * Delirium - ~ gets +2/+2 as long as there are four or more card types among cards in your graveyard. */

   deathtouch(player, card, event);

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card)
	  && delirium(player) && !is_humiliated(player, card))
	event_result += 2;

  return 0;
}

int card_grapple_with_the_past(int player, int card, event_t event)
{
  /* Grapple with the Past	|1|G	0x200edd2
   * Instant
   * Put the top three cards of your library into your graveyard, then you may return a creature or land card from your graveyard to your hand. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  mill(player, 3);

	  if (any_in_graveyard_by_type(player, TYPE_CREATURE | TYPE_LAND))
		{
		  test_definition_t test;
		  new_default_test_definition(&test, TYPE_CREATURE | TYPE_LAND, "Select a creature or land card.");
		  new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
		}

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

/* Hamlet Captain	|1|G	=>innistrad.c:card_hamlet_captain	0x2003f1d
 * Creature - Human Warrior 2/2
 * Whenever ~ attacks or blocks, other Humans you control get +1/+1 until end of turn. */

int card_ishkanah_grafwidow(int player, int card, event_t event)
{
  /* Ishkanah, Grafwidow	|4|G	0x200edd7
   * Legendary Creature - Spider 3/5
   * Reach
   * Delirium - When ~ enters the battlefield, if there are four or more card types among cards in your graveyard, put three 1/2 |Sgreen Spider creature tokens with reach onto the battlefield.
   * |6|B: Target opponent loses 1 life for each Spider you control. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && delirium(player) ){
		generate_tokens_by_id(player, card, CARD_ID_SPIDER, 3);
	}

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.allowed_controller = 1-player;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  lose_life(inst->targets[0].player, count_subtype(player, TYPE_PERMANENT, SUBTYPE_SPIDER));
	}

  return generic_activated_ability(player, card, event, GAA_CAN_ONLY_TARGET_OPPONENT,
								   MANACOST_XB(6,1), 0, &td, "TARGET_OPPONENT");
}

int card_kessig_prowler(int player, int card, event_t event)
{
/* Kessig Prowler	|G	0x200efe4
 * Creature - Werewolf Horror 2/1
 * |4|G: Transform ~.*/

	double_faced_card(player, card, event);

	if (event == EVENT_RESOLVE_ACTIVATION )
	{
		card_instance_t* instance = get_card_instance(player, card);
		if( ! check_special_flags3(instance->parent_controller, instance->parent_card, SF3_CARD_IS_FLIPPED) ){
			transform(instance->parent_controller, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XG(4,1), 0, NULL, NULL);
}
// * --TRANSFORM--
 /* Sinuous Predator	""	= conspiracy.c:card_charging_rhino (and unimplemented Stalking Tiger, Ironhoof Ox, Norwood Riders),
 * though it works poorly
 * Creature - Eldrazi Werewolf 4/4
 * ~ can't be blocked by more than one creature. */

int card_noose_constrictor(int player, int card, event_t event)
{
  /* Noose Constrictor	|1|G	0x200ee09
   * Creature - Snake 2/2
   * Reach
   * Discard a card: ~ gets +1/+1 until end of turn. */

  if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST)
	return hand_count[player];

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (in_play(inst->parent_controller, inst->parent_card))
		pump_until_eot_merge_previous(player, card, inst->parent_controller, inst->parent_card, 1,1);
	}

  return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
}

int permeating_mass_legacy(int player, int card, event_t event)
{
	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_CHANGE_TYPE && affect_me(p, c) && instance->targets[1].card > 0 ){
			event_result = instance->targets[1].card;
		}
	}

	return 0;
}

int card_permeating_mass(int player, int card, event_t event)
{
/* Permeating Mass	|G	0x200efe9
 * Creature - Spirit 1/3
 * Whenever ~ deals combat damage to a creature, that creature becomes a copy of ~. */

	if( ! in_play(player, card) || is_humiliated(player, card) ){
		return 0;
	}

	if( event == EVENT_DEAL_DAMAGE ){
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t *damage = get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card != -1 && damage->damage_source_player == player &&
				damage->damage_source_card == card && damage->info_slot > 0 &&
				is_what(damage->damage_target_player, damage->damage_target_card, TYPE_CREATURE)
			  ){
				if( instance->info_slot < 0 ){
					instance->info_slot = 0;
				}
				if( instance->info_slot < 10 ){
					instance->targets[instance->info_slot].player = damage->damage_target_player;
					instance->targets[instance->info_slot].card = damage->damage_target_card;
					instance->info_slot++;
				}
			}
		}
	}

	if( get_card_instance(player, card)->info_slot > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
		reason_for_trigger_controller == player )
	{
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				card_instance_t *instance = get_card_instance(player, card);
				int i;
				for(i=0; i<instance->info_slot; i++){
					int legacy = create_targetted_legacy_effect(player, card, &permeating_mass_legacy,
																instance->targets[i].player, instance->targets[i].card);
					card_instance_t *leg = get_card_instance(player, legacy);
					leg->targets[1].card = instance->internal_card_id;;
				}
				instance->info_slot = 0;
		}
	}

	return 0;
}

/* Prey Upon	|G	=>innistrad.c:card_prey_upon	0x20049db
 * Sorcery
 * Target creature you control fights target creature you don't control. */

/* Primal Druid	|1|G	=>mirrodin_besieged.c:card_viridian_emissary	0x2002ba4
 * Creature - Human Druid 0/3
 * When ~ dies, you may search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library. */

int card_shrill_howler(int player, int card, event_t event)
{
  /* Shrill Howler	|2|G	0x200eddc
   * Creature - Werewolf Horror 3/1
   * Creatures with power less than ~'s power can't block it.
   * |5|G: Transform ~. */

  double_faced_card(player, card, event);

  if (event == EVENT_BLOCK_LEGALITY
	  && card == attacking_card && player == attacking_card_controller
	  && get_power(affected_card_controller, affected_card) < get_power(player, card)
	  && !is_humiliated(player, card))
	event_result = 1;

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (in_play(inst->parent_controller, inst->parent_card)
		  && !check_special_flags3(inst->parent_controller, inst->parent_card, SF3_CARD_IS_FLIPPED))
		transform(inst->parent_controller, inst->parent_card);
	}

  return generic_activated_ability(player, card, event, 0, MANACOST_XG(5,1), 0, NULL, NULL);
}
// --TRANSFORM--
int card_howling_chorus(int player, int card, event_t event)
{
  /* Howling Chorus	""	0x0x200ede1
   * Creature - Eldrazi Werewolf 3/5
   * Creatures with power less than ~'s power can't block it.
   * Whenever ~ deals combat damage to a player, put a 3/2 colorless Eldrazi Horror creature token onto the battlefield. */

  if (event == EVENT_BLOCK_LEGALITY
	  && card == attacking_card && player == attacking_card_controller
	  && get_power(affected_card_controller, affected_card) < get_power(player, card)
	  && !is_humiliated(player, card))
	event_result = 1;

  if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE))
	generate_token_by_id(player, card, CARD_ID_ELDRAZI_HORROR);

  return 0;
}

int card_somberwald_stag(int player, int card, event_t event)
{
  /* Somberwald Stag	|3|G|G	0x200ede6
   * Creature - Elk 4/3
   * When ~ enters the battlefield, you may have it fight target creature you don't control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = td.preferred_controller = 1-player;

	  card_instance_t* inst = get_card_instance(player, card);
	  inst->number_of_targets = 0;
	  if (can_target(&td) && pick_next_target_noload(&td, "Select target creature you don't control."))
		fight(player, card, inst->targets[0].player, inst->targets[0].card);
	}

  return 0;
}

int card_spirit_of_the_hunt(int player, int card, event_t event)
{
  /* Spirit of the Hunt	|1|G|G	0x200edeb
   * Creature - Wolf Spirit 3/3
   * Flash
   * When ~ enters the battlefield, each other creature you control that's a Wolf or a Werewolf gets +0/+3 until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.subtype = SUBTYPE_WOLF;
	  test.sub2 = SUBTYPE_WEREWOLF;
	  test.subtype_flag = F2_MULTISUBTYPE;
      test.not_me = 1;

	  pump_creatures_until_eot(player, card, player, 0, 0,3, 0,0, &test);
	}

  return flash(player, card, event);
}

int card_splendid_reclamation(int player, int card, event_t event)
{
  /* Splendid Reclamation	|3|G	0x200edf0
   * Sorcery
   * Return all land cards from your graveyard to the battlefield tapped. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  reanimate_all(player, card, player, TYPE_LAND,MATCH, 0,0, 0,0, 0,0, -1,0, REANIMATE_TAP);
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

/* Springsage Ritual	|3|G	=>m11.c:card_solemn_offering	0x2002767
 * Instant
 * Destroy target artifact or enchantment. You gain 4 life. */

/* Swift Spinner	|3|G	=>time_spiral.c:card_ashcoat_bear	0x200cb63
 * Creature - Spider 2/3
 * Flash
 * Reach */

/* Tangleclaw Werewolf	|2|G|G	0x000000
 * Creature - Werewolf Horror 2/4
 * ~ can block an additional creature each combat.
 * |6|G: Transform ~.
 * --TRANSFORM--
 * Fibrous Entangler	""	0x000000
 * Creature - Eldrazi Werewolf 4/6
 * Vigilance
 * ~ must be blocked if able.
 * ~ can block an additional creature each combat. */

int card_ulvenwald_captive(int player, int card, event_t event)
{
  /* Ulvenwald Captive	|1|G	0x200edf5
   * Creature - Werewolf Horror 1/2
   * Defender
   * |T: Add |G to your mana pool.
   * |5|G|G: Transform ~. */

  double_faced_card(player, card, event);

  if (IS_ACTIVATING(event))
	{
	  enum
	  {
		CHOICE_MANA = 1,
		CHOICE_TRANSFORM
	  } choice = DIALOG(player, card, event,
						DLG_AUTOCHOOSE_IF_1, DLG_NO_DISPLAY_FOR_AI,
						"Mana",
							mana_producer(player, card, EVENT_CAN_ACTIVATE),
							paying_mana(),
						"Transform",
							!paying_mana() && can_use_activated_abilities(player, card),
							3,
							DLG_MANA(MANACOST_XG(5,2)));

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (choice == 0)	// cancelled
		return 0;
	  else if (choice == CHOICE_TRANSFORM)
		{
		  if (event == EVENT_RESOLVE_ACTIVATION)
			{
			  card_instance_t* inst = get_card_instance(player, card);
			  if (in_play(inst->parent_controller, inst->parent_card)
				  && !check_special_flags3(inst->parent_controller, inst->parent_card, SF3_CARD_IS_FLIPPED))
				transform(inst->parent_controller, inst->parent_card);
			}
		  return 0;
		}
	}

  return mana_producer(player, card, event);
}
// --TRANSFORM--
/* Ulvenwald Abomination	""	=>scars_of_mirrodin.c:card_palladium_myr	0x200acf5
 * Creature - Eldrazi Werewolf 4/6
 * |T: Add |C|C to your mana pool. */

static int ulvenwald_observer_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_ULVENWALD_OBSERVER ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( in_play(affected_card_controller, affected_card) ){
			int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
			if( kill_code ){
				if( instance->damage_target_player > -1 ){
					int p = instance->damage_target_player;
					int c = instance->damage_target_card;
					if( affect_me(p, c) ){
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
						if( instance->targets[11].player < 0 ){
							instance->targets[11].player = 0;
						}
						SET_BYTE1(instance->targets[11].player) = 1;
					}
				}
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				this_test.toughness = 3;
				this_test.toughness_flag = F5_TOUGHNESS_GREATER_THAN_VALUE;
				count_for_gfp_ability(player, card, event, player, 0, &this_test);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			draw_cards(player, amount);
			kill_card(player, card, KILL_EXILE);
		}
	}

	//Reset counts if the legacy is still invisible
	if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && event == EVENT_END_TRIGGER ){
		get_card_instance(player, card)->targets[11].player = 0;
	}

	if( event == EVENT_CLEANUP && ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_ulvenwald_observer(int player, int card, event_t event){
  /* Ulvenwald Observer	|4|G|G	0x200edfa
   * Creature - Treefolk 6/6
   * Whenever a creature you control with toughness 4 or greater dies, draw a card. */

   if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)ulvenwald_observer_effect ){
					if( (inst->damage_target_player == player && inst->damage_target_card == card) ||
						(inst->targets[0].player == player && inst->targets[0].card == card) )
					{
						found = 1;
						break;
					}
				}
			}
		}
		if( ! found ){
			int legacy = create_targetted_legacy_effect(player, card, &ulvenwald_observer_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.toughness = 3;
		this_test.toughness_flag = F5_TOUGHNESS_GREATER_THAN_VALUE;
		count_for_gfp_ability(player, card, event, player, 0, &this_test);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			draw_cards(player, amount);
		}
	}

	return 0;
}

static const char* is_not_transformed(int who_chooses, int player, int card)
{
	if( ! check_special_flags3(player, card, SF3_CARD_IS_FLIPPED) ){
		return NULL;
	}
	return "this Werewolf is already transformed.";
}

int card_waxing_moon(int player, int card, event_t event)
{
/* Waxing Moon	|1|G	0x200efee
 * Instant
 * Transform up to one target Werewolf you control. Creatures you control gain trample until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.required_subtype = SUBTYPE_WEREWOLF;
	td.preferred_controller = player;
	if( player == AI ){
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		td.extra = (int32_t)is_not_transformed;
	}

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->number_of_targets == 1 && valid_target(&td) ){
			transform(instance->targets[0].player, instance->targets[0].card);
		}
		pump_subtype_until_eot(player, card, player, -1, 0, 0, KEYWORD_TRAMPLE, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT | GS_OPTIONAL_TARGET, &td,
						"Select target Werewolf you control.", 1, NULL);
}

int card_wolfkin_bond(int player, int card, event_t event)
{
  /* Wolfkin Bond	|4|G	0x200edff
   * Enchantment - Aura
   * Enchant creature
   * When ~ enters the battlefield, put a 2/2 |Sgreen Wolf creature token onto the battlefield.
   * Enchanted creature gets +2/+2. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
		generate_token_by_id(player, card, CARD_ID_WOLF);

	return generic_aura(player, card, event, player, 2,2, 0,0, 0,0,0);
}

int card_woodcutters_grit(int player, int card, event_t event)
{
  /* Woodcutter's Grit	|2|G	0x200ee04
   * Instant
   * Target creature you control gets +3/+3 and gains hexproof until end of turn. */

  return vanilla_instant_pump(player, card, event, player, player, 3,3, 0,SP_KEYWORD_HEXPROOF);
}

/* Woodland Patrol	|2|G	=>unlimited.c:card_serra_angel	0x200c122
 * Creature - Human Scout 3/2
 * Vigilance */

/*** Multi ***/

int card_bloodhall_priest(int player, int card, event_t event)
{
	/* Bloodhall Priest	|2|B|R	0x200ee0e
	 * Creature - Vampire Cleric 4/4
	 * Whenever ~ enters the battlefield or attacks, if you have no cards in hand, ~ deals 2 damage to any target.
	 * Madness |1|B|R */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( hand_count[player] == 0 && (event == EVENT_ETB_ABILITY || declare_attackers_trigger(player, card, event, 0, player, card)) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		get_card_instance(player, card)->number_of_targets = 0;
		if (can_target(&td) && pick_target(&td, "TARGET_ANY"))
			damage_target0(player, card, 2);
	}

	return madness(player, card, event, MANACOST_XBR(1,1,1));
}

int card_campaign_of_vengeance(int player, int card, event_t event)
{
  /* Campaign of Vengeance	|3|W|B	0x200ee13
   * Enchantment
   * Whenever a creature you control attacks, defending player loses 1 life and you gain 1 life. */

  int attackers = declare_attackers_trigger(player, card, event, 0, player, -1);
  if (attackers)
	for (; attackers > 0; --attackers)	// separate life/gain-life event for each trigger
	  {
		lose_life(1-player, 1);
		gain_life(player, 1);
	  }

  return global_enchantment(player, card, event);
}

int card_gisa_and_geralf(int player, int card, event_t event)
{
/* Gisa and Geralf	|2|U|B	0x200eff3
 * Legendary Creature - Human Wizard 4/4
 * When ~ enters the battlefield, put the top four cards of your library into your graveyard.
 * During each of your turns, you may cast a Zombie creature card from your graveyard. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		mill(player, 4);
	}

	if( in_play(player, card) && ! is_humiliated(player, card) ){

		if( event == EVENT_CAN_ACTIVATE && get_card_instance(player, card)->info_slot == 0 )
		{
			int i = 0;
			while( get_grave(player)[i] != -1 ){
					if( has_subtype(-1, get_grave(player)[i], SUBTYPE_ZOMBIE) &&
						is_what(-1, get_grave(player)[i], TYPE_CREATURE) )
					{
						int tr = can_legally_play_iid(player, get_grave(player)[i]);
						if( tr && has_mana_to_cast_iid(player, event, get_grave(player)[i]) ){
							if( tr == 99 ){
								return 99;
							}
							if( can_sorcery_be_played(player, event) ||
								(cards_data[get_grave(player)[i]].type & (TYPE_INSTANT | TYPE_INTERRUPT)) )
							{
								return 1;
							}
						}
					}
					i++;
			}
		}

		if( event == EVENT_ACTIVATE ){
			int playable[2][count_graveyard(player)];
			int pc = 0;
			int i = 0;
			while( get_grave(player)[i] != -1 ){
					if( has_subtype(-1, get_grave(player)[i], SUBTYPE_ZOMBIE) &&
						is_what(-1, get_grave(player)[i], TYPE_CREATURE) )
					{
						int tr = can_legally_play_iid(player, get_grave(player)[i]);
						if( tr && has_mana_to_cast_iid(player, event, get_grave(player)[i]) ){
							if( can_sorcery_be_played(player, event) ||
								(cards_data[get_grave(player)[i]].type & (TYPE_INSTANT | TYPE_INTERRUPT)) )
							{
								playable[0][pc] = get_grave(player)[i];
								playable[1][pc] = i;
								pc++;
							}
						}
					}
					i++;
			}

			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a Zombie card to play.");

			int selected = select_card_from_zone(player, player, playable[0], pc, 0, AI_MAX_VALUE, -1, &this_test);
			if( selected != -1 ){
				if( charge_mana_from_id(player, -1, event, cards_data[playable[0][selected]].id) ){
					get_card_instance(player, card)->info_slot = 1;
					play_card_in_grave_for_free(player, playable[1][selected], 1);
					cant_be_responded_to = 1;	// The spell will be respondable to, but this (fake) activation won't
				}
				else{
					spell_fizzled = 1;
					return 0;
				}
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}

		if( event == EVENT_CLEANUP ){
			get_card_instance(player, card)->info_slot = 0;
		}
	}

	return 0;
}

int card_grim_flayer(int player, int card, event_t event)
{
  /* Grim Flayer	|B|G	0x200ee18
   * Creature - Human Warrior 2/2
   * Trample
   * Whenever ~ deals combat damage to a player, look at the top three cards of your library. Put any number of them into your graveyard and the rest back on top of your library in any order.
   * Delirium - ~ gets +2/+2 as long as there are four or more card types among cards in your graveyard. */

  if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE))
	reveal_top_cards_of_library_and_choose_type(player, card, player, 3, 0, TUTOR_GRAVE, 0, TUTOR_DECK, 0, TYPE_ANY);

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card)
	  && delirium(player) && !is_humiliated(player, card))
	event_result += 2;

  return 0;
}

int card_herons_grace_champion(int player, int card, event_t event)
{
  /* Heron's Grace Champion	|2|G|W	0x200ee1d
   * Creature - Human Knight 3/3
   * Flash
   * Lifelink
   * When ~ enters the battlefield, other Humans you control get +1/+1 and gain lifelink until end of turn. */

  lifelink(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.subtype = SUBTYPE_HUMAN;
	  test.not_me = 1;

	  pump_creatures_until_eot(player, card, player, 0, 1,1, 0,SP_KEYWORD_LIFELINK, &test);
	}

  return flash(player, card, event);
}

/* Mercurial Geists	|2|U|R	=>rise_of_the_eldrazi.c:card_kiln_fiend	0x2004b0c
 * Creature - Spirit 1/3
 * Flying
 * Whenever you cast an instant or sorcery spell, ~ gets +3/+0 until end of turn. */

static int fx_creatures_power_le_2_cant_block(int player, int card, event_t event)
{
  if (event == EVENT_ABILITIES && get_power(affected_card_controller, affected_card) < 3)
	cannot_block(affected_card_controller, affected_card, event);

  if (event == EVENT_CLEANUP)
	kill_card(player, card, KILL_EXILE);

  return 0;
}
int card_mournwillow(int player, int card, event_t event)
{
  /* Mournwillow	|1|B|G	0x200ee22
   * Creature - Plant Skeleton 3/2
   * Haste
   * Delirium - When ~ enters the battlefield, if there are four or more card types among cards in your graveyard, creatures with power 2 or less can't block this turn. */

    haste(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( delirium(player) )
			create_legacy_effect(player, card, &fx_creatures_power_le_2_cant_block);
	}
	return 0;
}

/* Ride Down	|R|W	=>khans_of_tarkir.c:card_ride_down	0x200d180
 * Instant
 * Destroy target blocking creature. Creatures that were blocked by that creature this combat gain trample until end of turn. */

int card_spell_queller(int player, int card, event_t event){
/* Spell Queller	|1|W|U	0x200eff8
 * Creature - Spirit 2/3
 * Flash
 * Flying
 * When ~ enters the battlefield, exile target spell with converted mana cost 4 or less.
 * When ~ leaves the battlefield, the exiled card's owner may cast that card without paying its mana cost. */

	if( event == EVENT_CAN_CAST ){
		target_definition_t td;
		counterspell_target_definition(player, card, &td, TYPE_ANY);
		td.extra = 4;
		td.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;

		card_instance_t *instance = get_card_instance( player, card );
		if( counterspell(player, card, event, &td, 0) ){
			instance->info_slot = 1;
			return 99;
		}
		else{
			instance->info_slot = 0;
			return 1;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card)  && current_turn == player){
		target_definition_t td;
		counterspell_target_definition(player, card, &td, TYPE_ANY);
		td.extra = 4;
		td.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;

		card_instance_t *instance = get_card_instance( player, card );
		if( current_turn == player ){
			ai_modifier -= 10;
		}
		if( instance->info_slot == 1 ){
			counterspell(player, card, event, &td, 0);
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance( player, card );
		if( instance->info_slot == 1 ){
			instance->targets[1].player = get_owner(instance->targets[0].player, instance->targets[0].card);
			instance->targets[1].card = get_original_internal_card_id(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
	}

	if( leaves_play(player, card, event) ){
		card_instance_t *instance = get_card_instance( player, card );
		if( instance->targets[1].card > -1 ){
			if( check_rfg(instance->targets[1].player, cards_data[instance->targets[1].card].id) ){
				remove_card_from_rfg(instance->targets[1].player, cards_data[instance->targets[1].card].id);
				add_card_to_hand(instance->targets[1].player, instance->targets[1].card);
			}
		}
	}

	return flash(player, card, event);
}

static int tamiyo_field_researcher_effect(int player, int card, event_t event){
	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( damage_dealt_by_me_arbitrary(p, c, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_REPORT_DAMAGE_DEALT, player, card) ){
			draw_cards(player, 1);
		}
	}

	if( current_turn != player && event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_tamiyo_field_researcher(int player, int card, event_t event){
/* Tamiyo, Field Researcher	|1|G|W|U	0x200effd
 * Planeswalker - Tamiyo (4)
 * +1: Choose up to two target creatures. Until your next turn, whenever either of those creatures deals combat damage, you draw a card.
 * -2: Tap up to two target nonland permanents. They don't untap during their controller's next untap step.
 * -7: Draw three cards. You get an emblem with "You may cast nonland cards from your hand without paying their mana costs." */

	if( ! IS_GAA_EVENT(event) ){
		return planeswalker(player, card, event, 4);
	}

	enum{
		CHOICE_CREATURES = 1,
		CHOICE_LOCK,
		CHOICE_EMBLEM
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.allow_cancel = 2;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_PERMANENT);
	td2.illegal_type = TYPE_LAND;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;

		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
							"Choose creatures", 1, 10, 		+1,
							"Lock permanents", 	1, 15, 		-2,
							"Emblem", 			1, 20, 		-7);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_CREATURES ){
			int i;
			for(i=0; i<2; i++){
				new_pick_target(&td, "TARGET_CREATURE", i, 0);
			}
		}
		if( choice == CHOICE_LOCK ){
			int i;
			for(i=0; i<2; i++){
				new_pick_target(&td2, "Select target nonland permanent.", i, GS_LITERAL_PROMPT);
			}
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		if( choice == CHOICE_CREATURES ){
			int i;
			for(i=0; i<instance->number_of_targets; i++){
				create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &tamiyo_field_researcher_effect,
												instance->targets[i].player, instance->targets[i].card);
			}
		}
		if( choice == CHOICE_LOCK ){
			int i;
			for(i=0; i<instance->number_of_targets; i++){
				effect_frost_titan(instance->parent_controller, instance->parent_card,
									instance->targets[i].player, instance->targets[i].card);
			}
		}
		if( choice == CHOICE_EMBLEM ){
			draw_cards(player, 3);
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_TAMIYO_FIELD_RESEARCHERS_EMBLEM);
		}
	}

	return planeswalker(player, card, event, 4);
}

int card_tamiyo_field_researcher_emblem(int player, int card, event_t event){
/* Tamiyo, Field Researcher's Emblem	""	0x200f002
 * Emblem - Tamiyo
 * You may cast nonland cards from your hand without paying their mana costs. */
	return card_omniscience(player, card, event);
}

int card_ulrich_of_the_krallenhorde(int player, int card, event_t event)
{
  /* Ulrich of the Krallenhorde	|3|R|G	0x200ee27
   * Legendary Creature - Human Werewolf 4/4
   * Whenever this creature enters the battlefield or transforms into ~, target creature gets +4/+4 until end of turn.
   * At the beginning of each upkeep, if no spells were cast last turn, transform ~. */

	double_faced_card(player, card, event);
	human_moon_phases(player, card, event);
	check_legend_rule(player, card, event);
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || event == EVENT_TRANSFORMED ){
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.preferred_controller = player;
	  td.allow_cancel = 0;

	  card_instance_t* inst = get_card_instance(player, card);
	  inst->number_of_targets = 0;
	  if (can_target(&td) && pick_target(&td, "TARGET_CREATURE"))
		pump_until_eot_merge_previous(player, card, inst->targets[0].player, inst->targets[0].card, 4,4);
	}

  return 0;
}
// --TRANSFORM--
int card_ulrich_uncontested_alpha(int player, int card, event_t event)
{
  /* Ulrich, Uncontested Alpha	""(RG)	0x200ee2c
   * Legendary Creature - Werewolf 6/6
   * Whenever this creature transforms into ~, you may have it fight target non-Werewolf creature you don't control.
   * At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~. */

  check_legend_rule(player, card, event);
  werewolf_moon_phases(player, card, event);

  if (event == EVENT_TRANSFORMED)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = 1-player;
	  td.required_subtype = SUBTYPE_WEREWOLF;
	  td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

	  card_instance_t* inst = get_card_instance(player, card);
	  inst->number_of_targets = 0;
	  if (can_target(&td) && pick_next_target_noload(&td, "Select target non-Werewolf creature you don't control."))
		fight(player, card, inst->targets[0].player, inst->targets[0].card);
	}

  return 0;
}

/*** Artifact ***/

/* Cathar's Shield	|0	=>scars_of_mirrodin.c:card_accorder_shield	0x200416b
 * Artifact - Equipment
 * Equipped creature gets +0/+3 and has vigilance.
 * Equip |3 */

int card_cryptolith_fragment(int player, int card, event_t event)
{
  /* Cryptolith Fragment	|3	0x200ee31
   * Artifact
   * ~ enters the battlefield tapped.
   * |T: Add one mana of any color to your mana pool. Each player loses 1 life.
   * At the beginning of your upkeep, if each player has 10 or less life, transform ~. */

  double_faced_card(player, card, event);
  comes_into_play_tapped(player, card, event);

  if ((event == EVENT_UPKEEP_TRIGGER_ABILITY || trigger_condition == TRIGGER_UPKEEP)
	  && life[0] <= 10 && life[1] <= 10)
	{
	  upkeep_trigger_ability(player, card, event, player);

	  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
		transform(player, card);
	}

  int rval = mana_producer(player, card, event);
  if (event == EVENT_ACTIVATE && cancel != 1)
	APNAP(p, lose_life(p, 1));
  return rval;
}
// --TRANSFORM--
int card_aurora_of_emrakul(int player, int card, event_t event)
{
  /* Aurora of Emrakul	""	0x200ee36
   * Creature - Eldrazi Reflection 1/4
   * Flying, deathtouch
   * Whenever ~ attacks, each opponent loses 3 life. */

  deathtouch(player, card, event);

  if (declare_attackers_trigger(player, card, event, 0, player, card))
	lose_life(1-current_turn, 3);

  return 0;
}

int card_cultists_staff(int player, int card, event_t event)
{
  /* Cultist's Staff	|2	200ee3b
   * Artifact - Equipment
   * Equipped creature gets +2/+2.
   * Equip |3 */

  return vanilla_equipment(player, card, event, 3, 2,2, 0,0);
}

/* Field Creeper	|2	=>vanilla	0x401000
 * Artifact Creature - Scarecrow 2/1 */

int card_geist_fueled_scarecrow(int player, int card, event_t event)
{
  /* Geist-Fueled Scarecrow	|4	0x200ee40
   * Artifact Creature - Scarecrow 4/4
   * Creature spells you cast cost |1 more to cast. */

  if (event == EVENT_MODIFY_COST_GLOBAL)
	{
	  if ((affected_card_controller == -1 ? event_result : affected_card_controller) == player
		  && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
		  && in_play(player, card) && !is_humiliated(player, card))
		COST_COLORLESS += 1;

	  return 0;	// explicit return at end of EVENT_MODIFY_COST_GLOBAL
	}

  return 0;
}

int card_lupine_prototype(int player, int card, event_t event)
{
  /* Lupine Prototype	|2	0x200ee45
   * Artifact Creature - Wolf Construct 5/5
   * ~ can't attack or block unless a player has no cards in hand. */

  if (event == EVENT_ATTACK_LEGALITY && affect_me(player, card)
	  && (hand_count[0] > 0 && hand_count[1] > 0)
	  && !is_humiliated(player, card))
	event_result = 1;

  if (event == EVENT_ABILITIES && affect_me(player, card)
	  && (hand_count[0] > 0 && hand_count[1] > 0)
	  && !is_humiliated(player, card) && in_play(player, card))
	cannot_block(player, card, event);

  return 0;
}

/* Slayer's Cleaver	|3	0x000000
 * Artifact - Equipment
 * Equipped creature gets +3/+1 and must be blocked by an Eldrazi if able.
 * Equip |4 */

int card_soul_separator(int player, int card, event_t event)
{
/* Soul Separator	|3	0x200f007
 * Artifact
 * |5, |T, Sacrifice ~: Exile target creature card from your graveyard.
 * 		Put a token onto the battlefield that's a copy of that card except it's 1/1, it's a Spirit in addition to its other types,
 *		and it has flying. Put a |Sblack Zombie creature token onto the battlefield with power equal to that card's power and
 *		toughness equal to that card's toughness. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(5), 0, NULL, NULL) ){
			if( count_graveyard_by_type(player, TYPE_CREATURE) && ! graveyard_has_shroud(player) ){
				return 1;
			}
			if( count_graveyard_by_type(1-player, TYPE_CREATURE) && ! graveyard_has_shroud(1-player) ){
				return 1;
			}
		}
	}

	if (event == EVENT_ACTIVATE)
	{
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(5)) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");
			if( select_target_from_either_grave(player, card, 0, AI_MAX_CMC, AI_MAX_CMC, &test, 0, 1) ){
				add_state(player, card, STATE_TAPPED);
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION)
	{
		card_instance_t *instance = get_card_instance( player, card);

		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);

		if( selected != -1 ){
			int iid = get_grave(instance->targets[0].player)[selected];
			rfg_card_from_grave(instance->targets[0].player, selected);

			token_generation_t token;
			default_token_definition(player, card, cards_data[iid].id, &token);
			token.pow = token.tou = 1;
			token.key_plus = KEYWORD_FLYING;
			token.color_forced = COLOR_TEST_WHITE;
			token.action = TOKEN_ACTION_ADD_SUBTYPE;
			token.action_argument = SUBTYPE_SPIRIT;
			generate_token(&token);

			default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
			token.pow = get_base_power_iid(instance->targets[0].player, iid);
			token.tou = get_base_toughness_iid(instance->targets[0].player, iid);
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
			generate_token(&token);
		}
	}

  return 0;
}

int card_stitchers_graft(int player, int card, event_t event)
{
  /* Stitcher's Graft	|1	0x200ee4a
   * Artifact - Equipment
   * Equipped creature gets +3/+3.
   * Whenever equipped creature attacks, it doesn't untap during its controller's next untap step.
   * Whenever ~ becomes unattached from a permanent, sacrifice that permanent.
   * Equip |2 */

	if( in_play(player, card) && is_equipping(player, card) && ! is_humiliated(player, card) ){
		card_instance_t* inst = get_card_instance(player, card);
		if( declare_attackers_trigger(player, card, event, 0, inst->damage_target_player, inst->damage_target_card)){
			does_not_untap_effect(player, card, inst->damage_target_player, inst->damage_target_card, 0, 1);
		}
	}

	if( event == EVENT_UNATTACH || leaves_play(player, card, event) ){
		if( is_equipping(player, card) ){
			card_instance_t *instance = get_card_instance(player, card);
			if( player == instance->damage_target_player ){
				kill_card(instance->damage_target_player, instance->damage_target_card, KILL_SACRIFICE);
			}
		}
	}

	return vanilla_equipment(player, card, event, 2, 3,3, 0,0);
}

int card_terrarion(int player, int card, event_t event)
{
/* Terrarion	|1	0x200f00c
 * Artifact
 * ~ enters the battlefield tapped.
 * |2, |T, Sacrifice ~: Add two mana in any combination of colors to your mana pool.
 * When ~ is put into a graveyard from the battlefield, draw a card. */

	comes_into_play_tapped(player, card, event);

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( mana_producer(player, card, EVENT_CAN_ACTIVATE) ){
			if( can_sacrifice_this_as_cost(player, card) && has_mana(player, COLOR_ANY, 2) ){
				declare_mana_available_any_combination_of_colors(player, COLOR_TEST_ANY_COLORED, 2);
			}
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( mana_producer(player, card, event) ){
			if( can_sacrifice_this_as_cost(player, card) && has_mana(player, COLOR_ANY, 2) ){
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			add_state(player, card, STATE_TAPPED);
			produce_mana_any_combination_of_colors(player, COLOR_TEST_ANY_COLORED, 2, NULL);
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_thirsting_axe(int player, int card, event_t event)
{
/* Thirsting Axe	|3	0x200f011
 * Artifact - Equipment
 * Equipped creature gets +4/+0.
 * At the beginning of your end step, if equipped creature didn't deal combat damage to a creature this turn, sacrifice it.
 * Equip |2 */

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance( player, card);
		int p = instance->parent_controller;
		int c = instance->parent_card;
		if( damage_dealt_by_me_arbitrary(player, card, event, DDBM_MUST_DAMAGE_CREATURE, p, c) ){
			instance->info_slot = 66;
		}
		if( trigger_condition == TRIGGER_EOT && affect_me(player, card) && reason_for_trigger_controller == player ){
			if( instance->info_slot ==  66 ){
				instance->info_slot = 0;
			}
			else{
				if( eot_trigger(player, card, event) ){
					kill_card(p, c, KILL_SACRIFICE);
				}
			}
		}
	}

	return vanilla_equipment(player, card, event, 2, 4, 0, 0, 0);
}

/*** Land ***/

int card_geier_reach_sanitarium(int player, int card, event_t event)
{
  /* Geier Reach Sanitarium	""	0x200ee4f
   * Legendary Land
   * |T: Add |C to your mana pool.
   * |2, |T: Each player draws a card, then discards a card. */

  check_legend_rule(player, card, event);

  if (IS_ACTIVATING(event))
	{
	  enum
	  {
		CHOICE_MANA = 1,
		CHOICE_LOOT
	  } choice = DIALOG(player, card, event,
						DLG_AUTOCHOOSE_IF_1, DLG_NO_DISPLAY_FOR_AI,
						"Mana",
							mana_producer(player, card, EVENT_CAN_ACTIVATE),
							paying_mana(),
						"Draw and discard",
							/* This calls generic_activated_ability() and charges mana manually instead of using a
							 * DLG_MANA() clause because both this and its mana ability have T in their costs */
							!paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE,
																		GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL),
							hand_count[AI] - 2*hand_count[HUMAN] + landsofcolor_controlled[AI][COLOR_ANY]);

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (choice == 0)	// cancelled
		return 0;
	  else if (choice == CHOICE_LOOT)
		{
		  if (event == EVENT_ACTIVATE)
			{
			  add_state(player, card, STATE_TAPPED);
			  if (!charge_mana_for_activated_ability(player, card, MANACOST_X(2)))
				remove_state(player, card, STATE_TAPPED);
			}
		  else if (event == EVENT_RESOLVE_ACTIVATION)
			APNAP(p,
				  draw_a_card(p);
				  discard(p, 0, player));

		  return 0;
		}
	  // else fall through to mana_producer below
	}

  return mana_producer(player, card, event);
}

static int hanweir_battlements_meld_ability(int player, int card, int mode){
// Mode = 1 --> Check
// Mode = 2 --> Meld
	int done = 0;
	int marked[2] = {-1, -1};
	int i;
	for(i=0; i<active_cards_count[player] && done != 3; i++){
		if( in_play(player, i) && get_owner(player, i) == player ){
			if( get_card_name(player, i) == CARD_ID_HANWEIR_BATTLEMENTS ){
				done |= 1;
				marked[0] = i;
			}
			if( get_card_name(player, i) == CARD_ID_HANWEIR_GARRISON ){
				done |= 2;
				marked[1] = i;
			}
		}
	}
	if( done == 3 && mode == 2){
		for(i=0; i<2; i++){
			kill_card(player, marked[i], KILL_EXILE);
		}
		int card_added = add_card_to_hand(player, get_internal_card_id_from_csv_id(CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP));
		put_into_play(player, card_added);
	}
	return done == 3 ? 1 : 0;
}

int card_hanweir_battlements(int player, int card, event_t event){
/* Hanweir Battlements	""	0x200f016
 * Land
 * |T: Add |C to your mana pool.
 * |R, |T: Target creature gains haste until end of turn.
 * |3|R|R, |T: If you both own and control ~ and a creature named Hanweir Garrison, exile them, then meld them into
 * Hanweir, the Writhing Township. */

	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_HASTE,
		CHOICE_MELD
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int can_haste = ! paying_mana() && can_target(&td) && has_mana_for_activated_ability(player, card, MANACOST_R(1)) ? 1 : 0;
		int priority_haste = current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS ? 15 : 5;
		int can_meld = ! paying_mana() && has_mana_for_activated_ability(player, card, MANACOST_XR(4, 2)) ? 1 : 0;
		if( player == AI && can_meld ){
			can_meld = hanweir_battlements_meld_ability(player, card, 1);
		}

		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Give haste", can_haste, priority_haste,
							"Meld", can_meld, priority_haste == 15 ? 20 : 1);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_HASTE ){
			if( charge_mana_for_activated_ability(player, card, MANACOST_R(1)) ){
				if( pick_target(&td, "TARGET_CREATURE") ){
					tap_card(player, card);
				}
			}
		}
		if( choice == CHOICE_MELD ){
			add_state(player, card, STATE_TAPPED);
			if( ! charge_mana_for_activated_ability(player, card, MANACOST_XR(3, 2)) ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		if( choice == CHOICE_HASTE ){
			if( valid_target(&td) ){
				pump_ability_until_eot(instance->parent_controller, instance->parent_card,
										instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_HASTE);
			}
		}
		if( choice == CHOICE_MELD ){
			hanweir_battlements_meld_ability(instance->parent_controller, instance->parent_card, 2);
		}
	}

	return 0;
}
//* --MELD WITH--
int card_hanweir_garrison(int player, int card, event_t event){
 /* Hanweir Garrison	|2|R	0x200f01b
 * Creature - Human Soldier 2/3
 * Whenever ~ attacks, put two 1/1 |Sred Human creature tokens onto the battlefield tapped and attacking.*/

	if( declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_HUMAN, &token);
		token.pow = token.tou = 1;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		token.qty = 2;
		token.action = TOKEN_ACTION_ATTACKING;
		generate_token(&token);
	}

	return 0;
}
// --MELD TO--
int card_hanweir_the_writhing_township(int player, int card, event_t event){
 /* Hanweir, the Writhing Township	""	0x200f020
 * Legendary Creature - Eldrazi Ooze 7/4
 * Trample, haste
 * Whenever ~ attacks, put two 3/2 colorless Eldrazi Horror creature tokens onto the battlefield tapped and attacking. */

	check_legend_rule(player, card, event);
	haste(player, card, event);

	if( declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ELDRAZI_HORROR, &token);
		token.qty = 2;
		token.action = TOKEN_ACTION_ATTACKING;
		generate_token(&token);
	}

	return 0;
}

/* Nephalia Academy	""	0x200e729 --
 * Land
 * If a spell or ability an opponent controls causes you to discard a card,
 * you may reveal that card and put it on top of your library instead of putting it anywhere else.
 * |T: Add |C to your mana pool. */
// The true effect is in 'discard_card_impl'

/*** Tokens ***/

/* Eldrazi Horror	""	=>vanilla	0x401000
 * Creature - Eldrazi Horror 3/2 */

/* Human Wizard	""	=>vanilla	0x401000
 * Creature - Human Wizard 1/1 */
