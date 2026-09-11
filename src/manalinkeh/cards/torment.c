#include "manalink.h"

// Functions
static int do_flashback_with_life(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white, int lif){
	if( event == EVENT_GRAVEYARD_ABILITY && can_pay_life(player, lif) ){
		return do_flashback(player, card, event, colorless, black, blue, green, red, white);
	}
	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		card_instance_t *instance = get_card_instance(player, card);
		if( pay_flashback(player, instance->internal_card_id, event, colorless, black, blue, green, red, white) ){
			lose_life(player, lif);
			return GAPAID_EXILE;
		}
	}
	return 0;
}

static int grave_recycler(int player, int card, event_t event, int crds){

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int sac = 1;
		if( count_graveyard(player) >= crds){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card to put on bottom of your deck.");
			int amount = 0;
			while( amount < crds ){
					int no_canc = 0;
					if( amount > 0 ){
						no_canc = 1;
					}
					if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_BOTTOM_OF_DECK, no_canc, AI_MAX_VALUE, &this_test) != -1 ){
						amount++;
					}
					else{
						break;
					}
			}
			if( amount == crds ){
				sac = 0;
			}
			if( sac ){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	return 0;
}

static int grave_eater(int player, int card, event_t event, int crds){

	// Whenever ~ attacks or blocks, any player may exile [crds] cards from his or her graveyard. If a player does, ~ assigns no combat damage this turn.
	if (declare_attackers_trigger(player, card, event, 0, player, card) ||
		(blocking(player, card, event) && !is_humiliated(player, card))
	   ){
		if( count_graveyard(1-player) >= crds ){
			char buffer[100];
			scnprintf(buffer, 100, " Remove %d cards from your grave\n Decline", crds);
			int choice = do_dialog(1-player, player, card, -1, -1, buffer, 0);
			if( choice == 0 ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");
				int amount = 0;
				while( 1 ){
						if( new_global_tutor(1-player, 1-player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test) != -1 ){
							amount++;
							if( amount == crds ){
								negate_combat_damage_this_turn(player, card, player, card, 0);
								break;
							}
						}
						else{
							break;
						}
				}
			}
		}
	}
	return 0;
}

static int dream(int player, int card, event_t event, int at_random){
	card_instance_t *instance= get_card_instance(player, card);
	if( event == EVENT_CAN_CAST ){
		return 1;
	}
	else if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! affect_me(player, card) ){ return 0; }
		int number = hand_count[player] + 1;
		while( number > hand_count[player] ){
			number = choose_a_number(player, "Discard how many cards?", hand_count[player]);
		}
		multidiscard(player, number, at_random);
		instance->info_slot = number;
	}
	return 0;
}

static void threshold_boost(int player, int card, event_t event, int pow, int tou, int key, int s_key){
	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES) && affect_me(player, card) ){
		if( in_play(player, card) && ! is_humiliated(player, card) && has_threshold(player) ){
			switch( event ){
					case EVENT_POWER:
						event_result += pow;
						break;
					case EVENT_TOUGHNESS:
						event_result += tou;
						break;
					case EVENT_ABILITIES:
					{
						event_result |= key;
						if( s_key ){
							special_abilities(player, card, event, s_key, player, card );
						}
						break;
					}
					default:
						break;
			}
		}
	}
}

static int possessed(int player, int card, event_t event, int clr){

	if( event == EVENT_SET_COLOR && affect_me(player, card) && ! is_humiliated(player, card) && has_threshold(player) ){
		event_result = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
	}

	threshold_boost(player, card, event, 1, 1, 0, 0);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_color = get_sleighted_color_test(player, card, 1<<clr);

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && has_threshold(player) ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XB(2, 1), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		char msg[100];
		scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select target %s creature.", clr));
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XB(2, 1), 0,
										&td, msg);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

// Cards

int card_acorn_harvest(int player, int card, event_t event){
	/* Acorn Harvest	|3|G
	 * Sorcery
	 * Put two 1/1 |Sgreen Squirrel creature tokens onto the battlefield.
	 * Flashback-|1|G, Pay 3 life. */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback_with_life(player, card, event, MANACOST_XG(1, 1), 3);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_SQUIRREL, 2);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return basic_spell(player, card, event);
}

int card_ambassador_laquatus(int player, int card, event_t event){
/* CARD_ID_AMBASSADOR_LAQUATUS	5765
Ambassador Laquatus	|1|U|U
Legendary Creature - Merfolk Wizard 1/3
|3: Target player puts the top three cards of his or her library into his or her graveyard. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			mill(instance->targets[0].player, 3);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_PLAYER");
}

int card_anurid_scavenger(int player, int card, event_t event){
/* CARD_ID_ANURID_SCAVENGER	5767
Anurid Scavenger	|2|G
Creature - Frog Beast 3/3
Protection from |Sblack
At the beginning of your upkeep, sacrifice ~ unless you put a card from your graveyard on the bottom of your library. */

	return grave_recycler(player, card, event, 1);
}

int card_aquamoeba(int player, int card, event_t event){
/* CARD_ID_AQUAMOEBA	5768
Aquamoeba	|1|U
Creature - Elemental Beast 1/3
Discard a card: Switch ~'s power and toughness until end of turn. */

	if( event == EVENT_POW_BOOST ){
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_DISCARD, MANACOST0, 0, NULL, NULL) ){
			int pow = get_power(player, card);
			int tou = get_toughness(player, card);
			return tou-pow;
		}
	}

	if( event == EVENT_TOU_BOOST ){
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_DISCARD, MANACOST0, 0, NULL, NULL) ){
			int pow = get_power(player, card);
			int tou = get_toughness(player, card);
			return pow-tou;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		switch_power_and_toughness_until_eot(player, card, instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
}

int card_arrogant_wurm(int player, int card, event_t event){
/* CARD_ID_ARROGANT_WURM	5769
Arrogant Wurm	|3|G|G
Creature - Wurm 4/4
Trample
Madness |2|G (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

	return madness(player, card, event, MANACOST_XG(2, 1));
}

// balshan collaborator --> frozen shade

int card_balthor_the_stout(int player, int card, event_t event){
/* CARD_ID_BALTHOR_THE_STOUT	5772
Balthor the Stout	|1|R|R
Legendary Creature - Dwarf Barbarian 2/2
Other Barbarian creatures get +1/+1.
|R: Another target Barbarian creature gets +1/+0 until end of turn. */

	check_legend_rule(player, card, event);

	boost_creature_type(player, card, event, SUBTYPE_BARBARIAN, 1, 1, 0, 0);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_BARBARIAN;
	td.special = TARGET_SPECIAL_NOT_ME;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card, 1, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_R(1), 0,
									&td, "Select target Barbarian creature.");
}

int card_basking_rootwalla(int player, int card, event_t event){
/* CARD_ID_BASKING_ROOTWALLA	5774
Basking Rootwalla	|G
Creature - Lizard 1/1
|1|G: ~ gets +2/+2 until end of turn. Activate this ability only once each turn.
Madness |0 (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

	// Used externally
	if( event == EVENT_CAN_PAY_MADNESS_COST || event == EVENT_PAY_MADNESS_COST ){
		return madness(player, card, event, MANACOST0);
	}

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_ONCE_PER_TURN, MANACOST_XG(1, 1), 0, NULL, NULL) ){
			return 2;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		return generic_shade(player, card, event, 0, MANACOST_XG(1, 1), 2, 2, 0, 0);
	}

	return generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST_XG(1, 1), 0, NULL, NULL);
}

int card_breakthrough(int player, int card, event_t event){
/* CARD_ID_BREAKTHROUGH	5776
Breakthrough	|X|U
Sorcery
Draw four cards, then choose X cards in your hand and discard the rest. */

	if(event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		int x = instance->info_slot;
		draw_cards (player,4);
		if( x < hand_count[player] ){
			int count = 0;
			while( count < x ){
					int player_hand[2][100];
					int phc = 0;
					int i;
					for(i=0; i<active_cards_count[player]; i++){
						if( in_hand(player, i) && ! check_state(player, i, STATE_TARGETTED) ){
							player_hand[0][phc] = get_original_internal_card_id(player, i);
							player_hand[1][phc] = i;
							phc++;
						}
					}
					test_definition_t test;
					default_test_definition(&test, TYPE_ANY);
					scnprintf(test.message, 100, "Select a card to keep (%d of %d).", count+1, x);
					int selected = select_card_from_zone(player, player, player_hand[0], phc, 1, AI_MAX_VALUE, -1, &test);
					add_state(player, player_hand[1][selected], STATE_TARGETTED);
					count++;
			}
			for( count = active_cards_count[player]-1; count > -1; count-- ){
				if( in_hand(player, count) ){
					if( check_state(player, count, STATE_TARGETTED) ){
						remove_state(player, count, STATE_TARGETTED);
					}
					else{
						new_discard_card(player, count, player, 0);
					}
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_cabal_coffers(int player, int card, event_t event){
/* CARD_ID_CABAL_COFFERS	1505
Cabal Coffers	""
Land
|2, |T: Add |B to your mana pool for each |H2Swamp you control. */

	// Used externally
	// original code : 0x4E279B

	if( event == EVENT_CAN_ACTIVATE ){
		if( mana_producer(player, card, event) && has_mana(player, COLOR_ANY, 2) ){
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		add_state(player, card, STATE_TAPPED);
		charge_mana(player, COLOR_ANY, 2);
		if( spell_fizzled != 1 ){
			int cc_mana = count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_SWAMP));
			produce_mana_tapped(player, card, COLOR_BLACK, cc_mana);
			ai_modifier += (cc_mana < 3 ? -96 : 48);
		}
		else{
			remove_state(player, card, STATE_TAPPED);
		}
	}

	return 0;
}

int card_cabal_ritual(int player, int card, event_t event){
/* CARD_ID_CABAL_RITUAL	5777
Cabal Ritual	|1|B
Instant
Add |B|B|B to your mana pool.
Threshold - Add |B|B|B|B|B to your mana pool instead if seven or more cards are in your graveyard. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (hand_count[player] * 12);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( has_threshold(player) ){
			produce_mana(player, COLOR_BLACK, 5 );
		}
		else{
			produce_mana(player, COLOR_BLACK, 3 );
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_carrion_rats(int player, int card, event_t event){
/* CARD_ID_CARRION_RATS	5780
Carrion Rats	|B
Creature - Rat 2/1
Whenever ~ attacks or blocks, any player may exile a card from his or her graveyard. If a player does, ~ assigns no combat damage this turn. */

	return grave_eater(player, card, event, 1);
}

int card_carrion_wurm(int player, int card, event_t event){
/* CARD_ID_CARRION_WURM	5781
Carrion Wurm	|3|B|B
Creature - Zombie Wurm 6/5
Whenever ~ attacks or blocks, any player may exile three cards from his or her graveyard. If a player does, ~ assigns no combat damage this turn. */

	return grave_eater(player, card, event, 3);
}

int card_centaur_chieftain(int player, int card, event_t event){
/* Centaur Chieftain	|3|G
 * Creature - Centaur 3/3
 * Haste
 * Threshold - As long as seven or more cards are in your graveyard, ~ has "When ~ enters the battlefield, creatures you control
 *		get +1/+1 and gain trample until end of turn." */

	haste(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( has_threshold(player) ){
			pump_subtype_until_eot(player, card, player, -1, 1, 1, KEYWORD_TRAMPLE, 0);
		}
	}

	return 0;
}

int card_cephalid_sage(int player, int card, event_t event){
/* CARD_ID_CEPHALID_SAGE	5786
Cephalid Sage	|3|U
Creature - Cephalid 2/3
Threshold - As long as seven or more cards are in your graveyard, ~ has "When ~ enters the battlefield, draw three cards, then discard two cards." */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( has_threshold(player) ){
			draw_cards(player, 3);
			new_multidiscard(player, 2, 0, player);
		}
	}

	return 0;
}

int card_cephalid_vandal(int player, int card, event_t event){
/* Cephalid Vandal	|1|U
 * Creature - Cephalid Rogue 1/1
 * At the beginning of your upkeep, put a shred counter on ~. Then put the top card of your library into your graveyard for each
 *		shred counter on ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counter(player, card, COUNTER_SHRED);
		mill(player, count_counters(player, card, COUNTER_SHRED));
	}

	return 0;
}

int card_chainers_edict(int player, int card, event_t event){
	/* Chainer's Edict	|1|B
	 * Sorcery
	 * Target player sacrifices a creature.
	 * Flashback |5|B|B */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XB(5, 2));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			new_sacrifice(player, card, instance->targets[0].player, SAC_CAUSED | SAC_NO_CANCEL, &test);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);

}

static void turn_into_black_nightmare(int source_p, int source_c, int zombo_p, int zombo_c){
	change_color(source_p, source_c, zombo_p, zombo_c, COLOR_TEST_BLACK, CHANGE_COLOR_ADD|CHANGE_COLOR_NO_SOUND);
	add_a_subtype(zombo_p, zombo_c, SUBTYPE_NIGHTMARE);
}

int card_chainer_dementia_master(int player, int card, event_t event){
/*
Chainer, Dementia Master |3|B|B
Legendary Creature — Human Minion 3/3
Nightmare creatures get +1/+1.
{B}{B}{B}, Pay 3 life: Put target creature card from a graveyard onto the battlefield under your control.
						That creature is black and is a Nightmare in addition to its other creature types.
When Chainer, Dementia Master leaves the battlefield, exile all Nightmares.
*/
    check_legend_rule(player, card, event);

	boost_creature_type(player, card, event, SUBTYPE_NIGHTMARE, 1, 1, 0, BCT_INCLUDE_SELF);

	if( leaves_play(player, card, event) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.subtype = SUBTYPE_NIGHTMARE;
		this_test.not_me = 1; //To avoid unwanted interactions
		APNAP(p, {new_manipulate_all(player, card, p, &this_test, KILL_EXILE);};);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_B(3), 3, NULL, NULL) ){
			int i;
			for(i=0; i<2; i++){
				if( has_dead_creature(i) && ! graveyard_has_shroud(i) ){
					return 1;
				}
			}
		}
	}

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_B(3)) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
			if( select_target_from_either_grave(player, card, 0, AI_MAX_CMC, AI_MAX_CMC, &this_test, 0, 1) != -1 ){
				lose_life(player, 3);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			reanimate_permanent_with_function( 	instance->parent_controller, instance->parent_card,
												instance->targets[0].player, selected, REANIMATE_DEFAULT, &turn_into_black_nightmare);
		}
	}

	return 0;
}

int card_churning_eddy(int player, int card, event_t event)
{
  /* Churning Eddy	|3|U
   * Sorcery
   * Return target creature and target land to their owners' hands. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td_creature;
	default_target_definition(player, card, &td_creature, TYPE_CREATURE);

	target_definition_t td_land;
	default_target_definition(player, card, &td_land, TYPE_LAND);

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td_land) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td_creature, "TARGET_CREATURE", 1, NULL);
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
		generic_spell(player, card, event, GS_CAN_TARGET, &td_creature, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			new_pick_target(&td_land, "TARGET_LAND", 1, 1);
		}
	}

	if (event == EVENT_RESOLVE_SPELL)
	{
		if (validate_target(player, card, &td_creature, 0))
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);

		if (validate_target(player, card, &td_land, 1))
			bounce_permanent(instance->targets[1].player, instance->targets[1].card);

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_cleansing_meditation(int player, int card, event_t event){
/* CARD_ID_CLEANSING_MEDITATION	5793
Cleansing Meditation	|1|W|W
Sorcery
Destroy all enchantments.
Threshold - If seven or more cards are in your graveyard, instead destroy all enchantments,
then return all cards in your graveyard destroyed this way to the battlefield. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int tr = has_threshold(player);
		if( tr ){
			instance->info_slot = count_graveyard(player)-1;
		}
		else{
			instance->info_slot = -1;
		}
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ENCHANTMENT);
		APNAP(p,{ new_manipulate_all(player, card, p, &this_test, KILL_DESTROY); };);
		if( tr ){
			new_reanimate_all(player, card, player, &this_test, REANIMATE_DEFAULT);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_compulsion(int player, int card, event_t event){
/* CARD_ID_COMPULSION	5794
Compulsion	|1|U
Enchantment
|1|U, Discard a card: Draw a card.
|1|U, Sacrifice ~: Draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_XU(1, 1), 0, NULL, NULL) ){
			if( hand_count[player] ){
				return 1;
			}
			if( can_sacrifice_this_as_cost(player, card) ){
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, hand_count[player] ? 1 : 0, can_sacrifice_this_as_cost(player, card)};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Discard", abils[1], 5,
							"Sac Compulsion", abils[2], 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		generic_activated_ability(player, card, event, 0, MANACOST_XU(1, 1), 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			if( choice == 1 ){
				discard(player, 0, player);
			}
			if( choice == 2 ){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_crippling_fatigue(int player, int card, event_t event){
	/* Crippling Fatigue	|1|B|B
	 * Sorcery
	 * Target creature gets -2/-2 until end of turn.
	 * Flashback-|1|B, Pay 3 life. */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback_with_life(player, card, event, MANACOST_XB(1, 1), 3);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -2, -2);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_dawn_of_the_dead(int player, int card, event_t event){
/* CARD_ID_DAWN_OF_THE_DEAD	5799
Dawn of the Dead	|2|B|B|B
Enchantment
At the beginning of your upkeep, you lose 1 life.
At the beginning of your upkeep, you may return target creature card from your graveyard to the battlefield.
That creature gains haste until end of turn. Exile it at the beginning of the next end step. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		lose_life(player, 1);
		if( has_dead_creature(player) && ! graveyard_has_shroud(player) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
			int result = new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
			if( result > -1 ){
				create_targetted_legacy_effect(player, card, &haste_and_remove_eot, player, result);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_deep_analysis(int player, int card, event_t event){
	/* Deep Analysis	|3|U
	 * Sorcery
	 * Target player draws two cards.
	 * Flashback-|1|U, Pay 3 life. */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback_with_life(player, card, event, MANACOST_XU(1, 1), 3);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td2;
	default_target_definition(player, card, &td2, 0);
	td2.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td2) ){
			draw_cards(get_card_instance(player, card)->targets[0].player, 2);
		}
		kill_card(player, card, get_flashback(player, card) ? KILL_EXILE : KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td2, "TARGET_PLAYER", 1, NULL);
}

int card_devestating_dreams(int player, int card, event_t event){
/* CARD_ID_DEVASTATING_DREAMS	5801
Devastating Dreams	|R|R
Sorcery
As an additional cost to cast ~, discard X cards at random.
Each player sacrifices X lands. ~ deals X damage to each creature. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance= get_card_instance(player, card);
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		test.qty = instance->info_slot;
		APNAP(p,{ new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test); };);

		new_damage_all(player, card, ANYBODY, instance->info_slot, NDA_ALL_CREATURES, NULL);

		kill_card(player, card, KILL_DESTROY);
	}
	return dream(player, card, event, 1);
}

int card_dwell_on_the_past(int player, int card, event_t event){
/*
Dwell on the Past |G
Sorcery
Target player shuffles up to four target cards from his or her graveyard into his or her library.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			select_multiple_cards_from_graveyard(player, instance->targets[0].player, 0, player == instance->targets[0].player ? AI_MAX_VALUE : AI_MIN_VALUE, NULL, 4, &instance->targets[1]);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int i, any_shuffled_in = 0;
		for (i = 1; i <= 4; ++i){
			int selected = validate_target_from_grave(player, card, instance->targets[0].player, i);
			if (selected != -1){
				from_graveyard_to_deck(instance->targets[0].player, selected, 2);
				any_shuffled_in = 1;
			}
		}
		if (any_shuffled_in){
			shuffle(instance->targets[0].player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_faceless_butcher(int player, int card, event_t event){
/* CARD_ID_FACELESS_BUTCHER	5805
Faceless Butcher	|2|B|B
Creature - Nightmare Horror 2/3
When ~ enters the battlefield, exile target creature other than ~.
When ~ leaves the battlefield, return the exiled card to the battlefield under its owner's control. */

	return_from_oblivion(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.special = TARGET_SPECIAL_NOT_ME;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select another target creature.", 0, GS_LITERAL_PROMPT) ){
			obliviation(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

static int false_memories_legacy(int player, int card, event_t event){

	if( eot_trigger(player, card, event) ){
		char buffer2[100];
		scnprintf(buffer2, 100, "Select a card to exile.");
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, buffer2);
		const int *grave = get_grave(player);
		int amount = 0;
		while( amount < 7 && grave[0] != -1 ){
				if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 1, AI_MIN_VALUE, &this_test) != -1 ){
					amount++;
				}
		}
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_false_memories(int player, int card, event_t event){
/* CARD_ID_FALSE_MEMORIES	5806
False Memories	|1|U
Instant
Put the top seven cards of your library into your graveyard. At the beginning of the next end step, exile seven cards from your graveyard. */

	if( event == EVENT_RESOLVE_SPELL ){
		mill(player, 7);
		create_legacy_effect(player, card, &false_memories_legacy);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_far_wanderings(int player, int card, event_t event){
/* Far Wanderings	|2|G
 * Sorcery
 * Search your library for a basic land card and put that card onto the battlefield tapped. Then shuffle your library.
 * Threshold - If seven or more cards are in your graveyard, instead search your library for three basic land cards and put them onto
 * 		the battlefield tapped. Then shuffle your library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (has_threshold(player) ? 48 : 24);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		tutor_basic_lands(player, TUTOR_PLAY_TAPPED, has_threshold(player) ? 3 : 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_fiery_temper(int player, int card, event_t event){
	/* Fiery Temper	|1|R|R	0x2008f45
	 * Instant
	 * ~ deals 3 damage to any target.
	 * Madness |R */
	if( event == EVENT_CAN_PAY_MADNESS_COST || event == EVENT_PAY_MADNESS_COST ){
		return madness(player, card, event, MANACOST_R(1));
	}
	return card_lightning_bolt(player, card, event);
}

int card_flaming_gambit(int player, int card, event_t event){
	/* Flaming Gambit	|X|R	0x2008f4a
	 * Instant
	 * ~ deals X damage to target player or planeswalker.
	 That player or that planeswalker's controller may choose a creature they control and have ~ deal that damage to it instead.
	 * Flashback |X|R|R */
	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XR(-1, 2));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			target_definition_t td1;
			default_target_definition(player, card, &td1, TYPE_CREATURE );
			td1.who_chooses = instance->targets[0].player;
			td1.allowed_controller = instance->targets[0].player;
			td1.preferred_controller = instance->targets[0].player;
			if( can_target(&td1) && new_pick_target(&td1, "TARGET_CREATURE", 1, 0) ){
				damage_creature(instance->targets[1].player, instance->targets[1].card, instance->info_slot, player, card);
			}
			else{
				damage_target0(player, card, instance->info_slot);
			}
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
}

int card_frantic_purification(int player, int card, event_t event){
	/* Frantic Purification	|2|W
	 * Instant
	 * Destroy target enchantment.
	 * Madness |W */

	if( event == EVENT_CAN_PAY_MADNESS_COST || event == EVENT_PAY_MADNESS_COST ){
		return madness(player, card, event, MANACOST_W(1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ENCHANTMENT", 1, NULL);
}

int card_gloomdrifter(int player, int card, event_t event){
/* Gloomdrifter	|3|B
 * Creature - Zombie Minion 2/2
 * Flying
 * Threshold - As long as seven or more cards are in your graveyard, ~ has "When ~ enters the battlefield, non|Sblack creatures get -2/-2
 *		until end of turn." */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( has_threshold(player) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.color = get_sleighted_color_test(player, card, get_sleighted_color_test(player, card, COLOR_TEST_BLACK));
			this_test.color_flag = 1;
			pump_creatures_until_eot(player, card, ANYBODY, 0, -2, -2, 0, 0, &this_test);
		}
	}

	return 0;
}

int card_grim_lavamancer(int player, int card, event_t event){
	/* Grim Lavamancer	|R	0x2001830
	 * Creature - Human Wizard 1/1
	 * |R, |T, Exile two cards from your graveyard: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td, NULL) ){
			return count_graveyard(player) >= 2 ? 1 : 0;
		}
	}
	if( event == EVENT_ACTIVATE ){
		int grave_copy[count_graveyard(player)];
		int i;
		for(i=0; i<count_graveyard(player); i++){
			grave_copy[i] = get_grave(player)[i];
		}
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a card to exile.");
		test.id = cards_data[iid_draw_a_card].id;
		test.id_flag = DOESNT_MATCH;
		for(i=0; i<2; i++){
			int selected = select_card_from_zone(player, player, grave_copy, count_graveyard(player), 0, AI_MIN_VALUE, -1, &test);
			if( selected != -1 ){
				grave_copy[selected] = iid_draw_a_card;
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_ANY");
		if( spell_fizzled != 1 ){
			int count = count_graveyard(player)-1;
			while( count > -1 ){
					if( grave_copy[count] == iid_draw_a_card ){
						rfg_card_from_grave(player, count);
					}
					count--;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		damage_target0(player, card, 2);
	}

	return 0;
}

int card_gurzigost(int player, int card, event_t event){
/* CARD_ID_GURZIGOST	5818
Gurzigost	|3|G|G
Creature - Beast 6/8
At the beginning of your upkeep, sacrifice ~ unless you put two cards from your graveyard on the bottom of your library.
|G|G, Discard a card: You may have ~ assign its combat damage this turn as though it weren't blocked. */

	if( event == EVENT_DEAL_DAMAGE && get_card_instance(player, card)->info_slot == 1){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card != -1 && damage->damage_source_player == player &&  damage->damage_source_card == card && damage->info_slot > 0 ){
				damage->damage_target_card = -1;
			}
		}
	}

	if( event == EVENT_DECLARE_BLOCKERS && is_attacking(player, card) && ! is_unblocked(player, card)){
		if( has_mana(player, COLOR_GREEN, 2) && hand_count[player] > 0 ){
			int choice = do_dialog(player, player, card, -1, -1, " Deal damage normally\n Ignore Blocker", 1);
			if( choice == 1 ){
				charge_mana(player, COLOR_GREEN, 2);
				if( spell_fizzled != 1 ){
					discard(player, 0, player);
					get_card_instance(player, card)->info_slot = 1;
				}
			}
		}
	}
	return grave_recycler(player, card, event, 2);
}

int card_hell_bent_rider(int player, int card, event_t event){
/* CARD_ID_HELL_BENT_RAIDER	5819
Hell-Bent Raider	|1|R|R
Creature - Human Barbarian 2/2
First strike, haste
Discard a card at random: ~ gains protection from |Swhite until end of turn. */

	haste(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade(player, card, event, 0, MANACOST0, 0, 0, KEYWORD_PROT_WHITE, 0);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD_RANDOM, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

int card_hypnox(int player, int card, event_t event){
/* CARD_ID_HYPNOX	5822
Hypnox	|8|B|B|B
Creature - Nightmare Horror 8/8
Flying
When ~ enters the battlefield, if you cast it from your hand, exile all cards from target opponent's hand.
When ~ leaves the battlefield, return the exiled cards to their owner's hand. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance(player, card)->info_slot = not_played_from_hand(player, card);
	}

	if( comes_into_play(player, card, event) && get_card_instance(player, card)->info_slot != 1 ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);

		if( would_validate_arbitrary_target(&td, 1-player, -1) ){
			int count = active_cards_count[1-player]-1;
			instance->targets[1].player = 1;
			while( count > -1 ){
					if( in_hand(1-player, count) ){
						int leg, idx;
						int iid = get_original_internal_card_id(1-player, count);
						exiledby_remember(player, card, 1-player, iid, &leg, &idx);
						rfg_card_in_hand(1-player, count);
					}
					count--;
			}
		}
	}

	if( leaves_play(player, card, event) ){
		return_cards_exiled_by_me_to_hand(player, card);
	}

	return 0;
}

int card_hypochondria(int player, int card, event_t event){
	/* Hypochondria	|1|W	0x2008f63
	 * Enchantment
	 * |W, Discard a card: Prevent the next 3 damage that would be dealt to any target this turn.
	 * |W, Sacrifice ~: Prevent the next 3 damage that would be dealt to any target this turn. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST_W(1), 0, &td, NULL);
		if( result ){
			if( hand_count[player] ){
				return result;
			}
			if( can_sacrifice_this_as_cost(player, card) ){
				return result;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, hand_count[player] ? 1 : 0, can_sacrifice_this_as_cost(player, card)};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Discard", abils[1], 5,
							"Sac Hypochondria", abils[2], 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST_W(1), 0, &td, "TARGET_DAMAGE");
		if( spell_fizzled != 1 ){
			if( choice == 1 ){
				discard(player, 0, player);
			}
			if( choice == 2 ){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 3);
		}
	}

	return 0;
}

int card_ichorid(int player, int card, event_t event){

	/* Ichorid	|3|B
	 * Creature - Horror 3/1
	 * Haste
	 * At the beginning of the end step, sacrifice ~.
	 * At the beginning of your upkeep, if ~ is in your graveyard, you may exile a |Sblack creature card other than ~ from your graveyard. If you do, return ~
	 * to the battlefield. */

	haste(player, card, event);
	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		int found = 0;
		const int *graveyard = get_grave(player);
		int i;
		for(i=0;i<count_graveyard(player);i++){
			if(graveyard[i] != -1 && ( cards_data[  graveyard[i] ].color & COLOR_TEST_BLACK ) && ( cards_data[  graveyard[i] ].type & TYPE_CREATURE ) ){
				found++;
			}
		}
		if( found > 1 ){
			int choice = do_dialog(player, player, card, -1, -1," Return Ichorid\n Do not return Ichorid", 0);
			if( choice == 0 ){
				int i_loc = -1;
				int iid = -1;
				// first remove ichorid from the grave
				int cg = count_graveyard(player);
				for (i = 0; i < cg; ++i){
					if( cards_data[graveyard[i]].id == CARD_ID_ICHORID ){
						i_loc = i;
						iid = turn_card_in_grave_face_down(player, i);
						break;
					}
				}
				ASSERT(iid != -1);

				// choose a black dude to remove
                test_definition_t this_test;
	            new_default_test_definition(&this_test, TYPE_CREATURE, get_sleighted_color_text(player, card, "Select a %s creature.", COLOR_BLACK));
				this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
				int selected = new_select_a_card(player, player, TUTOR_FROM_GRAVE, 0, AI_MIN_VALUE, -1, &this_test);
				turn_card_in_grave_face_up(player, i_loc, iid);

				if( selected != -1){
					if( i_loc > selected ){
						--i_loc;
					}
					rfg_card_from_grave(player, selected);

					// put ichorid into play
					reanimate_permanent(player, -1, player, i_loc, REANIMATE_DEFAULT);
				}
			}
		}
		return -2;
	}
	if( eot_trigger(player, card, event ) ){
		kill_card(player, card, KILL_SACRIFICE);
	}
	return 0;
}

int card_insidious_dreams(int player, int card, event_t event){
/* CARD_ID_INSIDIOUS_DREAMS	5825
Insidious Dreams	|3|B
Instant
As an additional cost to cast ~, discard X cards.
Search your library for X cards. Then shuffle your library and put those cards on top of it in any order. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance= get_card_instance(player, card);

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card to put on top of your deck.");
		this_test.no_shuffle = 1;

		int i;
		int tutored[instance->info_slot];
		for (i = 0; i < instance->info_slot; ++i){
			tutored[i] = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		}

		shuffle(player);

		int really_tutored = 0;

		for (i = 0; i < instance->info_slot; ++i){
			if(tutored[i] != -1){
				put_on_top_of_deck(player, tutored[i]);
				really_tutored++;
			}
		}

		if( really_tutored > 1 ){
			rearrange_top_x(player, player, really_tutored);
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return dream(player, card, event, 0);
}

static int insist_effect(int player, int card, event_t event){
	if( event == EVENT_CAST_SPELL && ! affect_me(player, card) && affected_card_controller == player ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			state_untargettable(affected_card_controller, affected_card, 1);
		}
	}
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me( player, card ) &&
		reason_for_trigger_controller == affected_card_controller
	  ){
		if( is_what(trigger_cause_controller, trigger_cause, TYPE_CREATURE) && check_state(trigger_cause_controller, trigger_cause, STATE_CANNOT_TARGET) ){
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					state_untargettable(trigger_cause_controller, trigger_cause, 0);
					kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( eot_trigger(player, card, event) ){
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_insist(int player, int card, event_t event){
/* CARD_ID_INSIST	5826
Insist	|G
Sorcery
The next creature spell you cast this turn can't be countered.
Draw a card. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		if( current_phase < PHASE_DECLARE_ATTACKERS ){
			ai_modifier+=25;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 1);
		create_legacy_effect(player, card, &insist_effect);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_krosan_restorer(int player, int card, event_t event){
/* CARD_ID_KROSAN_RESTORER	5830
Krosan Restorer	|2|G
Creature - Human Druid 1/2
|T: Untap target land.
Threshold - |T: Untap up to three target lands. Activate this ability only if seven or more cards are in your graveyard. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_LAND");
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_LAND");
		if( spell_fizzled != 1 ){
			ai_mod_for_untap_effects(player, card, 0, 0);
			if( has_threshold(player) ){
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
				int i;
				for(i=1; i<3; i++){
					if( new_pick_target(&td, "TARGET_LAND", i, 0) ){
						ai_mod_for_untap_effects(player, card, i, 0);
					}
				}
			}
			int i;
			for(i=0; i<instance->number_of_targets; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				untap_card(instance->targets[i].player, instance->targets[i].card);
			}
		}
	}

	return 0;
}

int card_laquatus_champion(int player, int card, event_t event){
/* CARD_ID_LAQUATUSS_CHAMPION	5831
Laquatus's Champion	|4|B|B
Creature - Nightmare Horror 6/3
When ~ enters the battlefield, target player loses 6 life.
When ~ leaves the battlefield, that player gains 6 life.
|B: Regenerate ~. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( pick_target(&td, "TARGET_PLAYER") ){
			instance->info_slot += lose_life(instance->targets[0].player, 6);
		}
	}

	if( leaves_play(player, card, event) ){
		card_instance_t *instance = get_card_instance(player, card);
		gain_life(instance->targets[0].player, instance->info_slot);
	}

	return regeneration(player, card, event, MANACOST_B(1));
}

static int last_laugh_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_LAST_LAUGH ){
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
				count_for_gfp_ability(player, card, event, ANYBODY, TYPE_PERMANENT, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			new_damage_all(player, card, ANYBODY, amount, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
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

int card_last_laugh(int player, int card, event_t event){
/*
Last Laugh |2|B|B
Enchantment
Whenever a permanent other than Last Laugh is put into a graveyard from the battlefield, Last Laugh deals 1 damage to each creature and
	each player.
When no creatures are on the battlefield, sacrifice Last Laugh.
*/
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)last_laugh_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &last_laugh_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY && ! affect_me(player, card) ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_PERMANENT, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			new_damage_all(player, card, ANYBODY, amount, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
		}
	}

	// When no creatures are on the battlefield, sacrifice ~.
	if( event == EVENT_STATIC_EFFECTS && ! check_battlefield_for_subtype(ANYBODY, TYPE_CREATURE, -1) ){
		kill_card(player, card, KILL_SACRIFICE);
	}

	return global_enchantment(player, card, event);
}

int card_liquify(int player, int card, event_t event){
/* CARD_ID_LIQUIFY	5833
Liquify	|2|U
Instant
Counter target spell with converted mana cost 3 or less. If that spell is countered this way, exile it instead of putting it into its owner's graveyard.*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);
	td.extra = 3;
	td.special |= TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;

	if (event == EVENT_RESOLVE_SPELL){
		if (counterspell_validate(player, card, &td, 0)){
			card_instance_t* instance = get_card_instance(player, card);
			set_flags_when_spell_is_countered(player, card, instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
}

int card_mesmeric_fiend(int player, int card, event_t event){
/* CARD_ID_MESMERIC_FIEND	5837
Mesmeric Fiend	|1|B
Creature - Nightmare Horror 1/1
When ~ enters the battlefield, target opponent reveals his or her hand and you choose a nonland card from it. Exile that card.
When ~ leaves the battlefield, return the exiled card to its owner's hand. */

	// Used externally
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    card_instance_t *instance = get_card_instance(player, card);
	if( event == EVENT_ETB_ABILITY ){
		if( target_opponent(player, card) && hand_count[1-player] ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND, "Select a nonland card.");
			this_test.type_flag = DOESNT_MATCH;
			this_test.zone = TARGET_ZONE_HAND;

			int cannot_cancel = check_battlefield_for_special_card(player, card, instance->targets[0].player, CBFSC_CHECK_ONLY, &this_test);

			int selected = new_select_a_card(player, instance->targets[0].player, TUTOR_FROM_HAND, cannot_cancel, AI_MAX_VALUE, -1, &this_test);
			if( selected > -1 ){
				create_card_name_legacy(player, card, get_id(instance->targets[0].player, selected));
				exile_card_and_remember_it_on_exiledby(player, card, instance->targets[0].player, selected);
			}
		}
	}

	if( leaves_play(player, card, event) ){
		return_cards_exiled_by_me_to_hand(player, card);
	}

	return 0;
}

int card_mind_sludge(int player, int card, event_t event){
/* CARD_ID_MIND_SLUDGE	1089
Mind Sludge	|4|B
Sorcery
Target player discards a card for each |H2Swamp you control. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_SWAMP));
			new_multidiscard(instance->targets[0].player, amount, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_morningtide(int player, int card, event_t event){
/* CARD_ID_MORNINGTIDE	5839
Morningtide	|1|W
Sorcery
Exile all cards from all graveyards. */

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p,{ rfg_whole_graveyard(p); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_mortal_combat(int player, int card, event_t event){
/* CARD_ID_MORTAL_COMBAT	5840
Mortal Combat	|2|B|B
Enchantment
At the beginning of your upkeep, if twenty or more creature cards are in your graveyard, you win the game. */

	if( current_turn == player && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) && reason_for_trigger_controller == player ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) > 19 && ! is_humiliated(player, card) ){
			if(event == EVENT_TRIGGER){
				event_result |= 2;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					lose_the_game(1-player);
			}
		}
	}

	if (event == EVENT_SHOULD_AI_PLAY){
		int creatures = count_graveyard_by_type(player, TYPE_CREATURE) - 10;
		if (creatures > 10){
			if (player == AI){
				ai_modifier += creatures * creatures * creatures;
			} else if (creatures >= 5){
				ai_modifier -= creatures * creatures * creatures;
			}
		}
		if (creatures >= 20){
			lose_the_game(1-player);
		}
	}

	return global_enchantment(player, card, event);
}

int card_mortiphobia(int player, int card, event_t event){
	/*
	  Mortiphobia |1|B|B
	  Enchantment
	  {1}{B}, Discard a card: Exile target card from a graveyard.
	  {1}{B}, Sacrifice Mortiphobia: Exile target card from a graveyard.
	*/
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( (count_graveyard(player) > 0 && ! graveyard_has_shroud(player)) || (count_graveyard(1-player) > 0 && ! graveyard_has_shroud(1-player)) ){
			if( generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST_XB(1, 1), 0, NULL, NULL) ){
				return 1;
			}
			if( generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XB(1, 1), 0, NULL, NULL) ){
				return 1;
			}
		}
	}

	if(event == EVENT_ACTIVATE ){
		int choice = instance->info_slot = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_DISCARD, MANACOST_XB(1, 1), 0, NULL, NULL) ){
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME, MANACOST_XB(1, 1), 0, NULL, NULL) ){
				choice = do_dialog(player, player, card, -1, -1, " Discard\n Sac Mortiphobia\n Cancel", 0);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 2 ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_XB(1, 1)) ){
			if( select_target_from_either_grave(player, card, 0, AI_MIN_VALUE, AI_MAX_VALUE, &this_test, 0, 1) != -1 ){
				instance->info_slot = choice;
				if( choice == 0 ){
					discard(player, 0, player);
				}
				if( choice == 1 ){
					kill_card(player, card, KILL_SACRIFICE);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			rfg_card_from_grave(instance->targets[0].player, selected);
		}
	}

	return 0;
}

int card_mutilate(int player, int card, event_t event){
/* CARD_ID_MUTILATE	5842
Mutilate	|2|B|B
Sorcery
All creatures get -1/-1 until end of turn for each |H2Swamp you control. */

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_SWAMP));
		APNAP(p, { pump_subtype_until_eot(player, card, p, -1, -amount, -amount, 0, 0); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_mystic_familiar(int player, int card, event_t event){
/* CARD_ID_MYSTIC_FAMILIAR	5843
Mystic Familiar	|1|W
Creature - Bird 1/2
Flying
Threshold - As long as seven or more cards are in your graveyard, ~ gets +1/+1 and has protection from |Sblack. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES) && affect_me(player, card) ){
		if( in_play(player, card) && ! is_humiliated(player, card) && has_threshold(player) ){
			if( event == EVENT_ABILITIES ){
				event_result |= get_sleighted_protection(player, card, KEYWORD_PROT_BLACK);
			}
			else{
				event_result++;
			}
		}
	}

	return 0;
}

int card_nantuko_cultivator(int player, int card, event_t event){
/* CARD_ID_NANTUKO_CULTIVATOR	5846
Nantuko Cultivator	|3|G
Creature - Insect Druid 2/2
When ~ enters the battlefield, you may discard any number of land cards. Put that many +1/+1 counters on ~ and draw that many cards. */

	if( comes_into_play(player, card, event) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a land card to discard.");
		this_test.zone = TARGET_ZONE_HAND;
		int count = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test);
		int amount = 0;
		while( count > 0 ){
				int result = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
				if( result != -1 ){
					discard_card(player, result);
					amount++;
					count--;
				}
				else{
					break;
				}
		}
		add_1_1_counters(player, card, amount);
		draw_cards(player, amount);
	}

	return 0;
}

int card_nantuko_shade(int player, int card, event_t event){
/* CARD_ID_NANTUKO_SHADE	1260
Nantuko Shade	|B|B
Creature - Insect Shade 2/1
|B: ~ gets +1/+1 until end of turn. */

	// Used externally
	return generic_shade_merge_pt(player, card, event, 0, MANACOST_B(1), 1,1);
}

int card_narcissism(int player, int card, event_t event){
/* CARD_ID_NARCISSISM	5847
Narcissism	|2|G
Enchantment
|G, Discard a card: Target creature gets +2/+2 until end of turn.
|G, Sacrifice ~: Target creature gets +2/+2 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_G(1), 0, &td, NULL);
		if( result ){
			if( hand_count[player] ){
				return result;
			}
			if( can_sacrifice_this_as_cost(player, card) ){
				return result;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, hand_count[player] ? 1 : 0, can_sacrifice_this_as_cost(player, card)};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Discard to boost.", abils[1], 5,
							"Sac Hypochondria to boost.", abils[2], 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_G(1), 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			if( choice == 1 ){
				discard(player, 0, player);
			}
			if( choice == 2 ){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
	}

	return 0;
}

int card_nostalgic_dreams(int player, int card, event_t event){
/* CARD_ID_NOSTALGIC_DREAMS	5848
Nostalgic Dreams	|G|G
Sorcery
As an additional cost to cast ~, discard X cards.
Return X target cards from your graveyard to your hand. Exile ~. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}
	else if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		int max_cards = count_graveyard(player);
		max_cards = MIN(max_cards, hand_count[player]);
		max_cards = MIN(max_cards, 19);

		if (max_cards <= 0 || graveyard_has_shroud(player)){
			ai_modifier -= 48;
			instance->info_slot = 0;
			return 0;
		}

		instance->info_slot = select_multiple_cards_from_graveyard(player, player, 0, AI_MAX_VALUE, NULL, max_cards, &instance->targets[0]);

		if (instance->info_slot > 0){
			multidiscard(player, instance->info_slot, 0);
		}
	}
	else if (event == EVENT_RESOLVE_SPELL){
		int i, num_validated = 0;
		for (i = 0; i < instance->info_slot; ++i){
			int selected = validate_target_from_grave(player, card, player, i);
			if (selected != -1){
				from_grave_to_hand(player, selected, TUTOR_HAND);
				++num_validated;
			}
		}
		if (num_validated == 0 && instance->info_slot > 0){
			spell_fizzled = 1;
			kill_card(player, card, KILL_DESTROY);
		} else {
			kill_card(player, card, KILL_EXILE);
		}
	}
	return 0;
}

int card_obsessive_search(int player, int card, event_t event){
/* CARD_ID_OBSESSIVE_SEARCH	5849
Obsessive Search	|U
Instant
Draw a card.
Madness |U (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

	if( event == EVENT_CAN_PAY_MADNESS_COST || event == EVENT_PAY_MADNESS_COST ){
		return madness(player, card, event, MANACOST_U(1));
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_organ_grinder(int player, int card, event_t event){
/* CARD_ID_ORGAN_GRINDER	5850
Organ Grinder	|2|B
Creature - Zombie 3/1
|T, Exile three cards from your graveyard: Target player loses 3 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			return count_graveyard(player) >= 3 ? 1 : 0;
		}
	}

	if( event == EVENT_ACTIVATE ){
		int grave_copy[count_graveyard(player)];
		int i;
		for(i=0; i<count_graveyard(player); i++){
			grave_copy[i] = get_grave(player)[i];
		}
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a card to exile.");
		test.id = cards_data[iid_draw_a_card].id;
		test.id_flag = DOESNT_MATCH;
		for(i=0; i<3; i++){
			int selected = select_card_from_zone(player, player, grave_copy, count_graveyard(player), 0, AI_MIN_VALUE, -1, &test);
			if( selected != -1 ){
				grave_copy[selected] = iid_draw_a_card;
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
		if( spell_fizzled != 1 ){
			int count = count_graveyard(player)-1;
			while( count > -1 ){
					if( grave_copy[count] == iid_draw_a_card ){
						rfg_card_from_grave(player, count);
					}
					count--;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		lose_life(instance->targets[0].player, 2);
	}

	return 0;
}

static int overmaster_effect(int player, int card, event_t event){
	if( event == EVENT_CAST_SPELL && ! affect_me(player, card) && affected_card_controller == player ){
		if( is_what(affected_card_controller, affected_card, TYPE_SPELL) ){
			state_untargettable(affected_card_controller, affected_card, 1);
			kill_card(player, card, KILL_EXILE);
		}
	}

	if( eot_trigger(player, card, event) ){
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_overmaster(int player, int card, event_t event){
/* CARD_ID_OVERMASTER	5851
Overmaster	|R
Sorcery
The next instant or sorcery spell you cast this turn can't be countered by spells or abilities.
Draw a card. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		if( current_phase < PHASE_DECLARE_ATTACKERS ){
			ai_modifier+=25;
		}
	}
	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 1);
		create_legacy_effect(player, card, &overmaster_effect);
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

int card_parallel_evolution(int player, int card, event_t event){
/* CARD_ID_PARALLEL_EVOLUTION	5852
Parallel Evolution	|3|G|G
Sorcery
For each creature token on the battlefield, its controller creates a token that's a copy of that creature.
Flashback |4|G|G|G (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XG(4, 3));
	}

	if( event == EVENT_RESOLVE_SPELL ){
		copy_all_tokens(player, card, TYPE_CREATURE, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_pardic_arsonist(int player, int card, event_t event){
	/* Pardic Arsonist	|2|R|R	0x2008fbd
	 * Creature - Human Barbarian 3/3
	 * Threshold - As long as seven or more cards are in your graveyard, ~ has "When ~ enters the battlefield, it deals 3 damage to any target." */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		if( has_threshold(player) && can_target(&td) ){
			if( pick_target(&td, "TARGET_ANY") ){
				damage_target0(player, card, 3);
			}
		}
		get_card_instance(player, card)->number_of_targets = 0;
	}

	return 0;
}

// pardic collaborator --> frozen shade

int card_pay_no_heed(int player, int card, event_t event){
/* CARD_ID_PAY_NO_HEED	5856
Pay No Heed	|W
Instant
Prevent all damage a source of your choice would deal this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, -1);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, "TARGET_DAMAGE", 1, NULL);
}

int card_petradon(int player, int card, event_t event){
/* CARD_ID_PETRADON	5857
Petradon	|6|R|R
Creature - Nightmare Beast 5/6
When ~ enters the battlefield, exile two target lands.
When ~ leaves the battlefield, return the exiled cards to the battlefield under their owners' control.
|R: ~ gets +1/+0 until end of turn. */

// 0x200f58e
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		int removed = 0;
		while( removed < 2 && can_target(&td) ){
				if( new_pick_target(&td, "TARGET_LAND", removed, 0) ){
					state_untargettable(instance->targets[removed].player, instance->targets[removed].card, 1);
					removed++;
				}
		}
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			int csvid = get_id(instance->targets[i].player, instance->targets[i].card);
			exile_card_and_remember_it_on_exiledby(player, card, instance->targets[i].player, instance->targets[i].card);
			create_card_name_legacy(player, card, csvid);
		}
	}

	if( leaves_play(player, card, event) ){
		return_cards_exiled_by_me_to_play(player, card);
	}

	return 0;
}

static int plagiarize_legacy(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) &&
		reason_for_trigger_controller == get_card_instance(player, card)->targets[0].player && !suppress_draw){
		if( event == EVENT_TRIGGER){
			event_result |= 2u;
		}
		else if( event == EVENT_RESOLVE_TRIGGER ){
				draw_cards(player, 1);
				suppress_draw = 1;
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_plagiarize(int player, int card, event_t event){

	/* Plagiarize	|3|U
	 * Instant
	 * Until end of turn, if target player would draw a card, instead that player skips that draw and you draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int legacy = create_legacy_effect(player, card, &plagiarize_legacy);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[0].player = instance->targets[0].player;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_possessed_aven(int player, int card, event_t event){
/* CARD_ID_POSSESSED_AVEN	5861
Possessed Aven	|2|U|U
Creature - Bird Soldier Horror 3/3
Flying
Threshold - As long as seven or more cards are in your graveyard, ~ gets +1/+1, is |Sblack, and has "|2|B, |T: Destroy target |Sblue creature." */

	return possessed(player, card, event, COLOR_BLUE);
}

int card_possessed_barbarian(int player, int card, event_t event){
/* CARD_ID_POSSESSED_BARBARIAN	5862
Possessed Barbarian	|2|R|R
Creature - Human Barbarian Horror 3/3
First strike
Threshold - As long as seven or more cards are in your graveyard, ~ gets +1/+1, is |Sblack, and has "|2|B, |T: Destroy target |Sred creature." */

	return possessed(player, card, event, COLOR_RED);
}

int card_possessed_centaur(int player, int card, event_t event){
/* CARD_ID_POSSESSED_CENTAUR	5863
Possessed Centaur	|2|G|G
Creature - Centaur Horror 3/3
Trample
Threshold - As long as seven or more cards are in your graveyard, ~ gets +1/+1, is |Sblack, and has "|2|B, |T: Destroy target |Sgreen creature." */

	return possessed(player, card, event, COLOR_GREEN);
}

int card_possessed_nomad(int player, int card, event_t event){
/* CARD_ID_POSSESSED_NOMAD	5864
Possessed Nomad	|2|W|W
Creature - Human Nomad Horror 3/3
Vigilance
Threshold - As long as seven or more cards are in your graveyard, ~ gets +1/+1, is |Sblack, and has "|2|B, |T: Destroy target |Swhite creature." */

	vigilance(player, card, event);
	return possessed(player, card, event, COLOR_WHITE);
}

int card_putrid_imp(int player, int card, event_t event){
/* CARD_ID_PUTRID_IMP	5866
Putrid Imp	|B
Creature - Zombie Imp 1/1
Discard a card: ~ gains flying until end of turn.
Threshold - As long as seven or more cards are in your graveyard, ~ gets +1/+1 and can't block. */

	threshold_boost(player, card, event, 1, 1, 0, SP_KEYWORD_CANNOT_BLOCK);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade(player, card, event, 0, MANACOST0, 0, 0, KEYWORD_FLYING, 0);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
}

int card_pyromania(int player, int card, event_t event){
	/* Pyromania	|2|R	0x2008fdb
	 * Enchantment
	 * |1|R, Discard a card at random: ~ deals 1 damage to any target.
	 * |1|R, Sacrifice ~: It deals 1 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, NULL);
		if( result ){
			if( hand_count[player] ){
				return result;
			}
			if( can_sacrifice_this_as_cost(player, card) ){
				return result;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, hand_count[player] ? 1 : 0, can_sacrifice_this_as_cost(player, card)};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Discard", abils[1], 5,
							"Sac Pyromania", abils[2], 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, "TARGET_ANY");
		if( spell_fizzled != 1 ){
			if( choice == 1 ){
				discard(player, DISC_RANDOM, player);
			}
			if( choice == 2 ){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}

	return 0;
}

int card_rancid_earth(int player, int card, event_t event){
/* CARD_ID_RANCID_EARTH	5869
Rancid Earth	|1|B|B
Sorcery
Destroy target land.
Threshold - If seven or more cards are in your graveyard, instead destroy that land and ~ deals 1 damage to each creature and each player. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if( has_threshold(player) ){
				new_damage_all(player, card, ANYBODY, 1, NDA_PLAYER_TOO | NDA_ALL_CREATURES, NULL);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
}

int card_restless_dreams(int player, int card, event_t event){
/* CARD_ID_RESTLESS_DREAMS	5871
Restless Dreams	|B
Sorcery
As an additional cost to cast ~, discard X cards.
Return X target creature cards from your graveyard to your hand. */

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}
	else if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		int max_cards = count_graveyard_by_type(player, TYPE_CREATURE);
		max_cards = MIN(max_cards, hand_count[player]);
		max_cards = MIN(max_cards, 19);

		if (max_cards <= 0 || graveyard_has_shroud(player)){
			ai_modifier -= 48;
			instance->info_slot = 0;
			return 0;
		}

		char buf[100];
		if (ai_is_speculating == 1){
			*buf = 0;
		} else if (max_cards == 1){
			strcpy(buf, "Select a creature card.");
		} else {
			sprintf(buf, "Select up to %d creature cards.", max_cards);
		}
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, buf);
		instance->info_slot = select_multiple_cards_from_graveyard(player, player, 0, AI_MAX_VALUE, &this_test, max_cards, &instance->targets[0]);

		if (instance->info_slot > 0){
			multidiscard(player, instance->info_slot, 0);
		}
	}
	else if (event == EVENT_RESOLVE_SPELL){
		int i;
		for (i = 0; i < instance->info_slot; ++i){
			int selected = validate_target_from_grave(player, card, player, i);
			if (selected != -1){
				from_grave_to_hand(player, selected, TUTOR_HAND);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

static int check_bf_for_permanent_with_the_same_name(int player, int card){
	CYCLE_ALL_CARDS(p, c, 	{
								if( in_play(p, c) && is_what(p, c, TYPE_PERMANENT) && has_same_name(player, card, p, c) ){
									return 1;
								}
							}
	);
	return 0;
}

int card_retraced_image(int player, int card, event_t event){
/* CARD_ID_RETRACED_IMAGE	5872
Retraced Image	|U
Sorcery
Reveal a card in your hand, then put that card onto the battlefield if it has the same name as a permanent. */

	if( event == EVENT_RESOLVE_SPELL ){
		int revealed = -1;
		if( player == AI ){
			int count = 0;
			int par = 0;
			while( count < active_cards_count[player] ){
					if( in_hand(player, count) && is_what(player, count, TYPE_PERMANENT) ){
						if( check_bf_for_permanent_with_the_same_name(player, count) ){
							if( get_cmc(player, count) > par ){
								par = get_cmc(player, count);
								revealed = count;
							}
						}
					}
					count++;
			}
		}
		else{
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a permanent card to reveal.");
			revealed = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_VALUE, -1, &this_test);
		}
		if( revealed != -1 ){
			reveal_card(player, card, player, revealed);
			if( check_bf_for_permanent_with_the_same_name(player, revealed) ){
				put_into_play(player, revealed);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static int remove_counters_at_eot(int player, int card, event_t event)
{
	if (eot_trigger(player, card, event))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  remove_counters(instance->damage_target_player, instance->damage_target_card, COUNTER_M1_M1, instance->eot_toughness);
	  kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_shambling_swarm(int player, int card, event_t event){

	/* Shambling Swarm	|1|B|B|B
	 * Creature - Horror 3/3
	 * When ~ dies, distribute three -1/-1 counters among one, two, or three target creatures. For each -1/-1 counter you put on a creature this way, remove a
	 * -1/-1 counter from that creature at the beginning of the next end step. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance= get_card_instance(player, card);

		int marked[2][151] = {{0}};

		int ssc;
		for(ssc = 0; ssc < 3 && can_target(&td); ++ssc){
			if (new_pick_target(&td, "TARGET_CREATURE", ssc, 0)){
				add_state(instance->targets[ssc].player, instance->targets[ssc].card, STATE_TARGETTED);
				marked[instance->targets[ssc].player][instance->targets[ssc].card]++;
			}
		}

		APNAP(p,{
					int c;
					for(c = 150; c > -1; c--){
						if( marked[p][c] ){
							int legacy = create_targetted_legacy_effect(player, card, &remove_counters_at_eot, p, c);
							get_card_instance(player, legacy)->eot_toughness = marked[p][c];
							add_counters(p, c, COUNTER_M1_M1, marked[p][c]);
						}
					}
				}
		);
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_sickening_dreams(int player, int card, event_t event){
/* CARD_ID_SICKENING_DREAMS	5876
Sickening Dreams	|1|B
Sorcery
As an additional cost to cast ~, discard X cards.
~ deals X damage to each creature and each player. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		new_damage_all(player, card, ANYBODY, get_card_instance(player, card)->info_slot, NDA_PLAYER_TOO | NDA_ALL_CREATURES, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return dream(player, card, event, 0);
}

int card_skullscorch(int player, int card, event_t event){
/* CARD_ID_SKULLSCORCH	5877
Skullscorch	|R|R
Sorcery
Target player discards two cards at random unless that player has ~ deal 4 damage to him or her. */


	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int choice = 0;
			int ai_choice = 1;
			if( life[instance->targets[0].player]-4 < 6 || hand_count[instance->targets[0].player] < 1 ){
				ai_choice = 0;
			}
			choice = do_dialog(instance->targets[0].player, player, card, -1, -1, " Discard 2 at random\n Take 4 damage", ai_choice);
			if( choice == 0 ){
				new_multidiscard(instance->targets[0].player, 2, DISC_RANDOM, player);
			}
			else{
				damage_player(instance->targets[0].player, 4, player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_sonic_seizures(int player, int card, event_t event){
	/* Sonic Seizure	|R	0x2008ff9
	 * Instant
	 * As an additional cost to cast this spell, discard a card at random.
	 * ~ deals 3 damage to any target. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST && hand_count[player] > 1 ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != 1 ){
			discard(player, DISC_RANDOM, player);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_strength_of_isolation(int player, int card, event_t event){
/* CARD_ID_STRENGTH_OF_ISOLATION	5884
Strength of Isolation	|1|W
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+2 and has protection from |Sblack.
Madness |W (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

	if( event == EVENT_CAN_PAY_MADNESS_COST || event == EVENT_PAY_MADNESS_COST ){
		return madness(player, card, event, MANACOST_W(1));
	}
	return generic_aura(player, card, event, player, 1, 2, get_sleighted_protection(player, card, KEYWORD_PROT_BLACK), 0, 0, 0, 0);
}

int card_strength_of_lunacy(int player, int card, event_t event){
/* CARD_ID_STRENGTH_OF_LUNACY	5885
Strength of Lunacy	|1|B
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+1 and has protection from |Swhite.
Madness |B (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

	if( event == EVENT_CAN_PAY_MADNESS_COST || event == EVENT_PAY_MADNESS_COST ){
		return madness(player, card, event, MANACOST_B(1));
	}
	return generic_aura(player, card, event, player, 2, 1, get_sleighted_protection(player, card, KEYWORD_PROT_WHITE), 0, 0, 0, 0);
}

int card_stupefying_touch(int player, int card, event_t event){
/* CARD_ID_STUPEFYING_TOUCH	5886
Stupefying Touch	|1|U
Enchantment - Aura
Enchant creature
When ~ enters the battlefield, draw a card.
Enchanted creature's activated abilities can't be activated. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	return disabling_targeted_aura(player, card, event, &td, "Select target creature.", 2);
}

static int tainted_land(int player, int card, event_t event, int color)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CHANGE_TYPE || event == EVENT_RESOLVE_SPELL)
	{
	  if (event == EVENT_CHANGE_TYPE && !(land_can_be_played & LCBP_DURING_EVENT_CHANGE_TYPE_SECOND_PASS))
		{
		  land_can_be_played |= LCBP_NEED_EVENT_CHANGE_TYPE_SECOND_PASS;	// delay until other cards have a change to change to or from swamp
		  return 0;
		}

	  if (check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_SWAMP))
		instance->info_slot = COLOR_TEST_COLORLESS|COLOR_TEST_BLACK|color;
	  else
		instance->info_slot = COLOR_TEST_COLORLESS;

	  if (event == EVENT_RESOLVE_SPELL)
		{
		  play_land_sound_effect_force_color(player, card, instance->info_slot);
		  return 0;	// so mana_producer() doesn't play the colorless sound
		}
	}

  return mana_producer(player, card, event);
}

int card_tainted_field(int player, int card, event_t event)
{
/* CARD_ID_TAINTED_FIELD	5887
Tainted Field	""
Land
|T: Add |C to your mana pool.
|T: Add |W or |B to your mana pool. Activate this ability only if you control |Ha Swamp. */

  return tainted_land(player, card, event, COLOR_TEST_WHITE);
}

int card_tainted_isle(int player, int card, event_t event)
{
/* CARD_ID_TAINTED_ISLE	5888
Tainted Isle	""
Land
|T: Add |C to your mana pool.
|T: Add |U or |B to your mana pool. Activate this ability only if you control |Ha Swamp. */

  return tainted_land(player, card, event, COLOR_TEST_BLUE);
}

int card_tainted_peak(int player, int card, event_t event)
{
/* CARD_ID_TAINTED_PEAK	5889
Tainted Peak	""
Land
|T: Add |C to your mana pool.
|T: Add |B or |R to your mana pool. Activate this ability only if you control |Ha Swamp. */

  return tainted_land(player, card, event, COLOR_TEST_RED);
}

int card_tainted_wood(int player, int card, event_t event)
{
/* CARD_ID_TAINTED_WOOD	5890
Tainted Wood	""
Land
|T: Add |C to your mana pool.
|T: Add |B or |G to your mana pool. Activate this ability only if you control |Ha Swamp. */

  return tainted_land(player, card, event, COLOR_TEST_GREEN);
}

int card_transcendence(int player, int card, event_t event){
/* CARD_ID_TRANSCENDENCE	5893
Transcendence	|3|W|W|W
Enchantment
You don't lose the game for having 0 or less life.
When you have 20 or more life, you lose the game.
Whenever you lose life, you gain 2 life for each 1 life you lost. (Damage dealt to you causes you to lose life.) */

	if( in_play(player, card) ){
		cannot_lose_the_game_for_having_less_than_0_life(player, card, event, player);
		if( life[player] >= 20 ){
			lose_the_game(player);
		}
	}
	return global_enchantment(player, card, event);
}

int card_turbulent_dreams(int player, int card, event_t event){
/* CARD_ID_TURBULENT_DREAMS	5894
Turbulent Dreams	|U|U
Sorcery
As an additional cost to cast ~, discard X cards.
Return X target nonland permanents to their owners' hands. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.illegal_type = TYPE_LAND;
	td.allow_cancel = 2; // "Done" button

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int max_targets = MIN(10, hand_count[player]);
		if( max_targets ){
			int i;
			for(i=0; i<max_targets && can_target(&td); i++){
				if( new_pick_target(&td, "Select target non-land permanent.", i, GS_LITERAL_PROMPT) ){
					state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
				}
				else{
					break;
				}
			}
			for(i=0; i<instance->number_of_targets; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			}
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a card to discard.");
			for(i=0; i<instance->number_of_targets; i++){
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &test);
				discard_card(player, selected);
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

int card_vengeful_dreams(int player, int card, event_t event){
/* CARD_ID_VENGEFUL_DREAMS	5896
Vengeful Dreams	|W|W
Instant
As an additional cost to cast ~, discard X cards.
Exile X target attacking creatures. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;
	td.allow_cancel = 2; // "Done" button

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int max_targets = MIN(10, hand_count[player]);
		if( max_targets ){
			int i;
			for(i=0; i<max_targets && can_target(&td); i++){
				if( new_pick_target(&td, "Select target attacking creature.", i, GS_LITERAL_PROMPT) ){
					state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
				}
				else{
					break;
				}
			}
			for(i=0; i<instance->number_of_targets; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			}
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a card to discard.");
			for(i=0; i<instance->number_of_targets; i++){
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &test);
				discard_card(player, selected);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_EXILE);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_violent_eruption(int player, int card, event_t event){
	/* Violent Eruption	|1|R|R|R	0x2009030
	 * Instant
	 * ~ deals 4 damage divided as you choose among any number of targets.
	 * Madness |1|R|R */
	if( event == EVENT_CAN_PAY_MADNESS_COST || event == EVENT_PAY_MADNESS_COST ){
		return madness(player, card, event, MANACOST_XR(1, 2));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card);
		int i;
		for(i=0; i<4; i++){
			if( validate_target(player, card, &td, i) ){
				damage_creature(instance->targets[i].player, instance->targets[i].card, 1, player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_ONE_OR_MULTIPLE_TARGETS, &td, "TARGET_ANY", 4, NULL);
}

