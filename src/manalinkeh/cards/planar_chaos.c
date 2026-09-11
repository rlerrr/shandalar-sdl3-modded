#include "manalink.h"

// Functions
void vanishing(int player, int card, event_t event, int number)
{
  /* 702.62. Vanishing
   * 702.62a Vanishing is a keyword that represents three abilities. "Vanishing N" means "This permanent enters the battlefield with N time counters on it," "At
   * the beginning of your upkeep, if this permanent has a time counter on it, remove a time counter from it," and "When the last time counter is removed from
   * this permanent, sacrifice it."
   * 702.62b Vanishing without a number means "At the beginning of your upkeep, if this permanent has a time counter on it, remove a time counter from it" and
   * "When the last time counter is removed from this permanent, sacrifice it."
   * 702.62c If a permanent has multiple instances of vanishing, each works separately. */

  enters_the_battlefield_with_counters(player, card, event, COUNTER_TIME, number);

  upkeep_trigger_ability(player, card, event, player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	remove_counter(player, card, COUNTER_TIME);

  if (event == EVENT_STATIC_EFFECTS)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int cur_time_counters = count_counters(player, card, COUNTER_TIME);
	  if (cur_time_counters == 0)
		{
		  if (instance->eot_toughness > 0)
			kill_card(player, card, KILL_SACRIFICE);
		}
	  else
		instance->eot_toughness = cur_time_counters;
	}
}

static void firefright(int player, int card, event_t event ){

	if( event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card) ){
		if( player == attacking_card_controller && card == attacking_card ){
			int score = 1;

			if( is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) ){
				score = 0;
			}

			if( get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_RED) ){
				score = 0;
			}

			if( score ){
				event_result = 1;
			}
		}
	}
}


// Cards
int card_aeon_chronicler(int player, int card, event_t event){
	/* Aeon Chronicler	|3|U|U
	 * Creature - Avatar 100/100
	 * ~'s power and toughness are each equal to the number of cards in your hand.
	 * Suspend X-|X|3|U. X can't be 0.
	 * Whenever a time counter is removed from ~ while it's exiled, draw a card. */

	if( event == EVENT_MODIFY_COST ){
		card_instance_t* instance = get_card_instance(player, card);
		if( has_mana_multi(player, MANACOST_XU(4, 1)) ){ // Can't be suspended with X = 0
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 1;

			if( has_mana_to_cast_iid(player, event, instance->internal_card_id) ){
				int ai_choice = 1;
				if( hand_count[player] > 3 ){
					ai_choice = 0;
				}
				choice = do_dialog(player, player, card, -1, -1, " Play normally\n Suspend\n Do nothing", ai_choice);
			}

			if( choice == 0 ){
				charge_mana_from_id(player, -1, event, get_id(player, card));
			}
			else if( choice == 1 ){
					charge_mana_multi(player, MANACOST_XU(3, 1));
					if( spell_fizzled != 1 ){
						charge_mana(player, COLOR_COLORLESS, -1);
						if( x_value == 0 ){ // Can't be suspended with X = 0
							spell_fizzled = 1;
						}
						if( spell_fizzled != 1 ){
							suspend_a_card(player, card, player, card, x_value);
							land_can_be_played &= ~LCBP_SPELL_BEING_PLAYED;
						}
					}
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	// Please see http://www.slightlymagic.net/forum/viewtopic.php?t=18981 before changing this again
	if( player != -1 && !is_humiliated(player, card) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
			event_result += hand_count[player];
		}
	}

	return 0;
}

int card_akroma_angel_of_fury(int player, int card, event_t event)
{
  /* Akroma, Angel of Fury	|5|R|R|R
   * Legendary Creature - Angel 6/6
   * ~ can't be countered.
   * Flying, trample, protection from |Swhite and from |Sblue
   * |R: ~ gets +1/+0 until end of turn.
   * Morph |3|R|R|R */

  check_legend_rule(player, card, event);

  if (morph(player, card, event, MANACOST_XR(3,3)))
	return 1;

  // Uncounterable if unmorphed.
  if (event == EVENT_CAST_SPELL && affect_me(player, card) && cancel != 1)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (!(instance->targets[12].card == get_internal_card_id_from_csv_id(CARD_ID_FACE_DOWN_CREATURE)))
		instance->state |= STATE_CANNOT_TARGET;
	}

  if (event == EVENT_RESOLVE_SPELL)
	state_untargettable(player, card, 0);

  return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1,0);
}

int card_ana_battlemage(int player, int card, event_t event){
	/*
	  Ana Battlemage |2|G
	  Creature - Human Wizard 2/2
	  Kicker {2}{U} and/or {1}{B} (You may pay an additional {2}{U} and/or {1}{B} as you cast this spell.)
	  When Ana Battlemage enters the battlefield, if it was kicked with its {2}{U} kicker, target player discards three cards.
	  When Ana Battlemage enters the battlefield, if it was kicked with its {1}{B} kicker, tap target untapped creature and that creature deals damage equal to its power to its controller.
	*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, 0 );
		td1.zone = TARGET_ZONE_PLAYERS;
		td1.allow_cancel = 0;

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.illegal_state = TARGET_STATE_TAPPED;
		td.allow_cancel = 0;

		volver1(player, card, event, MANACOST_XU(2, 1), MANACOST_XB(1, 1), 10*can_target(&td1), 5*can_target(&td));
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		if( check_special_flags(player, card, SF_KICKED) ){
			target_definition_t td1;
			default_target_definition(player, card, &td1, 0 );
			td1.zone = TARGET_ZONE_PLAYERS;
			td1.allow_cancel = 0;

			if( can_target(&td1) && pick_target(&td1, "TARGET_PLAYER") ){
				instance->number_of_targets = 1;
				new_multidiscard(instance->targets[0].player, 3, 0, player);
			}
		}
		if( check_special_flags(player, card, SF_KICKED2) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE );
			td.illegal_state = TARGET_STATE_TAPPED;
			td.allow_cancel = 0;

			if( can_target(&td) && new_pick_target(&td, "Select target untapped creature.", 0, GS_LITERAL_PROMPT) ){
				instance->number_of_targets = 1;
				action_on_target(player, card, 0, ACT_TAP);
				damage_player(instance->targets[0].player, get_power(instance->targets[0].player, instance->targets[0].card), instance->targets[0].player, instance->targets[0].card);
			}
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_auramancers_guise(int player, int card, event_t event){
/* CARD_ID_AURAMANCERS_GUISE	8742
Auramancer's Guise	|2|U|U
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2 for each Aura attached to it and has vigilance. */

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			event_result += count_auras_enchanting_me(instance->damage_target_player, instance->damage_target_card) * 2;
		}
		if (event == EVENT_ABILITIES && affect_me(instance->damage_target_player, instance->damage_target_card)){
			vigilance(instance->damage_target_player, instance->damage_target_card, event);
		}
	}

	return vanilla_aura(player, card, event, player);
}

int card_aven_riftwatcher(int player, int card, event_t event){
	/* Aven Riftwatcher	|2|W
	 * Creature - Bird Rebel Soldier 2/3
	 * Flying
	 * Vanishing 3
	 * When ~ enters the battlefield or leaves the battlefield, you gain 2 life. */

	vanishing(player, card, event, 3);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || leaves_play(player, card, event) ){
		gain_life(player, 2);
	}

	return 0;
}

int card_benalish_commander(int player, int card, event_t event)
{
  /* Benalish Commander	|3|W
   * Creature - Human Soldier 100/100
   * ~'s power and toughness are each equal to the number of Soldiers you control.
   * Suspend X-|X|W|W. X can't be 0.
   * Whenever a time counter is removed from ~ while it's exiled, put a 1/1 |Swhite Soldier creature token onto the battlefield. */

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && player != -1 && affect_me(player, card) && ! is_humiliated(player, card) )
	event_result += count_subtype(player, TYPE_PERMANENT, SUBTYPE_SOLDIER);

  return suspend(player, card, event, -1, MANACOST_XW(-1,2), NULL, NULL);
}

int card_big_game_hunter(int player, int card, event_t event){
/* CARD_ID_BIG_GAME_HUNTER	8745
Big Game Hunter	|1|B|B
Creature - Human Rebel Assassin 1/1
When ~ enters the battlefield, destroy target creature with power 4 or greater. It can't be regenerated.
Madness |B (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.)*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.power_requirement = 4 | TARGET_PT_GREATER_OR_EQUAL;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) ){
			if( new_pick_target(&td, "Select target creature with power 4 or greater.", 0, GS_LITERAL_PROMPT) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
				instance->number_of_targets = 0;
			}
		}
		instance->number_of_targets = 0;
	}

	return madness(player, card, event, MANACOST_B(1));
}

int card_blightspeaker(int player, int card, event_t event){
/*
Blightspeaker |1|B
Creature — Human Rebel Cleric 1/1, 1B (2)
{T}: Target player loses 1 life.
{4}, {T}: Search your library for a Rebel permanent card with converted mana cost 3 or less and put it onto the battlefield.
	Then shuffle your library.
*/
	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->targets[1].card = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_LOSE_LIFE = 1,
		CHOICE_TUTOR
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL)
		};
		if( abils[2] && IS_AI(player) && instance->targets[1].card == -1 ){
			abils[2] = 0;
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Make target lose 1 life", 	abils[1], life[1-player] < 6 ? 10 : 1,
							"Tutor a Rebel", 			abils[2], life[1-player] > 6 ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_LOSE_LIFE ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
		}
		if( choice == CHOICE_TUTOR ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_LOSE_LIFE && valid_target(&td) ){
			lose_life(instance->targets[0].player, 1);
		}
		if( instance->info_slot == CHOICE_TUTOR ){
			card_instance_t *parent = get_card_instance(player, instance->parent_card);

			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a Rebel permanent card with CMC 3 or less.");
			this_test.subtype = SUBTYPE_REBEL;
			this_test.cmc = 4;
			this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
			parent->targets[1].card = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

int card_body_double(int player, int card, event_t event)
{
  /* Body Double	|4|U
   * Creature - Shapeshifter 0/0
   * You may have ~ enter the battlefield as a copy of any creature card in a graveyard. */

  if (event == EVENT_CAST_SPELL && affect_me(player, card) && !has_dead_creature(ANYBODY))
	ai_modifier -= 100;

  int iid = -1;
  if (event == EVENT_RESOLVE_SPELL)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
	  if (IS_AI(player))	// Prevent AI from looping if the only creature in a graveyard is another Body Double.
		{
		  test.id = CARD_ID_BODY_DOUBLE;
		  test.id_flag = DOESNT_MATCH;
		}

	  iid = select_target_from_either_grave(player, card, SFG_NOTARGET, AI_MAX_VALUE, AI_MAX_VALUE, &test, 0, 1);
	}

  enters_the_battlefield_as_copy_of(player, card, event, -1, iid);	// enters_the_battlefield_as_copy_of() only examines iid during EVENT_RESOLVE_SPELL

  return 0;
}

int card_bog_serpent(int player, int card, event_t event)
{
/* CARD_ID_BOG_SERPENT	8748
Bog Serpent	|5|B
Creature - Serpent 5/5
~ can't attack unless defending player controls |Ha Swamp.
When you control no |H1Swamps, sacrifice ~. */

	if( event == EVENT_ATTACK_LEGALITY && affect_me(player, card) && ! is_humiliated(player, card) &&
		! basiclandtypes_controlled[1-player][get_hacked_color(player, card, COLOR_BLACK)])
		event_result = 1;

	// When you control no Swamps, sacrifice ~.
	if( in_play(player, card) && ! is_humiliated(player, card) &&
		! basiclandtypes_controlled[player][get_hacked_color(player, card, COLOR_BLACK)])
		kill_card(player, card, KILL_SACRIFICE);

	return 0;
}

int card_boom_bust(int player, int card, event_t event){
	/*
	  Boom |1|R
	  Sorcery
	  Destroy target land you control and target land you don't control.

	  Bust |5|R
	  Sorcery
	  Destroy all lands.
	*/
	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.allowed_controller = player;
		td.preferred_controller = player;

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_LAND);
		td2.allowed_controller = 1-player;

		return generic_split_card(player, card, event, can_target(&td) && can_target(&td2), 0, MANACOST_XR(5, 1), 1, 0, 0, NULL, NULL);
	}

	if( event == EVENT_PLAY_FIRST_HALF ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.allowed_controller = player;
		td.preferred_controller = player;

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_LAND);
		td2.allowed_controller = 1-player;

		if( new_pick_target(&td, "Select target land you control.", 0, 1 | GS_LITERAL_PROMPT) ){
			new_pick_target(&td2, "Select target land opponent controls.", 1, 1 | GS_LITERAL_PROMPT);
		}
	}


	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.allowed_controller = player;
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_LAND);
	td2.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	int priority_bust = 5;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		if( (instance->info_slot & (1<<8)) || played_for_free(player, card) || (is_token(player, card) && ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK)) ){
			priority_bust = 5 +(5*((count_subtype(player, TYPE_CREATURE, -1)-count_subtype(1-player, TYPE_CREATURE, -1)-1)));
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot & 1 ){
			if( validate_target(player, card, &td, 0) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
			if( validate_target(player, card, &td2, 1) ){
				kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
			}
		}
		if( instance->info_slot & 2 ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_LAND);
			new_manipulate_all(player, card, 2, &this_test, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, can_target(&td) && can_target(&td2), 10, MANACOST_XR(5, 1), 1, priority_bust, 0, "Boom", "Bust");
}

int card_braids_conjurer_adept(int player, int card, event_t event){
/* CARD_ID_BRAIDS_CONJURER_ADEPT	8750
Braids, Conjurer Adept	|2|U|U
Legendary Creature - Human Wizard 2/2
At the beginning of each player's upkeep, that player may put an artifact, creature, or land card from his or her hand onto the battlefield.*/

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( hand_count[current_turn] > 0 ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ARTIFACT | TYPE_CREATURE | TYPE_LAND, "Select an artifact, creature, or land card.");
			new_global_tutor(current_turn, current_turn, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
	}

	return 0;
}

// brute force --> giant growth

int card_calciderm(int player, int card, event_t event){
	/* Calciderm	|2|W|W
	 * Creature - Beast 5/5
	 * Shroud
	 * Vanishing 4 */
	vanishing(player, card, event, 4);
	return 0;
}

static const char* is_damage_to_player_or_planeswalker_or_sliver_creature(int who_chooses, int player, int card, int targeting_player, int targeting_card){
	card_instance_t* dmg = get_card_instance(player, card);
	if( dmg->internal_card_id != damage_card ){
		return ",damage";
	}
	if( dmg->damage_target_card >= 0 &&
		!damage_is_to_planeswalker(dmg) &&
		!has_subtype(dmg->damage_target_player, dmg->damage_target_card, SUBTYPE_SLIVER)
	  ){
		return ",damage to a player, planeswalker, or Sliver creature";
	}
	return NULL;
}

int cautery_sliver_shared_ability(int player, int card, event_t event){

	if( get_card_instance(player, card)->targets[1].player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->targets[1].player;
		int c = instance->targets[1].card;

		if( leaves_play(p, c, event) ){
			kill_card(player, card, KILL_EXILE);
		}

		if( is_humiliated(p, c) ){
			return 0;
		}

		if( ! IS_GAA_EVENT(event) ){
			return 0;
		}

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.preferred_controller = player;
		td.required_subtype = SUBTYPE_SLIVER;
		td.extra = (int32_t)activating_sliver;
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		td.illegal_abilities = 0;

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td2.allow_cancel = 0;

		target_definition_t td3;
		base_target_definition(player, card, &td3, 0);
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		td.extra = (int32_t)is_damage_to_player_or_planeswalker_or_sliver_creature;
		td3.allow_cancel = 0;

		if( event == EVENT_CAN_ACTIVATE && in_play(player, card) ){
			int pp, cc, result = 0;
			for(pp = 0; pp<2; pp++){
				for(cc=0; cc < active_cards_count[pp]; cc++){
					if( in_play(pp, cc) && is_what(pp, cc, TYPE_PERMANENT) && has_subtype(pp, cc, SUBTYPE_SLIVER) ){
						result = generic_activated_ability(pp, cc, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET,
															MANACOST_X(1), 0, &td3, NULL);
						if( result == 99 ){
							return result;
						}
						if( generic_activated_ability(pp, cc, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_X(1), 0, &td2, NULL) ){
							result = 1;
						}
					}
				}
			}
			return result;
		}

		if( event == EVENT_ACTIVATE ){
			if( new_pick_target(&td, "Select a Sliver to activate", 0, 1 | GS_LITERAL_PROMPT) ){
				instance->number_of_targets = 1;
				if( can_use_activated_abilities(instance->targets[0].player, instance->targets[0].card) ){
					if( generic_activated_ability(instance->targets[0].player, instance->targets[0].card, EVENT_CAN_ACTIVATE,
													GAA_SACRIFICE_ME | GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST_X(1), 0, &td3, NULL)
					  ){
						generic_activated_ability(instance->targets[0].player, instance->targets[0].card, EVENT_CAN_ACTIVATE,
													GAA_SACRIFICE_ME | GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST_X(1), 0, &td3, "TARGET_DAMAGE") ;
						if( spell_fizzled != 1 ){
							instance->targets[2] = get_card_instance(instance->targets[0].player, instance->targets[0].card)->targets[0];
							instance->targets[3].player = 1;
						}
					}
					else{
						generic_activated_ability(instance->targets[0].player, instance->targets[0].card, EVENT_CAN_ACTIVATE,
													GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_X(1), 0, &td2, "TARGET_ANY");
						if( spell_fizzled != 1 ){
							instance->targets[2] = get_card_instance(instance->targets[0].player, instance->targets[0].card)->targets[0];
							instance->targets[3].player = 2;
						}
					}
				}
				else{
					spell_fizzled = 1;
				}
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( instance->targets[3].player == 1 ){
				if( would_validate_arbitrary_target(&td3, instance->targets[2].player, instance->targets[2].card) ){
					card_instance_t *dmg = get_card_instance(instance->targets[2].player, instance->targets[2].card);
					int amount = dmg->info_slot;
					dmg->info_slot = amount > 1 ? amount-1: 0;
				}
			}
			if( instance->targets[3].player == 2 ){
				if( would_validate_arbitrary_target(&td2, instance->targets[2].player, instance->targets[2].card) ){
					damage_creature(instance->targets[2].player, instance->targets[2].card, 1, instance->targets[0].player, instance->targets[0].card);
				}
			}
		}
	}

	return 0;
}

int card_cautery_sliver(int player, int card, event_t event){
	/* Cautery Sliver	|R|W	0x20071bd
	 * Creature - Sliver 2/2
	 * All Slivers have "|1, Sacrifice this permanent: This permanent deals 1 damage to any target."
	 * All Slivers have "|1, Sacrifice this permanent: Prevent the next 1 damage that would be dealt to target player, planeswalker, or Sliver creature this turn." */
	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &cautery_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &cautery_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

int card_chronozoa(int player, int card, event_t event)
{
  /* Chronozoa	|3|U
   * Creature - Illusion 3/3
   * Flying
   * Vanishing 3
   * When ~ dies, if it had no time counters on it, put two tokens that are copies of it onto the battlefield. */

	vanishing(player, card, event, 3);

	if( count_counters(player, card, COUNTER_TIME) == 0 && this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) )
	{
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_CHRONOZOA, &token);
		token.qty = 2;
		token.no_sleight = 1;
		generate_token(&token);
	}

	return 0;
}

int card_circle_of_affliction(int player, int card, event_t event){
/*
Circle of Affliction |1|B
Enchantment
As Circle of Affliction enters the battlefield, choose a color.
Whenever a source of the chosen color deals damage to you, you may pay {1}. If you do, target player loses 1 life and you gain 1 life.
*/

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = 1<<choose_a_color_and_show_legacy(player, card, 1-player, -1);
	}

	if( event == EVENT_DEAL_DAMAGE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_card_instance(player, card)->info_slot;
		check_damage_test(ANYBODY, -1, event, DDBM_MUST_DAMAGE_TRIGGERING_PLAYER, player, card, 0, &test);
	}

	int result = resolve_damage_trigger(player, card, event, DDBM_MUST_DAMAGE_TRIGGERING_PLAYER, player, card);
	if( result ){
		card_instance_t *instance = get_card_instance(player, card);
		int i;
		for(i=0; i<result; i++){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.zone = TARGET_ZONE_PLAYERS;
			if( can_target(&td) && has_mana(player, COLOR_ANY, 1) &&
				charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1) )
			{
				if( pick_target(&td, "TARGET_PLAYER") ){
					lose_life(instance->targets[0].player, 1);
					gain_life(player, 1);
				}
				else{
					break;
				}
			}
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_citanul_woodreaders(int player, int card, event_t event){
/* CARD_ID_CITANUL_WOODREADERS	8757
Citanul Woodreaders	|2|G
Creature - Human Druid 1/4
Kicker |2|G (You may pay an additional |2|G as you cast this spell.)
When ~ enters the battlefield, if it was kicked, draw two cards. */

	etb_ability(player, card, event, kicked(player, card) ? RESOLVE_TRIGGER_MANDATORY : 0);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 2);
	}

	return kicker(player, card, event, MANACOST_XG(2, 1));
}

int card_crovax_ascendant_hero(int player, int card, event_t event){
/* CARD_ID_CROVAX_ASCENDANT_HERO	8759
Crovax, Ascendant Hero	|4|W|W
Legendary Creature - Human 4/4
Other |Swhite creatures get +1/+1.
Non|Swhite creatures get -1/-1.
Pay 2 life: Return ~ to its owner's hand. */

	check_legend_rule(player, card, event);

	if( IS_GAA_EVENT(event) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( event == EVENT_RESOLVE_ACTIVATION ){
			bounce_permanent(instance->parent_controller, instance->parent_card);
		}
		return generic_activated_ability(player, card, event, 0, MANACOST0, 3, NULL, NULL);
	}

	return evincars(player, card, event, COLOR_TEST_WHITE);
}

int darkheart_sliver_shared_ability(int player, int card, event_t event){

	if( get_card_instance(player, card)->targets[1].player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->targets[1].player;
		int c = instance->targets[1].card;

		if( leaves_play(p, c, event) ){
			add_status(player, card, STATUS_INVISIBLE_FX);
		}

		if( ! in_play(p, c) ){
			if( event == EVENT_CAN_ACTIVATE ){
				return 0;
			}

			if( event == EVENT_CLEANUP ){
				kill_card(player, card, KILL_EXILE);
			}
		}

		if( is_humiliated(p, c) ){
			return 0;
		}

		if( ! IS_GAA_EVENT(event) ){
			return 0;
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			gain_life(player, 3);
		}

		return shared_sliver_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
	}

	return 0;
}

int card_darkheart_sliver(int player, int card, event_t event){
	/*
	  Darkheart Sliver |B|G
	  Creature - Sliver 2/2
	  All Slivers have "Sacrifice this permanent: You gain 3 life."
	*/

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &darkheart_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &darkheart_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

int card_dash_hopes(int player, int card, event_t event){
	/* Dash Hopes	|B|B
	 * Instant
	 * When you cast ~, any player may pay 5 life. If a player does, counter ~.
	 * Counter target spell. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			int choice = 1;
			if( ! check_special_flags(player, card, SF_NOT_CAST) ){
				APNAP(p, {
						if( can_pay_life(p, 1) ){
							int ai_choice = 0;
							if( choice == 0 || life[p]-5 < 6 ){
								ai_choice = 1;
							}
							int p_choice = do_dialog(p, player, card, -1, -1," Counter Dash Hopes\n Pass", ai_choice);
							if( p_choice == 0 ){
								lose_life(p, 5);
								if( choice == 1 ){
									choice = 0;
								}
							}
						}
					}
				);
			}
			if( choice == 0 ){
				real_counter_a_spell(player, card, player, card);
				return 0;
			}
			else{
				card_instance_t *instance = get_card_instance(player, card);
				real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
}

int card_dawn_charm(int player, int card, event_t event){
/*
Dawn Charm |1|W
Instant
Choose one —
• Prevent all combat damage that would be dealt this turn.
• Regenerate target creature.
• Counter target spell that targets you.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	target_definition_t td2;
	counterspell_target_definition(player, card, &td2, TYPE_ANY);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_FOG = 1,
		CHOICE_REGENERATE,
		CHOICE_COUNTERSPELL
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET | GS_REGENERATION, &td, NULL, 1, NULL) )
		{
			return 99;
		}
		if( generic_spell(player, card, event, GS_COUNTERSPELL, &td2, NULL, 1, NULL) ){
			card_instance_t *spell = get_card_instance(card_on_stack_controller, card_on_stack);
			int i;
			for(i=0; i<spell->number_of_targets; i++){
				if( spell->targets[i].player == player && spell->targets[i].card == -1 ){
					return 99;
				}
			}
		}
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int abils[4] = {
						0,
						basic_spell(player, card, EVENT_CAN_CAST),
						generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET | GS_REGENERATION, &td, NULL, 1, NULL),
						0
		};
		if( generic_spell(player, card, EVENT_CAN_CAST, GS_COUNTERSPELL, &td2, NULL, 1, NULL) ){
			card_instance_t *spell = get_card_instance(card_on_stack_controller, card_on_stack);
			int i;
			for(i=0; i<spell->number_of_targets; i++){
				if( spell->targets[i].player == player && spell->targets[i].card == -1 ){
					abils[3] = 99;
					break;
				}
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Fog", abils[1], 5,
							"Regenerate target creature", abils[2], 10,
							"Counter a spell that targets you", abils[3], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_FOG ){
			return basic_spell(player, card, event);
		}
		if( choice == CHOICE_REGENERATE ){
			return generic_spell(player, card, event, GS_CAN_TARGET | GS_REGENERATION, &td, "TARGET_CREATURE", 1, NULL);
		}
		if( choice == CHOICE_COUNTERSPELL ){
			return generic_spell(player, card, event, GS_COUNTERSPELL, &td2, NULL, 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( instance->info_slot == CHOICE_FOG ){
			fog_effect(player, card);
		}
		if( instance->info_slot == CHOICE_REGENERATE && valid_target(&td) &&
			can_be_regenerated(instance->targets[0].player, instance->targets[0].card) )
		{
			regenerate_target(instance->targets[0].player, instance->targets[0].card);
		}
		if( instance->info_slot == CHOICE_COUNTERSPELL ){
			if( counterspell_validate(player, card, &td2, 0) ){
				int validated = 0;
				card_instance_t *spell = get_card_instance(instance->targets[0].player, instance->targets[0].card);
				int i;
				for(i=0; i<spell->number_of_targets; i++){
					if( spell->targets[i].player == player && spell->targets[i].card == -1 ){
						validated = 1;
						break;
					}
				}
				if( validated ){
					real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_dead_gone(int player, int card, event_t event){
	/*
	  Dead |R
	  Instant
	  Dead deals 2 damage to target creature.

	  Gone |2|R
	  Instant
	  Return target creature you don't control to its owner's hand.
	*/
	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.allowed_controller = 1-player;

		return generic_split_card(player, card, event, can_target(&td), 0, MANACOST_XR(2, 1), can_target(&td2), 0, 0, NULL, NULL);
	}

	if( event == EVENT_PLAY_FIRST_HALF ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		pick_target(&td, "TARGET_CREATURE");
	}

	if( event == EVENT_PLAY_SECOND_HALF ){
		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.allowed_controller = 1-player;
		new_pick_target(&td2, "Select target creature an opponent controls.", 0, 1 | GS_LITERAL_PROMPT);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( (instance->info_slot & 1) && valid_target(&td) ){
			damage_target0(player, card, 2);
		}
		if( (instance->info_slot & 2) && valid_target(&td2) ){
			action_on_target(player, card, 0, ACT_BOUNCE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, can_target(&td), 10, MANACOST_XR(2, 1), can_target(&td2), 8, 0, "Dead", "Gone");
}

int card_deadwood_treefolk(int player, int card, event_t event)
{
  /* Deadwood Treefolk	|5|G
   * Creature - Treefolk 3/6
   * Vanishing 3
   * When ~ enters the battlefield or leaves the battlefield, return another target creature card from your graveyard to your hand. */

	vanishing(player, card, event, 3);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( any_in_graveyard_by_type(player, TYPE_CREATURE) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select another creature card.");
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 1, AI_MAX_VALUE, &test);
		}
	}

	if( leaves_play(player, card, event) ){
		card_instance_t* instance = get_card_instance(player, card);
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select another creature card.");
		if( any_in_graveyard_by_type(player, TYPE_CREATURE) ){
			if( instance->kill_code >= KILL_DESTROY && instance->kill_code < KILL_EXILE ){
				tutor_from_grave_to_hand_except_for_dying_card(player, card, 1, AI_MAX_VALUE, &test);
			}
			else{
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 1, AI_MAX_VALUE, &test);
			}
		}
	}

	return 0;
}

int card_detritivore(int player, int card, event_t event)
{
  /* Detritivore	|2|R|R
   * Creature - Lhurgoyf 100/100
   * ~'s power and toughness are each equal to the number of nonbasic land cards in your opponents' graveyards.
   * Suspend X-|X|3|R. X can't be 0.
   * Whenever a time counter is removed from ~ while it's exiled, destroy target nonbasic land. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && player != -1 && affect_me(player, card) && !is_humiliated(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.subtype = SUBTYPE_BASIC;
		test.subtype_flag = DOESNT_MATCH;
		event_result += new_special_count_grave(1-player, &test);
	}

	return suspend(player, card, event, -1, MANACOST_XR(3, 1), NULL, NULL);
}

int card_dichotomancy(int player, int card, event_t event){
/* Dichotomancy	|7|U|U
 * Sorcery
 * For each tapped nonland permanent target opponent controls, search that player's library for a card with the same name as that permanent
 * 		and put it onto the battlefield under your control. Then that player shuffles his or her library.
 * Suspend 3-|1|U|U */

	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allowed_controller = 1-player;
		return suspend(player, card, event, 3, MANACOST_XU(1,2), &td, "TARGET_PLAYER");
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
    td.allowed_controller = 1-player;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int count;
			card_instance_t* instance = get_card_instance(player, card);
			for(count=0; count<active_cards_count[instance->targets[0].player]; count++){
				if( in_play(instance->targets[0].player, count) && is_tapped(instance->targets[0].player, count) &&
					! is_what(instance->targets[0].player, count, TYPE_LAND)
				  ){
					tutor_card_with_the_same_name(instance->targets[0].player, count, player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_PLAY, 0);
				}
			}
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return suspend(player, card, event, 3, MANACOST_XU(1,2), &td, "TARGET_PLAYER");
}

int card_dormant_sliver(int player, int card, event_t event)
{
  /* Dormant Sliver	|2|G|U
   * Creature - Sliver 2/2
   * All Sliver creatures have defender.
   * All Slivers have "When this permanent enters the battlefield, draw a card." */

	boost_subtype(player, card, event, SUBTYPE_SLIVER, 0, 0, KEYWORD_DEFENDER,0, BCT_INCLUDE_SELF);

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_SLIVER;

		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(trigger_cause_controller, 1);
	}

	return slivercycling(player, card, event);
}

// dunerider outlaw --> whirling dervish

int card_dust_elemental(int player, int card, event_t event){
	/* Dust Elemental	|2|W|W
	 * Creature - Elemental 6/6
	 * Flash
	 * Flying; fear
	 * When ~ enters the battlefield, return three creatures you control to their owner's hand. */

	fear(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		base_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		int i, selected = pick_up_to_n_targets_noload(&td, "Select a creature you control.", 3);
		for (i = 0; i < selected; ++i){
			if (instance->targets[i].card == card && instance->targets[i].player == player){
				ai_modifier += player == AI ? -64 : 64;
			}
			bounce_permanent(instance->targets[i].player, instance->targets[i].card);
		}
		instance->number_of_targets = 0;
	}

	return flash(player, card, event);
}

int card_enslave(int player, int card, event_t event ){
/* CARD_ID_ENSLAVE	8774
Enslave	|4|B|B
Enchantment - Aura
Enchant creature
You control enchanted creature.
At the beginning of your upkeep, enchanted creature deals 1 damage to its owner. */

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		upkeep_trigger_ability(player, card, event, player);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);
		int owner = get_owner(instance->damage_target_player, instance->damage_target_card);
		damage_player(owner, 1, instance->damage_target_player, instance->damage_target_card);
	}

	return card_control_magic(player, card, event);
}

// essence warden --> soul warden

int card_evolution_charm(int player, int card, event_t event){
	/* Evolution Charm	|1|G
	 * Instant
	 * Choose one -
	 * * Search your library for a basic land card, reveal it, put it into your hand, then shuffle your library.
	 * * Return target creature card from your graveyard to your hand.
	 * * Target creature gains flying until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_TUTOR = 1,
		CHOICE_RECYCLE,
		CHOICE_FLYING
	};

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int abils[4] = {
						0,
						basic_spell(player, card, EVENT_CAN_CAST),
						generic_spell(player, card, EVENT_CAN_CAST, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test),
						generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td, NULL, 1, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Tutor basic land", 			abils[1], get_total_available_lands(player) < 4 ? 5 : 1,
							"Return creature from grave", 	abils[2], current_turn == 1-player && current_phase >= PHASE_MAIN2 ? 10 : 1,
							"Give flying", 					abils[3], current_phase <= PHASE_DECLARE_BLOCKERS ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_TUTOR ){
			return basic_spell(player, card, event);
		}
		if( choice == CHOICE_RECYCLE ){
			return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
		}
		if( choice == CHOICE_FLYING ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_TUTOR ){
			tutor_basic_land(player, 0, 0);
		}
		if( instance->info_slot == CHOICE_RECYCLE ){
			int selected = validate_target_from_grave_source(player, card, player, 0);
			if( selected > -1 ){
				from_grave_to_hand(player, selected, TUTOR_HAND);
			}
		}
		if( instance->info_slot == CHOICE_FLYING ){
			if( valid_target(&td) ){
				pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FLYING, 0);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_extirpate(int player, int card, event_t event){
/* Extirpate	|B
 * Instant
 * Split second
 * Choose target card in a graveyard other than a basic land card. Search its owner's graveyard, hand, and library for all cards with
 * 		the same name as that card and exile them. Then that player shuffles his or her library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select target card to exile.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &test);
		if( spell_fizzled != 1 ){
			state_untargettable(player, card, 1);
			int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
			if( selected > -1 ){
				int csvid = cards_data[get_grave(instance->targets[0].player)[selected]].id;
				lobotomy_effect(player, instance->targets[0].player, csvid, 1);
			}

		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		state_untargettable(player, card, 0);
		kill_card( player, card, KILL_DESTROY );
	}

	return 0;
}

int card_erratic_mutation(int player, int card, event_t event){
/* CARD_ID_ERRATIC_MUTATION	8775
Erratic Mutation	|2|U
Instant
Choose target creature. Reveal cards from the top of your library until you reveal a nonland card.
That creature gets +X/-X until end of turn, where X is that card's converted mana cost. Put all cards revealed this way on the bottom of your library in any order.*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int *deck = deck_ptr[player];
			if( deck[0] != -1 ){
				int count = 0;
				int good = 0;
				while( deck[count] != -1 ){
						if( ! is_what(-1, deck[count], TYPE_LAND) ){
							good = 1;
							break;
						}
						count++;
				}
				show_deck( HUMAN, deck, count+1, "Cards revealed by Erratic Mutation", 0, 0x7375B0 );
				if( good == 1 ){
					int amount = get_cmc_by_id(cards_data[deck[count]].id);
					pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, amount, -amount);
				}
				put_top_x_on_bottom(player, player, count+1);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_fatal_frenzy(int player, int card, event_t event){
/* CARD_ID_FATAL_FRENZY	8780
Fatal Frenzy	|2|R
Instant
Until end of turn, target creature you control gains trample and gets +X/+0, where X is its power. Sacrifice it at the beginning of the next end step.*/

	if( event == EVENT_CHECK_PUMP ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->targets[0].player > -1 && instance->targets[0].card > -1 ){
			int amount = get_power(instance->targets[0].player, instance->targets[0].card);
			return vanilla_instant_pump(player, card, event, player, player, amount, 0, KEYWORD_TRAMPLE, 0);
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = td.allowed_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = get_power(instance->targets[0].player, instance->targets[0].card);
			int legacy = pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, amount, 0,
												KEYWORD_TRAMPLE, SP_KEYWORD_DIE_AT_EOT);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[3].card = KILL_SACRIFICE;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

static int firefright_effect(int player, int card, event_t event ){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		firefright(p, c, event);
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_firefright_mage(int player, int card, event_t event){//UNUSEDCARD
/* CARD_ID_FIREFRIGHT_MAGE	8781
Firefright Mage	|R
Creature - Goblin Spellshaper 1/1
|1|R, |T, Discard a card: Target creature can't be blocked this turn except by artifact creatures and/or |Sred creatures. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &firefright_effect,
											instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD, MANACOST_XR(1, 1), 0, &td, "TARGET_CREATURE");
}

int frenetic_sliver_shared_ability(int player, int card, event_t event){

	if( get_card_instance(player, card)->targets[1].player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->targets[1].player;
		int c = instance->targets[1].card;

		if( leaves_play(p, c, event) ){
			add_status(player, card, STATUS_INVISIBLE_FX);
		}

		if( ! in_play(p, c) ){
			if( event == EVENT_CAN_ACTIVATE ){
				return 0;
			}

			if( event == EVENT_CLEANUP ){
				kill_card(player, card, KILL_EXILE);
			}
		}

		if( is_humiliated(p, c) ){
			return 0;
		}

		if( ! IS_GAA_EVENT(event) ){
			return 0;
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( in_play(instance->targets[0].player, instance->targets[0].card) ){
				if( flip_a_coin(instance->targets[0].player, instance->targets[0].card) ){
					remove_until_eot(instance->targets[0].player, instance->targets[0].card, instance->targets[0].player, instance->targets[0].card);
				}
				else{
					kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
				}
			}
		}

		return shared_sliver_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
	}

	return 0;
}

int card_frenetic_sliver(int player, int card, event_t event){
	/*
	  Frenetic Sliver |1|U|R
	  Creature - Sliver 2/2
	  All Slivers have "{0}: If this permanent is on the battlefield, flip a coin.
	  If you win the flip, exile this permanent and return it to the battlefield under its owner's control at the beginning
	  of the next end step. If you lose the flip, sacrifice it."*/

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &frenetic_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &frenetic_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

int card_fungal_behemoth(int player, int card, event_t event)
{
  /* Fungal Behemoth	|3|G
   * Creature - Fungus 100/100
   * ~'s power and toughness are each equal to the number of +1/+1 counters on creatures you control.
   * Suspend X-|X|G|G. X can't be 0.
   * Whenever a time counter is removed from ~ while it's exiled, you may put a +1/+1 counter on target creature. */

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && player != -1 && affect_me(player, card) && ! is_humiliated(player, card) )
	{
		int c;
		for (c = 0; c < active_cards_count[player]; ++c)
			if (in_play(player, c) && is_what(player, c, TYPE_CREATURE))
				event_result += count_1_1_counters(player, c);
	}

	return suspend(player, card, event, -1, MANACOST_XG(-1, 2), NULL, NULL);
}

static const char* is_permanent_or_suspend_legacy(int who_chooses, int player, int card){
	if( is_what(player, card, TYPE_PERMANENT) ){
		return NULL;
	}
	if( is_what(player, card, TYPE_EFFECT) ){
		card_instance_t *inst = get_card_instance(player, card);
		if( inst->info_slot == (int)generic_suspend_legacy ){
			return NULL;
		}
	}
	return "must be a permanent or a Suspend legacy card.";
}

int card_fury_charm(int player, int card, event_t event){
/* Fury Charm	|1|R
 * Instant
 * Choose one - Destroy target artifact;
				or target creature gets +1/+1 and gains trample until end of turn;
				or remove two time counters from target permanent or suspended card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_PERMANENT | TYPE_EFFECT);
	td2.preferred_controller = ANYBODY;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = (int32_t)is_permanent_or_suspend_legacy;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_KILL_ARTIFACT = 1,
		CHOICE_GIVE_P1_P1_TRAMPLE,
		CHOICE_REMOVE_TIME_COUNTERS
	};

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return (can_target(&td) || can_target(&td1) || can_target(&td2)) ? 1 : 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Destroy artifact", can_target(&td), 1,
							"+1/+1 and Trample", can_target(&td1), current_phase == PHASE_AFTER_BLOCKING ? 10 : 1,
							"Remove 2 Time Counters", can_target(&td2), 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_KILL_ARTIFACT ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
		}
		if( choice == CHOICE_GIVE_P1_P1_TRAMPLE ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_CREATURE", 1, NULL);
		}
		if( choice == CHOICE_REMOVE_TIME_COUNTERS ){
			generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td2, "Select target permanent or Suspend legacy card.",
						1, NULL);
			if( spell_fizzled != 1 ){
				if( ! count_counters(instance->targets[0].player, instance->targets[0].card, COUNTER_TIME) ){
					ai_modifier -= 96;
				}
				if( instance->targets[0].player == player ){
					ai_modifier += (is_what(instance->targets[0].player, instance->targets[0].card, TYPE_EFFECT) ? 48 : -48);
				}
				if( instance->targets[0].player != player ){
					ai_modifier += (! is_what(instance->targets[0].player, instance->targets[0].card, TYPE_EFFECT) ? 48 : -48);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_KILL_ARTIFACT && valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}

		if( instance->info_slot == CHOICE_GIVE_P1_P1_TRAMPLE && valid_target(&td1) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 1, 1, KEYWORD_TRAMPLE, 0);
		}

		if( instance->info_slot == CHOICE_REMOVE_TIME_COUNTERS && valid_target(&td2) ){
			remove_counters(instance->targets[0].player, instance->targets[0].card, COUNTER_TIME, 2);
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_gaeas_anthem(int player, int card, event_t event){
	/*
	  Gaea's Anthem |1|G|G
	  Enchantment
	  Creatures you control get +1/+1.
	*/
	boost_creature_type(player, card, event, -1, 1, 1, 0, BCT_CONTROLLER_ONLY);
	return global_enchantment(player, card, event);
}

int card_giant_dustwasp(int player, int card, event_t event){
	/* Giant Dustwasp	|3|G|G
	 * Creature - Insect 3/3
	 * Flying
	 * Suspend 4-|1|G */
	return suspend(player, card, event, 4, MANACOST_XG(1, 1), NULL, NULL);
}

int card_hammerheim_deadeye(int player, int card, event_t event){
/* CARD_ID_HAMMERHEIM_DEADEYE	8788
Hammerheim Deadeye	|3|R
Creature - Giant Warrior 3/3
Echo |5|R (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)
When ~ enters the battlefield, destroy target creature with flying. */

	echo(player, card, event, MANACOST_XR(5, 1));

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.required_abilities = KEYWORD_FLYING;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td1) && new_pick_target(&td1, "TARGET_CREATURE", 0, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}
	return 0;
}


int card_hedge_troll(int player, int card, event_t event){
/* CARD_ID_HEDGE_TROLL	8790
Hedge Troll	|2|G
Creature - Troll Cleric 2/2
~ gets +1/+1 as long as you control |Ha Plains.
|W: Regenerate ~. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card) ){
		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_PLAINS) ){
			event_result++;
		}
	}

	return regeneration(player, card, event, MANACOST_W(1));
}

int card_heroes_remembered(int player, int card, event_t event){
	/* Heroes Remembered	|6|W|W|W
	 * Sorcery
	 * You gain 20 life.
	 * Suspend 10-|W */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, 20);
		kill_card(player, card, KILL_DESTROY);
	}

	return suspend(player, card, event, 10, MANACOST_W(1), NULL, NULL);
}

int card_intet_the_dreamer(int player, int card, event_t event)
{
  /* Intet, the Dreamer	|3|U|R|G
   * Legendary Creature - Dragon 6/6
   * Flying
   * Whenever ~ deals combat damage to a player, you may pay |2|U. If you do, exile the top card of your library face down. You may look at that card for as
   * long as it remains exiled. You may play that card without paying its mana cost for as long as Intet remains on the battlefield. */

	check_legend_rule(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, RESOLVE_TRIGGER_AI(player)|DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		while( amount ){
				if( has_mana_multi(player, MANACOST_XU(2, 1)) &&
					charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, MANACOST_XU(2,1)) )
				{
					int iid = deck_ptr[player][0];
					if (iid != -1)
					{
						int csvid = cards_data[iid].id;
						obliterate_top_card_of_deck(player);
						play_sound_effect(WAV_DESTROY);
						if (in_play(player, card))	// just in case it gets sacrificed to something like Ashnod's Altar to pay for the mana cost
							create_may_play_card_from_exile_effect(player, card, player, csvid, MPCFE_FACE_DOWN | MPCFE_FOR_FREE | MPCFE_ATTACH);
					}
				}
				else{
					break;
				}
				amount--;
		}
	}

	return 0;
}


int card_jedit_ojanen_of_efrava(int player, int card, event_t event)
{
/* CARD_ID_JEDIT_OJANEN_OF_EFRAVA	8795
Jedit Ojanen of Efrava	|3|G|G|G
Legendary Creature - Cat Warrior 5/5
|H2Forestwalk (This creature can't be blocked as long as defending player controls |Ha Forest.)
Whenever ~ attacks or blocks, create a 2/2 |Sgreen Cat Warrior creature token with |H2forestwalk. */

  check_legend_rule(player, card, event);

  // Whenever ~ attacks or blocks, put a 2/2 |Sgreen Cat Warrior creature token with |H2forestwalk onto the battlefield.
  if (declare_attackers_trigger(player, card, event, 0, player, card)
	  || (blocking(player, card, event) && !is_humiliated(player, card)))
	{
	  token_generation_t token;
	  default_token_definition(player, card, CARD_ID_CAT_WARRIOR, &token);
	  token.key_plus = get_hacked_walk(player, card, KEYWORD_FORESTWALK);
	  generate_token(&token);
	}

  return 0;
}

int card_jodahs_avenger(int player, int card, event_t event){
/*
Jodah's Avenger |5|U
Creature — Shapeshifter 4/4
{0}: Until end of turn, Jodah's Avenger gets -1/-1 and gains your choice of double strike, protection from red, vigilance, or shadow.
(A creature with shadow can block or be blocked by only creatures with shadow.)
*/
	if( event == EVENT_RESOLVE_SPELL || event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[2].player = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_DOUBLE_STRIKE = 1,
		CHOICE_PROT_RED,
		CHOICE_VIGILANCE,
		CHOICE_SHADOW
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		ai_modifier += ((get_power(player, card) >= 2 && get_toughness(player, card) >= 2) ? 24 : -96);
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int current_ab = get_abilities(player, card, EVENT_ABILITIES, -1) &
							(get_sleighted_protection(player, card, KEYWORD_PROT_RED) | KEYWORD_DOUBLE_STRIKE);
			current_ab |= instance->targets[2].player;
			int priorities[5] = {0, 1, 5, 1, 1};
			if( current_phase == PHASE_AFTER_BLOCKING && is_attacking(player, card) && is_unblocked(player, card) ){
				if( !( current_ab & KEYWORD_DOUBLE_STRIKE ) ){
					priorities[CHOICE_DOUBLE_STRIKE] = 10;
				}
			}
			if( current_phase == PHASE_MAIN1 ){
				if( !( current_ab & SP_KEYWORD_SHADOW ) ){
					priorities[CHOICE_SHADOW] = 10;
				}
				if( !( current_ab & SP_KEYWORD_VIGILANCE ) ){
					priorities[CHOICE_VIGILANCE] = 10;
				}
			}
			char buf[128];
			scnprintf(buf, 128, "%s", get_sleighted_color_text(player, card, "Gains Prot. from %s", COLOR_RED));
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Gains Double Strike", 	1, priorities[1],
								buf, 					1, priorities[2],
								"Gains Vigilance", 		1, priorities[3],
								"Gains Shadow", 		1, priorities[4]);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			int abils[5] = {0, KEYWORD_DOUBLE_STRIKE, get_sleighted_protection(player, card, KEYWORD_PROT_RED), SP_KEYWORD_VIGILANCE,
							SP_KEYWORD_VIGILANCE};
			instance->targets[2].player |= abils[choice];
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int key = 0;
		int s_key = 0;
		if( instance->info_slot == CHOICE_DOUBLE_STRIKE ){
			key = KEYWORD_DOUBLE_STRIKE;
		}
		if( instance->info_slot == CHOICE_PROT_RED ){
			key = get_sleighted_protection(player, card, KEYWORD_PROT_RED);
		}
		if( instance->info_slot == CHOICE_VIGILANCE ){
			s_key = SP_KEYWORD_VIGILANCE;
		}
		if( instance->info_slot == CHOICE_SHADOW ){
			s_key = SP_KEYWORD_VIGILANCE;
		}
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								-1, -1, key, s_key);
	}

	return 0;
}

int card_kavu_predator(int player, int card, event_t event){
/* CARD_ID_KAVU_PREDATOR	8797
Kavu Predator	|1|G
Creature - Kavu 2/2
Trample
Whenever an opponent gains life, put that many +1/+1 counters on ~. */

	// the real code is in "gain_life" functions in the "functions.c"
	return 0;
}

int card_keldon_marauders(int player, int card, event_t event){
	/* Keldon Marauders	|1|R	0x200548a
	 * Creature - Human Warrior 3/3
	 * Vanishing 2
	 * When ~ enters the battlefield or leaves the battlefield, it deals 1 damage to target player or planeswalker. */
	vanishing(player, card, event, 2);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || leaves_play(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER", 0, 0) ){
			damage_target0(player, card, 1);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

static const char* must_damage_one_of_my_creatures(int who_chooses, int player, int card)
{
	card_instance_t *instance = get_card_instance(player, card);
	if( instance->internal_card_id == damage_card && instance->damage_target_card != -1 && instance->damage_target_player == who_chooses ){
		return NULL;
	}
	return "must be damage directed to one of your creatures";
}

int card_kor_dirge(int player, int card, event_t event){
 /* CARD_ID_KOR_DIRGE	8799
Kor Dirge	|2|B
Instant
All damage that would be dealt this turn to target creature you control by a source of your choice is dealt to another target creature instead.*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ANY);
	td.extra = (int32_t)must_damage_one_of_my_creatures;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td1) ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, "TARGET_DAMAGE", 1, NULL);
		if( spell_fizzled != 1 ){
			new_pick_target(&td1, "TARGET_CREATURE", 1, 1);
		}
	}
	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td1, 1) ){
			card_instance_t *this = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			this->damage_target_player = instance->targets[1].player;
			this->damage_target_card = instance->targets[1].card;
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_magus_of_the_coffers(int player, int card, event_t event){
/* CARD_ID_MAGUS_OF_THE_COFFERS	8802
Magus of the Coffers	|4|B
Creature - Human Wizard 4/4
|2, |T: Add |B to your mana pool for each |H2Swamp you control. */

	if( event == EVENT_ACTIVATE ){
		int amount = count_subtype(player, TYPE_LAND, SUBTYPE_SWAMP);
		if( amount >= 3 ){
			ai_modifier += (amount * 12);
		}
	}

	return card_cabal_coffers(player, card, event);
}

int card_magus_of_the_arena(int player, int card, event_t event){
/* CARD_ID_MAGUS_OF_THE_ARENA	1266
Magus of the Arena	|4|R|R
Creature - Human Wizard 5/5
|3, |T: Tap target creature you control and target creature of an opponent's choice he or she controls. Those creatures fight each other.
(Each deals damage equal to its power to the other.) */

	if( ! IS_GAA_EVENT(event) ){
		return card_arena(player, card, event);
	}
	return 0;
}

int card_malach_of_the_dawn(int player, int card, event_t event)
{
  // 0x1202040

  /* Malach of the Dawn	|2|W|W
   * Creature - Angel 2/4
   * Flying
   * |W|W|W: Regenerate ~. */
  return regeneration(player, card, event, MANACOST_W(3));
}

int card_magus_of_the_tabernacle(int player, int card, event_t event){
/* CARD_ID_MAGUS_OF_THE_TABERNACLE	1287
Magus of the Tabernacle	|3|W
Creature - Human Wizard 2/6
All creatures have "At the beginning of your upkeep, sacrifice this creature unless you pay |1." */

	pendrell_effect(player, card, event, KILL_SACRIFICE);
	return 0;
}

int card_melancholy(int player, int card, event_t event)
{
/* CARD_ID_MELANCHOLY	8805
Melancholy	|2|B
Enchantment - Aura
Enchant creature
When ~ enters the battlefield, tap enchanted creature.
Enchanted creature doesn't untap during its controller's untap step.
At the beginning of your upkeep, sacrifice ~ unless you pay |B. */

  return thirst_impl(player, card, event, MANACOST_B(1));
}

int card_mirri_the_cursed(int player, int card, event_t event){
/* CARD_ID_MIRRI_THE_CURSED	8809
Mirri the Cursed	|2|B|B
Legendary Creature - Vampire Cat 3/2
Flying, first strike, haste
Whenever ~ deals combat damage to a creature, put a +1/+1 counter on ~. */

	check_legend_rule(player, card, event);

	haste(player, card, event);

	return card_sengir_vampire(player, card, event);
}

int card_molten_firebird(int player, int card, event_t event)
{
/* CARD_ID_MOLTEN_FIREBIRD	8810
Molten Firebird	|4|R
Creature - Phoenix 2/2
Flying
When ~ dies, return it to the battlefield under its owner's control at the beginning of the next end step and you skip your next draw step.
|4|R: Exile ~. */

  return ivory_gargoyle_impl(player, card, event, MANACOST_XR(4,1));
}

int necrotic_sliver_shared_ability(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[1].player > -1 ){
		int p = instance->targets[1].player;
		int c = instance->targets[1].card;

		if( leaves_play(p, c, event) ){
			add_status(player, card, STATUS_INVISIBLE_FX);
		}

		if( ! in_play(p, c) ){
			if( event == EVENT_CAN_ACTIVATE ){
				return 0;
			}

			if( event == EVENT_CLEANUP ){
				kill_card(player, card, KILL_EXILE);
			}
		}

		if( is_humiliated(p, c) ){
			return 0;
		}

		if( ! IS_GAA_EVENT(event) ){
			return 0;
		}

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_PERMANENT);
		td2.allow_cancel = 0;

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( would_validate_arbitrary_target(&td2, instance->targets[2].player, instance->targets[2].card) ){
				kill_card(instance->targets[2].player, instance->targets[2].card, KILL_DESTROY);
			}
		}

		return shared_sliver_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_X(3), 0, &td2, "TARGET_PERMANENT");
	}

	return 0;
}

int card_necrotic_sliver(int player, int card, event_t event){
	/*
	  Necrotic Sliver |1|W|B
	  Creature - Sliver 2/2
	  All Slivers have "{3}, Sacrifice this permanent: Destroy target permanent."
	*/

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &necrotic_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &necrotic_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

// null profusion --> recycle

int card_numot_the_devastator(int player, int card, event_t event){
/* CARD_ID_NUMOT_THE_DEVASTATOR	8816
Numot, the Devastator	|3|R|W|U
Legendary Creature - Dragon 6/6
Flying
Whenever ~ deals combat damage to a player, you may pay |2|R. If you do, destroy up to two target lands. */

	check_legend_rule(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.allow_cancel = 2; //"Done" button

		card_instance_t *instance = get_card_instance(player, card);

		while( amount ){
				if( has_mana_multi(player, MANACOST_XR(2, 1)) && can_target(&td) &&
					charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, MANACOST_XR(2,1)) )
				{
					int i = 0;
					while( i < 2 ){
							if( new_pick_target(&td, "TARGET_LAND", i, 0) ){
								state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
								i++;
							}
							else{
								break;
							}
					}
					for(i=0; i<instance->number_of_targets; i++){
						state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
						kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
					}
					instance->number_of_targets = 0;
				}
				amount--;
		}
	}

	return 0;
}

int card_oros_the_avenger(int player, int card, event_t event){
/* CARD_ID_OROS_THE_AVENGER	8817
Oros, the Avenger	|3|W|B|R
Legendary Creature - Dragon 6/6
Flying
Whenever ~ deals combat damage to a player, you may pay |2|W. If you do, ~ deals 3 damage to each non|Swhite creature. */

	check_legend_rule(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		while( amount ){
				if( has_mana_multi(player, MANACOST_XW(2, 1)) &&
					charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, MANACOST_XW(2,1)) )
				{
					test_definition_t this_test;
					default_test_definition(&this_test, TYPE_CREATURE);
					this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
					this_test.color_flag = 1;
					new_damage_all(player, card, ANYBODY, 3, 0, &this_test);
				}
				amount--;
		}
	}

	return 0;
}

int card_ovinize(int player, int card, event_t event){
	/*
	  Ovinize |1|U
	  Instant
	  Target creature loses all abilities and becomes 0/1 until end of turn.
	*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t hc;
			default_test_definition(&hc, 0);
			hc.power = 0;
			hc.toughness = 1;
			humiliate_and_set_pt_abilities(player, card, instance->targets[0].player, instance->targets[0].card, 4, &hc);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_pallid_mycoderm(int player, int card, event_t event){
	/* Pallid Mycoderm	|3|W
	 * Creature - Fungus 2/4
	 * At the beginning of your upkeep, put a spore counter on ~.
	 * Remove three spore counters from ~: Put a 1/1 |Sgreen Saproling creature token onto the battlefield.
	 * Sacrifice a Saproling: Fungus and/or Saproling creatures you control get +1/+1 until end of turn. */

	add_spore_counters(player, card, event);

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
		int spore_counter_to_remove = 3;
		if( generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, spore_counter_to_remove), NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, 0);
	}

	if( event == EVENT_ACTIVATE ){
		int spore_counter_to_remove = 3;
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0,
													GVC_COUNTERS(COUNTER_SPORE, spore_counter_to_remove), NULL, NULL),
						generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, 0, NULL, NULL,
																		0, &test, 0)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Generate a Saproling", abils[1], 5,
							"Pump Fungi & Saprolings", abils[2], current_phase == PHASE_AFTER_BLOCKING ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_SAPROLING ){
			return generic_activated_ability(player, card, event, 0, MANACOST0,
											GVC_COUNTERS(COUNTER_SPORE, spore_counter_to_remove), NULL, NULL);
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
			int count = active_cards_count[player]-1;
			while( count > -1 ){
					if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) &&
						(has_subtype(player, count, SUBTYPE_FUNGUS) || has_subtype(player, count, SUBTYPE_SAPROLING)) )
					{
						pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card, player, count, 1, 1);
					}
					count--;
			}
		}
	}

	return 0;
}

int card_piracy_charm(int player, int card, event_t event)
{
  /* Piracy Charm	|U
   * Instant
   * Choose one - Target creature gains |H2islandwalk until end of turn; or target creature gets +2/-1 until end of turn; or target player discards a card. */

  if (IS_CASTING(player, card, event))
	{
	  target_definition_t td_creature, td_walk_creature, td_weak_creature, td_pump_creature, td_player;
	  default_target_definition(player, card, &td_creature, TYPE_CREATURE);
	  td_creature.preferred_controller = ANYBODY;

	  default_target_definition(player, card, &td_player, 0);
	  td_player.zone = TARGET_ZONE_PLAYERS;

	  int can_walk = can_target(&td_creature);
	  int can_pump = can_walk;
	  int can_discard = can_target(&td_player);

	  int ai_priority_walk = 0, ai_priority_pump = 0, ai_priority_discard = 0;

	  if (IS_AI(player))
		{
		  default_target_definition(player, card, &td_walk_creature, TYPE_CREATURE);
		  td_walk_creature.preferred_controller = player;

		  default_target_definition(player, card, &td_weak_creature, TYPE_CREATURE);
		  td_weak_creature.toughness_requirement = 1 | TARGET_PT_LESSER_OR_EQUAL | TARGET_PT_INCLUDE_DAMAGE;

		  default_target_definition(player, card, &td_pump_creature, TYPE_CREATURE);
		  td_pump_creature.preferred_controller = player;
		  td_pump_creature.toughness_requirement = 2 | TARGET_PT_GREATER_OR_EQUAL | TARGET_PT_INCLUDE_DAMAGE;

		  ai_priority_walk = can_walk && can_target(&td_walk_creature) ? 5 : 2;
		  ai_priority_pump = can_pump && (can_target(&td_weak_creature) || can_target(&td_pump_creature)) ? 7 : 1;
		  ai_priority_discard = hand_count[1-player] > 0 ? 3 : 0;
		}

	  enum
	  {
		CHOICE_ISLANDWALK = 1,
		CHOICE_PUMP,
		CHOICE_DISCARD
	  } choice = DIALOG(player, card, event,
						get_hacked_land_text(player, card, "Gain %lwalk", SUBTYPE_ISLAND), can_walk, ai_priority_walk,
						"Get +2/-1", can_pump, ai_priority_pump,
						"Discard", can_discard, ai_priority_discard);

	  if (event == EVENT_CAN_CAST)
		return choice;
	  else if (event == EVENT_CAST_SPELL && affect_me(player, card))
		switch (choice)
		  {
			case CHOICE_ISLANDWALK:
			  pick_target(IS_AI(player) ? &td_walk_creature : &td_creature, "TARGET_CREATURE");
			  break;

			case CHOICE_PUMP:
			  if (!IS_AI(player))
				pick_target(&td_creature, "TARGET_CREATURE");
			  else
				{
				  if (can_target(&td_weak_creature))
					pick_target(&td_weak_creature, "TARGET_CREATURE");
				  else if (can_target(&td_pump_creature))
					pick_target(&td_pump_creature, "TARGET_CREATURE");
				  else
					pick_target(&td_creature, "TARGET_CREATURE");
				}
			  break;

			case CHOICE_DISCARD:
			  pick_target(&td_player, "TARGET_PLAYER");
			  break;
		  }
	  else	// EVENT_RESOLVE_SPELL
		{
		  card_instance_t* instance = get_card_instance(player, card);

		  switch (choice)
			{
			  case CHOICE_ISLANDWALK:
				if (valid_target(&td_creature))
				  alternate_legacy_text(1, player,
										pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
															   0, 0, KEYWORD_ISLANDWALK, 0));
				break;

			  case CHOICE_PUMP:
				if (valid_target(&td_creature))
				  alternate_legacy_text(2, player,
										pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, -1, 0, 0));
				break;

			  case CHOICE_DISCARD:
				if (valid_target(&td_player))
				  discard(instance->targets[0].player, 0, player);
				break;
			}

		  kill_card(player, card, KILL_DESTROY);
		}
	}

  return 0;
}

int card_pongify(int player, int card, event_t event){
/* CARD_ID_PONGIFY	8822
Pongify	|U
Instant
Destroy target creature. It can't be regenerated. Its controller creates a 3/3 |Sgreen Ape creature token. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_APE, &token);
			token.t_player = instance->targets[0].player;
			token.pow = 3;
			token.tou = 3;
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_psychotrope_thallid(int player, int card, event_t event){
	/* Psychotrope Thallid	|2|G
	 * Creature - Fungus 1/1
	 * At the beginning of your upkeep, put a spore counter on ~.
	 * Remove three spore counters from ~: Put a 1/1 |Sgreen Saproling creature token onto the battlefield.
	 * |1, Sacrifice a Saproling: Draw a card. */


	add_spore_counters(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Saproling to sacrifice.");
	test.subtype = SUBTYPE_SAPROLING;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_SAPROLING = 1,
		CHOICE_DRAW
	};

	if( event == EVENT_CAN_ACTIVATE ){
		int spore_counter_to_remove = 3;
		if( generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, spore_counter_to_remove), NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL, 0, &test, 0);
	}

	if( event == EVENT_ACTIVATE ){
		int spore_counter_to_remove = 3;
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0,
													GVC_COUNTERS(COUNTER_SPORE, spore_counter_to_remove), NULL, NULL),
						generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(1), 0, NULL, NULL,
																		0, &test, 0)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Generate a Saproling.", abils[1], 5,
							"Draw a card.", abils[2], current_phase == PHASE_AFTER_BLOCKING ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_SAPROLING ){
			return generic_activated_ability(player, card, event, 0, MANACOST0,
											GVC_COUNTERS(COUNTER_SPORE, spore_counter_to_remove), NULL, NULL);
		}
		if( choice == CHOICE_DRAW ){
			return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL, 0, &test, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAPROLING ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING);
		}
		if( instance->info_slot == CHOICE_DRAW ){
			draw_a_card(player);
		}
	}

	return 0;
}

int card_pyrohemia(int player, int card, event_t event)
{
/* CARD_ID_PYROHEMIA	8827
Pyrohemia	|2|R|R
Enchantment
At the beginning of the end step, if no creatures are on the battlefield, sacrifice ~.
|R: ~ deals 1 damage to each creature and each player. */

  return pestilence_impl(player, card, event, 1, COLOR_RED);
}

int card_radha_heir_to_keld(int player, int card, event_t event)
{
/* CARD_ID_RADHA_HEIR_TO_KELD	8828
Radha, Heir to Keld	|R|G
Legendary Creature - Elf Warrior 2/2
Whenever ~ attacks, you may add |R|R to your mana pool.
|T: Add |G to your mana pool. */

  check_legend_rule(player, card, event);

  // Whenever ~ attacks, you may add |R|R to your mana pool.
  if (declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), player, card))
	produce_mana(player, COLOR_RED, 2);

  // |T: Add |G to your mana pool.
  return mana_producing_creature(player, card, event, 0, COLOR_GREEN, 1);
}

int card_rathi_trapper(int player, int card, event_t event){
/* CARD_ID_RATHI_TRAPPER	8829
Rathi Trapper	|1|B
Creature - Human Rebel Rogue 1/2
|B, |T: Tap target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(1), 0, &td, "TARGET_CREATURE");
}

int card_reality_acid(int player, int card, event_t event)
{
/* CARD_ID_REALITY_ACID	8830
Reality Acid	|2|U
Enchantment - Aura
Enchant permanent
Vanishing 3 (This Aura enters the battlefield with three time counters on it. At the beginning of your upkeep, remove a time counter from it.
When the last is removed, sacrifice it.)
When ~ leaves the battlefield, enchanted permanent's controller sacrifices it. */

	card_instance_t* instance = get_card_instance(player, card);

  // AI
	if (event == EVENT_SHOULD_AI_PLAY && instance->damage_target_player >= 0 && instance->damage_target_card >= 0)
	{
		int mod = my_base_value(instance->damage_target_player, instance->damage_target_card);
		if (instance->damage_target_player == player)
			ai_modifier -= mod;
		else
			ai_modifier += mod;
	}

	// Vanishing 3
	vanishing(player, card, event, 3);

	// When ~ leaves the battlefield, enchanted permanent's controller sacrifices it.
	if (leaves_play(player, card, event) && instance->damage_target_player >= 0 && instance->damage_target_card >= 0
		&& in_play(instance->damage_target_player, instance->damage_target_card))
	  kill_card(instance->damage_target_player, instance->damage_target_card, KILL_SACRIFICE);

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	// Enchant permanent
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.preferred_controller = 1-player;

	return targeted_aura(player, card, event, &td, "TARGET_PERMANENT");
}

int card_rebuff_the_wicked(int player, int card, event_t event){
/* CARD_ID_REBUFF_THE_WICKED	8831
Rebuff the Wicked	|W
Instant
Counter target spell that targets a permanent you control. */

	if( event == EVENT_CAN_CAST ){
		int result = card_counterspell(player, card, event);
		if( result > 0 ){
			card_instance_t *instance = get_card_instance(card_on_stack_controller, card_on_stack);
			int i;
			for(i=0; i<instance->number_of_targets; i++){
				if( instance->targets[i].player == player && instance->targets[i].card != -1 ){
					if( is_what(instance->targets[i].player, instance->targets[i].card, TYPE_PERMANENT) ){
						return result;
					}
				}
			}
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			int valid = 0;
			card_instance_t *spell = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			int i;
			for(i=0; i<spell->number_of_targets; i++){
				if( spell->targets[i].player == player && spell->targets[i].card != -1 ){
					if( is_what(spell->targets[i].player, spell->targets[i].card, TYPE_PERMANENT) ){
						valid = 1;
						break;
					}
				}
			}
			if( valid ){
				real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			}
		}
		return 0;
	}

	return card_counterspell(player, card, event);
}

int card_reckless_wurm(int player, int card, event_t event){
/* CARD_ID_RECKLESS_WURM	8832
Reckless Wurm	|3|R|R
Creature - Wurm 4/4
Trample
Madness |2|R (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

	return madness(player, card, event, MANACOST_XR(2, 1));
}

int card_retether(int player, int card, event_t event){
/* CARD_ID_RETETHER	8834
Retether	|3|W
Sorcery
Return each Aura card from your graveyard to the battlefield. Only creatures can be enchanted this way.
(Aura cards that can't enchant a creature on the battlefield remain in your graveyard.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ENCHANTMENT);
		test.subtype = SUBTYPE_AURA;
		new_reanimate_all(player, card, player, &test, REANIMATE_DEFAULT);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_revered_dead(int player, int card, event_t event)
{
/* CARD_ID_REVERED_DEAD	8835
Revered Dead	|1|W
Creature - Spirit Soldier 1/1
|W: Regenerate ~. */

  return regeneration(player, card, event, MANACOST_W(1));
}

int card_riftmarked_knight(int player, int card, event_t event){
	/* Riftmarked Knight	|1|W|W
	 * Creature - Human Rebel Knight 2/2
	 * Protection from |Sblack; flanking
	 * Suspend 3-|1|W|W
	 * When the last time counter is removed from ~ while it's exiled, put a 2/2 |Sblack Knight creature token with flanking, protection from |Swhite, and haste
	 * onto the battlefield. */
	flanking(player, card, event);
	return suspend(player, card, event, 3, MANACOST_XW(1, 2), NULL, NULL);
}

int card_riptide_pilferer(int player, int card, event_t event)
{
  /* Riptide Pilferer	|1|U
   * Creature - Merfolk Rogue 1/1
   * Whenever ~ deals combat damage to a player, that player discards a card.
   * Morph |U */

  whenever_i_deal_damage_to_a_player_he_discards_a_card(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE, 0);

  return morph(player, card, event, MANACOST_U(1));
}

int card_roiling_horror(int player, int card, event_t event)
{
  /* Roiling Horror	|3|B|B
   * Creature - Horror 100/100
   * ~'s power and toughness are each equal to your life total minus the life total of an opponent with the most life.
   * Suspend X-|X|B|B|B. X can't be 0.
   * Whenever a time counter is removed from ~ while it's exiled, target player loses 1 life and you gain 1 life. */
	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && player != -1 && affect_me(player, card) && ! is_humiliated(player, card) )
		event_result += life[player] - life[1-player];

	return suspend(player, card, event, -1, MANACOST_XB(-1, 3), NULL, NULL);
}

int card_saltblast(int player, int card, event_t event){
/* CARD_ID_SALTBLAST	8841
Saltblast	|3|W|W
Sorcery
Destroy target non|Swhite permanent. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td,
						get_sleighted_color_text(player, card, "Select target non%s permanent.", COLOR_WHITE), 1, NULL);
}

int card_saltfield_recluse(int player, int card, event_t event){
/* CARD_ID_SALTFIELD_RECLUSE	8842
Saltfield Recluse	|2|W
Creature - Human Rebel Cleric 1/2
|T: Target creature gets -2/-0 until end of turn. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	return vanilla_creature_pumper(player, card, event, MANACOST0, GAA_UNTAPPED, -2, 0, 0, 0, &td);
}

int card_seal_of_primordium(int player, int card, event_t event){
/* CARD_ID_SEAL_OF_PRIMORDIUM	8843
Seal of Primordium	|1|G
Enchantment
Sacrifice ~: Destroy target artifact or enchantment. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}

	return seal(player, card, event, &td, "DISENCHANT");
}

// serendib sorcerer --> Enchantress Queen

int card_serras_boon(int player, int card, event_t event)
{
  /* Serra's Boon	|2|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+2 as long as it's |Swhite. Otherwise, it gets -2/-1. */

  if (event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_SHOULD_AI_PLAY)
	{
	  card_instance_t* instance;
	  if (!(instance = in_play(player, card))
		  || instance->damage_target_player < 0
		  || (event != EVENT_SHOULD_AI_PLAY && !affect_me(instance->damage_target_player, instance->damage_target_card)))
		return 0;

	  int white = get_color(instance->damage_target_player, instance->damage_target_card) & get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
	  switch (event)
		{
		  case EVENT_SHOULD_AI_PLAY:
			ai_modifier += (!white == !(instance->damage_target_player == AI)) ? 24 : -24;
			break;

		  case EVENT_POWER:
			event_result += white ? 1 : -2;
			break;

		  case EVENT_TOUGHNESS:
			event_result += white ? 2 : -1;
			break;

		  default:
			break;
		}
	}

  return vanilla_aura(player, card, event, ANYBODY);
}

int card_shade_of_trokair(int player, int card, event_t event){
	/* Shade of Trokair	|3|W
	 * Creature - Shade 1/2
	 * |W: ~ gets +1/+1 until end of turn.
	 * Suspend 3-|W */
	if( IS_GAA_EVENT(event) || event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		return generic_shade_merge_pt(player, card, event, 0, MANACOST_W(1), 1, 1);
	}
	return suspend(player, card, event, 3, MANACOST_W(1), NULL, NULL);
}

int card_shaper_parasite(int player, int card, event_t event){
/* CARD_ID_SHAPER_PARASITE	8847
Shaper Parasite	|1|U|U
Creature - Illusion 2/3
Morph |2|U (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, target creature gets +2/-2 or -2/+2 until end of turn. */

	if( event == EVENT_TURNED_FACE_UP ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) ){
			if( pick_target(&td, "TARGET_CREATURE") ){
				instance->number_of_targets = 1;
				int p_pump = 2;
				int t_pump = -2;
				int choice = do_dialog(player, player, card, -1, -1," Give +2/-2\n Give -2/+2", 0);
				if( choice == 1 ){
					p_pump = -2;
					t_pump = 2;
				}
				pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, p_pump, t_pump);
			}
		}
		instance->number_of_targets = 0;
	}
	return morph(player, card, event, MANACOST_XU(2, 1));
}

int card_shivan_meteor(int player, int card, event_t event)
{
  /* Shivan Meteor	|3|R|R
   * Sorcery
   * ~ deals 13 damage to target creature.
   * Suspend 2-|1|R|R */

	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		return suspend(player, card, event, 2, MANACOST_XR(1,2), &td, "TARGET_CREATURE");
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if (event == EVENT_RESOLVE_SPELL)
	{
		card_instance_t* instance = get_card_instance(player, card);
		if (valid_target(&td))
			damage_creature(instance->targets[0].player, instance->targets[0].card, 13, player, card);

		kill_card(player, card, KILL_DESTROY);
	}

	return suspend(player, card, event, 2, MANACOST_XR(1,2), &td, "TARGET_CREATURE");
}

// shivan wumpus --> argothian wurm

int shrouded_lore_impl(int player, int card, event_t event, color_t color){

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if( count_graveyard(player) > 0 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ANY, "Select a card to return to opponent's hand.");
				test.id = 904; // Face down card
				test.id_flag = DOESNT_MATCH;

				const int *grave = get_grave(player);
				int g_copy[count_deck(player)];
				int gcc = 0;
				while( grave[gcc] != -1 ){
						g_copy[gcc] = grave[gcc];
						gcc++;
				}
				int selected = -1;
				int available = gcc;
				int ai_best_choice = -1;
				if( player == AI ){
					ai_best_choice = new_select_a_card(player, player, TUTOR_FROM_GRAVE, 0, AI_MAX_VALUE, -1, &test);
				}
				while( selected == -1 && available > 1 ){
						selected = select_card_from_zone(instance->targets[0].player, instance->targets[0].player, g_copy, gcc, 1, AI_MIN_VALUE, -1, &test);

						int id = cards_data[g_copy[selected]].id;
						card_ptr_t* c = cards_ptr[ id ];
						int choice = 0;
						if( has_mana(player, color, 1) ){
							int ai_choice = 0;
							if( selected != ai_best_choice ){
								ai_choice = 1;
							}
							char buffer[100];
							scnprintf(buffer, 100, " Get %s\n Another choice", c->name );
							choice = do_dialog(player, player, card, -1, -1, buffer, ai_choice);
						}
						if( choice == 1){
							charge_mana(player, color, 1);
							if( spell_fizzled != 1 ){
								g_copy[selected] = get_internal_card_id_from_csv_id(904);
								selected = -1;
								available--;
							}
						}
				}
				if( available == 1 && selected == -1 ){
					int i;
					for(i=0; i<gcc; i++){
						if( cards_data[g_copy[i]].id != 904 ){
							selected = i;
							break;
						}
					}
				}
				from_grave_to_hand(player, selected, TUTOR_HAND);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, "TARGET_PLAYER", 1, NULL);
}

int card_shrouded_lore(int player, int card, event_t event){
/* CARD_ID_SHROUDED_LORE	8850
Shrouded Lore	|B
Sorcery
Target opponent chooses a card in your graveyard. You may pay |B. If you do, repeat this process except that opponent can't choose a card already chosen for ~.
Then put the last chosen card into your hand. */

  return shrouded_lore_impl(player, card, event, COLOR_BLACK);
}

int card_simian_spirit_guide(int player, int card, event_t event){
	/* Simian Spirit Guide	|2|R
	 * Creature - Ape Spirit 2/2
	 * Exile ~ from your hand: Add |R to your mana pool. */

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND ){
		return 1;
	}
	if( event == EVENT_ACTIVATE_FROM_HAND ){
		kill_card(player, card, KILL_EXILE);
		return 1;
	}
	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND ){
		produce_mana(player, COLOR_RED, 1);
	}
	return 0;
}

int card_skirk_shaman(int player, int card, event_t event){
/* CARD_ID_SKIRK_SHAMAN	8852
Skirk Shaman	|1|R|R
Creature - Goblin Shaman 2/2
~ can't be blocked except by artifact creatures and/or |Sred creatures. */

	firefright(player, card, event);
	return 0;
}

int card_stingscourger(int player, int card, event_t event){
/* CARD_ID_STINGSCOURGER	8855
Stingscourger	|1|R
Creature - Goblin Warrior 2/2
Echo |3|R (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)
When ~ enters the battlefield, return target creature an opponent controls to its owner's hand. */

	echo(player, card, event, MANACOST_XR(3, 1));

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_stonecloaker(int player, int card, event_t event){
/*
Stonecloaker |2|W
Creature — Gargoyle 3/2
Flash (You may cast this spell any time you could cast an instant.)
Flying
When Stonecloaker enters the battlefield, return a creature you control to its owner's hand.
When Stonecloaker enters the battlefield, exile target card from a graveyard.
*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select a creature you control & return it to its owner's hand.", 0, GS_LITERAL_PROMPT) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			instance->number_of_targets = 0;
		}

		if( (count_graveyard(player) && ! graveyard_has_shroud(player)) ||
			(count_graveyard(1-player) && ! graveyard_has_shroud(1-player)) )
		{
			instance->targets[0].player = 1-player;
			if( count_graveyard(1-player) > 0 && ! graveyard_has_shroud(1-player) ){
				if( count_graveyard(player) > 0 && ! graveyard_has_shroud(player) ){
					target_definition_t td1;
					default_target_definition(player, card, &td1, TYPE_CREATURE);
					td1.zone = TARGET_ZONE_PLAYERS;
					td1.illegal_abilities = 0;
					td1.allow_cancel = 0;
					new_pick_target(&td1, "Select a graveyard owner.", 0, GS_LITERAL_PROMPT);
					instance->number_of_targets = 0;
				}
			}
			else{
				instance->targets[0].player = player;
			}
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");
			new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_GRAVE, TUTOR_RFG, 1, AI_MAX_VALUE, &this_test);
		}
	}
	return flash(player, card, event);
}

int card_sulfur_elemental(int player, int card, event_t event){
/* CARD_ID_SULFUR_ELEMENTAL	8858
Sulfur Elemental	|2|R
Creature - Elemental 3/2
Flash (You may cast this spell any time you could cast an instant.)
Split second (As long as this spell is on the stack, players can't cast spells or activate abilities that aren't mana abilities.)
|SWhite creatures get +1/-1. */

	cannot_be_countered(player, card, event);

	boost_creature_by_color(player, card, event, COLOR_TEST_WHITE, 1, -1, 0, BCT_INCLUDE_SELF);

	return flash(player, card, event);
}

int card_sunlance(int player, int card, event_t event){
/* CARD_ID_SUNLANCE	8859
Sunlance	|W
Sorcery
~ deals 3 damage to target non|Swhite creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET  | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_WHITE), 1, NULL);
}

int card_synchronous_sliver(int player, int card, event_t event){
/* Synchronous Sliver	|4|U
* Creature - Sliver 3/3
* All Sliver creatures have vigilance. */

  boost_subtype(player, card, event, SUBTYPE_SLIVER, 0,0, 0,SP_KEYWORD_VIGILANCE, BCT_INCLUDE_SELF);
  return 0;
}

int card_temporal_extortion(int player, int card, event_t event){
/* CARD_ID_TEMPORAL_EXTORTION	8861
Temporal Extortion	|B|B|B|B
Sorcery
When you cast ~, any player may pay half his or her life, rounded up. If a player does, counter ~.
Take an extra turn after this one. */

	if( event == EVENT_RESOLVE_SPELL ){
		int choice = 1;
		if( ! check_special_flags(player, card, SF_NOT_CAST) ){
			APNAP(p, {
					if( can_pay_life(p, 1) ){
						int ai_choice = 0;
						if( choice == 0 || life[p] < 12 || check_battlefield_for_id(p, CARD_ID_STRANGLEHOLD) ){
							ai_choice = 1;
						}
						int p_choice = do_dialog(p, player, card, -1, -1," Counter Temporal Extortion\n Pass", ai_choice);
						if( p_choice == 0 ){
							int amount = round_up_value(life[p]);
							lose_life(p, amount);
							if( choice == 1 ){
								choice = 0;
							}
						}
					}
				}
			);
		}
		if( choice == 0 ){
			real_counter_a_spell(player, card, player, card);
		}
		else{
			return card_time_walk(player, card, event);
		}
	}

	return basic_spell(player, card, event);
}

int card_teneb_the_harvester(int player, int card, event_t event){
/* CARD_ID_TENEB_THE_HARVESTER	8862
Teneb, the Harvester	|3|B|G|W
Legendary Creature - Dragon 6/6
Flying
Whenever ~ deals combat damage to a player, you may pay |2|B. If you do, put target creature card from a graveyard onto the battlefield under your control. */

	check_legend_rule(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		while( amount ){
				if( has_mana_multi(player, MANACOST_XU(2, 1)) &&
					charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, MANACOST_XU(2,1)) )
				{
					test_definition_t test;
					new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");
					reanimate_test_from_any_graveyard(player, card, &test, 1, REANIMATE_DEFAULT);
				}
				else{
					break;
				}
				amount--;
		}
	}

	return 0;
}

int card_timbermare(int player, int card, event_t event){
/* CARD_ID_TIMBERMARE	8864
Timbermare	|3|G
Creature - Elemental Horse 5/5
Haste
Echo |5|G (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)
When ~ enters the battlefield, tap all other creatures. */

	echo(player, card, event, MANACOST_XR(5, 1));

	return card_thundermare(player, card, event);
}

int card_torchling(int player, int card, event_t event){
/*
Torchling |3|R|R
Creature — Shapeshifter 3/3
{R}: Untap Torchling.
{R}: Target creature blocks Torchling this turn if able.
{R}: Change the target of target spell that targets only Torchling.
{1}: Torchling gets +1/-1 until end of turn.
{1}: Torchling gets -1/+1 until end of turn.
*/
	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( current_turn == player ){
			return generic_shade(player, card, event, 0, MANACOST_X(1), 1, -1, 0, 0);
		}
		return generic_shade(player, card, event, 0, MANACOST_X(1), -1, 1, 0, 0);
	}


	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_UNTAP = 1,
		CHOICE_FORCE_BLOCK,
		CHOICE_CHANGE_TARGET, //Currently unimplemented
		CHOICE_P1_M1,
		CHOICE_M1_P1
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int abils[6] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_R(1), 0, NULL, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_R(1), 0, &td, NULL),
						0,
						1,
						1
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Untap", abils[1], current_turn == 1-player && is_tapped(player, card) ? 10 : 1,
							"Force creature to block Torchling", abils[2], 1,
							"Change target of spell", abils[3], 1,
							"+1/-1", abils[4], current_turn == player ? 10 : 1,
							"-1/+1", abils[5], current_turn != player ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_UNTAP ){
			return generic_activated_ability(player, card, event, 0, MANACOST_R(1), 0, NULL, NULL);
		}
		if( choice == CHOICE_FORCE_BLOCK ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_CREATURE");
		}
		return generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_UNTAP ){
			untap_card(player, instance->parent_card);
		}
		if( instance->info_slot == CHOICE_FORCE_BLOCK && valid_target(&td) ){
			target_must_block_me(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1);
		}
		if( instance->info_slot == CHOICE_P1_M1 ){
			return generic_shade(player, card, event, 0, MANACOST_X(1), 1, -1, 0, 0);
		}
		if( instance->info_slot == CHOICE_M1_P1 ){
			return generic_shade(player, card, event, 0, MANACOST_X(1), -1, 1, 0, 0);
		}
	}

	return 0;
}

int card_treacherous_urge(int player, int card, event_t event){
/* CARD_ID_TREACHEROUS_URGE	8868
Treacherous Urge	|4|B
Instant
Target opponent reveals his or her hand. You may put a creature card from it onto the battlefield under your control.
That creature gains haste. Sacrifice it at the beginning of the next end step. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			int result = new_select_a_card(player, instance->targets[0].player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test);
			if( result > -1 ){
				card_instance_t *trg = get_card_instance(instance->targets[0].player, result);
				int card_added = add_card_to_hand(player, trg->internal_card_id);
				obliterate_card(instance->targets[0].player, result);
				if( player == HUMAN && instance->targets[0].player == AI ){
					add_state(player, card_added, STATE_OWNED_BY_OPPONENT);
				}
				if( player == AI && instance->targets[0].player == HUMAN ){
					remove_state(player, card_added, STATE_OWNED_BY_OPPONENT);
				}
				put_into_play(player, card_added);
				pump_ability_t pump;
				default_pump_ability_definition(player, card, &pump, 0, 0, 0, SP_KEYWORD_HASTE);
				pump.paue_flags = PAUE_END_AT_EOT | PAUE_REMOVE_TARGET_AT_EOT;
				pump.eot_removal_method = KILL_SACRIFICE;
				pump_ability(player, card, player, card_added, &pump);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_uktabi_drake(int player, int card, event_t event){
/* CARD_ID_UKTABI_DRAKE	8869
Uktabi Drake	|G
Creature - Drake 2/1
Flying, haste
Echo |1|G|G (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.) */

	haste(player, card, event);

	echo(player, card, event, MANACOST_XG(1, 2));

	return 0;
}

int card_urborg_tomb_of_yawgmoth(int player, int card, event_t event){
/* CARD_ID_URBORG_TOMB_OF_YAWGMOTH	8870
Urborg, Tomb of Yawgmoth	""
Legendary Land
Each land is |Ha Swamp in addition to its other land types. */

	// Used externally
	check_legend_rule(player, card, event);

	if (event == EVENT_RESOLVE_SPELL){
		play_land_sound_effect(player, card);
	}

	return all_lands_are_basiclandtype(player, card, event, 2, COLOR_BLACK, SUBTYPE_SWAMP);
}

int card_vitaspore_thallid(int player, int card, event_t event){

	/* Vitaspore Thallid	|1|G
	 * Creature - Fungus 1/1
	 * At the beginning of your upkeep, put a spore counter on ~.
	 * Remove three spore counters from ~: Put a 1/1 |Sgreen Saproling creature token onto the battlefield.
	 * Sacrifice a Saproling: Target creature gains haste until end of turn. */

	add_spore_counters(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Saproling to sacrifice.");
	test.subtype = SUBTYPE_SAPROLING;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_SAPROLING = 1,
		CHOICE_HASTE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		int spore_counter_to_remove = 3;
		if( generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, spore_counter_to_remove), NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL, 0, &test, 0);
	}

	if( event == EVENT_ACTIVATE ){
		int spore_counter_to_remove = 3;
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0,
													GVC_COUNTERS(COUNTER_SPORE, spore_counter_to_remove), NULL, NULL),
						generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST0, 0,
																		&td, NULL, 0, &test, 0)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Generate a Saproling", abils[1], 5,
							"Give Haste", abils[2], current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_SAPROLING ){
			return generic_activated_ability(player, card, event, 0, MANACOST0,
											GVC_COUNTERS(COUNTER_SPORE, spore_counter_to_remove), NULL, NULL);
		}
		if( choice == CHOICE_HASTE ){
			return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0,
																	&td, "TARGET_CREATURE", 0, &test, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAPROLING ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING);
		}
		if( instance->info_slot == CHOICE_HASTE ){
			if( valid_target(&td) ){
				pump_ability_until_eot(instance->parent_controller, instance->parent_card,
										instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_HASTE);
			}
		}
	}

	return 0;
}

int card_voidstone_gargoyle(int player, int card, event_t event){
/* CARD_ID_VOIDSTONE_GARGOYLE	8876
Voidstone Gargoyle	|3|W|W
Creature - Gargoyle 3/3
Flying
As ~ enters the battlefield, choose a nonland card name.
Spells with the chosen name can't be cast.
Activated abilities of sources with the chosen name can't be activated. */

	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_LAND;

		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a nonland card.");
		test.type_flag = DOESNT_MATCH;
		int csvid = select_permanent_to_disable(player, card, &td, "Select a nonland permanent in play", &test);
		get_card_instance(player, card)->info_slot = csvid;
		activated_abilities_disabler_enters_bf(player, card, ACT_DISABLE_ALL_ACTIVATED_ABILITIES);
	}

	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card) ){
		int csvid = get_card_instance(player, card)->info_slot;
		if( csvid > 0 && get_id(affected_card_controller, affected_card) == csvid )
		{
			infinite_casting_cost();
		}
	}

	return activated_abilities_disabler(player, card, event, ACT_DISABLE_ALL_ACTIVATED_ABILITIES);
}

int card_volcano_hellion(int player, int card, event_t event){
/*
Volcano Hellion |2|R|R
Creature — Hellion 6/5
Volcano Hellion has echo {X}, where X is your life total. (At the beginning of your upkeep, if this came under your control since
	the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)
When Volcano Hellion enters the battlefield, it deals an amount of damage of your choice to you and target creature.
	The damage can't be prevented.
*/
	echo(player, card, event, MANACOST_X(life[player]));

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		my_damage_cannot_be_prevented(player, card, event);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		if( can_target(&td) ){
			int dmg = internal_rand(6)+1;
			while( life[player]-dmg < 6 ){
					dmg--;
			}
			if( ! IS_AI(player) ){
				while( 1 ){
						dmg = choose_a_number(player, "How many damage will be done by Volcano Hellion?", dmg);
						if( dmg > -1 ){
							break;
						}
				}
			}
			if( new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
				damage_target0(player, card, dmg);
				damage_player(player, dmg, player, card);
			}
		}
		get_card_instance(player, card)->number_of_targets = 0;
	}

	return 0;
}

int card_vorosh_the_hunter(int player, int card, event_t event){
/* CARD_ID_VOROSH_THE_HUNTER	8878
Vorosh, the Hunter	|3|G|U|B
Legendary Creature - Dragon 6/6
Flying
Whenever ~ deals combat damage to a player, you may pay |2|G. If you do, put six +1/+1 counters on ~. */

	check_legend_rule(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		while( amount ){
				if( has_mana_multi(player, MANACOST_XG(2, 1)) &&
					charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, MANACOST_XG(2, 1)) )
				{
					add_1_1_counters(player, card, 6);
				}
				else{
					break;
				}
				amount--;
		}
	}

	return 0;
}

int card_whitemane_lion(int player, int card, event_t event){

	/* Whitemane Lion	|1|W
	 * Creature - Cat 2/2
	 * Flash
	 * When ~ enters the battlefield, return a creature you control to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		base_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_next_target_noload(&td, "Select a creature you control.")){
			if (instance->targets[0].card == card && instance->targets[0].player == player){
				ai_modifier += player == AI ? -64 : 64;
			}
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return flash(player, card, event);
}

static int has_the_same_pt_sum(int iid, int loword_sum_hiword_player, int unused1, int unused2){
	int player = SHIWORD(loword_sum_hiword_player);
	return get_base_power_iid(player, iid) + get_base_toughness_iid(player, iid) == SLOWORD(loword_sum_hiword_player);
}
int card_wild_pair(int player, int card, event_t event){
/* Wild Pair	|4|G|G
 * Enchantment
 * Whenever a creature enters the battlefield, if you cast it from your hand, you may search your library for a creature card with
 *		the same total power and toughness and put it onto the battlefield. If you do, shuffle your library. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		if( ! not_played_from_hand(trigger_cause_controller, trigger_cause) ){
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);

			etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
		}
	}

	if( event == EVENT_ETB_ABILITY ){
		int sum = get_power(trigger_cause_controller, trigger_cause)+get_toughness(trigger_cause_controller, trigger_cause);
		char msg[100];
		scnprintf(msg, 100, "Select a creature card whose total power and toughness is %d.", sum);
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, msg);
		this_test.special_selection_function = &has_the_same_pt_sum;
		this_test.value_for_special_selection_function = (sum & 0xFFFF) | (player << 16);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
	}

	return global_enchantment(player, card, event);
}

int card_wistful_thinking(int player, int card, event_t event){
/* CARD_ID_WISTFUL_THINKING	8882
Wistful Thinking	|2|U
Sorcery
Target player draws two cards, then discards four cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier_for_targeted_card_drawing(player, card, 0, 2);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(instance->targets[0].player, 2);
			new_multidiscard(instance->targets[0].player, 4, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

