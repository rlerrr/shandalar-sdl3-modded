#include "manalink.h"

// Functions

static int revolt(int player){
	return get_trap_condition(player, TRAP_REVOLT);
}

static void av_free_spell(int player, int card, int max_cmc){
	if( hand_count[player] < 1 )
		return;

	test_definition_t test;
	default_test_definition(&test, TYPE_LAND);
	test.type_flag = DOESNT_MATCH;
	test.cmc = max_cmc + 1;
	test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
	int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_VALUE, -1, &test);
	if( selected > -1 )
		play_card_in_hand_for_free(player, selected);
}

static int improvise_impl(int player, int card, event_t event, int x_spell){

	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		td.allowed_controller = td.preferred_controller = player;
		td.illegal_state = TARGET_STATE_TAPPED;
		td.illegal_abilities = 0;
		COST_COLORLESS -= target_available(player, card, &td);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( x_spell )
			set_special_flags2(player, card, SF2_X_SPELL);
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			card_instance_t *instance = get_card_instance( player, card);
			instance->info_slot = 0;
			int choice = DIALOG(player, card, event, DLG_NO_STORAGE,
								"Use Improvise", 5, 1,
								"Play normally", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			int cless = get_updated_casting_cost(player, card, -1, event, -1);

			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_ARTIFACT);
			td.allowed_controller = td.preferred_controller = player;
			td.illegal_state = TARGET_STATE_TAPPED;
			td.illegal_abilities = 0;
			int arts = target_available(player, card, &td);

			int amount = cless - arts;

			int art_to_tap[15];
			int att = 0;
			int xv = 0;
			if( choice == 1 ){
				while( (amount || x_spell) && can_target(&td) ){
						if( new_pick_target(&td, "Select an artifact you control to tap.", 0, GS_LITERAL_PROMPT) ){
							art_to_tap[att] = instance->targets[0].card;
							att++;
							state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
							amount--;
							instance->number_of_targets = 0;
							if( ! amount )
								xv++;
						}
						else{
							break;
						}
				}
			}
			instance->number_of_targets = 0;
			if( amount )
				charge_mana(player, COLOR_COLORLESS, amount);
			if( x_spell ){
				charge_mana(player, COLOR_COLORLESS, -1);
				if( spell_fizzled != 1 )
					xv += x_value;
			}
			int i;
			for(i=0; i<att; i++){
				state_untargettable(player, art_to_tap[i], 0);
				if( spell_fizzled != 1 )
					tap_card(player, art_to_tap[i]);
			}
			if( x_spell ){
				set_x_for_x_spells(player, card, event, xv);
			}
			if( spell_fizzled == 1 )
				return 0;
			if( att )
				return 2;
			return 1;
		}
	}

	return 0;
}

static int improvise(int player, int card, event_t event){
	return improvise_impl(player, card, event, 0);
}

static int eot_revolt(int player, int card, event_t event){
	if( trigger_condition == TRIGGER_EOT && current_turn == player ){
		if( revolt(player) && eot_trigger(player, card, event) ){
			return 1;
		}
	}
	return 0;
}
// Cards

int card_aegis_automaton(int player, int card, event_t event){
/*
Aegis Automaton	|2
Artifact Creature - Construct 0/3
|4|W: Return another target creature you control to its owner's hand.
*/

	if( ! IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = player;
	td.special = TARGET_SPECIAL_NOT_ME;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
		bounce_permanent(instance->targets[0].player, instance->targets[0].card);

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XW(4, 1), 0,
									&td, "Select another target creature you control.");
}

static const char* is_creature_or_vehicle(int who_chooses, int player, int card){

	if( is_what(player, card, TYPE_CREATURE) || has_subtype(player, card, SUBTYPE_VEHICLE) )
		return NULL;

	return "must be a creature or a Vehicle.";
}

int card_aerial_modification(int player, int card, event_t event){
/*
Aerial Modification	|4|W
Enchantment - Aura
Enchant creature or Vehicle
As long as enchanted permanent is a Vehicle, it's a creature in addition to its other types.
Enchanted creature gets +2/+2 and has flying.
*/

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( ! affect_me(p, c) )
			return 0;

		switch( event ){
				case EVENT_POWER:
				case EVENT_TOUGHNESS:
					event_result += 2;
					break;
				case EVENT_ABILITIES:
					event_result |= KEYWORD_FLYING;
					break;
				default:
					break;
		}

		if( event == EVENT_CHANGE_TYPE && has_subtype(p, c, SUBTYPE_VEHICLE) ){
			if( ! is_what(-1, event_result, TYPE_CREATURE) ){
				if( instance->dummy3 < 1 ){
					int newtype = create_a_card_type(event_result);
					cards_at_7c7000[newtype]->type |= (cards_data[event_result].type | TYPE_CREATURE);
					instance->dummy3 = newtype;
					add_status(p, c, STATUS_ANIMATED);
				}
				event_result = instance->dummy3;
			}
			else{
				remove_status(p, c, STATUS_ANIMATED);
			}
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = (int32_t)is_creature_or_vehicle;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target creature or Vehicle.");
}

int card_aeronaut_admiral(int player, int card, event_t event){
/*
Aeronaut Admiral	|3|W
Creature - Human Pilot 3/1
Flying
Vehicles you control have flying.
*/
	return boost_creature_type(player, card, event, SUBTYPE_VEHICLE, 0, 0, KEYWORD_FLYING, BCT_CONTROLLER_ONLY);
}

int card_aether_chaser(int player, int card, event_t event){
/*
Aether Chaser	|1|R
Creature - Human Artificer 2/1
First strike
When ~ enters the battlefield, you get |E|E (two energy counters).
Whenever ~ attacks, you may pay |E|E. If you do, create a 1/1 colorless Servo artifact creature token.
*/
	// Used externally
	etb_energy(player, card, event, 2);
	if( attack_trigger_with_energy(player, card, event, 2) )
	    remove_energy(player, 2);
		make_servo(player, card, 1);
	return 0;
}

/*
Aether Herder	|3|G --> Aether Chaser
Creature - Elf Artificer Druid 3/3
When ~ enters the battlefield, you get |E|E (two energy counters).
Whenever ~ attacks, you may pay |E|E. If you do, create a 1/1 colorless Servo artifact creature token.
*/

/*
Aether Inspector	|3|W --> Aether Chaser
Creature - Dwarf Artificer 2/3
Vigilance
When ~ enters the battlefield, you get |E|E (two energy counters).
Whenever ~ attacks, you may pay |E|E. If you do, create a 1/1 colorless Servo artifact creature token.
*/

int card_aether_poisoner(int player, int card, event_t event){
/*
Aether Poisoner	|1|B
Creature - Human Artificer 1/1
Deathtouch (Any amount of damage this deals to a creature is enough to destroy it.)
When ~ enters the battlefield, you get |E|E (two energy counters).
Whenever ~ attacks, you may pay |E|E. If you do, create a 1/1 colorless Servo artifact creature token.
*/
	deathtouch(player, card, event);
	return card_aether_chaser(player, card, event);
}

/*
Aether Swooper	|1|U --> Aether Chaser
Creature - Vedalken Artificer 1/2
Flying
When ~ enters the battlefield, you get |E|E (two energy counters).
Whenever ~ attacks, you may pay |E|E. If you do, create a 1/1 colorless Servo artifact creature token.
*/


int card_aethergeode_miner(int player, int card, event_t event){
/*
Aethergeode Miner	|1|W
Creature - Dwarf Scout 3/1
Whenever ~ attacks, you get |E|E (two energy counters).
Pay |E|E: Exile ~, then return it to the battlefield under its owner's control.
*/

	if( declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		add_energy(player, 2);
	}

	if( ! IS_GAA_EVENT(event))
		return 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION )
		blink_effect(instance->parent_controller, instance->parent_card, NULL);

	return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 2), NULL, NULL);
}

int card_aethersphere_harvester(int player, int card, event_t event){
/*
Aethersphere Harvester	|3
Artifact - Vehicle 3/5
Flying
When ~ enters the battlefield, you get |E|E (two energy counters).
Pay |E: ~ gains lifelink until end of turn.
Crew 1 (Tap any number of creatures you control with total power 1 or more: This Vehicle becomes an artifact creature until end of turn.)
*/
	etb_energy(player, card, event, 2);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_LIFELINK = 1,
		CHOICE_CREW
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 1), NULL, NULL) )
			return 1;
		return vehicle(player, card, event, 1);
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int abils[3] = {	0,
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0,
							GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 1), NULL, NULL),
							vehicle(player, card, EVENT_CAN_ACTIVATE, 1)
		};

		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Lifelink", abils[1], current_phase == PHASE_AFTER_BLOCKING ? 10 : 1,
							"Crew", abils[2], 5);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_LIFELINK ){
			return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 1), NULL, NULL);
		}

		if( choice == CHOICE_CREW ){
			return vehicle(player, card, event, 1);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_LIFELINK ){
			return generic_shade(player, card, event, 0, MANACOST0, 0, 0, 0, SP_KEYWORD_LIFELINK);
		}

		if( instance->info_slot == CHOICE_CREW ){
			return vehicle(player, card, event, 1);
		}
	}

	return 0;
}


int card_aetherstream_leopard(int player, int card, event_t event){
/*
Aetherstream Leopard	|2|G
Creature - Cat 2/3
Trample
When ~ enters the battlefield, you get |E (an energy counter).
Whenever ~ attacks, you may pay |E. If you do, it gets +2/+0 until end of turn.
*/
	etb_energy(player, card, event, 1);
	if( attack_trigger_with_energy(player, card, event, 1) )
	    remove_energy(player, 1);
		pump_until_eot(player, card, player, card, 2, 0);
	return 0;
}

int card_aethertide_whale(int player, int card, event_t event){
/*
Aethertide Whale	|4|U|U
Creature - Whale 6/4
Flying
When ~ enters the battlefield, you get |E|E|E|E|E|E (six energy counters).
Pay |E|E|E|E: Return ~ to its owner's hand.
*/
	etb_energy(player, card, event, 6);

	if( ! IS_GAA_EVENT(event))
		return 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION )
		bounce_permanent(instance->parent_controller, instance->parent_card);

	return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 4), NULL, NULL);
}

int card_aetherwind_basket(int player, int card, event_t event){
/*
Aetherwind Basker	|4|G|G|G
Creature - Lizard 7/7
Trample
Whenever ~ enters the battlefield or attacks, you get |E (an energy counter) for each creature you control.
Pay |E: ~ gets +1/+1 until end of turn.
*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		add_energy(player, count_subtype(player, TYPE_CREATURE, -1));
	}

	if( event == EVENT_RESOLVE_ACTIVATION )
		return generic_shade(player, card, event, 0, MANACOST0, 1, 1, 0, 0);

	return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 1), NULL, NULL);
}

int card_aid_from_the_cowl(int player, int card, event_t event){
/*
Aid from the Cowl	|3|G|G
Enchantment
Revolt - At the beginning of your end step, if a permanent you controlled left the battlefield this turn,
	reveal the top card of your library. If it's a permanent card, you may put it onto the battlefield.
	Otherwise, you may put it on the bottom of your library.
*/

	if( eot_revolt(player, card, event) ){
		int iid = deck_ptr[player][0];
		if( iid > -1 ){
			int abils[4] = {0, is_what(-1, iid, TYPE_PERMANENT), 1, 1};
			int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_OMIT_ILLEGAL,
								DLG_SMALLCARD_ID(iid),
								"Put this permanent into play", abils[1], 10,
								"Put this card on bottom", abils[2], internal_rand(5),
								"Do nothing", abils[3], 1);
			if( choice == 1 )
				put_into_play_a_card_from_deck(player, player, 0);
			if( choice == 2 )
				put_top_card_of_deck_to_bottom(player);
		}
	}

	return global_enchantment(player, card, event);
}

int card_airdrop_aeronauts(int player, int card, event_t event){
/*
Airdrop Aeronauts	|3|W|W
Creature - Dwarf Scout 4/3
Flying
Revolt - When ~ enters the battlefield, if a permanent you controlled left the battlefield this turn, you gain 5 life.
*/

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && revolt(player) )
		etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 5);
	}

	return 0;
}

int card_ajani_unyielding(int player, int card, event_t event){
/*
Ajani Unyielding	|4|G|W
Planeswalker - Ajani (4)
+2: Reveal the top three cards of your library. Put all nonland permanent cards revealed this way into your hand and the rest on the
	bottom of your library in any order.
-2: Exile target creature. Its controller gains life equal to its power.
-9: Put five +1/+1 counters on each creature you control and five loyalty counters on each other planeswalker you control.
*/

	if( ! IS_GAA_EVENT(event) ){
		return planeswalker(player, card, event, 4);
	}

	enum{
		CHOICE_REVEAL = 1,
		CHOICE_EXILE,
		CHOICE_ULTIMATUM,
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;

		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
							"Reveal top 3", 					1, 					5, 		+2,
							"Produce RR", 						can_target(&td), 	10, 	-2,
							"Counters for all", 				1, 					20, 	-9);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_EXILE ){
			new_pick_target(&td, "TARGET_CREATURE", 0, 1);
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;

		if( choice == CHOICE_REVEAL ){
			int qty = MIN(3, count_deck(player));
			if( qty ){
				APNAP(p, {show_deck(p, deck_ptr[player], qty, "Cards revealed by Ajani Unyielding", 0, 0x7375B0 );};);
				int i;
				int remaining = qty;
				for(i=0; i<qty; i++){
					if( is_what(-1, deck_ptr[player][i], TYPE_PERMANENT) ){
						add_card_to_hand(player, deck_ptr[player][i]);
						remove_card_from_deck(player, i);
						remaining--;
					}
				}
				if( remaining )
					put_top_x_on_bottom(player, player, remaining);
			}
		}

		if( choice == CHOICE_EXILE && valid_target(&td) ){
			gain_life(instance->targets[0].player, get_power(instance->targets[0].player, instance->targets[0].card));
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}

		if( choice == CHOICE_ULTIMATUM ){
			int c = active_cards_count[player]-1;
			while( c > -1 ){
					if( in_play(player, c) ){
						if( is_what(player, c, TYPE_CREATURE) )
							add_counters(player, c, COUNTER_P1_P1, 5);
						if( is_planeswalker(player, c) && !(player == instance->parent_controller && c == instance->parent_card) )
							add_counters(player, c, COUNTER_LOYALTY, 5);
					}
					c--;
			}
		}
	}

	return planeswalker(player, card, event, 4);
}

int card_ajanis_aid(int player, int card, event_t event){
/*
Ajani's Aid	|2|G|W
Enchantment
When ~ enters the battlefield, you may search your library and/or graveyard for a card named Ajani, Valiant Protector, reveal it, and put it
	into your hand. If you search your library this way, shuffle it.
Sacrifice ~: Prevent all combat damage a creature of your choice would deal this turn.
*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a card named Ajani, Valiant Protector.");
		test.id = CARD_ID_AJANI_VALIANT_PROTECTOR;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &test);
	}

	if( ! IS_GAA_EVENT(event) )
		return global_enchantment(player, card, event);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION )
		maze_of_ith_effect(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME | GAA_LITERAL_PROMPT, MANACOST0,
									0, &td, "Select a creature.");
}

int card_ajanis_comrade(int player, int card, event_t event){
/*
Ajani's Comrade	|1|G
Creature - Elf Soldier 2/2
Trample
At the beginning of combat on your turn, if you control an Ajani planeswalker, put a +1/+1 counter on ~.
*/

	if( beginning_of_combat(player, card, event, player, card) ){
		int c = active_cards_count[player]-1;
		while( c > -1 ){
				if( in_play(player, c) ){
					if( is_planeswalker(player, c) && has_subtype(player, c, SUBTYPE_AJANI) )
						add_counters(player, c, COUNTER_P1_P1, 1);
				}
				c--;
		}
	}

	return 0;
}


int card_ajani_valiant_protector(int player, int card, event_t event){
/*
Ajani, Valiant Protector	|4|G|W
Planeswalker - Ajani (4)
+2: Put two +1/+1 counters on up to one target creature.
+1: Reveal cards from the top of your library until you reveal a creature card. Put that card into your hand and the rest on the bottom of
	your library in a random order.
-11: Put X +1/+1 counters on target creature, where X is your life total. That creature gains trample until end of turn.
*/

	if( ! IS_GAA_EVENT(event) ){
		return planeswalker(player, card, event, 4);
	}

	enum{
		CHOICE_COUNTERS = 1,
		CHOICE_REVEAL,
		CHOICE_MEGAPUMP,
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		instance->targets[0].player = instance->targets[0].card = -1;

		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
							"Add 2 +1/+1 counter", 				1, 	10, 			+2,
							"Reveal until you get a creature", 	1,					5, 				+1,
							"Megapump a creature", 				can_target(&td), 	life[player]*2, -11);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_COUNTERS ){
			td.allow_cancel = 2; //"Done" button
			new_pick_target(&td, "TARGET_CREATURE", 0, 0);
		}

		if( choice == CHOICE_MEGAPUMP ){
			new_pick_target(&td, "TARGET_CREATURE", 0, 1);
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;

		if( choice == CHOICE_COUNTERS ){
			if( instance->targets[0].player > -1 && valid_target(&td) )
				add_counters(instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 2);
		}

		if( choice == CHOICE_MEGAPUMP && valid_target(&td) ){
			add_counters(instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, life[player]);
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_TRAMPLE, 0);
		}

		if( choice == CHOICE_REVEAL ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			reveal_card_until_you_reveal_test(player, player, &this_test, RCUYRT_ADD_TEST_TO_HAND | RCUYRT_PUT_REST_ON_BOTTOM_RANDOM);
		}

	}

	return planeswalker(player, card, event, 4);
}

int card_alley_evasion(int player, int card, event_t event){
/*
Alley Evasion	|W
Instant
Choose one -
* Target creature you control gets +1/+2 until end of turn.
* Return target creature you control to its owner's hand.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum{
		CHOICE_PUMP = 1,
		CHOICE_BOUNCE,
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if(	event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Pump creature", 1, current_phase == PHASE_AFTER_BLOCKING ? 10 : 1,
								"Bounce creature", 1, 5);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature you control.", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int choice = instance->info_slot;

		if( choice == CHOICE_PUMP && valid_target(&td) )
			return vanilla_instant_pump(player, card, event, player, player, 1, 2, 0, 0);
		if( choice == CHOICE_BOUNCE && valid_target(&td) )
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

/*
Alley Strangler	|2|B	0x2009977
Creature - Aetherborn Rogue 2/3
Menace
*/

/*
Audacious Infiltrator	|1|W	0x200c28a
Creature - Dwarf Rogue 3/1
~ can't be blocked by artifact creatures.
*/

/*
Augmenting Automaton	|1	0x2007929
Artifact Creature - Construct 1/1
|1|B: ~ gets +1/+1 until end of turn.
*/


int card_barals_expertise(int player, int card, event_t event){
/*
Baral's Expertise	|3|U|U
Sorcery
Return up to three target artifacts and/or creatures to their owners' hands.
You may cast a card with converted mana cost 4 or less from your hand without paying its mana cost.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_ARTIFACT);
	td.allow_cancel = 2; //"Done" button

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		int validated = 0;
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				bounce_permanent(instance->targets[i].player, instance->targets[i].card);
				validated++;
			}
		}
		if( validated )
			av_free_spell(player, card, 4);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET | GS_LITERAL_PROMPT, &td, "Select target artifact or crearure.", 3, NULL);
}


int card_baral_chief_of_compliance(int player, int card, event_t event){
/*
Baral, Chief of Compliance	|1|U
Legendary Creature - Human Wizard 1/3
Instant and sorcery spells you cast cost |1 less to cast.
Whenever a spell or ability you control counters a spell, you may draw a card. If you do, discard a card.
*/
	check_legend_rule(player, card, event);

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card) ){
		if( is_what(affected_card_controller, affected_card, TYPE_SPELL) &&
			! is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
		{
			COST_COLORLESS--;
		}
	}

	return 0;
}

int card_barricade_breaker(int player, int card, event_t event){
/*
Barricade Breaker	|7
Artifact Creature - Juggernaut 7/5
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
~ attacks each combat if able.
*/
	attack_if_able(player, card, event);
	return improvise(player, card, event);
}

/*
Bastion Enforcer	|2|W	0x401000
Creature - Dwarf Soldier 3/2
*/


int card_bastion_inventor(int player, int card, event_t event){
/*
Bastion Inventor	|5|U
Creature - Vedalken Artificer 4/4
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
Hexproof (This creature can't be the target of spells or abilities your opponents control.)
*/
	hexproof(player, card, event);
	return improvise(player, card, event);
}

int card_battle_at_the_bridge(int player, int card, event_t event){
/*
Battle at the Bridge	|X|B
Sorcery
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
Target creature gets -X/-X until end of turn. You gain X life.
*/

	if( ! IS_GS_EVENT(player, card, event) && event != EVENT_MODIFY_COST )
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_MODIFY_COST )
		return improvise(player, card, event);

	if( event == EVENT_CAN_CAST )
		return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int val = improvise_impl(player, card, event, 1);
		if( val ){
			td.allow_cancel = val == 2 ? 0 : 1;
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int val = instance->info_slot;
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -val, -val);
			gain_life(player, val);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_call_for_unity(int player, int card, event_t event){
/*
Call for Unity	|3|W|W
Enchantment
Revolt - At the beginning of your end step, if a permanent you controlled left the battlefield this turn, put a unity counter on ~.
Creatures you control get +1/+1 for each unity counter on ~.
*/
	if( eot_revolt(player, card, event) ){
		add_counters(player, card, COUNTER_UNITY, 1);
	}

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		boost_creature_type(player, card, event, -1, 1, 1, 0, BCT_CONTROLLER_ONLY);
	}

	return global_enchantment(player, card, event);
}

int card_caught_in_the_brights(int player, int card, event_t event){
/*
Caught in the Brights	|2|W
Enchantment - Aura
Enchant creature
Enchanted creature can't attack or block.
When a Vehicle you control attacks, exile enchanted creature.
*/
	int amt;
	if ((amt = declare_attackers_trigger(player, card, event, DAT_TRACK, player, -1)) && ! is_humiliated(player, card) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		unsigned char* attackers = (unsigned char*)(&instance->targets[2].player);
		int exile = 0;
		for(--amt; amt >= 0; --amt){
			if (in_play(current_turn, attackers[amt]) && has_subtype(current_turn, attackers[amt], SUBTYPE_VEHICLE) ){
				exile = 1;
				break;
			}
		}
		if( exile )
			kill_card(instance->damage_target_player, instance->damage_target_card, KILL_EXILE);
	}

	return card_pacifism(player, card, event);
}

int card_chandras_revolution(int player, int card, event_t event){
/*
Chandra's Revolution	|3|R
Sorcery
~ deals 4 damage to target creature. Tap target land. That land doesn't untap during its controller's next untap step.
*/
	if( ! IS_GS_EVENT(player, card, event) )
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_LAND);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_CAST && can_target(&td) )
		return generic_spell(player, card, event, GS_CAN_TARGET, &td2, NULL, 1, NULL);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			new_pick_target(&td2, "TARGER_LAND", 1, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
		if( validate_target(player, card, &td2, 1) ){
			effect_frost_titan(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_cogwork_assembler(int player, int card, event_t event){
/*
Cogwork Assembler	|3
Artifact Creature - Assembly-Worker 2/3
|7: Create a token that's a copy of target artifact. That token gains haste. Exile it at the beginning of the next end step.
*/
	if( ! IS_GAA_EVENT(event) )
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			token_generation_t token;
			copy_token_definition(player, card, &token, instance->targets[0].player, instance->targets[0].card);
			token.legacy = 1;
			token.special_code_for_legacy = &haste_and_remove_eot;
			generate_token(&token);
		}
	}


	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(7), 0, &td, "TARGET_ARTIFACT");
}

int card_consulate_crackdown(int player, int card, event_t event){
/*
Consulate Crackdown	|3|W|W
Enchantment
When ~ enters the battlefield, exile all artifacts your opponents control until ~ leaves the battlefield.
*/
	if( comes_into_play(player, card, event) ){
		int c = active_cards_count[1-player]-1;
		while( c > -1 ){
				if( in_play(1-player, c) && is_what(1-player, c, TYPE_ENCHANTMENT) ){
					exile_card_and_remember_it_on_exiledby(player, card, 1-player, c);
				}
				c--;
		}
	}

	if( leaves_play(player, card, event) ){
		int leg = 0;
		int idx = 0;
		int* loc;
		while ((loc = exiledby_find_any(player, card, &leg, &idx)) != NULL){
				int owner = (*loc & 0x80000000) ? 1 : 0;
				int iid = *loc & ~0x80000000;
				*loc = -1;
				int card_added = add_card_to_hand(owner, iid);
				remove_card_from_rfg(owner, cards_data[iid].id);
				put_into_play(owner, card_added);
		}
	}

	return global_enchantment(player, card, event);
}

int card_consulate_dreadnought(int player, int card, event_t event){
/*
Consulate Dreadnought	|1
Artifact - Vehicle 7/11
Crew 6 (Tap any number of creatures you control with total power 6 or more: This Vehicle becomes an artifact creature until end of turn.)
*/
	return vehicle(player, card, event, 6);
}

/* Consulate Turret	|3
 * Artifact
 * |T: You get |E.
 * |T, Pay |E|E|E: ~ deals 2 damage to target player or planeswalker. */

/*
Countless Gears Renegade	|1|W
Creature - Dwarf Artificer 2/2
Revolt - When ~ enters the battlefield, if a permanent you controlled left the battlefield this turn, create a 1/1 colorless Servo artifact creature token.
*/

/*
Crackdown Construct	|4
Artifact Creature - Construct 2/2
Whenever you activate an ability of an artifact or creature that isn't a mana ability, ~ gets +1/+1 until end of turn.
*/

/*
Cruel Finality	|2|B
Instant
Target creature gets -2/-2 until end of turn. Scry 1. (Look at the top card of your library. You may put that card on the bottom of your library.)
*/

/*
Daredevil Dragster	|3
Artifact - Vehicle 4/4
At end of combat, if ~ attacked or blocked this combat, put a velocity counter on it. Then if it has two or more velocity counters on it, sacrifice it and draw two cards.
Crew 2 (Tap any number of creatures you control with total power 2 or more: This Vehicle becomes an artifact creature until end of turn.)
*/

/*
Daring Demolition	|2|B|B
Sorcery
Destroy target creature or Vehicle.
*/

/*
Dark Intimations	|2|U|B|R
Sorcery
Each opponent sacrifices a creature or planeswalker, then discards a card. You return a creature or planeswalker card from your graveyard to your hand, then draw a card.
When you cast a Bolas planeswalker spell, exile ~ from your graveyard. That planeswalker enters the battlefield with an additional loyalty counter on it.
*/

/*
Dawnfeather Eagle	|4|W
Creature - Bird 3/3
Flying
When ~ enters the battlefield, creatures you control get +1/+1 and gain vigilance until end of turn.
*/

/*
Deadeye Harpooner	|2|W
Creature - Dwarf Warrior 2/2
Revolt - When ~ enters the battlefield, if a permanent you controlled left the battlefield this turn, destroy target tapped creature an opponent controls.
*/

/*
Decommission	|2|W
Instant
Destroy target artifact or enchantment.
Revolt - If a permanent you controlled left the battlefield this turn, you gain 3 life.
*/

/*
Defiant Salvager	|2|B
Creature - Aetherborn Artificer 2/2
Sacrifice an artifact or creature: Put a +1/+1 counter on ~. Activate this ability only any time you could cast a sorcery.
*/

/*
Deft Dismissal	|3|W
Instant
~ deals 3 damage divided as you choose among one, two, or three target attacking or blocking creatures.
*/

/*
Destructive Tampering	|2|R
Sorcery
Choose one -
* Destroy target artifact.
* Creatures without flying can't block this turn.
*/

/*
Disallow	|1|U|U
Instant
Counter target spell, activated ability, or triggered ability. (Mana abilities can't be targeted.)
*/

/*
Dispersal Technician	|4|U
Creature - Vedalken Artificer 3/2
When ~ enters the battlefield, you may return target artifact to its owner's hand.
*/

/*
Druid of the Cowl	|1|G	=>m14.c:card_elvish_mystic 0x200abc9
Creature - Elf Druid 1/3
|T: Add |G to your mana pool.
*/

/*
Efficient Construction	|3|U
Enchantment
Whenever you cast an artifact spell, create a 1/1 colorless Thopter artifact creature token with flying.
*/

/*
Embraal Gear-Smasher	|2|R
Creature - Human Warrior 2/3
|T, Sacrifice an artifact: ~ deals 2 damage to each opponent.
*/

/*
Enraged Giant	|5|R
Creature - Giant 4/4
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
Trample, haste
*/

/*
Exquisite Archangel	|5|W|W
Creature - Angel 5/5
Flying
If you would lose the game, instead exile ~ and your life total becomes equal to your starting life total.
*/

/*
Fatal Push	|B
Instant
Destroy target creature if it has converted mana cost 2 or less.
Revolt - Destroy that creature if it has converted mana cost 4 or less instead if a permanent you controlled left the battlefield this turn.
*/

/*
Felidar Guardian	|3|W
Creature - Cat Beast 1/4
When ~ enters the battlefield, you may exile another target permanent you control, then return that card to the battlefield under its owner's control.
*/

/*
Fen Hauler	|6|B
Creature - Insect 5/5
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
~ can't be blocked by artifact creatures.
*/

/*
Filigree Crawler	|4
Artifact Creature - Insect 2/2
When ~ dies, create a 1/1 colorless Thopter artifact creature token with flying.
*/

/*
Foundry Assembler	|5
Artifact Creature - Assembly-Worker 3/3
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
*/

/*
Foundry Hornet	|3|B
Creature - Insect 2/3
Flying
When ~ enters the battlefield, if you control a creature with a +1/+1 counter on it, creatures your opponents control get -1/-1 until end of turn.
*/

/*
Fourth Bridge Prowler	|B
Creature - Human Rogue 1/1
When ~ enters the battlefield, you may have target creature get -1/-1 until end of turn.
*/

/*
Freejam Regent	|4|R|R
Creature - Dragon 4/4
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
Flying
|1|R: ~ gets +2/+0 until end of turn.
*/

/*
Frontline Rebel	|2|R	=>m10.c:card_berserkers_of_blood_ridge 0x20025c8
Creature - Human Warrior 3/3
~ attacks each combat if able.
*/

/*
Ghirapur Osprey	|2|W	0x401000
Creature - Bird 2/2
Flying
*/

/*
Gifted Aetherborn	|B|B	=>zendikar.c:card_vampire_nighthawk	0x2001afb
Creature - Aetherborn Vampire 2/3
Deathtouch, lifelink
*/

/*
Glint-Sleeve Siphoner	|1|B
Creature - Human Rogue 2/1
Menace
Whenever ~ enters the battlefield or attacks, you get |E (an energy counter).
At the beginning of your upkeep, you may pay |E|E. If you do, you draw a card and you lose 1 life.
*/

/*
Gonti's Aether Heart	|6
Legendary Artifact
Whenever ~ or another artifact enters the battlefield under your control, you get |E|E (two energy counters).
Pay |E|E|E|E|E|E|E|E, Exile ~: Take an extra turn after this one.
*/

/*
Gonti's Machinations	|B
Enchantment
Whenever you lose life for the first time each turn, you get |E. (You get an energy counter. Damage causes loss of life.)
Pay |E|E, Sacrifice ~: Each opponent loses 3 life. You gain life equal to the life lost this way.
*/

/*
Greenbelt Rampager	|G
Creature - Elephant 3/4
When ~ enters the battlefield, pay |E|E (two energy counters). If you can't, return ~ to its owner's hand and you get |E.
*/

/*
Greenwheel Liberator	|1|G
Creature - Elf Warrior 2/1
Revolt - ~ enters the battlefield with two +1/+1 counters on it if a permanent you controlled left the battlefield this turn.
*/

/*
Gremlin Infestation	|3|R
Enchantment - Aura
Enchant artifact
At the beginning of your end step, ~ deals 2 damage to enchanted artifact's controller.
When enchanted artifact is put into a graveyard, create a 2/2 |Sred Gremlin creature token.
*/

/*
Heart of Kiran	|2
Legendary Artifact - Vehicle 4/4
Flying, vigilance
Crew 3 (Tap any number of creatures you control with total power 3 or more: This Vehicle becomes an artifact creature until end of turn.)
You may remove a loyalty counter from a planeswalker you control rather than pay ~'s crew cost.
*/

/*
Herald of Anguish	|5|B|B
Creature - Demon 5/5
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
Flying
At the beginning of your end step, each opponent discards a card.
|1|B, Sacrifice an artifact: Target creature gets -2/-2 until end of turn.
*/

/*
Heroic Intervention	|1|G
Instant
Permanents you control gain hexproof and indestructible until end of turn.
*/

/*
Hidden Herbalists	|1|G
Creature - Human Druid 2/2
Revolt - When ~ enters the battlefield, if a permanent you controlled left the battlefield this turn, add |G|G to your mana pool.
*/

/*
Hidden Stockpile	|W|B
Enchantment
Revolt - At the beginning of your end step, if a permanent you controlled left the battlefield this turn, create a 1/1 colorless Servo artifact creature token.
|1, Sacrifice a creature: Scry 1.
*/

/*
Highspire Infusion	|1|G
Instant
Target creature gets +3/+3 until end of turn. You get |E|E (two energy counters).
*/

/*
Hinterland Drake	|2|U
Creature - Drake 2/3
Flying
~ can't block artifact creatures.
*/

/*
Hope of Ghirapur	|1
Legendary Artifact Creature - Thopter 1/1
Flying
Sacrifice ~: Until your next turn, target player who was dealt combat damage by ~ this turn can't cast noncreature spells.
*/

/* Hungry Flames	|2|R
 * Instant
 * ~ deals 3 damage to target creature and 2 damage to target player or planeswalker. */

/*
Ice Over	|1|U
Enchantment - Aura
Enchant artifact or creature
Enchanted permanent doesn't untap during its controller's untap step.
*/

/*
Illusionist's Stratagem	|3|U
Instant
Exile up to two target creatures you control, then return those cards to the battlefield under their owner's control.
Draw a card.
*/

/* Implement of Combustion	|1
 * Artifact
 * |R, Sacrifice ~: It deals 1 damage to target player or planeswalker.
 * When ~ is put into a graveyard from the battlefield, draw a card. */

/*
Implement of Examination	|3
Artifact
|U, Sacrifice ~: Draw a card.
When ~ is put into a graveyard from the battlefield, draw a card.
*/

/*
Implement of Ferocity	|1
Artifact
|G, Sacrifice ~: Put a +1/+1 counter on target creature. Activate this ability only any time you could cast a sorcery.
When ~ is put into a graveyard from the battlefield, draw a card.
*/

/*
Implement of Improvement	|1
Artifact
|W, Sacrifice ~: You gain 2 life.
When ~ is put into a graveyard from the battlefield, draw a card.
*/

/*
Implement of Malice	|2
Artifact
|B, Sacrifice ~: Target player discards a card. Activate this ability only any time you could cast a sorcery.
When ~ is put into a graveyard from the battlefield, draw a card.
*/

/*
Indomitable Creativity	|X|R|R|R
Sorcery
Destroy X target artifacts and/or creatures. For each permanent destroyed this way, its controller reveals cards from the top of his or her library until an artifact or creature card is revealed and exiles that card. Those players put the exiled cards onto the battlefield, then shuffle their libraries.
*/

/*
Inspiring Roar	|3|W
Sorcery
Put a +1/+1 counter on each creature you control.
*/

/*
Inspiring Statuary	|3
Artifact
Nonartifact spells you cast have improvise. (Your artifacts can help cast those spells. Each artifact you tap after you're done activating mana abilities pays for |1.)
*/

/*
Invigorated Rampage	|1|R
Instant
Choose one -
* Target creature gets +4/+0 and gains trample until end of turn.
* Two target creatures each get +2/+0 and gain trample until end of turn.
*/

/*
Ironclad Revolutionary	|4|B|B
Creature - Aetherborn Artificer 4/4
When ~ enters the battlefield, you may sacrifice an artifact. If you do, put two +1/+1 counters on ~ and each opponent loses 2 life.
*/

/*
Irontread Crusher	|4
Artifact - Vehicle 6/6
Crew 3 (Tap any number of creatures you control with total power 3 or more: This Vehicle becomes an artifact creature until end of turn.)
*/

/*
Kari Zev's Expertise	|1|R|R
Sorcery
Gain control of target creature or Vehicle until end of turn. Untap it. It gains haste until end of turn.
You may cast a card with converted mana cost 2 or less from your hand without paying its mana cost.
*/

/*
Kari Zev, Skyship Raider	|1|R
Legendary Creature - Human Pirate 1/3
First strike, menace
Whenever ~ attacks, create a legendary 2/1 |Sred Monkey creature token named Ragavan that's tapped and attacking. Exile that token at end of combat.
*/

/*
Lathnu Sailback	|4|R	0x401000
Creature - Lizard 5/4
*/

/*
Leave in the Dust	|3|U
Instant
Return target nonland permanent to its owner's hand.
Draw a card.
*/

/*
Lifecraft Awakening	|X|G
Instant
Put X +1/+1 counters on target artifact you control. If it isn't a creature or Vehicle, it becomes a 0/0 Construct artifact creature.
*/

/*
Lifecraft Cavalry	|4|G
Creature - Elf Warrior 4/4
Trample
Revolt - ~ enters the battlefield with two +1/+1 counters on it if a permanent you controlled left the battlefield this turn.
*/

/*
Lifecrafter's Bestiary	|3
Artifact
At the beginning of your upkeep, scry 1.
Whenever you cast a creature spell, you may pay |G. If you do, draw a card.
*/

/*
Lifecrafter's Gift	|3|G
Instant
Put a +1/+1 counter on target creature, then put a +1/+1 counter on each creature you control with a +1/+1 counter on it.
*/

/*
Lightning Runner	|3|R|R
Creature - Human Warrior 2/2
Double strike, haste
Whenever ~ attacks, you get |E|E (two energy counters), then you may pay |E|E|E|E|E|E|E|E. If you pay, untap all creatures you control, and after this phase, there is an additional combat phase.
*/

/*
Maulfist Revolutionary	|1|G|G
Creature - Human Warrior 3/3
Trample
When ~ enters the battlefield or dies, for each kind of counter on target permanent or player, give that permanent or player another counter of that kind.
*/

/*
Maverick Thopterist	|3|U|R
Creature - Human Artificer 2/2
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
When ~ enters the battlefield, create two 1/1 colorless Thopter artifact creature tokens with flying.
*/

/*
Mechanized Production	|2|U|U
Enchantment - Aura
Enchant artifact you control
At the beginning of your upkeep, create a token that's a copy of enchanted artifact. Then if you control eight or more artifacts with the same name as one another, you win the game.
*/

/*
Merchant's Dockhand	|1
Artifact Creature - Construct 1/2
|3|U, |T, Tap X untapped artifacts you control: Look at the top X cards of your library. Put one of them into your hand and the rest on the bottom of your library in any order.
*/

/*
Metallic Mimic	|2
Artifact Creature - Shapeshifter 2/1
As ~ enters the battlefield, choose a creature type.
~ is the chosen type in addition to its other types.
Each other creature you control of the chosen type enters the battlefield with an additional +1/+1 counter on it.
*/

/*
Metallic Rebuke	|2|U
Instant
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
Counter target spell unless its controller pays |3.
*/

/*
Midnight Entourage	|2|B|B
Creature - Aetherborn Rogue 3/3
Other Aetherborn you control get +1/+1.
Whenever ~ or another Aetherborn you control dies, you draw a card and you lose 1 life.
*/

/*
Mobile Garrison	|3
Artifact - Vehicle 3/4
Whenever ~ attacks, untap another target artifact or creature you control.
Crew 2 (Tap any number of creatures you control with total power 2 or more: This Vehicle becomes an artifact creature until end of turn.)
*/

/*
Monstrous Onslaught	|3|G|G
Sorcery
~ deals X damage divided as you choose among any number of target creatures, where X is the greatest power among creatures you control as you cast ~.
*/

/*
Narnam Renegade	|G
Creature - Elf Warrior 1/2
Deathtouch
Revolt - ~ enters the battlefield with a +1/+1 counter on it if a permanent you controlled left the battlefield this turn.
*/

/*
Natural Obsolescence	|1|G
Instant
Put target artifact on the bottom of its owner's library.
*/

/*
Night Market Aeronaut	|3|B
Creature - Aetherborn Warrior 2/2
Flying
Revolt - ~ enters the battlefield with a +1/+1 counter on it if a permanent you controlled left the battlefield this turn.
*/

/*
Night Market Guard	|3	=>unlimited.c:card_two_headed_giant_of_foriys 0x200b263
Artifact Creature - Construct 3/1
~ can block an additional creature each combat.
*/

/*
Oath of Ajani	|G|W
Legendary Enchantment
When ~ enters the battlefield, put a +1/+1 counter on each creature you control.
Planeswalker spells you cast cost |1 less to cast.
*/

/*
Outland Boar	|2|R|G
Creature - Boar 4/4
~ can't be blocked by creatures with power 2 or less.
*/

/*
Pacification Array	|1
Artifact
|2, |T: Tap target artifact or creature.
*/

/*
Paradox Engine	|5
Legendary Artifact
Whenever you cast a spell, untap all nonland permanents you control.
*/

/*
Peacewalker Colossus	|3
Artifact - Vehicle 6/6
|1|W: Another target Vehicle you control becomes an artifact creature until end of turn.
Crew 4 (Tap any number of creatures you control with total power 4 or more: This Vehicle becomes an artifact creature until end of turn.)
*/

/*
Peema Aether-Seer	|3|G
Creature - Elf Druid 3/2
When ~ enters the battlefield, you get an amount of |E (energy counters) equal to the greatest power among creatures you control.
Pay |E|E|E: Target creature blocks this turn if able.
*/

/*
Pendulum of Patterns	|2
Artifact
When ~ enters the battlefield, you gain 3 life.
|5, |T, Sacrifice ~: Draw a card.
*/

/*
Perilous Predicament	|4|B
Instant
Each opponent sacrifices an artifact creature and a nonartifact creature.
*/

/*
Pia's Revolution	|2|R
Enchantment
Whenever a nontoken artifact is put into your graveyard from the battlefield, return that card to your hand unless target opponent has ~ deal 3 damage to him or her.
*/

/*
Planar Bridge	|6
Legendary Artifact
|8, |T: Search your library for a permanent card, put it onto the battlefield, then shuffle your library.
*/

/*
Precise Strike	|R	=>m13.c:card_kindled_fury 0x20067b3
Instant
Target creature gets +1/+0 and gains first strike until end of turn.
*/

/*
Prizefighter Construct	|5	0x401000
Artifact Creature - Construct 6/2
*/

/* Quicksmith Rebel	|3|R
 * Creature - Human Artificer 3/2
 * When ~ enters the battlefield, target artifact you control gains "|T: This artifact deals 2 damage to any target" for as long as you control ~. */

/*
Quicksmith Spy	|3|U
Creature - Human Artificer 2/3
When ~ enters the battlefield, target artifact you control gains "|T: Draw a card" for as long as you control ~.
*/

/*
Ravenous Intruder	|1|R	=>antiquities.c:card_atog2 0x200c2a8
Creature - Gremlin 1/2
Sacrifice an artifact: ~ gets +2/+2 until end of turn.
*/

/*
Reckless Racer	|2|R
Creature - Human Pilot 2/3
First strike
Whenever ~ becomes tapped, you may discard a card. If you do, draw a card.
*/

/*
Release the Gremlins	|X|X|R
Sorcery
Destroy X target artifacts. Create X 2/2 |Sred Gremlin creature tokens.
*/

/*
Renegade Map	|1
Artifact
~ enters the battlefield tapped.
|T, Sacrifice ~: Search your library for a basic land card, reveal it, put it into your hand, then shuffle your library.
*/

/*
Renegade Rallier	|1|G|W
Creature - Human Warrior 3/2
Revolt - When ~ enters the battlefield, if a permanent you controlled left the battlefield this turn, return target permanent card with converted mana cost 2 or less from your graveyard to the battlefield.
*/

/*
Renegade Wheelsmith	|1|R|W
Creature - Dwarf Pilot 3/2
Whenever ~ becomes tapped, target creature can't block this turn.
*/

/*
Renegade's Getaway	|2|B
Instant
Target permanent gains indestructible until end of turn. Create a 1/1 colorless Servo artifact creature token. (Effects that say "destroy" don't destroy a permanent with indestructible, and if it's a creature, it can't be destroyed by damage.)
*/

/*
Reservoir Walker	|5
Artifact Creature - Construct 3/3
When ~ enters the battlefield, you gain 3 life and get |E|E|E (three energy counters).
*/

/*
Resourceful Return	|1|B
Sorcery
Return target creature card from your graveyard to your hand. If you control an artifact, draw a card.
*/

/*
Restoration Specialist	|1|W
Creature - Dwarf Artificer 2/1
|W, Sacrifice ~: Return up to one target artifact card and up to one target enchantment card from your graveyard to your hand.
*/

/*
Reverse Engineer	|3|U|U
Sorcery
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
Draw three cards.
*/

/*
Ridgescale Tusker	|3|G|G
Creature - Beast 5/5
When ~ enters the battlefield, put a +1/+1 counter on each other creature you control.
*/

/*
Rishkar's Expertise	|4|G|G
Sorcery
Draw cards equal to the greatest power among creatures you control.
You may cast a card with converted mana cost 5 or less from your hand without paying its mana cost.
*/

/*
Rishkar, Peema Renegade	|2|G
Legendary Creature - Elf Druid 2/2
When ~ enters the battlefield, put a +1/+1 counter on each of up to two target creatures.
Each creature you control with a counter on it has "|T: Add |G to your mana pool."
*/

/*
Rogue Refiner	|1|G|U
Creature - Human Rogue 3/2
When ~ enters the battlefield, draw a card and you get |E|E (two energy counters).
*/

/*
Salvage Scuttler	|4|U
Creature - Crab 4/4
Whenever ~ attacks, return an artifact you control to its owner's hand.
*/

/*
Scrap Trawler	|3
Artifact Creature - Construct 3/2
Whenever ~ or another artifact you control is put into a graveyard from the battlefield, return to your hand target artifact card in your graveyard with lesser converted mana cost.
*/

/*
Scrapper Champion	|3|R	=>kaladesh.c:card_thriving_ibex 0x200f16f
Creature - Human Artificer 2/2
Double strike (This creature deals both first-strike and regular combat damage.)
When ~ enters the battlefield, you get |E|E (two energy counters).
Whenever ~ attacks, you may pay |E|E. If you do, put a +1/+1 counter on it.
*/

/*
Scrounging Bandar	|1|G
Creature - Cat Monkey 0/0
~ enters the battlefield with two +1/+1 counters on it.
At the beginning of your upkeep, you may move any number of +1/+1 counters from ~ onto another target creature.
*/

/*
Secret Salvage	|3|B|B
Sorcery
Exile target nonland card from your graveyard. Search your library for any number of cards with the same name as that card, reveal them, and put them into your hand. Then shuffle your library.
*/

/*
Servo Schematic	|2
Artifact
When ~ enters the battlefield or is put into a graveyard from the battlefield, create a 1/1 colorless Servo artifact creature token.
*/

/*
Shielded Aether Thief	|1|U
Creature - Vedalken Rogue 0/4
Flash (You may cast this spell any time you could cast an instant.)
Whenever ~ blocks, you get |E (an energy counter).
|T, Pay |E|E|E: Draw a card.
*/

/*
Shipwreck Moray	|3|U
Creature - Fish 0/5
When ~ enters the battlefield, you get |E|E|E|E (four energy counters).
Pay |E: ~ gets +2/-2 until end of turn.
*/

/*
Siege Modification	|1|R|R
Enchantment - Aura
Enchant creature or Vehicle
As long as enchanted permanent is a Vehicle, it's a creature in addition to its other types.
Enchanted creature gets +3/+0 and has first strike.
*/

/*
Silkweaver Elite	|2|G
Creature - Elf Archer 2/2
Reach (This creature can block creatures with flying.)
Revolt - When ~ enters the battlefield, if a permanent you controlled left the battlefield this turn, draw a card.
*/

/*
Skyship Plunderer	|1|U
Creature - Human Pirate 2/1
Flying
Whenever ~ deals combat damage to a player, for each kind of counter on target permanent or player, give that permanent or player another counter of that kind.
*/

/*
Sly Requisitioner	|4|B
Creature - Human Artificer 2/2
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
Whenever a nontoken artifact you control is put into a graveyard from the battlefield, create a 1/1 colorless Servo artifact creature token.
*/

/*
Solemn Recruit	|1|W|W
Creature - Dwarf Warrior 2/2
Double strike
Revolt - At the beginning of your end step, if a permanent you controlled left the battlefield this turn, put a +1/+1 counter on ~.
*/

/*
Spire Patrol	|2|W|U	=>m15.c:card_frost_lynx 0x200c8c0
Creature - Human Soldier 3/2
Flying
When ~ enters the battlefield, tap target creature an opponent controls. That creature doesn't untap during its controller's next untap step.
*/

/*
Spire of Industry	""
Land
|T: Add |C to your mana pool.
|T, Pay 1 life: Add one mana of any color to your mana pool. Activate this ability only if you control an artifact.
*/

/*
Sram's Expertise	|2|W|W
Sorcery
Create three 1/1 colorless Servo artifact creature tokens.
You may cast a card with converted mana cost 3 or less from your hand without paying its mana cost.
*/

/*
Sram, Senior Edificer	|1|W
Legendary Creature - Dwarf Advisor 2/2
Whenever you cast an Aura, Equipment, or Vehicle spell, draw a card.
*/

/*
Sweatworks Brawler	|3|R
Creature - Human Artificer 3/3
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
Menace
*/

/*
Take into Custody	|U
Instant
Tap target creature. It doesn't untap during its controller's next untap step.
*/

/*
Tezzeret the Schemer	|2|U|B
Planeswalker - Tezzeret (5)
+1: Create a colorless artifact token named Etherium Cell with "|T, Sacrifice this artifact: Add one mana of any color to your mana pool."
-2: Target creature gets +X/-X until end of turn, where X is the number of artifacts you control.
-7: You get an emblem with "At the beginning of combat on your turn, target artifact you control becomes an artifact creature with base power and toughness 5/5."
*/

/*
Tezzeret the Schemer Emblem	""
Emblem - Tezzeret
At the beginning of combat on your turn, target artifact you control becomes an artifact creature with base power and toughness 5/5.
*/

/*
Tezzeret's Betrayal	|3|U|B
Sorcery
Destroy target creature. You may search your library and/or graveyard for a card named Tezzeret, Master of Metal, reveal it, and put it into your hand. If you search your library this way, shuffle it.
*/

/*
Tezzeret's Simulacrum	|3
Artifact Creature - Golem 2/3
|T: Target opponent loses 1 life. If you control a Tezzeret planeswalker, that player loses 3 life instead.
*/

/*
Tezzeret's Touch	|1|U|B
Enchantment - Aura
Enchant artifact
Enchanted artifact is a creature with base power and toughness 5/5 in addition to its other types.
When enchanted artifact is put into a graveyard, return that card to its owner's hand.
*/

/*
Tezzeret, Master of Metal	|4|U|B
Planeswalker - Tezzeret (5)
+1: Reveal cards from the top of your library until you reveal an artifact card. Put that card into your hand and the rest on the bottom of your library in a random order.
-3: Target opponent loses life equal to the number of artifacts you control.
-8: Gain control of all artifacts and creatures target opponent controls.
*/

/*
Thopter Arrest	|2|W
Enchantment
When ~ enters the battlefield, exile target artifact or creature an opponent controls until ~ leaves the battlefield.
*/

/*
Treasure Keeper	|4
Artifact Creature - Construct 3/3
When ~ dies, reveal cards from the top of your library until you reveal a nonland card with converted mana cost 3 or less. You may cast that card without paying its mana cost. Put all revealed cards not cast this way on the bottom of your library in a random order.
*/

/*
Trophy Mage	|2|U
Creature - Human Wizard 2/2
When ~ enters the battlefield, you may search your library for an artifact card with converted mana cost 3, reveal it, put it into your hand, then shuffle your library.
*/

/*
Unbridled Growth	|G
Enchantment - Aura
Enchant land
Enchanted land has "|T: Add one mana of any color to your mana pool."
Sacrifice ~: Draw a card.
*/

/*
Universal Solvent	|1
Artifact
|7, |T, Sacrifice ~: Destroy target permanent.
*/

/*
Untethered Express	|4
Artifact - Vehicle 4/4
Trample
Whenever ~ attacks, put a +1/+1 counter on it.
Crew 1 (Tap any number of creatures you control with total power 1 or more: This Vehicle becomes an artifact creature until end of turn.)
*/

/*
Vengeful Rebel	|2|B
Creature - Aetherborn Warrior 3/2
Revolt - When ~ enters the battlefield, if a permanent you controlled left the battlefield this turn, target creature an opponent controls gets -3/-3 until end of turn.
*/

/*
Verdant Automaton	|2
Artifact Creature - Construct 1/2
|3|G: Put a +1/+1 counter on ~.
*/

/* Walking Ballista	|X|X
 * Artifact Creature - Construct 0/0
 * ~ enters the battlefield with X +1/+1 counters on it.
 * |4: Put a +1/+1 counter on ~.
 * Remove a +1/+1 counter from ~: It deals 1 damage to any target. */

/*
Watchful Automaton	|3
Artifact Creature - Construct 2/2
|2|U: Scry 1. (Look at the top card of your library. You may put that card on the bottom of your library.)
*/

/*
Welder Automaton	|2
Artifact Creature - Construct 2/1
|3|R: ~ deals 1 damage to each opponent.
*/

/*
Weldfast Engineer	|1|B|R
Creature - Human Artificer 3/3
At the beginning of combat on your turn, target artifact creature you control gets +2/+0 until end of turn.
*/

/*
Whir of Invention	|X|U|U|U
Instant
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
Search your library for an artifact card with converted mana cost X or less, put it onto the battlefield, then shuffle your library.
*/

/*
Wind-Kin Raiders	|4|U|U
Creature - Human Artificer 4/3
Improvise (Your artifacts can help cast this spell. Each artifact you tap after you're done activating mana abilities pays for |1.)
Flying
*/

/*
Winding Constrictor	|B|G
Creature - Snake 2/3
If one or more counters would be placed on an artifact or creature you control, that many plus one of each of those kinds of counters are placed on that permanent instead.
If you would get one or more counters, you get that many plus one of each of those kinds of counters instead.
*/

/*
Wrangle	|1|R
Sorcery
Gain control of target creature with power 4 or less until end of turn. Untap that creature. It gains haste until end of turn.
*/

/*
Yahenni's Expertise	|2|B|B
Sorcery
All creatures get -3/-3 until end of turn.
You may cast a card with converted mana cost 3 or less from your hand without paying its mana cost.
*/

/*
Yahenni, Undying Partisan	|2|B
Legendary Creature - Aetherborn Vampire 2/2
Haste
Whenever a creature an opponent controls dies, put a +1/+1 counter on ~.
Sacrifice another creature: ~ gains indestructible until end of turn.
*/
