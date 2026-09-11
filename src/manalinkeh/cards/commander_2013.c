#include "manalink.h"

// Functions
static void store_mana_paid_to_play_this(int player, int card, event_t event){

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			card_instance_t *instance = get_card_instance(player, card);
			instance->targets[3].card = 0;
			int i;
			for(i=0; i<7; i++)
				instance->targets[3].card += mana_paid[i];
		}
	}
}

// Cards

// black
int card_baleful_force(int player, int card, event_t event){
/* CARD_ID_BALEFUL_FORCE	13606
Baleful Force	|5|B|B|B
Creature - Elemental 7/7
At the beginning of each upkeep, you draw a card and you lose 1 life. */

	upkeep_trigger_ability(player, card, event, 2);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		draw_cards(player, 1);
		lose_life(player, 1);
	}

	return global_enchantment(player, card, event);
}

int card_curse_of_shallow_graves(int player, int card, event_t event)
{
/* CARD_ID_CURSE_OF_SHALLOW_GRAVES	13611
Curse of Shallow Graves	|2|B
Enchantment - Aura Curse
Enchant player
Whenever a player attacks enchanted player with one or more creatures, that attacking player may create a tapped 2/2 |Sblack Zombie creature token.*/

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		int cursed_player = instance->targets[4].player;
		if( cursed_player > -1 ){
			if( declare_attackers_trigger(player, card, event, DAT_ATTACKS_PLAYER | RESOLVE_TRIGGER_AI(player), 1-cursed_player, -1) )
			{
				token_generation_t token;
				default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
				token.action = TOKEN_ACTION_TAPPED;
				generate_token(&token);
			}
		}
	}

	// Enchant player
	return curse(player, card, event);
}

int card_tempt_with_immortality(int player, int card, event_t event){
/* Tempt with Immortality	|4|B
 * Sorcery
 * Tempting offer - Return a creature card from your graveyard to the battlefield.
 * Each opponent may return a creature card from his or her graveyard to the battlefield.
 * For each player who does, return a creature card from your graveyard to the battlefield. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! count_graveyard_by_type(player, TYPE_CREATURE) ){
			ai_modifier -= 50;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
		int returned[2] = {-1};
		APNAP(p,{
					if( count_graveyard_by_type(p, TYPE_CREATURE) ){
						returned[p] = new_global_tutor(p, p, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_CMC, &this_test);
					}
				};
		);
		if( returned[1-player] > -1 && count_graveyard_by_type(player, TYPE_CREATURE) ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_CMC, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_price_of_knowledge(int player, int card, event_t event){
/* CARD_ID_PRICE_OF_KNOWLEDGE	13632
Price of Knowledge	|6|B
Enchantment
Players have no maximum hand size.
At the beginning of each opponent's upkeep, ~ deals damage to that player equal to the number of cards in that player's hand. */

	if( event == EVENT_MAX_HAND_SIZE && in_play(player, card) && ! is_humiliated(player, card) ){
		event_result = 99;
	}

	upkeep_trigger_ability(player, card, event, 1-player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( hand_count[1-player] > 0 ){
			damage_player(1-player, hand_count[1-player], player, card);
		}
	}

	return global_enchantment(player, card, event);
}

int card_fell_shepherd(int player, int card, event_t event)
{
/* Fell Shepherd	|5|B|B
 * Creature - Avatar 8/6
 * Whenever ~ deals combat damage to a player, you may return to your hand all creature cards that were put into your graveyard
 * 		from the battlefield this turn.
 * |B, Sacrifice another creature: Target creature gets -2/-2 until end of turn. */

  if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER|DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_TRIGGER_OPTIONAL))
	return_all_dead_this_turn_to_hand(player, TYPE_CREATURE);

  if (!IS_ACTIVATING(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  card_instance_t* instance = get_card_instance(player, card);

  test_definition_t test;
  new_default_test_definition(&test, TYPE_CREATURE, "Select another creature to sacrifice.");
  test.not_me = 1;

  if (event == EVENT_CAN_ACTIVATE)
	return CAN_ACTIVATE(player, card, MANACOST_B(1)) && new_can_sacrifice_as_cost(player, card, &test);

  if (event == EVENT_ACTIVATE)
	{
	  int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
	  if (!sac)
		{
		  cancel = 1;
		  return 0;
		}

	  instance->number_of_targets = 0;
	  if (can_target(&td) && pick_target(&td, "TARGET_CREATURE"))
		kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
	  else
		{
		  cancel = 1;
		  state_untargettable(BYTE2(sac), BYTE3(sac), 0);
		}
	}

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -2, -2);

  return 0;
}

int card_hooded_horror(int player, int card, event_t event){
/* CARD_ID_HOODED_HORROR	13621
Hooded Horror	|4|B
Creature - Horror 4/4
~ can't be blocked as long as defending player controls the most creatures or is tied for the most. */

	if( event == EVENT_BLOCK_LEGALITY && in_play(player, card) && ! is_humiliated(player, card) ){
		if( player == attacking_card_controller && card == attacking_card ){
			if( count_subtype(1-current_turn, TYPE_CREATURE, -1) >= count_subtype(current_turn, TYPE_CREATURE, -1) ){
				event_result = 1;
			}
		}
	}

	return 0;
}

int card_ophiomancer(int player, int card, event_t event){
/* Ophiomancer	|2|B
 * Creature - Human Shaman 2/2
 * At the beginning of each upkeep, if you control no Snakes, put a 1/1 |Sblack Snake creature token with deathtouch onto the battlefield. */

	upkeep_trigger_ability(player, card, event, 2);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( count_subtype(player, TYPE_PERMANENT, SUBTYPE_SNAKE) < 1 ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SNAKE, &token);
			token.pow = 1;
			token.tou = 1;
			token.color_forced = COLOR_TEST_BLACK;
			token.s_key_plus = SP_KEYWORD_DEATHTOUCH;
			generate_token(&token);
		}
	}

	return 0;
}

int card_toxic_deluge(int player, int card, event_t event){
/* Toxic Deluge	|2|B
 * Sorcery
 * As an additional cost to cast ~, pay X life.
 * All creatures get -X/-X until end of turn. */

	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return can_pay_life_as_cost_for_spells_or_activated_abilities(player, 1);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			int ltp = check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_MAX_TOU, &this_test);
			while( life[player]-ltp < 6 ){
					ltp--;
			}
			if( player == HUMAN ){
				ltp = choose_a_number(player, "Pay how much life?", life[player]);
			}
			if( ltp < 1 || ltp > life[player] ){
				spell_fizzled = 1;
			}
			else{
				lose_life(player, ltp);
				instance->info_slot = ltp;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p, {pump_subtype_until_eot(player, card, p, -1, -instance->info_slot, -instance->info_slot, 0, 0);};);
		kill_card(player, card, KILL_SACRIFICE);
	}

	return 0;
}

// blue
int card_curse_of_inertia(int player, int card, event_t event)
{
/*
Curse of Inertia |2|U
Enchantment — Aura Curse
Enchant player
Whenever a player attacks enchanted player with one or more creatures, that attacking player may tap or untap target permanent of
his or her choice.
*/

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		int cursed_player = instance->targets[4].player;
		if( cursed_player > -1 ){
			if( declare_attackers_trigger(player, card, event, DAT_ATTACKS_PLAYER | RESOLVE_TRIGGER_AI(player), 1-cursed_player, -1) )
			{
				instance->number_of_targets = 0;

				target_definition_t td;
				default_target_definition(player, card, &td, TYPE_PERMANENT);

				if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
					twiddle(player, card, 0);
				}
			}
		}
	}

	// Enchant player
	return curse(player, card, event);
}

int card_diviner_spirit(int player, int card, event_t event ){
/*
Diviner Spirit |4|U
Creature — Spirit 2/4
Whenever Diviner Spirit deals combat damage to a player, you and that player each draw that many cards.
*/
	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE |
							DDBM_REPORT_DAMAGE_DEALT) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		int amount = instance->targets[16].player;
		instance->targets[16].player = 0;
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		draw_cards(player, amount);
		APNAP(p, { if( times_damaged[p] ){ draw_cards(p, amount); };};);
	}

	return 0;
}

int card_djinn_of_infinite_deceits(int player, int card, event_t event){
/*
Djinn of Infinite Deceits |4|U|U
Creature — Djinn 2/7
Flying
{T}: Exchange control of two target nonlegendary creatures. You can't activate this ability during combat.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td_any_nonlegend;
	default_target_definition(player, card, &td_any_nonlegend, TYPE_CREATURE);
	td_any_nonlegend.allowed_controller = 2;
	td_any_nonlegend.preferred_controller = 2;
	td_any_nonlegend.required_subtype = SUBTYPE_LEGEND;
	td_any_nonlegend.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

	target_definition_t td_opp_nonlegend;
	default_target_definition(player, card, &td_opp_nonlegend, TYPE_CREATURE);
	td_opp_nonlegend.allowed_controller = 1-player;
	td_opp_nonlegend.required_subtype = SUBTYPE_LEGEND;
	td_opp_nonlegend.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

	target_definition_t td_own_nonlegend;
	default_target_definition(player, card, &td_own_nonlegend, TYPE_CREATURE);
	td_own_nonlegend.allowed_controller = player;
	td_own_nonlegend.preferred_controller = player;
	td_own_nonlegend.required_subtype = SUBTYPE_LEGEND;
	td_own_nonlegend.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

	target_definition_t *td, *td1;
	if (player == AI){
		td = &td_opp_nonlegend;
		td1 = &td_own_nonlegend;
	} else {
		td = td1 = &td_any_nonlegend;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && (current_phase <= PHASE_MAIN1 || current_phase >= PHASE_MAIN2) && can_target(td) && can_target(td1) ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0)
			&& pick_target(td, "TARGET_NONLEGENDARY_CREATURE")
			&& new_pick_target(td1, "TARGET_NONLEGENDARY_CREATURE", 1, 1)
		  ){
			instance->number_of_targets = 2;
			tap_card(player, card);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, td, 0) && validate_target(player, card, td1, 1) ){
			exchange_control_of_target_permanents(instance->parent_controller, instance->parent_card,
													instance->targets[0].player, instance->targets[0].card,
													instance->targets[1].player, instance->targets[1].card);
		}
	}

	return 0;
}

// illusionist's gambit --> skipped

int card_order_of_succession(int player, int card, event_t event){
/*
Order of Succession |3|U
Sorcery
Choose left or right. Starting with you and proceeding in the chosen direction, each player chooses a creature controlled by
	the next player in that direction. Each player gains control of the creature he or she chose.
*/

	if( event == EVENT_RESOLVE_SPELL ){
		int targetted[2] = {-1};
		APNAP(p,{
					target_definition_t td;
					default_target_definition(player, card, &td, TYPE_CREATURE );
					td.preferred_controller = 1-p;
					td.allowed_controller = 1-p;
					td.who_chooses = p;
					td.illegal_abilities = 0;
					td.allow_cancel = 0;
					if( can_target(&td) && new_pick_target(&td, "Select a creature controlled by your opponent.", 0, 0 | GS_LITERAL_PROMPT) ){
						targetted[p] = get_card_instance(player, card)->targets[0].card;
						add_state(1-p, targetted[p], STATE_TARGETTED);
					}
					get_card_instance(player, card)->number_of_targets = 0;
				}
		);
		APNAP(p,{
					if( targetted[p] > -1 ){
						remove_state(1-p, targetted[p], STATE_TARGETTED);
						gain_control_permanently(p, 1-p, targetted[p]);
					}
				}
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_true_name_nemesis(int player, int card, event_t event){
/* CARD_ID_TRUE_NAME_NEMESIS	13651
True-Name Nemesis	|1|U|U
Creature - Merfolk Rogue 3/1
As ~ enters the battlefield, choose a player.
~ has protection from the chosen player. (This creature can't be blocked, targeted, dealt damage, or enchanted by anything controlled by that player.)*/
// Approximation

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		hexproof(player, card, event);

		unblockable(player, card, event);

		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *damage= get_card_instance(affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( damage->damage_target_card == card && damage->damage_target_player == player &&
					damage->damage_source_player != player &&
					damage->info_slot > 0
				  ){
					int amount = damage->info_slot;
					prevent_damage(affected_card_controller, affected_card, amount);
				}
			}
		}
	}

	return 0;
}

int card_tempt_with_reflections(int player, int card, event_t event){
/*
Tempt with Reflections |3|U
Sorcery
Tempting offer — Choose target creature you control. Put a token onto the battlefield that's a copy of that creature.
Each opponent may put a token onto the battlefield that's a copy of that creature.
For each opponent who does, put a token onto the battlefield that's a copy of that creature.
*/

	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allowed_controller = player;
		td. allow_cancel = 0;

		if( can_target(&td) && new_pick_target(&td, "Select a creature you control", 0, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			instance->number_of_targets = 0;

			token_generation_t token;
			copy_token_definition(player, card, &token, instance->targets[0].player, instance->targets[0].card);
			generate_token(&token);

			int iid = get_card_instance(instance->targets[0].player, instance->targets[0].card)->internal_card_id;

			char buffer[100];
			scnprintf(buffer, 100, "Get a copy of %s", cards_ptr[cards_data[iid].id]->name);

			int choice = DIALOG(player, card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_RANDOM,
								DLG_SMALLCARD_ID(iid), DLG_WHO_CHOOSES(1-player),
								buffer, 1, internal_rand(10),
								"Decline", 1, internal_rand(10));

			if( choice == 1 ){
				token.t_player = 1-player;
				generate_token(&token);
				token.t_player = player;
				generate_token(&token);
			}
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return basic_spell(player, card, event);
}

int card_tidal_force(int player, int card, event_t event ){
/* CARD_ID_TIDAL_FORCE	13649
Tidal Force	|5|U|U|U
Creature - Elemental 7/7
At the beginning of each upkeep, you may tap or untap target permanent. */

	upkeep_trigger_ability_mode(player, card, event, ANYBODY, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);

		if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
			twiddle(player, card, 0);
		}

		get_card_instance(player, card)->number_of_targets = 0;
	}

	return 0;
}

// green
int card_bane_of_progress(int player, int card, event_t event){
/* CARD_ID_BANE_OF_PROGRESS	13607
Bane of Progress	|4|G|G
Creature - Elemental 2/2
When ~ enters the battlefield, destroy all artifacts and enchantments. Put a +1/+1 counter on ~ for each permanent destroyed this way. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT | TYPE_ENCHANTMENT);
		this_test.type_flag = F1_NO_PWALKER;

		int result = 0;
		APNAP(p, { result += new_manipulate_all(player, card, p, &this_test, KILL_DESTROY); };);

		add_1_1_counters(player, card, result);
	}

	return 0;
}

int card_curse_of_predation(int player, int card, event_t event)
{
/* CARD_ID_CURSE_OF_PREDATION	13610
Curse of Predation	|2|G
Enchantment - Aura Curse
Enchant player
Whenever a creature attacks enchanted player, put a +1/+1 counter on it. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		int cursed_player = instance->targets[0].player;
		/* Look at original targets[0] rather than preserved targets[4], since the former doesn't change on this card
		 * but DAT_TRACK overwrites the latter. (instance->damage_target_player/card seem like they'd work just fine.) */
		if( cursed_player > -1 ){
			int amt;
			if( (amt = declare_attackers_trigger(player, card, event, DAT_ATTACKS_PLAYER | DAT_TRACK, 1-cursed_player, -1)) )
			{
				unsigned char* attackers = (unsigned char*)(&instance->targets[2].player);
				for (--amt; amt >= 0; --amt){
					if (in_play(current_turn, attackers[amt])){
						add_1_1_counter(current_turn, attackers[amt]);
					}
				}
			}
		}
	}

	// Enchant player
	return curse(player, card, event);
}

int card_naya_soulbeast(int player, int card, event_t event){
/*
Naya Soulbeast |6|G|G
Creature — Beast 0/0
Trample
When you cast Naya Soulbeast, each player reveals the top card of his or her library.
Naya Soulbeast enters the battlefield with X +1/+1 counters on it, where X is the total converted mana cost of all cards revealed this way.
*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && ! check_special_flags(player, card, SF_NOT_CAST) &&
		! is_token(player, card) )
	{
		APNAP(p,{
					if( deck_ptr[p][0] != -1 ){
						show_deck(p, deck_ptr[p], 1, "Here's the top card of your deck", 0, 0x7375B0 );
						show_deck(1-p, deck_ptr[p], 1, "Here's the top card of opponent's deck", 0, 0x7375B0 );
						get_card_instance( player, card )->info_slot += get_cmc_by_internal_id(deck_ptr[p][0]);
					}
				};
		);
	}

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, get_card_instance(player, card)->info_slot);

	return 0;
}

// primal vigor --> doubling season

int card_restore2(int player, int card, event_t event){
/*
Restore |1|G
Sorcery
Put target land card from a graveyard onto the battlefield under your control.
*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select target land card.");

	if( event == EVENT_RESOLVE_SPELL){
		card_instance_t *instance = get_card_instance(player, card);
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			reanimate_permanent(player, card, instance->targets[0].player, selected, REANIMATE_DEFAULT);
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &test);
}

int card_spawning_grounds(int player, int card, event_t event){
/* Spawning Grounds	|6|G|G
 * Enchantment - Aura
 * Enchant land
 * Enchanted land has "|T: Put a 5/5 |Sgreen Beast creature token with trample onto the battlefield." */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		if( IS_GAA_EVENT(event) ){
			card_instance_t *instance = get_card_instance(player, card);
			if( event == EVENT_RESOLVE_ACTIVATION ){
				token_generation_t token;
				default_token_definition(instance->damage_target_player, instance->damage_target_card, CARD_ID_BEAST, &token);
				token.pow = 5;
				token.tou = 5;
				token.color_forced = get_sleighted_color_test(instance->damage_target_player, instance->damage_target_card, COLOR_TEST_GREEN);
				token.key_plus = KEYWORD_TRAMPLE;
				generate_token(&token);
			}
			return generic_activated_ability(instance->damage_target_player, instance->damage_target_card, event, GAA_UNTAPPED,
											MANACOST0, 0, NULL, NULL);
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, LAND);
	td.preferred_controller = td.allowed_controller = player;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target land you control.");
}

int card_tempt_with_discovery(int player, int card, event_t event){
/* Tempt with Discovery	|3|G
 * Sorcery
 * Tempting offer - Search your library for a land card and put it onto the battlefield.
 * Each opponent may search his or her library for a land card and put it onto the battlefield.
 * For each opponent who searches a library this way, search your library for a land card and put it onto the battlefield.
 * Then each player who searched a library this way shuffles it. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a land card.");
		this_test.no_shuffle = 1;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
		int result = new_global_tutor(1-player, 1-player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
		if( result != -1 ){
			shuffle(1-player);
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
		}
		shuffle(player);
		kill_card(player, card, KILL_SACRIFICE);
	}

	return basic_spell(player, card, event);
}

// red
int card_curse_of_chaos(int player, int card, event_t event ){
/* CARD_ID_CURSE_OF_CHAOS	13608
Curse of Chaos	|2|R
Enchantment - Aura Curse
Enchant player
Whenever a player attacks enchanted player with one or more creatures, that attacking player may discard a card.
If the player does, he or she draws a card. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		int cursed_player = instance->targets[4].player;
		if( cursed_player > -1 ){
			if( declare_attackers_trigger(player, card, event, DAT_ATTACKS_PLAYER, 1-cursed_player, -1) ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, 0, "Select a card to discard.");

				int selected = new_select_a_card(current_turn, current_turn, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
				if( selected != -1 ){
					discard_card(current_turn, selected);
					draw_cards(current_turn, 1);
				}
			}
		}
	}

	// Enchant player
	return curse(player, card, event);
}

int card_from_the_ashes(int player, int card, event_t event){
/* From the Ashes	|3|R
 * Sorcery
 * Destroy all nonbasic lands.
 * For each land destroyed this way, its controller may search his or her library for a basic land card and put it onto the battlefield.
 * Then each player who searched his or her library this way shuffles it. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "");
		this_test.subtype = SUBTYPE_BASIC;
		this_test.subtype_flag = DOESNT_MATCH;

		int r[2];
		APNAP(p, r[p] = new_manipulate_all(player, card, p, &this_test, KILL_DESTROY););
		APNAP(p,{
					if( r[p] > 0 && do_dialog(p, player, card, -1, -1, " Search\n Decline", 0) == 0){
						tutor_basic_lands(p, TUTOR_PLAY, r[p]);
					}
				}
		);

		kill_card(player, card, KILL_SACRIFICE);
	}

	return basic_spell(player, card, event);
}

int card_sudden_demise(int player, int card, event_t event){
/* Sudden Demise	|X|R
 * Sorcery
 * Choose a color. ~ deals X damage to each creature of the chosen color. */

	if( event == EVENT_RESOLVE_SPELL ){
		int clr = 1<<choose_a_color(player, get_deck_color(player, 1-player));
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = clr;
		new_damage_all(player, card, ANYBODY, get_card_instance(player, card)->info_slot, 0, &this_test);
		kill_card(player, card, KILL_SACRIFICE);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_tempt_with_vengeance(int player, int card, event_t event){
/* Tempt with Vengeance	|X|R
 * Sorcery
 * Tempting offer - Put X 1/1 |Sred Elemental creature tokens with haste onto the battlefield.
 * Each opponent may put X 1/1 |Sred Elemental creature tokens with haste onto the battlefield.
 * For each player who does, put X 1/1 |Sred Elemental creature tokens with haste onto the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = get_card_instance(player, card)->info_slot;
		if( amount ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
			token.pow = token.tou = 1;
			token.s_key_plus = SP_KEYWORD_HASTE;
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_RED);
			token.qty = amount;
			generate_token(&token);

			char choice[100];
			scnprintf(choice, 100, "Get %d 1/1 Elemental token%s", amount, amount == 1 ? "" : "s");
			if( DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_STORAGE, DLG_RANDOM, DLG_WHO_CHOOSES(1-player),
					   choice, 1, 2,
					   "Decline", 1, 1) == 1 )
			{
				token.t_player = 1-player;
				generate_token(&token);

				token.t_player = player;
				generate_token(&token);
			}
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_terra_ravager(int player, int card, event_t event)
{
/* CARD_ID_TERRA_RAVAGER	13648
Terra Ravager	|2|R|R
Creature - Elemental Beast 0/4
Whenever ~ attacks, it gets +X/+0 until end of turn, where X is the number of lands defending player controls. */

  if (declare_attackers_trigger(player, card, event, 0, player, card))
	pump_until_eot(player, card, player, card, count_subtype(1-player, TYPE_LAND, -1), 0);

  if (event == EVENT_POW_BOOST && current_turn == player && current_phase <= PHASE_DECLARE_ATTACKERS)
	return count_subtype(1-player, TYPE_LAND, -1);

  return 0;
}

// widespread panic --> doubling season

int card_witch_hunt(int player, int card, event_t event ){
/* CARD_ID_WITCH_HUNT	13654
Witch Hunt	|4|R
Enchantment
Players can't gain life.
At the beginning of your upkeep, ~ deals 4 damage to you.
At the beginning of your end step, target opponent chosen at random gains control of ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		damage_player(player, 4, player, card);
	}

	if( current_turn == player && eot_trigger(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		if( would_validate_arbitrary_target(&td, 1-player, -1) ){
			give_control_of_self(player, card);
		}

	}

	if( in_play(player, card) &&  event == EVENT_STATIC_EFFECTS && affect_me(player, card) && ! is_humiliated(player, card) )
	{
		player_bits[0] |= PB_CANT_GAIN_LIFE;
		player_bits[1] |= PB_CANT_GAIN_LIFE;
	}

	return global_enchantment(player, card, event);
}


// white
int card_act_of_authority(int player, int card, event_t event ){
/*
Act of Authority |1|W|W
Enchantment
When Act of Authority enters the battlefield, you may exile target artifact or enchantment.
At the beginning of your upkeep, you may exile target artifact or enchantment. If you do, its controller gains control of Act of Authority.
*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

		card_instance_t *instance = get_card_instance(player, card);

		if( pick_target(&td, "DISENCHANT") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		instance->number_of_targets = 0;
	}

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "DISENCHANT") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			give_control_of_self(player, card);
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_angel_of_finality(int player, int card, event_t event){
/* CARD_ID_ANGEL_OF_FINALITY	13605
Angel of Finality	|3|W
Creature - Angel 3/4
Flying
When ~ enters the battlefield, exile all cards from target player's graveyard. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			int count = count_graveyard(instance->targets[0].player)-1;
			while( count > -1 ){
					rfg_card_from_grave(instance->targets[0].player, count);
					count--;
			}
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_curse_of_the_forsaken(int player, int card, event_t event)
{
/* CARD_ID_CURSE_OF_THE_FORSAKEN	13612
Curse of the Forsaken	|2|W
Enchantment - Aura Curse
Enchant player
Whenever a creature attacks enchanted player, its controller gains 1 life. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		int cursed_player = instance->targets[4].player;
		if( cursed_player > -1 ){
			if( declare_attackers_trigger(player, card, event, DAT_ATTACKS_PLAYER|DAT_SEPARATE_TRIGGERS, cursed_player, -1) ){
				gain_life(current_turn, 1);
			}
		}
	}

  // Enchant player
  return curse(player, card, event);
}

int card_darksteel_mutation_insect(int player, int card, event_t event ){
	indestructible(player, card, event);
	return 0;
}

int card_darksteel_mutation(int player, int card, event_t event ){
/* CARD_ID_DARKSTEEL_MUTATION	13613
Darksteel Mutation	|1|W
Enchantment - Aura
Enchant creature
Enchanted creature is an Insect artifact creature with base power and toughness 0/1 and has indestructible,
and it loses all other abilities, card types, and creature types. */

	card_instance_t *instance = get_card_instance(player, card);

	if( in_play(player, card) && instance->damage_target_player > -1 ){
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_CHANGE_TYPE && affect_me(p, c) ){
			get_card_instance(p, c)->targets[1].card = event_result;
			if( instance->targets[1].player == -1 ){
				instance->targets[1].player = get_internal_card_id_from_csv_id(CARD_ID_DARKSTEEL_MUTATION_INSECT);
			}
			event_result = instance->targets[1].player;
		}

		if( event == EVENT_SET_COLOR && affect_me(p, c) && instance->targets[1].card > -1 ){
			event_result |= cards_data[instance->targets[1].card].color;
		}

	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

// mystic barrier --> skipped

static const char* serene_master_is_blocking_him(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  if (is_blocking(targeting_card, card))
	return NULL;
  else
	return "must be blocked by Serene Master";
}

int card_serene_master(int player, int card, event_t event ){
/* Serene Master	|1|W
 * Creature - Human Monk 0/2
 * Whenever ~ blocks, exchange its power and the power of target creature it's blocking until end of combat. */

	 if( current_turn != player && blocking(player, card, event) ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.allowed_controller =  1-player;
		td1.preferred_controller = 1-player;
		td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		td1.extra = (int32_t)serene_master_is_blocking_him;
		td1.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td1) && new_pick_target(&td1, "Select target creature blocking Serene Master.", 0, GS_LITERAL_PROMPT) ){
			int his_pow = get_power(instance->targets[0].player, instance->targets[0].card);
			int my_pow = get_power(player, card);
			set_pt_and_abilities_until_eot(player, card, player, card, his_pow, -1, 0, 0, 1);
			set_pt_and_abilities_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, my_pow, -1, 0, 0, 1);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_tempt_with_glory(int player, int card, event_t event)
{
  /* Tempt with Glory	|5|W
   * Sorcery
   * Tempting offer - Put a +1/+1 counter on each creature you control. Each opponent may put a +1/+1 counter on each creature he or she controls. For each
   * opponent who does, put a +1/+1 counter on each creature you control. */

	if (event == EVENT_RESOLVE_SPELL)
	{
		manipulate_type(player, card, player, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));

		if (DIALOG(player, card, EVENT_ACTIVATE,
				 DLG_WHO_CHOOSES(1-player), DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
				 "Add +1/+1 counters", 1, count_subtype(1-player, TYPE_CREATURE, -1),
				 "Pass",               1, count_subtype(  player, TYPE_CREATURE, -1)) == 1)
		{
			manipulate_type(player, card, 1-player, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));
			manipulate_type(player, card,   player, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return basic_spell(player, card, event);
}

int card_unexpectedly_absent(int player, int card, event_t event){

	/* Unexpectedly Absent	|X|W|W
	 * Instant
	 * Put target nonland permanent into its owner's library just beneath the top X cards of that library. */

	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.illegal_type = TYPE_LAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			put_permanent_under_the_first_x_card_of_its_owners_library(instance->targets[0].player, instance->targets[0].card,
																		instance->info_slot);
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, "TARGET_PERMANENT", 1, NULL);
}

// gold
int card_derevi_empyrial_tactician(int player, int card, event_t event ){
/* CARD_ID_DEREVI_EMPYRIAL_TACTICIAN	13614
Derevi, Empyrial Tactician	|G|W|U
Legendary Creature - Bird Wizard 2/3
Flying
Whenever ~ enters the battlefield or a creature you control deals combat damage to a player, you may tap or untap target permanent.
|1|G|W|U: Put ~ onto the battlefield from the command zone. */

	check_legend_rule(player, card, event);

	if( comes_into_play(player, card, event) ||
		subtype_deals_damage(player, card, event, player, -1, DDBM_TRIGGER_OPTIONAL|DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_OPPONENT)
	  ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		int i, times_triggered = trigger_condition == TRIGGER_DEAL_DAMAGE ? instance->targets[1].card : 1;
		for (i = 0; i < times_triggered; ++i){
			if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
				twiddle(player, card, 0);
				instance->number_of_targets = 0;
			}
			else{
				break;
			}
		}
	}
	return 0;
}

int card_gahiji_honored_one(int player, int card, event_t event)
{
/* CARD_ID_GAHIJI_HONORED_ONE	13620
Gahiji, Honored One	|2|R|G|W
Legendary Creature - Beast 4/4
Whenever a creature attacks one of your opponents or a planeswalker an opponent controls, that creature gets +2/+0 until end of turn. */

  check_legend_rule(player, card, event);

  // Whenever a creature attacks one of your opponents or a planeswalker an opponent controls, that creature gets +2/+0 until end of turn.
  int amt;
  if ((amt = declare_attackers_trigger(player, card, event, DAT_TRACK, player, -1)))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  unsigned char* attackers = (unsigned char*)(&instance->targets[2].player);
	  for (--amt; amt >= 0; --amt)
		if (in_play(current_turn, attackers[amt]))
		  pump_until_eot(player, card, current_turn, attackers[amt], 2, 0);
	}

  return 0;
}

int card_jeleva_nephalias_scourge(int player, int card, event_t event ){
	/*
	  Jeleva, Nephalia's Scourge English |1|U|B|R
	  Legendary Creature - Vampire Wizard 1/3
	  Flying
	  When Jeleva, Nephalia's Scourge enters the battlefield, each player exiles the top X cards of his or her library,
	  where X is the amount of mana spent to cast Jeleva.
	  Whenever Jeleva attacks, you may cast an instant or sorcery card exiled with it without paying its mana cost.
	*/

	store_mana_paid_to_play_this(player, card, event);

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);
		int amount = instance->targets[3].card;
		if( amount ){
			int i, leg = 0, idx = 0;
			for(i=0; i<2; i++){
				int p = i == 0 ? player : 1-player;
				int *deck = deck_ptr[p];
				int to_remove = MIN(count_deck(p), amount);
				if( to_remove > 0 ){
					show_deck( player, deck, to_remove, "Cards exiled by Jeleva", 0, 0x7375B0 );
					while( to_remove ){
							if( is_what(-1, deck[0], TYPE_SPELL) && ! is_what(-1, deck[0], TYPE_CREATURE) ){
								exiledby_remember(player, card, p, deck[0], &leg, &idx);
								instance->info_slot++;
							}
							rfg_card_in_deck(p, 0);
							to_remove--;
					}
				}
			}
		}
	}

		if (declare_attackers_trigger(player, card, event, (player == AI ? (get_card_instance(player, card)->info_slot ?
									RESOLVE_TRIGGER_MANDATORY : 0) : RESOLVE_TRIGGER_OPTIONAL), player, card) )
	{
		card_instance_t *instance = get_card_instance(player, card);
			int rval = exiledby_choose(player, card, CARD_ID_JELEVA_NEPHALIAS_SCOURGE, EXBY_CHOOSE, 0, "spell", 1);
			int* loc = (int*)rval;
			if( loc ){
				int *loc2 = exiledby_find(player, card, *loc, NULL, NULL);
				if( *loc2 > -1 ){
					int p = *loc2 & 0x80000000;
					int iid = *loc2 & ~0x80000000;
					*loc2 = -1;
					if( remove_card_from_rfg(p, cards_data[iid].id) ){
						play_card_in_exile_for_free(player, p, cards_data[iid].id);
					}
					instance->info_slot--;
				}
			}
	}

	return 0;
}

int card_marath_will_of_the_wild(int player, int card, event_t event ){
	/* Marath, Will of the Wild	|R|G|W	0x200b362
	 * Legendary Creature - Elemental Beast 0/0
	 * ~ enters the battlefield with a number of +1/+1 counters on it equal to the amount of mana spent to cast it.
	 * |X, Remove X +1/+1 counters from ~: Choose one -
	 * * Put X +1/+1 counters on target creature. X can't be 0.
	 * * ~ deals X damage to any target. X can't be 0.
	 * * Create an X/X |Sgreen Elemental creature token. X can't be 0. */

	check_legend_rule(player, card, event);

	store_mana_paid_to_play_this(player, card, event);

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1,
										(get_card_instance(player, card)->targets[3].card > 0 ?
										get_card_instance(player, card)->targets[3].card : 0));

	if( IS_GAA_EVENT(event) ){
		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.preferred_controller = player;

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		enum{
			CHOICE_PASS_COUNTERS = 1,
			CHOICE_DAMAGE,
			CHOICE_ELEMENTAL,
		};

		if( event == EVENT_CAN_ACTIVATE ){
			return generic_activated_ability(player, card, event, 0, MANACOST_X(1), GVC_COUNTER(COUNTER_P1_P1), NULL, NULL);
		}

		if( event == EVENT_ACTIVATE ){
			instance->targets[1].card = instance->targets[1].player = instance->number_of_targets = 0;
			int max = count_1_1_counters(player, card);
			while( ! has_mana(player, COLOR_ANY, max) && max > 0 ){
					max--;
			}
			int amount = max;
			if( get_toughness(player, card) <= amount ){
				amount--;
			}
			if( player == HUMAN ){
				amount = choose_a_number(player, "Remove how many counters (you cannot choose 0) ?", max);
				if( amount > count_1_1_counters(player, card) || ! has_mana_for_activated_ability(player, card, MANACOST_X(amount)) ||
					amount <= 0 )
				{
					spell_fizzled = 1;
					return 0;
				}
			}
			if( amount > 0 ){
				instance->info_slot = instance->number_of_targets = 0;
				if( charge_mana_for_activated_ability(player, card, MANACOST_X(amount)) ){
					int priorities[4] = {
											0,
											current_phase == PHASE_AFTER_BLOCKING ? 8 : 1,
											current_phase == PHASE_MAIN1 || current_phase == PHASE_MAIN2 ? 10 : 1,
											current_phase != PHASE_BEFORE_BLOCKING ? 5 : 8
					};
					int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE,
										"Pass +1/+1 counters", 			can_target(&td1), 	priorities[1],
										"Damage creature or player", 	can_target(&td2), 	priorities[2],
										"Generate an Elemental", 		1,					priorities[3]);
					if( ! choice ){
						spell_fizzled = 1;
						return 0;
					}
					switch( choice ){
							case CHOICE_PASS_COUNTERS:
							{
								if( pick_target(&td1, "TARGET_CREATURE") ){
									remove_1_1_counters(player, card, amount);
								}
								break;
							}
							case CHOICE_DAMAGE:
							{
								if( pick_target(&td2, "TARGET_ANY") ){
									remove_1_1_counters(player, card, amount);
								}
								break;
							}
							case CHOICE_ELEMENTAL:
								remove_1_1_counters(player, card, amount);
								break;
					}
					SET_BYTE0(instance->info_slot) = choice;
					SET_BYTE1(instance->info_slot) = amount;
				}
			}
		}
		if( event == EVENT_RESOLVE_ACTIVATION ){
			int choice = BYTE0(instance->info_slot);
			int amount = BYTE1(instance->info_slot);
			if( choice == CHOICE_PASS_COUNTERS && valid_target(&td1) ){
				add_1_1_counters(instance->targets[0].player, instance->targets[0].card, amount);
			}
			if( choice == CHOICE_DAMAGE && valid_target(&td2) ){
				damage_target0(player, card, amount);
			}
			if( choice == CHOICE_ELEMENTAL ){
				token_generation_t token;
				default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
				token.pow = token.tou = amount;
				token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
				generate_token(&token);
			}
		}
	}

	return 0;
}

int card_nekusar_the_mindrazer(int player, int card, event_t event){
	/* Nekusar, the Mindrazer	|2|U|B|R
	 * Legendary Creature - Zombie Wizard 2/4
	 * At the beginning of each player's draw step, that player draws an additional card.
	 * Whenever an opponent draws a card, ~ deals 1 damage to that player. */

	check_legend_rule(player, card, event);

	if( event == EVENT_DRAW_PHASE && in_play(player, card) && ! is_humiliated(player, card) ){
		event_result++;
	}

	if (card_drawn_trigger(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY)){
		damage_player(1-player, 1, player, card);
	}

	return 0;
}

int card_oloro_ageless_ascetic(int player, int card, event_t event){
/* CARD_ID_OLORO_AGELESS_ASCETIC	13628
Oloro, Ageless Ascetic	|3|W|U|B
Legendary Creature - Giant Soldier 4/5
At the beginning of your upkeep, you gain 2 life.
Whenever you gain life, you may pay |1. If you do, draw a card and each opponent loses 1 life.
At the beginning of your upkeep, if ~ is in the command zone, you gain 2 life. */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		gain_life(player, 2);
	}

	return 0;
}

int card_prossh_skyraider_of_kher(int player, int card, event_t event){
/* Prossh, Skyraider of Kher	|3|B|R|G
 * Legendary Creature - Dragon 5/5
 * Flying
 * When you cast ~, put X 0/1 |Sred Kobold creature tokens named Kobolds of Kher Keep onto the battlefield,
 * 		where X is the amount of mana spent to cast Prossh.
 * Sacrifice another creature: Prossh gets +1/+0 until end of turn. */

	check_legend_rule(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			store_mana_paid_to_play_this(player, card, event);
		}
		if( instance->targets[3].card > 0 && ! check_special_flags(player, card, SF_NOT_CAST) && ! is_token(player, card) ){
			generate_tokens_by_id(player, card, CARD_ID_KOBOLDS_OF_KHER_KEEP, instance->targets[3].card);
		}
	}

	if( event == EVENT_POW_BOOST ){
		int max_sac = 0;
		int c;
		for(c=0; c < active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_CREATURE) && c != player ){
				max_sac += (check_special_flags2(player, c, SF2_CANNOT_BE_SACRIFICED) ? 0 : 1);
			}
		}
		int amount = get_cost_mod_for_activated_abilities(player, card, MANACOST0);
		while( ! has_mana(player, COLOR_ANY, amount * max_sac) ){
				max_sac--;
		}
		return max_sac;
	}


	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select another creature to sacrifice.");
	test.not_me = 1;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade(player, card, event, 0, MANACOST0, 1, 0, 0, 0);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, 0);
}

int card_roon_of_the_hidden_realm(int player, int card, event_t event)
{
/* CARD_ID_ROON_OF_THE_HIDDEN_REALM	13636
Roon of the Hidden Realm	|2|G|W|U
Legendary Creature - Rhino Soldier 4/4
Vigilance, trample
|2, |T: Exile another target creature. Return that card to the battlefield under its owner's control at the beginning of the next end step.*/

	check_legend_rule(player, card, event);

	vigilance(player, card, event);

 	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	// |2, |T: Exile another target creature. Return that card to the battlefield under its owner's control at the beginning of the next end step.
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.special = TARGET_SPECIAL_NOT_ME;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
		card_instance_t* instance = get_card_instance(player, card);
		remove_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_ANOTHER_CREATURE");
}

int card_sydri_galvanic_genius(int player, int card, event_t event)
{
/*
Sydri, Galvanic Genius |W|U|B
Legendary Creature — Human Artificer 2/2
{U}: Target noncreature artifact becomes an artifact creature with power and toughness each equal to its converted mana cost
	until end of turn.
{W}{B}: Target artifact creature gains deathtouch and lifelink until end of turn.
*/
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td_noncreature_artifact;
	default_target_definition(player, card, &td_noncreature_artifact, TYPE_ARTIFACT);
	td_noncreature_artifact.illegal_type = TYPE_CREATURE;

	target_definition_t td_artifact_creature;
	default_target_definition(player, card, &td_artifact_creature, TYPE_CREATURE);
	td_artifact_creature.special = TARGET_SPECIAL_ARTIFACT_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	enum
	{
		CHOICE_ANIMATE = 1,
		CHOICE_DEATHTOUCH_LIFELINK
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_U(1), 0, &td_noncreature_artifact, NULL) )
		{
			return 1;
		}
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_WB(1, 1), 0, &td_artifact_creature, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_U(1), 0,
													&td_noncreature_artifact, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_WB(1, 1), 0,
													&td_artifact_creature, NULL)
		};
		int priorities[3] = {
							0,
							current_phase < PHASE_DECLARE_BLOCKERS ? 10 : 1,
							current_phase == PHASE_AFTER_BLOCKING ? 10 : 1
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Animate artifact", 		abils[1], priorities[1],
							"Deathtouch and lifelink", 	abils[2], priorities[2]);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_ANIMATE ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_U(1), 0,
											&td_noncreature_artifact, "XENIC_POLTERGEIST");
		}
		if( choice == CHOICE_DEATHTOUCH_LIFELINK ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_U(1), 0,
											&td_artifact_creature, "TARGET_ARTIFACTCREATURE");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		switch (choice)
		{
			case CHOICE_ANIMATE:
			{
				int cmc = get_cmc(instance->targets[0].player, instance->targets[0].card);
				artifact_animation( instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 1, cmc, cmc, 0, 0);
				break;
			}

			case CHOICE_DEATHTOUCH_LIFELINK:
			{
				pump_ability_until_eot( instance->parent_controller, instance->parent_card,
										instance->targets[0].player, instance->targets[0].card,
										0, 0, 0, SP_KEYWORD_DEATHTOUCH | SP_KEYWORD_LIFELINK);
				break;
			}
		}
	}

	return 0;
}

int card_shattergang_brothers(int player, int card, event_t event ){
/*
Shattergang Brothers |1|B|R|G
Legendary Creature — Goblin Artificer 3/3
{2}{B}, Sacrifice a creature: Each other player sacrifices a creature.
{2}{R}, Sacrifice an artifact: Each other player sacrifices an artifact.
{2}{G}, Sacrifice an enchantment: Each other player sacrifices an enchantment.
*/
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_SAC_CREATURE = 1,
		CHOICE_SAC_ARTIFACT,
		CHOICE_SAC_ENCHANTMENT
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST_XB(2, 1), 0, NULL, NULL) ){
			return 1;
		}
		if( can_sacrifice_type_as_cost(player, 1, TYPE_ARTIFACT) ){
			return generic_activated_ability(player, card, event, 0, MANACOST_XR(2, 1), 0, NULL, NULL);
		}
		if( can_sacrifice_type_as_cost(player, 1, TYPE_ENCHANTMENT) ){
		return generic_activated_ability(player, card, event, 0, MANACOST_XG(2, 1), 0, NULL, NULL);
	    }
	}

	if( event == EVENT_ACTIVATE ){
		int abils[4] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_CREATURE, MANACOST_XB(2, 1), 0,
													NULL, NULL),
						can_sacrifice_type_as_cost(player, 1, TYPE_ARTIFACT) &&
								generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XR(2, 1), 0, NULL, NULL),
								can_sacrifice_type_as_cost(player, 1, TYPE_ENCHANTMENT) &&
								generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XG(2, 1), 0, NULL, NULL)
		};

		int priorities[4] = {
								0,
								count_subtype(1-player, TYPE_CREATURE, -1) ?
								5+(5*(count_subtype(player, TYPE_CREATURE, -1)-count_subtype(1-player, TYPE_CREATURE, -1))) : 0,
								count_subtype(1-player, TYPE_ARTIFACT, -1) ?
								5+(5*(count_subtype(player, TYPE_ARTIFACT, -1)-count_subtype(1-player, TYPE_ARTIFACT, -1))) : 0,
								count_subtype(1-player, TYPE_ENCHANTMENT, -1) ?
								5+(5*(count_subtype(player, TYPE_ENCHANTMENT, -1)-count_subtype(1-player, TYPE_ENCHANTMENT, -1))) : 0
		};

		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Sac a creature", 		abils[1], priorities[1],
							"Sac an artifact", 		abils[2], priorities[2],
							"Sac an enchantment", 	abils[3], priorities[3]);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_SAC_CREATURE ){
			return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST_XB(2, 1), 0, NULL, NULL);
		}
		if( choice == CHOICE_SAC_ARTIFACT ){
			test_definition_t test;
		    default_test_definition(&test, TYPE_ARTIFACT);
			int sac = new_sacrifice(player, card, player, SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			generic_activated_ability(player, card, event, 0, MANACOST_XR(2, 1), 0, NULL, "TARGET_ARTIFACT");
			if( spell_fizzled != 1 ){
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
		}
		if( choice == CHOICE_SAC_ENCHANTMENT ){
		    test_definition_t test;
		    default_test_definition(&test, TYPE_ENCHANTMENT);
			int sac = new_sacrifice(player, card, player, SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			generic_activated_ability(player, card, event, 0, MANACOST_XG(2, 1), 0, NULL, "TARGET_ENCHANTMENT");
			if( spell_fizzled != 1 ){
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int types[4] = { 0, TYPE_CREATURE, TYPE_ARTIFACT, TYPE_ENCHANTMENT };
		test_definition_t test;
		default_test_definition(&test, types[instance->info_slot]);
		new_sacrifice(player, card, 1-player, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}
	return 0;
}

// artifacts

int card_eye_of_doom(int player, int card, event_t event){
/* Eye of Doom	|4
 * Artifact
 * When ~ enters the battlefield, each player chooses a nonland permanent and puts a doom counter on it.
 * |2, |T, Sacrifice ~: Destroy each permanent with a doom counter on it. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_LAND;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		APNAP(p,{
					td.preferred_controller = 1-p;
					td.who_chooses = p;
					if( can_target(&td) && pick_target(&td, "TARGET_NONLAND_PERMANENT") ){
						instance->number_of_targets = 0;
						add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_DOOM);
					}
				}
		);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int marked[2][151] = {{0}};
		CYCLE_ALL_CARDS(p, c, 	{
									if( in_play(p, c) && is_what(p, c, TYPE_PERMANENT) && ! count_counters(p, c, COUNTER_DOOM) ){
										marked[p][c] = 1;
									}
								}
		);
		APNAP(p,{
					int count;
					for( count = 150; count > -1; --count){
						if( marked[p][count] ){
							kill_card(p, marked[p][count], KILL_DESTROY);
						}
					}
				};
		);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL);
}

int card_surveyors_scope(int player, int card, event_t event){
/*
Surveyor's Scope |2
Artifact
{T}, Exile Surveyor's Scope: Search your library for up to X basic land cards, where X is the number of players who control
	at least two more lands than you. Put those cards onto the battlefield, then shuffle your library.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_LAND, "Select a basic land card.");
	this_test.subtype = SUBTYPE_BASIC;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( count_subtype(1-player, TYPE_LAND, -1) - count_subtype(player, TYPE_LAND, -1) > 1 ){
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_RFG_ME, MANACOST0, 0, NULL, NULL);
}

// lands
int card_opal_palace(int player, int card, event_t event){
/* Opal Palace	""
 * Land
 * |T: Add |C to your mana pool.
 * |1, |T: Add to your mana pool one mana of any color in your commander's color identity.
 * If you spend this mana to cast your commander, it enters the battlefield with a number of additional +1/+1 counters on it equal
 * to the number of times it's been cast from the command zone this game. */

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int count = 0;
		while( count < active_cards_count[player] ){
				if( in_play(player, count) && get_id(player, count) == CARD_ID_ELDER_DRAGON_HIGHLANDER ){
					card_instance_t *this = get_card_instance(player, count);
					int fake = this->info_slot;
					instance->info_slot = cards_data[fake].color;
					instance->info_slot |= get_color_from_remainder_text(cards_data[fake].id);
					break;
				}
				count++;
		}
		if( event == EVENT_RESOLVE_SPELL ){
			play_land_sound_effect_force_color(player, card, instance->info_slot);
		}
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && mana_producer(player, card, EVENT_CAN_ACTIVATE) ){
		declare_mana_available(player, COLOR_COLORLESS, 1);
	}

	if( (event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		enum{
			CHOICE_COLORLESS = 1,
			CHOICE_COLORED,
		};

		int choice = CHOICE_COLORLESS;
		if( has_mana_for_activated_ability(player, card, MANACOST_X(2)) && player != AI && instance->info_slot > 0 ){
			choice = DIALOG(player, card, event, DLG_NO_STORAGE,
							"Add colorless mana", 	1, 1,
							"Add colored mana", 	1, 1);
		}

		if( choice == CHOICE_COLORLESS ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_COLORED ){
			instance->state |= STATE_TAPPED;
			charge_mana_for_activated_ability(player, card, MANACOST_X(1));
			if( spell_fizzled == 1){
				instance->state &= ~STATE_TAPPED;
				return 0;
			}
			produce_mana_tapped_all_one_color(player, card, instance->info_slot, 1);
			if( spell_fizzled == 1){
				instance->state &= ~STATE_TAPPED;
				return 0;
			}
		}
	}

	return 0;
}
