#include "manalink.h"

// Functions
int count_domain(int player, int card){
	int i, count = 0;
	for (i = COLOR_BLACK; i <= COLOR_WHITE; ++i){
		if (basiclandtypes_controlled[player][i] > 0){
			++count;
		}
	}
	return count;
}

static void familiar(int player, int card, event_t event, color_test_t colors)
{
  /* [Something]scape Familiar	|1|C
   * Creature
   * |S[Color1] spells and |S[Color2] spells you cast cost |1 less to cast. */

  if (event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player
	  && (get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, colors)))
	--COST_COLORLESS;
}

static void gating(int player, int card, event_t event, int c1, int c2){

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;
	int c1s = get_sleighted_color_test(player, card, c1);
	int c2s = get_sleighted_color_test(player, card, c2);
	td.required_color = c1s | c2s;
	td.allow_cancel = 0;

	card_instance_t *instance = get_card_instance(player, card);
	int clr = COLOR_BLACK;
			switch (c1s){
					// The common cases - exactly one color and nothing else
					case COLOR_TEST_BLUE:	clr = COLOR_BLUE;	break;
					case COLOR_TEST_GREEN:	clr = COLOR_GREEN;	break;
					case COLOR_TEST_RED:	clr = COLOR_RED;		break;
					case COLOR_TEST_WHITE:	clr = COLOR_WHITE;	break;
					default: break;
			}
	int clr1 = COLOR_BLACK;
			switch (c2s){
					// The common cases - exactly one color and nothing else
					case COLOR_TEST_BLUE:	clr1 = COLOR_BLUE;	break;
					case COLOR_TEST_GREEN:	clr1 = COLOR_GREEN;	break;
					case COLOR_TEST_RED:	clr1 = COLOR_RED;		break;
					case COLOR_TEST_WHITE:	clr1 = COLOR_WHITE;	break;
					default: break;
			}
	char msg[100];
	scnprintf(msg, 100, "%s", get_sleighted_color_text2(player, card, "Select a %s or %s creature card you control.", clr, clr1));

	if( can_target(&td) && new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
		bounce_permanent(instance->targets[0].player, instance->targets[0].card);
	}
	instance->number_of_targets = 0;
}

void volver1(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white,
			int colorless2, int black2, int blue2, int green2, int red2, int white2, int kicker1_priority, int kicker2_priority
  ){
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags(player, card, SF_NOT_CAST) && ! is_token(player, card) ){
			int options[10] = {
							black > 0 && has_mana_multi(player, colorless, black, 0, 0, 0, 0),
							black2 > 0 && has_mana_multi(player, colorless2, black2, 0, 0, 0, 0),
							blue > 0 && has_mana_multi(player, colorless, 0, blue, 0, 0, 0),
							blue2 > 0 && has_mana_multi(player, colorless2, 0, blue2, 0, 0, 0),
							green > 0 && has_mana_multi(player, colorless, 0, 0, green, 0, 0),
							green2 > 0 && has_mana_multi(player, colorless2, 0, 0, green2, 0, 0),
							red > 0 && has_mana_multi(player, colorless, 0, 0, 0, red, 0),
							red2 > 0 && has_mana_multi(player, colorless2, 0, 0, 0, red2, 0),
							white > 0 && has_mana_multi(player, colorless, 0, 0, 0, 0, white),
							white2 > 0 && has_mana_multi(player, colorless2, 0, 0, 0, 0, white2)
			};

			int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL, DLG_NO_CANCEL,
/*1*/					"Pay black kicker", options[0], kicker1_priority,
/*2*/					"Pay black kicker", options[1], kicker2_priority,
/*3*/					"Pay blue kicker", options[2], kicker1_priority,
/*4*/					"Pay blue kicker", options[3], kicker2_priority,
/*5*/					"Pay green kicker", options[4], kicker1_priority,
/*6*/					"Pay green kicker", options[5], kicker2_priority,
/*7*/					"Pay red kicker", options[6], kicker1_priority,
/*8*/					"Pay red kicker", options[7], kicker2_priority,
/*9*/					"Pay white kicker", options[8], kicker1_priority,
/*10*/					"Pay white kicker", options[9], kicker2_priority,
/*11*/					"Pay both", has_mana_multi(player, colorless+colorless2, black+black2, blue+blue2, green+green2, red+red2, white+white2), kicker1_priority+kicker2_priority,
/*12*/					"Decline", 1, 1);

			if( choice != 12 ){
				int mana_to_charge[6];
				if( choice == 11 ){
					mana_to_charge[0] = colorless+colorless2;
				}
				else{
					mana_to_charge[0] = choice == 1 || choice == 3 || choice == 5 || choice == 7 || choice == 9 ? colorless : colorless2;
				}
				if( choice == 11 ){
					mana_to_charge[1] = black+black2;
				}
				else{
					mana_to_charge[1] = choice == 1 || choice == 3 || choice == 5 || choice == 7 || choice == 9 ? black : black2;
				}
				if( choice == 11 ){
					mana_to_charge[2] = blue+blue2;
				}
				else{
					mana_to_charge[2] = choice == 1 || choice == 3 || choice == 5 || choice == 7 || choice == 9 ? blue : blue2;
				}
				if( choice == 11 ){
					mana_to_charge[3] = green+green2;
				}
				else{
					mana_to_charge[3] = choice == 1 || choice == 3 || choice == 5 || choice == 7 || choice == 9 ? green : green2;
				}
				if( choice == 11 ){
					mana_to_charge[4] = red+red2;
				}
				else{
					mana_to_charge[4] = choice == 1 || choice == 3 || choice == 5 || choice == 7 || choice == 9 ? red : red2;
				}
				if( choice == 11 ){
					mana_to_charge[5] = white+white2;
				}
				else{
					mana_to_charge[5] = choice == 1 || choice == 3 || choice == 5 || choice == 7 || choice == 9 ? white : white2;
				}
				charge_mana_multi(player, mana_to_charge[0], mana_to_charge[1], mana_to_charge[2], mana_to_charge[3], mana_to_charge[4], mana_to_charge[5]);
				if( spell_fizzled != 1 ){
					if( choice == 11 ){
						set_special_flags(player, card, SF_KICKED | SF_KICKED2);
					}
					else{
						int sf = (mana_to_charge[2] == 1) | (mana_to_charge[5] == 1) ? SF_KICKED : SF_KICKED2;
						set_special_flags(player, card, sf);
					}
				}
			}
		}
	}
}

// Cards

int card_allied_strategies(int player, int card, event_t event){
	/* Allied Strategies	|4|U
	 * Sorcery
	 * Domain - Target player draws a card for each basic land type among lands he or she controls. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card)){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier_for_targeted_card_drawing(player, card, 0, count_domain(instance->targets[0].player, 0));
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(instance->targets[0].player, count_domain(instance->targets[0].player, 0) );
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_arctic_merfolk(int player, int card, event_t event){
/* CARD_ID_ARCTIC_MERFOLK	5201
Arctic Merfolk	|1|U
Creature - Merfolk 1/1
Kicker-Return a creature you control to its owner's hand.
(You may return a creature you control to its owner's hand in addition to any other costs as you cast this spell.)
If ~ was kicked, it enters the battlefield with a +1/+1 counter on it. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags(player, card, SF_NOT_CAST) && ! is_token(player, card) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.allowed_controller = player;
			td.preferred_controller = player;
			td.illegal_abilities = 0;

			card_instance_t *instance = get_card_instance(player, card);

			if( can_target(&td) ){
				int choice = do_dialog(player, player, card, -1, -1, " Pay kicker\n Pass", 0);
				if( choice == 0 && new_pick_target(&td, "Select a creature you control.", 0, GS_LITERAL_PROMPT) ){
					bounce_permanent(instance->targets[0].player, instance->targets[0].card);
					set_special_flags(player, card, SF_KICKED);
				}
				instance->number_of_targets = 0;
			}
		}
	}

	if( kicked(player, card) ){
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 1);
	}

	return 0;
}

int card_bog_down(int player, int card, event_t event)
{
  /* Bog Down	|2|B
   * Sorcery
   * Kicker-Sacrifice two lands.
   * Target player discards two cards. If ~ was kicked, that player discards three cards instead. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if (event == EVENT_CAN_CAST)
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "");
	test.qty = 2;

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
		if( ! is_token(player, card) ){
			get_card_instance(player, card)->number_of_targets = 0;

			char marked[151];
			if (new_can_sacrifice_as_cost(player, card, &test)
				&& do_dialog(player, player, card, -1, -1, " Pay kicker\n Pass",
						   basiclandtypes_controlled[player][COLOR_ANY] >= 6 && hand_count[1-player] >= 3 ? 0 : 1) == 0)
			{
			  if (mark_sacrifice(player, card, player, SAC_JUST_MARK|SAC_ALL_OR_NONE|SAC_AS_COST, &test, marked) == 2)
				pick_target(&td, "TARGET_PLAYER");

			  int c;
			  for (c = 0; c < active_cards_count[player]; ++c)
				if (marked[c])
				  {
					get_card_instance(player, c)->state &= ~STATE_CANNOT_TARGET;
					if (cancel != 1)
					  kill_card(player, c, KILL_SACRIFICE);
				  }

			  if (cancel != 1)
				set_special_flags(player, card, SF_KICKED);
			}
			else{
				pick_target(&td, "TARGET_PLAYER");
			}
		}
		
	}

	if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		new_multidiscard(get_card_instance(player, card)->targets[0].player, kicked(player, card) ? 3 : 2, 0, player);

	  kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_cavern_harpy(int player, int card, event_t event){
/* CARD_ID_CAVERN_HARPY	5206
Cavern Harpy	|U|B
Creature - Harpy Beast 2/1
Flying
When ~ enters the battlefield, return a |Sblue or |Sblack creature you control to its owner's hand.
Pay 1 life: Return ~ to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gating(player, card, event, COLOR_TEST_BLUE, COLOR_TEST_BLACK);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		if( in_play(instance->parent_controller, instance->parent_card) ){
			bounce_permanent(instance->parent_controller, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, 1, NULL, NULL);
}

int card_crosis_catacombs(int player, int card, event_t event){
/* CARD_ID_CROSISS_CATACOMBS	5209
Crosis's Catacombs	""
Land - Lair
When ~ enters the battlefield, sacrifice it unless you return a non-Lair land you control to its owner's hand.
|T: Add |U, |B, or |R to your mana pool. */

	// also code for the others "Lair" lands

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.required_subtype = SUBTYPE_LAIR;
		td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
		td.illegal_abilities = 0;

		ai_modifier += (can_target(&td) ? 48 : -96);
		return 0; //Otherwise it will get modified by "mana_producer" too
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.required_subtype = SUBTYPE_LAIR;
		td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
		td.illegal_abilities = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if (select_target(player, card, &td, "Select a non-Lair land.", &(instance->targets[0]))){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			instance->number_of_targets = 0;
		}
		else {
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return mana_producer(player, card, event);
}

int card_crosis_charm(int player, int card, event_t event){
/*
Crosis's Charm |U|B|R
Instant
Choose one —
• Return target permanent to its owner's hand.
• Destroy target nonblack creature. It can't be regenerated.
• Destroy target artifact.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	target_definition_t td3;
	default_target_definition(player, card, &td3, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance( player, card);

	enum{
		CHOICE_BOUNCE = 1,
		CHOICE_KILL_CREATURE,
		CHOICE_KILL_ARTIFACT
	};

	if( event == EVENT_CAN_CAST && basic_spell(player, card, event) ){
		return ( can_target(&td) || can_target(&td2) || can_target(&td3) ) ? 1 : 0;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Bounce permanent", 		can_target(&td), 1,
								"Kill a nonblack creature", can_target(&td2), 5,
								"Kill an artifact",			can_target(&td3), 5);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		switch( instance->info_slot ){
				case CHOICE_BOUNCE:
					return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
				case CHOICE_KILL_CREATURE:
					return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td2,
										get_sleighted_color_text(player, card, "Select target non%s creature", COLOR_BLACK), 1, NULL);
				case CHOICE_KILL_ARTIFACT:
					return generic_spell(player, card, event, GS_CAN_TARGET, &td3, "TARGET_ARTIFACT", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int choice = instance->info_slot;
		if( choice == CHOICE_BOUNCE && validate_target(player, card, &td, 0) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		if( choice == CHOICE_KILL_CREATURE && validate_target(player, card, &td2, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		if( choice == CHOICE_KILL_ARTIFACT && validate_target(player, card, &td3, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_darigaaz_charm(int player, int card, event_t event){
	/* Darigaaz's Charm	|B|R|G	0x2008a54
	 * Instant
	 * Choose one -
	 * * Return target creature card from your graveyard to your hand.
	 * * ~ deals 3 damage to any target.
	 * * Target creature gets +3/+3 until end of turn. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.preferred_controller = player;

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

	enum{
		CHOICE_RAISE_DEAD = 1,
		CHOICE_DAMAGE,
		CHOICE_PUMP
	};

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
			return 1;
		}
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td1, NULL, 1, NULL) ){
			return 1;
		}
		if( generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test) ){
			return 1;
		}
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
								"Raise Dead", (count_graveyard_by_type(player, TYPE_CREATURE) > 0 && ! graveyard_has_shroud(player)), 6,
								"Lightning Bolt", can_target(&td) , 8,
								"Giant Growth", can_target(&td1), (current_phase == PHASE_AFTER_BLOCKING ? 10 : 5)
								);

			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			instance->info_slot = choice;
		}

		if( instance->info_slot == CHOICE_RAISE_DEAD ){
			int result = select_target_from_grave_source(player, card, player, 0, AI_MAX_VALUE, &this_test, 0);
			if( result == -1 ){
				spell_fizzled = 1;
			}
		}
		if( instance->info_slot == CHOICE_DAMAGE ){
			pick_target(&td, "TARGET_ANY");
		}
		if( instance->info_slot == CHOICE_PUMP ){
			pick_target(&td1, "TARGET_CREATURE");
		}
	}

	if(event == EVENT_RESOLVE_SPELL){
		if( instance->info_slot == CHOICE_RAISE_DEAD ){
			int result = validate_target_from_grave_source(player, card, player, 0);
			if( result > -1 ){
				from_grave_to_hand(player, result, TUTOR_HAND);
			}
		}
		if( instance->info_slot == CHOICE_DAMAGE && validate_target(player, card, &td, 0) ){
			damage_target0(player, card, 3);
		}
		if( instance->info_slot == CHOICE_PUMP && validate_target(player, card, &td1, 0) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 3, 3);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_dark_suspicions(int player, int card, event_t event){
/* CARD_ID_DARK_SUSPICIONS	5214
Dark Suspicions	|2|B|B
Enchantment
At the beginning of each opponent's upkeep, that player loses X life, where X is the number of cards in that player's hand minus the number of cards in your hand.*/

	upkeep_trigger_ability(player, card, event, 1-player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int amount = hand_count[1-player]-hand_count[player];
		if( amount > 0 ){
			lose_life(1-player, amount);
		}
	}

	return global_enchantment(player, card, event);
}

int card_deadapult(int player, int card, event_t event){
	/* Deadapult	|2|R	0x2008a5e
	 * Enchantment
	 * |R, Sacrifice a Zombie: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td.allow_cancel = 0;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Zombie to sacrifice.");
	test.subtype = SUBTYPE_ZOMBIE;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST_R(1), 0,
															&td, "TARGET_ANY", 0, &test, 0);
}

int card_destructive_flow(int player, int card, event_t event){
/* CARD_ID_DESTRUCTIVE_FLOW	5217
Destructive Flow	|B|R|G
Enchantment
At the beginning of each player's upkeep, that player sacrifices a nonbasic land. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a nonbasic land to sacrifice.");
		this_test.subtype = SUBTYPE_BASIC;
		this_test.subtype_flag = 1;
		new_sacrifice(player, card, current_turn, SAC_CAUSED | SAC_NO_CANCEL, &this_test);
	}

	return global_enchantment(player, card, event);
}

int card_diabolic_intent(int player, int card, event_t event){
/* CARD_ID_DIABOLIC_INTENT	5218
Diabolic Intent	|1|B
Sorcery
As an additional cost to cast ~, sacrifice a creature.
Search your library for a card and put that card into your hand. Then shuffle your library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_CREATURE, -1)*12);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		char msg[100] = "Select a card to tutor";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, msg);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_SAC_CREATURE_AS_COST, NULL, NULL, 0, NULL);
}

int card_doomsday_specter(int player, int card, event_t event){
/* CARD_ID_DOOMSDAY_SPECTER	5221
Doomsday Specter	|2|U|B
Creature - Specter 2/3
Flying
When ~ enters the battlefield, return a |Sblue or |Sblack creature you control to its owner's hand.
Whenever ~ deals combat damage to a player, look at that player's hand and choose a card from it. The player discards that card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gating(player, card, event, COLOR_TEST_BLUE, COLOR_TEST_BLACK);
	}

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					while( times_damaged[p] ){
							ec_definition_t ec;
							default_ec_definition(p, player, &ec);

							test_definition_t this_test;
							default_test_definition(&this_test, TYPE_ANY);
							new_effect_coercion(&ec, &this_test);
							times_damaged[p]--;
					}
				}
		);
	}

	return 0;
}

int card_draco(int player, int card, event_t event){
/* CARD_ID_DRACO	5222
Draco	|16
Artifact Creature - Dragon 9/9
Domain - ~ costs |2 less to cast for each basic land type among lands you control.
Flying
Domain - At the beginning of your upkeep, sacrifice ~ unless you pay |10. This cost is reduced by |2 for each basic land type among lands you control.*/

	basic_upkeep(player, card, event, MANACOST_X(10-2*count_domain(player, card)));
	if(event == EVENT_MODIFY_COST ){
		COST_COLORLESS -= 2*count_domain(player, card);
	}
	return 0;
}

int card_dralnus_crusade(int player, int card, event_t event){
/*
Dralnu's Crusade |1|B|R
Enchantment
Goblin creatures get +1/+1.
All Goblins are black and are Zombies in addition to their other creature types.
*/
	boost_creature_type(player, card, event, SUBTYPE_GOBLIN, 1, 1, 0, BCT_INCLUDE_SELF);

	if( event == EVENT_SET_COLOR && in_play(player, card) && ! is_humiliated(player, card) &&
		has_subtype(affected_card_controller, affected_card, SUBTYPE_GOBLIN) )
	{
		event_result = COLOR_TEST_BLACK;
	}

	return global_enchantment(player, card, event);
}

int card_dralnus_pet(int player, int card, event_t event){
/* CARD_ID_DRALNUS_PET	5224
Dralnu's Pet	|1|U|U
Creature - Shapeshifter 2/2
Kicker-|2|B, Discard a creature card. (You may pay |2|B and discard a creature card in addition to any other costs as you cast this spell.)
If ~ was kicked, it enters the battlefield with flying and with X +1/+1 counters on it, where X is the discarded card's converted mana cost.*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags(player, card, SF_NOT_CAST) && ! is_token(player, card) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card to discard.");
			this_test.zone = TARGET_ZONE_HAND;

			if( check_battlefield_for_special_card(player, card, player, 0, &this_test) && has_mana_multi(player, MANACOST_XU(2, 1)) ){
				int choice = do_dialog(player, player, card, -1, -1, " Pay kicker\n Pass", 0);
				if( choice == 0 ){
					charge_mana_multi(player, MANACOST_XU(2, 1));
					if( spell_fizzled != 1 ){
						int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MAX_CMC, -1, &this_test);
						get_card_instance(player, card)->info_slot = get_cmc(player, selected);
						discard_card(player, selected);
						set_special_flags(player, card, SF_KICKED);
					}
				}
			}
		}
	}

	if( kicked(player, card) ){
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, get_card_instance(player, card)->info_slot);
	}

	if( event == EVENT_ABILITIES && affect_me(player, card) && kicked(player, card) && ! is_humiliated(player, card) ){
		event_result |= KEYWORD_FLYING;
	}

	return 0;
}

int card_dromar_charm(int player, int card, event_t event){
/* CARD_ID_DROMARS_CHARM	5226
Dromar's Charm	|W|U|B
Instant
Choose one -
* You gain 5 life.
* Counter target spell.
* Target creature gets -2/-2 until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card);

	enum{
		CHOICE_GAIN_LIFE = 1,
		CHOICE_COUNTERSPELL,
		CHOICE_M2_M2
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL) ){
			return 99;
		}
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int can_counter = counterspell(player, card, EVENT_CAN_CAST, NULL, 0);
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Gain 5 life", 1, life[player] < 6 ? 25 : 1,
								"Counterspell", can_counter, 15,
								"Target crit gets -5/-5", can_target(&td), 5);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_COUNTERSPELL ){
			return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
		}
		if( instance->info_slot == CHOICE_M2_M2 ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
	}

	if(event == EVENT_RESOLVE_SPELL){
		if( instance->info_slot == CHOICE_GAIN_LIFE ){
			gain_life(player, 5);
		}
		if( instance->info_slot == CHOICE_COUNTERSPELL ){
			return counterspell(player, card, event, NULL, 0);
		}
		if( instance->info_slot == CHOICE_M2_M2 && validate_target(player, card, &td, 0) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -2, -2);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_eladamris_call(int player, int card, event_t event){
	/*
	  Eladamri's Call |W|G
	  Instant
	  Search your library for a creature card, reveal that card, and put it into your hand. Then shuffle your library.
	*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn == 1-player ? 96 : 24);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_ertai_the_corrupted(int player, int card, event_t event){
/*
Ertai, the Corrupted |2|W|U|B
Legendary Creature — Human Wizard 3/4
{U}, {T}, Sacrifice a creature or enchantment: Counter target spell.
*/
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE | TYPE_ENCHANTMENT, "Select a creature or enchantment to sacrifice.");

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( counterspell_validate(player, card, &td, 0) ){
			real_counter_a_spell(instance->parent_controller, instance->parent_card,
								instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_SPELL_ON_STACK, MANACOST_U(1), 0, &td, NULL,
															0, &test, 0);
}

int card_flametongue_kavu(int player, int card, event_t event){
/* CARD_ID_FLAMETONGUE_KAVU	5232
Flametongue Kavu	|3|R
Creature - Kavu 4/2
When ~ enters the battlefield, it deals 4 damage to target creature. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			damage_target0(player, card, 4);
		}
		get_card_instance(player, card)->number_of_targets = 0;
	}

	return 0;
}

int card_fleetfoot_panther(int player, int card, event_t event){
/* CARD_ID_FLEETFOOT_PANTHER	5233
Fleetfoot Panther	|1|G|W
Creature - Cat 3/4
Flash
When ~ enters the battlefield, return a |Sgreen or |Swhite creature you control to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gating(player, card, event, COLOR_TEST_GREEN, COLOR_TEST_WHITE);
	}

	return flash(player, card, event);
}

int card_gaeas_herald(int player, int card, event_t event){
/* CARD_ID_GAEAS_HERALD	1043
Gaea's Herald	|1|G
Creature - Elf 1/1
Creature spells can't be countered. */

	type_uncounterable(player, card, event, ANYBODY, TYPE_CREATURE, NULL);

	return 0;
}

int card_gerrards_command(int player, int card, event_t event){
/* CARD_ID_GERRARDS_COMMAND	5237
Gerrard's Command	|G|W
Instant
Untap target creature. It gets +3/+3 until end of turn. */

	if( event == EVENT_CHECK_PUMP ){
		return vanilla_instant_pump(player, card, event, ANYBODY, player, 3, 3, 0, 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			untap_card(instance->targets[0].player, instance->targets[0].card);
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 3, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_horned_kavu(int player, int card, event_t event){
/* CARD_ID_HORNED_KAVU	5243
Horned Kavu	|R|G
Creature - Kavu 3/4
When ~ enters the battlefield, return a |Sred or |Sgreen creature you control to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gating(player, card, event, COLOR_TEST_RED, COLOR_TEST_GREEN);
	}
	return 0;
}

int card_hull_breach(int player, int card, event_t event){
/* CARD_ID_HULL_BREACH	5244
Hull Breach	|R|G
Sorcery
Choose one -
* Destroy target artifact.
* Destroy target enchantment.
* Destroy target artifact and target enchantment. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_KILL_ARTIFACT = 1,
		CHOICE_KILL_ENCHANTMENT,
		CHOICE_DO_BOTH
	};

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return can_target(&td) || can_target(&td2) ? 1 : 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int can_do_both = 0;
			td.illegal_type = TYPE_ENCHANTMENT;
			td2.illegal_type = TYPE_ARTIFACT;
			if( can_target(&td) && can_target(&td2) ){
				can_do_both = 1;
			}
			td.illegal_type = td2.illegal_type = 0;
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Kill an artifact", can_target(&td), 1,
								"Kill an enchantment", can_target(&td2), 1,
								"Do both", can_do_both, 10);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		int base_target = 0;
		if( instance->info_slot & CHOICE_KILL_ARTIFACT ){
			new_pick_target(&td, "TARGET_ARTIFACT", 0, 1);
			if( spell_fizzled == 1 ){
				return 0;
			}
			base_target++;
		}
		if( instance->info_slot & CHOICE_KILL_ENCHANTMENT ){
			new_pick_target(&td2, "TARGET_ENCHANTMENT", base_target, 1);
		}
	}


	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_DO_BOTH ){
			if( validate_target(player, card, &td, 0) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
			if( validate_target(player, card, &td2, 1) ){
				kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
			}
		}
		if( instance->info_slot == CHOICE_KILL_ARTIFACT ){
			if( validate_target(player, card, &td, 0) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
		}
		if( instance->info_slot == CHOICE_KILL_ENCHANTMENT ){
			if( validate_target(player, card, &td2, 0) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_lashknife_barrier(int player, int card, event_t event){
/* CARD_ID_LASHKNIFE_BARRIER	5251
Lashknife Barrier	|2|W
Enchantment
When ~ enters the battlefield, draw a card.
If a source would deal damage to a creature you control, it deals that much damage minus 1 to that creature instead. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card != -1 && damage->damage_target_player == player &&
				damage->info_slot > 0 )
			{
				prevent_damage(affected_card_controller, affected_card, 1);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_lava_zombie(int player, int card, event_t event){
/* CARD_ID_LAVA_ZOMBIE	5252
Lava Zombie	|1|B|R
Creature - Zombie 4/3
When ~ enters the battlefield, return a |Sblack or |Sred creature you control to its owner's hand.
|2: ~ gets +1/+0 until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gating(player, card, event, COLOR_TEST_BLACK, COLOR_TEST_RED);
	}
	return generic_shade(player, card, event, 0, MANACOST_X(2), 1, 0, 0, 0);
}

int card_lord_of_the_undead(int player, int card, event_t event){
	/* Lord of the Undead	|1|B|B
	 * Creature - Zombie 2/2
	 * Other Zombie creatures get +1/+1.
	 * |1|B, |T: Return target Zombie card from your graveyard to your hand. */

	if( ! IS_GAA_EVENT(event) ){
		return boost_creature_type(player, card, event, SUBTYPE_ZOMBIE, 1, 1, 0, 0);
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select target Zombie card.");
	this_test.subtype = SUBTYPE_ZOMBIE;

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (current_turn == 1-player && current_phase >= PHASE_MAIN2 ? 48 : 12);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected > -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST_XB(1, 1), 0, NULL, NULL,
															0, &this_test, GS_GRAVE_RECYCLER);
}

int card_maggot_carrier(int player, int card, event_t event){
	/*
	  Maggot Carrier |B
	  Creature - Zombie 1/1
	  When Maggot Carrier enters the battlefield, each player loses 1 life.
	*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		lose_life(player, 1);
		lose_life(1-player, 1);
	}

	return 0;
}

int card_malicious_advice(int player, int card, event_t event){
/*
Malicious Advice |X|U|B
Instant
Tap X target artifacts, creatures, and/or lands. You lose X life.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int diff = true_get_updated_casting_cost(player, card, -1, event, -1);
		int max = has_mana(player, COLOR_ANY, 1)-diff;
		if( check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			max = instance->info_slot;
		}
		int trgs = 0;
		while( can_target(&td) && trgs < max ){
				if( new_pick_target(&td, "TARGET_ARTIFACT_CREATURE_LAND", trgs, 0) ){
					state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
					trgs++;
					if( IS_AI(player) && life[player]-trgs < 6 ){
						break;
					}
				}
				else{
					break;
				}
		}
		int i;
		for(i=0; i<trgs; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
		}
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			charge_mana(player, COLOR_ANY, trgs + diff);
			if( spell_fizzled != 1 ){
				instance->info_slot = trgs;
			}
		}
	}


	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		int good = 0;
		for(i=0; i<instance->info_slot; i++){
			if( validate_target(player, card, &td, i) ){
				tap_card(instance->targets[i].player, instance->targets[i].card);
				good++;
			}
		}
		if( good ){
			lose_life(player, instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_march_of_souls(int player, int card, event_t event){
/* March of Souls	|4|W	0x200f539
 * Sorcery
 * Destroy all creatures. They can't be regenerated. For each creature destroyed this way, its controller puts a 1/1 |Swhite Spirit creature token with flying onto the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		int killed[2] = {0};
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		APNAP(p,{ killed[p] = new_manipulate_all(player, card, p, &this_test, KILL_BURY); };);

		APNAP(p,{
					token_generation_t token;
					default_token_definition(player, card, CARD_ID_SPIRIT, &token);
					token.t_player = p;
					token.qty = killed[p];
					token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
					token.key_plus = KEYWORD_FLYING;
					generate_token(&token);
				}
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_marsh_crocodile(int player, int card, event_t event){
/* CARD_ID_MARSH_CROCODILE	5258
Marsh Crocodile	|2|U|B
Creature - Crocodile 4/4
When ~ enters the battlefield, return a |Sblue or |Sblack creature you control to its owner's hand.
When ~ enters the battlefield, each player discards a card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( hand_count[player] > 0 ){
		    gating(player, card, event, COLOR_TEST_BLUE, COLOR_TEST_BLACK);
		    APNAP(p, {discard(p, 0, player);};);
		}
		else{
		     APNAP(p, {discard(p, 0, player);};);
			 gating(player, card, event, COLOR_TEST_BLUE, COLOR_TEST_BLACK);
		}
	}
	return 0;
}

void meddling_mage_effect(int player, int card, event_t event){
	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card);
		int csvid = -1;

		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a nonland card.");
		test.type_flag = DOESNT_MATCH;
		if( IS_AI(player) ){
			csvid = ai_name_a_random_csvid(player, card, 1-player, 0, &test);
		}
		else{
			csvid = new_card_from_list(player, 3, &test);
		}
		if( csvid > -1 ){
			instance->targets[1].card = csvid;
			create_card_name_legacy(player, card, instance->targets[1].card);
		}
	}

	if( event == EVENT_MODIFY_COST_GLOBAL ){
		if( get_id(affected_card_controller, affected_card) == get_card_instance( player, card)->targets[1].card ){
			infinite_casting_cost();
		}
	}
}

int card_meddling_mage(int player, int card, event_t event){
/* CARD_ID_MEDDLING_MAGE	5259
Meddling Mage	|W|U
Creature - Human Wizard 2/2
As ~ enters the battlefield, choose a nonland card name.
Spells with the chosen name can't be cast. */

	meddling_mage_effect(player, card, event);
	return 0;
}

int card_mogg_sentry(int player, int card, event_t event){
	/*
	  Mogg Sentry |R
	  Creature - Goblin Warrior 1/1
	  Whenever an opponent casts a spell, Mogg Sentry gets +2/+2 until end of turn.
	*/
	if( new_specific_spell_played(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		pump_until_eot_merge_previous(player, card, player, card, 2, 2);
	}

	return 0;
}

int card_nemata_grove_guardian(int player, int card, event_t event){
	/* Nemata, Grove Guardian	|4|G|G
	 * Legendary Creature - Treefolk 4/5
	 * |2|G: Put a 1/1 |Sgreen Saproling creature token onto the battlefield.
	 * Sacrifice a Saproling: Saproling creatures get +1/+1 until end of turn. */

	check_legend_rule(player, card, event);

	if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION){
		if (event == EVENT_CAN_ACTIVATE && !can_use_activated_abilities(player, card)){
			return 0;
		}

		int can_generate = has_mana_for_activated_ability(player, card, MANACOST_XG(2, 1));
		int ai_priority_generate = 2;
		int can_sacr = (has_mana_for_activated_ability(player, card, MANACOST_X(0))
						&& can_sacrifice_as_cost(player, 1, TYPE_PERMANENT, 0, SUBTYPE_SAPROLING, 0, 0, 0, 0, 0, -1, 0));
		int ai_priority_sacr = count_subtype(player, TYPE_CREATURE, SUBTYPE_SAPROLING) > 1 ? 1 : -1;

		int choice = DIALOG(player, card, event,
							"Generate a Saproling", can_generate, ai_priority_generate,
							"Sacrifice a Saproling", can_sacr, ai_priority_sacr);

		if (event == EVENT_CAN_ACTIVATE){
			return choice;
		} else if (event == EVENT_ACTIVATE){
			if (choice == 1){
				charge_mana_for_activated_ability(player, card, MANACOST_XG(2, 1));
			} else if (choice == 2 && charge_mana_for_activated_ability(player, card, MANACOST_X(0))){
				if (current_phase != PHASE_DECLARE_BLOCKERS){
					/* Discourage sacrifice in phases other than declare blockers, but do it via ai_modifier instead of dialog priority so it can still
					 * sacrifice in response to either the sacrificed saproling being target for destruction, or a different one being damaged */
					ai_modifier -= 24;
				}
				if (!sacrifice(player, card, player, 0, TYPE_PERMANENT, 0, SUBTYPE_SAPROLING, 0, 0, 0, 0, 0, -1, 0)){
					cancel = 1;
				}
			}
		} else {	// EVENT_RESOLVE_ACTIVATION
			if (choice == 1){
				generate_token_by_id(player, card, CARD_ID_SAPROLING);
			} else {
				pump_subtype_until_eot(player, card, 2, SUBTYPE_SAPROLING, 1, 1, 0, 0);
			}
		}
	}

	return 0;
}

int card_nightscape_battlemage(int player, int card, event_t event){
	/*
	  Nightscape Battlemage |2|B
	  Creature - Zombie Wizard 2/2
	  Kicker {2}{U} and/or {2}{R} (You may pay an additional {2}{U} and/or {2}{R} as you cast this spell.)
	  When Nightscape Battlemage enters the battlefield, if it was kicked with its {2}{U} kicker, return up to two target nonblack creatures to their owners' hands.
	  When Nightscape Battlemage enters the battlefield, if it was kicked with its {2}{R} kicker, destroy target land.
	*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.illegal_color = COLOR_TEST_BLACK;

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_LAND);
		td1.allow_cancel = 0;

		volver1(player, card, event, MANACOST_XU(2, 1), MANACOST_XR(2, 1), 5*target_available(player, card, &td), 5*can_target(&td1));
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		if( check_special_flags(player, card, SF_KICKED) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

			int trgs = 0;
			char msg[100];
			scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select target non%s creature", COLOR_BLACK));
			while( can_target(&td) && trgs < 2 ){
					if( new_pick_target(&td, msg, trgs, GS_LITERAL_PROMPT) ){
						state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
						trgs++;
					}
					else{
						break;
					}
			}
			int i;
			for(i=0; i<instance->number_of_targets; i++){
				bounce_permanent(instance->targets[i].player, instance->targets[i].card);
			}
			instance->number_of_targets = 0;
		}
		if( check_special_flags(player, card, SF_KICKED2) ){
			target_definition_t td1;
			default_target_definition(player, card, &td1, TYPE_LAND);
			td1.allow_cancel = 0;

			if( can_target(&td1) && pick_target(&td1, "TARGET_LAND") ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_nightscape_familiar(int player, int card, event_t event)
{
  /* Nightscape Familiar	|1|B
   * Creature - Zombie 1/1
   * |SBlue spells and |Sred spells you cast cost |1 less to cast.
   * |1|B: Regenerate ~. */

  familiar(player, card, event, COLOR_TEST_BLUE | COLOR_TEST_RED);
  return regeneration(player, card, event, MANACOST_XB(1,1));
}

int card_orims_chant(int player, int card, event_t event){
	/*
	  Orim's Chant |W
	  Instant
	  Kicker {W} (You may pay an additional {W} as you cast this spell.)
	  Target player can't cast spells this turn.
	  If Orim's Chant was kicked, creatures can't attack this turn.
	*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
		if( spell_fizzled != 1 ){
			if( ! is_token(player, card) ){
				do_kicker(player, card, MANACOST_W(1));
			}
		}
	}


	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int se_card = target_player_cant_cast_type(player, card, instance->targets[0].player, TYPE_CREATURE|TYPE_ENCHANTMENT|TYPE_ARTIFACT|
			                                           TARGET_TYPE_PLANESWALKER|TYPE_SPELL|TYPE_EFFECT);
			if( kicked(player, card) && se_card > -1 ){
				add_flag_to_special_effect(player, se_card, SE_CREATURES_CANT_ATTACK);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_phyrexian_scuta(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_SCUTA	5273
Phyrexian Scuta	|3|B
Creature - Zombie 3/3
Kicker-Pay 3 life. (You may pay 3 life in addition to any other costs as you cast this spell.)
If ~ was kicked, it enters the battlefield with two +1/+1 counters on it. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags(player, card, SF_NOT_CAST) && ! is_token(player, card) ){
			int ai_choice = 0;
			if( (life[player]-3) < 6 ){
				ai_choice = 1;
			}
			if( can_pay_life(player, 3) ){
				int choice = do_dialog(player, player, card, -1, -1," Pay kicker\n Pass\n", ai_choice);
				if( choice == 0 ){
					lose_life(player, 3);
					set_special_flags(player, card, SF_KICKED);
				}
			}
		}
	}

	if( kicked(player, card) ){
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);
	}

	return 0;
}

int card_phyrexian_tyranny(int player, int card, event_t event){
	/* Phyrexian Tyranny	|U|B|R
	 * Enchantment
	 * Whenever a player draws a card, that player loses 2 life unless he or she pays |2. */

	if (card_drawn_trigger(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY)){
		int p = trigger_cause_controller;
		if (!charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, p, COLOR_COLORLESS, 2)){
			lose_life(p, 2);
		}
	}

	return global_enchantment(player, card, event);
}

int card_primal_growth(int player, int card, event_t event){
	/*
	  Primal Growth |2|G
	  Sorcery, 2G
	  Kicker — Sacrifice a creature. (You may sacrifice a creature in addition to any other costs as you cast this spell.)
	  Search your library for a basic land card, put that card onto the battlefield, then shuffle your library.
	  If Primal Growth was kicked, instead search your library for up to two basic land cards, put them onto the battlefield,
	  then shuffle your library.
	*/
	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE) ){
			int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_NO_CANCEL,
								"Pay Kicker", 1, (count_subtype(player, TYPE_CREATURE, -1) > 3 ? 10 : 0),
								"Decline", 1, 5);
			if( choice == 1 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice");
				int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
				if( sac ){
					kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
					set_special_flags(player, card, SF_KICKED);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		tutor_basic_lands(player, TUTOR_PLAY, (check_special_flags(player, card, SF_KICKED) ? 2 : 1));
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_questing_phelddagrif(int player, int card, event_t event){
	/* Questing Phelddagrif	|1|G|W|U
	 * Creature - Phelddagrif 4/4
	 * |G: ~ gets +1/+1 until end of turn. Target opponent puts a 1/1 |Sgreen Hippo creature token onto the battlefield.
	 * |W: ~ gains protection from |Sblack and from |Sred until end of turn. Target opponent gains 2 life.
	 * |U: ~ gains flying until end of turn. Target opponent may draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_P1_P1 = 1,
		CHOICE_PROT,
		CHOICE_FLYING
	};

	if( event == EVENT_CAN_ACTIVATE && would_validate_arbitrary_target(&td, 1-player, -1) ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL) ||
			generic_activated_ability(player, card, event, 0, MANACOST_W(1), 0, NULL, NULL) ||
			generic_activated_ability(player, card, event, 0, MANACOST_U(1), 0, NULL, NULL) )
		{
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		int abils[4] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_G(1), 0, NULL, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_W(1), 0, NULL, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_U(1), 0, NULL, NULL)
		};
		int current_abilities = get_abilities(player, card, EVENT_ABILITIES, -1);
		if( instance->targets[1].card > -1 ){
			current_abilities |= instance->targets[1].card;
		}
		int priorities[4] = {
							0,
							current_phase == PHASE_AFTER_BLOCKING ? 10 : 1,
							5,
							current_phase < PHASE_BEFORE_BLOCKING && !(current_abilities & (KEYWORD_PROT_BLACK | KEYWORD_PROT_RED)) ? 10 : 1
		};
		char msg[100];
		snprintf(msg, 100, "%s", get_sleighted_color_text2(player, card, "Gains Protec. %s or %s", COLOR_BLACK, COLOR_RED));
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Gets +1/+1", 	abils[1], priorities[1],
							msg, 			abils[2], priorities[2],
							"Gain flying", 	abils[3], priorities[3]);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_P1_P1 ){
			return generic_activated_ability(player, card, event, GAA_CAN_ONLY_TARGET_OPPONENT, MANACOST_G(1), 0, &td, NULL);
		}
		if( choice == CHOICE_PROT ){
			return generic_activated_ability(player, card, event, GAA_CAN_ONLY_TARGET_OPPONENT, MANACOST_W(1), 0, &td, NULL);
		}
		if( choice == CHOICE_FLYING ){
			return generic_activated_ability(player, card, event, GAA_CAN_ONLY_TARGET_OPPONENT, MANACOST_U(1), 0, &td, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			if( instance->info_slot == CHOICE_P1_P1 ){
				generic_shade_merge_pt(player, card, event, 0, MANACOST0, 1, 1);
				token_generation_t token;
				default_token_definition(player, card, CARD_ID_HIPPO, &token);
				token.t_player = instance->targets[0].player;
				generate_token(&token);
			}
			if( instance->info_slot == CHOICE_PROT ){
			    alternate_legacy_text(2, player, pump_ability_until_eot(player, card, instance->parent_controller, instance->parent_card, 0, 0,
				                                                         get_sleighted_protection(player, card, KEYWORD_PROT_BLACK|KEYWORD_PROT_RED), 0));
			    gain_life(instance->targets[0].player, 2);
			}
			if( instance->info_slot == CHOICE_FLYING ){
				alternate_legacy_text(3, player, pump_ability_until_eot(player, card, instance->parent_controller, instance->parent_card, 0, 0,
				                                                            KEYWORD_FLYING, 0));
				draw_some_cards_if_you_want(instance->parent_controller, instance->parent_card, instance->targets[0].player, 1);
			}
		}
	}

	return 0;
}

int card_quirion_dryad(int player, int card, event_t event){
/* CARD_ID_QUIRION_DRYAD	5285
Quirion Dryad	|1|G
Creature - Dryad 1/1
Whenever you cast a |Swhite, |Sblue, |Sblack, or |Sred spell, put a +1/+1 counter on ~. */

	if( ! is_unlocked(player, card, event, 4) ){ return 0; }

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK | COLOR_TEST_BLUE | COLOR_TEST_RED | COLOR_TEST_WHITE);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			add_1_1_counter(player, card);
		}
	}

	return 0;
}

  /* Quirion Explorer = card_sylvok_explorer 2005abb
CARD_ID_QUIRION_EXPLORER	5286
Quirion Explorer	|1|G
Creature - Elf Druid Scout 1/1
|T: Add to your mana pool one mana of any color that a land an opponent controls could produce. */

int card_rith_charm(int player, int card, event_t event){
	/* Rith's Charm	|R|G|W
	 * Instant
	 * Choose one -
	 * * Destroy target nonbasic land.
	 * * Put three 1/1 |Sgreen Saproling creature tokens onto the battlefield.
	 * * Prevent all damage a source of your choice would deal this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = SUBTYPE_NONBASIC;

	target_definition_t td2;
	default_target_definition(player, card, &td2, 0);
	td2.extra = damage_card;

	card_instance_t *instance = get_card_instance( player, card);

	enum{
		CHOICE_KILL_LAND = 1,
		CHOICE_SAPROLINGS,
		CHOICE_PREVENT
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_DAMAGE_PREVENTION | GS_CAN_TARGET, &td2, NULL, 1, NULL) ){
			return 99;
		}
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {
							0,
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td, NULL, 1, NULL),
							basic_spell(player, card, EVENT_CAN_CAST),
							generic_spell(player, card, EVENT_CAN_CAST, GS_DAMAGE_PREVENTION | GS_CAN_TARGET, &td2, NULL, 1, NULL)
			};
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Destroy target nonbasic land", abils[1], 5,
								"3 1/1 Saprolings", abils[2], (10-(count_subtype(player, TYPE_CREATURE, -1)*3)),
								"Prevent damage", abils[3], 25);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_KILL_LAND ){
			return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nonbasic land.", 1, NULL);
		}
		if( instance->info_slot == CHOICE_SAPROLINGS ){
			return basic_spell(player, card, event);
		}
		if( instance->info_slot == CHOICE_PREVENT ){
			generic_spell(player, card, event, GS_DAMAGE_PREVENTION | GS_CAN_TARGET, &td2, "TARGET_DAMAGE", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_KILL_LAND && valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( instance->info_slot == CHOICE_SAPROLINGS ){
			generate_tokens_by_id(player, card, CARD_ID_SAPROLING, 3);
		}
		if( instance->info_slot == CHOICE_PREVENT && valid_target(&td2) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, -1);
			card_instance_t *dmg = get_card_instance( instance->targets[0].player, instance->targets[0].card );
			prevent_damage_until_eot(player, card, dmg->damage_source_player, dmg->damage_source_card, -1);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_root_greevil(int player, int card, event_t event){
/* CARD_ID_ROOT_GREEVIL	5291
Root Greevil	|3|G
Creature - Beast 2/3
|2|G, |T, Sacrifice ~: Destroy all enchantments of the color of your choice. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int clr = 1<<choose_a_color(player, get_deck_color(player, 1-player));
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ENCHANTMENT);
		this_test.color = clr;
		new_manipulate_all(player, card, 2, &this_test, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_XG(2, 1), 0, NULL, NULL);
}

int card_rushing_river(int player, int card, event_t event){

	/* Rushing River	|2|U
	 * Instant
	 * Kicker-Sacrifice a land.
	 * Return target nonland permanent to its owner's hand. If ~ was kicked, return another target nonland permanent to its owner's hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.illegal_type = TYPE_LAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int sac = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) && can_sacrifice_type_as_cost(player, 1, TYPE_LAND) &&
			target_available(player, card, &td) >= 2 &&
			do_dialog(player, player, card, -1, -1, " Pay kicker\n Decline", basiclandtypes_controlled[player][COLOR_ANY] >= 5 ? 0 : 1) == 0 )
		{
			sac = controller_sacrifices_a_permanent(player, card, TYPE_LAND, SAC_JUST_MARK|SAC_RETURN_CHOICE|SAC_AS_COST);
			if (!sac){
				spell_fizzled = 1;
				return 0;
			}
			state_untargettable(BYTE2(sac), BYTE3(sac), 1);
		}

		if( pick_target(&td, "TARGET_NONLAND_PERMANENT") ){
			if( sac ){
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
				new_pick_target(&td, "TARGET_NONLAND_PERMANENT", 1, 1);
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
				if( spell_fizzled != 1 ){
					set_special_flags(player, card, SF_KICKED);
					kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
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

int card_sawtooth_loon(int player, int card, event_t event){
/* CARD_ID_SAWTOOTH_LOON	5295
Sawtooth Loon	|2|W|U
Creature - Bird 2/2
Flying
When ~ enters the battlefield, return a |Swhite or |Sblue creature you control to its owner's hand.
When ~ enters the battlefield, draw two cards, then put two cards from your hand on the bottom of your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gating(player, card, event, COLOR_TEST_WHITE, COLOR_TEST_BLUE);
		draw_cards(player, 2);
		int amount = 2;
		if( amount > hand_count[player] ){
			amount = hand_count[player];
		}
		char msg[100] = "Select a card to put on bottom.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, msg);
		while( amount > 0 ){
				int result = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &this_test);
				put_on_bottom_of_deck(player, result);
				amount--;
		}
	}
	return 0;
}

int card_shifting_sky(int player, int card, event_t event){
	/*
	  Shifting Sky |2|U
	  Enchantment
	  As Shifting Sky enters the battlefield, choose a color.
	  All nonland permanents are the chosen color.
	*/
	if (event == EVENT_RESOLVE_SPELL){
		get_card_instance(player, card)->targets[1].card = 1<<choose_a_color_and_show_legacy(player, card, 1-player, -1);
	}

	if (event == EVENT_SET_COLOR && ! is_humiliated(player, card) ){
		int col = get_card_instance(player, card)->targets[1].card;
		if (col > 0 && is_what(affected_card_controller, affected_card, TYPE_PERMANENT) &&
			! is_what(affected_card_controller, affected_card, TYPE_LAND) )
		{
			event_result = col;
		}
	}

	return global_enchantment(player, card, event);
}

int card_shivan_wurm(int player, int card, event_t event){
/* CARD_ID_SHIVAN_WURM	5297
Shivan Wurm	|3|R|G
Creature - Wurm 7/7
Trample
When ~ enters the battlefield, return a |Sred or |Sgreen creature you control to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gating(player, card, event, COLOR_TEST_RED, COLOR_TEST_GREEN);
	}
	return 0;
}

int card_silver_drake(int player, int card, event_t event){
/* CARD_ID_SILVER_DRAKE	5299
Silver Drake	|1|W|U
Creature - Drake 3/3
Flying
When ~ enters the battlefield, return a |Swhite or |Sblue creature you control to its owner's hand. */

// 0x200f53e
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gating(player, card, event, COLOR_TEST_WHITE, COLOR_TEST_BLUE);
	}
	return 0;
}

int card_sparkcaster(int player, int card, event_t event){
	/* Sparkcaster	|2|R|G	0x200213b
	 * Creature - Kavu 5/3
	 * When ~ enters the battlefield, return a |Sred or |Sgreen creature you control to its owner's hand.
	 * When ~ enters the battlefield, it deals 1 damage to target player or planeswalker. */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TARGET_TYPE_PLANESWALKER);
		td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td1.allow_cancel = 0;
		if( can_target(&td1) && new_pick_target(&td1, "TARGET_PLAYER_OR_PLANESWALKER", 0, 0) ){
			damage_target0(player, card, 1);
		}
		get_card_instance(player, card)->number_of_targets = 0;
		gating(player, card, event, COLOR_TEST_RED, COLOR_TEST_GREEN);
	}
	return 0;
}

int card_star_compass(int player, int card, event_t event){
	/*
	  Star Compass |2
	  Artifact
	  Star Compass enters the battlefield tapped.
	  {T}: Add to your mana pool one mana of any color that a basic land you control could produce.
	*/
	comes_into_play_tapped(player, card, event);
	return card_fellwar_stone(player, card, event);
}

int card_stormscape_battlemage(int player, int card, event_t event){
	/*
	  Stormscape Battlemage |2|U
	  Creature - Metathran Wizard 2/2
	  Kicker {W} and/or {2}{B}
	  When Stormscape Battlemage enters the battlefield, if it was kicked with its {W} kicker, you gain 3 life.
	  When Stormscape Battlemage enters the battlefield, if it was kicked with its {2}{B} kicker, destroy target nonblack creature. That creature can't be regenerated.
	*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.illegal_color = COLOR_TEST_BLACK;
		td.allow_cancel = 0;

		volver(player, card, event, MANACOST_XB(2, 1), MANACOST_W(1), 10*can_target(&td), 5 + life[player] < 6 ? 10 : 0);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( check_special_flags(player, card, SF_KICKED) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
			td.allow_cancel = 0;
			if( can_target(&td) && new_pick_target(&td, get_sleighted_color_text(player, card, "Select target non%s creature", COLOR_BLACK), 0, 0| GS_LITERAL_PROMPT) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
			instance->number_of_targets = 0;
		}
		if( check_special_flags(player, card, SF_KICKED2) ){
			gain_life(player, 3);
		}
	}

	return 0;
}

int card_stormscape_familiar(int player, int card, event_t event)
{
  /* Stormscape Familiar	|1|U
   * Creature - Bird 1/1
   * Flying
   * |SWhite spells and |Sblack spells you cast cost |1 less to cast. */

  familiar(player, card, event, COLOR_TEST_WHITE | COLOR_TEST_BLACK);
  return 0;
}

int card_strafe(int player, int card, event_t event)
{
/* CARD_ID_STRAFE	5312
Strafe	|R
Sorcery
~ deals 3 damage to target non|Sred creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_RED);

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_SPELL)
	{
		if (valid_target(&td))
			damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, card);

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
							get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_RED), 1, NULL);
}

int card_stratadon(int player, int card, event_t event){
/* CARD_ID_STRATADON	5313
Stratadon	|10
Artifact Creature - Beast 5/5
Domain - ~ costs |1 less to cast for each basic land type among lands you control.
Trample */

	if(event == EVENT_MODIFY_COST ){
		COST_COLORLESS -= count_domain(player, card);
	}
	return 0;
}

int card_sunken_hope(int player, int card, event_t event){
/* CARD_ID_SUNKEN_HOPE	5314
Sunken Hope	|3|U|U
Enchantment
At the beginning of each player's upkeep, that player returns a creature he or she controls to its owner's hand. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = current_turn;
		td.preferred_controller = current_turn;
		td.who_chooses = current_turn;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) ){
			if( new_pick_target(&td, "Select a creature you control.", 0, GS_LITERAL_PROMPT) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_sunscape_battlemage(int player, int card, event_t event){
	/*
	  Sunscape Battlemage |2|W
	  Creature - Human Wizard 2/2
	  Kicker {1}{G} and/or {2}{U} (You may pay an additional {1}{G} and/or {2}{U} as you cast this spell.)
	  When Sunscape Battlemage enters the battlefield, if it was kicked with its {1}{G} kicker, destroy target creature with flying.
	  When Sunscape Battlemage enters the battlefield, if it was kicked with its {2}{U} kicker, draw two cards.
	*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_abilities = KEYWORD_FLYING;
		td.allow_cancel = 0;

		volver1(player, card, event, MANACOST_XU(2, 1), MANACOST_XG(1, 1), 5 + (hand_count[player] < 6 ? 5 : 0), 10*can_target(&td));
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( check_special_flags(player, card, SF_KICKED) ){
			draw_cards(player, 2);
		}
		if( check_special_flags(player, card, SF_KICKED2) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.required_abilities = KEYWORD_FLYING;
			td.allow_cancel = 0;
			if( can_target(&td) && new_pick_target(&td, "Select target creature with flying.", 0, GS_LITERAL_PROMPT) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_sunscape_familiar(int player, int card, event_t event)
{
  /* Sunscape Familiar	|1|W
   * Creature - Wall 0/3
   * Defender
   * |SGreen spells and |Sblue spells you cast cost |1 less to cast. */

  familiar(player, card, event, COLOR_TEST_GREEN | COLOR_TEST_BLUE);
  return 0;
}

int card_tahngarth_talruum_hero(int player, int card, event_t event){
/* CARD_ID_TAHNGARTH_TALRUUM_HERO	5318
Tahngarth, Talruum Hero	|3|R|R
Legendary Creature - Minotaur Warrior 4/4
Vigilance
|1|R, |T: ~ deals damage equal to its power to target creature. That creature deals damage equal to its power to ~. */

	check_legend_rule(player, card, event);

	vigilance(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int my_power = get_power(instance->parent_controller, instance->parent_card);
			int his_power = get_power(instance->targets[0].player, instance->targets[0].card);
			damage_creature(instance->targets[0].player, instance->targets[0].card, my_power,
							instance->parent_controller, instance->parent_card);
			damage_creature(instance->parent_controller, instance->parent_card, his_power,
							instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, "TARGET_CREATURE");
}

int card_terminal_moraine(int player, int card, event_t event){

	/* Terminal Moraine	""
	 * Land
	 * |T: Add |1 to your mana pool.
	 * |2, |T, Sacrifice ~: Search your library for a basic land card and put that card onto the battlefield tapped. Then shuffle your library. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_TUTOR_LAND
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(3), 0, NULL, NULL) )
		{
			choice = CHOICE_TUTOR_LAND;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Produce mana", 1, 1,
							"Sac & Tutor basic land", 1, 5);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_TUTOR_LAND ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_TUTOR_LAND ){
			tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 1);
		}
	}

	return 0;
}

int card_terminate(int player, int card, event_t event){
	/*
	  Terminate |B|R
	  Instant
	  Destroy target creature. It can't be regenerated.
	*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_thornscape_battlemage(int player, int card, event_t event){
	/* Thornscape Battlemage	|2|G	0x2002136
	 * Creature - Elf Wizard 2/2
	 * Kicker |R and/or |W
	 * When ~ enters the battlefield, if it was kicked with its |R kicker, it deals 2 damage to any target.
	 * When ~ enters the battlefield, if it was kicked with its |W kicker, destroy target artifact. */
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_ARTIFACT);
		td1.allow_cancel = 0;

		volver1(player, card, event, MANACOST_R(1), MANACOST_W(1), 8*can_target(&td), 5*can_target(&td1));
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		if( check_special_flags(player, card, SF_KICKED2) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			td.allow_cancel = 0;

			if( can_target(&td) && pick_target(&td, "TARGET_ANY") ){
				damage_target0(player, card, 2);
			}
			instance->number_of_targets = 0;
		}

		if( check_special_flags(player, card, SF_KICKED) ){
			target_definition_t td1;
			default_target_definition(player, card, &td1, TYPE_ARTIFACT);
			td1.allow_cancel = 0;

			if( can_target(&td1) && pick_target(&td1, "TARGET_ARTIFACT") ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
			instance->number_of_targets = 0;
		}

	}

	return 0;
}

int card_thornscape_familiar(int player, int card, event_t event)
{
  /* Thornscape Familiar	|1|G
   * Creature - Insect 2/1
   * |SRed spells and |Swhite spells you cast cost |1 less to cast. */

  familiar(player, card, event, COLOR_TEST_RED | COLOR_TEST_WHITE);
  return 0;
}

int card_thunderscape_battlemage(int player, int card, event_t event){
	/*
	  Thunderscape Battlemage |2|R
	  Creature - Human Wizard 2/2
	  Kicker {1}{B} and/or {G} (You may pay an additional {1}{B} and/or {G} as you cast this spell.)
	  When Thunderscape Battlemage enters the battlefield, if it was kicked with its {1}{B} kicker, target player discards two cards.
	  When Thunderscape Battlemage enters the battlefield, if it was kicked with its {G} kicker, destroy target enchantment.
	*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_ENCHANTMENT);
		td1.allow_cancel = 0;

		volver(player, card, event, MANACOST_XB(1, 1), MANACOST_G(1), 8*can_target(&td), 5*can_target(&td1));
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		if( check_special_flags(player, card, SF_KICKED) ){
			target_definition_t td;
			default_target_definition(player, card, &td, 0);
			td.zone = TARGET_ZONE_PLAYERS;
			td.allow_cancel = 0;

			if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
				new_multidiscard(instance->targets[0].player, 2, 0, player);
			}
			instance->number_of_targets = 0;
		}
		if( check_special_flags(player, card, SF_KICKED2) ){
			target_definition_t td1;
			default_target_definition(player, card, &td1, TYPE_ENCHANTMENT);
			td1.allow_cancel = 0;

			if( can_target(&td1) && pick_target(&td1, "TARGET_ENCHANTMENT") ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_thunderscape_familiar(int player, int card, event_t event)
{
  /* Thunderscape Familiar	|1|R
   * Creature - Kavu 1/1
   * First strike
   * |SBlack spells and |Sgreen spells you cast cost |1 less to cast. */

  familiar(player, card, event, COLOR_TEST_BLACK | COLOR_TEST_GREEN);
  return 0;
}

int card_treva_charm(int player, int card, event_t event){
/*
Treva's Charm |W|U|G
Instant
Choose one —
• Destroy target enchantment.
• Exile target attacking creature.
• Draw a card, then discard a card.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance( player, card);

	enum{
		CHOICE_KILL_ENCH = 1,
		CHOICE_EXILE_ATTACKER,
		CHOICE_DRAW_DISCARD
	};

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {
							0,
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td, NULL, 1, NULL),
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td2, NULL, 1, NULL),
							basic_spell(player, card, EVENT_CAN_CAST)
			};
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Destroy target enchantment", abils[1], 5,
								"Exile target attacking creature", abils[2], 10,
								"Draw & discard", abils[3], 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_KILL_ENCH ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ENCHANTMENT", 1, NULL);
		}
		if( instance->info_slot == CHOICE_EXILE_ATTACKER ){
			return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td2, "Select target attacking creature.", 1, NULL);
		}
		if( instance->info_slot == CHOICE_DRAW_DISCARD ){
			return basic_spell(player, card, event);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_KILL_ENCH && valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( instance->info_slot == CHOICE_EXILE_ATTACKER && valid_target(&td2) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		if( instance->info_slot == CHOICE_DRAW_DISCARD ){
			draw_a_card(player);
			discard(player, 0, player);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_urzas_guilt(int player, int card, event_t event){
/* CARD_ID_URZAS_GUILT	5326
Urza's Guilt	|2|U|B
Sorcery
Each player draws two cards, then discards three cards, then loses 4 life. */

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p,{
				draw_cards(p, 2);
				new_multidiscard(p, 3, 0, player);
				lose_life(p, 4);
			};
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_voice_of_all(int player, int card, event_t event){
/* CARD_ID_VOICE_OF_ALL	1620
Voice of All	|2|W|W
Creature - Angel 2/2
Flying
As ~ enters the battlefield, choose a color.
~ has protection from the chosen color. */

	// original code : 0x4e4a34

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card) ){
		event_result |= get_card_instance(player, card)->info_slot;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = select_a_protection(player);
	}

	return 0;
}

int card_warped_devotion(int player, int card, event_t event){
	/*
	  0x4151A0
	  Warped Devotion |2|B
	  Enchantment
	  Whenever a permanent is returned to a player's hand, that player discards a card.
	*/
	if( ! is_humiliated(player, card) ){
		if( trigger_condition == TRIGGER_BOUNCE_PERMANENT ){
			if( affect_me( player, card) && reason_for_trigger_controller == player ){
				if( trigger_cause != -1 ){ //Won't trigger for tokens
					if(event == EVENT_TRIGGER){
						event_result |= RESOLVE_TRIGGER_MANDATORY;
					}
					else if(event == EVENT_RESOLVE_TRIGGER){
							discard(trigger_cause_controller, 0, player);
					}
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}
