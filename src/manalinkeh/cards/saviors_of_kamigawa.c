#include "manalink.h"

// Global functions
static int sweep(int player, int card, int subtype){

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;
	td.required_subtype = subtype;
	td.allow_cancel = 2; //'Done' button

	card_instance_t *instance = get_card_instance(player, card);

	int amount = 0;
	int targetted[100];
	int tc = 0;

	while( can_target(&td) && tc < 100 ){
			instance->number_of_targets = 0;
			if( new_pick_target(&td, get_hacked_land_text(player, card, "Select a %s to bounce.", subtype), 0, GS_LITERAL_PROMPT) ){
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
				targetted[tc] = instance->targets[0].card;
				tc++;
			}
	}
	instance->number_of_targets = 0;

	int i;
	for(i=0; i<tc; i++){
		bounce_permanent(player, targetted[i]);
		amount++;
	}

	return amount;
}

int epic_legacy(int player, int card, event_t event){
/* Epic (For the rest of the game, you can't cast spells. At the beginning of each of your upkeeps, copy this spell except for its epic ability.)*/
	int card_added = generate_reserved_token_by_id(player, CARD_ID_SPECIAL_EFFECT);
	card_instance_t *instance = get_card_instance(player, card_added);
	instance->targets[0].player = player;
	instance->targets[0].card = SE_TYPE_CANT_BE_PLAYED | SE_EPIC;
	instance->targets[1].player = TYPE_CREATURE | TYPE_ENCHANTMENT | TYPE_ARTIFACT | TYPE_SPELL | TARGET_TYPE_PLANESWALKER;
	instance->targets[1].card = get_id(player, card);
	put_into_play(player, card_added);
	create_card_name_legacy(player, card_added, get_id(player, card));
	return card_added;
}



// cards
int card_adamaro_first_to_desire(int player, int card, event_t event){
	/* Adamaro, First to Desire	|1|R|R
	 * Legendary Creature - Spirit 100/100
	 * ~'s power and toughness are each equal to the number of cards in the hand of the opponent with the most cards in hand. */

	check_legend_rule(player, card, event);

	if (player != -1){
		modify_pt_and_abilities(player, card, event, hand_count[1-player], hand_count[1-player], 0);
	}

	return 0;
}

int card_akki_drillmaster(int player, int card, event_t event){
/* CARD_ID_AKKI_DRILLMASTER	7598
Akki Drillmaster	|2|R
Creature - Goblin Shaman 2/2
|T: Target creature gains haste until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			int mod = -48;
			if( is_sick(instance->targets[0].player,  instance->targets[0].card) ){
				mod = 0;
				if( current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS ){
					if( get_power(instance->targets[0].player,  instance->targets[0].card) >
						get_power(player, card) &&
						get_toughness(instance->targets[0].player,  instance->targets[0].card) >
						get_toughness(player, card) )
					{
						mod += 48;
					}
				}
				if( current_phase >= PHASE_AFTER_BLOCKING ){
					int iid = get_card_instance(instance->targets[0].player,  instance->targets[0].card)->internal_card_id;
					if( cards_data[iid].extra_ability & EA_ACT_ABILITY ){
						mod += 24;
					}
				}
			}
			ai_modifier += mod;
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player,  instance->targets[0].card, 0, 0, 0, SP_KEYWORD_HASTE);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_akuta_born_from_ashes(int player, int card, event_t event){
/* CARD_ID_AKUTA_BORN_OF_ASH	7600
Akuta, Born of Ash	|2|B|B
Legendary Creature - Spirit 3/2
Haste
At the beginning of your upkeep, if you have more cards in hand than each opponent, you may sacrifice |Ha Swamp.
If you do, return ~ from your graveyard to the battlefield. */

	check_legend_rule(player, card, event);

	haste(player, card, event);

	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		if( hand_count[player] > hand_count[1-player] ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_PERMANENT, "Select a Swamp to sacrifice.");
			test.subtype = SUBTYPE_SWAMP;
			if( new_can_sacrifice(player, card, player, &test) ){
				int ai_mode = ! check_battlefield_for_id(2, CARD_ID_GRAFDIGGERS_CAGE) &&
							count_subtype(player, TYPE_LAND, SUBTYPE_SWAMP) >= 2 ? 0 : 1;
				int choice = do_dialog(player, player, card, -1, -1," Return Akuta to play\n Decline", ai_mode);
				if( choice == 0 ){
					if( new_sacrifice(player, card, player, 0, &test) ){
						if( ! check_battlefield_for_id(2, CARD_ID_GRAFDIGGERS_CAGE) ){
							remove_state(player, card, STATE_INVISIBLE);
							put_into_play(player, card);
							return -1;
						}
					}
				}
			}
		}
		return -2;
	}

	return 0;
}

// araba mothrider --> devoted retainer

int card_arashi_the_sky_asunder(int player, int card, event_t event){
	/* Arashi, the Sky Asunder	|3|G|G
	 * Legendary Creature - Spirit 5/5
	 * |X|G, |T: ~ deals X damage to target creature with flying.
	 * Channel - |X|G|G, Discard Arashi: Arashi deals X damage to each creature with flying. */

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND  ){
		if( has_mana_multi(player, MANACOST_G(2)) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE_FROM_HAND ){
		charge_mana_multi(player, MANACOST_XG(-1, 2));
		if( spell_fizzled != 1 ){
			get_card_instance(player, card)->targets[0].player = x_value;
			discard_card(player, card);
			return 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		int card_added = add_card_to_hand(player, get_internal_card_id_from_csv_id(CARD_ID_ARASHI_THE_SKY_ASUNDER));
		card_instance_t* added_inst = get_card_instance(player, card_added);
		added_inst->state |= STATE_INVISIBLE;	--hand_count[player];

		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.keyword = KEYWORD_FLYING;

		new_damage_all(player, card_added, ANYBODY, get_card_instance(player, card)->targets[0].player, 0, &test);

		obliterate_card(player, card_added);
	}

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST_XG(-1, 1), 0,
									&td, "Select target creature with flying.");
}

int card_aether_shockwave(int player, int card, event_t event){
/* CARD_ID_AETHER_SHOCKWAVE	7604
Aether Shockwave	|3|W
Instant
Choose one -
* Tap all Spirits.
* Tap all non-Spirit creatures. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_SPIRIT;
		test.state = STATE_TAPPED;
		test.state_flag = DOESNT_MATCH;

		int my_count_spirit = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test);
		int his_count_spirit = check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &test);

		int ai_choice = 1;
		if( his_count_spirit >  my_count_spirit ){
			ai_choice = 0;
		}
		int choice = do_dialog(player, player, card, -1, -1, " Tap all Spirits\n Tap all non-Spirit creatures", ai_choice);

		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_SPIRIT;
		test.subtype_flag = choice == 0 ? MATCH : DOESNT_MATCH;

		APNAP(p, { new_manipulate_all(player, card, p, &test, ACT_TAP); };);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

// ayumi the last vistor --> livonya silone

// blood clock --> umbilicus

int card_bounteous_kirin(int player, int card, event_t event){
/* CARD_ID_BOUNTEOUS_KIRIN	7608
Bounteous Kirin	|5|G|G
Legendary Creature - Kirin Spirit 4/4
Flying
Whenever you cast a Spirit or Arcane spell, you may gain life equal to that spell's converted mana cost. */

	check_legend_rule(player, card, event);

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		gain_life(player, get_cmc(trigger_cause_controller, trigger_cause));
	}

	return 0;
}

// captive flame --> Ghitu war cry

int card_celestial_kirin(int player, int card, event_t event){
/* CARD_ID_CELESTIAL_KIRIN	7612
Celestial Kirin	|2|W|W
Legendary Creature - Kirin Spirit 3/3
Flying
Whenever you cast a Spirit or Arcane spell, destroy all permanents with that spell's converted mana cost. */

	check_legend_rule(player, card, event);

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.cmc = get_cmc(trigger_cause_controller, trigger_cause);

		APNAP(p, { new_manipulate_all(player, card, p, &test, KILL_DESTROY); };);
	}

	return 0;
}

ARCANE(card_charge_across_the_araba){
/* CARD_ID_CHARGE_ACROSS_THE_ARABA	7613
Charge Across the Araba	|4|W
Instant - Arcane
Sweep - Return any number of |H1Plains you control to their owner's hand. Creatures you control get +1/+1 until end of turn for each |H2Plains returned this way.*/

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = sweep(player, card, SUBTYPE_PLAINS);
		pump_subtype_until_eot(player, card, player, -1, amount, amount, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_choice_of_damnation(int player, int card, event_t event){
/*
Choice of Damnations |5|B
Sorcery - Arcane
Target opponent chooses a number.
You may have that player lose that much life.
If you don't, that player sacrifices all but that many permanents.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = 0;
			if( instance->targets[0].player != AI ){
				amount = choose_a_number(instance->targets[0].player, "Choose a number.", life[player]);
			}
			else{
				amount = count_subtype(instance->targets[0].player, TYPE_PERMANENT, -1);
				while( amount >= life[instance->targets[0].player] && amount > 0){
						amount--;
				}
			}
			char buffer[100];
			snprintf(buffer, 100, "Opponent chooses %d.", amount );
			do_dialog(player, player, card, -1, -1, buffer, 0);

			int ai_choice = 0;
			int total_permanents_of_opponent = count_subtype(instance->targets[0].player, TYPE_PERMANENT, -1);
			if( life[instance->targets[0].player] - amount >= 6 && total_permanents_of_opponent-amount < 4 ){
				ai_choice = 1;
			}
			snprintf(buffer, 100, " Opponent loses %d life\n Opponent sacrifices all but %d permanents", amount, amount );
			int choice = do_dialog(player, player, card, -1, -1, buffer, ai_choice);
			if( choice == 0 ){
				lose_life(instance->targets[0].player, amount);
			}
			else{
				test_definition_t test;
				default_test_definition(&test, TYPE_PERMANENT);
				test.qty = total_permanents_of_opponent - amount;
				new_sacrifice(player, card, instance->targets[0].player, SAC_CAUSED | SAC_NO_CANCEL, &test);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_cloudhoof_kirin(int player, int card, event_t event){
/* CARD_ID_CLOUDHOOF_KIRIN	7615
Cloudhoof Kirin	|3|U|U
Legendary Creature - Kirin Spirit 4/4
Flying
Whenever you cast a Spirit or Arcane spell, you may have target player put the top X cards of his or her library into his or her graveyard,
where X is that spell's converted mana cost. */

	check_legend_rule(player, card, event);

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		int amount = get_cmc(trigger_cause_controller, trigger_cause);

		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
			mill(instance->targets[0].player, amount);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

ARCANE(card_death_denied)
{
/* CARD_ID_DEATH_DENIED	7619
Death Denied	|X|B|B
Instant - Arcane
Return X target creature cards from your graveyard to your hand. */

	if (event == EVENT_CAN_CAST){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		return generic_spell(player, card, event, GS_X_SPELL | GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
	}

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int max_cards = count_graveyard_by_type(player, TYPE_CREATURE);
	  int generic_mana_available = has_mana(player, COLOR_ANY, 1);	// A useful trick from card_fireball().
	  max_cards = MIN(max_cards, generic_mana_available);
	  max_cards = MIN(max_cards, 19);

	  if (max_cards <= 0 || graveyard_has_shroud(player))
		{
		  ai_modifier -= 48;
		  instance->info_slot = 0;
		  return 0;
		}

	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  if (ai_is_speculating != 1)
		{
		  if (max_cards == 1)
			strcpy(test.message, "Select a creature card.");
		  else
			sprintf(test.message, "Select up to %d creature cards.", max_cards);
		}

	  target_t targets[19];
	  int num_chosen = select_multiple_cards_from_graveyard(player, player, 0, AI_MAX_VALUE, &test, max_cards, &targets[0]);

	  SET_BYTE0(instance->info_slot) = num_chosen;

	  if (num_chosen == 0)
		return 0;

	  charge_mana(player, COLOR_COLORLESS, instance->info_slot);
	  if (cancel == 1)
		return 0;

	  SET_BYTE1(instance->info_slot) = player;
	  int leg = create_targetted_legacy_effect(player, card, &empty, player, card);
	  if (leg == -1)
		SET_BYTE2(instance->info_slot) = 255;
	  else
		{
		  SET_BYTE2(instance->info_slot) = leg;
		  card_instance_t* legacy = get_card_instance(player, leg);
		  legacy->token_status |= STATUS_INVISIBLE_FX;
		  ASSERT(sizeof(legacy->targets) == sizeof(targets));
		  memcpy(legacy->targets, targets, sizeof(targets));
		}
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int num_chosen = BYTE0(instance->info_slot);
	  if (num_chosen == 0)
		return 0;

	  int orig_player = BYTE1(instance->info_slot);
	  int leg = BYTE2(instance->info_slot);
	  if (leg == 255)
		return 0;

	  int i;
	  for (i = 0; i < num_chosen; ++i)
		{
		  int selected = validate_target_from_grave(orig_player, leg, player, i);
		  if (selected != -1)
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
	}

  return 0;
}

ARCANE(card_death_of_a_thousand_stings){
/* CARD_ID_DEATH_OF_A_THOUSAND_STINGS	7620
Death of a Thousand Stings	|4|B
Instant - Arcane
Target player loses 1 life and you gain 1 life.
At the beginning of your upkeep, if you have more cards in hand than each opponent, you may return ~ from your graveyard to your hand. */

	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		if( hand_count[player] > hand_count[1-player] ){
			int choice = do_dialog(player, player, card, -1, -1," Return Death of a Thousand Stings\n Pass", 0);
			if( choice == 0 ){
				remove_state(player, card, STATE_INVISIBLE);
				return -1;
			}
		}
		return -2;
	}

	if (!IS_GS_EVENT(player, card, event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card) ) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}


	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			lose_life(instance->targets[0].player, 1);
			gain_life(player, 1);
		}
	}

	return 0;
}

ARCANE(card_gaze_of_adamaro)
{
  /* Gaze of Adamaro	|2|R|R
   * Instant - Arcane
   * ~ deals damage equal to the number of cards in target player's hand to that player. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;

  if (event == EVENT_RESOLVE_SPELL && valid_target(&td))
	damage_target0(player, card, hand_count[get_card_instance(player, card)->targets[0].player]);

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_descendant_of_kiyomaro(int player, int card, event_t event){
/* CARD_ID_DESCENDANT_OF_KIYOMARO	7624
Descendant of Kiyomaro	|1|W|W
Creature - Human Soldier 2/3
As long as you have more cards in hand than each opponent, ~ gets +1/+2 and has "Whenever this creature deals combat damage, you gain 3 life." */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( hand_count[player] > hand_count[1-player] ){
			switch( event ){
					case EVENT_POWER:
						event_result++;
						break;
					case EVENT_TOUGHNESS:
						event_result += 2;
						break;
					default:
						break;
			}
			int result = damage_dealt_by_me_arbitrary(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE, player, card);
			if( result ){
				gain_life(player, result * 3);
			}
		}
	}

	return 0;
}

int card_descendant_of_masumaro(int player, int card, event_t event){
/* CARD_ID_DESCENDANT_OF_MASUMARO	7625
Descendant of Masumaro	|2|G
Creature - Human Monk 1/1
At the beginning of your upkeep, put a +1/+1 counter on ~ for each card in your hand, then remove a +1/+1 counter from ~ for each card in target opponent's hand.*/

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_1_1_counters(player, card, hand_count[player]);
		int amount = hand_count[1-player];
		if( amount > count_1_1_counters(player, card) ){
			amount = count_1_1_counters(player, card);
		}
		remove_1_1_counters(player, card, amount);
	}
	return 0;
}

int card_descendant_of_soramaro(int player, int card, event_t event){
/* CARD_ID_DESCENDANT_OF_SORAMARO	7626
Descendant of Soramaro	|3|U
Creature - Human Wizard 2/3
|1|U: Look at the top X cards of your library, where X is the number of cards in your hand, then put them back in any order. */

	if( event == EVENT_ACTIVATE ){
		if( hand_count[player] >= 3 ){
			int total = get_total_available_lands(player);
			if( total <= 3 || total >= 6 ){
				ai_modifier += 48;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( hand_count[player] > 0 ){
			rearrange_top_x(player, player, hand_count[player]);
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XU(1, 1), 0, NULL, NULL);
}

int card_ebony_owl_netsuke(int player, int card, event_t event){
/* CARD_ID_EBONY_OWL_NETSUKE	7629
Ebony Owl Netsuke	|2
Artifact
At the beginning of each opponent's upkeep, if that player has seven or more cards in hand, ~ deals 4 damage to him or her. */

	upkeep_trigger_ability(player, card, event, 1-player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( hand_count[current_turn] > 6 ){
			damage_player(current_turn, 4, player, card);
		}
	}
	return 0;
}

int card_eiganjo_free_riders(int player, int card, event_t event){
/* CARD_ID_EIGANJO_FREE_RIDERS	7630
Eiganjo Free-Riders	|3|W
Creature - Human Soldier 3/4
Flying
At the beginning of your upkeep, return a |Swhite creature you control to its owner's hand. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.allow_cancel = 0;
		td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		td.illegal_abilities = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) ){
			char msg[100];
			scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select a %s creature you control.", COLOR_WHITE));
			if( new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
		instance->number_of_targets = 0;
	}

	return 0;
}


int card_elder_pine_of_jukai(int player, int card, event_t event){
/* CARD_ID_ELDER_PINE_OF_JUKAI	7631
Elder Pine of Jukai	|2|G
Creature - Spirit 2/1
Whenever you cast a Spirit or Arcane spell, reveal the top three cards of your library.
Put all land cards revealed this way into your hand and the rest on the bottom of your library in any order.
Soulshift 2 (When this creature dies, you may return target Spirit card with converted mana cost 2 or less from your graveyard to your hand.) */

	if( arcane_spirit_spell_trigger(player, card, event, 2) ){
		int amount = MIN(3, count_deck(player));
		if( ! amount ){
			return 0;
		}
		int *deck = deck_ptr[player];
		show_deck(0, deck, amount, "Here's the revealed cards", 0, 0x7375B0 );
		show_deck(1, deck, amount, "Here's the revealed cards", 0, 0x7375B0 );
		int i;
		int found = 0;
		for(i=0; i<amount; i++){
			if( is_what(-1, deck[i], TYPE_LAND) ){
				add_card_to_hand(player, deck[i]);
				remove_card_from_deck(player, i);
				found++;
			}
		}
		amount-=found;
		if( amount > 0 ){
			put_top_x_on_bottom(player, player, amount);
		}
	}

	return soulshift(player, card, event, 2, 1);
}

int card_endless_swarm(int player, int card, event_t event){
	/* Endless Swarm	|5|G|G|G
	 * Sorcery
	 * Put a 1/1 |Sgreen Snake creature token onto the battlefield for each card in your hand.
	 * Epic */

	if(event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_SNAKE, hand_count[player]);
		if( ! check_special_flags3(player, card, SF3_EPIC) ){
			epic_legacy(player, card, event);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_enduring_ideal(int player, int card, event_t event){
/* CARD_ID_ENDURING_IDEAL	7633
Enduring Ideal	|5|W|W
Sorcery
Search your library for an enchantment card and put it onto the battlefield. Then shuffle your library.
Epic (For the rest of the game, you can't cast spells. At the beginning of each of your upkeeps, copy this spell except for its epic ability.)*/

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ENCHANTMENT, "Select an enchantment card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
		if( ! check_special_flags3(player, card, SF3_EPIC) ){
			epic_legacy(player, card, event);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_erayo_soratami_ascendant(int player, int card, event_t event){
/* CARD_ID_ERAYO_SORATAMI_ASCENDANT	7634
Erayo, Soratami Ascendant	|1|U
Legendary Creature - Moonfolk Monk 1/1
Flying
Whenever the fourth spell of a turn is cast, flip ~.
--FLIP--
~'s Essence
Legendary Enchantment
Whenever an opponent casts his or her first spell each turn, counter that spell. */

	double_faced_card(player, card, event);

	check_legend_rule(player, card, event);

	if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		if( get_storm_count() == 3 ){
			get_card_instance(player, card)->targets[2].player = 4;
			true_transform(player, card);
		}
	}

	return 0;
}

int card_erayos_essence(int player, int card, event_t event){
/* CARD_ID_ERAYOS_ESSENCE	7635
Erayo's Essence	|1|U
Legendary Enchantment
Whenever an opponent casts his or her first spell each turn, counter that spell. */

	check_legend_rule(player, card, event);

	if( event == EVENT_CAST_SPELL && affected_card_controller != player ){
		if( ! is_what(affected_card_controller, affected_card_controller, TYPE_LAND) &&
			! is_token(affected_card_controller, affected_card_controller) )
		{
			get_card_instance(player, card)->targets[2].player++;
		}
	}

	if( get_card_instance(player, card)->targets[2].player == 1 && ! is_humiliated(player, card) &&
		specific_spell_played(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, 0, 0, 0, 0, 0, 0, 0, -1, 0) )
	{
		if( ! check_state(trigger_cause_controller, trigger_cause, STATE_CANNOT_TARGET) ){
			real_counter_a_spell(player, card, trigger_cause_controller, trigger_cause);
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[2].player = 0;
	}

	return 0;
}

int card_eternal_dominion(int player, int card, event_t event){
/* CARD_ID_ETERNAL_DOMINION	7636
Eternal Dominion	|7|U|U|U
Sorcery
Search target opponent's library for an artifact, creature, enchantment, or land card. Put that card onto the battlefield under your control.
Then that player shuffles his or her library.
Epic (For the rest of the game, you can't cast spells. At the beginning of each of your upkeeps, copy this spell except for its epic ability.
You may choose a new target for the copy.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_PERMANENT, "Select a creature, land, artifact or enchantment card.");
			test.type_flag = F1_NO_PWALKER;
			new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
			if( ! check_special_flags3(player, card, SF3_EPIC) ){
				epic_legacy(player, card, event);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, "TARGET_PLAYER", 1, NULL);
}

static int card_evermind_impl(int player, int card, event_t event)
{
/* CARD_ID_EVERMIND	7637
Evermind	""
Instant - Arcane
(Nonexistent mana costs can't be paid.)
Draw a card.
Splice onto Arcane |1|U (As you cast an Arcane spell, you may reveal this card from your hand and pay its splice cost.
If you do, add this card's effects to that spell.)*/

  if (event == EVENT_CAN_CAST)
	return 1;

  if (event == EVENT_RESOLVE_SPELL)
	draw_cards(player, 1);

  return 0;
}
int card_evermind(int player, int card, event_t event)
{
  // Nonexistent mana costs can't be paid
  if (event == EVENT_CAN_CAST && !played_for_free(player, card))
	return 0;
  return arcane_with_splice(player, card, event, card_evermind_impl, MANACOST_XU(1,1));
}

ARCANE(card_exile_into_darkness){
/* CARD_ID_EXILE_INTO_DARKNESS	7638
Exile into Darkness	|4|B
Sorcery
Target player sacrifices a creature with converted mana cost 3 or less.
At the beginning of your upkeep, if you have more cards in hand than each opponent, you may return ~ from your graveyard to your hand. */

	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		if( hand_count[player] > hand_count[1-player] ){
			int choice = do_dialog(player, player, card, -1, -1," Return Exile into Darkness\n Pass", 0);
			if( choice == 0 ){
				remove_state(player, card, STATE_INVISIBLE);
				return -1;
			}
		}
		return -2;
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature with converted mana cost 3 or less to sacrifice.");
			test.cmc = 4;
			test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
			new_sacrifice(player, card, instance->targets[0].player, SAC_CAUSED | SAC_NO_CANCEL, &test);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_feral_lightning(int player, int card, event_t event){
/* CARD_ID_FERAL_LIGHTNING	7639
Feral Lightning	|3|R|R|R
Sorcery
Create three 3/1 |Sred Elemental creature tokens with haste. Exile them at the beginning of the next end step. */

	if( event == EVENT_RESOLVE_SPELL){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
		token.qty = 3;
		token.pow = 3;
		token.tou = 1;
		token.special_infos = 66;
		token.s_key_plus = SP_KEYWORD_HASTE;
		generate_token(&token);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

ARCANE(card_footsteps_of_the_goryo){
/* CARD_ID_FOOTSTEPS_OF_THE_GORYO	7641
Footsteps of the Goryo	|2|B
Sorcery - Arcane
Return target creature card from your graveyard to the battlefield. Sacrifice that creature at the beginning of the next end step. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");
	test.ai_selection_mode = AI_MAX_CMC;

	if(event == EVENT_RESOLVE_SPELL){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected > -1 ){
			int zombie = reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
			if (zombie != -1 && in_play(player, zombie)){
				int leg = pump_ability_until_eot(player, card, player, zombie, 0, 0, 0, SP_KEYWORD_DIE_AT_EOT);
				if (leg != -1){
					card_instance_t* legacy = get_card_instance(player, leg);
					legacy->targets[3].card = KILL_SACRIFICE;
				}
			}
		}
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
}

int card_ghost_lit_nourisher(int player, int card, event_t event){
	/* Ghost-Lit Nourisher	|2|G
	 * Creature - Spirit 2/1
	 * |2|G, |T: Target creature gets +2/+2 until end of turn.
	 * Channel - |3|G, Discard ~: Target creature gets +4/+4 until end of turn. */

	if( ! IS_GAA_EVENT(event) && ! IS_ACTIVATING_FROM_HAND(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND  ){
		if( has_mana_multi(player, MANACOST_XG(3, 1)) ){
			return can_target(&td);
		}
	}

	if( event == EVENT_ACTIVATE_FROM_HAND ){
		charge_mana_multi(player, MANACOST_XG(3, 1));
		if( spell_fizzled != 1 && pick_target(&td, "TARGET_CREATURE") ){
			discard_card(player, card);
			return 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 4, 4);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XG(2, 1), 0, &td, "TARGET_CREATURE");
}

int card_ghost_lit_raider(int player, int card, event_t event){
	/* Ghost-Lit Raider	|2|R
	 * Creature - Spirit 2/1
	 * |2|R, |T: ~ deals 2 damage to target creature.
	 * Channel - |3|R, Discard ~: ~ deals 4 damage to target creature. */

	if( ! IS_GAA_EVENT(event) && ! IS_ACTIVATING_FROM_HAND(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND  ){
		if( has_mana_multi(player, MANACOST_XR(3, 1)) ){
			return can_target(&td);
		}
	}

	if( event == EVENT_ACTIVATE_FROM_HAND ){
		charge_mana_multi(player, MANACOST_XR(3, 1));
		if( spell_fizzled != 1 && pick_target(&td, "TARGET_CREATURE") ){
			discard_card(player, card);
			return 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XR(2, 1), 0, &td, "TARGET_CREATURE");
}

int card_ghost_lit_redeemer(int player, int card, event_t event){
	/* Ghost-Lit Redeemer	|W
	 * Creature - Spirit 1/1
	 * |W, |T: You gain 2 life.
	 * Channel - |1|W, Discard ~: You gain 4 life. */

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND  ){
		if( has_mana_multi(player, MANACOST_XW(1, 1)) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE_FROM_HAND ){
		charge_mana_multi(player, MANACOST_XW(1, 1));
		if( spell_fizzled != 1 ){
			discard_card(player, card);
			return 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		gain_life(player, 4);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 2);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_W(1), 0, NULL, NULL);
}

int card_ghost_lit_stalker(int player, int card, event_t event){
	/* Ghost-Lit Stalker	|B
	 * Creature - Spirit 1/1
	 * |4|B, |T: Target player discards two cards. Activate this ability only any time you could cast a sorcery.
	 * Channel - |5|B|B, Discard ~: Target player discards four cards. Activate this ability only any time you could cast a sorcery. */

	if( ! IS_GAA_EVENT(event) && ! IS_ACTIVATING_FROM_HAND(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND  ){
		if( has_mana_multi(player, MANACOST_XB(5, 2)) && can_sorcery_be_played(player, event) ){
			return can_target(&td);
		}
	}

	if( event == EVENT_ACTIVATE_FROM_HAND ){
		charge_mana_multi(player, MANACOST_XB(5, 2));
		if( spell_fizzled != 1 && pick_target(&td, "TARGET_PLAYER") ){
			discard_card(player, card);
			return 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		if( valid_target(&td) ){
			new_multidiscard(instance->targets[0].player, 4, 0, player);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			new_multidiscard(instance->targets[0].player, 2, 0, player);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XB(4, 1), 0,
									&td, "TARGET_PLAYER");
}

// glitterfang --> viashino sandstalker

// hand of cruelty / honor --> devoted retainer

int card_haru_onna(int player, int card, event_t event){
/* CARD_ID_HARU_ONNA	7655
Haru-Onna	|3|G
Creature - Spirit 2/1
When ~ enters the battlefield, draw a card.
Whenever you cast a Spirit or Arcane spell, you may return ~ to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		bounce_permanent(player, card);
	}

	return 0;
}

int card_hidetsugus_second_rite(int player, int card, event_t event){
/* CARD_ID_HIDETSUGUS_SECOND_RITE	7656
Hidetsugu's Second Rite	|3|R
Instant
If target player has exactly 10 life, ~ deals 10 damage to that player. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int target = instance->targets[0].player;
			if( life[ target ] == 10 ){
				damage_player(target, 10, player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

static int homura_human_ascendant_flip_legacy(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	if( event == EVENT_CHANGE_TYPE && instance->targets[0].player > -1 && affect_me(instance->targets[0].player, instance->targets[0].card) &&
		instance->targets[1].player != 66
	  ){
		if( instance->targets[1].card == -1 ){
			instance->targets[1].card = get_internal_card_id_from_csv_id(CARD_ID_HOMURAS_ESSENCE);
		}
		event_result = instance->targets[1].card;
	}
	if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && player == reason_for_trigger_controller
		&& instance->targets[0].player == trigger_cause_controller && instance->targets[0].card == trigger_cause
	  ){
		if (event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if (event == EVENT_RESOLVE_TRIGGER){
				true_transform(instance->targets[0].player, instance->targets[0].card);
				kill_card(player, card, KILL_EXILE);
		}
	}
	return 0;
}


int card_homuras_essence(int player, int card, event_t event){
/* CARD_ID_HOMURAS_ESSENCE	7658
Homura's Essence	|4|R|R
Legendary Enchantment
Creatures you control get +2/+2 and have flying and "|R: This creature gets +1/+0 until end of turn." */

	check_legend_rule(player, card, event);

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		instance->targets[13].player = CARD_ID_HOMURAS_ESSENCE;
		instance->targets[13].card = CARD_ID_HOMURA_HUMAN_ASCENDANT;
	}

	boost_creature_type(player, card, event, -1, 2, 2, KEYWORD_FLYING, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);

	return card_ghitu_war_cry(player, card, event);
}

int card_homura_human_ascendant(int player, int card, event_t event)
{
/* CARD_ID_HOMURA_HUMAN_ASCENDANT	7657
Homura, Human Ascendant	|4|R|R
Legendary Creature - Human Monk 4/4
~ can't block.
When ~ dies, return it to the battlefield flipped.
--FLIP--
~'s Essence
Legendary Enchantment
Creatures you control get +2/+2 and have flying and "|R: This creature gets +1/+0 until end of turn." */

  check_legend_rule(player, card, event);

  nice_creature_to_sacrifice(player, card);

  double_faced_card(player, card, event);

  int owner, position;
  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY)
	  && find_in_owners_graveyard(player, card, &owner, &position))
	{
		int to_add = add_card_to_hand(owner, get_grave(owner)[position]);
		set_special_flags3(owner, to_add, SF3_REANIMATED);
		int legacy = create_legacy_effect(owner, to_add,  &homura_human_ascendant_flip_legacy);
		card_instance_t *instance = get_card_instance(owner, legacy);
		instance->targets[0].player = owner;
		instance->targets[0].card = to_add;
		obliterate_card_in_grave(owner, position);
		put_into_play(owner, to_add);
	}

  return 0;
}

static int effect_ideas(int player, int card, event_t event){

	if( eot_trigger(player, card, event ) ){
		new_multidiscard(player, 3, 0, player);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

ARCANE(card_ideas_unbound){
/* CARD_ID_IDEAS_UNBOUND	7659
Ideas Unbound	|U|U
Sorcery - Arcane
Draw three cards. Discard three cards at the beginning of the next end step. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_LAND, -1) * 8);
	}

	if( event == EVENT_RESOLVE_SPELL){
		create_legacy_effect(player, card, &effect_ideas );
		draw_cards (player,3);
	}

	return basic_spell(player, card, event);
}

int card_iizuka_the_restless(int player, int card, event_t event){
/* CARD_ID_IIZUKA_THE_RUTHLESS	7660
Iizuka the Ruthless	|3|R|R
Legendary Creature - Human Samurai 3/3
Bushido 2 (Whenever this creature blocks or becomes blocked, it gets +2/+2 until end of turn.)
|2|R, Sacrifice a Samurai: Samurai creatures you control gain double strike until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return bushido(player, card, event, 2);
	}

	card_instance_t *instance = get_card_instance(player, card);

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Samurai to sacrifice.");
	test.subtype = SUBTYPE_SAMURAI;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, SUBTYPE_SAMURAI, 0, 0, KEYWORD_DOUBLE_STRIKE, 0);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST_XR(2, 1), 0, NULL, NULL, 0, &test, 0);
}

int card_iname_as_one(int player, int card, event_t event)
{
  /* Iname as One	|8|B|B|G|G
   * Legendary Creature - Spirit 8/8
   * When ~ enters the battlefield, if you cast it from your hand, you may search your library for a Spirit permanent card,
   * 	put it onto the battlefield, then shuffle your library.
   * When ~ dies, you may exile it. If you do, return target Spirit permanent card from your graveyard to the battlefield. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a Spirit permanent card.");
		this_test.subtype = SUBTYPE_SPIRIT;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
	}

	int mode = RESOLVE_TRIGGER_OPTIONAL;
	// AI activates only if there's another targetable Spirit in his graveyard
	if( event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(player, card) ){
		if( is_this_dying(player, card) && IS_AI(player) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_PERMANENT);
			this_test.subtype = SUBTYPE_SPIRIT;
			if( new_special_count_grave(player, &this_test) // card won't be in graveyard yet during EVENT_GRAVEYARD_FROM_PLAY
				&& ! !graveyard_has_shroud(player) )
			{
				mode = RESOLVE_TRIGGER_MANDATORY;
			}
		}
	}

	if( this_dies_trigger(player, card, event, mode) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_SPIRIT;
		if( exile_from_owners_graveyard(player, card)
			&& new_special_count_grave(player, &this_test)	// card's already been exiled, so still correct to only need one spirit
			&& !graveyard_has_shroud(player) )
		{
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_CMC, &this_test);
		}
	}

	return 0;
}

int card_infernal_kirin(int player, int card, event_t event){
/* CARD_ID_INFERNAL_KIRIN	7662
Infernal Kirin	|2|B|B
Legendary Creature - Kirin Spirit 3/3
Flying
Whenever you cast a Spirit or Arcane spell, target player reveals his or her hand and discards all cards with that spell's converted mana cost. */

	check_legend_rule(player, card, event);

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		int amount = get_cmc(trigger_cause_controller, trigger_cause);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
			ec_definition_t ec;
			default_ec_definition(instance->targets[0].player, player, &ec);
			ec.effect = EC_ALL_WHICH_MATCH_CRITERIA | EC_DISCARD;

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			this_test.cmc = amount;
			new_effect_coercion(&ec, &this_test);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_ivory_crane_netsuke(int player, int card, event_t event){
/* CARD_ID_IVORY_CRANE_NETSUKE	7667
Ivory Crane Netsuke	|2
Artifact
At the beginning of your upkeep, if you have seven or more cards in hand, you gain 4 life. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( hand_count[current_turn] > 6 ){
			gain_life(current_turn, 4);
		}
	}
	return 0;
}

int card_jiwari_the_earth_aflame(int player, int card, event_t event){
	/* Jiwari, the Earth Aflame	|3|R|R
	 * Legendary Creature - Spirit 3/3
	 * |X|R, |T: ~ deals X damage to target creature without flying.
	 * Channel - |X|R|R|R, Discard Jiwari: Jiwari deals X damage to each creature without flying. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) && ! IS_ACTIVATING_FROM_HAND(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND  ){
		if( has_mana_multi(player, MANACOST_R(3)) ){
			return 1;
		}
	}
	if( event == EVENT_ACTIVATE_FROM_HAND ){
		charge_mana_multi(player, MANACOST_XR(-1, 3));
		if( spell_fizzled != 1 ){
			instance->targets[0].player = x_value;
			discard_card(player, card);
			return 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		int card_added = add_card_to_hand(player, get_internal_card_id_from_csv_id(CARD_ID_JIWARI_THE_EARTH_AFLAME));
		card_instance_t* added_inst = get_card_instance(player, card_added);
		added_inst->state |= STATE_INVISIBLE;   --hand_count[player];

		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.keyword = KEYWORD_FLYING;
		new_damage_all(player, card_added, ANYBODY, instance->targets[0].player, 0, &test);

		obliterate_card(player, card_added);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_XR(-1, 1), 0, &td, "TARGET_CREATURE");
}

int card_kagemaro_first_to_suffer(int player, int card, event_t event){
	/* Kagemaro, First to Suffer	|3|B|B
	 * Legendary Creature - Demon Spirit 100/100
	 * ~'s power and toughness are each equal to the number of cards in your hand.
	 * |B, Sacrifice Kagemaro: All creatures get -X/-X until end of turn, where X is the number of cards in your hand. */

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		APNAP(p, { pump_subtype_until_eot(player, card, p, -1, -hand_count[player], -hand_count[player], 0, 0); };);
	}

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && !is_humiliated(player, card) && player != -1){
		event_result += hand_count[player];
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_B(1), 0, NULL, NULL);
}

static void store_iid_on_kaho(int player, int card, int iid){
	card_instance_t *instance = get_card_instance(player, card);
	int pos = BYTE0(instance->info_slot);
	while( pos < 10 ){
			if( instance->targets[pos].player == -1 ){
				instance->targets[pos].player = iid;
				break;
			}
			if( instance->targets[pos].card == -1 ){
				instance->targets[pos].card = iid;
				break;
			}
			pos++;
	}
	SET_BYTE0(instance->info_slot) = pos;
}

static int retrieve_stored_iid_from_kaho(int player, int card, int arr[]){
	card_instance_t *instance = get_card_instance(player, card);
	int i;
	int ac = 0;
	for(i=0; i<10; i++){
		int iid = instance->targets[i].player;
		if( iid == -1 ){
			break;
		}
		instance->targets[i].player = -1;
		if( check_rfg(get_owner(player, card), cards_data[iid].id) ){
			arr[ac] = iid;
			ac++;
		}
		iid = instance->targets[i].card;
		if( iid == -1 ){
			break;
		}
		instance->targets[i].card = -1;
		if( check_rfg(get_owner(player, card), cards_data[iid].id) ){
			arr[ac] = iid;
			ac++;
		}
	}
	return ac;
}


int card_kaho_minamo_hystorian(int player, int card, event_t event){
/* Legendary Creature - Human Wizard 2/2
 * When ~ enters the battlefield, search your library for up to three instant cards and exile them. Then shuffle your library.
 * |X, |T: You may cast a card with converted mana cost X exiled with ~ without paying its mana cost. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);
		int max = MIN(3, count_deck(player));
		if( ! max ){
			return 0;
		}
		int pos = BYTE0(instance->info_slot);
		if( pos >= 10 ){
			return 0;
		}
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_INSTANT | TYPE_INTERRUPT, "Select an instant card to exile.");
		if( player == AI ){
			this_test.cmc = 4;
			this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		}
		this_test.no_shuffle = 1;
		while( max ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_MAX_VALUE, -1, &this_test);
				if( selected > -1 ){
					int iid = deck_ptr[player][selected];
					store_iid_on_kaho(player, card, iid);
					rfg_card_in_deck(player, selected);
					max--;
				}
				else{
					break;
				}
		}
		shuffle(player);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_PLAY_EXILED_CARD = 1,
		CHOICE_SHOW_EXILED
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			int exiled[18];
			int max_exiled = retrieve_stored_iid_from_kaho(player, card, exiled);
			if( max_exiled ){
				int can_play_instant = 0;
				int can_play_interrupt = 0;
				int i;
				for(i=0; i<max_exiled; i++){
						if( has_mana_for_activated_ability(player, card, MANACOST_X(get_cmc_by_internal_id(exiled[i]))) ){
							int result = can_legally_play_iid(player, exiled[i]);
							if( result == 99 ){
								can_play_interrupt = 1;
								break;
							}
							if( result == 1 ){
								can_play_instant = 1;
								break;
							}
						}
				}
				for(i=0; i<max_exiled; i++){
					store_iid_on_kaho(player, card, exiled[i]);
				}
				if( can_play_interrupt ){
					return 99;
				}
				if( can_play_instant ){
					return 1;
				}
			}
		}
		if( ! IS_AI(player) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_PLAY_EXILED_CARD;
		if( ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Play an exiled card", 1, 1,
							"Show exiled cards", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_PLAY_EXILED_CARD ){
			// The card shouldn't be chosen until resolution, just X; but that would badly complicate the AI.
			if( IS_AI(player) ){
				int exiled[18];
				int max_exiled = retrieve_stored_iid_from_kaho(player, card, exiled);
				if( max_exiled ){
					int playable[18];
					int pc = 0;
					int i;
					for(i=0; i<max_exiled; i++){
						if( has_mana_for_activated_ability(player, card, MANACOST_X(get_cmc_by_internal_id(exiled[i]))) ){
							if( can_legally_play_iid(player, exiled[i]) ){
								playable[pc] = exiled[i];
								pc++;
							}
						}
					}
					for(i=0; i<max_exiled; i++){
						store_iid_on_kaho(player, card, exiled[i]);
					}
					test_definition_t test;
					default_test_definition(&test, TYPE_ANY);
					int selected = select_card_from_zone(player, player, playable, pc, 0, AI_MAX_VALUE, -1, &test);
					if( selected != -1 ){
						generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
						if( spell_fizzled != 1 ){
							int cmc = get_cmc_by_internal_id(playable[selected]);
							charge_mana_multi(player, MANACOST_X(cmc));
							if( spell_fizzled != 1 ){
								SET_BYTE1(instance->info_slot) = cmc;
								SET_BYTE2(instance->info_slot) = get_owner(player, card);
								add_state(player, card, STATE_TAPPED);
							}
						}
					}
				}
			}
			else{
				generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
				if( spell_fizzled != 1 ){
					charge_mana(player, COLOR_COLORLESS, -1);
					if( spell_fizzled != 1 ){
						SET_BYTE1(instance->info_slot) = x_value;
						SET_BYTE2(instance->info_slot) = get_owner(player, card);
						add_state(player, card, STATE_TAPPED);
					}
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int owner = BYTE2(instance->info_slot);
		int x = BYTE1(instance->info_slot);
		int exiled[18];
		int max_exiled = 0;
		if( in_play(instance->parent_controller, instance->parent_card) ){
			max_exiled = retrieve_stored_iid_from_kaho(instance->parent_controller, instance->parent_card, exiled);
		}
		else{
			max_exiled = retrieve_stored_iid_from_kaho(player, card, exiled);
		}
		if( max_exiled ){
			int playable[18];
			int pc = 0;
			int i;
			for(i=0; i<max_exiled; i++){
				if( get_cmc_by_internal_id(exiled[i]) == x ){
					if( can_legally_play_iid(player, exiled[i]) ){
						playable[pc] = exiled[i];
						pc++;
					}
				}
			}
			if( in_play(instance->parent_controller, instance->parent_card) ){
				for(i=0; i<max_exiled; i++){
					store_iid_on_kaho(instance->parent_controller, instance->parent_card, exiled[i]);
				}
			}
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select an exiled card to play.");
			int selected = select_card_from_zone(player, player, playable, pc, 0, AI_MAX_VALUE, -1, &test);
			if( selected != -1 ){
				play_card_in_exile_for_free(player, owner, playable[selected]);
			}
		}
	}

	return 0;
}

int card_kami_of_the_crescent_moon(int player, int card, event_t event)
{
  /* Kami of the Crescent Moon	|U|U
   * Legendary Creature - Spirit 1/3
   * At the beginning of each player's draw step, that player draws an additional card. */

	check_legend_rule(player, card, event);

	if (event == EVENT_DRAW_PHASE && ! is_humiliated(player, card) ){
		++event_result;
	}

	return 0;
}

int card_kami_of_the_tender_garden(int player, int card, event_t event){
/* CARD_ID_KAMI_OF_THE_TENDED_GARDEN	7673
Kami of the Tended Garden	|3|G
Creature - Spirit 4/4
At the beginning of your upkeep, sacrifice ~ unless you pay |G.
Soulshift 3 (When this creature dies, you may return target Spirit card with converted mana cost 3 or less from your graveyard to your hand.) */

	basic_upkeep(player, card, event, 0, 0, 0, 1, 0, 0);

	return soulshift(player, card, event, 3, 1);
}

int card_kashi_tribe_elite(int player, int card, event_t event){
/* CARD_ID_KASHI_TRIBE_ELITE	7674
Kashi-Tribe Elite	|1|G|G
Creature - Snake Warrior 2/3
Legendary Snakes you control have shroud. (They can't be the targets of spells or abilities.)
Whenever ~ deals combat damage to a creature, tap that creature and it doesn't untap during its controller's next untap step. */

	if( event == EVENT_ABILITIES && affected_card_controller == player &&
		! is_humiliated(player, card) &&
		has_subtype(affected_card_controller, affected_card, SUBTYPE_SNAKE) &&
		is_legendary(affected_card_controller, affected_card) )
	{
		event_result |= KEYWORD_SHROUD;
	}

	return freeze_when_damage(player, card, event);
}

int card_kitaki_wars_wage(int player, int card, event_t event){
/* CARD_ID_KATAKI_WARS_WAGE	7675
Kataki, War's Wage	|1|W
Legendary Creature - Spirit 2/1
All artifacts have "At the beginning of your upkeep, sacrifice this artifact unless you pay |1." */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int i;
		int p = current_turn;
		for (i=active_cards_count[p];i>=0;i--){
			if( is_what(p, i, TYPE_ARTIFACT) && in_play(p, i) && has_mana(p, COLOR_COLORLESS, 1) ){
				int choice = do_dialog(p, player, card, p, i, " Pay 1\n Sacrifice this artifact", 0);
				if( choice == 0 ){
					charge_mana_multi(p, 1, 0, 0, 0, 0, 0);
					if( spell_fizzled == 1 ){
						choice = 1;
					}
				}
				if( choice == 1 ){
					kill_card( p, i, KILL_SACRIFICE );
				}
			}
		}
	}
	return 0;
}

int card_kemuri_onna(int player, int card, event_t event){
/* CARD_ID_KEMURI_ONNA	7676
Kemuri-Onna	|4|B
Creature - Spirit 3/3
When ~ enters the battlefield, target player discards a card.
Whenever you cast a Spirit or Arcane spell, you may return ~ to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
			discard(instance->targets[0].player, 0, player);
		}
		instance->number_of_targets = 0;
	}

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		bounce_permanent(player, card);
	}

	return 0;
}

int card_kikus_shadow(int player, int card, event_t event){
/* CARD_ID_KIKUS_SHADOW	7677
Kiku's Shadow	|B|B
Sorcery
Target creature deals damage to itself equal to its power. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int pow = get_power(instance->targets[0].player, instance->targets[0].card);
			damage_creature(instance->targets[0].player, instance->targets[0].card, pow,
							instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_kiri_onna(int player, int card, event_t event){
/* CARD_ID_KIRI_ONNA	7678
Kiri-Onna	|4|U
Creature - Spirit 2/2
When ~ enters the battlefield, return target creature to its owner's hand.
Whenever you cast a Spirit or Arcane spell, you may return ~ to its owner's hand. */

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		bounce_permanent(player, card);
	}

	return card_man_o_war(player, card, event);
}

int card_kiyomaro_first_to_stand(int player, int card, event_t event){
	/* Kiyomaro, First to Stand	|3|W|W
	 * Legendary Creature - Spirit 100/100
	 * ~'s power and toughness are each equal to the number of cards in your hand.
	 * As long as you have four or more cards in hand, Kiyomaro has vigilance.
	 * Whenever Kiyomaro deals damage, if you have seven or more cards in hand, you gain 7 life. */

	check_legend_rule(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card) && hand_count[player] >= 4 ){
		vigilance(player, card, event);
		if( hand_count[player] >= 7 ){
			int result = damage_dealt_by_me_arbitrary(player, card, event, 0, player, card);
			if( result ){
				gain_life(player, 7 * result);
			}
		}
	}

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && !is_humiliated(player, card) && player != -1){
		event_result += hand_count[player];
	}

	return 0;
}

int card_kuon_essence(int player, int card, event_t event){

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, 2);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		new_sacrifice(player, card, current_turn, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	return 0;
}

int card_kuon_ogre_ascendant(int player, int card, event_t event){

	/* Kuon, Ogre Ascendant	|B|B|B [Kuon's Essence]
	 * Legendary Creature - Ogre Monk 2/4
	 * At the beginning of the end step, if three or more creatures died this turn, flip ~. */

	double_faced_card(player, card, event);

	check_legend_rule(player, card, event);

	if( creatures_dead_this_turn >= 3 && eot_trigger(player, card, event) ){
		true_transform(player, card);
	}

	return 0;
}

int card_kuros_taken(int player, int card, event_t event){

	bushido(player, card, event, 1);

	return regeneration(player, card, event, MANACOST_XB(1, 1));
}

int card_maga_traitor_to_mortals(int player, int card, event_t event){

  /* Maga, Traitor to Mortals	|X|B|B|B
   * Legendary Creature - Human Wizard 0/0
   * ~ enters the battlefield with X +1/+1 counters on it.
   * When ~ enters the battlefield, target player loses life equal to the number of +1/+1 counters on it. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
			lose_life(instance->targets[0].player, count_counters(player, card, COUNTER_P1_P1));
		}
		instance->number_of_targets = 0;
	}

	return card_ivy_elemental(player, card, event);
}

int card_manriki_gusari(int player, int card, event_t event){

	/* Manriki-Gusari	|2
	 * Artifact - Equipment
	 * Equipped creature gets +1/+2 and has "|T: Destroy target Equipment."
	 * Equip |1 */

	if( is_equipping(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			return vanilla_equipment(player, card, event, 2, 1, 2, 0, 0);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.required_subtype = SUBTYPE_EQUIPMENT;

	card_instance_t *instance = get_card_instance(player, card);
	int p = instance->damage_target_player;
	int c = instance->damage_target_card;

	enum{
		CHOICE_EQUIP = 1,
		CHOICE_KILL_EQUIP
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( is_equipping(player, card) ){
			if( granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
				return 1;
			}
		}
		return basic_equipment(player, card, event, 1);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						can_activate_basic_equipment(player, card, event, 1),
						0
		};
		if( is_equipping(player, card) ){
			if( granted_generic_activated_ability(player, card, p, c, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0,
													&td, NULL) )
			{
				abils[2] = 1;
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Equip", abils[1], 5,
							"Destroy target equipment", abils[2], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 1);
		}
		if( choice == CHOICE_KILL_EQUIP ){
			granted_generic_activated_ability(player, card, p, c, event, 0, MANACOST0, 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				if( pick_target(&td, "TARGET_EQUIPMENT") ){
					add_state(p, c, STATE_TAPPED);
				}
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 1);
		}
		if( instance->info_slot == CHOICE_KILL_EQUIP && valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_masumaro_first_to_live(int player, int card, event_t event){
	/* Masumaro, First to Live	|3|G|G|G
	 * Legendary Creature - Spirit 100/100
	 * ~'s power and toughness are each equal to twice the number of cards in your hand. */

	check_legend_rule(player, card, event);

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && !is_humiliated(player, card) && player != -1){
		event_result += 2 * hand_count[player];
	}

	return 0;
}


int card_meishin_the_mind_cage(int player, int card, event_t event){

	check_legend_rule(player, card, event);

	boost_creature_type(player, card, event, -1, -hand_count[player], 0, 0, BCT_INCLUDE_SELF);

	return global_enchantment(player, card, event);
}

int card_michiko_konda_truth_seeker(int player, int card, event_t event){
/*
Michiko Konda, Truth Seeker |3|W
Legendary Creature — Human Advisor 2/2
Whenever a source an opponent controls deals damage to you, that player sacrifices a permanent.
*/

	check_legend_rule(player, card, event);

	check_damage_test(1-player, -1, event, DDBM_MUST_DAMAGE_TRIGGERING_PLAYER, player, card, TYPE_ANY, NULL);

	int result = resolve_damage_trigger(player, card, event, DDBM_MUST_DAMAGE_TRIGGERING_PLAYER, player, card);
	if( result ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a permanent to sacrifice.");
		test.qty = result;
		new_sacrifice(player, card, 1-player, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	return 0;
}

int card_mikokoro_center_of_the_sea(int player, int card, event_t event){
	/* Mikokoro, Center of the Sea	""
	 * Legendary Land
	 * |T: Add |C to your mana pool.
	 * |2, |T: Each player draws a card. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_DRAW
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL) )
		{
			choice = CHOICE_DRAW;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_DRAW && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Everybody draw a card", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_DRAW ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DRAW ){
			draw_cards(player, 1);
			draw_cards(1-player, 1);
		}
	}

	return 0;
}

int card_miren_the_moaning_well(int player, int card, event_t event){
	/* Miren, the Moaning Well	""
	 * Legendary Land
	 * |T: Add |C to your mana pool.
	 * |3, |T, Sacrifice a creature: You gain life equal to the sacrificed creature's toughness. */

	card_instance_t *instance = get_card_instance( player, card );

	check_legend_rule(player, card, event);

#define CAN_SAC	(!is_tapped(player, card) && !is_animated_and_sick(player, card) && can_use_activated_abilities(player, card)	\
				 && has_mana_for_activated_ability(player, card, MANACOST_X(4)) && can_sacrifice_as_cost(player, 1, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0))

	if (event == EVENT_CAN_ACTIVATE){
		int rval = mana_producer(player, card, event);
		if (rval){
			return rval;
		}
		return CAN_SAC;
	}

	if( event == EVENT_ACTIVATE ){
		int choice = instance->info_slot = 0;
		if (!paying_mana() && CAN_SAC){
			choice = do_dialog(player, player, card, -1, -1, " Add 1\n Sac and gain life\n Cancel", 1);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		else if( choice == 1 ){
				add_state(player, card, STATE_TAPPED);
				if (charge_mana_for_activated_ability(player, card, MANACOST_X(3)) ){
					test_definition_t test;
					new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
					int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
					if (!sac){
						untap_card_no_event(player, card);
						cancel = 1;
						return 0;
					}
					tap_card(player, card);
					instance->info_slot = get_toughness(BYTE2(sac), BYTE3(sac));
					kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				}
				else{
					untap_card_no_event(player, card);
					spell_fizzled = 1;
				}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 ){
			gain_life(player, instance->info_slot);
		}
	}

	return mana_producer(player, card, event);
#undef CAN_SAC
}

int card_molting_skin(int player, int card, event_t event){
/* CARD_ID_MOLTING_SKIN	7697
Molting Skin	|2|G
Enchantment
Return ~ to its owner's hand: Regenerate target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( land_can_be_played & LCBP_REGENERATION ){
			if( generic_activated_ability(player, card, event, GAA_BOUNCE_ME | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
				return 99;
			}
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_BOUNCE_ME | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target( instance->targets[0].player, instance->targets[0].card );
		}
	}

	return 0;
}

ARCANE(card_murmurs_from_beyond){
/*
Murmurs from Beyond |2|U
Instant � Arcane
Reveal the top three cards of your library. An opponent chooses one of them. Put that card into your graveyard and the rest into your hand.
*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn == 1-player ? 48 : 24);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = MIN(3, count_deck(player));
		if( amount ){
			show_deck(player, deck_ptr[player], amount, "Murmurs from Beyond revealed...", 0, 0x7375B0 );

			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a card.");
			test.create_minideck = amount;
			test.no_shuffle = 1;
			new_global_tutor(1-player, player, TUTOR_FROM_DECK, TUTOR_GRAVE, 1, AI_MIN_VALUE, &test);
			amount--;
			while( amount ){
					add_card_to_hand(player, deck_ptr[player][0]);
					remove_card_from_deck(player, 0);
					amount--;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_neverending_torment(int player, int card, event_t event){

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if( hand_count[player] ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ANY, "Select a card to exile.");
				test.qty = hand_count[player];
				new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_RFG, 0, AI_MAX_VALUE, &test);
			}
			if( ! check_special_flags3(player, card, SF3_EPIC) ){
				epic_legacy(player, card, event);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_nightsoil_kami(int player, int card, event_t event){
	return soulshift(player, card, event, 5, 1);
}

int card_nikko_onna(int player, int card, event_t event){

	if( arcane_spirit_spell_trigger(player, card, event, 1+player) ){
		bounce_permanent(player, card);
	}

	return card_monk_realist(player, card, event);
}

int card_o_naginata(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return vanilla_equipment(player, card, event, 2, 3, 0, KEYWORD_TRAMPLE, 0);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.power_requirement = 3 | TARGET_PT_GREATER_OR_EQUAL;

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_EQUIPMENT, MANACOST_X(2), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_EQUIPMENT | GAA_LITERAL_PROMPT, MANACOST_X(2), 0,
										&td, "Select target creature you control with POW 3 or higher.");
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t* instance = get_card_instance(player, card);
			equip_target_creature(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_oboro_palace_in_the_clouds(int player, int card, event_t event){
	/* Oboro, Palace in the Clouds	""
	 * Legendary Land
	 * |T: Add |U to your mana pool.
	 * |1: Return ~ to its owner's hand. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_BOUNCE
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(2), 0, NULL, NULL) )
		{
			choice = CHOICE_BOUNCE;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Bounce Oboro", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_BOUNCE ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
			remove_state(player, card, STATE_TAPPED);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_BOUNCE ){
			bounce_permanent(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_oni_of_the_wild_places(int player, int card, event_t event){

	haste(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		td.allow_cancel = 0;
		td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		td.illegal_abilities = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) ){
			char msg[100];
			scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select a %s creature you control.", COLOR_RED));
			if( new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_oppressive_will(int player, int card, event_t event){
	/* Oppressive Will	|2|U
	 * Instant
	 * Counter target spell unless its controller pays |1 for each card in your hand. */

	if(event == EVENT_RESOLVE_SPELL ){
		counterspell_resolve_unless_pay_x(player, card, NULL, 0, hand_count[player]);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
}

int card_overwhelming_intellect(int player, int card, event_t event){
/*
Overwhelming Intellect |4|U|U
Instant
Counter target creature spell. Draw cards equal to that spell's converted mana cost.
*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_CREATURE);

	if(event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, &td, 0) ){
			card_instance_t *instance = get_card_instance( player, card );
			int amount = get_cmc(instance->targets[0].player, instance->targets[0].card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			draw_cards(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
}

int card_pains_reward(int player, int card, event_t event){

	if(event == EVENT_RESOLVE_SPELL ){
			int life_bid[2] = {0, 0};
			if( player != AI ){
				life_bid[player] = choose_a_number(player, "Bid how much life?", life[player]);
			}
			else{
				life_bid[player] = life[1-player];
				if( check_battlefield_for_id(player, CARD_ID_PLATINUM_EMPERION) ){
					life_bid[player]++;
				}
				else{
					while( life_bid[player] > (life[player]-6) ){
							life_bid[player]--;
					}
				}
			}
			if( player != HUMAN ){
				char buffer[100];
				snprintf(buffer, 100, "Opponent bids %d life.", life_bid[player] );
				do_dialog(HUMAN, player, card, -1, -1, buffer, 0);
			}
			int bid_turn = 1-player;
			while( 1 ){
					if( bid_turn != AI ){
						life_bid[bid_turn] = choose_a_number(bid_turn, "Bid how much life?", life_bid[bid_turn]);
					}
					else{
						if( life_bid[1-bid_turn] >= life[bid_turn] ){
							life_bid[bid_turn] = 0;
						}
						else{
							if( check_battlefield_for_id(bid_turn, CARD_ID_PLATINUM_EMPERION) ){
								life_bid[bid_turn] = life[1-bid_turn]+1;
							}
							else{
								if( life_bid[1-bid_turn]+1 <= (life[bid_turn]-6) ){
									life_bid[bid_turn] = life_bid[1-bid_turn]++;
								}
								else{
									life_bid[bid_turn] = 0;
								}
							}
						}
					}
					if( bid_turn != HUMAN ){
						char buffer[100];
						if( life_bid[bid_turn] > 0 ){
							snprintf(buffer, 100, "Opponent bids %d life.", life_bid[bid_turn] );
						}
						else{
							snprintf(buffer, 100, "Opponent passes.");
						}
						do_dialog(HUMAN, player, card, -1, -1, buffer, 0);
					}
					if( life_bid[bid_turn] > 0 && life_bid[bid_turn] > life_bid[1-bid_turn]){
						bid_turn = 1-bid_turn;
					}
					else{
						break;
					}
			}
			int winner = player;
			if( life_bid[1-player] > life_bid[player] ){
				winner = 1-player;
			}
			lose_life(winner, life_bid[winner]);
			draw_cards(winner, 4);
			kill_card(player, card, KILL_DESTROY);

	}

	return basic_spell(player, card, event);
}

ARCANE(card_path_of_angers_flame){
	// Creatures you control get +2/+0 until end of turn.

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 2, 0, 0, 0);
	}

	return basic_spell(player, card, event);
}

int card_pithing_needle(int player, int card, event_t event){

	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);

		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a card.");
		int csvid = select_permanent_to_disable(player, card, &td, "Select a card in play", &test);
		get_card_instance(player, card)->info_slot = csvid;
		activated_abilities_disabler_enters_bf(player, card, ACT_DISABLE_NONMANA_ACTIVATED_ABILITIES);
	}

	return activated_abilities_disabler(player, card, event, ACT_DISABLE_NONMANA_ACTIVATED_ABILITIES);
}

// presence of the wise --> gerrard's wisdom

int card_promise_of_the_bunrei(int player, int card, event_t event){
	/* Promise of Bunrei	|2|W
	 * Enchantment
	 * When a creature you control dies, sacrifice ~. If you do, put four 1/1 colorless Spirit creature tokens onto the battlefield. */

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
	}

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		generate_tokens_by_id(player, card, CARD_ID_SPIRIT, 4);
		kill_card(player, card, KILL_SACRIFICE);
	}

	return global_enchantment(player, card, event);
}

int card_promised_kannushi(int player, int card, event_t event){
	return soulshift(player, card, event, 7, 1);
}

int card_raving_oni_slave(int player, int card, event_t event){

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( ! check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_DEMON) ){
			lose_life(player, 3);
		}
	}

	if( leaves_play(player, card, event) && ! check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_DEMON) ){
		lose_life(player, 3);
	}

	return 0;
}

int card_razorjaw_oni(int player, int card, event_t event){

	if( event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card) ){
		if( get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_BLACK) ){
			event_result = 1;
		}
	}

	return 0;
}

int card_reki_the_history_of_kamigawa(int player, int card, event_t event){

	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_LEGEND;
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

ARCANE(card_rending_vines){
	// Destroy target artifact or enchantment if its converted mana cost is less than or equal to the number of cards in your hand.
	// Draw a card.

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
		if( spell_fizzled != 1 && IS_AI(player) && get_cmc(instance->targets[0].player, instance->targets[0].card) > hand_count[player] ){
			ai_modifier -= 256;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if (get_cmc(instance->targets[0].player, instance->targets[0].card) <= hand_count[player]){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
			draw_cards(player, 1);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
}

int card_reverence(int player, int card, event_t event)
{
	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_ATTACK_LEGALITY && affected_card_controller != player &&
			get_power(affected_card_controller, affected_card) <= 2 )
		{
			event_result = 1;
		}
	}

	return global_enchantment(player, card, event);
}

int card_rune_tails_essence(int player, int card, event_t event){

	check_legend_rule(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( damage->damage_target_card != -1 && damage->damage_target_player == player &&
					damage->info_slot > 0 && ! damage_is_to_planeswalker(damage) )
				{
					prevent_damage(affected_card_controller, affected_card, -1);
				}
			}
		}
	}

	return 0;
}

int card_rune_tail_kitsune_ascendant(int player, int card, event_t event){

	check_legend_rule(player, card, event);

	double_faced_card(player, card, event);

	if (life[player] > 29 && in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		if (instance->targets[13].player == instance->targets[13].card){	// Don't transform back when we get back here during EVENT_CHANGE_TYPE
			true_transform(player, card);
		}
	}
	return 0;
}

static int effect_sakashima_the_impostor(int player, int card, event_t event)
{
  if (effect_follows_control_of_attachment(player, card, event))
	return 0;

  card_instance_t* instance = get_card_instance(player, card);
  int p = instance->damage_target_player, c = instance->damage_target_card;

  check_legend_rule(p, c, event);

  if (event == EVENT_RESOLVE_ACTIVATION)
	get_card_instance(instance->parent_controller, instance->parent_card)->targets[1].player = 66;

  if (instance->targets[1].player == 66 && eot_trigger(player, card, event))
	bounce_permanent(p, c);

  if (event == EVENT_CAN_SKIP_TURN)	// Just in case the trigger gets countered
	instance->targets[1].player = 0;

  return generic_activated_ability(player, card, event, 0, MANACOST_XU(2,2), 0, NULL, NULL);
}

int card_sakashima_the_impostor(int player, int card, event_t event)
{
	/* Sakashima the Impostor	|2|U|U	0x20064a7
	 * Legendary Creature - Human Rogue 3/1
	 * You may have ~ enter the battlefield as a copy of any creature on the battlefield, except its name is ~, it's legendary in addition to its other types, and it has "|2|U|U: Return this creature to its owner's hand at the beginning of the next end step." */
  check_legend_rule(player, card, event);

  if (enters_the_battlefield_as_copy_of_any_creature(player, card, event))
	{
	  add_a_subtype(player, card, SUBTYPE_LEGEND);
	  set_legacy_image(player, CARD_ID_SAKASHIMA_THE_IMPOSTOR, create_targetted_legacy_activate(player, card, effect_sakashima_the_impostor, player, card));
	}

  return 0;
}

int card_sakura_tribe_scout(int player, int card, event_t event)
{
	if( event == EVENT_ACTIVATE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.zone = TARGET_ZONE_HAND;
		if( check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test) ){
			int mod = -48;
			if( current_turn == 1-player ){
				if( check_state(player, card, STATE_BLOCKING) ){
					card_instance_t *instance = get_card_instance(player, card);
					if( get_power(1-player, instance->blocking) > get_toughness(player, card) ){
						mod = 48;
					}
				}
				if( current_phase >= PHASE_MAIN2 ){
					mod = 48;
				}
			}
			else{
				mod = current_phase <= PHASE_DECLARE_BLOCKERS ? 12 : 24;
			}
			ai_modifier += mod;
		}
		else{
			ai_modifier -= 256;
		}
	}

	// |T: You may put a land card from your hand onto the battlefield.
	if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_LAND, "Select a land to put into play.");
	  new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(0), 0, NULL, NULL);
}

int card_sasayas_essence(int player, int card, event_t event){
/* CARD_ID_SASAYAS_ESSENCE	7734
Sasaya's Essence	|1|G|G
Legendary Enchantment
Whenever a land you control is tapped for mana, for each other land you control with the same name, add one mana to your mana pool of any type that land produced.*/

	check_legend_rule(player, card, event);

	if ((event == EVENT_COUNT_MANA || event == EVENT_TAP_CARD)
		&& affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_LAND)){
		// See comments in card_mana_flare().

		if (!in_play(player, card)){
			return 0;
		}

		// Avoid counting lands if we can
		if (event == EVENT_COUNT_MANA){
			if (is_tapped(affected_card_controller, affected_card) || is_animated_and_sick(affected_card_controller, affected_card)
				|| !can_produce_mana(affected_card_controller, affected_card) ){
				return 0;
			}
		} else {	// event == EVENT_TAP_CARD
			if (tapped_for_mana_color < 0){
				return 0;
			}
		}

		int id = get_id(affected_card_controller, affected_card);
		int c, other_lands = 0;
		for (c = 0; c < active_cards_count[player]; ++c){
				if( in_play(player, c) && get_id(player, c) == id && c != affected_card){
					++other_lands;
				}
		}

		if (other_lands <= 0){
			return 0;
		}

		if (event == EVENT_COUNT_MANA){
			// A check for EA_MANA_SOURCE is conspicuous by its absence in the exe version of Mana Flare.
			int num_colors = 0;
			color_t col;
			card_instance_t* aff_instance = get_card_instance(affected_card_controller, affected_card);
			for (col = COLOR_COLORLESS; col <= COLOR_ARTIFACT; ++col){
				if (aff_instance->card_color & (1 << col)){
					++num_colors;
				}
			}

			if (num_colors > 0){
				/* This isn't correct - see the comment in card_axebane_guardian() in return_to_ravnica.c.  This card seems much more likely to overflow the
				 * array for declare_mana_available_hex() if it's split up the way Axebane Guardian is, so just assume it has to produce all the same color
				 * for each land. */
				declare_mana_available_hex(affected_card_controller, aff_instance->card_color, other_lands);
			} else {
				declare_mana_available(affected_card_controller, single_color_test_bit_to_color(aff_instance->card_color), other_lands);
			}
		} else {	// event == EVENT_TAP_CARD
			produce_mana_of_any_type_tapped_for(player, card, other_lands);
		}
	}

	return 0;
}

int card_sasaya_orochi_ascendant(int player, int card, event_t event){
/*
Sasaya, Orochi Ascendant |1|G|G
Legendary Creature — Snake Monk 2/3
Reveal your hand: If you have seven or more land cards in your hand, flip Sasaya, Orochi Ascendant.
*/
	check_legend_rule(player, card, event);

	double_faced_card(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			test_definition_t test;
			default_test_definition(&test, TYPE_LAND);
			int amount = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test);
			ai_modifier += (amount >= 7 ? 256 : -256);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		int amount = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test);
		if( amount >= 7 ){
			true_transform(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_seed_the_land(int player, int card, event_t event){
	/* Seed the Land	|2|G|G
	 * Enchantment
	 * Whenever a land enters the battlefield, its controller puts a 1/1 |Sgreen Snake creature token onto the battlefield. */

	etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_LAND, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SNAKE, &token);
		token.t_player = trigger_cause_controller;
		generate_token(&token);
	}

	return global_enchantment(player, card, event);
}

int card_seek_the_horizon(int player, int card, event_t event){
/* CARD_ID_SEEK_THE_HORIZON	7738
Seek the Horizon	|3|G
Sorcery
Search your library for up to three basic land cards, reveal them, and put them into your hand. Then shuffle your library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_deck(player) >= 20 ? 48 : -48);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a basic land card.");
		test.subtype = SUBTYPE_BASIC;
		test.qty = 3;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &test);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_sekki_seasons_guide(int player, int card, event_t event){
/* Sekki, Seasons' Guide	|5|G|G|G
 * Legendary Creature - Spirit 0/0
 * ~ enters the battlefield with eight +1/+1 counters on it.
 * If damage would be dealt to Sekki, prevent that damage, remove that many +1/+1 counters from Sekki, and put that many
 *		1/1 colorless Spirit creature tokens onto the battlefield.
 * Sacrifice eight Spirits: Return Sekki from your graveyard to the battlefield. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 8);

	if(event == EVENT_GRAVEYARD_ABILITY){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_SPIRIT;
		test.qty = 8;
		if( new_can_sacrifice_as_cost(player, card, &test) ){
			return GA_RETURN_TO_PLAY;
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a Spirit to sacrifice.");
		test.subtype = SUBTYPE_SPIRIT;
		int sacs[8];
		int sc = 0;
		while( sc < 8 ){
				int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
				if (!sac){
					cancel = 1;
					break;
				}
				sacs[sc] = sac;
				sc++;
		}
		int i;
		for(i=0; i<sc; i++){
			state_untargettable(BYTE2(sacs[i]), BYTE3(sacs[i]), 0);
			if( sc == 8 ){
				kill_card(BYTE2(sacs[i]), BYTE3(sacs[i]), KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card) ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == card && damage->damage_target_player == player &&
				damage->info_slot > 0 && count_1_1_counters(player, card) > 0 ){
				int amount = MIN(damage->info_slot, count_1_1_counters(player, card));
				prevent_damage(affected_card_controller, affected_card, amount);
				remove_1_1_counters(player, card, amount);
				generate_tokens_by_id(player, card, CARD_ID_SPIRIT, amount);
			}
		}
	}
	return 0;
}

int card_shinen_of_lifes_roar(int player, int card, event_t event){
	/* Shinen of Life's Roar	|1|G
	 * Creature - Spirit 1/2
	 * All creatures able to block ~ do so.
	 * Channel - |2|G|G, Discard ~: All creatures able to block target creature this turn do so. */

	if( ! IS_ACTIVATING_FROM_HAND(event) ){
		return everybody_must_block_me(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND  ){
		if( has_mana_multi(player, MANACOST_XG(2, 2)) ){
			return can_target(&td);
		}
	}

	if( event == EVENT_ACTIVATE_FROM_HAND ){
		charge_mana_multi(player, MANACOST_XG(2, 2));
		if( spell_fizzled != 1 && pick_target(&td, "TARGET_CREATURE") ){
			discard_card(player, card);
			return 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 && valid_target(&td) ){
		pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_LURE);
	}

	return 0;
}

int card_sink_into_takenuma(int player, int card, event_t event){
/* CARD_ID_SINK_INTO_TAKENUMA	7747
Sink into Takenuma	|3|B
Sorcery - Arcane
Sweep - Return any number of |H1Swamps you control to their owner's hand. Target player discards a card for each |H2Swamp returned this way. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int mod = MIN(count_subtype(player, TYPE_LAND, SUBTYPE_SWAMP), hand_count[1-player]);
		ai_modifier += (mod * 8);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 1) ){
			int amount = sweep(player, card, SUBTYPE_SWAMP);
			new_multidiscard(instance->targets[0].player, amount, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_skull_collector(int player, int card, event_t event){
/* CARD_ID_SKULL_COLLECTOR	7748
Skull Collector	|1|B|B
Creature - Ogre Warrior 3/3
At the beginning of your upkeep, return a |Sblack creature you control to its owner's hand.
|1|B: Regenerate ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		td.allow_cancel = 0;
		td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		td.illegal_abilities = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) ){
			char msg[100];
			scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select a %s creature you control.", COLOR_BLACK));
			if( new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
		instance->number_of_targets = 0;
	}

	return regeneration(player, card, event, MANACOST_XB(1, 1));
}

int card_skyfire_kirin(int player, int card, event_t event){
/* CARD_ID_SKYFIRE_KIRIN	7749
Skyfire Kirin	|2|R|R
Legendary Creature - Kirin Spirit 3/3
Flying
Whenever you cast a Spirit or Arcane spell, you may gain control of target creature with that spell's converted mana cost until end of turn. */

	check_legend_rule(player, card, event);

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		int cmc = get_cmc(trigger_cause_controller, trigger_cause);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.extra = cmc;
		td.special = TARGET_SPECIAL_CMC_EQUAL;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) ){
			char msg[100];
			scnprintf(msg, 100, "Select target creature with CMC %d.", cmc);
			if( new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
				gain_control_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
			}
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_soramaro_first_to_dream(int player, int card, event_t event){
	/* Soramaro, First to Dream	|4|U|U
	 * Legendary Creature - Spirit 100/100
	 * Flying
	 * ~'s power and toughness are each equal to the number of cards in your hand.
	 * |4, Return a land you control to its owner's hand: Draw a card. */

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && !is_humiliated(player, card) && player != -1){
		event_result += hand_count[player];
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND );
	td.preferred_controller = player;
	td.allowed_controller = player;
	td.illegal_abilities = 0;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(4), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if( current_turn == 1-player && current_phase >= PHASE_AFTER_BLOCKING ){
			ai_modifier += (count_subtype(player, TYPE_LAND, -1) * 8);
		}
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(4), 0,
									&td, "Select a land you control.");
		if( spell_fizzled != 1 ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			instance->number_of_targets = 0;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return 0;
}

ARCANE_WITH_SPLICE(card_spiritual_visit, MANACOST_W(1)){
	/* Spiritual Visit	|W
	 * Instant - Arcane
	 * Put a 1/1 colorless Spirit creature token onto the battlefield.
	 * Splice onto Arcane |W */

	if( event == EVENT_RESOLVE_SPELL ){
		generate_token_by_id(player, card, CARD_ID_SPIRIT);
	}

	return basic_spell(player, card, event);
}

// stampeding serow --> stampeding wildebeest

ARCANE(card_sunder_from_within){
/* CARD_ID_SUNDER_FROM_WITHIN	7757
Sunder from Within	|2|R|R
Sorcery - Arcane
Destroy target artifact or land. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_LAND );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target artifact or land.", 1, NULL);
}

int card_thoughts_of_ruin(int player, int card, event_t event){
/* CARD_ID_THOUGHTS_OF_RUIN	7758
Thoughts of Ruin	|2|R|R
Sorcery
Each player sacrifices a land for each card in your hand. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		test.qty = hand_count[player];

		APNAP(p,{ new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_urami(int player, int card, event_t event){
/* CARD_ID_URAMI	17504
Urami	""(token)
Legendary Creature - Demon Spirit 5/5
Flying */

	check_legend_rule(player, card, event);
	return 0;
}


int card_tomb_of_urami(int player, int card, event_t event){
/* Tomb of Urami	""
 * Legendary Land
 * |T: Add |B to your mana pool. ~ deals 1 damage to you if you don't control an Ogre.
 * |2|B|B, |T, Sacrifice all lands you control: Put a legendary 5/5 |Sblack Demon Spirit creature token with flying named Urami
 *		onto the battlefield. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_URAMI
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_XB(2, 3), 0,
															NULL, NULL, TYPE_LAND, NULL, 0) )
		{
			choice = CHOICE_URAMI;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_URAMI && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Summon Urami", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			mana_producer(player, card, event);
			if( cancel != 1 ){
				if( ! check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_OGRE) ){
					damage_player(player, 1, player, card);
				}
			}
		}

		if( choice == CHOICE_URAMI ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, 0, MANACOST_XB(2, 2), 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				test_definition_t test;
				default_test_definition(&test, TYPE_LAND);
				new_manipulate_all(player, card, player, &test, KILL_SACRIFICE);
			}
			else{
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_URAMI ){
			generate_token_by_id(player, card, CARD_ID_URAMI);
		}
	}

	return 0;
}

int card_trusted_advisor(int player, int card, event_t event){
/* CARD_ID_TRUSTED_ADVISOR	7761
Trusted Advisor	|U
Creature - Human Advisor 1/2
Your maximum hand size is increased by two.
At the beginning of your upkeep, return a |Sblue creature you control to its owner's hand. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		td.allow_cancel = 0;
		td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		td.illegal_abilities = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) ){
			char msg[100];
			scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select a %s creature you control.", COLOR_BLUE));
			if( new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
		instance->number_of_targets = 0;
	}

	if( current_turn == player && event == EVENT_MAX_HAND_SIZE && ! is_humiliated(player, card) ){
		event_result+=2;
	}

	return 0;
}

int card_twincast(int player, int card, event_t event)
{
/* CARD_ID_TWINCAST	1271
Twincast	|U|U
Instant
Copy target instant or sorcery spell. You may choose new targets for the copy. */

  int rval = twincast(player, card, event, NULL, NULL);

  if (event == EVENT_RESOLVE_SPELL)
	kill_card(player, card, KILL_DESTROY);

  return rval;
}

int card_undying_flame(int player, int card, event_t event){
	/* Undying Flames	|4|R|R	0x20064f7
	 * Sorcery
	 * Exile cards from the top of your library until you exile a nonland card. ~ deals damage to any target equal to that card's converted mana cost.
	 * Epic */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int *deck = deck_ptr[player];
			int dmg = 0;
			int count = 0;
			int found = 0;
			while( deck[count] != -1 ){
					if( ! is_what(-1, deck[0], TYPE_LAND) ){
						found = 1;
						break;
					}
					count++;
			}
			if( found ){
				show_deck(player, deck, count+1, "Cards revealed by Undying Flame", 0, 0x7375B0 );
				dmg = get_cmc_by_id( cards_data[deck[count]].id);
				while( count > -1 ){
						rfg_card_in_deck(player, count);
						count--;
				}
			}
			if( dmg ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, dmg, player, card);
			}
			if( ! check_special_flags3(player, card, SF3_EPIC) ){
				epic_legacy(player, card, event);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

static int wine_of_blood_and_iron_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( trigger_condition == TRIGGER_EOT && affect_me(player, card) && reason_for_trigger_controller == player ){
			if( instance->targets[1].player > 0 ){
				instance->targets[1].player--;
			}
			else{
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_MANDATORY;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						if( ! check_state(p, c, STATE_OUBLIETTED) && instance->targets[0].player == instance->damage_target_player ){
							kill_card(p, c, KILL_SACRIFICE);
						}
						else{
							kill_card(player, card, KILL_EXILE);
						}
				}
			}
		}
	}

	return 0;
}

int card_wine_of_blood_and_iron(int player, int card, event_t event){
/*
Wine of Blood and Iron |3
Artifact, 3 (3)
{4}: Target creature gets +X/+0 until end of turn, where X is its power. Sacrifice Wine of Blood and Iron at the beginning of
	the next end step.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = get_power(instance->targets[0].player, instance->targets[0].card);
			pump_ability_until_eot(player, instance->parent_card,
										instance->targets[0].player, instance->targets[0].card, amount, 0, 0, 0);
			int legacy = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &wine_of_blood_and_iron_legacy,
														instance->parent_controller, instance->parent_card);
			if( current_phase == PHASE_DISCARD ){
				get_card_instance(instance->parent_controller, legacy)->targets[1].player = 1;
			}
			get_card_instance(instance->parent_controller, legacy)->number_of_targets = 0;
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(4), 0, &td, "TARGET_CREATURE");
}

int card_yuki_onna(int player, int card, event_t event){
/* CARD_ID_YUKI_ONNA	7764
Yuki-Onna	|3|R
Creature - Spirit 3/1
When ~ enters the battlefield, destroy target artifact.
Whenever you cast a Spirit or Arcane spell, you may return ~ to its owner's hand. */

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		bounce_permanent(player, card);
	}

	return card_uktabi_orangutan(player, card, event);
}

