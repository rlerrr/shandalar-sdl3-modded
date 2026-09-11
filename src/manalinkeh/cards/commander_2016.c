#include "manalink.h"

// Functions

static int basic_landcycling(int player, int card, event_t event)
{

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND )
		return cycling(player, card, event, MANACOST_X(2));
	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND )
		tutor_basic_land(player, 0, 0);
	return 0;
}

// White
int card_duelists_heritage(int player, int card, event_t event)
{
/*
Duelist's Heritage |2|W	0x200f593
Enchantment
Whenever one or more creatures attack, you may have target attacking creature gain double strike until end of turn.
*/
	int trig_mode = IS_AI(player) ? (current_turn == player ? RESOLVE_TRIGGER_MANDATORY : 0) : RESOLVE_TRIGGER_OPTIONAL;
	if( declare_attackers_trigger(player, card, event, trig_mode, ANYBODY, -1) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_state = TARGET_STATE_ATTACKING;
        td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target attacking creature.", 0, GS_LITERAL_PROMPT) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_DOUBLE_STRIKE, 0);
		}
		instance->number_of_targets = 0;
	}
	return global_enchantment(player, card, event);
}

int card_entrapment_maneuver(int player, int card, event_t event){
/*
Entrapment Maneuver |3|W	0x200f598
Instant
Target player sacrifices an attacking creature. You create X 1/1 white Soldier creature tokens, where X is that creature's toughness
*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a attacking creature to sacrifice.");
			test.state = STATE_ATTACKING;
			int sac = new_sacrifice(player, card, instance->targets[0].player, SAC_JUST_MARK|SAC_RETURN_CHOICE|SAC_CAUSED|SAC_NO_CANCEL, &test);
			if( sac ){
				int amount = get_toughness(BYTE2(sac), BYTE3(sac));
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				generate_tokens_by_id(player, card, CARD_ID_SOLDIER, amount);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int orzhov_advokist_legacy(int player, int card, event_t event){
	if( upkeep_trigger(player, card, event) )
		kill_card(player, card, KILL_EXILE);
	nobody_can_attack(player, card, event, 1-player);
	return 0;
}

int card_orzhov_advokist(int player, int card, event_t event){
/*
Orzhov Advokist |2|W	0x200f59d
Creature - Human Advisor 1/4
At the beginning of your upkeep, each player may put two +1/+1 counters on a creature her or she controls.
If a player does, creatures that player controls can't attack you or a planeswalker you control until your next upkeep.
*/

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);
		APNAP(p,{
					target_definition_t td;
					default_target_definition(player, card, &td, TYPE_CREATURE);
					td.allowed_controller = td.preferred_controller = p;
					td.who_chooses = p;
					td.illegal_abilities = 0;

					if( can_target(&td) && new_pick_target(&td, "Select a creature you control.", 0, GS_LITERAL_PROMPT) ){
						add_1_1_counters(instance->targets[0].player, instance->targets[0].card, 2);
						if( instance->targets[0].player != player ){
							int legacy = create_legacy_effect(player, card, &orzhov_advokist_legacy);
							remove_state(player, legacy, STATE_PROCESSING);
						}
					}
					instance->number_of_targets = 0;
				}
		);
	}

	return 0;
}

int card_selfless_squire(int player, int card, event_t event){
/*
Selfless Squire |3|W	0x200f5a2
Creature - Human Soldier 1/1
Flash
When Selfless Squire enters the battlefield, prevent all damage that would be dealt to you this turn.
Whenever damage that would be dealt to you is prevented, put that many +1/+1 counters on Selfless Squire.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		prevent_all_damage_to_target(player, card, player, -1, 1);
	}

	if( event == EVENT_CAN_CAST ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.extra = damage_card;
		td.special = TARGET_SPECIAL_DAMAGE_PLAYER;
		if( generic_spell(player, card, event, GS_DAMAGE_PREVENTION | GS_CAN_TARGET, &td, NULL, 1, NULL) ){
			return 99;
		}
		return 1;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn == 1-player ? 24 : -24);
	}

	return 0;
}

int card_sublime_exhalation(int player, int card, event_t event){
/*
Sublime Exhalation |6|W	0x200f5a7
Sorcery
Undaunted (This spell costs 1 less to cast for each opponent.)
Destroy all creatures.
*/
	if( event == EVENT_MODIFY_COST ){
		if( player > -1 ){
			COST_COLORLESS --;
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		APNAP(p, { new_manipulate_all(player, card, p, &test, KILL_DESTROY); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}


// Blue

int card_coastal_breach(int player, int card, event_t event){
/*
Coastal Breach |6|U	0x200f5ac
Sorcery
Undaunted (This spell costs 1 less to cast for each opponent.)
Return all nonland permanents to their owners' hands.
*/
	if( event == EVENT_MODIFY_COST ){
		if( player > -1 ){
			COST_COLORLESS --;
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.type_flag = DOESNT_MATCH;
		APNAP(p, { new_manipulate_all(player, card, p, &test, ACT_BOUNCE); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_deepglow_skate(int player, int card, event_t event){
/*
Deepglow Skate |4|U	0x200f5b1
Creature - Fish 3/3
When Deepglow Skate enters the battlefield, double the number of each kind of counter on any number of target permanents.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.preferred_controller = player;
		td.allow_cancel = 2; //"done" button

		card_instance_t *instance= get_card_instance(player, card);

		int i;
		for(i=0; i<10; i++){
			if( can_target(&td) && new_pick_target(&td, "TARGET_PERMANENT", i, 0) ){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
			}
			else{
				break;
			}
		}
		for(i=0; i<instance->number_of_targets; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			copy_counters(instance->targets[i].player, instance->targets[i].card, instance->targets[i].player, instance->targets[i].card, 0);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_farie_artisans(int player, int card, event_t event){
/*
Faerie Artisans |3|U	0x200f5b6
Creature - Faerie Artificer 2/2
Flying
Whenever a nontoken creature enters the battlefield under an opponent's control, create a token that's a copy of that creature except that
	it's an artifact in addition to all its other types. Then exile all other tokens created with Faerie Artisans.
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.type_flag = F1_NO_TOKEN;

		etb_ability_global(1-player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		int marked[2][151] = {{0}};
		APNAP(p,{
					int c;
					for(c = 0; c<active_cards_count[p]; c++){
						if( in_play(p, c) && is_what(p, c, TYPE_PERMANENT) && is_token(p, c) ){
							card_instance_t *inst = get_card_instance(p, c);
							if( inst->damage_source_player == p && inst->damage_source_card == c ){
								marked[p][c] = 1;
							}
						}
					}
				}
		);

		int csvid = get_id(trigger_cause_controller, trigger_cause);
		token_generation_t token;
		default_token_definition(player, card, csvid, &token);
		token.action = TOKEN_ACTION_CONVERT_INTO_ARTIFACT;
		generate_token(&token);

		APNAP(p,{
					int c;
					for(c = 150; c > -1; c--){
						if( marked[p][c] ){
							kill_card(p, c, KILL_EXILE);
						}
					}
				}
		);

	}

	return 0;
}

static void equip_germ(token_generation_t* token, int germ, int number)
{
	if( number != 0 )
		return;
	equip_target_creature(token->s_player, token->s_card, token->t_player, germ);
}

int card_grip_of_phyresis(int player, int card, event_t event){
/*
Grip of Phyresis |2|U	0x200f5bb
Instant
Gain control of target Equipment, then create a 0/0 black Germ creature token and attach that Equipment to it.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.required_subtype = SUBTYPE_EQUIPMENT;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int new_equip_player = player;
			int new_equip_card = gain_control_permanently(player, instance->targets[0].player, instance->targets[0].card);
			if( new_equip_card == -1 ){
				new_equip_player = instance->targets[0].player;
				new_equip_card = instance->targets[0].card;
			}
			token_generation_t token;
			default_token_definition(new_equip_player, new_equip_card, CARD_ID_GERM, &token);
			token.t_player = player;
			token.special_code_on_generation = &equip_germ;
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_EQUIPMENT", 1, NULL);
}

int card_manifold_insights(int player, int card, event_t event){
/*
Manifold Insights |2|U	0x200f5c0
Sorcery
Reveal the top ten cards of your library. Starting with the next opponent in turn order, each opponent chooses a different nonland card from
among them. Put the chosen cards into your hand and the rest on the bottom of your library in a random order.
*/
	if( event == EVENT_RESOLVE_SPELL ){
		int amount = MIN(10, count_deck(player));
		if( amount ){
			show_deck(0, deck_ptr[player], amount, "Cards revealed by Manifold Insights", 0, 0x7375B0 );
			show_deck(1, deck_ptr[player], amount, "Cards revealed by Manifold Insights", 0, 0x7375B0 );
			int found_nonland = 0;
			int i;
			for(i=0; i<amount; i++){
				if( ! is_what(-1, deck_ptr[player][i], TYPE_LAND) ){
					found_nonland = 1;
					break;
				}
			}
			if( found_nonland ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_LAND, "Select a nonland card.");
				test.type_flag = DOESNT_MATCH;
				test.create_minideck = amount;
				test.no_shuffle = 1;
				if( new_global_tutor(1-player, player, TUTOR_FROM_DECK, TUTOR_TPLAYER_HAND, 1, AI_MIN_VALUE, &test) > -1 ){
					amount--;
				}
			}
			if( amount ){
				put_top_x_on_bottom_in_random_order(player, amount);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

// Black

/*	09/02/2017: Impossible to do with the current engine.
Cruel Entertainment |6|B
Sorcery
Choose target player and another target player. The first player controls the second player during the second player's next turn, and the second player controls the first player during the first player's next turn.
*/

/*	09/02/2017: Skipped. It's worthless on 1 vs 1.
Curse of Vengeance |B
Enchantment - Aura Curse
Enchant player
Whenever enchanted player casts a spell, put a spite counter on Curse of Vengeance.
When enchanted player loses the game, you gain X life and you draw X cards, where X is the number of spite counters on Curse of Vengeance.
*/

int card_curtains_call(int player, int card, event_t event){
/*
Curtains' Call |5|B	0x200f5c5
Instant
Undaunted (This spell costs 1 less to cast for each opponent.)
Destroy two target creatures.
*/
	if( event == EVENT_MODIFY_COST ){
		if( player > -1 ){
			COST_COLORLESS --;
		}
		return 0;
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

int card_magus_of_the_will(int player, int card, event_t event){
/*
Magus of the Will |2|B	0x200f5ca
Creature - Human Wizard 3/3
2B, t, Exile Magus of the Will: Until end of turn, you may play cards from your graveyard.
	If a card would be put into your graveyard from anywhere this turn, exile that card instead.
*/

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (count_graveyard(player) * 4);
		ai_modifier += (has_mana(player, COLOR_ANY, 1) * 4);
	}

	if (event == EVENT_RESOLVE_ACTIVATION )
	{
		create_legacy_activate(player, card, &effect_yawgmoths_will);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_RFG_ME, MANACOST_XB(2, 1), 0, NULL, NULL);
}

int card_parting_thoughts(int player, int card, event_t event){
/*
Parting Thoughts |2|B	0x200f5cf
Sorcery
Destroy target creature. You draw X cards and you lose X life, where X is the number of counters on that creature.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = count_counters(instance->targets[0].player, instance->targets[0].card, COUNTER_invalid);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			draw_cards(player, amount);
			lose_life(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}


// Red
int card_charging_cinderhorn(int player, int card, event_t event){
/*
Charging Cinderhorn |3|R	0x200f5d4
Creature - Elemental Ox 4/2
Haste
At the beginning of each player's end step, if no creatures attacked this turn, put a fury counter on Charging Cinderhorn.
Charging Cinderhorn deals damage to that player equal to the number of fury counters on it to that player.
*/
	haste(player, card, event);
	// Can't use "store_attackers" as it's cleared before "eot_trigger"
	if (event == EVENT_DECLARE_ATTACKERS && !is_humiliated(player, card))
	{
		if (number_of_attackers_declared == 0)
			return 0;

		card_instance_t* instance = get_card_instance(player, card);
		int c;
		for (c = 0; c <= active_cards_count[current_turn] - 1; ++c){
			if( in_play_and_attacking(current_turn, c) )
			{
				instance->info_slot++;
			}
		}
	}

	if( trigger_condition == TRIGGER_EOT &&  affect_me(player, card ) && reason_for_trigger_controller == player ){
		card_instance_t *instance = get_card_instance( player, card);
		int trig = instance->info_slot ? 0 : 1;
		instance->info_slot = 0;
		if( trig && eot_trigger(player, card, event) ){
			add_counter(player, card, COUNTER_FURY);
			damage_player(current_turn, count_counters(player, card, COUNTER_FURY), player, card);
		}
	}

	return 0;
}

int card_divergent_transformations(int player, int card, event_t event){
/*
Divergent Transformations |6|R	0x200f5d9
Instant
Undaunted (This spell costs 1 less to cast for each opponent.)
Exile two target creatures. For each of those creatures, its controller reveals cards from the top of his or her library until he or she
	reveals a creature card, puts that card onto the battlefield, then shuffles the rest into his or her library.
*/
	if( event == EVENT_MODIFY_COST ){
		if( player > -1 ){
			COST_COLORLESS --;
		}
		return 0;
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int exiled[2] = {-1};
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_EXILE);
				exiled[i] = instance->targets[i].player;
			}
		}
		for(i=0; i<2; i++){
			if( exiled[i] > -1 ){
				test_definition_t test;
				default_test_definition(&test, TYPE_CREATURE);
				reveal_card_until_you_reveal_test(exiled[i], exiled[i], &test, RCUYRT_PUT_TEST_INTO_PLAY | RCUYRT_SHUFFLE_REST);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

int card_frenzied_fugue(int player, int card, event_t event){
/*
Frenzied Fugue |3|R	0x200f5de
Enchantment - Aura
Enchant permanent
When Frenzied Fugue enters the battlefield or at the beginning of your upkeep, gain control of target permanent until end of turn.
	Untap that permanent. It gains haste until end of turn.
*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);
		untap_card(instance->damage_target_player, instance->parent_card);
		pump_ability_until_eot(player, card, instance->damage_target_player, instance->parent_card, 0, 0, 0, SP_KEYWORD_HASTE);
		instance->info_slot = 1;
	}

	if( get_card_instance(player, card)->info_slot == 1 && eot_trigger(player, card, event) ){
		get_card_instance(player, card)->info_slot = 0;
		end_of_stealing_aura_effect(player, card);
		remove_state(player, card, STATE_POWER_STRUGGLE); //AKA this is no more an active "stealing" effect and won't be taken in account when dealing with other "stealing effects"
	}

	if( current_turn == player && upkeep_trigger(player, card, event) ){
		card_instance_t *instance = get_card_instance(player, card);
		gain_control_permanently(player, instance->damage_target_player, instance->damage_target_card);
		add_state(player, card, STATE_POWER_STRUGGLE); //AKA reactivating this as "stealing" effect
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return aot_like_spell(player, card, event, &td, "TARGET_CREATURE", 0);
	}

	return generic_stealing_aura(player, card, event, &td, "TARGET_CREATURE");
}

int card_goblin_spymaster(int player, int card, event_t event){
/*
Goblin Spymaster |2|R	0x200f5e3
Creature - Goblin Rogue 2/1
First strike
At the beginning of each opponent's end step, that player creates a 1/1 red Goblin creature token with "Creatures you control attack each combat if able."
*/

	if( current_turn != player && eot_trigger(player, card, event) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_GOBLIN, &token);
		token.legacy = 1;
		token.special_infos = 68;
		generate_token(&token);
	}

	return 0;
}

int card_runehorn_hellkite(int player, int card, event_t event){
/*
Runehorn Hellkite |5|R	0x200f5e8
Creature - Dragon 5/5
Flying
5R, Exile Runehorn Hellkite from your graveyard: Each player discards his or her hand, then draws seven cards.
*/
	if( event == EVENT_GRAVEYARD_ABILITY ){
		if( has_mana_multi(player, MANACOST_XR(5, 1)) ){
			return GA_BASIC;
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		charge_mana_multi(player, MANACOST_XR(5, 1));
		if( spell_fizzled != 1 ){
			return GAPAID_EXILE;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATED_GRAVEYARD_ABILITY ){
		APNAP(p,{
					new_discard_all(p, player);
					draw_cards(p, 7);
				};
		);
	}

	return 0;
}

// Green
int benefactors_draught_legacy(int player, int card, event_t event){

	if( event == EVENT_DECLARE_BLOCKERS && affected_card_controller == 1-player && blocking(affected_card_controller, affected_card, event) ){
		draw_a_card(player);
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_benefactors_draught(int player, int card, event_t event){
/*
Benefactor's Draught |1|G	0x200f5ed
instant
Untap all creatures. Until end of turn, whenever a creature an opponent controls blocks, draw a card.
Draw a card.
*/
	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		APNAP(p,{ new_manipulate_all(player, card, p, &test, ACT_UNTAP); };);
		create_legacy_effect(player, card, &benefactors_draught_legacy);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_evolutionary_escalation(int player, int card, event_t event){
/*
Evolutionary Escalation |1|G	0x200f5f2
Enchantment
At the beginning of your upkeep, put three +1+1 counters on target creature you control and three +1/+1 counters on target creature an
	opponent controls.
*/
	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);
		APNAP(p,{
					td.allowed_controller = td.preferred_controller = p;
					char msg[100];
					if( p == player ){
						scnprintf(msg, 100, "Select a creature you control.");
					}
					else{
						scnprintf(msg, 100, "Select a creature your opponent controls.");
					}
					if( can_target(&td) && new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
						add_1_1_counters(instance->targets[0].player, instance->targets[0].card, 2);
					}
					instance->number_of_targets = 0;
				}
		);
	}

	return global_enchantment(player, card, event);
}

int card_primeval_protector(int player, int card, event_t event){
/*
Primeval Protector |10|G	0x200f5f7
Creature - Avatar 10/10
Primeval Protector costs 1 less to cast for each creature your opponents control.
When Primeval Protector enters the battlefield, put a +1/+1 counter on each other creature you control.
*/
	if( event == EVENT_MODIFY_COST ){
		if( player > -1 ){
			COST_COLORLESS -= count_subtype(1-player, TYPE_CREATURE, -1);
		}
		return 0;
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		new_manipulate_all(player, card, player, &test, ACT_ADD_COUNTERS(COUNTER_P1_P1, 2));
	}

	return 0;
}

int card_seeds_of_renewal(int player, int card, event_t event)
{
/*
Seeds of Renewal |6|G	0x200f5fc
Sorcery
Undaunted (This spell costs 1 less to cast for each opponent.)
Return up to two target cards from your graveyard to your hand. Exile Seeds of Renewal.
*/

	if( event == EVENT_MODIFY_COST ){
		if( player > -1 ){
			COST_COLORLESS --;
		}
		return 0;
	}

	if (!IS_GS_EVENT(player, card, event))
		return 0;
	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select up to two target cards.");
	int rval = spell_return_up_to_n_target_cards_from_your_graveyard_to_your_hand(player, card, event, 2, &test, 1);
	if (event == EVENT_RESOLVE_SPELL)
	{
		if (spell_fizzled != 1)
			discard(player, 0, player);
		kill_card(player, card, KILL_DESTROY);
	}
	return rval;
}

int card_stonehoof_chieftain(int player, int card, event_t event){
/*
Stonehoof Chieftain |7|G	0x200f601
Creature - Centaur Warrior 8/8
Trample, indestructible
Whenever another creature you control attacks, it gains trample and indestructible until end of turn.
*/
	indestructible(player, card, event);

	if (event == EVENT_DECLARE_ATTACKERS && !is_humiliated(player, card))
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.not_me = 1;
		store_attackers(player, card, event, DAT_TRACK, player, -1, &test);
	}

	int amt;
	if ((amt = resolve_declare_attackers_trigger(player, card, event, DAT_TRACK)))
	{
		card_instance_t* instance = get_card_instance(player, card);
		unsigned char* attackers = (unsigned char*)(&instance->targets[2].player);
		for (--amt; amt >= 0; --amt)
			if (in_play(current_turn, attackers[amt]))
				pump_ability_until_eot(player, card, current_turn, attackers[amt], 0, 0, KEYWORD_TRAMPLE, SP_KEYWORD_INDESTRUCTIBLE);
	}

	if( event == EVENT_CLEANUP )
		store_attackers(player, card, event, DAT_TRACK, player, -1, NULL);

	return 0;
}

// Gold
int card_akiri_line_slinger(int player, int card, event_t event){
/*
Akiri, Line-Slinger |R|W	0x200f606
Legendary Creature - Kor Soldier Ally 0/3
First strike, vigilance
Akiri, Line-Slinger gets +1/+0 for each artifact you control.
Partner (You can have two commanders if both have partner.)
*/
	check_legend_rule(player, card, event);
	vigilance(player, card, event);
	if( event == EVENT_POWER && affect_me(player, card) && ! is_humiliated(player, card) )
		event_result += count_subtype(player, TYPE_ARTIFACT, -1);
	return 0;
}

int card_ancient_excavation(int player, int card, event_t event){
/*
Ancient Excavation |2|U|B	0x200f60b
Instant
Draw cards equal to the number of cards in your hand, then discard a card for each card drawn this way.
Basic landcycling 2 (2, Discard this card: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library.)
*/

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return basic_landcycling(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (hand_count[player] * 8);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int amount_before = hand_count[player];
		draw_cards(player, amount_before);
		int diff = hand_count[player] - amount_before;
		if( diff ){
			new_multidiscard(player, diff, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_atraxa_praetors_voice(int player, int card, event_t event){
/*
Atraxa, Praetors' Voice |G|W|U|B	0x200f610
Legendary Creature - Angel Horror 4/4
Flying, vigilance, deathtouch, lifelink
At the beginning of your end step, proliferate.
(You choose any number of permanents and/or players with counters on them, then give each another counter of a kind already there.)
*/
	check_legend_rule(player, card, event);
	vigilance(player, card, event);
	lifelink(player, card, event);
	deathtouch(player, card, event);
	if( current_turn == player && eot_trigger(player, card, event) )
		proliferate(player, card);
	return 0;
}

int card_breya_etherium_shaper(int player, int card, event_t event){
	/* Breya, Etherium Shaper	|W|U|B|R	0x200f615
	 * Legendary Artifact Creature - Human 4/4
	 * When ~ enters the battlefield, create two 1/1 |Sblue Thopter artifact creature tokens with flying.
	 * |2, Sacrifice two artifacts: Choose one -
	 * * ~ deals 3 damage to target player or planeswalker.
	 * * Target creature gets -4/-4 until end of turn.
	 * * You gain 5 life. */
	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_THOPTER, &token);
		token.qty = 2;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		generate_token(&token);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_DAMAGE_PLAYER = 1,
		CHOICE_M4_M4,
		CHOICE_5_LIFE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_sacrifice_type_as_cost(player, 2, TYPE_ARTIFACT) ){
			return generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		int abils[4] = {0, can_target(&td), can_target(&td2), 1};
		int pr[4] = {
						0,
						life[1-player] < 6 ? 10 : 1,
						5,
						life[player] < 6 ? 10 : 1,
		};
 		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"3 damage to target player", abils[1], pr[1],
							"-4/-4 to target creature", abils[2], pr[2],
							"Gain 5 life", abils[3], pr[3]);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		test_definition_t test;
		new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");
		int sacs[2] = {-1};
		int i;
		for(i=0; i<2; i++){
			sacs[i] = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			if( ! sacs[i] ){
				break;
			}
		}
		for(i=0; i<2; i++){
			if( sacs[i] ){
				state_untargettable(BYTE2(sacs[i]), BYTE3(sacs[i]), 0);
			}
			else{
				spell_fizzled = 1;
			}
		}
		if( spell_fizzled == 1 ){
			return 0;
		}
		if( choice == CHOICE_DAMAGE_PLAYER ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_PLAYER_OR_PLANESWALKER");
		}
		if( choice == CHOICE_M4_M4 ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(2), 0, &td2, "TARGET_CREATURE");
		}
		if( choice == CHOICE_5_LIFE ){
			generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL);
		}
		if( spell_fizzled != 1 ){
			for(i=0; i<2; i++){
				kill_card(BYTE2(sacs[i]), BYTE3(sacs[i]), KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DAMAGE_PLAYER && valid_target(&td) ){
			damage_target0(player, card, 3);
		}
		if( instance->info_slot == CHOICE_M4_M4 && valid_target(&td2) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							-4, -4);
		}
		if( instance->info_slot == CHOICE_5_LIFE ){
			gain_life(player, 5);
		}
	}

	return 0;
}

int card_bruse_tarl_boorish_herder(int player, int card, event_t event){
/*
Bruse Tarl, Boorish Herder |2|R|W	0x200f61a
Legendary Creature - Human Ally 3/3
Whenever Bruse Tarl, Boorish Herder enters the battlefield or attacks, target creature you control gains double strike and lifelink until
	end of turn.
Partner (You can have two commanders if both have partner.)
*/
	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = td.allowed_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target creature you control.", 0, GS_LITERAL_PROMPT) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
									0, 0, KEYWORD_DOUBLE_STRIKE, SP_KEYWORD_LIFELINK);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_grave_upheaval(int player, int card, event_t event){
/*
Grave Upheaval |4|B|R	0x200f61f
Sorcery
Put target creature card from a graveyard onto the battlefield under your control. It gains haste.
Basic landcycling 2 ( 2, Discard this card: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library.)
*/

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return basic_landcycling(player, card, event);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
	this_test.ai_selection_mode = AI_MAX_CMC;

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			int zombo = reanimate_permanent(player, card, instance->targets[0].player, selected, REANIMATE_NO_CONTROL_LEGACY);
			pump_ability_until_eot(player, card, player, zombo, 0, 0, 0, SP_KEYWORD_HASTE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &this_test);
}

int card_ikra_shidiqi_the_usurper(int player, int card, event_t event){
/*
Ikra Shidiqi, the Usurper |3|B|G	0x200f624
Legendary Creature - Naga Wizard 3/7
Menace
Whenever a creature you control deals combat damage to a player, you gain life equal to that creature's toughness.
Partner (You can have two commanders if both have partner.)
*/
	check_legend_rule(player, card, event);
	menace(player, card, event);

	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);
	check_damage_test(player, -1, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGING_SOURCES | DDBM_MUST_BE_COMBAT_DAMAGE, player, card,
						0, &test);

	int result = resolve_damage_trigger(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGING_SOURCES |
										DDBM_MUST_BE_COMBAT_DAMAGE, player, card);
	if( result ){
		card_instance_t *instance = get_card_instance(player, card);
		int i;
		for(i=0; i<result; i++){
			gain_life(player, get_toughness(instance->targets[i].player, instance->targets[i].card));
			instance->targets[i].player = instance->targets[i].card = -1;
		}
	}

	return 0;
}

int card_ishai_ojutai_dragonspeaker(int player, int card, event_t event){
/*
Ishai, Ojutai Dragonspeaker |2|W|U	0x200f629
Legendary Creature - Bird Monk 1/1
Flying
Whenever an opponent casts a spell, put a +1/+1 counter on Ishai, Ojutai Dragonspeaker.
Partner (You can have two commanders if both have partner.)
*/

	check_legend_rule(player, card, event);

	if( new_specific_spell_played(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, NULL) )
		add_1_1_counter(player, card);

	return 0;
}

int card_kraum_ludevics_opus(int player, int card, event_t event){
/*
Kraum, Ludevic's Opus |3|U|R	0x200f62e
Legendary Creature - Zombie Horror 4/4
Flying, haste
Whenever an opponent casts his or her second spell each turn, draw a card.
Partner (You can have two commanders if both have partner.)
*/

	check_legend_rule(player, card, event);
	haste(player, card, event);

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && get_specific_storm_count(1-player) == 2 ){
		if( new_specific_spell_played(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
			draw_a_card(player);
		}
	}

	return 0;
}

int card_kydele_chosen_of_kruphix(int player, int card, event_t event){
/*
Kydele, Chosen of Kruphix |2|G|U	0x200f633
Legendary Creature - Human Wizard 2/3
T: Add C to your mana pool for each card you've drawn this turn.
Partner (You can have two commanders if both have partner.)
*/
	check_legend_rule(player, card, event);

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && mana_producer(player, card, EVENT_CAN_ACTIVATE) ){
		declare_mana_available(player, COLOR_COLORLESS, cards_drawn_this_turn[player]);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( IS_AI(player) && cards_drawn_this_turn[player] == 0 ){
			return 0;
		}
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		produce_mana_tapped(player, card, COLOR_COLORLESS, cards_drawn_this_turn[player]);
	}

	return 0;
}

int card_kynaios_and_tiro_of_meletis(int player, int card, event_t event){
/*
Kynaios and Tiro of Meletis |R|G|W|U	0x200f638
Legendary Creature - Human Soldier 2/8
At the beginning of your end step, draw a card. Each player may put a land card from his or her hand onto the battlefield,
	then each opponent who didn't draws a card.
*/
	check_legend_rule(player, card, event);

	if( current_turn == player && eot_trigger(player, card, event) ){
		draw_cards(player, 1);
		APNAP(p,{
					if( hand_count[p] ){
						test_definition_t test;
						new_default_test_definition(&test, TYPE_LAND, "Select a land card.");
						int result = new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
						if( result == -1 && p != player ){
							draw_a_card(p);
						}
					}
				}
		);
	}

	return 0;
}

int card_ludevic_necro_alchemist(int player, int card, event_t event){
/*
Ludevic, Necro-Alchemist |1|U|R	0x200f63d
Legendary Creature - Human Wizard 1/4
At the beginning of each player's end step, that player may draw a card if a player other than you lost life this turn.
Partner (You can have two commanders if both have partner.)
*/
	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_EOT && affect_me(player, card) ){
		if( get_trap_condition(1-player, TRAP_LIFE_LOST) ){
			draw_some_cards_if_you_want(player, card, current_turn, 1);
		}
	}

	return 0;
}

int card_migratory_route(int player, int card, event_t event){
/*
Migratory Route |3|W|U	0x200f642
Sorcery
Create four 1/1 white Bird creature tokens with flying.
Basic landcycling 2 (2, Discard this card: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library.)
*/

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return basic_landcycling(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_BIRD, 4);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_ravos_soultender(int player, int card, event_t event){
/*
Ravos, Soultender |3|W|B	0x200f647
Legendary Creature - Human Cleric 2/2
Flying
Other creatures you control get +1/+1.
At the beginning of your upkeep, you may return target creature card from your graveyard to your hand.
Partner (You can have two commanders if both have partner.)
*/
	check_legend_rule(player, card, event);

	boost_creature_type(player, card, event, -1, 1, 1, 0, BCT_CONTROLLER_ONLY);

	upkeep_trigger_ability_mode(player, card, event, player,
								count_graveyard_by_type(player, TYPE_CREATURE) && ! graveyard_has_shroud(player) ?
								RESOLVE_TRIGGER_AI(player) : 0);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");
		new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
	}

	return 0;
}

static int reyhan_last_of_the_abzan_effect(int player, int card, event_t event){

	//In case something is copying Pawn of Ulamog and then reverts to its original form.
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_REYHAN_LAST_OF_THE_ABZAN ){
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
				if( is_this_dying(affected_card_controller, affected_card) &&
					is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
					count_1_1_counters(affected_card_controller, affected_card) )
				{
					if( instance->targets[11].player < 0 )
						instance->targets[11].player = 0;
					int pos = MAX(1, BYTE0(instance->targets[11].player));
					while( pos < 10 ){
							if( instance->targets[pos].player == -1 ){
								instance->targets[pos].player = count_1_1_counters(affected_card_controller, affected_card);
								break;
							}
							if( instance->targets[pos].card == -1 ){
								instance->targets[pos].card = count_1_1_counters(affected_card_controller, affected_card);
								break;
							}
							pos++;
					}
					SET_BYTE0(instance->targets[11].player) = pos;
				}
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			card_instance_t *instance = get_card_instance(player, card);
			int recorded = BYTE0(result);
			int stored[16];
			int sc = 0;
			int i;
			for(i=1; i<recorded; i++){
				int amount = instance->targets[i].player;
				if( amount == -1 ){
					break;
				}
				instance->targets[i].player = -1;
				stored[sc] = amount;
				sc++;

				amount = instance->targets[i].card;
				if( amount == -1 ){
					break;
				}
				instance->targets[i].card = -1;
				stored[sc] = amount;
				sc++;
			}
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.preferred_controller = player;

			for(i=0; i<sc; i++){
				char msg[100];
				scnprintf(msg, 100, "Select target creature (+%d/+%d)", stored[i], stored[i]);
				if( can_target(&td) && new_pick_target(&td, msg, 1, GS_LITERAL_PROMPT) ){
					add_1_1_counters(instance->targets[1].player, instance->targets[1].card, stored[i]);
					instance->number_of_targets = 1;
				}
				else{
					break;
				}
			}
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

int card_reyhan_last_of_the_abzan(int player, int card, event_t event){
/*
Reyhan, Last of the Abzan |1|B|G	0x200f64c
Legendary Creature - Human Warrior 0/0
Reyhan, Last of the Abzan enters the battlefield with three +1/+1 counters on it.
Whenever a creature you control dies or is put into the command zone, if it had one or more +1/+1 counters on it,
	you may put that many +1/+1 counters on target creature.
Partner (You may have two commanders if both have partner.)
*/
	check_legend_rule(player, card, event);

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)reyhan_last_of_the_abzan_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &reyhan_last_of_the_abzan_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY && in_play(affected_card_controller, affected_card) ){
		if( is_this_dying(affected_card_controller, affected_card) && is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
			count_1_1_counters(affected_card_controller, affected_card) )
		{
			card_instance_t *instance = get_card_instance(player, card);
			if( instance->targets[11].player < 0 )
				instance->targets[11].player = 0;
			int pos = BYTE0(instance->targets[11].player);
			while( pos < 10 ){
					if( instance->targets[pos].player == -1 ){
						instance->targets[pos].player = count_1_1_counters(affected_card_controller, affected_card);
						break;
					}
					if( instance->targets[pos].card == -1 ){
						instance->targets[pos].card = count_1_1_counters(affected_card_controller, affected_card);
						break;
					}
					pos++;
			}
			SET_BYTE0(instance->targets[11].player) = pos;
		}
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			card_instance_t *instance = get_card_instance(player, card);
			int recorded = BYTE0(result);
			int stored[18];
			int sc = 0;
			int i;
			for(i=0; i<recorded; i++){
				int amount = instance->targets[i].player;
				if( amount == -1 ){
					break;
				}
				instance->targets[i].player = -1;
				stored[sc] = amount;
				sc++;

				amount = instance->targets[i].card;
				if( amount == -1 ){
					break;
				}
				instance->targets[i].card = -1;
				stored[sc] = amount;
				sc++;
			}
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.preferred_controller = player;

			for(i=0; i<sc; i++){
				char msg[100];
				scnprintf(msg, 100, "Select target creature (+%d/+%d)", stored[i], stored[i]);
				if( can_target(&td) && new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
					add_1_1_counters(instance->targets[0].player, instance->targets[0].card, stored[i]);
					instance->number_of_targets = 0;
				}
				else{
					break;
				}
			}
		}
	}

	return 0;
}

int card_saskia_the_unyielding(int player, int card, event_t event){
/*
Saskia the Unyielding |B|R|G|W	0x200f651
Legendary Creature - Human Soldier 3/4
Vigilance, haste
As Saskia the Unyielding enters the battlefield, choose a player.
Whenever a creature you control deals combat damage to a player, it deals that much damage to the chosen player.
*/
	check_legend_rule(player, card, event);
	vigilance(player, card, event);
	haste(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = td.illegal_abilities = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( new_pick_target(&td, "Select a player.", 0, GS_LITERAL_PROMPT) ){
			SET_BYTE1(instance->info_slot) = instance->targets[0].player;
			instance->number_of_targets = 0;
		}
	}

	if( event == EVENT_DEAL_DAMAGE && ! is_humiliated(player, card) ){
		card_instance_t *dmg = get_card_instance(affected_card_controller, affected_card);
		if( dmg->internal_card_id == damage_card &&
			dmg->info_slot > 0 &&
			dmg->damage_target_card == -1 &&
			! damage_is_to_planeswalker(dmg) &&
			dmg->damage_source_player == player &&
			is_what(dmg->damage_source_player, dmg->damage_source_card, TYPE_CREATURE) &&
			dmg->token_status & (STATUS_COMBAT_DAMAGE|STATUS_FIRST_STRIKE_DAMAGE) )
		{
			card_instance_t *instance = get_card_instance(player, card);
			int pos = BYTE0(instance->info_slot);
			while( pos < 10 ){
					int *storage = &instance->targets[pos].player;
					if( ! storage ){
						SET_BYTE0(storage) = dmg->damage_source_player;
						SET_BYTE1(storage) = dmg->damage_source_card;
						SET_BYTE2(storage) = dmg->info_slot;
						break;
					}
					storage = &instance->targets[pos].card;
					if( ! storage ){
						SET_BYTE0(storage) = dmg->damage_source_player;
						SET_BYTE1(storage) = dmg->damage_source_card;
						SET_BYTE2(storage) = dmg->info_slot;
						break;
					}
					pos++;
			}
			SET_BYTE0(instance->info_slot) = pos;
			SET_BYTE2(instance->info_slot)++;
		}
	}

	if( trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
		reason_for_trigger_controller == player && BYTE2(get_card_instance(player, card)->info_slot) )
	{
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				card_instance_t *instance = get_card_instance(player, card);
				int pos = BYTE0(instance->info_slot);
				SET_BYTE0(instance->info_slot) = 0;
				SET_BYTE2(instance->info_slot) = 0;
				int i;
				for(i=0; i<=pos; i++){
					int value = instance->targets[i].player;
					if( value == -1 ){
						break;
					}
					int s_player = BYTE0(value);
					int s_card = BYTE1(value);
					int dmg = BYTE2(value);
					instance->targets[i].player = -1;
					damage_player(BYTE1(instance->info_slot), dmg, s_player, s_card);

					value = instance->targets[i].card;
					if( value == -1 ){
						break;
					}
					s_player = BYTE0(value);
					s_card = BYTE1(value);
					dmg = BYTE2(value);
					instance->targets[i].card = -1;
					damage_player(BYTE1(instance->info_slot), dmg, s_player, s_card);
				}
		}
	}

	return 0;
}


int card_sidar_kondo_of_jamuraa(int player, int card, event_t event){
/*
Sidar Kondo of Jamuraa |2|G|W	0x200f656
Legendary Creature - Human Knight 2/5
Flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.)
Creatures your opponents control without flying or reach can't block creatures with power 2 or less.
Partner (You can have two commanders if both have partner.)
*/
	check_legend_rule(player, card, event);
	flanking(player, card, event);

	if( event == EVENT_BLOCK_LEGALITY && in_play(player, card) && ! is_humiliated(player, card) && attacking_card_controller == player ){
		if( !(get_abilities(affected_card_controller, affected_card, EVENT_ABILITIES, -1) & (KEYWORD_FLYING | KEYWORD_REACH)) ){
			if( get_power(attacking_card_controller, attacking_card) <= 2 ){
				event_result = 1;
			}
		}
	}

	return 0;
}

int card_silas_renn_seeker_adept(int player, int card, event_t event){
/*
Silas Renn, Seeker Adept |1|U|B	0x200f65b
Legendary Artifact Creature - Human 2/2
Deathtouch
Whenever Silas Renn, Seeker Adept deals combat damage to a player, choose target artifact card in your graveyard.
	You may cast that card this turn.
Partner (You can have two commanders if both have partner.)
*/
	check_legend_rule(player, card, event);
	deathtouch(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		if( count_graveyard_by_type(player, TYPE_ARTIFACT) && ! graveyard_has_shroud(player) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ARTIFACT, "Select target artifact card.");
			int selected = new_select_a_card(player, player, TUTOR_FROM_GRAVE, 0, AI_MAX_VALUE, -1, &test);
			if( selected > -1 ){
				create_spell_has_flashback_legacy(player, card, selected, FBL_PLAYING_PERMANENT_CARD);
			}
		}
	}

	return 0;
}

int card_sylvan_reclamation(int player, int card, event_t event){
/*
Sylvan Reclamation |3|G|W	0x200f660
Instant
Exile up to two target artifacts and/or enchantments.
Basic landcycling 2 (2, Discard this card: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library.)
*/

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return basic_landcycling(player, card, event);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++)
			if( validate_target(player, card, &td, i) )
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_EXILE);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "DISENCHANT", 2, NULL);
}

int card_tana_the_bloodsower(int player, int card, event_t event){
/*
Tana, the Bloodsower |2|R|G	0x200f665
Legendary Creature - Elf Druid 2/2
Trample
Whenever Tana, the Bloodsower, deals combat damage to a player, create that many 1/1 green Saproling creature tokens.
Partner (You can have two commanders if both have partner.)
*/
	check_legend_rule(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_REPORT_DAMAGE_DEALT);
	if( result ){
		card_instance_t *instance = get_card_instance(player, card);
		int total = BYTE0(instance->targets[16].player) + BYTE1(instance->targets[16].player);
		instance->targets[16].player = 0;
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, total);
	}

	return 0;
}

int card_thrasios_triton_hero(int player, int card, event_t event){
/*
Thrasios, Triton Hero |G|U	0x200f66a
Legendary Creature - Merfolk Wizard 1/3
4: Scry 1, then reveal the top card of your library. If it's a land card, put it onto the battlefield tapped. Otherwise, draw a card.
Partner (You can have two commanders if both have partner.)
*/
	check_legend_rule(player, card, event);

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (current_turn == 1-player && current_phase >= PHASE_MAIN2 ? 24 : 12);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( count_deck(player) ){
			scry(player, 1);
			int iid = deck_ptr[0][player];
			if( iid > -1 ){
				reveal_card_iid(player, card, iid);
				if( is_what(-1, iid, TYPE_LAND) )
					put_into_play_a_card_from_deck(player, player, 0);
				else
					draw_a_card(player);
			}
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_X(4), 0, NULL, NULL);
}

int card_tymna_the_weaver(int player, int card, event_t event){
/*
Tymna the Weaver |1|W|B	0x200f66f
Legendary Creature - Human Cleric 2/2
Lifelink
At the beginning of your postcombat main phase, you may pay X life, where X is the number of opponents that were dealt combat damage
	this turn. If you do, draw X cards.
Partner (You can have two commanders if both have partner.)
*/
	check_legend_rule(player, card, event);
    lifelink(player, card, event);

	if( beginning_of_phase(player, card, event, player, -1, PHASE_MAIN2) ){
		if( get_trap_condition(1-player, TRAP_DAMAGED_BY_CREATURE) && can_pay_life(player, 1) ){
			if( do_dialog(player, player, card, -1, -1, " Pay 1 life\n Decline", life[player] >= 6 ? 0 : 1) == 0 ){
				lose_life(player, 1);
				draw_a_card(player);
			}
		}
	}

	return 0;
}

int card_treacherous_terrain(int player, int card, event_t event){
/*
Treacherous Terrain |6|R|G	0x200f674
Sorcery
Treacherous Terrain deals damage to each opponent equal to the number of lands that player controls.
Basic landcycling 2 (2, Discard this card: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library.)
*/

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return basic_landcycling(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		damage_player(1-player, count_subtype(1-player, TYPE_LAND, -1), player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_vial_smasher_the_fierce(int player, int card, event_t event){
	// !!!DOM: to an opponent chosen at random => choose an opponent at random... to that player or a planeswalker that player controls
	/* Vial Smasher the Fierce	|1|B|R	0x200f679
	 * Legendary Creature - Goblin Berserker 2/3
	 * Whenever you cast your first spell each turn, choose an opponent at random. ~ deals damage equal to that spell's converted mana cost to that player or a planeswalker that player controls.
	 * Partner */
	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && get_specific_storm_count(player) == 1 ){
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
			damage_player(1-player, get_cmc(trigger_cause_controller, trigger_cause), player, card);
		}
	}

	return 0;
}

int yidris_maelstrom_wielder_legacy(int player, int card, event_t event){

	if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		do_cascade(player, card, get_cmc(trigger_cause_controller, trigger_cause));
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_yidris_maelstrom_wielder(int player, int card, event_t event){
/*
Yidris, Maelstrom Wielder |U|B|R|G	0x200f67e
Legendary Creature - Ogre Wizard 5/4
Trample
Whenever Yidris, Maelstrom Wielder deals combat damage to a player, as you cast spells from your hand this turn, they gain cascade.
(When you cast the spell, exile cards from the top of your library until you exile a nonland card that costs less.You may cast it without paying its mana cost. Put the exiled cards on the bottom of the library in a random order.)
*/
	check_legend_rule(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		create_legacy_effect(player, card, &yidris_maelstrom_wielder_legacy);
	}

	return 0;
}

// Artifacts

int card_armory_automation(int player, int card, event_t event){
/*
Armory Automation |3	0x200f683
Artifact Creature - Construct 2/2
Whenever Armory Automation enters the battlefield or attacks, you attach any number of target Equipment to it.
	(Control of the Equipment doesn't change.)
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.required_subtype = SUBTYPE_EQUIPMENT;
		td.preferred_controller = ANYBODY;

		card_instance_t *instance = get_card_instance(player, card);

		int t = 0;
		while( can_target(&td) && t < 10 ){
				if( new_pick_target(&td, "TARGET_EQUIPMENT", t, GS_LITERAL_PROMPT) ){
					state_untargettable(instance->targets[t].player, instance->targets[t].card, 1);
					t++;
				}
				else{
					break;
				}
		}
		for(t = 0; t < instance->number_of_targets; t++){
			state_untargettable(instance->targets[t].player, instance->targets[t].card, 0);
			equip_target_creature(instance->targets[t].player, instance->targets[t].card, player, card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_boompile(int player, int card, event_t event){
/*
Boompile |4	0x200f688
Artifact
T: Flip a coin. If you win the flip, destroy all nonland permanents.
*/
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( flip_a_coin(player, card) ){
			test_definition_t test;
			default_test_definition(&test, TYPE_LAND);
			test.type_flag = DOESNT_MATCH;
			APNAP(p, { new_manipulate_all(player, card, p, &test, KILL_DESTROY); };);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_conquerors_flail(int player, int card, event_t event){
/*
Conqueror's Flail |2	0x200f68d
Artifact - Equipment
Equipped creature gets +1/+1 for each color among permanents you control.
As long as Conqueror's Flail is attached to a creature, your opponents can't cast spells during your turn.
Equip 2
*/
	if( event == EVENT_MODIFY_COST && player > -1 && current_turn == player && affected_card_controller == 1-player &&
		is_equipping(player, card) )
	{
		infinite_casting_cost();
		return 0;
	}

	if( is_equipping(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			int pump = 0;

			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_PERMANENT);
			// Works properly for (possibly laced) lands, despite them using card_data_t::color, due to a special case in get_abilities()
			td.allowed_controller = player;
			td.preferred_controller = player;
			td.illegal_abilities = 0;

			int i;
			for (i = COLOR_BLACK; i <= COLOR_WHITE; ++i){
				td.required_color = 1 << i;
				if( can_target(&td) ){
					pump++;
				}
			}
			event_result += pump;
		}
	}

	return basic_equipment(player, card, event, 2);
}

int card_crystalline_crawler(int player, int card, event_t event){
/*
Crystalline Crawler |4	0x200f692
Artifact Creature - Construct 1/1
Converge — Crystalline Crawler enters the battlefield with a +1/+1 counter on it for each color of mana spent to cast it.
Remove a +1/+1 counter from Crystalline Crawler: Add one mana of any color to your mana pool.
t: Put a +1/+1 counter on Crystalline Crawler.
*/
	store_num_colors_paid_in_info_slot(player, card, event);
	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, get_card_instance(player, card)->info_slot);

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( can_produce_mana(player, card) && count_1_1_counters(player, card) )
			declare_mana_available_hex(player, COLOR_TEST_ANY_COLORED, 1);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_COUNTER
	};


	if( event == EVENT_CAN_ACTIVATE ){
		if( can_produce_mana(player, card) && count_1_1_counters(player, card) )
			return 1;
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		get_card_instance(player, card)->info_slot = 0;
		int abils[3] = {
						0,
						can_produce_mana(player, card) && count_1_1_counters(player, card) ? 1 : 0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Get mana", abils[1], 1,
							"Add +1/+1 counter", abils[2] && ! paying_mana() ? 1 : 0, 5);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MANA ){
			remove_1_1_counter(player, card);
			produce_mana_any_combination_of_colors(player, COLOR_TEST_ANY_COLORED, 1, "");
		}
		if( choice == CHOICE_COUNTER ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION )
	{
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->info_slot == CHOICE_COUNTER )
			add_1_1_counter(instance->parent_controller, instance->parent_card);
	}

	return 0;
}

int card_prismatic_geoscope(int player, int card, event_t event){
/*
Prismatic Geoscope |5	0x200f697
Artifact
Prismatic Geoscope enters the battlefield tapped.
Domain — t: Add X mana in any combination of colors to your mana pool, where X is the number of basic land types among lands you control.
*/
	comes_into_play_tapped(player, card, event);

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( mana_producer(player, card, EVENT_CAN_ACTIVATE) ){
			declare_mana_available_any_combination_of_colors(player, COLOR_TEST_ANY_COLORED, count_domain(player, card));
		}
	}

	if( event == EVENT_CAN_ACTIVATE )
		return mana_producer(player, card, event);

	if( event == EVENT_ACTIVATE )
		produce_mana_tapped_any_combination_of_colors(player, card, COLOR_TEST_ANY_COLORED, count_domain(player, card), "");

	return 0;
}

// Lands
int card_ash_barrens(int player, int card, event_t event){
/*
Ash Barrens
Land
t: Add C to your mana pool.
Basic landcycling 1 (1, Discard this card: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library.)
*/

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND )
		return cycling(player, card, event, MANACOST_X(1));

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND )
		tutor_basic_land(player, 0, 0);

	return mana_producer(player, card, event);
}
