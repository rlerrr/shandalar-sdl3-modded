#include <limits.h>

#include "manalink.h"

#ifndef SHANDALAR

static int oubliette_legacy(int player, int card, event_t event);
int phase_out_target_until_source_leaves_play(int s_player, int s_card, int t_player, int t_card);

#endif

static void mark_creatures(intptr_t itgt, intptr_t iinfo_slot, int t_player, int t_card)
{
  int16_t** tgt = (int16_t**)itgt;
  **tgt = t_card;
  ++*tgt;
}

int card_abu_jafar(int player, int card, event_t event)
{
/* CARD_ID_ABU_JAFAR	391
Abu Ja'far	|W
Creature - Human 0/1
When ~ dies, destroy all creatures blocking or blocked by it. They can't be regenerated.*/

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG))
	instance->destroys_if_blocked |= DIFB_DESTROYS_ALL;

  if (event == EVENT_RESOLVE_THIS_DIES_TRIGGER	// this is the legacy, not the creature
	  || (((player == current_turn && (instance->state & STATE_ATTACKING))
		   || (player == 1-current_turn && (instance->state & STATE_BLOCKING)))
		  && !is_humiliated(player, card, AL_TRIG)))
	{
	  // Checking this directly breaks encapsulation.  Horrid.
	  if (event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(player, card) && instance->kill_code != KILL_EXILE && !check_for_death_trigger_removal(player, card))
		{
		  /* Have to store affected creatures while this is still in play.
		   *
		   * Store one card index per word in the targets array, and assume all are controlled by the other player.  Not updateable if some other simultaneous
		   * trigger causes a control change (Sower of Temptations, perhaps), but putting only one per target isn't an option while copy effects use targets[12]. */
		  instance->number_of_targets = 0;
		  int16_t* tgt = (int16_t*)(&instance->targets[0].player);

		  if (player == current_turn)
			for_each_creature_blocking_me(player, card, mark_creatures, (intptr_t)(&tgt), 0);

		  if (player == 1-current_turn)
			for_each_creature_blocked_by_me(player, card, mark_creatures, (intptr_t)(&tgt), 0);
		}

	  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
		{
		  card_instance_t* corpse = get_card_instance(instance->damage_source_player, instance->damage_source_card);
		  int16_t* tgt;
		  for (tgt = (int16_t*)(&corpse->targets[0].player); *tgt != -1; ++tgt)
			kill_card(1-player, *tgt, KILL_BURY);

		  event_flags |= EF_RERUN_DAMAGE_PREVENTION;
		}
	}

  return 0;
}

int card_aladdin(int player, int card, event_t event){
/* CARD_ID_ALADDIN	392
Aladdin	|2|R|R
Creature - Human Rogue 1/1
|1|R|R, |T: Gain control of target artifact for as long as you control ~. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			gain_control_until_source_is_in_play_and_tapped(instance->parent_controller, instance->parent_card,
															instance->targets[0].player, instance->targets[0].card,
															GCUS_SOURCE_IN_PLAY | GCUS_CONTROLLED);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XR(1, 2), 0, &td, "TARGET_ARTIFACT");
}

int card_aladdins_lamp(int player, int card, event_t event){
/* CARD_ID_ALADDINS_LAMP	393
Aladdin's Lamp	|10
Artifact
|X, |T: The next time you would draw a card this turn, instead look at the top X cards of your library,
put all but one of them on the bottom of your library in a random order, then draw a card. X can't be 0. */

	if(trigger_condition == TRIGGER_REPLACE_CARD_DRAW && reason_for_trigger_controller == player
		&& affect_me(player, card) && !suppress_draw
	   ){
		int result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL);
		if( result && (has_mana_for_activated_ability(player, card, MANACOST_X(2)) || ! duh_mode(player)) ){
			if (event == EVENT_TRIGGER){
				event_result = RESOLVE_TRIGGER_AI(player);
			}
			else if (event == EVENT_RESOLVE_TRIGGER){
					if( charge_mana_for_activated_ability(player, card, MANACOST_X(0)) ){
						charge_mana(player, COLOR_COLORLESS, -1);
						if( spell_fizzled != 1 ){
							tap_card(player, card);
							int amount = MIN(x_value, count_deck(player));
							test_definition_t this_test;
							default_test_definition(&this_test, TYPE_ANY);
							this_test.create_minideck = amount;
							this_test.no_shuffle = 1;
							int card_added = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 1, AI_MAX_VALUE, &this_test);
							amount--;
							if( amount ){
								put_top_x_on_bottom_in_random_order(player, amount);
							}
							put_on_top_of_deck(player, card_added);
						}
					}
			}
		}
	}

	return 0;
}

int card_aladdins_ring(int player, int card, event_t event){
	/* Aladdin's Ring	|8	0x200c1c2
	 * Artifact
	 * |8, |T: ~ deals 4 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(8), 0, &td, "TARGET_ANY");
}

int card_ali_baba(int player, int card, event_t event){
/* CARD_ID_ALI_BABA	394
Ali Baba	|R
Creature - Human Rogue 1/1
|R: Tap target Wall. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.required_subtype = SUBTYPE_WALL;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_WALL");
}

int card_ali_from_cairo(int player, int card, event_t event)
{
/* CARD_ID_ALI_FROM_CAIRO	395
Ali from Cairo	|2|R|R
Creature - Human 0/1
Damage that would reduce your life total to less than 1 reduces it to 1 instead. */

  // 0x4c48a0
  // Used externally
  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	ai_modifier += 48;

  card_instance_t* damage;
  if (event == EVENT_DAMAGE_REDUCTION
	  && event_result < 1
	  && (damage = get_card_instance(affected_card_controller, affected_card))
	  && damage->damage_target_player == player && damage->damage_target_card == -1)
	event_result = MIN(1, life[player]);	// If life already below 1, then neither increase nor decrease it

  if (event == EVENT_ATTACK_RATING && affect_me(player, card))
	ai_defensive_modifier += 24;

  if (event == EVENT_BLOCK_RATING && affect_me(player, card))
	ai_defensive_modifier -= 128;

  if (event == EVENT_SHOULD_AI_PLAY && in_play(player, card))
	{
	  if (player == AI)
		ai_modifier += 480;
	  else
		ai_modifier -= 480;
	}

  return 0;
}

int card_army_of_allah(int player, int card, event_t event){
/* CARD_ID_ARMY_OF_ALLAH	397
Army of Allah	|1|W|W
Instant
Attacking creatures get +2/+0 until end of turn. */

	if(event == EVENT_RESOLVE_SPELL){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.state = STATE_ATTACKING;
		pump_creatures_until_eot(player, card, current_turn, 0, 2, 0, 0, 0, &test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

// bird maiden --> vanilla

int card_bottle_of_suleiman(int player, int card, event_t event){
	/* Bottle of Suleiman	|4
	 * Artifact
	 * |1, Sacrifice ~: Flip a coin. If you win the flip, put a 5/5 colorless Djinn artifact creature token with flying onto the battlefield.
	 If you lose the flip, ~ deals 5 damage to you. */

	if( event == EVENT_ACTIVATE && player == AI ){
		ai_modifier+=(5*(count_subtype(1-player, TYPE_CREATURE, -1)-count_subtype(player, TYPE_CREATURE, -1)));
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( flip_a_coin(player, card) ){
			generate_token_by_id(player, card, CARD_ID_DJINN);
		}
		else{
			damage_player(player, 5, player, card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL);
}

int card_brass_man(int player, int card, event_t event){
/* CARD_ID_BRASS_MAN	930
Brass Man	|1
Artifact Creature - Construct 1/3
~ doesn't untap during your untap step.
At the beginning of your upkeep, you may pay |1. If you do, untap ~. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		does_not_untap(player, card, event);

		if( is_tapped(player, card) && has_mana(player, COLOR_COLORLESS, 1) && current_turn == player &&
			upkeep_trigger_mode(player, card, event, RESOLVE_TRIGGER_AI(player))
		  ){
			charge_mana(player, COLOR_COLORLESS, 1);
			if( spell_fizzled != 1 ){
				untap_card(player, card);
			}
		}
	}

	return 0;
}

// camel --> hardcoded. Does the ability work at all ?

int card_bazaar_of_baghdad(int player, int card, event_t event){
  /* CARD_ID_BAZAAR_OF_BAGHDAD	398
   * Bazaar of Baghdad	""
   * Land
   * |T: Draw two cards, then discard three cards. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_ABILITY = 2
  };


	// 0x4A8520

	if (event == EVENT_RESOLVE_SPELL){ play_land_sound_effect(player, card); }
	if (event == EVENT_CAN_ACTIVATE && mana_colors_added[player]){ return mana_producer(player, card, event); }
	if (!IS_GAA_EVENT(event)){ return mana_colors_added[player] ? mana_producer(player, card, event) : 0; }
	card_instance_t *instance = get_card_instance(player, card);
	if (event == EVENT_ACTIVATE){
	    if (mana_colors_added[player]){
	        if (paying_mana() || !generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL)){
	            instance->info_slot = CHOICE_MANA;
	            mana_producer(player, card, event);
	        }else{
	            instance->info_slot = CHOICE_ABILITY;
	            int choice = CHOICE_ABILITY;
	            if (!IS_AI(player)){
	                int priority = count_deck(player) <= 10 ? -100 : 0;
	                if (count_graveyard(player) <= 29){ priority += 25; }
	                choice = DIALOG(player, card, event, DLG_RANDOM, "Get mana", 1, 1, "Use Ability", 1, priority);
	                if (!choice){ spell_fizzled = 1; return 0; }
	            }
	            if (choice == CHOICE_MANA){ mana_producer(player, card, event); }
	            if (choice == CHOICE_ABILITY){ generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL); }
	        }
	    }
	    if (player == AI){
	        if (deck_ptr[AI][10] == -1){ ai_modifier -= 100; }
	        else if (graveyard_ptr_mutable[AI][29] == -1){ ai_modifier += 25; }
	    }
	    instance->info_slot = CHOICE_ABILITY;
	    return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
	}
	if (event == EVENT_RESOLVE_ACTIVATION && instance->info_slot == CHOICE_ABILITY){
	    draw_cards(player, 2);
	    multidiscard(player, 3, 0);
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_city_in_a_bottle(int player, int card, event_t event){
/* CARD_ID_CITY_IN_A_BOTTLE	402
City in a Bottle	|2
Artifact
Whenever another nontoken permanent with a name originally printed in the Arabian Nights expansion is on the battlefield, its controller sacrifices it.
Players can't cast spells or play lands with a name originally printed in the Arabian Nights expansion. */

#define IS_ARABIAN_NIGHTS(csvid) (((csvid) >= CARD_ID_ABU_JAFAR && (csvid) <= CARD_ID_YDWEN_EFREET)	\
								  || (csvid) == CARD_ID_BRASS_MAN || (csvid) == CARD_ID_DANCING_SCIMITAR)
	if( event == EVENT_RESOLVE_SPELL || event == EVENT_STATIC_EFFECTS ){
		int i, count;
		for (i = 0; i < 2; i++){
			for (count = active_cards_count[i] - 1; count >= 0; --count){
				if( !(i == player && count == card) && in_play(i, count) && is_what(i, count, TYPE_PERMANENT) && !is_token(i, count)){
					int csvid = get_id(i, count);
					if (IS_ARABIAN_NIGHTS(csvid)){
						kill_card(i, count, KILL_SACRIFICE);
					}
				}
			}
		}
	}

	if( event == EVENT_MODIFY_COST_GLOBAL && !is_humiliated(player, card, AL_STATIC)){
		int csvid = get_id(affected_card_controller, affected_card);
		if (IS_ARABIAN_NIGHTS(csvid)){
			infinite_casting_cost();
		}
	}

	return 0;
#undef IS_ARABIAN_NIGHTS
}

int card_city_of_brass(int player, int card, event_t event){
/* CARD_ID_CITY_OF_BRASS	403
City of Brass	""
Land
Whenever ~ becomes tapped, it deals 1 damage to you.
|T: Add one mana of any color to your mana pool. */

	//0x4A86B0
	if(event == EVENT_TAP_CARD && affect_me(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		damage_player(player, 1, player, card);
	}
	return mana_producer(player, card, event);
}

int card_cyclone(int player, int card, event_t event)
{
  /* Cyclone	|2|G|G
   * Enchantment
   * At the beginning of your upkeep, put a wind counter on this enchantment, then sacrifice this enchantment unless you pay |G for each wind counter on it. If you pay, this enchantment deals damage equal to the number of wind counters on it to each creature and each player. */

  // 0x4308f0

  /* At the beginning of your upkeep, put a wind counter on ~, then sacrifice ~ unless you pay |G for each wind counter on it. If you pay, ~ deals damage equal
   * to the number of wind counters on it to each creature and each player. */

  if (event == EVENT_CAN_CAST)
	return 1;

  upkeep_trigger_ability(player, card, event, player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
	  add_counters(player, card, player, card, COUNTER_WIND, 1);
	  int counters = count_counters(player, card, COUNTER_WIND);
	  ASSERT(counters > 0);
	  if( (IS_AI(player) && ! cyclone_should_pay_upkeep(counters))// That function's specific to Cyclone, and returns nonzero if the AI thinks it's better to sacrifice
		  || !charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_GREEN, counters) )
		kill_card(player, card, KILL_SACRIFICE);
	  else
		new_damage_all(player, card, ANYBODY, counters, NDA_PLAYER_TOO, NULL);
	}

  // AI
  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	ai_modifier += 8 * (3 * (life[player] - life[1 - player]) + 6);

  if (event == EVENT_SHOULD_AI_PLAY && count_counters(player, card, COUNTER_WIND) > has_mana(player, COLOR_GREEN, 1))
	kill_card(player, card, KILL_DESTROY);

  return 0;
}

int card_cuombajj_witches(int player, int card, event_t event){
	/* Cuombajj Witches	|B|B	0x200c1d6
	 * Creature - Human Wizard 1/3
	 * |T: ~ deals 1 damage to any target and 1 damage to any target of an opponent's choice. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td1.who_chooses = 1-player;
	td1.preferred_controller = player;
	td1.allow_cancel = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ANY");
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			if( pick_target(&td, "TARGET_ANY") ){
				if( new_pick_target(&td1, "TARGET_ANY", 1, 1) ){
					tap_card(player, card);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( validate_target(player, card, &td, 0) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 1, instance->parent_controller, instance->parent_card);
		}
		if( validate_target(player, card, &td1, 1) ){
			damage_creature(instance->targets[1].player, instance->targets[1].card, 1, instance->parent_controller, instance->parent_card);
		}
	}

	return 0;

}

// dancing scimitar --> vanilla

int card_dandan(int player, int card, event_t event){
/* CARD_ID_DANDAN	406
Dandan	|U|U
Creature - Fish 4/1
~ can't attack unless defending player controls |Han Island.
When you control no |H1Islands, sacrifice ~. */

	landhome(player, card, event, SUBTYPE_ISLAND);
	return 0;
}

int card_desert2(int player, int card, event_t event){
  /* Desert	""
   * Land - Desert
   * |T: Add |C.
   * |T: This land deals 1 damage to target attacking creature. Activate only during the end of combat step. */

/*
Desert
Land � Desert
{T}: Add {C} to your mana pool.
{T}: Desert deals 1 damage to target attacking creature. Activate this ability only during the end of combat step.
*/
	// original code : 0x4A8970

	if( trigger_condition == TRIGGER_END_COMBAT && affect_me(player, card) ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.allowed_controller = current_turn;
		td1.preferred_controller = current_turn;
		td1.required_state = TARGET_STATE_ATTACKING;

		card_instance_t *instance = get_card_instance(player, card);

		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td1, NULL) ){
			if (player == HUMAN && ai_is_speculating != 1){	// Original interface
				if( end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_OPTIONAL) ){
					instance->number_of_targets = 0;
					if( charge_mana_for_activated_ability(player, card, MANACOST0) &&
						new_pick_target(&td1, "Select target attacking creature.", 0, GS_LITERAL_PROMPT) )
					{
						add_state(player, card, STATE_TAPPED);
						damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card);
					}
					else{ remove_state(player, card, STATE_TAPPED); }
					tapped_for_mana_color = -1;

				}
			}

			else {	// AI interface
					instance->number_of_targets = 0;
					if(current_turn == player ){
						return 0;
					}

					int count, num_deserts = 1;
					// Count number of other activateable deserts
					for (count = 0; count < active_cards_count[player]; ++count){
						if (in_play(player, count) && count != card && get_id(player, count) == CARD_ID_DESERT &&
							generic_activated_ability(player, count, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td1, NULL)
							&& !(get_card_instance(player, count)->state & STATE_PROCESSING)) // already activated
						{
							++num_deserts;
						}
					}

					td1.toughness_requirement = num_deserts | TARGET_PT_LESSER_OR_EQUAL | TARGET_PT_INCLUDE_DAMAGE;
					if (!can_target(&td1)){
						return 0;
					}

					if (event == EVENT_TRIGGER){
						event_result |= RESOLVE_TRIGGER_MANDATORY;
					}
					else if (event == EVENT_RESOLVE_TRIGGER){
							if( charge_mana_for_activated_ability(player, card, MANACOST0) &&
								new_pick_target(&td1, "Select target attacking creature.", 0, GS_LITERAL_PROMPT) )
							{
								add_state(player, card, STATE_TAPPED);
							tapped_for_mana_color = -1;
								damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card);
							}
					}
			}
		}
	}

	return mana_producer(player, card, event);
}

int card_desert_nomads(int player, int card, event_t event){
  /* Desert Nomads |2|R
   * Creature � Human Nomad 2/2
   * Desertwalk
   * Prevent all damage that would be dealt to Desert Nomads by Deserts. */

	if( event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card, AL_STATIC) ){
		if( attacking_card_controller == player && attacking_card == card ){
			if( count_subtype(1-current_turn, TYPE_LAND, SUBTYPE_DESERT) ){
				event_result = 1;
			}
		}
	}

	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *affected = get_card_instance(affected_card_controller, affected_card);
		if( affected->internal_card_id == damage_card ){
			if( affected->damage_target_player == player &&  affected->damage_target_card == card &&
				affected->display_pic_csv_id == CARD_ID_DESERT )
			{
				prevent_damage(affected_card_controller, affected_card, affected->info_slot);
			}
		}
	}
	return 0;
}

int card_desert_twister(int player, int card, event_t event){
/* CARD_ID_DESERT_TWISTER	409
Desert Twister	|4|G|G
Sorcery
Destroy target permanent. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
}

int card_diamond_valley(int player, int card, event_t event){
  /* CARD_ID_DIAMOND_VALLEY	410
   * Diamond Valley	""
   * Land
   * |T, Sacrifice a creature: You gain life equal to the sacrificed creature's toughness. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_ABILITY = 2
  };


	// 0x4A8B40

	if (event == EVENT_RESOLVE_SPELL){
	    play_land_sound_effect(player, card);
	    return mana_colors_added[player] ? mana_producer(player, card, event) : 0;
	}
	if (!IS_GAA_EVENT(event)){
	    return mana_colors_added[player] ? mana_producer(player, card, event) : 0;
	}
	if (event == EVENT_CAN_ACTIVATE && mana_colors_added[player]){ return mana_producer(player, card, event); }
	card_instance_t* instance = get_card_instance(player, card);
	if (event == EVENT_ACTIVATE){
	    if (!mana_colors_added[player]){
	        instance->info_slot = CHOICE_ABILITY;
	        return altar_basic(player, card, event, 4, TYPE_CREATURE);
	    }
	    if (!paying_mana() && has_mana_for_activated_ability(player, card, MANACOST_X(1))
	        && altar_basic(player, card, event, 4, TYPE_CREATURE)){
	        instance->info_slot = CHOICE_ABILITY;
	        int choice = CHOICE_ABILITY;
	        if (!IS_AI(player)){
	            choice = DIALOG(player, card, event, DLG_RANDOM, "Get mana", 1, 1, "Use Ability", 1, 1);
	        }
	        if (!choice){ spell_fizzled = 1; return 0; }
	        if (choice == CHOICE_MANA){ mana_producer(player, card, event); }
	        else if (choice == CHOICE_ABILITY){ altar_basic(player, card, event, 4, TYPE_CREATURE); }
	    } else {
	        instance->info_slot = CHOICE_MANA;
	        mana_producer(player, card, event);
	    }
	}
	if (event == EVENT_RESOLVE_ACTIVATION && instance->info_slot == CHOICE_ABILITY){ gain_life(player, instance->targets[1].card); }
	return altar_basic(player, card, event, 4, TYPE_CREATURE);
}

int card_drop_of_honey(int player, int card, event_t event){
/* CARD_ID_DROP_OF_HONEY	411
Drop of Honey	|G
Enchantment
At the beginning of your upkeep, destroy the creature with the least power. It can't be regenerated.
If two or more creatures are tied for least power, you choose one of them. When there are no creatures on the battlefield, sacrifice ~. */
	// 0x430FA0

	card_instance_t *instance = get_card_instance(player, card);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		base_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		if( can_target(&td) ){
			int i;
			int t_player = -1;
			int t_card = -1;
			int doubles_count = 1;
			int min_t = 100;
			int count;
			for(i=0; i<2; i++){
				count = active_cards_count[i]-1;
				while( count > -1 ){
					   if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) ){
						   if( get_power(i, count) < min_t ){
							   t_player = i;
							   t_card = count;
							   min_t = get_power(i, count);
							   doubles_count = 1;
							   instance->targets[doubles_count].player = t_player;
							   instance->targets[doubles_count].card = t_card;
						   }
						   else if( get_power(i, count) == min_t ){
									doubles_count++;
									instance->targets[doubles_count].player = i;
									instance->targets[doubles_count].card = count;
						   }


					   }
					   count--;
				}
			}

			if( doubles_count > 1 ){
				i=0;
				for(i=0; i<2; i++){
					count = 0;
					while( count < active_cards_count[i] ){
							if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) ){
								card_instance_t *this = get_card_instance(i, count);
								this->state |= STATE_CANNOT_TARGET;
							}
							count++;
					}
				}
				int k;
				for(k=0; k<doubles_count; k++){
					card_instance_t *this = get_card_instance(instance->targets[k+1].player, instance->targets[k+1].card);
					this->state  &= ~STATE_CANNOT_TARGET;
				}
				if( pick_target(&td, "TARGET_CREATURE") ){
					instance->number_of_targets = 1;
					t_player = instance->targets[0].player;
					t_card = instance->targets[0].card;
					for(i=0; i<2; i++){
						count = 0;
						while( count < active_cards_count[i] ){
								if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) ){
									card_instance_t *this = get_card_instance(i, count);
									if( this->state & STATE_CANNOT_TARGET ){
										this->state  &= ~STATE_CANNOT_TARGET;
									}
								}
								count++;
						}
					}
				}
			}

			if( t_player != -1 && t_card != -1 ){
				kill_card(t_player, t_card, KILL_BURY);
			}
		}
	}

	if( event == EVENT_STATIC_EFFECTS && ! count_subtype(ANYBODY, TYPE_CREATURE, -1) ){
		kill_card(player, card, KILL_SACRIFICE);
	}

	return global_enchantment(player, card, event);
}

int card_ebony_horse(int player, int card, event_t event)
{
/* CARD_ID_EBONY_HORSE	412
Ebony Horse	|3
Artifact
|2, |T: Untap target attacking creature you control. Prevent all combat damage that would be dealt to and dealt by that creature this turn. */
  // 0x424560

#define DECL_TGT(td)	target_definition_t td;											\
						default_target_definition(player, card, &td, TYPE_CREATURE);	\
						td.allowed_controller = td.preferred_controller = player;		\
						td.required_state = TARGET_STATE_ATTACKING

#define ABILITY()		generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_ATTACKING_CREATURE")

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	ai_modifier += 3 * total_power_of_creatures_by_color[AI][COLOR_ANY];

  if (event == EVENT_CAN_ACTIVATE)
	{
	  DECL_TGT(td);
	  return ABILITY();
	}

  if (event == EVENT_ACTIVATE)
	{
	  DECL_TGT(td);
	  ABILITY();
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  DECL_TGT(td);
	  if (valid_target(&td))
		{
		  maze_of_ith_effect(player, card, instance->targets[0].player, instance->targets[0].card);
		  untap_card(instance->targets[0].player, instance->targets[0].card);
		}

	  get_card_instance(instance->parent_controller, instance->parent_card)->number_of_targets = 0;
	}

  return 0;
#undef DECL_TGT
#undef ABILITY
}

int card_el_hajjaj(int player, int card, event_t event){
/* CARD_ID_EL_HAJJAJ	413
El-Hajjaj	|1|B|B
Creature - Human Wizard 1/1
Whenever ~ deals damage, you gain that much life. */
	// 4C8070
//Also same code address for Kjeldoran Gargoyle, Warrior Angel, Horned Cheetah, Zebra Unicorn.

	spirit_link_effect(player, card, event, player);
	return 0;
}

int card_elephant_graveyard(int player, int card, event_t event){
/* CARD_ID_ELEPHANT_GRAVEYARD	414
Elephant Graveyard	""
Land
|T: Add |C to your mana pool.
|T: Regenerate target Elephant. */
	// 0x4A8D40

	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}


	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_ELEPHANT;
	td.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_REGENERATE
	};

	card_instance_t *instance = get_card_instance(player, card);

#define CAN_REGEN	(!paying_mana() && (land_can_be_played & LCBP_REGENERATION)	\
					 && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(0), 0, &td, "TARGET_CREATURE"))
#define CAN_MANA	(mana_producer(player, card, EVENT_CAN_ACTIVATE))

	if( event == EVENT_CAN_ACTIVATE ){
		if (CAN_REGEN){
			return 99;
		}
		return CAN_MANA;
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int choice = CHOICE_MANA;
		instance->info_slot = choice;
		if (CAN_REGEN){
			if (CAN_MANA){
				choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
								"Produce mana", 1, 1,
								"Regenerate target Elephant", CAN_REGEN, 10);
			}
			else{
				choice = CHOICE_REGENERATE;
			}
		}
		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_REGENERATE){
			if( charge_mana_for_activated_ability(player, card, MANACOST0) && pick_target(&td, "ELEPHANT_GRAVEYARD") ){
				instance->info_slot = CHOICE_REGENERATE;
				tap_card(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_REGENERATE ){
			if( can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
				regenerate_target(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
#undef CAN_REGEN
#undef CAN_MANA
}

int card_erg_raiders(int player, int card, event_t event)
{
  /* CARD_ID_ERG_RAIDERS	415
   * Erg Raiders	|1|B
   * Creature - Human Warrior 2/3
   * At the beginning of your end step, if ~ didn't attack this turn, ~ deals 2 damage to you unless it came under your control this turn. */

  if (event == EVENT_RESOLVE_SPELL ||
      (event == EVENT_DECLARE_ATTACKERS && is_attacking(player, card)))
    get_card_instance(player, card)->info_slot = 0x42;

  if (current_turn == player && eot_trigger(player, card, event))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->info_slot != 0x42)
      damage_player(player, 2, player, card);
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
  }
  return 0;
}

static int erhnam_djinn_ai_pick_target(int player, int card, target_definition_t* td)
{
  //0x452bc0 (called only from exe version of Erhnam Djinn)
  int best_rating = INT_MIN;
  int best_card = -1;

  int c;
  for (c = 0; c < active_cards_count[HUMAN]; ++c)
	if (in_play(HUMAN, c) && would_validate_arbitrary_target(td, HUMAN , c))
	  {
		card_instance_t* instance = get_card_instance(HUMAN, c);
		int abils = instance->regen_status;
		int rating = 0;

		if (abils & get_hacked_walk(player, card, KEYWORD_FORESTWALK))
		  rating += 200;

		if (abils & KEYWORD_DEFENDER)
		  rating += 50;

		if (instance->state & STATE_TAPPED)
		  rating += 3;

		int iid = instance->internal_card_id;
		if (cards_data[iid].extra_ability & EA_ACT_ABILITY)
		  rating += 2;

		if (cards_data[iid].extra_ability & EA_MANA_SOURCE)
		  rating += 1;

		rating += num_bits_set(abils & ~KEYWORD_NONABILITIES);

		rating -= 2 * instance->power;

		if (rating > best_rating)
		  {
			best_rating = rating;
			best_card = c;
		  }
	  }

  return best_card;
}

static int effect_erhnam_djinn(int player, int card, event_t event)
{
  //0x4577b0
  /* Targets:
   * 0: permanent this is attached to (also in card_instance_t::damage_target_player/card)
   * 1.player: landwalk type to add
   * 1.card: 1 iff at least one cleanup phase has been seen */
  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_ABILITIES && affect_me(instance->damage_target_player, instance->damage_target_card) && affected_card != -1)
	event_result |= instance->targets[1].player;

  if (current_phase == PHASE_UPKEEP && player == current_turn && instance->targets[1].card == 1)
	{
	  if (instance->damage_target_card != -1)
		{
		  card_instance_t* ench_inst = get_card_instance(instance->damage_target_player, instance->damage_target_card);
		  ench_inst->regen_status |= KEYWORD_RECALC_ABILITIES;
		}
	  instance->targets[1].card = 0;	// prevent re-entrance
	  kill_card(player, card, KILL_EXILE);
	}

  if (event == EVENT_CLEANUP)
	instance->targets[1].card = 1;

  return 0;
}

int card_erhnam_djinn(int player, int card, event_t event)
{
/* CARD_ID_ERHNAM_DJINN	416
Erhnam Djinn	|3|G
Creature - Djinn 4/5
At the beginning of your upkeep, target non-Wall creature an opponent controls gains |H2forestwalk until your next upkeep.
(It can't be blocked as long as defending player controls |Ha Forest.) */

  //0x452990

  if (event == EVENT_CAST_SPELL && affect_me(player, card)
	  && player == AI && !(trace_mode & 2)
	  && basiclandtypes_controlled[player][get_hacked_color(player, card, COLOR_GREEN)] > 0)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = 1-player;
	  td.required_subtype = SUBTYPE_WALL;
	  td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
	  td.allow_cancel = 0;

	  int tgt = erhnam_djinn_ai_pick_target(player, card, &td);
	  if (tgt >= 0)
		ai_modifier -= 96 * get_card_instance(HUMAN, tgt)->power / MAX(1, life[player]);
	}

  upkeep_trigger_ability(player, card, event, player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = 1-player;
	  td.required_subtype = SUBTYPE_WALL;
	  td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
	  td.allow_cancel = 0;

	  card_instance_t* instance = get_card_instance(player, card);
	  instance->number_of_targets = 0;

	  if (player == AI && !(trace_mode & 2))
		{
		  int tgt = erhnam_djinn_ai_pick_target(player, card, &td);
		  if (tgt >= 0)
			{
			  instance->targets[0].player = 1-player;
			  instance->targets[0].card = tgt;
			  instance->number_of_targets = 1;
			}
		}
	  else if (can_target(&td))
		pick_target(&td, "ERHNAM_DJINN");

	  if (instance->number_of_targets == 1)
		{
		  int leg = create_targetted_legacy_effect(player, card, effect_erhnam_djinn, instance->targets[0].player, instance->targets[0].card);
		  card_instance_t* legacy = get_card_instance(player, leg);
		  legacy->targets[1].player = get_hacked_walk(player, card, KEYWORD_FORESTWALK);
		}
	}

  if (event == EVENT_SHOULD_AI_PLAY
	  && current_phase == PHASE_DISCARD
	  && !basiclandtypes_controlled[player][get_hacked_color(player, card, COLOR_GREEN)])
	ai_modifier += 96;

  return 0;
}

int card_eye_for_an_eye(int player, int card, event_t event){
/* CARD_ID_EYE_FOR_AN_EYE	417
Eye for an Eye	|W|W
Instant
The next time a source of your choice would deal damage to you this turn, instead that source deals that much damage to you
and ~ deals that much damage to that source's controller. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;
	td.extra = damage_card;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			damage_player(target->damage_source_player, target->info_slot, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, "TARGET_DAMAGE", 1, NULL);
}

int card_fishliver_oil(int player, int card, event_t event){
/* CARD_ID_FISHLIVER_OIL	418
Fishliver Oil	|1|U
Enchantment - Aura
Enchant creature (Target a creature as you cast this. This card enters the battlefield attached to that creature.)
Enchanted creature has |H2islandwalk. (It can't be blocked as long as defending player controls |Han Island.) */

	return generic_aura(player, card, event, player, 0, 0, get_hacked_walk(player, card, KEYWORD_ISLANDWALK), 0, 0, 0, 0);
}

int card_flying_carpet(int player, int card, event_t event){
/* CARD_ID_FLYING_CARPET	419
Flying Carpet	|4
Artifact
|2, |T: Target creature gains flying until end of turn. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
								0, 0, KEYWORD_FLYING, 0);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_CREATURE");
}

// flying men --> vanilla

int card_ghazban_ogre(int player, int card, event_t event){
  /* Ghazban Ogre |G
   * 	  Creature - Ogre 2/2
   * 	  At the beginning of your upkeep, if a player has more life than each other player, the player with the most life gains control of
   * 	  Ghazban Ogre. */

	// Used externally
	if( current_turn == player && upkeep_trigger_mode(player, card, event, (life[player] != life[1-player] ? RESOLVE_TRIGGER_MANDATORY : 0)) ){
		if( life[player] > life[1-player] ){
			gain_control_permanently(player, player, card);
		}
		if( life[player] < life[1-player] ){
			gain_control_permanently(1-player, player, card);
		}
	}

	return 0;
}

int card_giant_tortoise(int player, int card, event_t event){
/* CARD_ID_GIANT_TORTOISE	422
Giant Tortoise	|1|U
Creature - Turtle 1/1
~ gets +0/+3 as long as it's untapped. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_TOUGHNESS && affect_me(player, card) ){
			event_result += is_tapped(player, card) ? 0 : 3;
		}
	}
	return 0;
}

int check_for_guardian_beast_protection(int t_player, int t_card){
	if( is_what(t_player, t_card, TYPE_ARTIFACT) && ! is_what(t_player, t_card, TYPE_CREATURE) ){
		int i;
		for(i=0; i<active_cards_count[t_player]; i++ ){
			if( in_play(t_player, i) && get_id(t_player, i) == CARD_ID_GUARDIAN_BEAST && ! is_humiliated(t_player, i, AL_STATIC) && ! is_tapped(t_player, i) ){
				return 1;
			}
		}
	}
	return 0;
}

int card_guardian_beast(int player, int card, event_t event){
/* CARD_ID_GUARDIAN_BEAST	423
Guardian Beast	|3|B
Creature - Beast 2/4
As long as ~ is untapped, noncreature artifacts you control can't be enchanted, they have indestructible, and other players can't gain control of them.
This effect doesn't remove Auras already attached to those artifacts. */

	if( ! is_humiliated(player, card, AL_STATIC) && in_play(player, card) ){
		if( affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) &&
			! is_what(affected_card_controller, affected_card, TYPE_CREATURE)
		  ){
			if( ! is_tapped(player, card) ){
				indestructible(affected_card_controller, affected_card, event);
			}
		}
	}
	return 0;
}

int card_hasran_ogress(int player, int card, event_t event)
{
  /* CARD_ID_HASRAN_OGRESS	424
   * Hasran Ogress	|B|B
   * Creature - Ogre 3/2
   * Whenever ~ attacks, it deals 3 damage to you unless you pay |2. */

  int result;
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }
  result = has_mana(player, 0, 2);
  if ((result != 0) && (charge_mana(player, COLOR_COLORLESS, 2), spell_fizzled != 1))
  {
    return 0;
  }
  damage_player(player, 3, player, card);
  return 0;
}

int card_hurr_jackal(int player, int card, event_t event){
/* CARD_ID_HURR_JACKAL	425
Hurr Jackal	|R
Creature - Jackal 1/1
|T: Target creature can't be regenerated this turn. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	if( player == AI ){
		td.required_state = TARGET_STATE_DESTROYED;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		cannot_regenerate_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
	}
	int mode = GAA_UNTAPPED | GAA_CAN_TARGET;
	mode |= player == AI ? GAA_REGENERATION : 0;

	return generic_activated_ability(player, card, event, mode, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int effect_ifh_biff_efreet(int player, int card, event_t event ){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->targets[0].player > -1 ){
			if( get_id(instance->targets[0].player, instance->targets[0].card) != CARD_ID_IFH_BIFF_EFREET ){
				kill_card(player, card, KILL_EXILE);
			}
			if( ! in_play(instance->targets[0].player, instance->targets[0].card) ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->targets[0].player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->targets[0].player;
		int c = instance->targets[0].card;

		if( event == EVENT_CAN_ACTIVATE ){
			if( can_use_activated_abilities(p, c) ){
				int cless = get_cost_mod_for_activated_abilities(p, c, MANACOST_G(1));
				if( has_mana_multi(player, MANACOST_XG(cless, 1)) ){
					return 1;
				}
			}
		}

		if( event == EVENT_ACTIVATE ){
			int cless = get_cost_mod_for_activated_abilities(p, c, MANACOST_G(1));
			charge_mana_multi(player, MANACOST_XG(cless, 1));
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.keyword = KEYWORD_FLYING;
			new_damage_all(p, c, ANYBODY, 1, NDA_PLAYER_TOO, &this_test);
		}

	}

	return 0;
}

int card_ifh_biff_efreet(int player, int card, event_t event ){
/* CARD_ID_IFH_BIFF_EFREET	426
Ifh-Biff Efreet	|2|G|G
Creature - Efreet 3/3
Flying
|G: ~ deals 1 damage to each creature with flying and each player. Any player may activate this ability. */

	//0x453880

	card_instance_t* instance = get_card_instance(player, card);
	if (event == EVENT_STATIC_EFFECTS && affect_me(player, card)){
	    int found = 0, p, c;
	    for (p = 0; p < 2; ++p){
	        for (c = 0; c <= active_cards_count[p]; ++c){
	            if (in_play(p, c) && is_what(p, c, TYPE_EFFECT)){
	                card_instance_t* legacy = get_card_instance(p, c);
	                if (legacy->info_slot == (intptr_t)effect_ifh_biff_efreet && legacy->targets[0].player == player && legacy->targets[0].card == card){
	                    if (p == player){ kill_card(p, c, KILL_EXILE); }
	                    else if (p == 1){ goto ability; }
	                    else { found = 1; }
	                }
	            }
	        }
	    }
	    if (!found){
	        int fake = add_card_to_hand(1-player, instance->internal_card_id);
	        int legacy = create_legacy_activate(1-player, fake, effect_ifh_biff_efreet);
	        card_instance_t* leg = get_card_instance(1-player, legacy);
	        leg->targets[0].player = player;
	        leg->targets[0].card = card;
	        leg->number_of_targets = 1;
	        obliterate_card(1-player, fake);
	    }
	}
	if (event == EVENT_RESOLVE_ACTIVATION){
	    test_definition_t test;
	    default_test_definition(&test, TYPE_CREATURE);
	    test.keyword = KEYWORD_FLYING;
	    new_damage_all(instance->parent_controller, instance->parent_card, ANYBODY, 1, NDA_PLAYER_TOO, &test);
	}
	ability:
	return generic_activated_ability(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL);
}

int card_island_fish_jasconius(int player, int card, event_t event)
{
/* CARD_ID_ISLAND_FISH_JASCONIUS	427
Island Fish Jasconius	|4|U|U|U
Creature - Fish 6/8
~ doesn't untap during your untap step.
At the beginning of your upkeep, you may pay |U|U|U. If you do, untap ~.
~ can't attack unless defending player controls |Han Island.
When you control no |H1Islands, sacrifice ~. */

	// 0x4C8E40

	does_not_untap(player, card, event);

	// At the beginning of your upkeep, you may pay |U|U|U. If you do, untap ~.
	// Simplification: only ask to pay once, even if there's multiple upkeeps due to Paradox Haze
	if( trigger_condition == TRIGGER_UPKEEP && (event == EVENT_TRIGGER || event == EVENT_RESOLVE_TRIGGER) && affect_me(player, card) &&
		! is_humiliated(player, card, AL_TRIG) && count_upkeeps(player) > 0 && current_turn == player && reason_for_trigger_controller == player )
	{
		if (event == EVENT_TRIGGER && has_mana(player, COLOR_BLUE, 3) ){
			event_result |= ((player == HUMAN && ai_is_speculating != 1) ? RESOLVE_TRIGGER_OPTIONAL
						 : (is_tapped(player, card) ? RESOLVE_TRIGGER_MANDATORY : 0));
		}

		if( event == EVENT_RESOLVE_TRIGGER && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_BLUE, 3) ){
			untap_card(player, card);
		}
	}

	// ~ can't attack unless defending player controls |Han Island.
	// When you control no |H1Islands, sacrifice ~.
	islandhome(player, card, event);

	return 0;
}

int card_island_of_wak_wak(int player, int card, event_t event){
  /* CARD_ID_ISLAND_OF_WAK_WAK	428
   * Island of Wak-Wak	""
   * Land
   * |T: Target creature with flying has base power 0 until end of turn. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_ABILITY = 2
  };



	// 0x4A8F70

	if (event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.required_abilities = KEYWORD_FLYING;

		int c;
		for(c=0; c<active_cards_count[1-player]; c++){
			if( in_play(1-player, c) && is_what(1-player, c, TYPE_CREATURE) && check_for_ability(1-player, c, KEYWORD_FLYING) ){
				if( would_validate_arbitrary_target(&td, 1-player, c) ){
					ai_modifier += get_power(1-player, c)*5;
				}
			}
		}
	}

	if (!IS_GAA_EVENT(event)){
        return mana_colors_added[player] ? mana_producer(player, card, event) : 0;
    }
    if (event == EVENT_CAN_ACTIVATE && mana_colors_added[player]){
        return mana_producer(player, card, event);
    }

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_ACTIVATE){
        if (!mana_colors_added[player]){
            instance->info_slot = CHOICE_ABILITY;
        } else if (paying_mana() || !generic_activated_ability(player, card, EVENT_CAN_ACTIVATE,
                       GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_FLYING")){
            instance->info_slot = CHOICE_MANA;
            mana_producer(player, card, event);
        } else {
            instance->info_slot = CHOICE_ABILITY;
            int choice = IS_AI(player) ? CHOICE_ABILITY : DIALOG(player, card, event, DLG_RANDOM,
                "Get mana", 1, 1, "Use Ability", 1, 1);
            if (!choice){ spell_fizzled = 1; return 0; }
            if (choice == CHOICE_MANA){ mana_producer(player, card, event); }
            else if (choice == CHOICE_ABILITY){
                generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET,
                                          MANACOST0, 0, &td, "TARGET_FLYING");
            }
        }
    }
	if( event == EVENT_RESOLVE_ACTIVATION && instance->info_slot == CHOICE_ABILITY ){
		if( validate_target(player, card, &td, 0) ){
			real_set_pt(instance->parent_controller, instance->parent_card,
                        instance->targets[0].player, instance->targets[0].card, 0, -1, 1);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_FLYING");
}

int card_jandors_saddlebags(int player, int card, event_t event)
{
/* CARD_ID_JANDORS_SADDLEBAGS	430
Jandor's Saddlebags	|2
Artifact
|3, |T: Untap target creature. */

  // 0x424dc0

#define DECL_TGT(td)	target_definition_t td;											\
						default_target_definition(player, card, &td, TYPE_CREATURE);	\
						td.preferred_controller = player

#define ABILITY()		generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_CREATURE")

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	ai_modifier += 3 * total_power_of_creatures_by_color[AI][COLOR_ANY];

  if (event == EVENT_CAN_ACTIVATE)
	{
	  DECL_TGT(td);
	  return ABILITY();
	}

  if (event == EVENT_ACTIVATE)
	{
	  DECL_TGT(td);
	  ABILITY();
	  if (player == AI && cancel != 1)
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  if (!(get_card_instance(instance->targets[0].player, instance->targets[0].card)->state & STATE_TAPPED))
			ai_modifier -= 24;
		}
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  DECL_TGT(td);
	  if (valid_target(&td))
		untap_card(instance->targets[0].player, instance->targets[0].card);

	  get_card_instance(instance->parent_controller, instance->parent_card)->number_of_targets = 0;
	}

  return 0;
#undef DECL_TGT
#undef ABILITY
}

int card_jandors_ring(int player, int card, event_t event){
/* CARD_ID_JANDORS_RING	429
Jandor's Ring	|6
Artifact
|2, |T, Discard the last card you drew this turn: Draw a card. */

	if (event == EVENT_CAN_ACTIVATE){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			this_test.zone = TARGET_ZONE_HAND;
			this_test.state = STATE_DRAWN_THIS_TURN;
			return check_battlefield_for_special_card(player, card, player, 0, &this_test);
		}
	}
	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			int count = active_cards_count[player]-1;
			while( count > -1 ){
					if( in_hand(player, count) && check_state(player, count, STATE_DRAWN_THIS_TURN) ){
						discard_card(player, count);
						tap_card(player, card);
						break;
					}
					count--;
			}
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}
	return 0;
}

int card_jeweled_bird(int player, int card, event_t event)
{
  /* Jeweled Bird	|1
   * Artifact
   * Remove ~ from your deck before playing if you're not playing for ante.
   * |T: Put ~ into the ante. If you do, put all other cards you own from the ante into your graveyard, then draw a card. */

  // 0x44F560



  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* activation = get_card_instance(player, card);
	  int p = activation->parent_controller, c = activation->parent_card;
	  card_instance_t* instance = in_play(p, c);
      if (!instance)
        return 0;

	  int oid = instance->original_internal_card_id;
	  if (is_token(p, c))
		oid = -1;

	  if (ante_cards[player][1] != -1	// anteing at least two cards
		  || (ante_cards[player][0] != -1	// anteing at least one card, and...
			  && (is_token(p, c)	// It'll successfully go to ante, then disappear.
				  // From the exe version.  I don't know whether expansion_rarity is meaningful anymore, but enh.
				  || cards_ptr[oid]->expansion_rarity <= cards_ptr[cards_data[ante_cards[player][0]].id]->expansion_rarity)))
		ai_modifier += player == AI ? 48 : -48;

	  obliterate_card(p, c);

	  int i, prev_ante[16];
	  for (i = 0; i < 16; ++i)
		{
		  prev_ante[i] = ante_cards[player][i];
		  ante_cards[player][i] = -1;
		}
	  ante_cards[player][0] = oid;

	  for (i = 0; i < 16; ++i)
		if (prev_ante[i] != -1)
		  {
			// Pretend they came from exile - close enough.
			int pos = add_card_to_rfg(player, prev_ante[i]);
			from_exile_to_graveyard(player, pos);
		  }

	  draw_a_card(player);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_jihad(int player, int card, event_t event){
/* CARD_ID_JIHAD	432
Jihad	|W|W|W
Enchantment
As ~ enters the battlefield, choose a color and an opponent.
|SWhite creatures get +2/+1 as long as the chosen player controls a nontoken permanent of the chosen color.
When the chosen player controls no nontoken permanents of the chosen color, sacrifice ~. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( player == AI ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.color = COLOR_TEST_WHITE;
			ai_modifier+=10*check_battlefield_for_special_card(player, card, player, 0, &this_test);
			this_test.type_flag = F1_NO_TOKEN;
			this_test.color = 1<<get_deck_color(player, 1-player);
			ai_modifier+=15*check_battlefield_for_special_card(player, card, 1-player, 0, &this_test);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		instance->info_slot = 1<<choose_a_color_and_show_legacy(player, card, 1-player, -1);
	}

	if( in_play(player, card) && (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affected_card_controller == player &&
		(get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_WHITE))
	  ){
		switch( event ){
				case EVENT_POWER:
					event_result += 2;
					break;
				case EVENT_TOUGHNESS:
					event_result ++;
					break;
				default:
					break;
		}
	}

	if( event == EVENT_ABILITIES && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, instance->info_slot);
		this_test.type_flag = F1_NO_TOKEN;
		if( ! check_battlefield_for_special_card(player, card, 1-player, 0, &this_test) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_junun_efreet(int player, int card, event_t event){
/* CARD_ID_JUNUN_EFREET	433
Junun Efreet	|1|B|B
Creature - Efreet 3/3
Flying
At the beginning of your upkeep, sacrifice ~ unless you pay |B|B. */

	basic_upkeep(player, card, event, MANACOST_B(2));
	return 0;
}

int card_juzam_djinn(int player, int card, event_t event){
/* CARD_ID_JUZAM_DJINN	434
Juzam Djinn	|2|B|B
Creature - Djinn 5/5
At the beginning of your upkeep, ~ deals 1 damage to you. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		damage_player(player, 1, player, card);
	}

	return 0;
}

int card_khabal_ghoul(int player, int card, event_t event){
/* CARD_ID_KHABAL_GHOUL	435
Khabal Ghoul	|2|B
Creature - Zombie 1/1
At the beginning of each end step, put a +1/+1 counter on ~ for each creature that died this turn. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier-=(15-(creatures_dead_this_turn*5));
	}

	if( eot_trigger(player, card, event) ){
		add_counters(player, card, player, card, COUNTER_P1_P1, creatures_dead_this_turn);
	}
	return 0;
}

static const char* is_djinn_or_efreet(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	if( has_subtype(player, card, SUBTYPE_DJINN) || has_subtype(player, card, SUBTYPE_EFREET) ){
		return NULL;
	}

	return "must be a Djinn or an Efreet";
}

int card_king_suleiman(int player, int card, event_t event){
/* CARD_ID_KING_SULEIMAN	436
King Suleiman	|1|W
Creature - Human 1/1
|T: Destroy target Djinn or Efreet. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(is_djinn_or_efreet); // So we could ditch TARGET_SPECIAL_DJINN_OR_EFREET

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "KING_SULEIMAN");
}

int card_kird_ape(int player, int card, event_t event){
/* CARD_ID_KIRD_APE	437
Kird Ape	|R
Creature - Ape 1/1
~ gets +1/+2 as long as you control |Ha Forest. */

	// original code : 00453E50

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && in_play(player, card) &&
		! is_humiliated(player, card, AL_STATIC) )
	{
		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_FOREST) ){
			switch( event ){
					case EVENT_POWER:
						event_result++;
						break;
					case EVENT_TOUGHNESS:
						event_result+=2;
						break;
					default:
						break;
			}
		}
	}

	return 0;
}

int card_library_of_alexandria(int player, int card, event_t event){
/* CARD_ID_LIBRARY_OF_ALEXANDRIA	438
Library of Alexandria	""
Land
|T: Add |C to your mana pool.
|T: Draw a card. Activate this ability only if you have exactly seven cards in hand. */

	card_instance_t *instance = get_card_instance( player, card );

	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( hand_count[player] == 7 && generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			return 1;
		}
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int ai_choice = instance->info_slot = 0;
		if( hand_count[player] == 7 && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			ai_choice = 1;
		}
		int choice = 0;
		if( ai_choice == 1 ){
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Draw a card\n Cancel", ai_choice);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		else if( choice == 1 ){
				tap_card(player, card);
				instance->info_slot = 1;
		}
		else{
			spell_fizzled = 1;
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 ){
			draw_cards(player, 1);
		}
		else{
			return mana_producer(player, card, event);
		}
	}

	return 0;
}

int card_magnetic_mountain(int player, int card, event_t event){
/* CARD_ID_MAGNETIC_MOUNTAIN	439
Magnetic Mountain	|1|R|R
Enchantment
|SBlue creatures don't untap during their controllers' untap steps.
At the beginning of each player's upkeep, that player may choose any number of tapped |Sblue creatures he or she controls and pay |4 for each creature chosen this way.
If the player does, untap those creatures. */

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = COLOR_TEST_BLUE;
		this_test.state = STATE_TAPPED;
		ai_modifier += (5*check_battlefield_for_special_card(player, card, 1-player, 0, &this_test)-
						5*check_battlefield_for_special_card(player, card, player, 0, &this_test));
	}

	if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		this_test.state = STATE_TAPPED;
		if( in_play(affected_card_controller, affected_card) && new_make_test_in_play(affected_card_controller, affected_card, -1, &this_test) ){
			get_card_instance(affected_card_controller, affected_card)->untap_status &= ~3;
		}
	}

	if( upkeep_trigger(player, card, event) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		this_test.state = STATE_TAPPED;
		int count = active_cards_count[current_turn]-1;
		int ai_best_choice = -1;
		while( count > -1 ){
				if( in_play(current_turn, count) && new_make_test_in_play(current_turn, count, -1, &this_test) ){
					int leave_tapped = 1;
					if( !has_mana(current_turn, COLOR_COLORLESS, 4) ){
						break;
					} else {
						int choice = 0;
						if( current_turn == AI ){
							if( ai_best_choice == -1 ){
								ai_best_choice = check_battlefield_for_special_card(player, card, current_turn, CBFSC_AI_MAX_VALUE, &this_test);
							}
							if( count != ai_best_choice ){
								choice = 1;
							}
						}
						if( current_turn == HUMAN ){
							choice = do_dialog(current_turn, current_turn, count, -1, -1, " Untap this\n Leave tapped", 0);
						}
						if( choice == 0 ){
							charge_mana(current_turn, COLOR_COLORLESS, 4);
							if( spell_fizzled != 1 ){
								leave_tapped = 0;
							}
						}
					}
					if( ! leave_tapped ){
						untap_card(current_turn, count);
						ai_best_choice = -1;
					}
				}
				count--;
		}
	}

	return 0;
}

int card_merchant_ship(int player, int card, event_t event){
/* CARD_ID_MERCHANT_SHIP	440
Merchant Ship	|U
Creature - Human 0/2
~ can't attack unless defending player controls |Han Island.
Whenever ~ attacks and isn't blocked, you gain 2 life.
When you control no |H1Islands, sacrifice ~. */

	// 0x453FD0
	if( ! is_humiliated(player, card, AL_STATIC) ){
		islandhome(player, card, event);

		if( current_turn == player && is_attacking(player, card) && event == EVENT_DECLARE_BLOCKERS && is_unblocked(player, card) ){
			gain_life(player, 2);
		}
	}

	return 0;
}

// metamorphosis --> uncodeable (mana usage restriction)

int card_mijae_djinn(int player, int card, event_t event)
{
  /* CARD_ID_MIJAE_DJINN	442
Mijae Djinn	|R|R|R
Creature - Djinn 6/3
Whenever ~ attacks, flip a coin. If you lose the flip, remove ~ from combat and tap it. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY && !flip_a_coin(player, card))
  {
    remove_state(player, card, STATE_ATTACKING);
    tap_card(player, card);
  }
  return 0;
}

// Moorish Cavalry --> vanilla

int nafs_asp_legacy(int player, int card, event_t event){

	if (event == EVENT_PHASE_CHANGED && current_phase == PHASE_DRAW && get_card_instance(player, card)->targets[1].player != 66 ){
		if( current_turn == get_card_instance(player, card)->targets[0].player ){
			int dmg = 1;
			if( has_mana(current_turn, COLOR_COLORLESS, 1) ){
				charge_mana_while_resolving(player, card, EVENT_DRAW_PHASE, current_turn, COLOR_COLORLESS, 1);
				if( spell_fizzled != 1 ){
					dmg = 0;
				}
			}
			if( dmg ){
				damage_player(get_card_instance(player, card)->targets[0].player, dmg, player, card);
			}
			get_card_instance(player, card)->targets[1].player = 66;	// prevent being asked multiple times
			kill_card(player, card, KILL_EXILE);
		}
	}

	if(event == EVENT_PHASE_CHANGED && current_phase != PHASE_DRAW ){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	return 0;
}

int card_nafs_asp(int player, int card, event_t event){
/* CARD_ID_NAFS_ASP	444
Nafs Asp	|G
Creature - Snake 1/1
Whenever ~ deals damage to a player, that player loses 1 life at the beginning of his or her next draw step unless he or she pays |1 before that draw step. */

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS) ){
		int value = get_card_instance(player, card)->targets[1].player;
		int t_player = BYTE0(value) ? 0 : 1;
		int legacy = create_legacy_effect(player, card, &nafs_asp_legacy);
		get_card_instance(player, legacy)->targets[0].player = t_player;
	}

	return 0;
}

int card_oasis(int player, int card, event_t event){
  /* CARD_ID_OASIS	445
   * Oasis	""
   * Land
   * |T: Prevent the next 1 damage that would be dealt to target creature this turn. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_ABILITY = 2
  };



	// 0x47A010

	if (event == EVENT_RESOLVE_SPELL){ play_land_sound_effect(player, card); }
	if (event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI){ ai_modifier += 48; }
	if (event == EVENT_CAN_ACTIVATE && mana_colors_added[player]){ return mana_producer(player, card, event); }
	if (!IS_GAA_EVENT(event)){ return mana_colors_added[player] ? mana_producer(player, card, event) : 0; }
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ANY);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_CREATURE;
	card_instance_t *instance = get_card_instance(player, card);
	if (event == EVENT_ACTIVATE){
	    if (!mana_colors_added[player]){
	        instance->info_slot = CHOICE_ABILITY;
	        return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_DAMAGE");
	    }
	    if (paying_mana() || !generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_DAMAGE")){
	        instance->info_slot = CHOICE_MANA;
	        mana_producer(player, card, event);
	    }else{
	        instance->info_slot = CHOICE_ABILITY;
	        int choice = CHOICE_ABILITY;
	        if (!IS_AI(player)){
	            choice = DIALOG(player, card, event, DLG_RANDOM, "Get mana", 1, 1, "Use Ability", 1, 1);
	            if (!choice){ spell_fizzled = 1; return 0; }
	        }
	        if (choice == CHOICE_MANA){ mana_producer(player, card, event); }
	        if (choice == CHOICE_ABILITY){ generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_DAMAGE"); }
	    }
	}
	if (event == EVENT_RESOLVE_ACTIVATION && instance->info_slot == CHOICE_ABILITY && validate_target(player, card, &td, 0)){
	    prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_DAMAGE");
}

int card_old_man_of_the_sea(int player, int card, event_t event){
  /* Old Man of the Sea |1|U|U
   * Creature � Djinn 2/3
   * You may choose not to untap Old Man of the Sea during your untap step.
   * {T}: Gain control of target creature with power less than or equal to Old Man of the Sea's power for as long as Old Man of the Sea
   * 	remains tapped and that creature's power remains less than or equal to Old Man of the Sea's power. */

	choose_to_untap(player, card, event);

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = get_power(player, card) | TARGET_PT_LESSER_OR_EQUAL;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			gain_control_until_source_is_in_play_and_tapped(instance->parent_controller, instance->parent_card,
															instance->targets[0].player, instance->targets[0].card,
															GCUS_SOURCE_IN_PLAY | GCUS_TAPPED | GCUS_CHECK_POWER);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22bbcd0; original cards/arabian_nights.c:2127. */
static int oubliette_legacy(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;
  int target_player;
  if (trigger_condition != TRIGGER_LEAVE_PLAY)
  {
    return 0;
  }
  if (((card == affected_card) && (affected_card_controller == player)) && (event == EVENT_TRIGGER))
  {
    instance = get_card_instance(player, card);
    if (((instance->targets[1].card == trigger_cause) &&
         (result = instance->targets[1].player, result == trigger_cause_controller)) &&
        (result == reason_for_trigger_controller))
    {
      target_player = (int)instance->damage_target_player;
      result = instance->damage_target_card;
      remove_special_flags3(target_player, result, SF3_CANNOT_PHASE_IN);
      phase_in(target_player, result);
      if (instance->targets[2].card == 0x1bf)
      {
        tap_card(target_player, result);
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22c1900; original cards/arabian_nights.c:2148. */
int phase_out_target_until_source_leaves_play(int s_player, int s_card, int t_player, int t_card)
{
  int card;
  card_instance_t *instance;
  int result;

  set_special_flags3(t_player, t_card, SF3_CANNOT_PHASE_IN);
  card = create_targetted_legacy_effect(s_player, s_card, oubliette_legacy, t_player, t_card);
  instance = get_card_instance(s_player, card);
  instance->targets[1].player = s_player;
  instance->targets[1].card = s_card;
  instance->number_of_targets = '\x02';
  result = get_id(s_player, s_card);
  instance->targets[2].card = result;
  phase_out(t_player, t_card);
  return card;
}
#endif

int card_oubliette(int player, int card, event_t event){
  /* Oubliette |1|B|B
   * Enchantment
   * When Oubliette enters the battlefield, target creature phases out until Oubliette leaves the battlefield. */

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		if( ! can_target(&td) ){
			ai_modifier-=25;
		}
		else{
			ai_modifier+=25;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			card_instance_t *instance = get_card_instance(player, card);
			phase_out_target_until_source_leaves_play(player, card, instance->targets[0].player, instance->targets[0].card);
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_piety2(int player, int card, event_t event){
/* CARD_ID_PIETY	448
Piety	|2|W
Instant
Blocking creatures get +0/+3 until end of turn. */

	if( event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.state = STATE_BLOCKING;
		pump_creatures_until_eot(player, card, ANYBODY, 0, 0, 3, 0, 0, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

const char* target_is_destroyed(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  // Normally, we'd just use td.required_state = TARGET_STATE_DESTROYED, but that will fail if the target can't regenerate.
  return (get_card_instance(player, card)->kill_code == KILL_DESTROY || get_card_instance(player, card)->kill_code == KILL_BURY) ? NULL : gs_target_error_destroyed;	//",destroyed"
}

const char* target_is_attached_to_a_land(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (instance->damage_target_card >= 0 && instance->damage_target_player >= 0
	  && is_what(instance->damage_target_player, instance->damage_target_card, TYPE_LAND))
	return NULL;
  else
	return "not attached to a land";
}

int card_pyramids(int player, int card, event_t event)
{
  /* Pyramids	|6
   * Artifact
   * |2: Choose one - Destroy target Aura attached to a land; or the next time target land would be destroyed this turn, remove all damage marked on it
   * instead. */

  // 0x44faf0



  if (!IS_GAA_EVENT(event))
	return 0;

  const char* prompt;
  generic_activated_ability_flags_t mode;
  target_definition_t td;

  // As with the exe version, always activate for pseudo-regeneration during the regeneration phase, and always activate for destruction otherwise.
  if (land_can_be_played & LCBP_REGENERATION)
	{
	  default_target_definition(player, card, &td, TYPE_LAND);
	  td.preferred_controller = player;
	  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	  td.extra = target_predicate_extra(target_is_destroyed);

	  prompt = "TARGET_LAND";
	  mode = GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION;	// Not GAA_REGENERATION, which is incompatible with targeting

	  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  if (player == AI
			  && (instance->targets[0].player != AI || check_for_ability(instance->targets[0].player, instance->targets[0].card, KEYWORD_REGENERATION)))
			ai_modifier -= 48;

		  /* An extremely stripped-down version of regenerate_target(), since this isn't quite the same as regeneration - it's not tapped; if it's a creature,
		   * it's not removed from combat; and it's not affected by can't-regenerate effects. */
		  card_instance_t* tgt = get_card_instance(instance->targets[0].player, instance->targets[0].card);
		  tgt->kill_code = 0;
		  tgt->unknown0x14 = 0;
		  tgt->token_status &= ~STATUS_DYING;
		  tgt->damage_on_card = 0;

		  play_sound_effect(WAV_REGEN);
		}
	}
  else
	{
	  default_target_definition(player, card, &td, TYPE_ENCHANTMENT);
	  td.required_subtype = SUBTYPE_AURA;
	  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	  td.extra = target_predicate_extra(target_is_attached_to_a_land);

	  prompt = "Select target Aura attached to a land.";
	  mode = GAA_CAN_TARGET | GAA_LITERAL_PROMPT;

	  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  if (player == AI)
			ai_modifier += instance->targets[0].player == AI ? -48 : 48;
		  kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

  return generic_activated_ability(player, card, event, mode, MANACOST_X(2), 0, &td, prompt);
}

// repentant blacksmith --> vanilla

int ring_of_ma_ruf_legacy(int player, int card, event_t event){

	if (trigger_condition == TRIGGER_REPLACE_CARD_DRAW
		&& reason_for_trigger_controller == player
		&& affect_me(player, card)
		&& !suppress_draw
		&& get_card_instance(player, card)->targets[1].player != 66
	   ){
		if (event == EVENT_TRIGGER){
			event_result = RESOLVE_TRIGGER_MANDATORY;
		}
		else if (event == EVENT_RESOLVE_TRIGGER){
				test_definition_t wish_test;
		default_test_definition(&wish_test, TYPE_ANY);
		generic_wish_effect(player, card, &wish_test, CARD_ID_BLACK_LOTUS);
				suppress_draw = 1;
				get_card_instance(player, card)->targets[1].player = 66;
				add_state(player, card, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_ring_of_ma_ruf(int player, int card, event_t event){
/* CARD_ID_RING_OF_MARUF	451
Ring of Ma'ruf	|5
Artifact
|5, |T, Exile ~: The next time you would draw a card this turn, instead choose a card you own from outside the game and put it into your hand. */

	if( event == EVENT_ACTIVATE ){
		ai_modifier += 25;
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t *instance = get_card_instance(player, card);
		create_legacy_effect(instance->parent_controller, instance->parent_card, &ring_of_ma_ruf_legacy);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_RFG_ME, MANACOST_X(5), 0, NULL, NULL);
}

int rukh_legacy(int player, int card, event_t event){

	if( eot_trigger(player, card, event) ){
		// Well, I don't know if this has the SUBTYPE_BIRD, as per recent wording, but since it's in the hardcoded range of CSV IDs, it's better to use it
		generate_token_by_id(player, card, CARD_ID_RUKH);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_rukh_egg(int player, int card, event_t event){
	/* Rukh Egg	|3|R
	 * Creature - Bird 0/3
	 * When ~ dies, put a 4/4 |Sred Bird creature token with flying onto the battlefield at the beginning of the next end step. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		create_legacy_effect(player, card, &rukh_legacy);
	}

	return 0;
}

int sandals_check(int player, int card, event_t event)
{

  if (get_card_instance(player, card)->damage_target_player > -1)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int p = instance->damage_target_player;
    int c = instance->damage_target_card;

    if (event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(p, c))
    {
      if (!check_for_death_trigger_removal(p, c) && is_this_dying(p, c))
      {
        instance->targets[11].player = 1;
        instance->damage_target_player = instance->damage_target_card = -1;
      }
    }
  }

  if (resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    card_instance_t *instance = get_card_instance(player, card);
    kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_sandals_of_abdallah(int player, int card, event_t event){
  /* Sandals of Abdallah |4
   * Artifact
   * {2}, {T}: Target creature gains islandwalk until end of turn. When that creature dies this turn, destroy Sandals of Abdallah.
   * 	(A creature with islandwalk can't be blocked as long as defending player controls an Island.) */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
								0, 0, get_hacked_walk(instance->parent_controller, instance->parent_card, KEYWORD_ISLANDWALK), 0);
		int l2 = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &sandals_check,
												instance->targets[0].player, instance->targets[0].card);
		get_card_instance(player, l2)->targets[1].player = instance->parent_controller;
		get_card_instance(player, l2)->targets[1].card = instance->parent_card;
		get_card_instance(player, l2)->number_of_targets = 2;
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_CREATURE");
}

int card_sandstorm(int player, int card, event_t event){
/* CARD_ID_SANDSTORM	454
Sandstorm	|G
Instant
~ deals 1 damage to each attacking creature. */

	if( event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.state = STATE_ATTACKING;
		new_damage_all(player, card, ANYBODY, 1, 0, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_serendib_djinn(int player, int card, event_t event){
/* CARD_ID_SERENDIB_DJINN	455
Serendib Djinn	|2|U|U
Creature - Djinn 5/6
Flying
At the beginning of your upkeep, sacrifice a land. If you sacrifice |Han Island this way, ~ deals 3 damage to you.
When you control no lands, sacrifice ~. */

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);

		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_RETURN_CHOICE | SAC_NO_CANCEL, &test);
		int dmg = has_subtype(BYTE2(sac), BYTE3(sac), get_hacked_subtype(player, card, SUBTYPE_ISLAND)) ? 1 : 0;
		kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		if( dmg ){
			damage_player(player, 3, player, card);
		}
	}

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( ! check_battlefield_for_subtype(player, TYPE_LAND, -1) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

// serendib efree --> juzam djinn

// sharazad --> uncodeable

int card_sindbad(int player, int card, event_t event){
/* CARD_ID_SINDBAD	458
Sindbad	|1|U
Creature - Human 1/1
|T: Draw a card and reveal it. If it isn't a land card, discard it. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		draw_cards(player, 1);
		int count = active_cards_count[player]-1;
		while( count > -1 ){
				if( in_hand(player, count) && check_state(player, count, STATE_DRAWN_THIS_TURN) ){
					reveal_card(instance->parent_controller, instance->parent_card, player, count);
					if( ! is_what(player, count, TYPE_LAND) ){
						discard_card(player, count);
					}
					break;
				}
				count--;
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_singing_tree(int player, int card, event_t event){
/* CARD_ID_SINGING_TREE	459
Singing Tree	|3|G
Creature - Plant 0/3
|T: Target attacking creature has base power 0 until end of turn. */

	if (event == EVENT_ATTACK_RATING && affect_me(player, card))
		ai_defensive_modifier += 12;

	if (event == EVENT_BLOCK_RATING && affect_me(player, card))
		ai_defensive_modifier -= 12;

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION){
		if ( valid_target(&td)){
			real_set_pt(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 0, -1, SET_PT_END_AT_EOT);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(0), 0, &td, "TARGET_CREATURE");
}

int card_sorceress_queen(int player, int card, event_t event){
  /* Sorceress Queen	|1|B|B
   * Creature - Human Wizard 1/1
   * |T: Target creature other than ~ becomes 0/2 until end of turn. */

  // 0x4c6f60



	if (event == EVENT_ATTACK_RATING && affect_me(player, card))
		ai_defensive_modifier += 12;

	if (event == EVENT_BLOCK_RATING && affect_me(player, card))
		ai_defensive_modifier -= 12;

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.special = TARGET_SPECIAL_NOT_ME;

	if (event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t *instance = get_card_instance( player, card );
		real_set_pt(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 0, 2, SET_PT_END_AT_EOT);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(0), 0, &td, "TARGET_ANOTHER_CREATURE");
}

// stone throwing devils --> vanilla

int card_unstable_mutation(int player, int card, event_t event)
{
/* CARD_ID_UNSTABLE_MUTATION	462
Unstable Mutation	|U
Enchantment - Aura
Enchant creature
Enchanted creature gets +3/+3.
At the beginning of the upkeep of enchanted creature's controller, put a -1/-1 counter on that creature. */

  // 0x4bc7a0

  card_instance_t* instance = in_play(player, card);

  if (instance)
	upkeep_trigger_ability(player, card, event, instance->damage_target_player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY && instance)
	add_counters(player, card, instance->damage_target_player, instance->damage_target_card, COUNTER_M1_M1, 1);

  // Enchanted creature gets +3/+3.
  int rval = generic_aura(player, card, event, player, 3,3, 0,0, 0,0,0);
  if (event == EVENT_CAST_SPELL && affect_me(player, card) && cancel != 1)
	{
	  if (!instance)
		instance = get_card_instance(player, card);

	  if (is_sick(instance->targets[0].player, instance->targets[0].card)
		  || is_tapped(instance->targets[0].player, instance->targets[0].card))
		ai_modifier -= 99;
	}

  if (event == EVENT_SHOULD_AI_PLAY && instance && instance->damage_target_player >= 0 && instance->damage_target_card >= 0)
	{
	  card_instance_t* aff = get_card_instance(instance->damage_target_player, instance->damage_target_card);
	  aff->counter_power -= 2;
	  aff->counter_toughness -= 2;
	  aff->regen_status |= KEYWORD_RECALC_POWER | KEYWORD_RECALC_TOUGHNESS;
	  if (player == AI && current_turn == AI && !(aff->state & STATE_ATTACKED))
		ai_modifier -= 60;
	}

  return rval;
}

// war elephant --> vanilla

int card_wyluli_wolf(int player, int card, event_t event){
/* CARD_ID_WYLULI_WOLF	464
Wyluli Wolf	|1|G
Creature - Wolf 1/1
|T: Target creature gets +1/+1 until end of turn. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_ydwen_efreet(int player, int card, event_t event){
/* CARD_ID_YDWEN_EFREET	465
Ydwen Efreet	|R|R|R
Creature - Efreet 3/6
Whenever ~ blocks, flip a coin. If you lose the flip, remove ~ from combat and it can't block this turn.
Creatures it was blocking that had become blocked by only ~ this combat become unblocked. */
   if( event == EVENT_DECLARE_BLOCKERS && blocking(player, card, event) && !is_humiliated(player, card, AL_TRIG) ){
		if( ! flip_a_coin(player, card) ){
			get_card_instance(player, card)->blocking = 0xff;
			remove_state(player, card, STATE_BLOCKING);
			remove_state(player, card, STATE_UNKNOWN8000);
			pump_ability_until_eot(player, card, player, card, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
		}
	}

	return 0;
}
