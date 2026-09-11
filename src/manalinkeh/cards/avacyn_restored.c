#include "manalink.h"

// ------------ General functions
int miracle(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_CARD_DRAWN && affect_me(player, card) && !suppress_draw ){
		if( reason_for_trigger_controller == player ){
			if( cards_drawn_this_turn[player] == 1 ){
				int result = -1;
				int i;
				for(i=0; i<active_cards_count[player]; i++){
					if( in_hand(player, i) && check_state(player, i, STATE_DRAWN_THIS_TURN) ){
						result = i;
						break;
					}
				}
				if( result == -1 ){
					return 0;
				}
				int trig = 0;
				int cost[6] = {0, 0, 0, 0, 0, 0};
				int ai_priority = 2;
				int iid = get_original_internal_card_id(player, result);
				int csvid = cards_data[iid].id;
				if( cards_data[iid].cc[2] == 15 && can_legally_play_iid(player, iid) ){
					if( csvid == CARD_ID_BANISHING_STROKE ){
						if( player == AI ){
							target_definition_t td;
							default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT | TYPE_CREATURE);
							td.allowed_controller = 1-player;
							td.illegal_abilities = KEYWORD_PROT_WHITE;
							ai_priority = can_target(&td) ? 2 : 0;
						}
						cost[5] = 1;
					}
					if( csvid == CARD_ID_TEMPORAL_MASTERY ){
						cost[0] = 1;
						cost[2] = 1;
					}
					if( csvid == CARD_ID_THUNDEROUS_WRATH ){
						if( player == AI ){
							target_definition_t td;
							default_target_definition(player, card, &td, TYPE_CREATURE);
							td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
							ai_priority = can_target(&td) ? 2 : 0;
						}
						cost[4] = 1;
					}
					if( csvid == CARD_ID_BONFIRE_OF_THE_DAMNED ){
						if( player == AI ){
							target_definition_t td;
							default_target_definition(player, card, &td, TYPE_CREATURE);
							td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
							ai_priority = can_target(&td) ? 2 : 0;
						}
						cost[4] = 1;
					}
					if( csvid == CARD_ID_REFORGE_THE_SOUL ){
						ai_priority = (7-hand_count[1-player]) < (7-hand_count[player]) ? 2 : 0;
						cost[0] = 1;
						cost[4] = 1;
					}
					if( csvid == CARD_ID_REVENGE_OF_THE_HUNTED || csvid == CARD_ID_BLESSINGS_OF_NATURE){
						if( player == AI ){
							target_definition_t td;
							default_target_definition(player, card, &td, TYPE_CREATURE);
							td.preferred_controller = player;
							ai_priority = can_target(&td) ? 2 : 0;
						}
						cost[3] = 1;
					}
					if( csvid == CARD_ID_ENTREAT_THE_ANGELS ){
						ai_priority = has_mana_multi(player, MANACOST_XW(2, 2)) ? 2 : 0;
						cost[5] = 2;
					}
					if( csvid == CARD_ID_TERMINUS ){
						if( player == AI ){
							ai_priority = count_subtype(1-player, TYPE_CREATURE, -1)-count_subtype(player, TYPE_CREATURE, -1) > 1 ? 2 : 0;
						}
						cost[5] = 1;
					}
					if( csvid == CARD_ID_DEVASTATION_TIDE ){
						if( player == AI ){
							test_definition_t this_test;
							default_test_definition(&this_test, TYPE_LAND);
							this_test.type_flag = DOESNT_MATCH;
							ai_priority = 	check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &this_test)-
											check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test) > 2 ? 2 : 0;
						}
						cost[0] = 1;
						cost[2] = 1;
					}
					if( csvid == CARD_ID_VANISHMENT ){
						if( player == AI ){
							target_definition_t td;
							default_target_definition(player, card, &td, TYPE_PERMANENT);
							td.illegal_type = TYPE_LAND;
							ai_priority = can_target(&td) ? 2 : 0;
						}
						cost[2] = 1;
					}
					int cless = get_updated_casting_cost(player, result, -1, event, cost[0]);
					if( has_mana_multi(player, cless, cost[1], cost[2], cost[3], cost[4], cost[5]) ){
						trig = 1;
					}
				}
				if( trig == 1){
					if(event == EVENT_TRIGGER){
						event_result |= 2u;
					}
					else if(event == EVENT_RESOLVE_TRIGGER){
							int cless = get_updated_casting_cost(player, -1, iid, event, cost[0]);
							int choice = DIALOG(player, result, EVENT_ACTIVATE,
												DLG_FULLCARD_ID(iid), DLG_NO_STORAGE, DLG_NO_CANCEL,
												"Cast", 1, ai_priority,
												"Pass", 1, 1);
							if( choice == 1 ){
								charge_mana_multi_while_resolving(player, i, EVENT_RESOLVE_SPELL, player,
															cless, cost[1], cost[2], cost[3], cost[4], cost[5]);
								if( spell_fizzled != 1 ){
									play_card_in_hand_for_free_miracle(player, result);
								}
							}
					}
				}
			}

		}
	}
	return 0;
}

static int is_paired(int player, int card){
	return check_special_flags2(player, card, SF2_PAIRED);
}

static int paired_legacy(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[1].player > -1 ){
		if( ! in_play(instance->targets[1].player, instance->targets[1].card) ||
			instance->damage_target_player != instance->targets[1].player
		  ){
			remove_special_flags2(instance->targets[0].player, instance->targets[0].card, SF2_PAIRED);
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

static void pair(int player, int card, int player2, int card2){
	card_instance_t *instance = get_card_instance(player, card);

	instance->targets[3].player = player2;
	instance->targets[3].card = card2;

	set_special_flags2(player, card, SF2_PAIRED);
	set_special_flags2(player2, card2, SF2_PAIRED);

	int legacy = create_targetted_legacy_effect(player, card, &paired_legacy, player2, card2);
	card_instance_t *instance3 = get_card_instance(player,legacy);
	instance3->targets[1].player = player;
	instance3->targets[1].card = card;
}

static int boost_paired_creature(int player, int card, event_t event, int pow, int tou, int key, int s_key){

	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[3].player > -1 ){
		modify_pt_and_abilities(instance->targets[3].player, instance->targets[3].card, event, pow, tou, key);
		if( s_key > 0 ){
			special_abilities(instance->targets[3].player, instance->targets[3].card, event, s_key, player, card);
		}
	}
	return 0;
}

static void pump_paired_creature_until_eot(int player, int card, int pow, int tou, int key, int s_key){

	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[3].player > -1 ){
		pump_ability_until_eot(player, card, instance->targets[3].player, instance->targets[3].card, pow, tou, key, s_key);
	}
}

static const char* target_is_unpaired(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  return is_paired(player, card) ? "already paired" : NULL;
}

static int soulbond(int player, int card, event_t event){

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		set_special_flags2(player, card, SF2_SOULBOND);
	}

	card_instance_t *instance = get_card_instance(player, card);
	if (instance->token_status & STATUS_DYING){
		return 0;
	}

	if (!is_paired(player, card) && specific_cip(player, card, event, player, RESOLVE_TRIGGER_AI(player), TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0)){
		if (instance->targets[1].player == player && instance->targets[1].card == card){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.preferred_controller = player;
			td.allowed_controller = player;
			td.illegal_abilities = 0;
			td.special = TARGET_SPECIAL_NOT_ME | TARGET_SPECIAL_EXTRA_FUNCTION;
			td.extra = (int32_t)target_is_unpaired;

			if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
				pair(player, card, instance->targets[0].player, instance->targets[0].card);
				if( check_special_flags2(instance->targets[0].player, instance->targets[0].card, SF2_SOULBOND) ){
					pair(instance->targets[0].player, instance->targets[0].card, player, card);
				}
			}
			instance->number_of_targets = 0;
		}
		else{
			if( ! is_paired(instance->targets[1].player, instance->targets[1].card) ){
				pair(player, card, instance->targets[1].player, instance->targets[1].card);
				if( check_special_flags2(instance->targets[1].player, instance->targets[1].card, SF2_SOULBOND) ){
					pair(instance->targets[1].player, instance->targets[1].card, player, card);
				}
			}
		}
	}

	if( instance->targets[3].player != -1 && ! in_play(instance->targets[3].player, instance->targets[3].card) ){
		remove_special_flags2(player, card, SF2_PAIRED);
		instance->targets[3].player = -1;
		instance->targets[3].card = -1;
	}
	return 0;
}

static void make_human(int player, int card, int amount){
	token_generation_t token;
	default_token_definition(player, card, CARD_ID_HUMAN, &token);
	token.pow = token.tou = 1;
	token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
	token.qty = amount;
	generate_token(&token);

}

static void make_angel(int player, int card, int amount){
	token_generation_t token;
	default_token_definition(player, card, CARD_ID_ANGEL, &token);
	token.pow = token.tou = 4;
	token.key_plus = KEYWORD_FLYING;
	token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
	token.qty = amount;
	generate_token(&token);

}

//--------------- Cards

// WHITE

int card_angel_of_glorys_rise(int player, int card, event_t event){
/* CARD_ID_ANGEL_OF_GLORYS_RISE	12303
Angel of Glory's Rise	|5|W|W
Creature - Angel 4/6
Flying
When ~ enters the battlefield, exile all Zombies, then return all Human creature cards from your graveyard to the battlefield. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_ZOMBIE;
		APNAP(p, {new_manipulate_all(player, card, p, &test, KILL_EXILE);};);
		test.subtype = SUBTYPE_ZOMBIE;
		new_reanimate_all(player, card, player, &test, REANIMATE_DEFAULT);
	}

	return 0;
}

int card_angel_of_jubilation(int player, int card, event_t event)
{
  /* Angel of Jubilation	|1|W|W|W
   * Creature - Angel 3/3
   * Flying
   * Other non|Sblack creatures you control get +1/+1.
   * Players can't pay life or sacrifice creatures to cast spells or activate abilities. */

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affected_card_controller == player && affected_card != card
	  && !(get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_BLACK))
	  && in_play(player, card) && in_play(affected_card_controller, affected_card) && !is_humiliated(player, card))
	event_result += 1;

  // Second ability implemented as hacks in can_pay_life(), can_sacrifice_as_cost(), and new_sacrifice().

  return 0;
}

// Angelic Wall --> vanilla

int card_avacyn_angel_of_hope(int player, int card, event_t event)
{
/* CARD_ID_AVACYN_ANGEL_OF_HOPE	12310
Avacyn, Angel of Hope	|5|W|W|W
Legendary Creature - Angel 8/8
Flying, vigilance, indestructible
Other permanents you control have indestructible. */

  check_legend_rule(player, card, event);

  vigilance(player, card, event);

  if (event == EVENT_ABILITIES && affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_PERMANENT)
	  && in_play(player, card) && !is_humiliated(player, card))
	indestructible(affected_card_controller, affected_card, event);

  return 0;
}

int card_banishing_stroke(int player, int card, event_t event){
/* CARD_ID_BANISHING_STROKE	12311
Banishing Stroke	|5|W
Instant
Put target artifact, creature, or enchantment on the bottom of its owner's library.
Miracle |W (You may cast this card for its miracle cost when you draw it if it's the first card you drew this turn.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.illegal_type = TYPE_LAND;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			put_on_bottom_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target artifact, creature, or enchantment.", 1, NULL);
}

int card_builders_blessing(int player, int card, event_t event){
/* CARD_ID_BUILDERS_BLESSING	12321
Builder's Blessing	|3|W
Enchantment
Untapped creatures you control get +0/+2. */

	if( event == EVENT_TOUGHNESS && affected_card_controller == player && ! is_humiliated(player, card) ){
		if( ! is_tapped(affected_card_controller, affected_card) ){
			event_result+=2;
		}
	}

	return global_enchantment(player, card, event);
}

int card_call_to_serve(int player, int card, event_t event){
/* CARD_ID_CALL_TO_SERVE	12324
Call to Serve	|1|W
Enchantment - Aura
Enchant non|Sblack creature
Enchanted creature gets +1/+2, has flying, and is an Angel in addition to its other types. */

	return generic_aura(player, card, event, player, 1, 2, KEYWORD_FLYING, 0, SUBTYPE_ANGEL, 0, 0);
}

int card_cathars_crusade(int player, int card, event_t event){

	/* Cathars' Crusade	|3|W|W
	 * Enchantment
	 * Whenever a creature enters the battlefield under your control, put a +1/+1 counter on each creature you control. */

	etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		manipulate_type(player, card, player, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));
	}

	return global_enchantment(player, card, event);
}

// Cathedral Sanctifier --> Angel of Mercy

int card_commanders_autority(int player, int card, event_t event){
	/* Commander's Authority	|4|W
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature has "At the beginning of your upkeep, put a 1/1 |Swhite Human creature token onto the battlefield." */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		upkeep_trigger_ability(player, card, event, p);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			make_human(p, c, 1);
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

int card_cloudshift(int player, int card, event_t event){
/* CARD_ID_CLOUDSHIFT	12330
Cloudshift	|W
Instant
Exile target creature you control, then return that card to the battlefield under your control. */

	// Used externally
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature you control.", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			blink_effect_controller(instance->targets[0].player, instance->targets[0].card, 0);
			kill_card(player, card, KILL_DESTROY );
		}
	}

	return 0;
}

int card_cursebreak(int player, int card, event_t event){
/* CARD_ID_CURSEBREAK	12336
Cursebreak	|1|W
Instant
Destroy target enchantment. You gain 2 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			gain_life(player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ENCHANTMENT", 1, NULL);
}

// Defang (unconfirmed) --> Muzzle

static void pump_if_angel(int s_player, int s_card, int t_player, int t_card){
	if( has_subtype(t_player, t_card, SUBTYPE_ANGEL) ){
		hack_silent_counters = 1;
		add_1_1_counters(t_player, t_card, 2);
		hack_silent_counters = 0;
	}
}

int card_defy_death(int player, int card, event_t event){
/*
Defy Death |3|W|W
Sorcery
Return target creature card from your graveyard to the battlefield. If it's an Angel, put two +1/+1 counters on it.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
	}
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( player == HUMAN ){
			return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
		}
		else{
			test.subtype = SUBTYPE_ANGEL;
			if( new_special_count_grave(player, &test) ){
				return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
			}
			else{
				default_test_definition(&test, TYPE_CREATURE);
				return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent_with_function(player, card, player, selected, REANIMATE_DEFAULT, &pump_if_angel);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
}

int card_devout_chaplain(int player, int card, event_t event){
/*
Devout Chaplain |2|W
Creature — Human Cleric 2/2
{T}, Tap two untapped Humans you control: Exile target artifact or enchantment.
*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.required_subtype = SUBTYPE_CLERIC;
	td.illegal_abilities = 0;
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;
	td.special = TARGET_SPECIAL_NOT_ME;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, &td1, NULL)  ){
			return target_available(player, card, &td) > 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int tapped[2] = {-1, -1};
			int tc = 0;
			while( tc < 2 ){
				if( select_target(player, card, &td, "Select a Cleric to tap.", &(instance->targets[tc])) ){
					state_untargettable(instance->targets[tc].player, instance->targets[tc].card, 1);
					tapped[tc] = instance->targets[tc].card;
					tc++;
				}
				else{
					break;
				}
			}
			int i;
			for(i=0; i<tc; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			}
			if( tc < 2 ){
				spell_fizzled = 1;
				return 0;
			}
			instance->number_of_targets = 0;
			if( pick_target(&td1, "DISENCHANT") ){
				for(i=0; i<2; i++){
					tap_card(player, tapped[i]);
				}
				tap_card(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if(valid_target(&td1) ){
			action_on_target(player, card, 0, KILL_EXILE);
		}
	}

	return 0;
}

static int divine_deflection_effect(int player, int card, event_t event){

	// instance->counter_toughness -> max damage to prevent

	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->damage_target_player == player && damage->info_slot > 0 ){
			int dmg = prevent_damage(affected_card_controller, affected_card, instance->counter_toughness);
			if( dmg ){
				damage_target0(player, card, dmg);
				instance->counter_toughness -= dmg;
				if( instance->counter_toughness < 1 ){
					kill_card(player, card, KILL_EXILE);
				}
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

static const char* damage_to_you_or_permanents_you_control(int who_chooses, int player, int card){
	card_instance_t* dmg = get_card_instance(player, card);
	if( dmg->internal_card_id != damage_card ){
		return "must be a damage card";
	}
	if( dmg->damage_target_player != who_chooses ){
		return "must be damage dealt to you";
	}
	return NULL;
}

int card_divine_deflection(int player, int card, event_t event){
	/* Divine Deflection	|X|W	0x2005d09
	 * Instant
	 * Prevent the next X damage that would be dealt to you and/or permanents you control this turn. If damage is prevented this way, ~ deals that much damage to any target. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_EFFECT);
	td2.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td2.extra = (int32_t)damage_to_you_or_permanents_you_control;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_CAST && can_target(&td) ){
		int result = generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td2, NULL, 1, NULL);
		if( result ){
			return result;
		}
		else{
			if( player == HUMAN ){
				return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 1, NULL);
			}
		}
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card)){
		int result = generic_spell(player, card, EVENT_CAN_CAST, GS_X_SPELL | GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td2, NULL, 1, NULL);
		instance->number_of_targets = instance->targets[1].player = 0;
		if( pick_target(&td, "TARGET_ANY") ){
			if( result ){
				new_pick_target(&td2, "TARGET_DAMAGE", 1, 1);
			}
			if( spell_fizzled == 1 ){
				return 0;
			}
			instance->targets[1].player = result;
			if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
				set_x_for_x_spells(player, card, event, x_value);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int do_legacy = 1;
			if( instance->targets[1].player ){
				if( validate_target(player, card, &td2, 1) ){
					int dmg = prevent_damage(instance->targets[1].player, instance->targets[1].card, instance->info_slot);
					if( dmg ){
						damage_target0(player, card, dmg);
						instance->info_slot -= dmg;
						if( instance->info_slot < 1 ){
							do_legacy = 0;
						}
					}
				}
			}
			if( do_legacy ){
				int legacy = create_legacy_effect(player, card, &divine_deflection_effect);
				card_instance_t *leg = get_card_instance(player, legacy);
				leg->counter_toughness = instance->info_slot;
				leg->targets[0] = instance->targets[0];
				leg->number_of_targets = 1;
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_emancipation_angel(int player, int card, event_t event){
/* CARD_ID_EMANCIPATION_ANGEL	12361
Emancipation Angel	|1|W|W
Creature - Angel 3/3
Flying
When ~ enters the battlefield, return a permanent you control to its owner's hand. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += ((count_subtype(player, TYPE_PERMANENT, -1)*5)-20);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select a permanent you control.", 0, 0) ){
			bounce_permanent(player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_entreat_the_angels(int player, int card, event_t event){
	/* Entreat the Angels	|X|X|W|W|W
	 * Sorcery
	 * Put X 4/4 |Swhite Angel creature tokens with flying onto the battlefield.
	 * Miracle |X|W|W */

	if(event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			get_card_instance(player, card)->info_slot = 0;
			if( check_special_flags2(player, card, SF2_MIRACLE) ){
				charge_mana(player, COLOR_COLORLESS, -1);
				if( spell_fizzled != 1 ){
					set_x_for_x_spells(player, card, event, x_value);
				}
			}
			else{
				if( ! played_for_free(player, card) && ! is_token(player, card) ){
					int amount = charge_mana_for_double_x(player, COLOR_COLORLESS);
					set_x_for_x_spells(player, card, event, amount/2);
				}
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		make_angel(player, card, get_card_instance(player, card)->info_slot);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

// Farbog Explorer --> vanilla

int card_goldnight_commander(int player, int card, event_t event)
{
/* CARD_ID_GOLDNIGHT_COMMANDER	12383
Goldnight Commander	|3|W
Creature - Human Cleric Soldier 2/2
Whenever another creature enters the battlefield under your control, creatures you control get +1/+1 until end of turn. */

	if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) )
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		pump_subtype_until_eot(player, card, player, -1, 1, 1, 0, 0);
	}

	return 0;
}

int card_goldnight_redeemer(int player, int card, event_t event){
/* CARD_ID_GOLDNIGHT_REDEEMER	12384
Goldnight Redeemer	|4|W|W
Creature - Angel 4/4
Flying
When ~ enters the battlefield, you gain 2 life for each other creature you control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 2*(count_permanents_by_type(player, TYPE_CREATURE)-1));
	}

	return 0;
}

int card_herald_of_war(int player, int card, event_t event){
/* CARD_ID_HERALD_OF_WAR	12396
Herald of War	|3|W|W
Creature - Angel 3/3
Flying
Whenever ~ attacks, put a +1/+1 counter on it.
Angel spells and Human spells you cast cost |1 less to cast for each +1/+1 counter on ~. */

	if (declare_attackers_trigger(player, card, event, 0, player, card)){
		add_1_1_counter(player, card);
	}

	// Angel spells and Human spells you cast cost |1 less to cast for each +1/+1 counter on ~.
	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card) ){
		if( has_subtype(affected_card_controller, affected_card, SUBTYPE_ANGEL) ||
			has_subtype(affected_card_controller, affected_card, SUBTYPE_HUMAN)
		  ){
			COST_COLORLESS-=count_1_1_counters(player, card);
		}
	}

	return 0;
}

int card_holy_justicar(int player, int card, event_t event){
/* CARD_ID_HOLY_JUSTICIAR	12397
Holy Justiciar	|3|W
Creature - Human Cleric 2/1
|2|W, |T: Tap target creature. If that creature is a Zombie, exile it. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
			if( has_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_ZOMBIE) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XW(2, 1), 0, &td, "TARGET_CREATURE");
}

static int leap_of_faith_effect(int player, int card, event_t event){

	if( event == EVENT_PREVENT_DAMAGE && get_card_instance(player, card)->damage_target_player > -1 ){
		int p = get_card_instance(player, card)->damage_target_player;
		int c = get_card_instance(player, card)->damage_target_card;
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card );
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == c && damage->damage_target_player == p && damage->info_slot > 0 )
			{
				prevent_damage(affected_card_controller, affected_card, damage->info_slot);
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_leap_of_faith(int player, int card, event_t event){
/* CARD_ID_LEAP_OF_FAITH	12411
Leap of Faith	|2|W
Instant
Target creature gains flying until end of turn. Prevent all damage that would be dealt to that creature this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FLYING, 0);
			int legacy = create_targetted_legacy_effect(player, card, &leap_of_faith_effect,
														instance->targets[0].player, instance->targets[0].card);
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_midnight_duelist(int player, int card, event_t event){
/* CARD_ID_MIDNIGHT_DUELIST	12423
Midnight Duelist	|W
Creature - Human Soldier 1/2
Protection from Vampires */

	protection_from_subtype(player, card, event, SUBTYPE_VAMPIRE);
	return 0;
}

int card_midvast_protector(int player, int card, event_t event){
/* CARD_ID_MIDVAST_PROTECTOR	12424
Midvast Protector	|3|W
Creature - Human Wizard 2/3
When ~ enters the battlefield, target creature you control gains protection from the color of your choice until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allowed_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target creature you control.", 0, 0) ){
			pump_ability_until_eot(player, card,  instance->targets[0].player, instance->targets[0].card, 0, 0,
									select_a_protection(player) | KEYWORD_RECALC_SET_COLOR, 0);
		}

		instance->number_of_targets = 0;
	}
	return 0;
}

int card_moonlight_geist(int player, int card, event_t event){
/*
Moonlight Geist |2|W
Creature — Spirit 2/1
Flying
{3}{W}: Prevent all combat damage that would be dealt to and dealt by Moonlight Geist this turn.
*/
	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_XW(3, 1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if( (current_turn == player && is_unblocked(player, card)) ||
			(current_turn == 1-player && get_card_instance(player, card)->blocking == 255) )
		{
			ai_modifier -= 25;
		}
		return generic_activated_ability(player, card, event, 0, MANACOST_XW(3, 1), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		maze_of_ith_effect(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card);
	}

	return 0;
}

int card_moorland_inquisitor(int player, int card, event_t event){
/* CARD_ID_MOORLAND_INQUISITOR	12429
Moorland Inquisitor	|1|W
Creature - Human Soldier 2/2
|2|W: ~ gains first strike until end of turn. */

	return generic_shade(player, card, event, 0, MANACOST_XW(2, 1), 0, 0, KEYWORD_FIRST_STRIKE, 0);
}

int card_nearheath_pilgrim(int player, int card, event_t event){
/* CARD_ID_NEARHEATH_PILGRIM	12432
Nearheath Pilgrim	|1|W
Creature - Human Cleric 2/1
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, both creatures have lifelink. */

	if( is_paired(player, card) ){
		lifelink(player, card, event);
		boost_paired_creature(player, card, event, 0, 0, 0, SP_KEYWORD_LIFELINK);
	}

	return soulbond(player, card, event);
}

int card_restoration_angel(int player, int card, event_t event){
/* CARD_ID_RESTORATION_ANGEL	12448
Restoration Angel	|3|W
Creature - Angel 3/4
Flash
Flying
When ~ enters the battlefield, you may exile target non-Angel creature you control, then return that card to the battlefield under your control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.required_subtype = SUBTYPE_ANGEL;
		td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && new_pick_target(&td, "Select target non-Angel creature you control.", 0, GS_LITERAL_PROMPT) ){
			blink_effect_controller(instance->targets[0].player, instance->targets[0].card, 0);
		}
		instance->number_of_targets = 0;
	}

	return flash(player, card, event);
}

int card_riders_of_gavony(int player, int card, event_t event){
/*
Riders of Gavony |2|W|W
Creature — Human Knight 3/3
Vigilance
As Riders of Gavony enters the battlefield, choose a creature type.
Human creatures you control have protection from creatures of the chosen type.
*/
	vigilance(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->targets[4].card = select_a_subtype(player, card);
	}

	if(event == EVENT_BLOCK_LEGALITY ){
		if( player == attacking_card_controller && has_creature_type(attacking_card_controller, attacking_card, SUBTYPE_HUMAN) ){
			if( has_creature_type(affected_card_controller, affected_card, get_card_instance(player, card)->targets[4].card) ){
				event_result = 1;
			}
		}
	}

	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *source = get_card_instance(affected_card_controller, affected_card);

		if( damage_card != source->internal_card_id || source->info_slot <= 0 ){
			return 0;
		}

		if( source->damage_target_player == player && source->damage_target_card != -1 ){
			if( in_play(source->damage_source_player, source->damage_source_card) &&
				has_subtype(source->damage_source_player, source->damage_source_card, get_card_instance(player, card)->targets[4].card)
			  ){
				prevent_damage(affected_card_controller, affected_card, source->info_slot);
			}
		}
	}

	return 0;
}

int card_righteous_blow(int player, int card, event_t event){
/* CARD_ID_RIGHTEOUS_BLOW	12451
Righteous Blow	|W
Instant
~ deals 2 damage to target attacking or blocking creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_state = TARGET_STATE_IN_COMBAT;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 2, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target attacking or blocking creature.",
						1, NULL);
}

// Seraph of Dawn --> Child of Night

int card_silverblade_paladin(int player, int card, event_t event){
/* CARD_ID_SILVERBLADE_PALADIN	12466
Silverblade Paladin	|1|W|W
Creature - Human Knight 2/2
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, both creatures have double strike. */

	if( is_paired(player, card) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_DOUBLE_STRIKE);
		boost_paired_creature(player, card, event, 0, 0, KEYWORD_DOUBLE_STRIKE, 0);
	}

	return soulbond(player, card, event);
}

int card_spectral_gateguards(int player, int card, event_t event){
/* CARD_ID_SPECTRAL_GATEGUARDS	12473
Spectral Gateguards	|4|W
Creature - Spirit Soldier 2/5
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, both creatures have vigilance. */

	if( is_paired(player, card) ){
		vigilance(player, card, event);
		boost_paired_creature(player, card, event, 0, 0, 0, SP_KEYWORD_VIGILANCE);
	}

	return soulbond(player, card, event);
}

// Thraben Stalwart --> Serra Angel

int card_terminus(int player, int card, event_t event){
/* CARD_ID_TERMINUS	12483
Terminus	|4|W|W
Sorcery
Put all creatures on the bottom of their owners' libraries.
Miracle |W (You may cast this card for its miracle cost when you draw it if it's the first card you drew this turn.) */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		APNAP(p, {new_manipulate_all(player, card, p, &this_test, ACT_PUT_ON_BOTTOM);};);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_voice_of_the_provinces(int player, int card, event_t event){
	/* Voice of the Provinces	|4|W|W
	 * Creature - Angel 3/3
	 * Flying
	 * When ~ enters the battlefield, put a 1/1 |Swhite Human creature token onto the battlefield. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		make_human(player, card, 1);
	}

	return 0;
}

int card_zealous_strike(int player, int card, event_t event){
	/* Zealous Strike	|1|W
	 * Instant
	 * Target creature gets +2/+2 and gains first strike until end of turn. */
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 2, KEYWORD_FIRST_STRIKE, 0);
}

//BLUE
int card_alchemists_apprentice(int player, int card, event_t event){
/* CARD_ID_ALCHEMISTS_APPRENTICE	12300
Alchemist's Apprentice	|1|U
Creature - Human Wizard 1/1
Sacrifice ~: Draw a card. */

	if(event == EVENT_RESOLVE_ACTIVATION){
		draw_cards(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_amass_the_components(int player, int card, event_t event){
/*
Amass the Components |3|U
Sorcery
Draw three cards, then put a card from your hand on the bottom of your library.
*/

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 3);

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card to put on bottom.");
		int selected = hand_count[player] ? new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &this_test) : -1;
		if( selected > -1 ){
			put_on_bottom_of_deck(player, selected);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_arcane_melee(int player, int card, event_t event){
/* CARD_ID_ARCANE_MELEE	12308
Arcane Melee	|4|U
Enchantment
Instant and sorcery spells cost |2 less to cast. */

	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card) ){
		if( is_what(affected_card_controller, affected_card, TYPE_SORCERY | TYPE_INSTANT | TYPE_INTERRUPT) &&
			! is_what(affected_card_controller, affected_card, TYPE_CREATURE)
		  ){
			COST_COLORLESS-=2;
		}
	}

	return global_enchantment(player, card, event);
}

int card_captain_of_the_mists(int player, int card, event_t event)
{
  /* Captain of the Mists	|2|U
   * Creature - Human Wizard 2/3
   * Whenever another Human enters the battlefield under your control, untap ~.
   * |1|U, |T: You may tap or untap target permanent. */

	if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card))
	{
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "");
		test.subtype = SUBTYPE_HUMAN;
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		untap_card(player, card);
	}

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_PERMANENT);
  td.preferred_controller = ANYBODY;

  int rval = generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_XU(1,1), 0, &td, "TARGET_PERMANENT");

  if (event == EVENT_ACTIVATE && player == AI && cancel != 1)
	ai_modifier_twiddle(player, card, 0);

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	twiddle(player, card, 0);

  return rval;
}

int card_crippling_chill(int player, int card, event_t event){
/* CARD_ID_CRIPPLING_CHILL	12335
Crippling Chill	|2|U
Instant
Tap target creature. It doesn't untap during its controller's next untap step.
Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			effect_frost_titan(player, card, instance->targets[0].player, instance->targets[0].card);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_deadeye_navigator(int player, int card, event_t event){
/* CARD_ID_DEADEYE_NAVIGATOR	12339
Deadeye Navigator	|4|U|U
Creature - Spirit 5/5
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, each of those creatures has "|1|U: Exile this creature, then return it to the battlefield under your control." */

	if( is_paired(player, card) ){

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_ACTIVATE ){
			int result = generic_activated_ability(player, card, event, 0, MANACOST_XU(1, 1), 0, NULL, NULL);
			if( result ){
				return result;
			}
			return granted_generic_activated_ability(player, card, instance->targets[3].player, instance->targets[3].card, event,
													0, MANACOST_XU(1, 1), 0, NULL, NULL);
		}

		if( event == EVENT_ACTIVATE ){
			int choice = instance->info_slot = 0;
			int can_activate_me = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XU(1, 1), 0, NULL, NULL);
			int can_activate_paired = granted_generic_activated_ability(player, card, instance->targets[3].player, instance->targets[3].card,
																		EVENT_CAN_ACTIVATE, 0, MANACOST_XU(1, 1), 0, NULL, NULL);
			if( can_activate_me ){
				if( can_activate_paired ){
					choice = do_dialog(player, player, card, -1, -1, " Blink Deadeye Navigator\n Blink paired creatur\n Cancel", 0);
				}
				else{
					choice = 1;
				}
			}
			if( choice == 0 ){
				generic_activated_ability(player, card, event, 0, MANACOST_XU(1, 1), 0, NULL, NULL);
				if(spell_fizzled != 1){
					instance->info_slot = 1;
				}
			}
			if( choice == 1 ){
				granted_generic_activated_ability(player, card, instance->targets[3].player, instance->targets[3].card, event,
													0, MANACOST_XU(1, 1), 0, NULL, NULL);
				if(spell_fizzled != 1){
					instance->info_slot = 2;
				}
			}
			if( choice == 2 ){
				spell_fizzled = 1;
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( instance->info_slot == 1 ){
				blink_effect_controller(instance->parent_controller, instance->parent_card, 0);
			}
			else{
				blink_effect_controller(instance->targets[3].player, instance->targets[3].card, 0);
			}
		}
	}

	return soulbond(player, card, event);
}

int card_devastation_tide(int player, int card, event_t event){
/* CARD_ID_DEVASTATION_TIDE	12349
Devastation Tide	|3|U|U
Sorcery
Return all nonland permanents to their owners' hands.
Miracle |1|U (You may cast this card for its miracle cost when you draw it if it's the first card you drew this turn.) */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;

		default_test_definition(&this_test, TYPE_ENCHANTMENT);
		APNAP(p, {new_manipulate_all(player, card, p, &this_test, ACT_BOUNCE);};);

		default_test_definition(&this_test, TYPE_LAND);
		this_test.type_flag = DOESNT_MATCH;
		APNAP(p, {new_manipulate_all(player, card, p, &this_test, ACT_BOUNCE);};);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_dreadwaters(int player, int card, event_t event){
/* CARD_ID_DREADWATERS	12354
Dreadwaters	|3|U
Sorcery
Target player puts the top X cards of his or her library into his or her graveyard, where X is the number of lands you control. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td)  ){
			mill(instance->targets[0].player, count_permanents_by_type(player, TYPE_LAND));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_elgaud_shieldmate(int player, int card, event_t event){
/* CARD_ID_ELGAUD_SHIELDMATE	12360
Elgaud Shieldmate	|3|U
Creature - Human Soldier 2/3
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, both creatures have hexproof. (They can't be the targets of spells or abilities your opponents control.) */

	if( is_paired(player, card) ){
		hexproof(player, card, event);
		boost_paired_creature(player, card, event, 0, 0, 0, SP_KEYWORD_HEXPROOF);
	}

	return soulbond(player, card, event);
}

int card_favourable_winds(int player, int card, event_t event){
/* CARD_ID_FAVORABLE_WINDS	12368
Favorable Winds	|1|U
Enchantment
Creatures you control with flying get +1/+1. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affected_card_controller == player && ! is_humiliated(player, card) &&
		in_play(player, card) && check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING) )
	{
		event_result++;
	}

	return global_enchantment(player, card, event);
}

int card_fettergeist(int player, int card, event_t event){
/* CARD_ID_FETTERGEIST	12370
Fettergeist	|2|U
Creature - Spirit 3/4
Flying
At the beginning of your upkeep, sacrifice ~ unless you pay |1 for each other creature you control. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int kill = 1;
		int mana = count_permanents_by_type(player, TYPE_CREATURE) - 1;
		if( has_mana(player, COLOR_COLORLESS, mana) ){
			charge_mana(player, COLOR_COLORLESS, mana);
			if( spell_fizzled != 1 ){
				kill = 0;
			}
		}
		if( kill == 1 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_fleeting_distraction(int player, int card, event_t event){
/* CARD_ID_FLEETING_DISTRACTION	10929
Fleeting Distraction	|U
Instant
Target creature gets -1/-0 until end of turn.
Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -1, 0);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_galvanic_alchemist(int player, int card, event_t event){
/* CARD_ID_GALVANIC_ALCHEMIST	12373
Galvanic Alchemist	|2|U
Creature - Human Wizard 1/4
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, each of those creatures has "|2|U: Untap this creature." */

	if( is_paired(player, card) ){

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_ACTIVATE ){
			int result = generic_activated_ability(player, card, event, 0, MANACOST_XU(2, 1), 0, NULL, NULL);
			if( result ){
				return result;
			}
			return granted_generic_activated_ability(player, card, instance->targets[3].player, instance->targets[3].card, event,
													0, MANACOST_XU(2, 1), 0, NULL, NULL);
		}

		if( event == EVENT_ACTIVATE ){
			int choice = instance->info_slot = 0;
			int can_activate_me = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XU(2, 1), 0, NULL, NULL);
			int can_activate_paired = granted_generic_activated_ability(player, card, instance->targets[3].player, instance->targets[3].card,
																		EVENT_CAN_ACTIVATE, 0, MANACOST_XU(2, 1), 0, NULL, NULL);
			if( can_activate_me ){
				if( can_activate_paired ){
					choice = do_dialog(player, player, card, -1, -1, " Untap Galvanic Alchemist\n Untap paired creatur\n Cancel", 0);
				}
				else{
					choice = 1;
				}
			}
			if( choice == 0 ){
				generic_activated_ability(player, card, event, 0, MANACOST_XU(2, 1), 0, NULL, NULL);
				if(spell_fizzled != 1){
					instance->info_slot = 1;
				}
			}
			if( choice == 1 ){
				granted_generic_activated_ability(player, card, instance->targets[3].player, instance->targets[3].card, event,
													0, MANACOST_XU(2, 1), 0, NULL, NULL);
				if(spell_fizzled != 1){
					instance->info_slot = 2;
				}
			}
			if( choice == 2 ){
				spell_fizzled = 1;
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( instance->info_slot == 1 ){
				untap_card(instance->parent_controller, instance->parent_card);
			}
			else{
				untap_card(instance->targets[3].player, instance->targets[3].card);
			}
		}
	}

	return soulbond(player, card, event);
}

int card_geist_snatch(int player, int card, event_t event){
	/* Geist Snatch	|2|U|U
	 * Instant
	 * Counter target creature spell. Put a 1/1 |Sblue Spirit creature token with flying onto the battlefield. */

	if( event != EVENT_RESOLVE_SPELL ){
		return card_remove_soul(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		counterspell_target_definition(player, card, &td, TYPE_CREATURE);
		if( counterspell_validate(player, card, &td, 0) ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SPIRIT, &token);
			token.key_plus = KEYWORD_FLYING;
			token.color_forced = COLOR_TEST_BLUE;
			generate_token(&token);
			return card_remove_soul(player, card, event);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_ghostform(int player, int card, event_t event){
/*
Ghostform |1|U
Sorcery
Up to two target creatures can't be blocked this turn.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.allow_cancel = 2; // "Done" button

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST){
		return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			new_pick_target(&td, "TARGET_CREATURE", 1, 0);
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card,
										0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_ghostly_flicker(int player, int card, event_t event){
/* CARD_ID_GHOSTLY_FLICKER	12378
Ghostly Flicker	|2|U
Instant
Exile two target artifacts, creatures, and/or lands you control, then return those cards to the battlefield under your control. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_CREATURE | TYPE_LAND);
	td.allowed_controller = td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 2, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "Select target artifact, creature, or land you control.", 0, 1 | GS_LITERAL_PROMPT) ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			new_pick_target(&td, "Select target artifact, creature, or land you control.", 1, 1 | GS_LITERAL_PROMPT);
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				blink_effect_controller(instance->targets[i].player, instance->targets[i].card, 0);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_ghostly_touch(int player, int card, event_t event)
{
/*
Ghostly Touch |1|U
Enchantment — Aura
Enchant creature
Enchanted creature has "Whenever this creature attacks, you may tap or untap target permanent."
*/

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( ! is_humiliated(player, card) ){
			if( declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), p, c) ){
				target_definition_t td;
				default_target_definition(p, c, &td, TYPE_PERMANENT);

				get_card_instance(p, c)->number_of_targets = 0;

				if( pick_target(&td, "TARGET_PERMANENT") ){
					ai_modifier_twiddle(p, c, 0);
					twiddle(p, c, 0);
				}
			}
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

// Gryff Vanguard (name unconfirmed) --> Wall of Blossoms

int card_havengul_skaab(int player, int card, event_t event)
{
/* CARD_ID_HAVENGUL_SKAAB	12393
Havengul Skaab	|5|U
Creature - Zombie Horror 4/5
Whenever ~ attacks, return another creature you control to its owner's hand. */

  if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.preferred_controller = player;
	  td.allowed_controller = player;
	  td.illegal_abilities = 0;
	  td.allow_cancel = 0;
	  td.special = TARGET_SPECIAL_NOT_ME;

	  if (can_target(&td) && pick_target(&td, "TARGET_ANOTHER_CREATURE_CONTROL"))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

  return 0;
}

static int effect_infinite_reflection(int player, int card, event_t event)
{
	if( event == EVENT_CHANGE_TYPE &&
		!(land_can_be_played & LCBP_DURING_EVENT_CHANGE_TYPE_SECOND_PASS) &&
		check_special_flags2(affected_card_controller, affected_card, SF2_INFINITE_REFLECTION))
	{
		card_instance_t* instance = get_card_instance(affected_card_controller, affected_card);
		if( instance->targets[12].card > 0 ){
			event_result = instance->targets[12].card;
		}
	}

	return 0;
}

int card_infinite_reflection(int player, int card, event_t event)
{
  /* Infinite Reflection	|5|U
   * Enchantment - Aura
   * Enchant creature
   * When ~ enters the battlefield attached to a creature, each other nontoken creature you control becomes a copy of that creature.
   * Nontoken creatures you control enter the battlefield as a copy of enchanted creature. */

	if( comes_into_play(player, card, event) && ! is_humiliated(player, card) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		if (instance->damage_target_card < 0 || !in_play(instance->damage_target_player, instance->damage_target_card))
			return 0;

		card_instance_t* inst;
		int c;
		int last = -1;
		int legacy_found = -1;
		for (c = 0; c < active_cards_count[player]; ++c){
			if( (inst = in_play(player, c)) ){
				if( is_what(player, c, TYPE_CREATURE) && !is_token(player, c) &&
					!(instance->damage_target_card == c && instance->damage_target_player == player))
				{
					set_special_flags2(player, c, SF2_INFINITE_REFLECTION);
					cloning(player, c, instance->damage_target_player, instance->damage_target_card);
					last = c;
				}
				if( is_what(player, c, TYPE_EFFECT) && get_card_instance(player, card)->info_slot == (int)effect_infinite_reflection ){
					legacy_found = c;
				}
			}
		}

		if( legacy_found == -1 ){
			legacy_found = create_legacy_effect(player, card, effect_infinite_reflection);
		}
		add_status(player, legacy_found, STATUS_INVISIBLE_FX);

		if (last != -1)
		{
			recalculate_all_cards_in_play();
			int singularity = 0;
			if( (is_legendary(player, last) || (singularity = check_battlefield_for_id(ANYBODY, CARD_ID_LEYLINE_OF_SINGULARITY))) &&
				!check_battlefield_for_id(ANYBODY, CARD_ID_MIRROR_GALLERY) )
			{
				for (c = 0; c < active_cards_count[player]; ++c){
					if( in_play(player, c) && is_what(player, c, TYPE_CREATURE) &&
						(is_legendary(player, c) || (singularity && !is_what(player, c, TYPE_LAND))) )
					{
						true_verify_legend_rule(player, c, get_id(player, c));
					}
				}
			}
		}
	}

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( event == EVENT_CAST_SPELL && ! affect_me(player, card) && affected_card_controller == player &&
				! is_token(affected_card_controller, affected_card) && is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
			{
				cloning(affected_card_controller, affected_card, instance->damage_target_player, instance->damage_target_card);
				set_special_flags2(affected_card_controller, affected_card, SF2_INFINITE_REFLECTION);
				get_card_instance(affected_card_controller, affected_card)->regen_status |= KEYWORD_RECALC_ALL;
				get_abilities(affected_card_controller, affected_card, EVENT_CHANGE_TYPE, -1);
				get_abilities(affected_card_controller, affected_card, EVENT_SET_COLOR, -1);
				get_abilities(affected_card_controller, affected_card, EVENT_POWER, -1);
				get_abilities(affected_card_controller, affected_card, EVENT_TOUGHNESS, -1);
			}
		}
	}

	return vanilla_aura(player, card, event, player);
}

int card_into_the_void(int player, int card, event_t event){
/* CARD_ID_INTO_THE_VOID	12404
Into the Void	|3|U
Sorcery
Return up to two target creatures to their owners' hands. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.allow_cancel = 2; // "Done" button

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST){
		return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			new_pick_target(&td, "TARGET_CREATURE", 1, 0);
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				action_on_target(player, card, i, ACT_BOUNCE);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_latch_seeker(int player, int card, event_t event){

	/* Blighted Agent	|1|U
	 * Creature - Human Rogue 1/1
	 * Infect
	 * ~ is unblockable. */

	/* Covert Operative	|4|U
	 * Creature - Human Wizard 3/2
	 * ~ is unblockable. */

	/* Jhessian Infiltrator	|G|U
	 * Creature - Human Rogue 2/2
	 * ~ is unblockable. */

	/* Latch Seeker	|1|U|U
	 * Creature - Spirit 3/1
	 * ~ is unblockable. */

	/* Metathran Soldier	|1|U
	 * Creature - Metathran Soldier 1/1
	 * ~ is unblockable. */

	/* Phantom Warrior	|1|U|U
	 * Creature - Illusion Warrior 2/2
	 * ~ is unblockable. */

	/* Plasma Elemental	|5|U
	 * Creature - Elemental 4/1
	 * ~ is unblockable. */

	/* Slither Blade	|U
	 * Creature - Naga Rogue 1/2
	 * ~ can't be blocked. */

	/* Talas Warrior	|1|U|U
	 * Creature - Human Pirate Warrior 2/2
	 * ~ is unblockable. */

	/* Tidal Kraken	|5|U|U|U
	 * Creature - Kraken 6/6
	 * ~ is unblockable. */

	/* Triton Shorestalker	|U
	 * Creature - Merfolk Rogue 1/1
	 * ~ can't be blocked. */

	 /* Unimpeded Trespasser	(Transforms from Univited Geist)
	  * Creature - Spirit 3/3
	  * ~ can't be blocked. */

	unblockable(player, card, event);

	return 0;
}

int card_lone_revenant(int player, int card, event_t event){
/* CARD_ID_LONE_REVENANT	12414
Lone Revenant	|3|U|U
Creature - Spirit 4/4
Hexproof (This creature can't be the target of spells or abilities your opponents control.)
Whenever ~ deals combat damage to a player, if you control no other creatures, look at the top four cards of your library.
Put one of them into your hand and the rest on the bottom of your library in any order. */

	hexproof(player, card, event);

	if (creature_count[player] == 1 && has_combat_damage_been_inflicted_to_a_player(player, card, event) ){
		impulse_effect(player, 4, 0);
	}

	return 0;
}

int card_lunar_mystic(int player, int card, event_t event){
/* CARD_ID_LUNAR_MYSTIC	12415
Lunar Mystic	|2|U|U
Creature - Human Wizard 2/2
Whenever you cast an instant spell, you may pay |1. If you do, draw a card. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_INSTANT | TYPE_INTERRUPT);

		if( has_mana(player, COLOR_ANY, 1) && new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test) ){
			charge_mana(player, COLOR_COLORLESS, 1);
			if( spell_fizzled != 1 ){
				draw_cards(player, 1);
			}
		}
	}

	return 0;
}

int card_mass_appeal(int player, int card, event_t event){
/* CARD_ID_MASS_APPEAL	12421
Mass Appeal	|2|U
Sorcery
Draw a card for each Human you control. */

	if(event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, count_subtype(player, TYPE_PERMANENT, SUBTYPE_HUMAN));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

// mist raven --> Man'o'War

// Mysthollow Griffin --> skipped

int card_nephalia_smugglers(int player, int card, event_t event){
/*
Nephalia Smuggler |U
Creature — Human Rogue 1/1, U (1)
{3}{U}, {T}: Exile another target creature you control, then return that card to the battlefield under your control.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_NOT_ME;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			blink_effect_controller(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT | GAA_UNTAPPED, MANACOST_XU(3, 1), 0,
									&td, "Select another target creature you control.");
}

int card_outwit(int player, int card, event_t event){
/* CARD_ID_OUTWIT	12438
Outwit	|U
Instant
Counter target spell that targets a player. */

	if( event != EVENT_CAN_CAST ){
		return card_counterspell(player, card, event);
	}

	if( event == EVENT_CAN_CAST ){
		int result = card_counterspell(player, card, event);
		if( result > 0 ){
			card_instance_t *instance = get_card_instance(card_on_stack_controller, card_on_stack);
			int i;
			for(i=0; i<instance->number_of_targets; i++){
				if( instance->targets[i].player > -1 && instance->targets[i].card == -1 ){
					return result;
				}
			}
		}
	}

	return 0;
}

int card_peel_from_reality(int player, int card, event_t event){
/* CARD_ID_PEEL_FROM_REALITY	7920
Peel from Reality	|1|U
Instant
Return target creature you control and target creature you don't control to their owners' hands. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.allowed_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.preferred_controller = 1-player;
	td2.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td2) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "Select target creature you control.", 0, 1 | GS_LITERAL_PROMPT) ){
			new_pick_target(&td2, "Select target creature you don't control.", 1, 1 | GS_LITERAL_PROMPT);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		if( validate_target(player, card, &td2, 1) ){
			bounce_permanent(instance->targets[1].player, instance->targets[1].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_rotcrown_ghoul(int player, int card, event_t event){
/* CARD_ID_ROTCROWN_GHOUL	12454
Rotcrown Ghoul	|4|U
Creature - Zombie 3/3
When ~ dies, target player puts the top five cards of his or her library into his or her graveyard. */

	if( this_dies_trigger(player, card, event, 2) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) ){
			if( new_pick_target(&td, "TARGET_PLAYER", 1, 0) ){
				mill(instance->targets[1].player, 5);
			}
		}
	}

	return 0;
}

// Scrapskin Drake --> Cloud Sprite

int card_card_second_guess(int player, int card, event_t event){
/* CARD_ID_SECOND_GUESS	12461
Second Guess	|1|U
Instant
Counter target spell that's the second spell cast this turn. */

	if( event != EVENT_CAN_CAST ){
		return card_counterspell(player, card, event);
	}

	if( event == EVENT_CAN_CAST ){
		if( get_storm_count() == 2 ){
			return card_counterspell(player, card, event);
		}
	}

	return 0;
}

int card_spectral_prison(int player, int card, event_t event)
{
/* CARD_ID_SPECTRAL_PRISON	12474
Spectral Prison	|1|U
Enchantment - Aura
Enchant creature
Enchanted creature doesn't untap during its controller's untap step.
When enchanted creature becomes the target of a spell, sacrifice ~. */

  if (kill_attachment_when_creature_is_targeted_by_spell(player, card, event, KILL_SACRIFICE))
	return 0;

  card_instance_t* instance = get_card_instance(player, card);
  if (instance->damage_target_card > -1 && in_play(player, card))
	does_not_untap(instance->damage_target_player, instance->damage_target_card, event);

  return vanilla_aura(player, card, event, 1-player);
}

int card_spirit_away(int player, int card, event_t event){
/* CARD_ID_SPIRIT_AWAY	12475
Spirit Away	|5|U|U
Enchantment - Aura
Enchant creature
You control enchanted creature.
Enchanted creature gets +2/+2 and has flying. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		int p = get_card_instance(player, card)->damage_target_player;
		int c = get_card_instance(player, card)->damage_target_card;
		modify_pt_and_abilities(p, c, event, 2, 2, KEYWORD_FLYING);
	}

	return card_control_magic(player, card, event);
}

int card_stern_mentor(int player, int card, event_t event){
/* CARD_ID_STERN_MENTOR	12476
Stern Mentor	|3|U
Creature - Human Wizard 2/2
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, each of those creatures has "|T: Target player puts the top two cards of his or her library into his or her graveyard."*/

	if( is_paired(player, card) ){

		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		target_definition_t td1;
		default_target_definition(instance->targets[3].player, instance->targets[3].card, &td1, TYPE_CREATURE);
		td1.zone = TARGET_ZONE_PLAYERS;

		if( event == EVENT_CAN_ACTIVATE ){
			int result = generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
			if( result ){
				return result;
			}
			return granted_generic_activated_ability(player, card, instance->targets[3].player, instance->targets[3].card, event,
													GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
		}

		if( event == EVENT_ACTIVATE ){
			int choice = instance->info_slot = 0;
			int can_activate_me = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
			int can_activate_paired = granted_generic_activated_ability(player, card, instance->targets[3].player, instance->targets[3].card,
																		EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
			if( can_activate_me ){
				if( can_activate_paired ){
					choice = do_dialog(player, player, card, -1, -1, " Tap Stern Mentor\n Tap paired creatur\n Cancel", 0);
				}
				else{
					choice = 1;
				}
			}
			if( choice == 0 ){
				generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
				if(spell_fizzled != 1){
					instance->info_slot = 1;
				}
			}
			if( choice == 1 ){
				granted_generic_activated_ability(player, card, instance->targets[3].player, instance->targets[3].card, event,
													GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
				if(spell_fizzled != 1){
					instance->info_slot = 2;
				}
			}
			if( choice == 2 ){
				spell_fizzled = 1;
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( instance->info_slot == 1 && validate_target(player, card, &td, 0) ){
				mill(instance->targets[0].player, 2);
			}
			if( instance->info_slot == 2 && validate_target(instance->targets[3].player, instance->targets[3].card, &td1, 0) ){
				card_instance_t *bonded = get_card_instance(instance->targets[3].player, instance->targets[3].card);
				mill(bonded->targets[0].player, 2);
			}
		}
	}

	return soulbond(player, card, event);
}

int card_stolen_goods(int player, int card, event_t event){

/* Stolen Goods	|3|U
 * Sorcery
 * Target opponent exiles cards from the top of his or her library until he or she exiles a nonland card.
 * Until end of turn, you may cast that card without paying its mana cost. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td)  ){
			int t_player = instance->targets[0].player;
			int *deck = deck_ptr[t_player];
			int removed[count_deck(t_player)];
			int z=0;
			while( deck[0] != -1 ){
					removed[z] = deck[0];
					rfg_top_card_of_deck(t_player);
					if( !(cards_data[removed[z]].type & TYPE_LAND) ){
						APNAP(p, {show_deck(p, removed, z+1, "These cards were revealed", 0, 0x7375B0 );};);
						create_may_play_card_from_exile_effect(player, card, t_player, cards_data[removed[z]].id,
																MPCFE_FOR_FREE|MPCFE_UNTIL_EOT);
						break;
					}
					z++;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_tamiyo_the_moon_sage(int player, int card, event_t event){

	/* Planeswalker - Tamiyo (4)
	 * +1: Tap target permanent. It doesn't untap during its controller's next untap step.
	 * -2: Draw a card for each tapped creature target player controls.
	 * -8: You get an emblem with "You have no maximum hand size" and "Whenever a card is put into your graveyard from anywhere, you may return it to your
	 * hand." */

	if (IS_ACTIVATING(event)){

		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.zone = TARGET_ZONE_PLAYERS;

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.state = STATE_TAPPED;

		int priority_draw_target[2] = {	check_battlefield_for_special_card(player, card, 0, CBFSC_GET_COUNT, &this_test),
										check_battlefield_for_special_card(player, card, 1, CBFSC_GET_COUNT, &this_test)
		};

		enum{
			CHOICE_FROST_TITAN = 1,
			CHOICE_DRAW,
			CHOICE_EMBLEM
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Lock permanent", can_target(&td), 10, 1,
						"Draw cards = tapped creatures", can_target(&td2), 3*MAX(priority_draw_target[0], priority_draw_target[1]), -2,
						"Emblem", 1, 15, -8);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_ACTIVATE)
		{
		  instance->number_of_targets = 0;
		  switch (choice)
			{
				case CHOICE_FROST_TITAN:
					pick_target(&td, "TARGET_PERMANENT");
					break;

				case CHOICE_DRAW:
					pick_target(&td2, "TARGET_PLAYER");
					break;

			  case CHOICE_EMBLEM:
					break;
			}
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
		switch (choice)
		  {
			case CHOICE_FROST_TITAN:
				{
					if ( valid_target(&td)){
						effect_frost_titan(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
					}
				}
				break;

			case CHOICE_DRAW:
				{
					if ( valid_target(&td2)){
						draw_cards(player, check_battlefield_for_special_card(player, card, instance->targets[0].player, CBFSC_GET_COUNT, &this_test));
					}
				}
				break;

			case CHOICE_EMBLEM:
			  generate_token_by_id(player, card, CARD_ID_TAMIYOS_EMBLEM);
			  break;
		  }
	}

	return planeswalker(player, card, event, 4);
}

int card_tamiyos_emblem(int player, int card, event_t event)
{
  /* Tamiyo's Emblem	""
   * Emblem
   * You have no maximum hand size.
   * Whenever a card is put into your graveyard from anywhere, you may return it to your hand. */

  if (event == EVENT_MAX_HAND_SIZE && current_turn == player)
	event_result += 99;

  // From library
  enable_xtrigger_flags |= ENABLE_XTRIGGER_MILLED;
  if (xtrigger_condition() == XTRIGGER_MILLED && affect_me(player, card) && reason_for_trigger_controller == player
	  && trigger_cause_controller == player)
	{
	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_AI(player);

	  if (event == EVENT_RESOLVE_TRIGGER)
		{
		  if (IS_AI(player))	// Return all cards.  Hopefully there's no Black Vise-like cards on the bf.
			{
			  // Go in reverse order, to minimize repositioning.
			  int i;
			  for (i = num_cards_milled - 1; i >= 0; --i)
				{
				  int pos = find_in_graveyard_by_source(player, cards_milled[i].source, cards_milled[i].position);
				  if (pos != -1)
					from_grave_to_hand(player, pos, TUTOR_HAND);
				  cards_milled[i].position = -1;	// No longer in graveyard, so keep any other triggers from looking
				}
			}
		  else
			{
			  int iids[500], indices[500];
			  while (1)
				{
				  int i, num = 0;
				  for (i = 0; i < num_cards_milled; ++i)
					{
					  if (cards_milled[i].position != -1)	// update position
						cards_milled[i].position = find_in_graveyard_by_source(player, cards_milled[i].source, cards_milled[i].position);
					  if (cards_milled[i].position != -1)	// still in graveyard?
						{
						  iids[num] = cards_milled[i].internal_card_id;
						  indices[num] = i;
						  ++num;
						}
					}

				  if (num == 0)
					break;

				  int chosen = show_deck(player, iids, num, "Tamiyo's Emblem: choose cards to return to hand", 0, 0x7375b0);	// DIALOGBUTTONS[2] = "Done"
				  if (chosen == -1)
					break;

				  from_grave_to_hand(player, cards_milled[indices[chosen]].position, TUTOR_HAND);
				  cards_milled[indices[chosen]].position = -1;	// No longer in graveyard, so don't look for it in further iterations
				}
			}
		}
	}

  // From anywhere else
  if (when_a_card_is_put_into_graveyard_from_anywhere_but_library_do_something(player, card, event, player, RESOLVE_TRIGGER_AI(player), NULL)
	  && gy_from_anywhere_pos != -1)
	{
	  int pos = find_in_graveyard_by_source(player, gy_from_anywhere_source, gy_from_anywhere_pos);
	  if (pos != -1)	// already removed, but hadn't been recorded
		from_grave_to_hand(player, pos, TUTOR_HAND);
	  gy_from_anywhere_pos = -1;	// No longer in graveyard, so keep any other triggers from looking
	}

  return 0;
}

int card_tandem_lookout(int player, int card, event_t event){
	/* Tandem Lookout	|2|U
	 * Creature - Human Scout 2/1
	 * Soulbond
	 * As long as ~ is paired with another creature, each of those creatures has "Whenever this creature deals damage to an opponent, draw a card." */

	if( is_paired(player, card) ){
		if (event != EVENT_DEAL_DAMAGE && trigger_condition != TRIGGER_DEAL_DAMAGE){
			return 0;
		}
		card_instance_t* instance = get_card_instance(player, card);
		card_instance_t* damage = damage_being_dealt(event);
		if (damage && damage->damage_target_card == -1 && damage->damage_target_player == 1-player && !damage_is_to_planeswalker(damage)){
			int p_player = instance->targets[3].player;
			int p_card = instance->targets[3].card;
			if( (damage->damage_source_player == player && damage->damage_source_card == card) ||
				(damage->damage_source_player == p_player && damage->damage_source_card == p_card)
			  ){
				if( instance->targets[6].player < 0 ){
					instance->targets[6].player = 0;
				}
				instance->targets[6].player++;
			}
		}
		if( instance->targets[6].player > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE &&
			player == reason_for_trigger_controller && affect_me(player, card )
		  ){
			if(event == EVENT_TRIGGER){
				event_result |= 2;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					draw_cards(player, instance->targets[6].player);
					instance->targets[6].player = 0;
			}
		}
	}

	return soulbond(player, card, event);
}

int card_temporal_mastery(int player, int card, event_t event ){
	/* Temporal Mastery	|5|U|U
	 * Sorcery
	 * Take an extra turn after this one. Exile ~.
	 * Miracle |1|U */

	if( event == EVENT_RESOLVE_SPELL ){
		time_walk_effect(player, card);
		kill_card(player, card, KILL_EXILE);
	}

	return basic_spell(player, card, event);
}

int card_vanishment(int player, int card, event_t event){
/* CARD_ID_VANISHMENT	12501
Vanishment	|4|U
Instant
Put target nonland permanent on top of its owner's library.
Miracle |U (You may cast this card for its miracle cost when you draw it if it's the first card you drew this turn.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );
	td.illegal_type = TYPE_LAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nonland permanent.", 1, NULL);

}

int card_wingcrafter(int player, int card, event_t event){
/* CARD_ID_WINGCRAFTER	12510
Wingcrafter	|U
Creature - Human Wizard 1/1
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, both creatures have flying. */

	if( is_paired(player, card) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_FLYING);
		boost_paired_creature(player, card, event, 0, 0, KEYWORD_FLYING, 0);
	}

	return soulbond(player, card, event);
}

// BLACK
int card_appetite_for_brains(int player, int card, event_t event){
/* CARD_ID_APPETITE_FOR_BRAINS	12307
Appetite for Brains	|B
Sorcery
Target opponent reveals his or her hand. You choose a card from it with converted mana cost 4 or greater and exile that card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			this_test.cmc = 3;
			this_test.cmc_flag = F5_CMC_GREATER_THAN_VALUE;

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			this_definition.effect = EC_RFG;
			new_effect_coercion(&this_definition, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

static int blood_artist_effect(int player, int card, event_t event){

	//In case something is copying Pawn of Ulamog and then reverts to its original form.
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_BLOOD_ARTIST ){
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
						//Will trigger also if this is not a creature when it goes into the graveyard;
						if( ! is_what(p, c, TYPE_CREATURE) ){
							count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
						}
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
						if( instance->targets[11].player < 0 ){
							instance->targets[11].player = 0;
						}
						SET_BYTE1(instance->targets[11].player) = 1;
					}
				}
				count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			life_sucking(player, card, amount);
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

int card_blood_artist(int player, int card, event_t event){
/* CARD_ID_BLOOD_ARTIST	12316
Blood Artist	|1|B
Creature - Vampire 0/1
Whenever ~ or another creature dies, target player loses 1 life and you gain 1 life. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)blood_artist_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &blood_artist_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			life_sucking(player, card, amount);
		}
	}

	return 0;
}


// Bloodflow Connoisseur --> Scarland Thrinax

// Butcher Ghoul --> Young Wolf

int card_corpse_traders(int player, int card, event_t event){
/* CARD_ID_CORPSE_TRADERS	12333
Corpse Traders	|3|B
Creature - Human Rogue 3/3
|2|B, Sacrifice a creature: Target opponent reveals his or her hand. You choose a card from it.
That player discards that card. Activate this ability only any time you could cast a sorcery. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			new_effect_coercion(&this_definition, &this_test);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE, MANACOST_XB(2, 1), 0,
									&td, "TARGET_PLAYER");
}

int card_dark_imposter(int player, int card, event_t event ){
/*
Dark Impostor |2|B
Creature — Vampire Assassin 2/2
{4}{B}{B}: Exile target creature and put a +1/+1 counter on Dark Impostor.
Dark Impostor has all activated abilities of all creature cards exiled with it.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) ){
		int can_activate = generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XB(4, 2), 0, &td, NULL);
		if( ! can_activate ){
			int i;
			for(i=2; i<10; i++){
				int ability = instance->targets[i].card;
				if( ability > -1 ){
					CardFunction ptFunction = cards_data[ability].code_pointer;
					if( ptFunction(player, card, EVENT_CAN_ACTIVATE ) ){
						can_activate = 1;
						break;
					}
				}
			}
		}
		return can_activate;
	}

	if( event == EVENT_ACTIVATE){
		instance->info_slot = instance->number_of_targets = 0;
		int stolen_abilities[12] = {-1, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}; //For simplicity sake, let's consider 10 to be the maximum amount of the "stolen" abilities. Position '0' in this array is 'cancel' and position '1' is the main Experiment Kraj ability.
		char stolen_abilities_name[12][100];
		int i, sac = 2;
		for(i=2; i < 10; i++){
			int iid = instance->targets[i].player;
			CardFunction ptFunction = cards_data[iid].code_pointer;
			if( ptFunction(player, card, EVENT_CAN_ACTIVATE ) ){
				stolen_abilities[sac] = iid;
				strcpy(stolen_abilities_name[sac], cards_ptr[cards_data[iid].id]->name);
				sac++;
			}
		}
		int my_ability = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_XB(4, 2), 0, &td, NULL);

		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL,
						"Kill & steal ability", my_ability, 5,
						stolen_abilities_name[2], stolen_abilities[2] > -1, 1,
						stolen_abilities_name[3], stolen_abilities[3] > -1, 1,
						stolen_abilities_name[4], stolen_abilities[4] > -1, 1,
						stolen_abilities_name[5], stolen_abilities[5] > -1, 1,
						stolen_abilities_name[6], stolen_abilities[6] > -1, 1,
						stolen_abilities_name[7], stolen_abilities[7] > -1, 1,
						stolen_abilities_name[8], stolen_abilities[8] > -1, 1,
						stolen_abilities_name[9], stolen_abilities[9] > -1, 1,
						stolen_abilities_name[10], stolen_abilities[10] > -1, 1,
						stolen_abilities_name[11], stolen_abilities[11] > -1, 1);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == 1 ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XB(4, 2), 0, &td, "TARGET_CREATURE");
		}
		else{
			// pay the costs for the selected card
			int ability = instance->targets[choice].player;
			CardFunction ptFunction = cards_data[ability].code_pointer;
			ptFunction(player, card, EVENT_ACTIVATE);
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		if( choice == 1 && valid_target(&td) ){
			card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
			int iid = get_original_internal_card_id(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			add_1_1_counter(instance->parent_controller, instance->parent_card);
			int i;
			for(i=2; i<10; i++){
				if( parent->targets[i].player == -1 ){
					parent->targets[i].player = iid;
				}
			}
		}
		if( choice > 1 ){
			int iid = instance->targets[choice].player;
			CardFunction ptFunction = cards_data[iid].code_pointer;
			ptFunction(player, card, EVENT_RESOLVE_ACTIVATION);
		}
	}

	return 0;
}

int card_demonic_taskmaster(int player, int card, event_t event){
/* CARD_ID_DEMONIC_TASKMASTER	12344
Demonic Taskmaster	|2|B
Creature - Demon 4/3
Flying
At the beginning of your upkeep, sacrifice a creature other than ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select another creature to sacrifice.");
		test.not_me = 1;
		if( check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test) ){
			new_sacrifice(player, card, player, SAC_NO_CANCEL, &test);
		}
	}

	return 0;
}

int card_demonic_rising(int player, int card, event_t event){
	/* Demonic Rising	|3|B|B
	 * Enchantment
	 * At the beginning of your end step, if you control exactly one creature, put a 5/5 |Sblack Demon creature token with flying onto the battlefield. */

	if( current_turn == player && creature_count[player] == 1 && eot_trigger(player, card, event) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_DEMON, &token);
		token.pow = token.tou = 5;
		token.key_plus = KEYWORD_FLYING;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		generate_token(&token);
	}

	return global_enchantment(player, card, event);
}

int card_demonlord_of_ashmouth(int player, int card, event_t event)
{
/* CARD_ID_DEMONLORD_OF_ASHMOUTH	12345
Demonlord of Ashmouth	|2|B|B
Creature - Demon 5/4
Flying
When ~ enters the battlefield, exile it unless you sacrifice another creature.
Undying (When this creature dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.)*/

	undying(player, card, event);

	if (comes_into_play(player, card, event))
	{
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select another creature to sacrifice.");
		test.not_me = 1;

		if (!new_sacrifice(player, card, player, 0, &test))
			kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_death_wind(int player, int card, event_t event){
/* CARD_ID_DEATH_WIND	12340
Death Wind	|X|B
Instant
Target creature gets -X/-X until end of turn. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		card_instance_t* instance = get_card_instance(player, card);
		return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, -instance->info_slot, -instance->info_slot, 0, 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -instance->info_slot, -instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, "TARGET_CREATURE", 1, NULL);
}

int card_descent_into_madness(int player, int card, event_t event){
/* CARD_ID_DESCENT_INTO_MADNESS	12347
Descent into Madness	|3|B|B
Enchantment
At the beginning of your upkeep, put a despair counter on ~, then each player exiles X permanents he or she controls and/or cards from his or her hand,
where X is the number of despair counters on ~. */

	card_instance_t *instance = get_card_instance(player, card);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counter(player, card, COUNTER_DESPAIR);
		int amount = count_counters(player, card, COUNTER_DESPAIR);
		int i;
		for(i=0; i<2; i++){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_PERMANENT);
			td.allowed_controller = i;
			td.preferred_controller = i;
			td.who_chooses = i;
			td.illegal_abilities = 0;

			target_definition_t td1;
			default_target_definition(player, card, &td1, TYPE_ANY);
			td1.zone = TARGET_ZONE_HAND;
			td1.allowed_controller = i;
			td1.preferred_controller = i;
			td1.who_chooses = i;
			td1.illegal_abilities = 0;
			td1.allow_cancel = 0;

			int perms = count_subtype(i, TYPE_PERMANENT, -1);
			amount = MIN(amount, perms);

			int sacrifices = 0;
			while( sacrifices < amount ){
					instance->number_of_targets = 0;
					if( can_target(&td) ){
						td.allow_cancel = ! can_target(&td1) ? 0 : 1;
						if( new_pick_target(&td, "Select a permanent you control to exile.", 0, GS_LITERAL_PROMPT) ){
							kill_card(i, instance->targets[0].card, KILL_EXILE);
							sacrifices++;
						}
						else{
							if( can_target(&td1) ){
								td1.allow_cancel = 0;
								if( new_pick_target(&td1, "Select a card in your hand to exile.", 0, GS_LITERAL_PROMPT) ){
									rfg_card_in_hand(i, instance->targets[0].card);
									sacrifices++;
								}
							}
						}
					}
					else{
						if( can_target(&td1) ){
							td1.allow_cancel = 0;
							if( new_pick_target(&td1, "Select a card in your hand to exile.", 0, GS_LITERAL_PROMPT) ){
								rfg_card_in_hand(i, instance->targets[0].card);
								sacrifices++;
							}
						}
					}
					if( ! can_target(&td) && ! can_target(&td1) ){
						break;
					}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_driver_of_the_dead(int player, int card, event_t event){
/* CARD_ID_DRIVER_OF_THE_DEAD	12355
Driver of the Dead	|3|B
Creature - Vampire 3/2
When ~ dies, return target creature card with converted mana cost 2 or less from your graveyard to the battlefield. */

	if( this_dies_trigger(player, card, event, 2) ){
		char msg[100] = "Select a creature card with cmc 2 or less.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, msg);
		this_test.cmc = 3;
		this_test.cmc_flag = 3;
		if( new_special_count_grave(player, &this_test) ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

int card_dread_slaver(int player, int card, event_t event){
/* CARD_ID_DREAD_SLAVER	12353
Dread Slaver	|3|B|B
Creature - Zombie Horror 3/5
Whenever a creature dealt damage by ~ this turn dies, return it to the battlefield under your control.
That creature is a |Sblack Zombie in addition to its other colors and types. */

	if( sengir_vampire_trigger(player, card, event, 2) ){
		card_instance_t *instance = get_card_instance( player, card );

		int i;
		for(i=2; i<instance->targets[11].card+2; i++){
			int t_player = instance->targets[i].player;
			int id = instance->targets[i].card;
			int count = count_graveyard(t_player)-1;
			const int *grave = get_grave(t_player);
			while( count > -1 ){
					if( cards_data[grave[count]].id == id ){
						reanimate_permanent(player, card, t_player, count, REANIMATE_ADD_BLACK_ZOMBIE);
						break;
					}
					count--;
			}
		}
		instance->targets[11].card = 0;
	}

	return 0;
}

int card_essence_harvest(int player, int card, event_t event){
/* CARD_ID_ESSENCE_HARVEST	12363
Essence Harvest	|2|B
Sorcery
Target player loses X life and you gain X life, where X is the greatest power among creatures you control. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			int amount = get_highest_power(player);
			lose_life(instance->targets[0].player, amount);
			gain_life(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_evernight_shade(int player, int card, event_t event){
/* CARD_ID_EVERNIGHT_SHADE	12364
Evernight Shade	|3|B
Creature - Shade 1/1
|B: ~ gets +1/+1 until end of turn.
Undying (When this creature dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.) */

	undying(player, card, event);
	return generic_shade(player, card, event, 0, MANACOST_B(1), 1, 1, 0, 0);
}

int card_exquisite_blood(int player, int card, event_t event){
/* CARD_ID_EXQUISITE_BLOOD	12365
Exquisite Blood	|4|B
Enchantment
Whenever an opponent loses life, you gain that much life. */

	return global_enchantment(player, card, event);
}

int card_ghoulflesh(int player, int card, event_t event){
/* CARD_ID_GHOULFLESH	12380
Ghoulflesh	|B
Enchantment - Aura
Enchant creature
Enchanted creature gets -1/-1 and is a |Sblack Zombie in addition to its other colors and types. */

	return generic_aura(player, card, event, 1-player, -1, -1, 0, 0, SUBTYPE_ZOMBIE, COLOR_TEST_BLACK, 0);
}

int card_gloom_surgeon(int player, int card, event_t event)
{
/*
Gloom Surgeon |1|B
Creature — Spirit 2/1
If combat damage would be dealt to Gloom Surgeon, prevent that damage and exile that many cards from the top of your library.
*/

	card_instance_t* damage = combat_damage_being_prevented(event);
	if (damage && damage->damage_target_card == card && damage->damage_target_player == player)
	{
		int amount = damage->info_slot;
		prevent_damage(affected_card_controller, affected_card, amount);
		rfg_top_n_cards_of_deck(player, amount);
	}

	return 0;
}

int card_grave_exchange(int player, int card, event_t event){
/*
Grave Exchange |4|B|B|
Sorcery
Return target creature card from your graveyard to your hand. Target player sacrifices a creature.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	char msg[100] = "Select a creature.";
	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, msg);

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if(  count_graveyard_by_type(player, TYPE_CREATURE) > 0 && ! graveyard_has_shroud(player) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "TARGET_PLAYER", 0, 1) ){
			if( select_target_from_grave_source(player, card, player, 0, AI_MAX_VALUE, &this_test, 1) == -1 ){
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 1);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
		if( valid_target(&td) ){
	        test_definition_t this_test2;
	        new_default_test_definition(&this_test2, TYPE_CREATURE, "Select a creature to sacrifice.");
			new_sacrifice(player, card, instance->targets[0].player, SAC_CAUSED | SAC_NO_CANCEL, &this_test2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_griselbrand(int player, int card, event_t event){
/* CARD_ID_GRISELBRAND	12386
Griselbrand	|4|B|B|B|B
Legendary Creature - Demon 7/7
Flying, lifelink
Pay 7 life: Draw seven cards. */

	check_legend_rule(player, card, event);

	lifelink(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 7);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, 7, NULL, NULL);
}

static int harvester_of_souls_legacy(int player, int card, event_t event){

	//In case something is copying Demon of Dark Schemes and then reverts to its original form.
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_HARVESTER_OF_SOULS ){
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
						return 0;
					}
				}
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				this_test.type_flag = F1_NO_TOKEN;
				this_test.not_me = 1;
				count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			draw_some_cards_if_you_want(player, card, player, amount);
		}
		kill_card(player, card, KILL_EXILE);
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

int card_harvester_of_souls(int player, int card, event_t event){
/*
Harvester of Souls |4|B|B
Creature — Demon 5/5
Deathtouch
Whenever another nontoken creature dies, you may draw a card.
*/
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)harvester_of_souls_legacy ){
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
			int legacy = create_targetted_legacy_effect(player, card, &harvester_of_souls_legacy, player, card);
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
		this_test.type_flag = F1_NO_TOKEN;
		this_test.not_me = 1;
		count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( result ){
			int amount = BYTE0(result);
			draw_some_cards_if_you_want(player, card, player, amount);
		}
	}

	deathtouch(player, card, event);

	return 0;
}

int card_homicidal_seclusion(int player, int card, event_t event)
{
/* CARD_ID_HOMICIDAL_SECLUSION	12398
Homicidal Seclusion	|4|B
Enchantment
As long as you control exactly one creature, that creature gets +3/+1 and has lifelink. */

  if ((event == EVENT_ABILITIES || event == EVENT_POWER || event == EVENT_TOUGHNESS)
	  && affected_card_controller == player
	  && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
	  && count_permanents_by_type(player, TYPE_CREATURE) == 1
	  && in_play(player, card) && !is_humiliated(player, card))
	{
	  if (event == EVENT_ABILITIES)
		lifelink(affected_card_controller, affected_card, event);
	  else if (event == EVENT_POWER)
		event_result += 3;
	  else	// event == EVENT_TOUGHNESS
		event_result += 1;
	}

  return global_enchantment(player, card, event);
}

int card_hunted_ghoul(int player, int card, event_t event){
/* CARD_ID_HUNTED_GHOUL	12402
Hunted Ghoul	|B
Creature - Zombie 1/2
~ can't block Humans. */

	if( event == EVENT_BLOCK_LEGALITY && affect_me(player, card) && !is_humiliated(player, card) ){
		if( has_subtype(attacking_card_controller, attacking_card, SUBTYPE_HUMAN) ){
			event_result = 1;
		}
	}
	return 0;
}

int card_human_frailty(int player, int card, event_t event){
/* CARD_ID_HUMAN_FRAILTY	12401
Human Frailty	|B
Instant
Destroy target Human creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_HUMAN;

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target Human creature.", 1, NULL);
}

int card_killing_wave(int player, int card, event_t event){
/* CARD_ID_KILLING_WAVE	12407
Killing Wave	|X|B
Sorcery
For each creature, its controller sacrifices it unless he or she pays X life. */

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = instance->info_slot;
		APNAP(p,{
					int cpl = can_pay_life(p, amount);
					if( can_sacrifice(player, p, 1, TYPE_CREATURE, 0) ){
						int count = active_cards_count[p]-1;
						while( count > -1 ){
								if( in_play(p, count) && is_what(p, count, TYPE_CREATURE) ){
									int choice = 1;
									if( cpl && life[p]>= amount ){
										int ai_choice = 0;
										if( life[p]-amount < 6 ){
											ai_choice = 1;
										}
										char buffer[100];
										snprintf(buffer, 100, " Pay %d life\n Sacrifice this", amount );
										choice = do_dialog(p, p, count, -1, -1, buffer, ai_choice);
									}
									if( choice == 0 ){
										lose_life(p, amount);
									}
									else{
										kill_card(p, count, KILL_SACRIFICE);
									}
								}
								count--;
						}
					};
				};
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_maalfeld_twins(int player, int card, event_t event){
	/* Maalfeld Twins	|5|B
	 * Creature - Zombie 4/4
	 * When ~ dies, put two 2/2 |Sblack Zombie creature tokens onto the battlefield. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		generate_tokens_by_id(player, card, CARD_ID_ZOMBIE, 2);
	}

	return 0;
}

int card_marrow_bats(int player, int card, event_t event){
/*
Marrow Bats |4|B
Creature — Bat Skeleton 4/1
Flying
Pay 4 life: Regenerate Marrow Bats.
*/

	if( event == EVENT_CAN_ACTIVATE && can_pay_life(player, 4) ){
		return regeneration(player, card, event, MANACOST0);
	}

	if( event == EVENT_ACTIVATE ){
		regeneration(player, card, event, MANACOST0);
		if( spell_fizzled != 1 ){
			lose_life(player, 4);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return regeneration(player, card, event, MANACOST0);
	}

	return 0;
}

int card_mental_agony(int player, int card, event_t event){
/* CARD_ID_MENTAL_AGONY	12422
Mental Agony	|3|B
Sorcery
Target player discards two cards and loses 2 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			new_multidiscard(instance->targets[0].player, 2, 0, player);
			lose_life(instance->targets[0].player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_necrobite(int player, int card, event_t event)
{
	/* Necrobite	|2|B
	 * Instant
	 * Target creature gains deathtouch until end of turn. Regenerate it. */
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 0,0, VANILLA_PUMP_REGENERATE,SP_KEYWORD_DEATHTOUCH);
}

int card_polluted_dead(int player, int card, event_t event){
/* CARD_ID_POLLUTED_DEAD	12441
Polluted Dead	|4|B
Creature - Zombie 3/3
When ~ dies, destroy target land. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_LAND") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_predators_gambit(int player, int card, event_t event)
{
  /* Predator's Gambit	|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+1.
   * Enchanted creature has intimidate as long as its controller controls no other creatures. */

  card_instance_t* inst;
  if (event == EVENT_ABILITIES
	  && (inst = in_play(player, card)) && affect_me(inst->damage_target_player, inst->damage_target_card)
	  && count_permanents_by_type(inst->damage_target_player, TYPE_CREATURE) == 1)
	intimidate(inst->damage_target_player, inst->damage_target_card, event);

  return generic_aura(player, card, event, player, 2, 1, 0, 0, 0, 0, 0);
}

// renegade demon --> vanilla

int card_searchlight_geist(int player, int card, event_t event){
/* CARD_ID_SEARCHLIGHT_GEIST	12460
Searchlight Geist	|2|B
Creature - Spirit 2/1
Flying
|3|B: ~ gains deathtouch until end of turn. (Any amount of damage it deals to a creature is enough to destroy it.) */

	return generic_shade(player, card, event, 0, MANACOST_XB(3, 1), 0, 0, 0, SP_KEYWORD_DEATHTOUCH);
}

int card_soulcage_fiend(int player, int card, event_t event){
/* CARD_ID_SOULCAGE_FIEND	12472
Soulcage Fiend	|1|B|B
Creature - Demon 3/2
When ~ dies, each player loses 3 life. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		lose_life(player, 3);
		lose_life(1-player, 3);
	}

	return 0;
}

int card_treacherous_pit_dweller(int player, int card, event_t event){
	/*
	  Treacherous Pit-Dweller |B|B
	  Creature - Demon 4/3
	  When Treacherous Pit-Dweller enters the battlefield from a graveyard, target opponent gains control of it.
	  Undying (When this creature dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.)
	*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && reason_for_trigger_controller == player ){
		if( check_special_flags3(trigger_cause_controller, trigger_cause, SF3_REANIMATED) ){
			etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		}
	}

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;
		if( would_validate_arbitrary_target(&td, 1-player, -1) ){
			give_control_of_self(player, card);
		}
	}

	return undying(player, card, event);
}

int card_triumph_of_cruelty(int player, int card, event_t event){
/* CARD_ID_TRIUMPH_OF_CRUELTY	12492
Triumph of Cruelty	|2|B
Enchantment
At the beginning of your upkeep, target opponent discards a card if you control the creature with the greatest power or tied for the greatest power. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( get_highest_power(player) > -1 && get_highest_power(player) >= get_highest_power(1-player) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.zone = TARGET_ZONE_PLAYERS;

			if( would_validate_arbitrary_target(&td, 1-player, -1) ){
				discard(1-player, 0, player);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_undead_executioner(int player, int card, event_t event){
/* CARD_ID_UNDEAD_EXECUTIONER	12498
Undead Executioner	|3|B
Creature - Zombie 2/2
When ~ dies, you may have target creature get -2/-2 until end of turn. */

	if( this_dies_trigger(player, card, event, 2) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -2, -2);
		}
	}

	return 0;
}

static int unhallowed_pact_effect(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		seek_grave_for_id_to_reanimate(player, card, instance->targets[0].player, cards_data[instance->targets[0].card].id,
										REANIMATE_DEFAULT);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_unhallowed_pact(int player, int card, event_t event){
/* CARD_ID_UNHALLOWED_PACT	12499
Unhallowed Pact	|2|B
Enchantment - Aura
Enchant creature
When enchanted creature dies, return that card to the battlefield under your control. */

	card_instance_t *instance = get_card_instance(player, card);

	if( in_play(player, card) && instance->damage_target_player != -1 ){
		if(event == EVENT_GRAVEYARD_FROM_PLAY){
			if( affect_me( instance->damage_target_player, instance->damage_target_card) ){
				if( is_this_dying(instance->damage_target_player, instance->damage_target_card) ){
					int legacy = create_legacy_effect(player, card, &unhallowed_pact_effect);
					card_instance_t *leg = get_card_instance(player, legacy);
					leg->targets[0].player = get_owner(instance->damage_target_player, instance->damage_target_card);
					leg->targets[0].card = get_original_internal_card_id(instance->damage_target_player, instance->damage_target_card);
					leg->targets[11].player = 1; // For "resolve_gfp_ability"
				}
			}
		}
	}


	return vanilla_aura(player, card, event, player);
}

//RED
// Archwing Dragon --> Viashivan Sandstalker

int card_aggravate(int player, int card, event_t event){
/* CARD_ID_AGGRAVATE	12299
Aggravate	|3|R|R
Instant
~ deals 1 damage to each creature target player controls. Each creature dealt damage this way attacks this turn if able. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			int c;
			for(c=active_cards_count[instance->targets[0].player]-1; c > -1; c--){
				if( in_play(instance->targets[0].player, c) && is_what(instance->targets[0].player, c, TYPE_CREATURE) ){
					int dc = damage_creature(instance->targets[0].player, c, 1, player, card);
					get_card_instance(player, dc)->targets[3].card |= DMG_MUST_ATTACK_IF_DEALT_DAMAGE_THIS_WAY;
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_banners_rised(int player, int card, event_t event){
/* CARD_ID_BANNERS_RAISED	12312
Banners Raised	|R
Instant
Creatures you control get +1/+0 until end of turn. */

	if(event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 1, 0, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_battle_hymn(int player, int card, event_t event){
/* CARD_ID_BATTLE_HYMN	12313
Battle Hymn	|1|R
Instant
Add |R to your mana pool for each creature you control. */

	if(event == EVENT_RESOLVE_SPELL ){
		produce_mana(player, COLOR_RED, count_permanents_by_type(player, TYPE_CREATURE));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_bonfire_of_the_damned(int player, int card, event_t event){
	/* Bonfire of the Damned	|X|X|R	0x2005b42
	 * Sorcery
	 * ~ deals X damage to target player or planeswalker and each creature that player or that planeswalker's controller controls.
	 * Miracle |X|R */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			get_card_instance(player, card)->info_slot = 0;
			if( check_special_flags2(player, card, SF2_MIRACLE) ){
				charge_mana(player, COLOR_COLORLESS, -1);
				if( spell_fizzled != 1 ){
					set_x_for_x_spells(player, card, event, x_value);
				}
			}
			else{
				if( ! played_for_free(player, card) && ! is_token(player, card) ){
					int amount = charge_mana_for_double_x(player, COLOR_COLORLESS);
					set_x_for_x_spells(player, card, event, amount/2);
				}
			}
			pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER");
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
			new_damage_all(player, card, instance->targets[0].player, instance->info_slot, NDA_ALL_CREATURES, NULL);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_burn_at_the_stake(int player, int card, event_t event){
	/* Burn at the Stake	|2|R|R|R	0x2005c87
	 * Sorcery
	 * As an additional cost to cast this spell, tap any number of untapped creatures you control.
	 * ~ deals damage to any target equal to three times the number of creatures tapped this way. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller = player;
	td1.preferred_controller = player;
	td1.illegal_abilities = 0;
	td1.illegal_state = TARGET_STATE_TAPPED;
	td1.allow_cancel = 2; //"Done" button

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int to_tap[10];
		int ttc = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			while( ttc < 10 && can_target(&td1) ){
					instance->number_of_targets = 0;
					if( new_pick_target(&td1, "Select a creature you control to tap.", 0, GS_LITERAL_PROMPT) ){
						to_tap[ttc] = instance->targets[0].card;
						ttc++;
						state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
					}
					else{
						break;
					}
			}
		}

		instance->number_of_targets = 0;
		pick_target(&td, "TARGET_ANY");

		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int i;
			for(i=0; i<ttc; i++){
				state_untargettable(player, to_tap[i], 0);
				if( spell_fizzled != 1 ){
					tap_card(player, to_tap[i]);
					instance->info_slot += 3;
				}
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_dangerous_wager(int player, int card, event_t event){
/* CARD_ID_DANGEROUS_WAGER	12337
Dangerous Wager	|1|R
Instant
Discard your hand, then draw two cards. */

	if(event == EVENT_RESOLVE_SPELL ){
		discard_all(player);
		draw_cards(player, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_dual_casting(int player, int card, event_t event){
/*
Dual Casting |1|R
Enchantment — Aura
Enchant creature
Enchanted creature has "{R}, {T}: Copy target instant or sorcery spell you control. You may choose new targets for the copy."
*/
	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		card_instance_t *trg = get_card_instance(t_player, t_card);

		target_definition_t td_fork;
		counterspell_target_definition(t_player, t_card, &td_fork, TYPE_INSTANT|TYPE_SORCERY);
		td_fork.allowed_controller = td_fork.preferred_controller = t_player;

		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
			return generic_activated_ability(t_player, t_card, event, GAA_UNTAPPED | GAA_SPELL_ON_STACK, MANACOST_R(1), 0, &td_fork, NULL);
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( validate_target(t_player, t_card, &td_fork, 0) ){
				copy_spell_from_stack(player, trg->targets[0].player, trg->targets[0].card);
			}
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target creature you control");
}

int card_falkenrath_exterminator(int player, int card, event_t event){
/* CARD_ID_FALKENRATH_EXTERMINATOR	12366
Falkenrath Exterminator	|1|R
Creature - Vampire Archer 1/1
Whenever ~ deals combat damage to a player, put a +1/+1 counter on it.
|2|R: ~ deals damage to target creature equal to the number of +1/+1 counters on ~. */

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(2, 1), 0, &td, NULL);
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				damage_target0(player, card, count_1_1_counters(instance->parent_controller, instance->parent_card));
			}
		}
	}

	return card_slith_predator(player, card, event);
}

int card_fervent_cathar(int player, int card, event_t event){
/* CARD_ID_FERVENT_CATHAR	12369
Fervent Cathar	|2|R
Creature - Human Knight 2/1
Haste
When ~ enters the battlefield, target creature can't block this turn. */

	haste(player, card, event);
	return card_goblin_shortcutter(player, card, event);
}

int card_gang_of_devils(int player, int card, event_t event){
	/* Gang of Devils	|5|R	0x2005c8c
	 * Creature - Devil 3/3
	 * When ~ dies, it deals 3 damage divided as you choose among one, two, or three targets. */
	if( this_dies_trigger(player, card, event, 2) ){
		target_and_divide_damage(player, card, NULL, NULL, 3);
	}

	return 0;
}

int card_guise_of_fire(int player, int card, event_t event){
/* CARD_ID_GUISE_OF_FIRE	12389
Guise of Fire	|R
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/-1 and attacks each turn if able. */

	return generic_aura(player, card, event, player, 1, -1, 0, SP_KEYWORD_MUST_ATTACK, 0, 0, 0);
}

int card_hanveir_lancer(int player, int card, event_t event){
/* CARD_ID_HANWEIR_LANCER	12390
Hanweir Lancer	|2|R
Creature - Human Knight 2/2
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, both creatures have first strike. */

	if( is_paired(player, card) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_FIRST_STRIKE);
		boost_paired_creature(player, card, event, 0, 0, KEYWORD_FIRST_STRIKE, 0);
	}
	return soulbond(player, card, event);
}

int card_havengul_vampire(int player, int card, event_t event){
/* CARD_ID_HAVENGUL_VAMPIRE	12394
Havengul Vampire	|3|R
Creature - Vampire 2/2
Whenever ~ deals combat damage to a player, put a +1/+1 counter on it.
Whenever another creature dies, put a +1/+1 counter on ~. */

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		if( ! affect_me(player, card) ){
			count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, 0);
		}
	}

	int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( amount ){
		add_1_1_counters(player, card, amount);
		get_card_instance(player, card)->targets[11].card = 0;
	}

	return card_slith_predator(player, card, event);
}

int card_heirs_of_stromkirk(int player, int card, event_t event){
/* CARD_ID_HEIRS_OF_STROMKIRK	12395
Heirs of Stromkirk	|2|R|R
Creature - Vampire 2/2
Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)
Whenever ~ deals combat damage to a player, put a +1/+1 counter on it. */

	intimidate(player, card, event);
	return card_slith_predator(player, card, event);
}

// Hound of Griselbrand --> Young Wolf

int card_kessig_malcontents(int player, int card, event_t event, int amount){
	/* Kessig Malcontents	|2|R	0x2005c96
	 * Creature - Human Warrior 3/1
	 * When ~ enters the battlefield, it deals damage to target player or planeswalker equal to the number of Humans you control. */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TARGET_TYPE_PLANESWALKER);
		td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td1.allow_cancel = 0;

		if( can_target(&td1) && pick_target(&td1, "TARGET_PLAYER_OR_PLANESWALKER") ){
			damage_target0(player, card, count_subtype(player, TYPE_PERMANENT, SUBTYPE_HUMAN));
		}
	}

	return 0;
}

int card_kruin_striker(int player, int card, event_t event)
{
/* CARD_ID_KRUIN_STRIKER	12408
Kruin Striker	|1|R
Creature - Human Warrior 2/1
Whenever another creature enters the battlefield under your control, ~ gets +1/+0 and gains trample until end of turn. */

	if (trigger_condition == TRIGGER_COMES_INTO_PLAY)
	{
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "");
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		pump_ability_until_eot(player, card, player, card, 1, 0, KEYWORD_TRAMPLE, 0);
	}

	return 0;
}

int card_lightning_mauler(int player, int card, event_t event){
/* CARD_ID_LIGHTNING_MAULER	12412
Lightning Mauler	|1|R
Creature - Human Berserker 2/1
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, both creatures have haste. */

	if( is_paired(player, card) ){
		haste(player, card, event);
		boost_paired_creature(player, card, event, 0, 0, 0, SP_KEYWORD_HASTE);
	}

	return soulbond(player, card, event);
}

int card_lightning_prowess(int player, int card, event_t event){
	/* Lightning Prowess	|2|R	0x2003900
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature has haste and "|T: This creature deals 1 damage to any target." */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t* instance = get_card_instance(player, card);
		if( event == EVENT_ABILITIES && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			haste(instance->damage_target_player, instance->damage_target_card, event);
		}
	}

	return card_hermetic_study(player, card, event);
}

int card_mad_prophet(int player, int card, event_t event){
/* CARD_ID_MAD_PROPHET	12417
Mad Prophet	|3|R
Creature - Human Shaman 2/2
Haste
|T, Discard a card: Draw a card. */

	haste(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD | GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_malicious_intent(int player, int card, event_t event){
/*
Malicious Intent |1|R
Enchantment — Aura
Enchant creature
Enchanted creature has "{T}: Target creature can't block this turn."
*/
	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		card_instance_t *trg = get_card_instance(t_player, t_card);

		target_definition_t td;
		default_target_definition(t_player, t_card, &td, TYPE_CREATURE);

		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
			return generic_activated_ability(t_player, t_card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( validate_target(t_player, t_card, &td, 0) ){
				pump_ability_until_eot(instance->parent_controller, instance->parent_card, trg->targets[0].player, trg->targets[0].card,
										0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
			}
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target creature you control");
}

int card_malignus(int player, int card, event_t event){
	/* Malignus	|3|R|R
	 * Creature - Elemental Spirit 100/100
	 * ~'s power and toughness are each equal to half the highest life total among your opponents, rounded up.
	 * Damage that would be dealt by ~ can't be prevented. */

	my_damage_cannot_be_prevented(player, card, event);

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && !is_humiliated(player, card) && player != -1){
		event_result += (life[1-player] + 1) / 2;
	}

	return 0;
}

// raging poltergeist --> vanilla

// Reforge the soul -> Wheel of Fortune

int card_riot_ringleader(int player, int card, event_t event)
{
/* CARD_ID_RIOT_RINGLEADER	12452
Riot Ringleader	|2|R
Creature - Human Warrior 2/2
Whenever ~ attacks, Human creatures you control get +1/+0 until end of turn. */

	if ( declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		pump_subtype_until_eot(player, card, player, SUBTYPE_HUMAN, 1, 0, 0, 0);
	}

	return 0;
}

int card_rush_of_blood(int player, int card, event_t event){
/* CARD_ID_RUSH_OF_BLOOD	12455
Rush of Blood	|2|R
Instant
Target creature gets +X/+0 until end of turn, where X is its power. */

	if ( event == EVENT_CHECK_PUMP )
	{
		card_instance_t *instance = get_card_instance(player, card);
		if (!has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id))
			return 0;

		if( instance->number_of_targets == 1 && instance->targets[0].player > -1 && instance->targets[0].card > -1 ){
			pumpable_power[player] += get_power(instance->targets[0].player, instance->targets[0].card);
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
							get_power(instance->targets[0].player, instance->targets[0].card), 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_rite_of_ruin(int player, int card, event_t event){
/* CARD_ID_RITE_OF_RUIN	12453
Rite of Ruin	|5|R|R
Sorcery
Choose an order for artifacts, creatures, and lands. Each player sacrifices one permanent of the first type,
sacrifices two of the second type, then sacrifices three of the third type. */

	if(event == EVENT_RESOLVE_SPELL ){
		int types[3] = {TYPE_LAND, TYPE_ARTIFACT, TYPE_CREATURE};
		int chosen[3];
		int mode = 0;
		int cc;
		for( cc=0; cc<3; cc++){
			int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_OMIT_ILLEGAL,
						"Land", !(mode & (1<<0)), (count_subtype(player, TYPE_LAND, -1)-count_subtype(1-player, TYPE_LAND, -1))*5,
						"Artifact", !(mode & (1<<1)), (count_subtype(player, TYPE_ARTIFACT, -1)-count_subtype(1-player, TYPE_ARTIFACT, -1))*5,
						"Creature", !(mode & (1<<2)), (count_subtype(player, TYPE_CREATURE, -1)-count_subtype(1-player, TYPE_CREATURE, -1))*5);
			mode |= (1<<(choice-1));
			chosen[cc] = types[choice-1];
		}
		APNAP(p, {
					int i;
					for(i=0; i<3; i++){
						impose_sacrifice(player, card, p, i+1, types[chosen[i]], 0, 0, 0, 0, 0, 0, 0, -1, 0);
					}
				}
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_pillar_of_flame(int player, int card, event_t event){
	/* Pillar of Flame	|R	0x2005c1e
	 * Sorcery
	 * ~ deals 2 damage to any target. If a creature dealt damage this way would die this turn, exile it instead. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance( player, card );
			int dc = damage_target0(player, card, 2);
			if( instance->targets[0].card != -1 && is_what(instance->targets[0].player, instance->targets[0].card, TYPE_CREATURE) ){
				get_card_instance(player, dc)->targets[3].card |= DMG_EXILE_IF_LETHALLY_DAMAGED_THIS_WAY;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_scalding_devil(int player, int card, event_t event){
	/* Scalding Devil	|1|R	0x2005e12
	 * Creature - Devil 1/1
	 * |2|R: ~ deals 1 damage to target player or planeswalker. */
	/* Volcanic Rambler	|5|R	0x2005e12
	 * Creature - Elemental 6/4
	 * |2|R: ~ deals 1 damage to target player or planeswalker. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			damage_target0(player, card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(2, 1), 0, &td1, "TARGET_PLAYER_OR_PLANESWALKER");
}

static void block_deal1dam(int player, int card, int blocking_player, int blocking_card)
{
  damage_creature(blocking_player, blocking_card, 1, player, card);
}
int card_somberwald_vigilante(int player, int card, event_t event)
{
  /* Somberwald Vigilante	|R
   * Creature - Human Warrior 1/1
   * Whenever ~ becomes blocked by a creature, ~ deals 1 damage to that creature. */

  if (event == EVENT_DECLARE_BLOCKERS && current_turn == player && !is_humiliated(player, card))
	for_each_creature_blocking_me(player, card, block_deal1dam, player, card);
  return 0;
}

int card_stonewright(int player, int card, event_t event){

	if( is_paired(player, card) ){

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_ACTIVATE ){
			int result = generic_activated_ability(player, card, event, 0, MANACOST_R(1), 0, NULL, NULL);
			if( result ){
				return result;
			}
			return granted_generic_activated_ability(player, card, instance->targets[3].player, instance->targets[3].card, event,
													0, MANACOST_R(1), 0, NULL, NULL);
		}

		if( event == EVENT_ACTIVATE ){
			int choice = instance->info_slot = 0;
			int ai_choice = 0;
			int can_activate_me = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_R(1), 0, NULL, NULL);
			int can_activate_paired = granted_generic_activated_ability(player, card, instance->targets[3].player, instance->targets[3].card,
																		EVENT_CAN_ACTIVATE, 0, MANACOST_R(1), 0, NULL, NULL);
			if( can_activate_me ){
				if( can_activate_paired ){
					if( is_attacking(instance->targets[3].player, instance->targets[3].card) &&
						is_unblocked(instance->targets[3].player, instance->targets[3].card) )
					{
						ai_choice = 1;
					}
					choice = do_dialog(player, player, card, -1, -1, " Pump Stonewright\n Pump paired creatur\n Cancel", ai_choice);
				}
				else{
					choice = 1;
				}
			}
			if( choice == 0 ){
				generic_activated_ability(player, card, event, 0, MANACOST_R(1), 0, NULL, NULL);
				if(spell_fizzled != 1){
					instance->info_slot = 1;
				}
			}
			if( choice == 1 ){
				granted_generic_activated_ability(player, card, instance->targets[3].player, instance->targets[3].card, event,
													0, MANACOST_R(1), 0, NULL, NULL);
				if(spell_fizzled != 1){
					instance->info_slot = 2;
				}
			}
			if( choice == 2 ){
				spell_fizzled = 1;
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( instance->info_slot == 1 ){
				pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 1, 0);
			}
			else{
				pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[3].player, instance->targets[3].card,
								1, 0);
			}
		}
	}

	return soulbond(player, card, event);
}

int card_thatchers_revolt(int player, int card, event_t event){
/* Thatcher Revolt	|2|R
 * Sorcery
 * Put three 1/1 |Sred Human creature tokens with haste onto the battlefield.
 * Sacrifice those tokens at the beginning of the next end step. */

	 if(event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_HUMAN, &token);
		token.qty = 3;
		token.s_key_plus = SP_KEYWORD_HASTE;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		token.special_infos = 66;
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_thunderous_wrath(int player, int card, event_t event){
	/* Explosive Impact	|5|R	0x2005b4c
	 * Instant
	 * ~ deals 5 damage to any target. */
	/* Thunderous Wrath	|4|R|R	0x2005b4c
	 * Instant
	 * ~ deals 5 damage to any target.
	 * Miracle |R */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			damage_target0(player, card, 5);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_tibalt_the_fiend_blooded(int player, int card, event_t event){

	/* Planeswalker - Tibalt (2)
	 * +1: Draw a card, then discard a card at random.
	 * -4: ~ deals damage equal to the number of cards in target player's hand to that player.
	 * -6: Gain control of all creatures until end of turn. Untap them. They gain haste until end of turn. */

	if (IS_ACTIVATING(event)){

		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		int priority_damage = would_validate_arbitrary_target(&td, 1-player, -1) && life[1-player] <= hand_count[1-player] ? 50 : 0;
		int priority_insurrection = (count_subtype(1-player, TYPE_CREATURE, -1)*3)-(12-(count_counters(player, card, COUNTER_LOYALTY)*2));

		enum{
			CHOICE_DRAW_DISCARD = 1,
			CHOICE_DAMAGE,
			CHOICE_INSURRECTION
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Draw & discard", 1, 5, 1,
						"Damage player", can_target(&td), priority_damage , -4,
						"Emblem", 1, priority_insurrection, -6);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_ACTIVATE)
		{
		  instance->number_of_targets = 0;
		  switch (choice)
			{
			  case CHOICE_DAMAGE:
					pick_target(&td, "TARGET_PLAYER");
					break;

			  case CHOICE_DRAW_DISCARD:
			  case CHOICE_INSURRECTION:
					break;
			}
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
			switch (choice)
			{
				case CHOICE_DRAW_DISCARD:
					draw_cards(player, 1);
					discard(player, DISC_RANDOM, player);
					break;

				case CHOICE_DAMAGE:
				{
					if( valid_target(&td) ){
						int amount = hand_count[instance->targets[0].player];
						damage_player(instance->targets[0].player, amount, player, instance->parent_card);
					}
				}
				break;

				case CHOICE_INSURRECTION:
				  insurrection_effect(instance->parent_controller, instance->parent_card);
				  break;
			}
	}

	return planeswalker(player, card, event, 2);
}

int card_tyrant_of_discord(int player, int card, event_t event){
/*
Tyrant of Discord |4|R|R|R
Creature — Elemental 7/7
When Tyrant of Discord enters the battlefield, target opponent chooses a permanent he or she controls at random and sacrifices it.
If a nonland permanent is sacrificed this way, repeat this process.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.zone = TARGET_ZONE_PLAYERS;

		if( would_validate_arbitrary_target(&td2, 1-player, -1) && can_sacrifice(player, 1-player, 1, TYPE_PERMANENT, 0) ){
			int permanents[active_cards_count[1-player]];
			int pc = 0;
			int c;
			for(c=0; c<active_cards_count[1-player]; c++){
				if( in_play(1-player, c) && is_what(1-player, c, TYPE_PERMANENT) &&
					! check_special_flags2(1-player, c, SF2_CANNOT_BE_SACRIFICED) )
				{
					permanents[pc] = c;
					pc++;
				}
			}
			while( pc ){
					int to_sac = internal_rand(pc);
					int stop = is_what(1-player, permanents[to_sac], TYPE_LAND);
					kill_card(1-player, permanents[to_sac], KILL_SACRIFICE);
					if( stop ){
						break;
					}
					int k;
					for(k=to_sac; k<(pc-1); k++){
						permanents[k] = permanents[k+1];
					}
					pc--;
			}
		}
	}

	return 0;
}

int card_uncanny_speed(int player, int card, event_t event){
/* CARD_ID_UNCANNY_SPEED	12497
Uncanny Speed	|1|R
Instant
Target creature gets +3/+0 and gains haste until end of turn. */

	return vanilla_instant_pump(player, card, event, ANYBODY, player, 3, 0, 0, SP_KEYWORD_HASTE);
}

int card_vexing_devil(int player, int card, event_t event){
/* CARD_ID_VEXING_DEVIL	12503
Vexing Devil	|R
Creature - Devil 4/3
When ~ enters the battlefield, any opponent may have it deal 4 damage to him or her. If a player does, sacrifice ~. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int ai_choice = 0;
		if( life[1-player] < 10 ){
			ai_choice = 1;
		}
		int choice = do_dialog(1-player, player, card, -1, -1, " Take 4 damage\n Pass", ai_choice);
		if( choice == 0 ){
			damage_player(1-player, 4, player, card);
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_vigilante_justice(int player, int card, event_t event){
	/* Vigilante Justice	|3|R	0x2005e21
	 * Enchantment
	 * Whenever a Human enters the battlefield under your control, ~ deals 1 damage to any target. */
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_HUMAN;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_ANY"))
			 damage_target0(player, card, 1);
		     instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_zealous_conscripts(int player, int card, event_t event){
/* CARD_ID_ZEALOUS_CONSCRIPTS	12514
Zealous Conscripts	|4|R
Creature - Human Warrior 3/3
Haste
When ~ enters the battlefield, gain control of target permanent until end of turn. Untap that permanent. It gains haste until end of turn. */

	haste(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
			effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

// GREEN
int enchanted_land_has_T_add_n_mana_of_any_one_color(int player, int card, event_t event, int amt){

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance( player, card );
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_CAN_ACTIVATE ){
			return mana_producer(p, c, event);
		}

		if( event == EVENT_ACTIVATE ){
			produce_mana_tapped_all_one_color(p, c, COLOR_TEST_ANY_COLORED, amt);
			if (cancel != 1){
				// tap_card() would do this, but clear tapped_for_mana_color first.
				dispatch_event(p, c, EVENT_TAP_CARD);
			}
		}

		if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
			if( mana_producer(p, c, EVENT_CAN_ACTIVATE) ){
				declare_mana_available_hex(p, COLOR_TEST_ANY_COLORED, amt);
			}
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;
	td.allowed_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return targeted_aura(player, card, event, &td, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		ai_modifier+=100;
		new_pick_target(&td, "Select target land you control.", 0, 1 | GS_LITERAL_PROMPT);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		return targeted_aura(player, card, event, &td, NULL);
	}

	return 0;
}

int card_abundant_growth(int player, int card, event_t event){
	/* Abundant Growth	|G
	 * Enchantment - Aura
	 * Enchant land
	 * When ~ enters the battlefield, draw a card.
	 * Enchanted land has "|T: Add one mana of any color to your mana pool." */

	if (comes_into_play(player, card, event)){
		draw_a_card(player);
	}

	return enchanted_land_has_T_add_n_mana_of_any_one_color(player, card, event, 1);
}


int card_blessing_of_nature(int player, int card, event_t event){
/* CARD_ID_BLESSINGS_OF_NATURE	12315
Blessings of Nature	|4|G
Sorcery
Distribute four +1/+1 counters among any number of target creatures.
Miracle |G (You may cast this card for its miracle cost when you draw it if it's the first card you drew this turn.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int i;
		for(i=0; i<4; i++){
			new_pick_target(&td, "TARGET_CREATURE", i, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<4; i++){
			if( validate_target(player, card, &td, i) ){
				add_1_1_counter(instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_bower_passage(int player, int card, event_t event){
/* CARD_ID_BOWER_PASSAGE	12319
Bower Passage	|1|G
Enchantment
Creatures with flying can't block creatures you control. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_BLOCK_LEGALITY && attacking_card_controller == player ){
			if( check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING) ){
				event_result = 1;
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_champion_of_lambholt(int player, int card, event_t event){
/*CARD_ID_CHAMPION_OF_LAMBHOLT	12329
Champion of Lambholt	|1|G|G
Creature - Human Warrior 1/1
Creatures with power less than ~'s power can't block creatures you control.
Whenever another creature enters the battlefield under your control, put a +1/+1 counter on ~.*/

	if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) )
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		add_1_1_counter(player, card);
	}

	if( event == EVENT_BLOCK_LEGALITY && player == attacking_card_controller && in_play(player, card) && ! is_humiliated(player, card) &&
		get_power(affected_card_controller, affected_card) < get_power(player, card))
	{
		event_result = 1;
	}

	return 0;
}

int card_craterhoof_behemoth(int player, int card, event_t event){
/* CARD_ID_CRATERHOOF_BEHEMOTH	12334
Craterhoof Behemoth	|5|G|G|G
Creature - Beast 5/5
Haste
When ~ enters the battlefield, creatures you control gain trample and get +X/+X until end of turn, where X is the number of creatures you control. */

	haste(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int amount = count_permanents_by_type(player, TYPE_CREATURE);
		pump_subtype_until_eot(player, card, player, -1, amount, amount, KEYWORD_TRAMPLE, 0);
	}

	return 0;
}

int card_descendants_path(int player, int card, event_t event){
/* CARD_ID_DESCENDANTS_PATH	12346
Descendants' Path	|2|G
Enchantment
At the beginning of your upkeep, reveal the top card of your library. If it's a creature card that shares a creature type with a creature you control,
you may cast that card without paying its mana cost. Otherwise, put that card on the bottom of your library. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int result = 0;
		int *deck = deck_ptr[player];
		show_deck( player, deck, 1, "Here's the first card of the deck", 0, 0x7375B0 );
		if( is_what(-1, deck[0], TYPE_CREATURE) ){
			int id = cards_data[deck[0]].id;
			int count = active_cards_count[player]-1;
			while( count > -1  ){
					if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) &&
						shares_creature_subtype(-1, deck[0], player, count)
					  ){
						card_ptr_t* c_me = cards_ptr[ id ];
						char buffer[600];
						scnprintf(buffer, 600, " Play %s\n Pass", c_me->name);
						int choice = do_dialog(player, player, card, -1, -1, buffer, 0);
						if( choice == 0 ){
							play_card_in_deck_for_free(player, player, 0);
						}
						result = 1;
						break;
					}
					count--;
			}
		}
		if( result == 0 ){
			put_top_card_of_deck_to_bottom(player);
		}
	}
	return global_enchantment(player, card, event);
}

int card_diregraf_escort(int player, int card, event_t event){
/* CARD_ID_DIREGRAF_ESCORT	12351
Diregraf Escort	|G
Creature - Human Cleric 1/1
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, both creatures have protection from Zombies. */

	if( is_paired(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		protection_from_subtype(player, card, event, SUBTYPE_ZOMBIE);
		protection_from_subtype(instance->targets[3].player, instance->targets[3].card, event, SUBTYPE_ZOMBIE);
	}

	return soulbond(player, card, event);
}

int card_druids_familiar(int player, int card, event_t event){
/* CARD_ID_DRUIDS_FAMILIAR	12356
Druid's Familiar	|3|G
Creature - Bear 2/2
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, each of those creatures gets +2/+2. */

	if( is_paired(player, card) ){
		modify_pt_and_abilities(player, card, event, 2, 2, 0);
		boost_paired_creature(player, card, event, 2, 2, 0, 0);
	}

	return soulbond(player, card, event);
}

int card_eaten_by_spiders(int player, int card, event_t event){
/* CARD_ID_EATEN_BY_SPIDERS	12359
Eaten by Spiders	|2|G
Instant
Destroy target creature with flying and all Equipment attached to that creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			equipments_attached_to_me(instance->targets[0].player, instance->targets[0].card, EATM_DESTROY);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature with flying.", 1, NULL);
}

int card_druids_repository(int player, int card, event_t event)
{
  /* Druids' Repository	|1|G|G
   * Enchantment
   * Whenever a creature you control attacks, put a charge counter on ~.
   * Remove a charge counter from ~: Add one mana of any color to your mana pool. */

  int amt;
  if ((amt = declare_attackers_trigger(player, card, event, 0, player, -1)))
	add_counters(player, card, COUNTER_CHARGE, amt);

  if (event == EVENT_CAN_ACTIVATE && count_counters(player, card, COUNTER_CHARGE) && can_produce_mana(player, card))
	return 1;

  if (event == EVENT_ACTIVATE && produce_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1))
	remove_counter(player, card, COUNTER_CHARGE);

  if (event == EVENT_COUNT_MANA && affect_me(player, card) && can_produce_mana(player, card))
	declare_mana_available_any_combination_of_colors(player, COLOR_TEST_ANY_COLORED, count_counters(player, card, COUNTER_CHARGE));

  return global_enchantment(player, card, event);
}

int card_flowering_lumberknot(int player, int card, event_t event)
{
/* CARD_ID_FLOWERING_LUMBERKNOT	12371
Flowering Lumberknot	|3|G
Creature - Treefolk 5/5
~ can't attack or block unless it's paired with a creature with soulbond. */

	if( (event == EVENT_ATTACK_LEGALITY || event == EVENT_BLOCK_LEGALITY) && affect_me(player, card) && !is_paired(player, card) &&
		! is_humiliated(player, card) )
	{
		event_result = 1;
	}

	return 0;
}

int card_geist_trapper(int player, int card, event_t event){
/* CARD_ID_GEIST_TRAPPERS	12376
Geist Trappers	|4|G
Creature - Human Warrior 3/5
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, both creatures have reach. */

	if( is_paired(player, card) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_REACH);
		boost_paired_creature(player, card, event, 0, 0, KEYWORD_REACH, 0);
	}

	return soulbond(player, card, event);
}

int card_grounded(int player, int card, event_t event){
/* CARD_ID_GROUNDED	12387
Grounded	|1|G
Enchantment - Aura
Enchant creature
Enchanted creature loses flying. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( event == EVENT_ABILITIES && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			if( event_result & KEYWORD_FLYING ){
				event_result &= ~KEYWORD_FLYING;
			}
		}
	}

	return vanilla_aura(player, card, event, 1-player);
}

int card_howlgeist(int player, int card, event_t event){
/* CARD_ID_HOWLGEIST	12400
Howlgeist	|5|G
Creature - Spirit Wolf 4/2
Creatures with power less than ~'s power can't block it.
Undying (When this creature dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.) */

	if( event == EVENT_BLOCK_LEGALITY && in_play(player, card) && ! is_humiliated(player, card) ){
		if( player == attacking_card_controller && card == attacking_card ){
			if( get_power(affected_card_controller, affected_card) < get_power(player, card) ){
				event_result = 1;
			}
		}
	}

	return undying(player, card, event);
}

int card_joint_assault(int player, int card, event_t event){
/* CARD_ID_JOINT_ASSAULT	12405
Joint Assault	|G
Instant
Target creature gets +2/+2 until end of turn. If it's paired with a creature, that creature also gets +2/+2 until end of turn. */

	if( event != EVENT_RESOLVE_SPELL ){
		return vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 2, 0, 0);
	}

	if( event == EVENT_RESOLVE_SPELL){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance( player, card );

		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
			pump_paired_creature_until_eot(instance->targets[0].player, instance->targets[0].card, 2, 2, 0, 0);
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_lair_delve(int player, int card, event_t event){
/* CARD_ID_LAIR_DELVE	12409
Lair Delve	|2|G
Sorcery
Reveal the top two cards of your library. Put all creature and land cards revealed this way into your hand and the rest on the bottom of your library in any order. */

	if( event == EVENT_RESOLVE_SPELL ){
		int *deck = deck_ptr[player];
		int amount = 2;
		if( amount > count_deck(player) ){
			amount = count_deck(player);
		}
		if( amount > 0 ){
			show_deck( HUMAN, deck, amount, "Cards revealed with Lair Delve", 0, 0x7375B0 );
			int amount2 = amount;
			int i;
			for(i=0; i<amount; i++ ){
				if( is_what(-1, deck[0], TYPE_CREATURE) || is_what(-1, deck[0], TYPE_LAND)){
					add_card_to_hand(player, deck[0]);
					remove_card_from_deck(player, 0);
					amount2--;
				}
			}
			if( amount2 > 0 ){
				put_top_x_on_bottom(player, player, amount2);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_natural_end(int player, int card, event_t event){
/* CARD_ID_NATURAL_END	12431
Natural End	|2|G
Instant
Destroy target artifact or enchantment. You gain 3 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			gain_life(player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
}

// nettle swine -> vanilla

int card_nightshade_peddler(int player, int card, event_t event){
/* CARD_ID_NIGHTSHADE_PEDDLER	12436
Nightshade Peddler	|1|G
Creature - Human Druid 1/1
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, both creatures have deathtouch. */

	if( is_paired(player, card) ){
		deathtouch(player, card, event);
		boost_paired_creature(player, card, event, 0, 0, 0, SP_KEYWORD_DEATHTOUCH);
	}

	return soulbond(player, card, event);
}

int card_pathbreaker_wurm(int player, int card, event_t event){
/* CARD_ID_PATHBREAKER_WURM	12439
Pathbreaker Wurm	|4|G|G
Creature - Wurm 6/4
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, both creatures have trample. */

	if( is_paired(player, card) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_TRAMPLE);
		boost_paired_creature(player, card, event, 0, 0, KEYWORD_TRAMPLE, 0);
	}

	return soulbond(player, card, event);
}

int card_primal_surge(int player, int card, event_t event){
/* CARD_ID_PRIMAL_SURGE	12443
Primal Surge	|8|G|G
Sorcery
Exile the top card of your library. If it's a permanent card, you may put it onto the battlefield. If you do, repeat this process. */

	if(event == EVENT_RESOLVE_SPELL ){
		int *deck = deck_ptr[player];
		while( deck[0] != -1 ){
				show_deck( HUMAN, deck, 1, "Here's the first card of deck", 0, 0x7375B0 );
				if( is_what(-1, deck[0], TYPE_PERMANENT) ){
					int choice = 0;
					if( player != AI ){
						char buffer[500];
						card_ptr_t* c_me = cards_ptr[ cards_data[deck[0]].id ];
						scnprintf(buffer, 500, " Put %s into play\n Pass", c_me->name);
						choice = do_dialog(player, player, card, -1, -1, buffer, 0);
					}
					if( choice == 0 ){
						put_into_play_a_card_from_deck(player, player, 0);
					}
					else{
						rfg_top_card_of_deck(player);
						break;
					}
				}
				else{
					rfg_top_card_of_deck(player);
					break;
				}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_rain_of_thorns(int player, int card, event_t event){
/*
Rain of Thorns English
Sorcery, 4GG (6)
Choose one or more —
• Destroy target artifact.
• Destroy target enchantment.
• Destroy target land.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_ARTIFACT);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_ENCHANTMENT);

	target_definition_t td3;
	default_target_definition(player, card, &td3, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		target_definition_t td4;
		default_target_definition(player, card, &td4, TYPE_ARTIFACT|TYPE_ENCHANTMENT|TYPE_LAND);
		return generic_spell(player, card, event, GS_CAN_TARGET, &td4, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->info_slot = instance->number_of_targets = 0;

		int base_target = 0;

		if( can_target(&td1) ){
			if( new_pick_target(&td1, "TARGET_ARTIFACT", base_target, 0) ){
				SET_BYTE0(instance->info_slot) = 1;
				base_target++;
			}
		}

		if( can_target(&td2) ){
			if( new_pick_target(&td2, "TARGET_ENCHANTMENT", base_target, 0) ){
				if( base_target == 0 ){
					SET_BYTE0(instance->info_slot) = 2;
				}
				if( base_target == 1 ){
					SET_BYTE1(instance->info_slot) = 2;
				}
				base_target++;
			}
		}

		if( can_target(&td3) ){
			if( new_pick_target(&td3, "TARGET_ENCHANTMENT", base_target, 0) ){
				if( base_target == 0 ){
					SET_BYTE0(instance->info_slot) = 3;
				}
				if( base_target == 1 ){
					SET_BYTE1(instance->info_slot) = 3;
				}
				if( base_target == 2 ){
					SET_BYTE2(instance->info_slot) = 3;
				}
				base_target++;
			}
		}

		if( ! base_target ){
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for (i = 0; i < instance->number_of_targets; ++i){
			int valid = 0;
			if( i == 0 ){
				if( BYTE0(instance->info_slot) == 1 && validate_target(player, card, &td1, i) ){
					valid = 1;
				}
				if( BYTE0(instance->info_slot) == 2 && validate_target(player, card, &td2, i) ){
					valid = 1;
				}
				if( BYTE0(instance->info_slot) == 3 && validate_target(player, card, &td3, i) ){
					valid = 1;
				}
			}
			if( i == 1 ){
				if( BYTE1(instance->info_slot) == 2 && validate_target(player, card, &td2, i) ){
					valid = 1;
				}
				if( BYTE1(instance->info_slot) == 3 && validate_target(player, card, &td3, i) ){
					valid = 1;
				}
			}
			if( i == 2 ){
				if( BYTE2(instance->info_slot) == 3 && validate_target(player, card, &td3, i) ){
					valid = 1;
				}
			}
			if( valid ){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_revenge_of_the_hunted(int player, int card, event_t event){
/* CARD_ID_REVENGE_OF_THE_HUNTED	12449
Revenge of the Hunted	|4|G|G
Sorcery
Until end of turn, target creature gets +6/+6 and gains trample, and all creatures able to block it this turn do so.
Miracle |G (You may cast this card for its miracle cost when you draw it if it's the first card you drew this turn.) */

	return vanilla_instant_pump(player, card, event, ANYBODY, player, 6, 6, KEYWORD_TRAMPLE, SP_KEYWORD_LURE);
}

int card_sheltering_word(int player, int card, event_t event){
	/*
	  Sheltering Word |1|G
	  Instant
	  Target creature you control gains hexproof until end of turn. You gain life equal to that creature's toughness.
	*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = td.allowed_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_HEXPROOF);
			gain_life(player, get_toughness(instance->targets[0].player, instance->targets[0].card) );
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature you control.", 1, NULL);
}

int card_snare_the_skies(int player, int card, event_t event){
	/* Snare the Skies	|G
	 * Instant
	 * Target creature gets +1/+1 and gains reach until end of turn. */
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 1, 1, KEYWORD_REACH, 0);
}

int card_somberwald_druid(int player, int card, event_t event){
/* CARD_ID_SOMBERWALD_DRYAD	12264
Somberwald Dryad	|1|G
Creature - Dryad 2/2
|H2Forestwalk (This creature can't be blocked as long as defending player controls |Ha Forest.) */

	// This is an approximation
	if(event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player ){
		if( ! is_tapped(player, card) && ! is_sick(player, card) && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			int amount = 3;
			int color_to_reduce = 0;
			card_ptr_t* c = cards_ptr[get_id(affected_card_controller, affected_card)];
			int clrs[5];
			clrs[0] = c->req_black;
			clrs[1] = c->req_blue;
			clrs[2] = c->req_green;
			clrs[3] = c->req_red;
			clrs[4] = c->req_white;
			int i;
			int par = 0;
			for(i=0; i<5; i++){
				if( clrs[i] > par ){
					par = clrs[i];
					color_to_reduce = i;
				}
			}
			if( color_to_reduce == 0 ){
				amount-=c->req_black;
				if( amount < 0 ){
					amount = 0;
				}
				COST_BLACK-=(3-amount);
			}
			if( color_to_reduce == 1 ){
				amount-=c->req_blue;
				if( amount < 0 ){
					amount = 0;
				}
				COST_BLUE-=(3-amount);
			}
			if( color_to_reduce == 2 ){
				amount-=c->req_green;
				if( amount < 0 ){
					amount = 0;
				}
				COST_GREEN-=(3-amount);
			}
			if( color_to_reduce == 3 ){
				amount-=c->req_red;
				if( amount < 0 ){
					amount = 0;
				}
				COST_RED-=(3-amount);
			}
			if( color_to_reduce == 4 ){
				amount-=c->req_white;
				if( amount < 0 ){
					amount = 0;
				}
				COST_WHITE-=(3-amount);
			}
			if( amount > 0 ){
				COST_COLORLESS-=amount;
			}
		}
	}

	if( specific_spell_played(player, card, event, player, 2, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0) ){
		tap_card(player, card);
	}

	return 0;
}

static int terrifying_presence_effect(int player, int card, event_t event)
{
	card_instance_t *instance, *damage = combat_damage_being_prevented(event);

	if( damage && (instance = get_card_instance(player, card)) &&
		!(damage->damage_source_card == instance->targets[0].card && damage->damage_source_player == instance->targets[0].player) &&
		(damage->targets[3].player & TYPE_CREATURE))	// probably redundant to status check
	{
		int amount = damage->info_slot;
		prevent_damage(affected_card_controller, affected_card, amount);
	}

	if (event == EVENT_CLEANUP)
	{
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_terrifying_presence(int player, int card, event_t event){
/*
Terrifying Presence |1|G
Instant
Prevent all combat damage that would be dealt by creatures other than target creature this turn.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			int legacy = create_legacy_effect(player, card, &terrifying_presence_effect);
			card_instance_t *leg = get_card_instance( player, legacy );
			leg->targets[0] = instance->targets[0];
			leg->number_of_targets = 1;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_soul_of_the_harvest(int player, int card, event_t event){
/* CARD_ID_SOUL_OF_THE_HARVEST	12471
Soul of the Harvest	|4|G|G
Creature - Elemental 6/6
Trample
Whenever another nontoken creature enters the battlefield under your control, you may draw a card. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.type_flag = F1_NO_TOKEN;
		this_test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_timberland_guide(int player, int card, event_t event){

	/* Bond Beetle	|G
	 * Creature - Insect 0/1
	 * When ~ enters the battlefield, put a +1/+1 counter on target creature. */

	/* Ironshell Beetle	|1|G
	 * Creature - Insect 1/1
	 * When ~ enters the battlefield, put a +1/+1 counter on target creature. */

	/* Satyr Grovedancer	|1|G
	 * Creature - Satyr Shaman 1/1
	 * When ~ enters the battlefield, put a +1/+1 counter on target creature. */

	/* Supply-Line Cranes	|3|W|W
	 * Creature - Bird 2/4
	 * Flying
	 * When ~ enters the battlefield, put a +1/+1 counter on target creature. */

	/* Timberland Guide	|1|G
	 * Creature - Human Scout 1/1
	 * When ~ enters the battlefield, put a +1/+1 counter on target creature. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_triumph_of_ferocity(int player, int card, event_t event){
/* CARD_ID_TRIUMPH_OF_FEROCITY	12493
Triumph of Ferocity	|2|G
Enchantment
At the beginning of your upkeep, draw a card if you control the creature with the greatest power or tied for the greatest power. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( get_highest_power(player) >= get_highest_power(1-player) ){
			draw_cards(player, 1);
		}
	}

	return global_enchantment(player, card, event);
}

int card_trusted_forcemage(int player, int card, event_t event){
/* CARD_ID_TRUSTED_FORCEMAGE	12494
Trusted Forcemage	|2|G
Creature - Human Shaman 2/2
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, each of those creatures gets +1/+1. */

	if( is_paired(player, card) ){
		modify_pt_and_abilities(player, card, event, 1, 1, 0);
		boost_paired_creature(player, card, event, 1, 1, 0, 0);
	}

	return soulbond(player, card, event);
}

int card_ulvenwald_tracker(int player, int card, event_t event){
	/* Ulvenwald Tracker	|G
	 * Creature - Human Shaman 1/1
	 * |1|G, |T: Target creature you control fights another target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.allowed_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_target(&td) ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XG(1, 1), 0, &td2, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		charge_mana_for_activated_ability(player, card, MANACOST_XG(1, 1));
		if( new_pick_target(&td, "Select target creature you control.", 0, 1 | GS_LITERAL_PROMPT) ){
			if( new_pick_target(&td2, "TARGET_CREATURE", 1, 1) ){
				add_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td2, 1) ){
			fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player, instance->targets[1].card);
		}
	}

	return 0;
}

// Vorstclaw -> vanilla

int card_wandering_wolf(int player, int card, event_t event){
/* CARD_ID_WANDERING_WOLF	12507
Wandering Wolf	|1|G
Creature - Wolf 2/1
Creatures with power less than ~'s power can't block it. */

	if( event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card) ){
		if( player == attacking_card_controller && card == attacking_card ){
			if( get_power(affected_card_controller, affected_card) < get_power(player, card) ){
				event_result = 1;
			}
		}
	}
	return 0;
}

int card_wild_defiance(int player, int card, event_t event)
{
/* CARD_ID_WILD_DEFIANCE	12508
Wild Defiance	|2|G
Enchantment
Whenever a creature you control becomes the target of an instant or sorcery spell, that creature gets +3/+3 until end of turn. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		const target_t* targets = any_becomes_target(player, card, event, player, 0, TYPE_CREATURE, -1,
													TYPE_INSTANT|TYPE_INTERRUPT|TYPE_SORCERY, ANYBODY, RESOLVE_TRIGGER_MANDATORY);
		if (targets){
			for (; targets->player != -1; ++targets){
				pump_until_eot(player, card, targets->player, targets->card, 3, 3);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_wildwood_geist(int player, int card, event_t event){
/* CARD_ID_WILDWOOD_GEIST	12509
Wildwood Geist	|4|G
Creature - Spirit 3/3
~ gets +2/+2 as long as it's your turn. */

	if( current_turn == player && in_play(player, card) && ! is_humiliated(player, card) ){
		modify_pt_and_abilities(player, card, event, 2, 2, 0);
	}

	return 0;
}

int card_wolfir_avenger(int player, int card, event_t event){
/* CARD_ID_WOLFIR_AVENGER	12511
Wolfir Avenger	|1|G|G
Creature - Wolf Warrior 3/3
Flash (You may cast this spell any time you could cast an instant.)
|1|G: Regenerate ~. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		return regeneration(player, card, event, MANACOST_XG(1, 1) );
	}

	return flash(player, card, event);
}

int card_wolfir_silverheart(int player, int card, event_t event){
/* CARD_ID_WOLFIR_SILVERHEART	12512
Wolfir Silverheart	|3|G|G
Creature - Wolf Warrior 4/4
Soulbond (You may pair this creature with another unpaired creature when either enters the battlefield. They remain paired for as long as you control both of them.)
As long as ~ is paired with another creature, each of those creatures gets +4/+4. */

	if( is_paired(player, card) ){
		modify_pt_and_abilities(player, card, event, 4, 4, 0);
		boost_paired_creature(player, card, event, 4, 4, 0, 0);
	}

	return soulbond(player, card, event);
}

int card_yew_spirit(int player, int card, event_t event){

	/* Yew Spirit	|4|G
	 * Creature - Spirit Treefolk 3/3
	 * |2|G|G: ~ gets +X/+X until end of turn, where X is its power. */
	// Used externally
	if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST){
		int initial_power = get_power(player, card);
		if (initial_power > 0){
			int final_power, num_times_can_activate = generic_shade_amt_can_pump(player, card, 1, 0, MANACOST_XG(2,2), -1);
			for (final_power = initial_power; num_times_can_activate > 0; --num_times_can_activate){
				final_power *= 2;
			}
			return final_power - initial_power;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int pp = instance->parent_controller, pc = instance->parent_card;
		if (in_play(pp, pc)){
			int plus = get_power(pp, pc);
			pump_until_eot_merge_previous(pp, pc, pp, pc, plus, plus);
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XG(2, 2), 0, NULL, NULL);
}

// GOLD

static int check_targets_for_bruna(int player, int card, int mode){

	int result = -1;
	int par = -1;
	int i;
	for(i=0; i<2; i++){
		int count;
		for(count=0; count<active_cards_count[i]; count++){
			if( in_play(i, count) && is_what(i, count, TYPE_ENCHANTMENT) && has_subtype(i, count, SUBTYPE_AURA_CREATURE) ){
				card_instance_t *instance = get_card_instance(i, count);
				if( !(i== player && count == card) ){
					if( instance->damage_target_player > -1 && is_what(instance->damage_target_player, instance->damage_target_card, TYPE_CREATURE) ){
						if( mode == 0 ){
							if( player == HUMAN ){
								return 1;
							}
							if( player == AI && i == HUMAN){
								return 1;
							}
						}
						if( mode == 1 ){
							if( player == AI && i == HUMAN){
								if( get_cmc(i, count) > par ){
									par = get_cmc(i, count);
									result = count;
								}
							}
						}
					}
				}
			}
		}
	}
	return result;
}

int card_bruna_light_of_alabaster(int player, int card, event_t event){

	/* Bruna, Light of Alabaster	|3|W|W|U
	 * Legendary Creature - Angel 5/5
	 * Flying, vigilance
	 * Whenever ~ attacks or blocks, you may attach to it any number of Auras on the battlefield and you may put onto the battlefield attached to it any number
	 * of Aura cards that could enchant it from your graveyard and/or hand. */

	check_legend_rule(player, card, event);

	vigilance(player, card, event);

	if (declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), player, card) ||
		(blocking(player, card, event) && !is_humiliated(player, card))
	   ){
		while( check_targets_for_bruna(player, card, 0) ){
				if( player == HUMAN ){
					target_definition_t td;
					base_target_definition(player, card, &td, TYPE_ENCHANTMENT);
					td.required_subtype = SUBTYPE_AURA_CREATURE;

					card_instance_t *instance = get_card_instance(player, card);

					instance->number_of_targets = 0;
					if( pick_next_target_noload(&td, "Select an Aura card with enchant creature.") ){
						card_instance_t *aura = get_card_instance(instance->targets[0].player, instance->targets[0].card);
						aura->damage_target_player = player;
						aura->damage_target_card = card;
					}
					else{
						cancel = 0;
						break;
					}
				}
				else{
					int result = check_targets_for_bruna(player, card, 1);
					if( result > -1 ){
						card_instance_t *aura = get_card_instance(1-player, result);
						aura->damage_target_player = player;
						aura->damage_target_card = card;
					}
				}
		}
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ENCHANTMENT, "Select an Aura card with enchant creature.");
		this_test.subtype = SUBTYPE_AURA_CREATURE;
		while( new_special_count_grave(player, &this_test) ){
				int result = new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_CMC, &this_test);
				if( result > -1 ){
					put_into_play_aura_attached_to_target(player, result, player, card);
				}
				else{
					break;
				}
		}
		this_test.zone = TARGET_ZONE_HAND;
		while( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
				int result = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test);
				if( result > -1 ){
					put_into_play_aura_attached_to_target(player, result, player, card);
				}
				else{
					break;
				}
		}
	}

	return 0;
}

int card_gisela_blade_of_goldnight(int player, int card, event_t event)
{
/* CARD_ID_GISELA_BLADE_OF_GOLDNIGHT	12381
Gisela, Blade of Goldnight	|4|R|W|W
Legendary Creature - Angel 5/5
Flying, first strike
If a source would deal damage to an opponent or a permanent an opponent controls, that source deals double that damage to that player or permanent instead.
If a source would deal damage to you or a permanent you control, prevent half that damage, rounded up. */

  check_legend_rule(player, card, event);

  /* Since the way this is supposed to work is the controller of the object being dealt damage chooses the order of replacement effects, assume he chooses to
   * halve damage before doing anything else, to maximize his subsequent damage prevention effects (so apply that during EVENT_PREVENT_DAMAGE); or to double
   * damage after doing everything else, to maximize his previous damage prevention effects (so apply that during EVENT_DEAL_DAMAGE). */
	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t* damage = damage_being_prevented(event);
		if (damage && damage->damage_target_player == player && !(damage->state & STATE_CANNOT_TARGET)){
			int amount = (damage->info_slot + 1) / 2;
			prevent_damage(affected_card_controller, affected_card, amount);
		}

		damage = damage_being_dealt(event);
		if (damage && damage->damage_target_player == 1-player){
			damage->info_slot *= 2;
		}
	}

	return 0;
}

int card_sigarda_host_of_herons(int player, int card, event_t event){
/* CARD_ID_SIGARDA_HOST_OF_HERONS	12465
Sigarda, Host of Herons	|2|G|W|W
Legendary Creature - Angel 5/5
Flying, hexproof
Spells and abilities your opponents control can't cause you to sacrifice permanents. */

	hexproof(player, card, event);

	check_legend_rule(player, card, event);

	// The true effect is in "sacrifice.c"

	return 0;
}

// ARTIFACTS

int card_angels_tomb(int player, int card, event_t event){
/* CARD_ID_ANGELS_TOMB	12306
Angel's Tomb	|3
Artifact
Whenever a creature enters the battlefield under your control, you may have ~ become a 3/3 |Swhite Angel artifact creature with flying until end of turn.*/

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);

		if( ! is_what(player, card, TYPE_CREATURE) && new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test) )
		{
			add_a_subtype(player, card, SUBTYPE_ANGEL);
			int result = artifact_animation(player, card, player, card, 1, 3, 3, KEYWORD_FLYING, 0);
			card_instance_t *instance = get_card_instance( player, result );
			instance->targets[6].player = COLOR_TEST_WHITE;
		}
	}

	return 0;
}

int card_angelic_armaments(int player, int card, event_t event){
/* CARD_ID_ANGELIC_ARMAMENTS	12305
Angelic Armaments	|3
Artifact - Equipment
Equipped creature gets +2/+2, has flying, and is a |Swhite Angel in addition to its other colors and types.
Equip |4 */

	if (event == EVENT_SET_COLOR && is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		if(  affect_me(instance->damage_target_player, instance->damage_target_card) ){
			event_result |= COLOR_TEST_WHITE;
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance( player, card );

		if( is_equipping(player, card) ){
			reset_subtypes(instance->damage_target_player, instance->damage_target_card, 2);
		}
		if( validate_target(player, card, &td, 0) ){
			add_a_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_ANGEL);
		}
	}

	return vanilla_equipment(player, card, event, 4, 2, 2, KEYWORD_FLYING, 0);
}

int card_bladed_bracers(int player, int card, event_t event){
/* CARD_ID_BLADED_BRACERS	12314
Bladed Bracers	|1
Artifact - Equipment
Equipped creature gets +1/+1.
As long as equipped creature is a Human or an Angel, it has vigilance.
Equip |2 (|2: Attach to target creature you control. Equip only as a sorcery.) */

	if( event == EVENT_ABILITIES && is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		if( has_subtype(instance->damage_target_player, instance->damage_target_card, SUBTYPE_ANGEL) ||
			has_subtype(instance->damage_target_player, instance->damage_target_card, SUBTYPE_HUMAN)
		  ){
			vigilance(instance->damage_target_player, instance->damage_target_card, event);
		}
	}

	return vanilla_equipment(player, card, event, 2, 1, 1, 0, 0);
}

int card_conjuring_closet(int player, int card, event_t event){
/* CARD_ID_CONJURERS_CLOSET	12332
Conjurer's Closet	|5
Artifact
At the beginning of your end step, you may exile target creature you control, then return that card to the battlefield under your control.*/

	if( trigger_condition == TRIGGER_EOT && affect_me(player, card) && current_turn == player){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance( player, card );

		instance->number_of_targets = 0;

		if( eot_trigger_mode(player, card, event, player, can_target(&td) ? RESOLVE_TRIGGER_AI(player) : 0) ){
			if( pick_target(&td, "ASHNODS_BATTLEGEAR") ){	// "Select target creature you control."
				blink_effect_controller(instance->targets[0].player, instance->targets[0].card, 0);
			}
		}
	}

	return basic_equipment(player, card, event, 4);
}

int card_gallows_at_willow_hill(int player, int card, event_t event){
/* Gallows at Willow Hill	|3
 * Artifact
 * |3, |T, Tap three untapped Humans you control: Destroy target creature.
		Its controller puts a 1/1 |Swhite Spirit creature token with flying onto the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allow_cancel = 0;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT);
	td1.allowed_controller = player;
	td1.preferred_controller = player;
	td1.illegal_abilities = 0;
	td1.required_subtype = SUBTYPE_HUMAN;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( target_available(player, card, &td1) >= 3 ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(3)) ){
			int tapped[3];
			int tc = 0;
			while( tc < 3 ){
				if( select_target(player, card, &td, "Select an Human to tap.", &(instance->targets[tc])) ){
					state_untargettable(instance->targets[tc].player, instance->targets[tc].card, 1);
					tapped[tc] = instance->targets[tc].card;
					tc++;
				}
				else{
					break;
				}
			}
			int i;
			for(i=0; i<tc; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			}
			if( tc < 3 ){
				spell_fizzled = 1;
				return 0;
			}
			instance->number_of_targets = 0;
			if( pick_target(&td1, "TARGET_PERMANENT") ){
				for(i=0; i<3; i++){
					tap_card(player, tapped[i]);
				}
				tap_card(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);

			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SPIRIT, &token);
			token.t_player = instance->targets[0].player;
			token.pow = token.tou = 1;
			token.key_plus = KEYWORD_FLYING;
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
			generate_token(&token);
		}
	}

	return 0;
}

// Hunted guardian --> Serra Angel

int card_moonsilver_spear(int player, int card, event_t event)
{
  /* Moonsilver Spear	|4
   * Artifact - Equipment
   * Equipped creature has first strike.
   * Whenever equipped creature attacks, put a 4/4 |Swhite Angel creature token with flying onto the battlefield.
   * Equip |4 */

   if( is_equipping(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		if (declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY,
										instance->damage_target_player, instance->damage_target_card) )
		{
			make_angel(player, card, 1);
		}
	}

	return vanilla_equipment(player, card, event, 4, 0, 0, KEYWORD_FIRST_STRIKE, 0);
}

int card_narstad_scrapper(int player, int card, event_t event){
/* CARD_ID_NARSTAD_SCRAPPER	12430
Narstad Scrapper	|5
Artifact Creature - Construct 3/3
|2: ~ gets +1/+0 until end of turn. */

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_X(2), 1, 0);
}

int card_otherworld_atlas(int player, int card, event_t event){
/*
Otherworld Atlas |4
Artifact
{T}: Put a charge counter on Otherworld Atlas.
{T}: Each player draws a card for each charge counter on Otherworld Atlas.
*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_COUNTER = 1,
		CHOICE_DRAW
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		ai_modifier += 20-(count_counters(player, card, COUNTER_CHARGE)*5);
		instance->info_slot = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int priority_counter = 20-(count_counters(player, card, COUNTER_CHARGE)*5);
			int priority_draw = count_counters(player, card, COUNTER_CHARGE)*5;
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Add a counter", 		1, priority_counter,
								"Both players draw", 	1, priority_draw);
			if( ! choice ){
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_COUNTER ){
			add_counter(instance->parent_controller, instance->parent_card, COUNTER_CHARGE);
		}
		if( instance->info_slot == CHOICE_DRAW ){
			APNAP(p, { draw_cards(p, count_counters(instance->parent_controller, instance->parent_card, COUNTER_CHARGE)); };);
		}
	}

	return 0;
}

int card_scroll_of_avacyn(int player, int card, event_t event){
/* CARD_ID_SCROLL_OF_AVACYN	12458
Scroll of Avacyn	|1
Artifact
|1, Sacrifice ~: Draw a card. If you control an Angel, you gain 5 life. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
		if( check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_ANGEL)  ){
			gain_life(player, 5);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL);
}

int card_scroll_of_griselbrand(int player, int card, event_t event){
/* CARD_ID_SCROLL_OF_GRISELBRAND	12459
Scroll of Griselbrand	|1
Artifact
|1, Sacrifice ~: Target opponent discards a card. If you control a Demon, that player loses 3 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
    td.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			discard(instance->targets[0].player, 0, player);
			if( check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_DEMON)  ){
				lose_life(instance->targets[0].player, 3);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_OPPONENT");
}

int card_tormentors_trident(int player, int card, event_t event){
/* CARD_ID_TORMENTORS_TRIDENT	12490
Tormentor's Trident	|2
Artifact - Equipment
Equipped creature gets +3/+0 and attacks each turn if able.
Equip |3 */

	return vanilla_equipment(player, card, event, 3, 3, 0, 0, SP_KEYWORD_MUST_ATTACK);
}

int card_vanguards_shield(int player, int card, event_t event){
/* CARD_ID_VANGUARDS_SHIELD	12500
Vanguard's Shield	|2
Artifact - Equipment
Equipped creature gets +0/+3 and can block an additional creature each combat.
Equip |3 (|3: Attach to target creature you control. Equip only as a sorcery.) */

	if( is_equipping(player, card) ){
		attached_creature_can_block_additional(player, card, event, 1);
	}

	return vanilla_equipment(player, card, event, 3, 0, 3, 0, 0);
}

int card_vessel_of_endless_rest(int player, int card, event_t event){
/* CARD_ID_VESSEL_OF_ENDLESS_REST	12502
Vessel of Endless Rest	|3
Artifact
When ~ enters the battlefield, put target card from a graveyard on the bottom of its owner's library.
|T: Add one mana of any color to your mana pool. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance( player, card );

		int targettable_players[2] = {0, 0};
		int i;
		for(i=0; i<2; i++){
			targettable_players[i] = count_graveyard(i) > 0 && ! graveyard_has_shroud(i) ? 1 : 0;
			if( targettable_players[i] && ! would_validate_arbitrary_target(&td, i, -1) ){
				targettable_players[i] = 0;
			}
		}

		if( targettable_players[0] + targettable_players[1] > 0 ){
			instance->targets[0].player = player;
			if( targettable_players[player] ){
				if( targettable_players[1-player] ){
					new_pick_target(&td, "Select a graveyard.", 0, 0 | GS_LITERAL_PROMPT);
				}
			}
			else{
				instance->targets[0].player = 1-player;
			}
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select target card to put on bottom of it's 'owner's library.");
			int ai_mode = instance->targets[0].player == player ? AI_MAX_VALUE : AI_MIN_VALUE;
			new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_GRAVE, TUTOR_BOTTOM_OF_DECK, 1, ai_mode, &test);
		}
	}

	return mana_producer(player, card, event);
}

// LANDS

int card_alchemists_refuge(int player, int card, event_t event)
{
  /* Alchemist's Refuge	""
   * Land
   * |T: Add |1 to your mana pool.
   * |G|U, |T: You may cast nonland cards this turn as though they had flash. */
  return enable_flash_land(player, card, event, MANACOST_GU(1,1), 0, TYPE_LAND, "Select a nonland card.", 1);
}

int card_cavern_of_souls(int player, int card, event_t event){
/* CARD_ID_CAVERN_OF_SOULS	12328
Cavern of Souls	""
Land
As ~ enters the battlefield, choose a creature type.
|T: Add |C to your mana pool.
|T: Add one mana of any color to your mana pool. Spend this mana only to cast a creature spell of the chosen type, and that spell can't be countered.*/

	// This is just an approximation

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		instance->targets[1].card = select_a_subtype(player, card);
		play_land_sound_effect_force_color(player, card, COLOR_TEST_ANY_COLORED);
		return 0;	// so the sound effect doesn't play again in mana_producer()
	}

	if(event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_tapped(player, card)){
		if( instance->targets[1].card > 0 && has_creature_type(affected_card_controller, affected_card_controller, instance->targets[1].card) ){
			card_ptr_t* c = cards_ptr[get_id(affected_card_controller, affected_card_controller)];
			if( c->req_black > 0 ){
				COST_BLACK-=1;
			}
			else if( c->req_blue > 0  ){
				COST_BLUE-=1;
			}
			else if( c->req_green > 0  ){
					COST_GREEN-=1;
			}
			else if( c->req_red > 0 ){
					COST_RED-=1;
			}
			else if( c->req_white > 0 ){
					COST_WHITE-=1;
			}
		}
	}

	if(! is_tapped(player, card)  && trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller){
		if( trigger_cause_controller == player && instance->targets[1].card > 0 ){
			int trig = 0;

			if( make_test_in_play(trigger_cause_controller, trigger_cause, -1, TYPE_CREATURE, 0, instance->targets[1].card, 0, 0, 0, 0, 0, -1, 0) ){
				trig = 1;
				instance->targets[1].player = 66;
			}
			if( trig > 0 ){
				if(event == EVENT_TRIGGER){
					event_result |= 2;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						tap_card(player, card);
						instance->targets[1].player = 0;
				}
			}
		}
	}

	if( instance->targets[1].player != 66 ){
		return mana_producer(player, card, event);
	}
	return 0;
}

int card_desolate_lighthouse(int player, int card, event_t event){
	/* Desolate Lighthouse	""
	 * Land
	 * |T: Add |C to your mana pool.
	 * |1|U|R, |T: Draw a card, then discard a card. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_DRAW_DISCARD
	};

	if( event == EVENT_ACTIVATE ){
		get_card_instance( player, card )->info_slot = 0;
		int choice = CHOICE_MANA;
		int can_use_ability = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_XUR(2, 1, 1), 0, NULL, NULL);
		if( ! paying_mana() && can_use_ability ){
			int priority_ab = hand_count[player] > 0 ? 14-(hand_count[player]*2) : 1;
			priority_ab += current_turn != player ? 5 : -5;
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 5,
							"Draw & discard", 1, priority_ab);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_DRAW_DISCARD ){
			add_state(player, card, STATE_TAPPED);
			if( ! charge_mana_for_activated_ability(player, card, MANACOST_XUR(1, 1, 1)) ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( get_card_instance( player, card )->info_slot == CHOICE_DRAW_DISCARD ){
			draw_cards(player, 1);
			discard(player, 0, player);
		}
	}

	return 0;
}

int card_seraphs_sanctuary(int player, int card, event_t event){
/* CARD_ID_SERAPH_SANCTUARY	12463
Seraph Sanctuary	""
Land
When ~ enters the battlefield, you gain 1 life.
Whenever an Angel enters the battlefield under your control, you gain 1 life.
|T: Add |C to your mana pool. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 1);
	}

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_ANGEL;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	return mana_producer(player, card, event);
}


int card_slayers_stronghold(int player, int card, event_t event){
/* Slayers' Stronghold	""
 * Land
 * |T: Add |C to your mana pool.
 * |R|W, |T: Target creature gets +2/+0 and gains vigilance and haste until end of turn. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_PUMP
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int choice = CHOICE_MANA;
		int can_use_ability = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_RW(1, 1), 0, &td, NULL);
		if( ! paying_mana() && can_use_ability ){
			int priority_ab = current_turn == player ? (current_phase < PHASE_DECLARE_ATTACKERS ? 15 : 1) :
														(current_phase == PHASE_AFTER_BLOCKING ? 15 : 1);
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 5,
							"+2/+0, vigilance and haste to target", 1, priority_ab);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_PUMP ){
			if( charge_mana_for_activated_ability(player, card, MANACOST_RW(1, 1)) ){
				pick_target(&td, "TARGET_CREATURE");
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PUMP ){
			if( valid_target(&td) ){
				pump_ability_until_eot(instance->parent_controller, instance->parent_card,
										instance->targets[0].player, instance->targets[0].card,
										2, 0, 0, SP_KEYWORD_HASTE | SP_KEYWORD_VIGILANCE);
			}
		}
	}

	return 0;
}

