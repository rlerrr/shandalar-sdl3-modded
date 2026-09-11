#include "manalink.h"
// functions
void add_spore_counters(int player, int card, event_t event){
	upkeep_trigger_ability(player, card, event, player);
	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counter(player, card, COUNTER_SPORE);
	}
}

static int fe_knights(int player, int card, event_t event, int mana_color){

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		int c[6] = {0};
		c[mana_color] = 2;
		return generic_shade(player, card, event, 0, c[0], c[1], c[2], c[3], c[4], c[5], 1, 0, 0, 0);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_FIRST_STRIKE = 1,
		CHOICE_PUMP,
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		int c[6] = {0};
		c[mana_color] = 1;
		return generic_activated_ability(player, card, event, 0, c[0], c[1], c[2], c[3], c[4], c[5], 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, 1, 0};
		int c[6] = {0};
		c[mana_color] = 2;
		if( has_mana_for_activated_ability(player, card, c[0], c[1], c[2], c[3], c[4], c[5]) ){
			abils[2] = 1;
		}
		int priorities[3] = {
							0,
							(instance->targets[1].player > -1 && !(instance->targets[1].player & KEYWORD_FIRST_STRIKE)) &&
							current_phase < PHASE_DECLARE_BLOCKERS ? 10 : 1,
							current_phase == PHASE_AFTER_BLOCKING ? 5 : 1,
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Gains First Strike", abils[1], priorities[1],
							"+1/+0 until EOT", abils[2], priorities[2]);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		c[mana_color] = choice;
		if( charge_mana_for_activated_ability(player, card, c[0], c[1], c[2], c[3], c[4], c[5]) ){
			if( instance->targets[1].player < 0 ){
				instance->targets[1].player = 0;
			}
			instance->targets[1].player |= (choice == CHOICE_FIRST_STRIKE ? KEYWORD_FIRST_STRIKE : 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		int pp = instance->parent_controller, pc = instance->parent_card;
		if( instance->info_slot == CHOICE_FIRST_STRIKE ){
			alternate_legacy_text(2, pp, pump_ability_until_eot(pp, pc, pp, pc, 0,0, KEYWORD_FIRST_STRIKE,0));
		}
		if( instance->info_slot == CHOICE_PUMP){
			alternate_legacy_text(1, pp, pump_until_eot_merge_previous(pp, pc, pp, pc, 1,0));
		}
	}

	return 0;
}

int fe_chant(int player, int card, event_t event, int land_subtype){

	basic_upkeep(player, card, event, MANACOST_BU((land_subtype == SUBTYPE_FOREST), (land_subtype == SUBTYPE_SWAMP)));

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.subtype = get_hacked_subtype(player, card, land_subtype);

		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = trigger_cause_controller;

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = p;
		td.preferred_controller = p;
		td.illegal_abilities = 0;
		td.who_chooses = p;
		td.allow_cancel = 0;

		enum{
			CHOICE_M1_M1_COUNTER = 1,
			CHOICE_3_DAMAGES
		};

		int choice = CHOICE_3_DAMAGES;
		if( can_target( &td ) ){
			choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
							"Add a -1/-1 counter", 	1, life[player]-3 <= 6 ? 10 : 1,
							"Take 3 damage", 		1, 5);
		}

		if( choice == CHOICE_M1_M1_COUNTER ){
			if( new_pick_target(&td, "Select a creature you control", 0, GS_LITERAL_PROMPT) ){
				add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, 1);
			}
			instance->number_of_targets = 0;
		}
		if( choice == CHOICE_3_DAMAGES ){
			damage_player(p, 3, player, card);
		}
	}
	return global_enchantment(player, card, event);
}

// cards
// white
int card_combat_medic(int player, int card, event_t event){
	/* Combat Medic	|2|W	0x2001e93
	 * Creature - Human Cleric Soldier 0/2
	 * |1|W: Prevent the next 1 damage that would be dealt to any target this turn. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
		}
	}
	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_XW(1, 1), 0, &td, "TARGET_DAMAGE");
}

int card_farrelite_priest(int player, int card, event_t event){
/*
Farrelite Priest |1|W|W
Creature — Human Cleric 1/3
{1}: Add {W} to your mana pool. If this ability has been activated four or more times this turn, sacrifice Farrelite Priest
		at the beginning of the next end step.
*/

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) ){
		return has_mana_for_activated_ability(player, card, MANACOST_X(1));
	}

	if( event == EVENT_ACTIVATE ){
		charge_mana_multi(player, MANACOST_X(1));
		if( spell_fizzled != 1 ){
			card_instance_t *instance = get_card_instance(player, card);
			produce_mana(player, COLOR_WHITE, 1);
			if( instance->info_slot < 0 ){ instance->info_slot = 0; }
			instance->info_slot++;
		}
	}

	if( trigger_condition == TRIGGER_EOT && affect_me(player, card) ){
		if(eot_trigger(player, card, event)){
			if( get_card_instance(player, card)->info_slot >= 4 && ! check_state(player, card, STATE_OUBLIETTED) &&
			        ! is_humiliated(player, card) )
			{
				kill_card(player, card, KILL_SACRIFICE);
			}
			get_card_instance(player, card)->info_slot = 0;
		}
	}

	return 0;
}

int card_farrels_mantle(int player, int card, event_t event){
/*
Farrel's Mantle |2|W
Enchantment — Aura
Enchant creature
Whenever enchanted creature attacks and isn't blocked, its controller may have it deal damage equal to its power plus 2
	to another target creature. If that player does, the attacking creature assigns no combat damage this turn.
*/

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( is_attacking(p, c) && event == EVENT_DECLARE_BLOCKERS && ! is_humiliated(player, card) ){
			if( is_unblocked(p, c) ){
				target_definition_t td;
				default_target_definition(p, c, &td, TYPE_CREATURE);
				td.who_chooses = p;
				td.special = TARGET_SPECIAL_NOT_ME;
				int will_use_ability = (life[1-player] - get_power(p, c)) > 6 ? 0 : 1;
				if( can_target(&td) && do_dialog(p, player, card, -1, -1, " Damage creature\n Pass", 1) == will_use_ability ){
					if( pick_target(&td, "TARGET_CREATURE") ){
						damage_target0(p, c, get_power(p, c) + 2);
						negate_combat_damage_this_turn(player, card, p, c, 0);
					}
					get_card_instance(p, c)->number_of_targets = 0;
				}
			}
		}
	}

	return vanilla_aura(player, card, event, player);
}

int card_farrels_zealot(int player, int card, event_t event){
/* CARD_ID_FARRELS_ZEALOT	1777
Farrel's Zealot	|1|W|W
Creature - Human 2/2
Whenever ~ attacks and isn't blocked, you may have it deal 3 damage to target creature. If you do, ~ assigns no combat damage this turn. */

	if( event == EVENT_DECLARE_BLOCKERS ){
		if( is_unblocked(player, card) && ! is_humiliated(player, card) ){
			int choice = do_dialog(player, player, card, -1, -1, " Damage creature\n Pass", 1);
			if( choice == 0 ){
				target_definition_t td;
				default_target_definition(player, card, &td, TYPE_CREATURE);
				if( pick_target(&td, "TARGET_CREATURE") ){
					damage_target0(player, card, 3);
					negate_combat_damage_this_turn(player, card, player, card, 0);
				}
			}
			get_card_instance(player, card)->number_of_targets = 0;
		}
	}
	return 0;
}

int card_hand_of_justice(int player, int card, event_t event){
	/* Hand of Justice	|5|W
	 * Creature - Avatar 2/6
	 * |T, Tap three untapped |Swhite creatures you control: Destroy target creature. */

	if (!IS_ACTIVATING(event)){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	
	target_definition_t td;
	base_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
	td.illegal_state = TARGET_STATE_TAPPED;
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_NOT_ME;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td1, NULL) ){
			return target_available(player, card, &td) >= 3;
		}
	}
			
	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
            int found = 0;
			CYCLE_ALL_CARDS(p, c, 	{
										if( p == player && in_play(p, c) && c != card ){
											if( is_what(p, c, TYPE_CREATURE) && ! is_tapped(p, c) &&
												!( get_color(p, c) & get_sleighted_color_test(p, c, COLOR_TEST_WHITE)) )
											{
												found++;
												if( found == 3 ){
													return 1;
												}
											}
										}
									}
			);


			instance->number_of_targets = 0;

			int to_tap[3];
			int ttc = 0;
			while( ttc < 3 ){
					char buffer[100];
					strcpy(buffer, get_sleighted_color_text(player, card, "Select an untapped %s creature.", COLOR_WHITE));
					if( new_pick_target(&td, buffer, 0, 1 | GS_LITERAL_PROMPT) ){
						to_tap[ttc] = instance->targets[0].card;
						state_untargettable(player, to_tap[ttc], 1);
						ttc++;
						instance->number_of_targets = 0;
					}
					else{
						break;
					}
			}
			int i;
			for(i=0; i<ttc; i++){
				state_untargettable(player, to_tap[i], 0);
			}
			if( ttc < 3 ){
				spell_fizzled = 1;
				return 0;
			}
			if( pick_target(&td1, "TARGET_CREATURE") ){
				for(i=0; i<ttc; i++){
					tap_card(player, to_tap[i]);
				}
				add_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td1) ){
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}

	return 0;
}

int card_heroism(int player, int card, event_t event){
/* Heroism |2|W
Enchantment
Sacrifice a white creature: For each attacking red creature, prevent all combat damage that would be dealt by that creature this turn
unless its controller pays {2}{R}. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);
	test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
	scnprintf(test.message, 100, "%s", get_sleighted_color_text(player, card, "Select a %s creature to sacrifice", COLOR_WHITE));

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		if( current_turn == 1-player && current_phase == PHASE_AFTER_BLOCKING ){
			test_definition_t test2;
			default_test_definition(&test2, TYPE_CREATURE);
			test2.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
			test2.state = STATE_ATTACKING;

			int amount = check_battlefield_for_special_card(player, card, current_turn, CBFSC_GET_COUNT, &test2);

			ai_modifier += (amount*5);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int count = active_cards_count[1-player]-1;
		while( count > -1 ){
				if( in_play(current_turn, count) && is_what(current_turn, count, TYPE_CREATURE) && is_attacking(current_turn, count) ){
					if( get_color(current_turn, count) & get_sleighted_color_test(player, card, COLOR_TEST_RED) ){
						int choice = 1;
						if( has_mana_multi(current_turn, MANACOST_XR(2, 1)) ){
							if( do_dialog(current_turn, player, card, -1, -1, " Pay 2R\n Do not pay", 0) == 0 ){
								charge_mana_multi(current_turn, MANACOST_XR(2, 1));
								if( spell_fizzled != 1 ){
									choice = 0;
								}
							}
						}
						if( choice == 1 ){
							negate_combat_damage_this_turn(instance->parent_controller, instance->parent_card, current_turn, count, 0);
						}
					}
				}
				count--;
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, 0);
}

int card_hymn_to_tourach(int player, int card, event_t event){
   /* Hymn to Tourach	|B|B
   * Sorcery
   * Target player discards two cards at random. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		new_multidiscard(get_card_instance(player, card)->targets[0].player, 2, DISC_RANDOM, player);

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_icatian_infantry(int player, int card, event_t event){
/* CARD_ID_ICATIAN_INFANTRY	1796
Icatian Infantry	|W
Creature - Human Soldier 1/1
|1: ~ gains first strike until end of turn.
|1: ~ gains banding until end of turn. */

	if( event == EVENT_CAN_ACTIVATE ){
		generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE && can_use_activated_abilities(player, card)  ){
		card_instance_t *instance = get_card_instance(player, card);
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			int ai_choice = 0;
			if( check_for_ability(player, card, KEYWORD_FIRST_STRIKE) && current_phase < PHASE_DECLARE_ATTACKERS ){
				ai_choice = 1;
				int choice = do_dialog(player, player, card, -1, -1, " First Strike\n Banding\n Cancel", ai_choice);
				if( choice == 2 ){
					spell_fizzled = 1;
					return 0;
				}
				instance->info_slot = 1+choice;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->info_slot == 1 ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									0, 0, KEYWORD_FIRST_STRIKE, 0);
		}
		if( instance->info_slot == 2 ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									0, 0, KEYWORD_BANDING, 0);
		}
	}

	return 0;
}

int card_icatian_javelineers(int player, int card, event_t event){
	/* Icatian Javelineers	|W	0x2001eb6
	 * Creature - Human Soldier 1/1
	 * ~ enters the battlefield with a javelin counter on it.
	 * |T, Remove a javelin counter from ~: ~ deals 1 damage to any target. */
	enters_the_battlefield_with_counters(player, card, event, COUNTER_JAVELIN, 1);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_JAVELIN), &td, "TARGET_ANY");
}

int card_icatian_lieutenant(int player, int card, event_t event){
/* CARD_ID_ICATIAN_LIEUTENANT	1798
Icatian Lieutenant	|W|W
Creature - Human Soldier 1/2
|1|W: Target Soldier creature gets +1/+0 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_SOLDIER;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if(valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XW(1, 1), 0,
									&td, "Select target Soldier creature.");
}

int card_icatian_moneychanger(int player, int card, event_t event){
/* CARD_ID_ICATIAN_MONEYCHANGER	1799
Icatian Moneychanger	|W
Creature - Human 0/2
~ enters the battlefield with three credit counters on it.
When ~ enters the battlefield, it deals 3 damage to you.
At the beginning of your upkeep, put a credit counter on ~.
Sacrifice ~: You gain 1 life for each credit counter on ~. Activate this ability only during your upkeep. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_CREDIT, 3);

	// When ~ enters the battlefield, it deals 3 damage to you.
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		damage_player(player, 3, player, card);
	}

	// At the beginning of your upkeep, put a credit counter on ~.
	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counter(player, card, COUNTER_CREDIT);
	}

	// Sacrifice ~: You gain 1 life for each credit counter on ~. Activate this ability only during your upkeep.
	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, count_counters(player, card, COUNTER_CREDIT) );
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_ONLY_ON_UPKEEP, MANACOST0, 0, NULL, NULL);
}

int card_icatian_priest(int player, int card, event_t event){
/* CARD_ID_ICATIAN_PRIEST	1801
Icatian Priest	|W
Creature - Human Cleric 1/1
|1|W|W: Target creature gets +1/+1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if(valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 1);
		}
	}
	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XW(1, 2), 0, &td, "TARGET_CREATURE");
}

int card_icatian_scout(int player, int card, event_t event){
/* CARD_ID_ICATIAN_SCOUT	1802
Icatian Scout	|W
Creature - Human Soldier Scout 1/1
|1, |T: Target creature gains first strike until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if(valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FIRST_STRIKE, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_X(1), 0, &td, "TARGET_CREATURE");
}

int card_icatian_skirmishers(int player, int card, event_t event){
/* CARD_ID_ICATIAN_SKIRMISHERS	1803
Icatian Skirmishers	|3|W
Creature - Human Soldier 1/1
First strike; banding
Whenever ~ attacks, all creatures banded with it gain first strike until end of turn. */

  if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  card_instance_t* instance = in_play(player, card), *aff;
	  if (!instance)
		return 0;

	  int c, band = instance->blocking;
	  if (band == 255)	// not banded
		return 0;

	  for (c = 0; c < active_cards_count[player]; ++c)
		if ((aff = in_play_and_attacking(player, c)) && aff->blocking == band)
		  pump_ability_until_eot(player, card, player, c, 0,0, KEYWORD_FIRST_STRIKE,0);
	}

	return 0;
}

int card_icatian_town(int player, int card, event_t event){
/* CARD_ID_ICATIAN_TOWN	1447
Icatian Town	|5|W
Sorcery
Create four 1/1 |Swhite Citizen creature tokens. */

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_CITIZEN, &token);
		token.pow = 1;
		token.tou = 1;
		token.qty = 4;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		generate_token(&token);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_order_of_leitbur(int player, int card, event_t event){
/* CARD_ID_ORDER_OF_LEITBUR	1555
Order of Leitbur	|W|W
Creature - Human Cleric Knight 2/1
Protection from |Sblack
|W: ~ gains first strike until end of turn.
|W|W: ~ gets +1/+0 until end of turn. */

	return fe_knights(player, card, event, COLOR_WHITE);
}


// blue cards
int card_homarid_warrior(int player, int card, event_t event){
/* CARD_ID_HOMARID_WARRIOR	1795
Homarid Warrior	|4|U
Creature - Homarid Warrior 3/3
|U: ~ gains shroud until end of turn and doesn't untap during your next untap step. Tap ~. */

	// Used externally
	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								0, 0, KEYWORD_SHROUD, 0 );
		if (in_play(instance->parent_controller, instance->parent_card) ){
			tap_card(instance->parent_controller, instance->parent_card);
			does_not_untap_effect(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									EDNT_CHECK_ORIGINAL_CONTROLLER, 1);
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_U(1), 0, NULL, NULL);
}

int card_deep_spawn(int player, int card, event_t event){
/* CARD_ID_DEEP_SPAWN	1758
Deep Spawn	|5|U|U|U
Creature - Homarid 6/6
Trample
At the beginning of your upkeep, sacrifice ~ unless you put the top two cards of your library into your graveyard.
|U: ~ gains shroud until end of turn and doesn't untap during your next untap step. Tap ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int kill = 1;
		int ai_choice = 0;
		if( count_deck(player) < 10 ){
			ai_choice = 1;
		}
		int choice = do_dialog(player, player, card, -1, -1, " Mill 2\n Sacrifice", ai_choice);
		if( choice == 0 ){
			mill(player, 2);
			kill = 0;
		}
		if( kill == 1 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return card_homarid_warrior(player, card, event);
}

static int effect_high_tide(int player, int card, event_t event){

	produce_mana_when_subtype_is_tapped(2, event, TYPE_PERMANENT, SUBTYPE_ISLAND, COLOR_BLUE, 1);

	if( eot_trigger(player, card, event ) ){
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_high_tide(int player, int card, event_t event){
/* CARD_ID_HIGH_TIDE	1790
High Tide	|U
Instant
Until end of turn, whenever a player taps |Han Island for mana, that player adds |U to his or her mana pool (in addition to the mana the land produces). */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_LAND, SUBTYPE_ISLAND)*5);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, &effect_high_tide );
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_homarid(int player, int card, event_t event){

	/* Homarid	|2|U
	 * Creature - Homarid 2/2
	 * ~ enters the battlefield with a tide counter on it.
	 * At the beginning of your upkeep, put a tide counter on ~.
	 * As long as there is exactly one tide counter on ~, it gets -1/-1.
	 * As long as there are exactly three tide counters on ~, it gets +1/+1.
	 * Whenever there are four tide counters on ~, remove all tide counters from it. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_TIDE, 1);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counter(player, card, COUNTER_TIDE);
	}

	if( affect_me(player, card) && ( event == EVENT_POWER || event == EVENT_TOUGHNESS) && ! is_humiliated(player, card) ){
		int counters = count_counters(player, card, COUNTER_TIDE);
		if( counters == 1 ){
			event_result--;
		}
		else if( counters == 3 ){
			event_result++;
		}
	}

	if (event == EVENT_STATIC_EFFECTS && count_counters(player, card, COUNTER_TIDE) >= 4 && ! is_humiliated(player, card) ){
		remove_all_counters(player, card, COUNTER_TIDE);
	}
	return 0;
}

int card_homarid_shaman(int player, int card, event_t event){
/* CARD_ID_HOMARID_SHAMAN	1793
Homarid Shaman	|2|U|U
Creature - Homarid Shaman 2/1
|U: Tap target |Sgreen creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target( &td ) ){
			tap_card(instance->targets[0].player, instance->targets[0].card );
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_U(1), 0,
									&td, get_sleighted_color_text(player, card, "Select target %s creature.", COLOR_GREEN));
}

int card_homarid_spawning_bed(int player, int card, event_t event){
/* CARD_ID_HOMARID_SPAWNING_BED	1794
Homarid Spawning Bed	|U|U
Enchantment
|1|U|U, Sacrifice a |Sblue creature: Create X 1/1 |Sblue Camarid creature tokens, where X is the sacrificed creature's converted mana cost. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);
	test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
	scnprintf(test.message, 100, "%s", get_sleighted_color_text(player, card, "Select a %s creature to sacrifice", COLOR_BLUE));

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_CAMARID, &token);
		token.pow = token.tou = 1;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		token.qty = get_card_instance(player, card)->info_slot;
		generate_token(&token);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST_XU(1, 2), 0, NULL, NULL,
															0, &test, SARV_REPORT_CMC);
}

int merseine_ability(int player, int card, event_t event){
	if( get_card_instance(player, card)->targets[0].player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int pl = instance->targets[0].player;
		int cr = instance->targets[0].card;

		card_instance_t *mers = get_card_instance(pl, cr);
		int enc_p = mers->damage_target_player;
		int enc_c = mers->damage_target_card;

		if( event == EVENT_CAN_ACTIVATE && count_counters(pl, cr, COUNTER_NET) && enc_p == player ){
			if( can_use_activated_abilities(pl, cr) ){
				card_ptr_t* c = cards_ptr[get_id(enc_p, enc_c)];
				int cless = c->req_colorless;
				if( check_special_flags2(player, card, SF2_X_SPELL) ){
					cless += get_card_instance(enc_p, enc_c)->info_slot;
				}
				int c1 = get_cost_mod_for_activated_abilities(pl, cr, cless, c->req_black, c->req_blue, c->req_green, c->req_red, c->req_white);
				if( has_mana_multi(player, c1, c->req_black, c->req_blue, c->req_green, c->req_red, c->req_white) ){
					return 1;
				}
			}
		}

		if( event == EVENT_ACTIVATE ){
			card_ptr_t* c = cards_ptr[get_id(enc_p, enc_c)];
			int cless = c->req_colorless;
			if( check_special_flags2(player, card, SF2_X_SPELL) ){
				cless += get_card_instance(enc_p, enc_c)->info_slot;
			}
			int c1 = get_cost_mod_for_activated_abilities(pl, cr, cless, c->req_black, c->req_blue, c->req_green, c->req_red, c->req_white);
			charge_mana_multi(player, c1, c->req_black, c->req_blue, c->req_green, c->req_red, c->req_white);
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			remove_counter(pl, cr, COUNTER_NET);
		}
	}
	return 0;
}

int card_merseine(int player, int card, event_t event){
/* Merseine	|2|U|U
 * Enchantment - Aura
 * Enchant creature
 * ~ enters the battlefield with three net counters on it.
 * Enchanted creature doesn't untap during its controller's untap step as long as ~ has a net counter on it.
 * Pay enchanted creature's mana cost: Remove a net counter from ~. Any player may activate this ability, but only if he or she controls
 * 		the enchanted creature. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_NET, 4);

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int pl = instance->damage_target_player;
		int cr = instance->damage_target_card;

		if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
			int found[2] = {-1};
			CYCLE_ALL_CARDS(p, c, 	{
										if( in_play(p, c) && is_what(p, c, TYPE_EFFECT) ){
											card_instance_t *inst = get_card_instance(player, c);
											if( inst->info_slot == (int)merseine_ability ){
												if( (inst->targets[0].player == player && inst->targets[0].card == card) )
												{
													found[p] = c;
												}
											}
										}
									}
			);
			if( found[pl] == -1 ){
				int legacy = -1;
				card_instance_t *inst;
				if( pl == player ){
					legacy = create_targetted_legacy_activate(player, card, &merseine_ability, pl, cr);
					inst = get_card_instance(player, legacy);
				}
				else{
					int fake = add_card_to_hand(1-player, get_original_internal_card_id(player, card));
					legacy = create_targetted_legacy_activate(1-player, fake, &merseine_ability, pl, cr);
					obliterate_card(1-player, fake);
					inst = get_card_instance(1-player, legacy);
				}
				inst->targets[0].player = player;
				inst->targets[0].card = card;
				inst->number_of_targets = 1;
			}
			if( found[1-pl] != -1 ){
				kill_card(1-pl, found[1-pl], KILL_EXILE);
			}
		}

		if( count_counters(player, card, COUNTER_NET) ){
			does_not_untap(pl, cr, event );
		}
	}

	return disabling_aura(player, card, event);
}

int card_river_merfolk(int player, int card, event_t event){
/* CARD_ID_RIVER_MERFOLK	1817
River Merfolk	|U|U
Creature - Merfolk 2/1
|U: ~ gains |H2mountainwalk until end of turn. (It can't be blocked as long as defending player controls |Ha Mountain.) */

	return generic_shade(player, card, event, 0, MANACOST_U(1), 0, 0, KEYWORD_MOUNTAINWALK, 0);
}

int card_seasinger(int player, int card, event_t event){
/* CARD_ID_SEASINGER	1820
Seasinger	|1|U|U
Creature - Merfolk 0/1
When you control no |H1Islands, sacrifice ~.
You may choose not to untap ~ during your untap step.
|T: Gain control of target creature whose controller controls |Han Island for as long as you control ~ and ~ remains tapped. */

	choose_to_untap(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! count_subtype(player, TYPE_LAND, SUBTYPE_ISLAND) ){
		kill_card(player, card, KILL_SACRIFICE);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE") ){
			return count_subtype(1-player, TYPE_LAND, SUBTYPE_ISLAND) > 0;
		}
	}

	if( event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && count_subtype(1-player, TYPE_LAND, SUBTYPE_ISLAND) > 0 ){
			gain_control_until_source_is_in_play_and_tapped(instance->parent_controller, instance->parent_card,
															instance->targets[0].player, instance->targets[0].card,
															GCUS_TAPPED | GCUS_CONTROLLED);
			card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
			parent->targets[0] = instance->targets[0];
		}
	}

	return 0;
}

int card_svyelunite_priest(int player, int card, event_t event){
/* CARD_ID_SVYELUNITE_PRIEST	1825
Svyelunite Priest	|1|U
Creature - Merfolk Cleric 1/1
|U|U, |T: Target creature gains shroud until end of turn. Activate this ability only during your upkeep. (It can't be the target of spells or abilities.)*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if(valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
									0, 0, KEYWORD_SHROUD, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_ONLY_ON_UPKEEP, MANACOST_U(2), 0, &td, "TARGET_CREATURE");
}

static void gains_first_strike(int player, int card, int t_player, int t_card)
{
	pump_ability_until_eot(player, card, t_player, t_card, 0, 0, KEYWORD_FIRST_STRIKE, 0);
}

int card_tidal_flats(int player, int card, event_t event){
	/*
	  Tidal Flats English |U
	  Enchantment
	  {U}{U}: For each attacking creature without flying, its controller may pay {1}.
	  If he or she doesn't, creatures you control blocking that creature gain first strike until end of turn.
	*/
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int marked[151] = {0};
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == current_turn && in_play(p, c) && is_what(p, c, TYPE_CREATURE) && is_attacking(p, c) &&
										check_for_ability(p, c, KEYWORD_FLYING) )
									{
										marked[c] = 1;
									}
								};
		);
		int i;
		for(i=150; i > -1; i--){
			if( marked[i] ){
				int choice = 1;
				if( has_mana_multi(current_turn, MANACOST_X(1)) ){
					if( do_dialog(current_turn, current_turn, i, -1, -1, " Pay 1\n Decline", 0) == 0 ){
						charge_mana_multi(current_turn, MANACOST_X(1));
						if( spell_fizzled != 1 ){
							choice = 0;
						}
					}
				}
				if( choice == 1 ){
					for_each_creature_blocking_me(current_turn, i, gains_first_strike,
													instance->parent_controller, instance->parent_card);
				}
			}
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_U(2), 0, NULL, NULL);
}

int card_tidal_influence(int player, int card, event_t event){

	/* Tidal Influence	|2|U
	 * Enchantment
	 * Cast ~ only if no permanents named ~ are on the battlefield.
	 * ~ enters the battlefield with a tide counter on it.
	 * At the beginning of your upkeep, put a tide counter on ~.
	 * As long as there is exactly one tide counter on ~, all |Sblue creatures get -2/-0.
	 * As long as there are exactly three tide counters on ~, all |Sblue creatures get +2/+0.
	 * Whenever there are four tide counters on ~, remove all tide counters from it. */

	if( event == EVENT_CAN_CAST && count_cards_by_id(player, get_id(player, card)) > 0 ){
		return 0;
	}

	enters_the_battlefield_with_counters(player, card, event, COUNTER_TIDE, 1);

	if( ! is_humiliated(player, card) ){
		upkeep_trigger_ability(player, card, event, player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			add_counter(player, card, COUNTER_TIDE);
		}

		if (event == EVENT_POWER && (get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_BLUE))){
			int counters = count_counters(player, card, COUNTER_TIDE);
			if( counters == 1 ){
				event_result -= 2;
			}
			else if( counters == 3 ){
				event_result += 2;
			}
		}

		if (event == EVENT_STATIC_EFFECTS && count_counters(player, card, COUNTER_TIDE) >= 4){
			remove_all_counters(player, card, COUNTER_TIDE);
		}
	}
	return global_enchantment(player, card, event);
}

int card_vodalian_mage(int player, int card, event_t event){
	/* Vodalian Mage	|2|U
	 * Creature - Merfolk Wizard 1/1
	 * |U, |T: Counter target spell unless its controller pays |1. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		counterspell_resolve_unless_pay_x(player, card, &td, 0, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SPELL_ON_STACK, MANACOST_U(1), 0, &td, NULL);
}

int card_vodalian_knights(int player, int card, event_t event)
{
  /* Vodalian Knights	|1|U|U
   * Creature - Merfolk Knight 2/2
   * First strike
   * ~ can't attack unless defending player controls |Han Island.
   * When you control no |H1Islands, sacrifice ~.
   * |U: ~ gains flying until end of turn. */

  islandhome(player, card, event);
  return card_gust_skimmer(player, card, event);
}

static int effect_vodalian_war_machine(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int vwm_player = instance->targets[0].player;
		int vwm_card = instance->targets[0].card;

		if( event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(vwm_player, vwm_card) ){
			instance->targets[11].player = 1;
			if( is_this_dying(vwm_player, vwm_card) ){
				instance->targets[11].player |= 2;
			}
		}
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( result ){
		if( result & 2 ){
			card_instance_t *instance = get_card_instance(player, card);

			int p = instance->damage_target_player;
			int c = instance->damage_target_card;

			kill_card(p, c, KILL_DESTROY);
		}
		kill_card(player, card, KILL_EXILE);
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE );
	}

	return 0;
}
int card_vodalian_war_machine(int player, int card, event_t event){
/*
Vodalian War Machine |1|U|U
Creature — Wall 0/4
Defender (This creature can't attack.)
Tap an untapped Merfolk you control: Vodalian War Machine can attack this turn as though it didn't have defender.
Tap an untapped Merfolk you control: Vodalian War Machine gets +2/+1 until end of turn.
When Vodalian War Machine dies, destroy all Merfolk tapped this turn to pay for its abilities.
*/

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( has_mana_for_activated_ability(player, card, MANACOST0) && !(current_turn == player && current_phase == PHASE_MAIN1) )
		{
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.required_subtype = SUBTYPE_MERFOLK;
			td.illegal_state = TARGET_STATE_TAPPED;
			td.allowed_controller = player;
			td.preferred_controller = player;
			td.illegal_abilities = 0;

			int amount = target_available(player, card, &td);
			return event == EVENT_POW_BOOST ?  amount*2 : amount;
		}
	}


	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_MERFOLK;
	td.illegal_state = TARGET_STATE_TAPPED;
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;

	enum{
		CHOICE_LOSE_DEFENDER = 1,
		CHOICE_P2_P1
	};

	if(event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int priority_can_attack = current_turn == player && current_phase == PHASE_MAIN1 ? 10 : 1;
		int choice = DIALOG(player, card, event, DLG_RANDOM,
						  "Able to attack", 1, priority_can_attack,
						  "+2/+1", 			1, 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			if( new_pick_target(&td, "Select an untapped Merfolk to tap.", 0, 1 | GS_LITERAL_PROMPT) ){
				instance->number_of_targets = 0;
				tap_card(instance->targets[0].player, instance->targets[0].card);
				int legacy_card = create_targetted_legacy_effect(player, card, &effect_vodalian_war_machine,
																instance->targets[0].player, instance->targets[0].card);
				card_instance_t *legacy_instance = get_card_instance(player, legacy_card);
				legacy_instance->targets[0].player = player;
				legacy_instance->targets[0].card = card;
				legacy_instance->number_of_targets = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_LOSE_DEFENDER ){
			return generic_shade(player, card, event, 0, MANACOST0, 2, 1, 0, 0);
		}
		if( instance->info_slot == CHOICE_P2_P1 ){
			create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, effect_defender_can_attack_until_eot,
											instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}


// black cards
int card_armor_thrull(int player, int card, event_t event){

	/* Armor Thrull	|2|B
	 * Creature - Thrull 1/3
	 * |T, Sacrifice ~: Put a +1/+2 counter on target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target( &td ) ){
			add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P2);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_basal_thrull(int player, int card, event_t event){
/* CARD_ID_BASAL_THRULL	1752
Basal Thrull	|B|B
Creature - Thrull 1/2
|T, Sacrifice ~: Add |B|B to your mana pool. */

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		produce_mana_tapped(player, card, COLOR_BLACK, 2);
		kill_card(player, card, KILL_SACRIFICE);
	}

	if( event == EVENT_COUNT_MANA ){
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL) ){
			declare_mana_available(player, COLOR_BLACK, 2);
		}
	}

	return 0;
}

int card_breeding_pit(int player, int card, event_t event){
/* CARD_ID_BREEDING_PIT	1755
Breeding Pit	|3|B
Enchantment
At the beginning of your upkeep, sacrifice ~ unless you pay |B|B.
At the beginning of your end step, create a 0/1 |Sblack Thrull creature token. */

	basic_upkeep(player, card, event, MANACOST_B(2));

	if( current_turn == player && eot_trigger(player, card, event) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_THRULL, &token);
		token.pow = 0;
		token.tou = 1;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		generate_token(&token);
	}

	return global_enchantment(player, card, event);
}

int card_derelor(int player, int card, event_t event){
/* CARD_ID_DERELOR	1761
Derelor	|3|B
Creature - Thrull 4/4
|SBlack spells you cast cost |B more to cast. */

	if(event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card) ){
		int color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		if( get_color(affected_card_controller, affected_card) & color ){
			COST_BLACK++;
		}
	}

	return 0;
}

int card_ebon_praetor(int player, int card, event_t event){
/* Ebon Praetor	|4|B|B
 * Creature - Avatar Praetor 5/5
 * First strike, trample
 * At the beginning of your upkeep, put a -2/-2 counter on ~.
 * Sacrifice a creature: Remove a -2/-2 counter from ~. If the sacrificed creature was a Thrull, put a +1/+0 counter on ~.
 * 		Activate this ability only during your upkeep and only once each turn. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counter(player, card, COUNTER_M2_M2);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_ONLY_ON_UPKEEP | GAA_ONCE_PER_TURN | GAA_SACRIFICE_CREATURE, MANACOST0, 0,
										NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_AS_COST | SAC_RETURN_CHOICE, &test);
		if( ! sac ){
			spell_fizzled = 1;
			return 0;
		}
		state_untargettable(BYTE2(sac), BYTE3(sac), 0);
		generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST0, 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			instance->info_slot = has_subtype(BYTE2(sac), BYTE3(sac), SUBTYPE_THRULL);
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		remove_counter(instance->parent_controller, instance->parent_card, COUNTER_M2_M2);
		if( instance->info_slot == 1 ){
			add_counter(instance->parent_controller, instance->parent_card, COUNTER_P1_P0);
		}
	}

	return 0;
}

int card_initiates_of_the_ebon_hand(int player, int card, event_t event){
/* CARD_ID_INITIATES_OF_THE_EBON_HAND	1806
Initiates of the Ebon Hand	|B
Creature - Cleric 1/1
|1: Add |B to your mana pool. If this ability has been activated four or more times this turn, sacrifice ~ at the beginning of the next end step. */

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) ){
		return has_mana_for_activated_ability(player, card, MANACOST_X(1));
	}

	if( event == EVENT_ACTIVATE ){
		charge_mana_multi(player, MANACOST_X(1));
		if( spell_fizzled != 1 ){
			card_instance_t *instance = get_card_instance(player, card);
			produce_mana(player, COLOR_BLACK, 1);
			if( instance->info_slot < 0 ){ instance->info_slot = 0; }
			instance->info_slot++;
		}
	}

	if( trigger_condition == TRIGGER_EOT && affect_me(player, card) ){
		if(eot_trigger(player, card, event)){
			if( get_card_instance(player, card)->info_slot >= 4 && ! check_state(player, card, STATE_OUBLIETTED) &&
			        ! is_humiliated(player, card) )
			{
				kill_card(player, card, KILL_SACRIFICE);
			}
			get_card_instance(player, card)->info_slot = 0;
		}
	}

	return 0;
}

int card_mindstab_thrull(int player, int card, event_t event){
/* CARD_ID_MINDSTAB_THRULL	1808
Mindstab Thrull	|1|B|B
Creature - Thrull 2/2
Whenever ~ attacks and isn't blocked, you may sacrifice it. If you do, defending player discards three cards. */

	if( event == EVENT_DECLARE_BLOCKERS ){
		if( is_unblocked(player, card) && ! is_humiliated(player, card) ){
			int ai_choice = 1;
			if( hand_count[1-player] > 1 ){
				ai_choice = 0;
			}
			int choice = do_dialog(player, player, card, -1, -1, " Sacrifice\n Do not sacrifice", ai_choice);
			if( choice == 0 ){
				kill_card(player, card, KILL_SACRIFICE );
				new_multidiscard(1-player, 3, 0, player);
			}
		}
	}
	return 0;
}

int card_necrite(int player, int card, event_t event){
/* CARD_ID_NECRITE	1809
Necrite	|1|B|B
Creature - Thrull 2/2
Whenever ~ attacks and isn't blocked, you may sacrifice it. If you do, destroy target creature defending player controls. It can't be regenerated. */

	if( event == EVENT_DECLARE_BLOCKERS ){
		if( is_unblocked(player, card) && ! is_humiliated(player, card) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.allowed_controller = 1-player;
			td.preferred_controller = 1-player;
			td.allow_cancel = 0;
			int ai_choice = 0;
			if( can_target( &td ) == 0 ){
				ai_choice = 1;
			}
			int choice = do_dialog(player, player, card, -1, -1, " Sacrifice\n Do not sacrifice", ai_choice);
			if( choice == 0 ){
				if( can_target( &td ) ){
					if( pick_target(&td, "TARGET_CREATURE" ) ){
						card_instance_t *instance = get_card_instance(player, card);
						kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY );
						kill_card(player, card, KILL_SACRIFICE );
					}
				}
			}
		}
	}
	return 0;
}

int card_order_of_the_ebon_hand(int player, int card, event_t event){
/* CARD_ID_ORDER_OF_THE_EBON_HAND	1556
Order of the Ebon Hand	|B|B
Creature - Cleric Knight 2/1
Protection from |Swhite
|B: ~ gains first strike until end of turn.
|B|B: ~ gets +1/+0 until end of turn. */

	return fe_knights(player, card, event, COLOR_BLACK);
}

int card_soul_exchange(int player, int card, event_t event){
/* Soul Exchange	|B|B
 * Sorcery
 * As an additional cost to cast ~, exile a creature you control.
 * Return target creature card from your graveyard to the battlefield.  Put a +2/+2 counter on that creature if the exiled creature
 *		was a Thrull. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	char msg[100] = "Select a creature card.";
	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, msg);

	if( event == EVENT_CAN_CAST && can_target(&td) ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 0, &this_test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->info_slot = instance->number_of_targets = 0;
		if( new_pick_target(&td, "Select a creature you control to exile.", 0, 1 | GS_LITERAL_PROMPT) ){
			int to_exile = instance->targets[0].card;
			if( select_target_from_grave_source(player, card, player, 0, AI_MAX_CMC, &this_test, 0) != -1 ){
				if( has_creature_type(player, to_exile, SUBTYPE_THRULL ) ){
					instance->info_slot = 1;
				}
				kill_card(player, to_exile, KILL_EXILE);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			int zombo = reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
			if( instance->info_slot == 1 ){
				add_counter(player, zombo, COUNTER_P2_P2);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_thrull_champion(int player, int card, event_t event){
/* CARD_ID_THRULL_CHAMPION	1834
Thrull Champion	|4|B
Creature - Thrull 2/2
Thrull creatures get +1/+1.
|T: Gain control of target Thrull for as long as you control ~. */

	boost_creature_type(player, card, event, SUBTYPE_THRULL, 1, 1, 0, BCT_INCLUDE_SELF);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.required_subtype = SUBTYPE_THRULL;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && in_play(instance->parent_controller, instance->parent_card) ){
			gain_control_until_source_is_in_play_and_tapped(instance->parent_controller, instance->parent_card,
															instance->targets[0].player, instance->targets[0].card, GCUS_CONTROLLED);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target Thrull.");
}

int base_carapace(int player, int card, event_t event, int pow, int tgh);

int card_thrull_retainer(int player, int card, event_t event)
{
  /* Thrull Retainer	|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+1.
   * Sacrifice ~: Regenerate enchanted creature. */
  return base_carapace(player, card, event, 1,1);
}

int card_thrull_wizard(int player, int card, event_t event){
/*CARD_ID_THRULL_WIZARD	1836
Thrull Wizard	|2|B
Creature - Thrull Wizard 1/1
|1|B: Counter target |Sblack spell unless that spell's controller pays |B or |3.*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = 0;
		if( has_mana(instance->targets[0].player, COLOR_COLORLESS, 3) ){
			choice = 1;
		}
		if( has_mana(instance->targets[0].player, COLOR_BLACK, 1) ){
			choice = 2;
		}
		if( choice && player == HUMAN ){
			choice = DIALOG(player, card, event, DLG_NO_STORAGE,
							"Pay 3", has_mana(instance->targets[0].player, COLOR_COLORLESS, 3), 1,
							"Pay B", has_mana(instance->targets[0].player, COLOR_BLACK, 1), 1);
		}
		if( choice ){
			ldoubleclicked = 0;
			charge_mana_multi(instance->targets[0].player, MANACOST_XB((choice == 1 ? 3 : 0),  (choice == 2 ? 1 : 0)));
			if( spell_fizzled == 1 ){
				choice = 0;
			}
		}
		if( choice == 0 ){
			real_counter_a_spell(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST_XB(1, 1), 0, &td, "TARGET_SPELL");
}

int card_tourachs_chant(int player, int card, event_t event){
/* CARD_ID_TOURACHS_CHANT	1839
Tourach's Chant	|1|B|B
Enchantment
At the beginning of your upkeep, sacrifice ~ unless you pay |B.
Whenever a player puts |Ha Forest onto the battlefield, ~ deals 3 damage to that player unless he or she puts a -1/-1 counter on a creature he or she controls. */

	return fe_chant(player, card, event, SUBTYPE_FOREST);
}

int card_tourachs_gate(int player, int card, event_t event){

	/* Tourach's Gate	|1|B|B
	 * Enchantment - Aura
	 * Enchant land you control
	 * Sacrifice a Thrull: Put three time counters on ~.
	 * At the beginning of your upkeep, remove a time counter from ~. If there are no time counters on ~, sacrifice it.
	 * Enchanted land has "|T: Attacking creatures you control get +2/-1 until end of turn." */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		remove_counter(player, card, COUNTER_TIME);
		if( count_counters(player, card, COUNTER_TIME) <= 0 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_CAN_ACTIVATE ){
			if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
				if( ! is_tapped(p, c) && ! is_animated_and_sick(p, c) ){
					return 1;
				}
				if( can_sacrifice_as_cost(player, 1, TYPE_PERMANENT, 0, SUBTYPE_THRULL, 0, 0, 0, 0, 0, -1, 0) ){
					return 1;
				}
			}
		}

		if( event == EVENT_ACTIVATE ){
			int abilities[2] = {	can_sacrifice_as_cost(player, 1, TYPE_PERMANENT, 0, SUBTYPE_THRULL, 0, 0, 0, 0, 0, -1, 0),
									! is_tapped(p, c) && ! is_animated_and_sick(p, c)

			};
			int priorities[2] = {	15-(count_counters(player, card, COUNTER_TIME)*5),
									current_turn == player && current_phase == PHASE_AFTER_BLOCKING ? 10 : 0,
			};
			if( abilities[0] && count_counters(player, card, COUNTER_TIME) < 1 ){
				ai_modifier+=25;
			}
			int choice = choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
						  "Sac a Thrull", abilities[0], priorities[0],
						  "Pump attacking creatures", abilities[1], priorities[1]);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( choice == 1 ){
				if( sacrifice(player, card, player, 0, TYPE_PERMANENT, 0, SUBTYPE_THRULL, 0, 0, 0, 0, 0, -1, 0) ){
					instance->info_slot = choice;
				}
				else{
					spell_fizzled = 1;
				}
			}
			if( choice == 2 ){
				tap_card(p, c);
				instance->info_slot = choice;
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( instance->info_slot == 1 ){
				add_counters(instance->parent_controller, instance->parent_card, COUNTER_TIME, 3);
			}
			if( instance->info_slot == 2 && current_turn == player){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				this_test.state = STATE_ATTACKING;

				pump_creatures_until_eot(instance->parent_controller, instance->parent_card, current_turn, 0, 2, -1, 0, 0, &this_test);
			}
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.allowed_controller = player;
	td.preferred_controller = player;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target land you control");
}


// red
int card_brassclaw_orcs(int player, int card, event_t event){
/* CARD_ID_BRASSCLAW_ORCS	1754
Brassclaw Orcs	|2|R
Creature - Orc 3/2
~ can't block creatures with power 2 or greater. */

	if(event == EVENT_BLOCK_LEGALITY && affect_me(player, card) && ! is_humiliated(player, card) ){
		if( get_power(attacking_card_controller, attacking_card) >= 2 ){
			event_result = 1;
		}
	}
	return 0;
}

int card_dwarven_armorer(int player, int card, event_t event){

	/* Dwarven Armorer	|R
	 * Creature - Dwarf 0/2
	 * |R, |T, Discard a card: Put a +0/+1 counter or a +1/+0 counter on target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int ai_choice = get_power(instance->targets[0].player, instance->targets[0].card) >
							get_toughness(instance->targets[0].player, instance->targets[0].card)
							? 1 : 0;
			int choice = do_dialog(player, player, card, -1, -1, " +1/+0\n +0/+1", ai_choice);
			if( choice == 0 ){
				add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P0);
			}
			else{
				add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_P0_P1);
			}
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DISCARD, MANACOST_R(1), 0, &td, "TARGET_CREATURE");
}

int card_dwarven_catapult(int player, int card, event_t event){
/* CARD_ID_DWARVEN_CATAPULT	1764
Dwarven Catapult	|X|R
Instant
~ deals X damage divided evenly, rounded down, among all creatures target opponent controls. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			if( player == HUMAN ){
				charge_mana(player, COLOR_COLORLESS, -1);
				if( spell_fizzled != 1 ){
					instance->info_slot = x_value;
				}
			}
			else{
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				int max_t = check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_MAX_TOU, &this_test);
				int opp_c = count_subtype(1-player, TYPE_CREATURE, -1);
				int i;
				for(i=1; i<max_t; i++){
					if( ! has_mana(player, COLOR_COLORLESS, i * opp_c) ){
						break;
					}
				}
				charge_mana(player, COLOR_COLORLESS, i * opp_c);
				if( spell_fizzled != 1 ){
					instance->info_slot = i * opp_c;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = instance->info_slot / (count_subtype(1-player, TYPE_CREATURE, -1)+1);
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_damage_all(player, card, 1-player, amount, NDA_ALL_CREATURES, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_dwarven_lieutenant(int player, int card, event_t event){
/* CARD_ID_DWARVEN_LIEUTENANT	1766
Dwarven Lieutenant	|R|R
Creature - Dwarf Soldier 1/2
|1|R: Target Dwarf creature gets +1/+0 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_DWARF;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if(valid_target(&td) ){
			pump_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 0);
		}
	}
	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XR(1, 1), 0,
									&td, "Select target Dwarf creature.");
}

int card_dwarven_soldier(int player, int card, event_t event){
/* CARD_ID_DWARVEN_SOLDIER	1768
Dwarven Soldier	|1|R
Creature - Dwarf Soldier 2/1
Whenever ~ blocks or becomes blocked by one or more Orcs, ~ gets +0/+2 until end of turn. */

	if( ! is_humiliated(player, card) ){
		if( event == EVENT_DECLARE_BLOCKERS ){
			card_instance_t *instance = get_card_instance(player, card);
			if( instance->state & STATE_ATTACKING ){
				int p = 1-player;
				int count = 0;
				while(count < active_cards_count[p]){
					card_instance_t *inst = get_card_instance(p, count);
					card_data_t* card_d = get_card_data(p, count);
					if((card_d->type & TYPE_CREATURE) && in_play(p, count) && inst->blocking == card && has_subtype(p, count, SUBTYPE_ORC) ){
						pump_until_eot(player, card, player, card, 0, 2);
						break;
					}
					count++;
				}
			}
			else if( instance->blocking != 255 && has_subtype(1-player, instance->blocking, SUBTYPE_ORC) ){
				pump_until_eot(player, card, player, card, 0, 2);
			}
		}
	}

	return 0;
}

int card_goblin_chirurgeon(int player, int card, event_t event){
	/* Goblin Chirurgeon	|R
	 * Creature - Goblin Shaman 0/2
	 * Sacrifice a Goblin: Regenerate target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}
	td.allow_cancel = 0;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Goblin to sacrifice.");
	test.subtype = SUBTYPE_GOBLIN;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			if( can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
				regenerate_target( instance->targets[0].player, instance->targets[0].card );
			}
		}
	}
	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET | GAA_REGENERATION,
															MANACOST0, 0, &td, "TARGET_CREATURE", 0, &test, 0);
}

int card_goblin_flotilla(int player, int card, event_t event){
/* CARD_ID_GOBLIN_FLOTILLA	1782
Goblin Flotilla	|2|R
Creature - Goblin 2/2
|H2Islandwalk (This creature can't be blocked as long as defending player controls |Han Island.)
At the beginning of each combat, unless you pay |R, whenever ~ blocks or becomes blocked by a creature this combat,
that creature gains first strike until end of turn. */

	card_instance_t *instance = get_card_instance(player, card);

	if( beginning_of_combat(player, card, event, player, card) ){
		charge_mana(player, COLOR_RED, 1);
		if( spell_fizzled == 1 ){
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( instance->info_slot == 1 && ! is_humiliated(player, card) ){
		for_each_creature_blocking_me(player, card, gains_first_strike, player, card);
		for_each_creature_blocked_by_me(player, card, gains_first_strike, player, card);
	}

	if( event == EVENT_CLEANUP ){
		instance->info_slot = 0;
	}

	return 0;
}

int card_goblin_grenade(int player, int card, event_t event){
	/* Goblin Grenade	|R	0x2001227
	 * Sorcery
	 * As an additional cost to cast this spell, sacrifice a Goblin.
	 * ~ deals 5 damage to any target. */
	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select Goblin to sacrifice.");
	test.subtype = SUBTYPE_GOBLIN;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_CAST){
		return can_target(&td) && (is_token(player, card) || new_can_sacrifice_as_cost(player, card, &test));
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card)){
		instance->number_of_targets = 0;
		if( is_token(player, card) ){	// copy of a spell, so don't have to (and can't) pay additional costs
			pick_target(&td, "TARGET_ANY");
		}
		else{
			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_RETURN_CHOICE | SAC_AS_COST, &test);
			if (sac){
				if( can_target(&td) && // maybe it was the only targettable creature or player
					pick_target(&td, "TARGET_ANY") )
				{
					kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				}
				else{
					state_untargettable(BYTE2(sac), BYTE3(sac), 0);
					cancel = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 5);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int effect_goblin_kites(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( event == EVENT_ABILITIES && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			event_result |= KEYWORD_FLYING;
		}
	}

	if( eot_trigger(player, card, event) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 &&
			! check_state(instance->damage_target_player, instance->damage_target_card, STATE_OUBLIETTED) )
		{
			int flip = flip_a_coin(player, card);
			if( flip == 0 ){
				kill_card(instance->damage_target_player, instance->damage_target_card, KILL_SACRIFICE);
			}
		}
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_goblin_kites(int player, int card, event_t event){
/* CARD_ID_GOBLIN_KITES	1784
Goblin Kites	|1|R
Enchantment
|R: Target creature you control with toughness 2 or less gains flying until end of turn.
Flip a coin at the beginning of the next end step. If you lose the flip, sacrifice that creature. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.toughness_requirement = 2 | TARGET_PT_LESSER_OR_EQUAL;
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &effect_goblin_kites,
											instance->targets[0].player, instance->targets[0].card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_R(1), 0,
									&td, "Select target creature you control.");
}

int card_goblin_war_drums(int player, int card, event_t event){
	/* Goblin War Drums	|2|R
	 * Enchantment
	 * Creatures you control have menace. */
	// Used externally
	if( event == EVENT_DECLARE_BLOCKERS && ! is_humiliated(player, card) ){
		int count;
		for (count = 0; count < active_cards_count[player]; ++count){
			if(in_play(player, count) ){
				menace(player, card, event);
			}
		}
	}
	return global_enchantment(player, card, event);
}

int card_goblin_warrens(int player, int card, event_t event){
	/* Goblin Warrens	|2|R
	 * Enchantment
	 * |2|R, Sacrifice two Goblins: Put three 1/1 |Sred Goblin creature tokens onto the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Goblin to sacrifice.");
	test.subtype = SUBTYPE_GOBLIN;

	if( event == EVENT_CAN_ACTIVATE && generic_activated_ability(player, card, event, 0, MANACOST_XR(2, 1), 0, NULL, NULL) ){
		test.qty = 2;
		return new_can_sacrifice_as_cost(player, card, &test);
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_XR(2, 1)) ){
			int sac[2];
			int i;
			for(i=0; i<2; i++){
				sac[i] = new_sacrifice(player, card, player, SAC_AS_COST | SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
				if( ! sac[i] ){
					spell_fizzled = 1;
					return 0;
				}
			}
			for(i=0; i<2; i++){
				state_untargettable(BYTE2(sac[i]), BYTE3(sac[i]), 0);
				kill_card(BYTE2(sac[i]), BYTE3(sac[i]), KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_tokens_by_id(player, card, CARD_ID_GOBLIN, 3);
	}

	return 0;
}

int card_orcish_captain(int player, int card, event_t event){
/* CARD_ID_ORCISH_CAPTAIN	1811
Orcish Captain	|R
Creature - Orc Warrior 1/1
|1: Flip a coin. If you win the flip, target Orc creature gets +2/+0 until end of turn. If you lose the flip, it gets -0/-2 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_ORC;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			if( flip_a_coin(instance->parent_controller, instance->parent_card) ){
				pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
												instance->targets[0].player, instance->targets[0].card, 2, 0);
			}
			else{
				pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
												instance->targets[0].player, instance->targets[0].card, 0, -2);
			}
		}
	}
	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(1), 0,
									&td, "Select target Orc creature.");
}

int card_orcish_spy(int player, int card, event_t event){

	/* Orcish Spy	|R
	 * Creature - Orc Rogue 1/1
	 * |T: Look at the top three cards of target player's library. */

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		show_deck( player, deck_ptr[get_card_instance(player, card)->targets[0].player], 3, "Spying...", 0, 0x7375B0 );
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
}

int card_orcish_veteran(int player, int card, event_t event){
/* CARD_ID_ORCISH_VETERAN	1812
Orcish Veteran	|2|R
Creature - Orc 2/2
~ can't block |Swhite creatures with power 2 or greater.
|R: ~ gains first strike until end of turn. */

	if( event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card) ){
		if( affect_me( player, card ) ){
			int color = 1 << get_sleighted_color(player, card, COLOR_WHITE);
			if( get_power(attacking_card_controller, attacking_card) > 1 && ( get_color(attacking_card_controller, attacking_card) & color) ){
				event_result = 1;
			}
		}
	}
	return generic_shade(player, card, event, 0, MANACOST_R(1), 0, 0, KEYWORD_FIRST_STRIKE, 0);
}

int card_orgg(int player, int card, event_t event){
/* CARD_ID_ORGG	1813
Orgg	|3|R|R
Creature - Orgg 6/6
Trample
~ can't attack if defending player controls an untapped creature with power 3 or greater.
~ can't block creatures with power 3 or greater. */

	if( ! is_humiliated(player, card) ){
		if( event == EVENT_BLOCK_LEGALITY ){
			if( affect_me( player, card ) ){
				if( get_power(attacking_card_controller, attacking_card) > 2 ){
					event_result = 1;
				}
			}
		}
		if( event == EVENT_ATTACK_LEGALITY ){
			int p = 1-player;
			int count = 0;
			while(count < active_cards_count[p]){
					card_data_t* card_d = get_card_data(p, count);
					if((card_d->type & TYPE_CREATURE) && in_play(p, count) && get_power(p, count) > 2 && ! is_tapped(p, count)){
						event_result = 1;
						break;
					}
					count++;
			}
		}
	}
	return 0;
}

int card_raiding_party(int player, int card, event_t event){
/*
Raiding Party |2|R
Enchantment
Raiding Party can't be the target of white spells or abilities from white sources.
Sacrifice an Orc: Each player may tap any number of untapped white creatures he or she controls.
	For each creature tapped this way, that player chooses up to two Plains. Then destroy all Plains that weren't chosen this way by
	any player.
*/
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select an Orc to sacrifice.");
	test.subtype = SUBTYPE_ORC;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		APNAP(p,{
					int c;
					for(c=0; c<active_cards_count[p]; c++){
						if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) &&
							(get_color(p, c) && get_sleighted_color_test(p, c, COLOR_TEST_WHITE)) )
						{
							target_definition_t td;
							default_target_definition(player, card, &td, TYPE_PERMANENT);
							td.required_subtype = get_hacked_subtype(player, card, SUBTYPE_PLAINS);
							td.illegal_abilities = 0;
							if( p == AI ){
								td.allowed_controller = td.preferred_controller = p;
							}

							card_instance_t *instance = get_card_instance(player, card);

							int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
												"Tap this creature", 	1, can_target(&td) ? 10 : 1,
												"Decline", 				1, 5);
							if( choice == 1 ){
								int i;
								for(i=0; i<2; i++){
									if( new_pick_target(&td, get_hacked_land_text(player, card, "Select a %s to keep.", SUBTYPE_PLAINS),
														0, 0) )
									{
										state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
										instance->number_of_targets = 0;
									}
									else{
										break;
									}
								}
							}
						}
					}
				};
		);
		APNAP(p,{
					int c;
					for(c = active_cards_count[p]-1; c>-1; c--){
						if( in_play(p, c) && has_subtype(p, c, get_hacked_subtype(player, card, SUBTYPE_PLAINS)) ){
							if( check_state(p, c, STATE_CANNOT_TARGET) ){
								remove_state(p, c, STATE_CANNOT_TARGET);
							}
							else{
								kill_card(p, c, KILL_DESTROY );
							}
						}
					}
				};
		);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, 0);
}


// green cards
int card_elven_fortress(int player, int card, event_t event){
/* CARD_ID_ELVEN_FORTRESS	1771
Elven Fortress	|G
Enchantment
|1|G: Target blocking creature gets +0/+1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_BLOCKING;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if(valid_target(&td) ){
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card, 0, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XG(1, 1), 0,
									&td, "Select target blocking creature.");
}

int card_elvish_farmer(int player, int card, event_t event){

	/* Elvish Farmer	|1|G
	 * Creature - Elf 0/2
	 * At the beginning of your upkeep, put a spore counter on ~.
	 * Remove three spore counters from ~: Put a 1/1 |Sgreen Saproling creature token onto the battlefield.
	 * Sacrifice a Saproling: You gain 2 life. */

	add_spore_counters(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_SAPROLING = 1,
		CHOICE_LIFE
	};

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Saproling to sacrifice.");
	test.subtype = SUBTYPE_SAPROLING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3), NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, 0);
	}

	if( event == EVENT_ACTIVATE ){
		int abilities[3] = {
							0,
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3),
														NULL, NULL),
							generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, 0, NULL, NULL,
																			0, &test, 0)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
						  "Generate a Saproling", abilities[1], 5,
						  "Sac a Saproling and gain 2 life", abilities[2], life[player] < 6 ? 10 : 1);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_SAPROLING ){
			return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3), NULL, NULL);
		}

		if( choice == CHOICE_LIFE ){
			return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAPROLING ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING);
		}
		if( instance->info_slot == CHOICE_LIFE ){
			gain_life(player, 2);
		}
	}

	return 0;
}

int card_elvish_hunter(int player, int card, event_t event){
/* CARD_ID_ELVISH_HUNTER	1774
Elvish Hunter	|1|G
Creature - Elf Archer 1/1
|1|G, |T: Target creature doesn't untap during its controller's next untap step. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	if( player == AI ){
		td.required_state = TARGET_STATE_TAPPED;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if(valid_target(&td1) ){
			does_not_untap_effect(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 1);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XG(1, 1), 0, &td, "TARGET_CREATURE");
}

int card_elvish_scout(int player, int card, event_t event){
/* CARD_ID_ELVISH_SCOUT	1775
Elvish Scout	|G
Creature - Elf Scout 1/1
|G, |T: Untap target attacking creature you control. Prevent all combat damage that would be dealt to and dealt by it this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if(valid_target(&td) ){
			maze_of_ith_effect(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_G(1), 0,
									&td, "Select target attacking creature you control.");
}

int card_feral_thallid(int player, int card, event_t event){

	/* Feral Thallid	|3|G|G|G
	 * Creature - Fungus 6/3
	 * At the beginning of your upkeep, put a spore counter on ~.
	 * Remove three spore counters from ~: Regenerate ~. */

	add_spore_counters(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		if( can_regenerate(instance->parent_controller, instance->parent_card) ){
			regenerate_target(instance->parent_controller, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_REGENERATION, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3), NULL, NULL);
}

int card_fungal_bloom(int player, int card, event_t event)
{
  /* Fungal Bloom	|G|G
   * Enchantment
   * |G|G: Put a spore counter on target Fungus. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.required_subtype = SUBTYPE_FUNGUS;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_SPORE);
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST_G(2), 0, &td, "Select target Fungus.");
}

int card_night_soil(int player, int card, event_t event){
	/* Night Soil	|G|G
	 * Enchantment
	 * |1, Exile two creature cards from a single graveyard: Put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) > 1 || count_graveyard_by_type(1-player, TYPE_CREATURE) > 1 ){
			return generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		card_instance_t *instance = get_card_instance(player, card);
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			instance->targets[0].player = 1-player;
			if( count_graveyard_by_type(1-player, TYPE_CREATURE) > 1 ){
				if( count_graveyard_by_type(player, TYPE_CREATURE) > 1 && player != AI ){
					target_definition_t td;
					default_target_definition(player, card, &td, TYPE_CREATURE);
					td.zone = TARGET_ZONE_PLAYERS;
					td.illegal_abilities = 0;

					if( pick_target(&td, "TARGET_PLAYER") ){
						instance->number_of_targets = 0;
					}
				}
			}
			else{
				instance->targets[0].player = player;
			}
			if( spell_fizzled != 1 ){
				char msg[100] = "Select a creature card to exile.";
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_CREATURE, msg);

				int exiled = 0;
				while( exiled < 2 ){
						int mode = exiled > 0 ? 1 : 0;
						int ai_mode = instance->targets[0].player == player ? AI_MIN_VALUE : AI_MAX_CMC;
						if( new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_GRAVE, TUTOR_RFG, mode, ai_mode, &this_test) != -1 ){
							exiled++;
						}
						else{
							spell_fizzled = 1;
							return 0;
						}
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_SAPROLING);
	}

	return global_enchantment(player, card, event);
}

int card_spore_cloud(int player, int card, event_t event){
/* CARD_ID_SPORE_CLOUD	1823
Spore Cloud	|1|G|G
Instant
Tap all blocking creatures. Prevent all combat damage that would be dealt this turn.
Each attacking creature and each blocking creature doesn't untap during its controller's next untap step. */

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			int count = active_cards_count[i]-1;
			while( count > -1 ){
					if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) ){
						if( check_state(i, count, STATE_ATTACKING | STATE_ATTACKED | STATE_BLOCKING | STATE_BLOCKED) ){
							does_not_untap_effect(player, card, i, count, 0, 1);
							if( check_state(i, count, STATE_BLOCKING | STATE_BLOCKED) ){
								tap_card(i, count);
							}
						}
					}
					count--;
			}
		}
		fog_effect(player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_spore_flower(int player, int card, event_t event)
{
  /* Spore Flower	|G|G
   * Creature - Fungus 0/1
   * At the beginning of your upkeep, put a spore counter on ~.
   * Remove three spore counters from ~: Prevent all combat damage that would be dealt this turn. */

	add_spore_counters(player, card, event);

	if (!IS_GAA_EVENT(event))
		return 0;

	if( event == EVENT_RESOLVE_ACTIVATION )
	{
		card_instance_t* instance = get_card_instance(player, card);
		fog_effect(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3), NULL, NULL);
}

int card_thallid(int player, int card, event_t event)
{
  /* Thallid	|G
   * Creature - Fungus 1/1
   * At the beginning of your upkeep, put a spore counter on ~.
   * Remove three spore counters from ~: Put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

  add_spore_counters(player, card, event);

  if (!IS_GAA_EVENT(event))
	return 0;

  if (event == EVENT_RESOLVE_ACTIVATION)
	generate_token_by_id(player, card, CARD_ID_SAPROLING);

  return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3), NULL, NULL);
}

int card_thallid_devourer(int player, int card, event_t event){
/* Thallid Devourer	|1|G|G
 * Creature - Fungus 2/2
 * At the beginning of your upkeep, put a spore counter on ~.
 * Remove three spore counters from ~: Put a 1/1 |Sgreen Saproling creature token onto the battlefield.
 * Sacrifice a Saproling: ~ gets +1/+2 until end of turn. */

	add_spore_counters(player, card, event);

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		int max_sac = 0;
		CYCLE_ALL_CARDS(p, c,	{
									if( in_play(p, c) && is_what(p, c, TYPE_PERMANENT) && has_subtype(p, c, SUBTYPE_SAPROLING) ){
										if( ! check_special_flags2(p, c, SF2_CANNOT_BE_SACRIFICED) ){
											max_sac++;
										}
									}
								}
		);
		return event == EVENT_TOU_BOOST ? 2*max_sac : max_sac;
	}


	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Saproling to sacrifice.");
	test.subtype = SUBTYPE_SAPROLING;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_SAPROLING = 1,
		CHOICE_PUMP
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3), NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, 0);
	}

	if( event == EVENT_ACTIVATE ){
		int abilities[3] = {
							0,
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3),
														NULL, NULL),
							generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, 0, NULL, NULL,
																			0, &test, 0)
		};
		int priorities[3] = {
							0,
							5,
							current_turn == player && is_attacking(player, card) && current_phase == PHASE_AFTER_BLOCKING ? 10 : 0
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
						  "Generate a Saproling", 		abilities[1], priorities[1],
						  "Sac a Saproling and pump", 	abilities[2], priorities[2]);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_SAPROLING ){
			return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3), NULL, NULL);
		}
		if( choice == CHOICE_PUMP ){
			return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAPROLING ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING);
		}
		if( instance->info_slot == CHOICE_PUMP ){
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
											instance->parent_controller, instance->parent_card, 1, 2);
		}
	}

	return 0;
}

static int effect_thelonite_druid(int player, int card, event_t event){

	if( event == EVENT_CHANGE_TYPE ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);

		global_type_change(player, card, event, player, TYPE_CREATURE, &this_test, 2, 3, 0, 0, 0);
	}

	if( event == EVENT_CLEANUP ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);

		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_play(player, i) && new_make_test_in_play(player, i, -1, &this_test) ){
				remove_status(player, i, STATUS_ANIMATED);
			}
		}
		kill_card(player, card, KILL_EXILE );
	}

	return 0;
}

int card_thelonite_druid(int player, int card, event_t event){
/* CARD_ID_THELONITE_DRUID	1831
Thelonite Druid	|2|G
Creature - Human Cleric Druid 1/1
|1|G, |T, Sacrifice a creature: |H1Forests you control become 2/3 creatures until end of turn. They're still lands. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		create_legacy_effect(instance->parent_controller, instance->parent_card, &effect_thelonite_druid);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_CREATURE, MANACOST_XG(1, 1), 0, NULL, NULL);
}

static int effect_thelonite_monk(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( instance->damage_target_player > -1 ){
		if( event == EVENT_CHANGE_TYPE && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			event_result = land_into_new_basic_land_type(event_result, get_hacked_subtype(player, card, SUBTYPE_FOREST));
		}
	}

	return 0;
}

int card_thelonite_monk(int player, int card, event_t event){
/* CARD_ID_THELONITE_MONK	1832
Thelonite Monk	|2|G|G
Creature - Insect Monk Cleric 1/2
|T, Sacrifice a |Sgreen creature: Target land becomes |Ha Forest. (This effect lasts indefinitely.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_LAND);

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, get_sleighted_color_text(player, card, "Select a %s creature.", COLOR_GREEN));
	this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td1) ){
		create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &effect_thelonite_monk,
										instance->targets[0].player, instance->targets[0].card);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST0, 0, &td1, "TARGET_LAND",
																0, &this_test, 0);
}

int card_thelons_chant(int player, int card, event_t event){
/* CARD_ID_THELONS_CHANT	1829
Thelon's Chant	|1|G|G
Enchantment
At the beginning of your upkeep, sacrifice ~ unless you pay |G.
Whenever a player puts |Ha Swamp onto the battlefield, ~ deals 3 damage to that player unless he or she puts a -1/-1 counter on a creature he or she controls. */

	return fe_chant(player, card, event, SUBTYPE_SWAMP);
}

int card_thelons_curse(int player, int card, event_t event){

	/* Thelon's Curse	|G|G
	 * Enchantment
	 * |SBlue creatures don't untap during their controllers' untap steps.
	 * At the beginning of each player's upkeep, that player may choose any number of tapped |Sblue creatures he or she controls and pay |U for each creature
	 * chosen this way. If the player does, untap those creatures. */

	if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
		! is_humiliated(player, card) && (get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_BLUE))
	  ){
		get_card_instance(affected_card_controller, affected_card)->untap_status &= ~3;
	}

	// can untap them during upkeep
	upkeep_trigger_ability(player, card, event, ANYBODY);

	if (event == EVENT_UPKEEP_TRIGGER_ABILITY){
		int i, p = current_turn, blue = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		for (i = 0; i < active_cards_count[p]; i++){
			if( is_tapped(p, i) && is_what(p, i, TYPE_CREATURE) && in_play(p, i) && (get_color(p, i) & blue) ){
				if( has_mana(p, COLOR_BLUE, 1) ){
					int choice = do_dialog(p, player, card, p, i, " Pay U to untap\n Pass", 0);
					if( choice == 0 && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, p, COLOR_BLUE, 1) ){
						untap_card(p, i);
					}
				}
			}
		}
	}
	return global_enchantment(player, card, event);
}

int card_thorn_thallid(int player, int card, event_t event)
{
	/* Thorn Thallid	|1|G|G	0x2001f60
	 * Creature - Fungus 2/2
	 * At the beginning of your upkeep, put a spore counter on ~.
	 * Remove three spore counters from ~: ~ deals 1 damage to any target. */
  add_spore_counters(player, card, event);

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	damage_target0(player, card, 1);

  return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3),
									&td, "TARGET_ANY");
}


// artifacts and lands
int card_aeolipile(int player, int card, event_t event){
	/* Aeolipile	|2	0x2001f65
	 * Artifact
	 * |1, |T, Sacrifice ~: It deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_X(1), 0,
									&td, "TARGET_ANY");
}

int card_balm_of_restoration(int player, int card, event_t event){
	/* Balm of Restoration	|2	0x2001f6a
	 * Artifact
	 * |1, |T, Sacrifice ~: Choose one -
	 * * You gain 2 life.
	 * * Prevent the next 2 damage that would be dealt to any target this turn. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0);
	td1.extra = damage_card;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION | GAA_SACRIFICE_ME, MANACOST_X(1), 0, &td1, NULL) ){
			return 99;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(1), 0,NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abilities[2] = {
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(1), 0,NULL, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION |
															GAA_SACRIFICE_ME, MANACOST_X(1), 0, &td1, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
						"Gain 2 life", abilities[0], life[player] < 6 ? 10 : 5,
						"Prevent 2 damage", abilities[1], 10);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			if( choice == 2 ){
				pick_target(&td1, "TARGET_DAMAGE");
			}
			if( spell_fizzled != 1 ){
				tap_card(player, card);
				instance->info_slot = 1;
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			gain_life(player, 2);
		}
		if( instance->info_slot == 1 && valid_target(&td1) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			if( target->info_slot > 2 ){
				target->info_slot-=2;
			}
			else{
				target->info_slot = 0;
			}
		}
	}
	return 0;
}

static int storage_land(int player, int card, event_t event, int color){

	/* ~ enters the battlefield tapped.
	 * You may choose not to untap ~ during your untap step.
	 * At the beginning of your upkeep, if ~ is tapped, put a storage counter on it.
	 * |T, Remove any number of storage counters from ~: Add |C to your mana pool for each storage counter removed this way. */

	comes_into_play_tapped(player, card, event);

	if( is_tapped(player, card) ){
		upkeep_trigger_ability(player, card, event, player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			add_counter(player, card, COUNTER_STORAGE);
		}
	}
	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}
	if( event == EVENT_CAN_ACTIVATE ){
		return CAN_TAP_FOR_MANA(player, card) && (!IS_AI(player) || count_counters(player, card, COUNTER_STORAGE) > 0);
	}
	if( event == EVENT_ACTIVATE ){
			int counter_count = count_counters(player, card, COUNTER_STORAGE);
			int number;
			do {
				number = choose_a_number(player, "Remove how many counters?", counter_count);
			} while (number > counter_count);

			if (number == -1){
				cancel = 1;
			} else {
				remove_counters(player, card, COUNTER_STORAGE, number);
				produce_mana_tapped(player, card, color, number);
			}
	}
	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
			if( CAN_TAP_FOR_MANA(player, card) ){
				if( check_special_flags2(player, card, SF2_CONTAMINATION) ){
					declare_mana_available(player, COLOR_BLACK, 1);
				}
				else{
					declare_mana_available(player, color, count_counters(player, card, COUNTER_STORAGE));
				}
			}
	}

	if (choosing_to_untap(player, card, event)){
		return count_counters(player, card, COUNTER_STORAGE) > 2;
	}
	return 0;
}

int card_bottomless_vault(int player, int card, event_t event){
	/* Bottomless Vault	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * You may choose not to untap ~ during your untap step.
	 * At the beginning of your upkeep, if ~ is tapped, put a storage counter on it.
	 * |T, Remove any number of storage counters from ~: Add |B to your mana pool for each storage counter removed this way. */
	return storage_land(player, card, event, COLOR_BLACK);
}

int card_conch_horn(int player, int card, event_t event){
/* CARD_ID_CONCH_HORN	1757
Conch Horn	|2
Artifact
|1, |T, Sacrifice ~: Draw two cards, then put a card from your hand on top of your library. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 2);
		if( hand_count[player] > 0 ){
			char msg[100] = "Select a card to put on top.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, msg);
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &this_test);
			put_on_top_of_deck(player, selected);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL);
}

static int effect_delifs_cone(int player, int card, event_t event)
{
  if (event == EVENT_DECLARE_BLOCKERS)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int p = instance->damage_target_player;
	  int c = instance->damage_target_card;

	  if (is_unblocked(p, c)
		  && DIALOG(player, card, EVENT_ACTIVATE,
					DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_RANDOM, DLG_SMALLCARD(p, c),
					"Gain life", 1, 5,
					"Pass", 1, 1) == 1)
		{
		  instance->targets[3].player = 1;
		  gain_life(player, get_power(p, c));
		}
	}

  if (event == EVENT_PREVENT_DAMAGE)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->targets[3].player == 1)
		no_combat_damage_this_turn(player, card, event);
	}

  if (event == EVENT_CLEANUP)
	kill_card(player, card, KILL_EXILE);

  return 0;
}

int card_delifs_cone(int player, int card, event_t event){
/* CARD_ID_DELIFS_CONE	1759
Delif's Cone	|0
Artifact
|T, Sacrifice ~: This turn, when target creature you control attacks and isn't blocked, you may gain life equal to its power.
If you do, it assigns no combat damage this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE && life[player] < 6 ){
		ai_modifier+=50;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		create_targetted_legacy_effect(player, card, &effect_delifs_cone, player, instance->targets[0].card);
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td, "ASHNODS_BATTLEGEAR");	// Select target creature you control.
}

static int effect_delifs_cube(int player, int card, event_t event)
{
  if (event == EVENT_DECLARE_BLOCKERS)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int p = instance->damage_target_player;
	  int c = instance->damage_target_card;

	  if (is_unblocked(p, c))
		{
		  instance->targets[3].player = 1;
		  add_counter(instance->damage_source_player, instance->damage_source_card, COUNTER_CUBE);
		}
	}

  if (event == EVENT_PREVENT_DAMAGE)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->targets[3].player == 1)
		no_combat_damage_this_turn(player, card, event);
	}

  if (event == EVENT_CLEANUP)
	kill_card(player, card, KILL_EXILE);

  return 0;
}

int card_delifs_cube(int player, int card, event_t event){
	/* Delif's Cube	|1
	 * Artifact
	 * |2, |T: This turn, when target creature you control attacks and isn't blocked, it assigns no combat damage this turn and you put a cube counter on ~.
	 * |2, Remove a cube counter from ~: Regenerate target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.preferred_controller = player;
	td1.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED | GAA_NONSICK, MANACOST_X(2), 0, &td, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL) ){
		      if( (land_can_be_played & LCBP_REGENERATION) && can_target(&td1) && count_counters(player, card, COUNTER_CUBE) > 0 ){
				       return 99;
		      }
		}
	}
	if( event == EVENT_ACTIVATE ){
	  instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			if( (land_can_be_played & LCBP_REGENERATION) && can_target(&td1) && count_counters(player, card, COUNTER_CUBE) > 0 ){
				if( pick_target(&td1, "TARGET_CREATURE") ){
					remove_counter(player, card, COUNTER_CUBE);
					instance->info_slot = 66;
				}
			}
			else{
				if (current_phase > PHASE_BEFORE_BLOCKING){
					ai_modifier -= 96;
				} else if( count_counters(player, card, COUNTER_CUBE) < 1 ){
					ai_modifier += 50;
				}
				if( pick_target(&td, "ASHNODS_BATTLEGEAR") ){	// Select target creature you control.
					add_state(player, card, STATE_TAPPED);
					instance->info_slot = 67;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 66 && valid_target(&td1) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target( instance->targets[0].player, instance->targets[0].card );
		}
		if( instance->info_slot == 67 && valid_target(&td) ){
			create_targetted_legacy_effect(player, card, effect_delifs_cube, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_draconian_cylix(int player, int card, event_t event){
/* CARD_ID_DRACONIAN_CYLIX	1762
Draconian Cylix	|3
Artifact
|2, |T, Discard a card at random: Regenerate target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_REGENERATION | GAA_DISCARD | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, NULL);
	}
	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_REGENERATION | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(2), 0,
									&td, "Target creature to regenerate.");
		if( spell_fizzled != 1 ){
			discard(player, DISC_RANDOM, player);
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target( instance->targets[0].player, instance->targets[0].card );
		}
	}
	return 0;
}

int card_dwarven_hold(int player, int card, event_t event){
	/* Dwarven Hold	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * You may choose not to untap ~ during your untap step.
	 * At the beginning of your upkeep, if ~ is tapped, put a storage counter on it.
	 * |T, Remove any number of storage counters from ~: Add |R to your mana pool for each storage counter removed this way. */
	return storage_land(player, card, event, COLOR_RED);
}

int card_dwarven_ruins(int player, int card, event_t event){
/* CARD_ID_DWARVEN_RUINS	1767
Dwarven Ruins	""
Land
~ enters the battlefield tapped.
|T: Add |R to your mana pool.
|T, Sacrifice ~: Add |R|R to your mana pool. */

	return sac_land_tapped(player, card, event, COLOR_RED, COLOR_RED, COLOR_RED);
}

int card_ebon_stronghold(int player, int card, event_t event){
/* CARD_ID_EBON_STRONGHOLD	1770
Ebon Stronghold	""
Land
~ enters the battlefield tapped.
|T: Add |B to your mana pool.
|T, Sacrifice ~: Add |B|B to your mana pool. */

	return sac_land_tapped(player, card, event, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK);
}

int card_elven_lyre(int player, int card, event_t event){
/* CARD_ID_ELVEN_LYRE	1772
Elven Lyre	|2
Artifact
|1, |T, Sacrifice ~: Target creature gets +2/+2 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_X(1), 0, &td, "TARGET_CREATURE");
}

int card_havenwood_battleground(int player, int card, event_t event){
/* CARD_ID_HAVENWOOD_BATTLEGROUND	1788
Havenwood Battleground	""
Land
~ enters the battlefield tapped.
|T: Add |G to your mana pool.
|T, Sacrifice ~: Add |G|G to your mana pool. */

	return sac_land_tapped(player, card, event, COLOR_GREEN, COLOR_GREEN, COLOR_GREEN);
}

int card_hollow_trees(int player, int card, event_t event){
	/* Hollow Trees	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * You may choose not to untap ~ during your untap step.
	 * At the beginning of your upkeep, if ~ is tapped, put a storage counter on it.
	 * |T, Remove any number of storage counters from ~: Add |G to your mana pool for each storage counter removed this way. */
	return storage_land(player, card, event, COLOR_GREEN);
}

int card_icatian_store(int player, int card, event_t event){
	/* Icatian Store	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * You may choose not to untap ~ during your untap step.
	 * At the beginning of your upkeep, if ~ is tapped, put a storage counter on it.
	 * |T, Remove any number of storage counters from ~: Add |W to your mana pool for each storage counter removed this way. */
	return storage_land(player, card, event, COLOR_WHITE);
}

int card_implements_of_sacrifice(int player, int card, event_t event){
/* CARD_ID_IMPLEMENTS_OF_SACRIFICE	1805
Implements of Sacrifice	|2
Artifact
|1, |T, Sacrifice ~: Add two mana of any one color to your mana pool. */

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL);
	}
	if( event == EVENT_ACTIVATE ){
		ai_modifier -= 36;
		charge_mana_multi(player, MANACOST_X(1));
		if( cancel != 1  ){
			produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, 2);
			if (cancel != 1){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}
	return 0;
}

static int effect_rainbow_vale(int player, int card, event_t event){
	if( eot_trigger(player, card, event) ){
		card_instance_t *instance = get_card_instance(player, card);
		give_control_of_self(instance->damage_target_player, instance->damage_target_card);
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_rainbow_vale(int player, int card, event_t event){
/* CARD_ID_RAINBOW_VALE	1815
Rainbow Vale	""
Land
|T: Add one mana of any color to your mana pool. An opponent gains control of ~ at the beginning of the next end step. */

	if( event == EVENT_ACTIVATE ){
		ai_modifier+=15;
		mana_producer(player, card, event);
		if( chosen_colors ){
			int legacy = create_targetted_legacy_effect(player, card, &effect_rainbow_vale, player, card);
			if( current_phase == PHASE_DISCARD ){
				remove_state(player, legacy, STATE_PROCESSING);
			}
		}
	}
	else{
		return mana_producer(player, card, event);
	}

	return 0;
}

int card_ring_of_renewal(int player, int card, event_t event){
/* CARD_ID_RING_OF_RENEWAL	1816
Ring of Renewal	|5
Artifact
|5, |T: Discard a card at random, then draw two cards. */

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD, MANACOST_X(5), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		charge_mana_for_activated_ability(player, card,  MANACOST_X(5));
		if( spell_fizzled !=1 ){
			tap_card(player, card);
			discard(player, DISC_RANDOM, player);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 2);
	}

	return 0;
}

int card_ruins_of_trokair(int player, int card, event_t event){
/* CARD_ID_RUINS_OF_TROKAIR	1818
Ruins of Trokair	""
Land
~ enters the battlefield tapped.
|T: Add |W to your mana pool.
|T, Sacrifice ~: Add |W|W to your mana pool. */

	return sac_land_tapped(player, card, event, COLOR_WHITE, COLOR_WHITE, COLOR_WHITE);
}

int card_sand_silos(int player, int card, event_t event){
	/* Sand Silos	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * You may choose not to untap ~ during your untap step.
	 * At the beginning of your upkeep, if ~ is tapped, put a storage counter on it.
	 * |T, Remove any number of storage counters from ~: Add |U to your mana pool for each storage counter removed this way. */
	return storage_land(player, card, event, COLOR_BLUE);
}

static int sword_and_shield(int player, int card, event_t event, int cost, int pow, int tou){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	choose_to_untap(player, card, event);

	if( instance->targets[1].card > -1 && ! in_play(player, instance->targets[1].card) ){
		instance->targets[1].card = -1;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_t pump;
			default_pump_ability_definition(instance->parent_controller, instance->parent_card, &pump, pow, tou, 0, 0);
			pump.paue_flags = PAUE_END_IF_SOURCE_UNTAP;
			pump_ability(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, &pump);
			get_card_instance(instance->parent_controller, instance->parent_card)->targets[1] = instance->targets[0];
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(cost), 0, &td, "TARGET_CREATURE");
}

int card_spirit_shield(int player, int card, event_t event){
/* CARD_ID_SPIRIT_SHIELD	1822
Spirit Shield	|3
Artifact
You may choose not to untap ~ during your untap step.
|2, |T: Target creature gets +0/+2 for as long as ~ remains tapped. */

	return sword_and_shield(player, card, event, 2, 0, 2);
}

int card_svyelunite_temple(int player, int card, event_t event){
/* CARD_ID_SVYELUNITE_TEMPLE	1826
Svyelunite Temple	""
Land
~ enters the battlefield tapped.
|T: Add |U to your mana pool.
|T, Sacrifice ~: Add |U|U to your mana pool. */

	return sac_land_tapped(player, card, event, COLOR_BLUE, COLOR_BLUE, COLOR_BLUE);
}

int card_zelyon_sword(int player, int card, event_t event){
/* CARD_ID_ZELYON_SWORD	1844
Zelyon Sword	|3
Artifact
You may choose not to untap ~ during your untap step.
|3, |T: Target creature gets +2/+0 for as long as ~ remains tapped. */

	return sword_and_shield(player, card, event, 3, 2, 0);
}
