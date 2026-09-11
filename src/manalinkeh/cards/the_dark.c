#include "manalink.h"

// Functions
void half_damage_to_target_half_to_player(int damaging_player, int damaging_card, int amount, int target_location){
	int dmg_to_target = amount/2;
	int dmg_to_player = amount/2;
	if( amount-dmg_to_target != dmg_to_target ){
		dmg_to_target = (amount-1)/2;
		dmg_to_player = (amount+1)/2;
	}
	card_instance_t *instance = get_card_instance(damaging_player, damaging_card);
	damage_creature(instance->targets[target_location].player, instance->targets[target_location].card, dmg_to_target, damaging_player, damaging_card);
	damage_player(damaging_player, dmg_to_player, damaging_player, damaging_card);
}

int land_into_new_basic_land_type(int internal_card_id_of_land_to_change, int basic_land_type_to_become){
	// returns the 'internal card id' value to use in EVENT_CHANGE_TYPE;
	int newtype = -1;
	int new_land_iid = 0;  // Hardcoded internal card id for CARD_ID_SWAMP
	if( basic_land_type_to_become == SUBTYPE_ISLAND ){
		new_land_iid = 1;  // Hardcoded internal card id for CARD_ID_ISLAND;
	}
	if( basic_land_type_to_become == SUBTYPE_FOREST ){
		new_land_iid = 2;  // Hardcoded internal card id for CARD_ID_FOREST;
	}
	if( basic_land_type_to_become == SUBTYPE_MOUNTAIN ){
		new_land_iid = 3;  // Hardcoded internal card id for CARD_ID_MOUNTAIN;
	}
	if( basic_land_type_to_become == SUBTYPE_PLAINS ){
		new_land_iid = 4;  // Hardcoded internal card id for CARD_ID_PLAINS;
	}
	if( cards_data[internal_card_id_of_land_to_change].type == TYPE_LAND ){
		newtype = new_land_iid;
	}
	else{
		newtype = create_a_card_type(new_land_iid);
		if (newtype != -1){
			cards_data[newtype].type |= cards_data[internal_card_id_of_land_to_change].type; // For artifact lands
			cards_data[newtype].power = cards_data[internal_card_id_of_land_to_change].power; // For Dryad Arbor
			cards_data[newtype].toughness = cards_data[internal_card_id_of_land_to_change].toughness; // For Dryad Arbor
		}
	}
	return newtype;
}

static int is_enchanting_a_basic_mountain(int player, int card){
	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( has_subtype(instance->damage_target_player, instance->damage_target_card, SUBTYPE_MOUNTAIN) &&
			has_subtype(instance->damage_target_player, instance->damage_target_card, SUBTYPE_BASIC) )
		{
			return 1;
		}
	}
	return 0;
}


// Cards
int card_amnesia(int player, int card, event_t event){
	/*
	  Amnesia |3|U|U|U
	  Sorcery
	  Target player reveals his or her hand and discards all nonland cards.
	*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL && valid_target(&td) ){
		reveal_target_player_hand(instance->targets[0].player);

		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.type_flag = DOESNT_MATCH;

		ec_definition_t ec;
		default_ec_definition(instance->targets[0].player, player, &ec);
		ec.effect = EC_DISCARD | EC_ALL_WHICH_MATCH_CRITERIA;

		new_effect_coercion(&ec, &test);

		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_angry_mob(int player, int card, event_t event){
	/*
	  Angry Mob English |2|W|W
	  Creature - Human 2+x/2+x
	  Trample
	  As long as it's your turn, Angry Mob's power and toughness are each equal to 2 plus the number of Swamps your opponents control.
	  As long as it's not your turn, Angry Mob's power and toughness are each 2.
	*/
	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
		if( current_turn == player && ! is_humiliated(player, card) ){
			event_result += count_subtype(1-player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_SWAMP));
		}
	}

	return 0;
}

int card_apprentice_wizard(int player, int card, event_t event){
/* CARD_ID_APPRENTICE_WIZARD	300
Apprentice Wizard	|1|U|U
Creature - Human Wizard 0/1
|U, |T: Add |C|C|C to your mana pool. */

	// 0x4cad10
	if (event == EVENT_CAN_ACTIVATE){
		if (!is_tapped(player, card) && !is_sick(player, card) && can_produce_mana(player, card) && has_mana(player, COLOR_BLUE, 1)){
			return 1;
		}
	} else if (event == EVENT_ACTIVATE){
		charge_mana(player, COLOR_BLUE, 1);
		if (cancel != 1){
			produce_mana_tapped(player, card, COLOR_COLORLESS, 3);
		}
	} else if (event == EVENT_ATTACK_RATING && affect_me(player, card)){
		ai_defensive_modifier += 36;
	} else if (event == EVENT_BLOCK_RATING && affect_me(player, card)){
		ai_defensive_modifier -= 36;
	}
	return 0;
}

int card_ashes_ot_ashes(int player, int card, event_t event){
	/*
	  Ashes to Ashes |1|B|B
	  Sorcery
	  Exile two target nonartifact creatures. Ashes to Ashes deals 5 damage to you.*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_type = TYPE_ARTIFACT;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		int good = 0;
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_EXILE);
				good++;
			}
		}
		if( good ){
			damage_player(player, 5, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nonartifact creature.", 2, NULL);
}

int card_ball_lightning(int player, int card, event_t event){
	/*
	  Ball Lightning |R|R|R
	  Creature - Elemental 6/1
	  Trample
	  Haste (This creature can attack and {T} as soon as it comes under your control.)
	  At the beginning of the end step, sacrifice Ball Lightning.
	*/
	// Used externally
	if( ! is_humiliated(player, card) ){
		haste(player, card, event);
		if( ! check_state(player, card, STATE_OUBLIETTED) && eot_trigger(player, card, event) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_banshee2(int player, int card, event_t event){
	/* Banshee	|2|B|B	0x200c73f
	 * Creature - Spirit 0/1
	 * |X, |T: ~ deals half X damage, rounded down, to any target, and half X damage, rounded up, to you. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		half_damage_to_target_half_to_player(instance->parent_controller, instance->parent_card, instance->info_slot, 0);
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(-1), 0, &td, "TARGET_ANY");
}

int card_barls_cage(int player, int card, event_t event)
{
/* CARD_ID_BARLS_CAGE	297
Barl's Cage	|4
Artifact
|3: Target creature doesn't untap during its controller's next untap step. */

	// 0x428330
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td_creature;
	default_target_definition(player, card, &td_creature, TYPE_CREATURE);

	target_definition_t td_tapped_creature;
	default_target_definition(player, card, &td_tapped_creature, TYPE_CREATURE);
	td_tapped_creature.required_state = TARGET_STATE_TAPPED;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td_creature)){
		card_instance_t* instance = get_card_instance(player, card);
		does_not_untap_effect(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 0, 1);
	}

	target_definition_t* td = (player == AI || ai_is_speculating == 1) ? &td_tapped_creature : &td_creature;	// specifically not during resolution
	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(3), 0, td, "TARGET_CREATURE");
}

void change_lands_into_new_land_type(int player, int card, event_t event, int orig_basic_land_type, int orig_basic_land_type_flag, int basic_land_type_to_become){
#pragma message"Works fine except for animated lands : if a card that uses this function comes into play after an animated land is animated, it will remain animated forever"

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_CHANGE_TYPE && is_what(affected_card_controller, affected_card, TYPE_LAND) && in_play(player, card)){
		if (!(orig_basic_land_type_flag & TARGET_TYPE_HACK_SLEIGHT_LEGACY)){
			orig_basic_land_type = get_hacked_subtype(player, card, orig_basic_land_type);
		}

		int matches = orig_basic_land_type_flag & (MATCH|DOESNT_MATCH);
		if( matches == MATCH && ! has_subtype(affected_card_controller, affected_card, orig_basic_land_type) ){
			return;
		}
		if( matches == DOESNT_MATCH && has_subtype(affected_card_controller, affected_card, orig_basic_land_type) ){
			return;
		}

		if( (orig_basic_land_type_flag & STATUS_BASICLAND_DEPENDANT) && !is_basic_land(affected_card_controller, affected_card) ){
			return;
		}

		if (is_humiliated(player, card)){
			return;
		}

		if (!(orig_basic_land_type_flag & TARGET_TYPE_HACK_SLEIGHT_LEGACY)){
			basic_land_type_to_become = get_hacked_subtype(player, card, basic_land_type_to_become);
		}

		if( cards_data[event_result].type == TYPE_LAND ){
			event_result = land_into_new_basic_land_type(event_result, basic_land_type_to_become);
		}
		else{
			int newtype = -1;
			int k;
			for(k=0; k<16; k++){
				if( instance->targets[k].player == -1 ){
					break;
				}
				else{
					int svalue = instance->targets[k].player;
					if( svalue & (1<<31) ){
						svalue &= ~(1<<31);
					}
					if( svalue == event_result ){
						newtype = instance->targets[k].card;
						break;
					}
				}
			}
			if( newtype == -1 && k < 16 ){
				newtype = land_into_new_basic_land_type(event_result, basic_land_type_to_become);
				if (newtype != -1){
					instance->targets[k].player = event_result;
					if( check_status(affected_card_controller, affected_card, STATUS_ANIMATED) ){
						instance->targets[k].player |= (1<<31);
					}
					instance->targets[k].card = newtype;
				}
			}
			if( newtype != -1 ){
				event_result = newtype;
			}
		}
	}
	if( event == EVENT_CLEANUP ){
		int k;
		for(k=0; k<16; k++){
			if( instance->targets[k].player > -1 ){
				if( instance->targets[k].player & (1<<31) ){
					int q;
					for(q=k; q<16; q++){
						instance->targets[q] = instance->targets[q+1];
					}
				}
			}
		}
	}
}

int card_blood_moon(int player, int card, event_t event){
/* CARD_ID_BLOOD_MOON	299
Blood Moon	|2|R
Enchantment
Nonbasic lands are |H1Mountains. */

	// 0x42BFF0

	change_lands_into_new_land_type(player, card, event, SUBTYPE_BASIC, DOESNT_MATCH, SUBTYPE_MOUNTAIN);

	return global_enchantment(player, card, event);
}

int blood_of_the_martyr_legacy(int player, int card, event_t event){

	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *instance = get_card_instance(affected_card_controller, affected_card);
		if( instance->internal_card_id == damage_card && instance->info_slot > 0 ){
			if( instance->damage_target_card != -1 ){
				int good = 1;
				if( duh_mode(player) && instance->damage_target_player != player ){
					good = 0;
				}
				if( good ){
					char buffer[100];
					scnprintf(buffer, 100, " Protect this (%d damages)\n Pass", instance->info_slot);
					int ai_choice = (life[player] - instance->info_slot) < 6 ? 1 : 0;
					int choice = do_dialog(player, instance->damage_target_player, instance->damage_target_card, instance->damage_target_player, instance->damage_target_card,
										   buffer, ai_choice);
					if( choice == 0 ){
						instance->damage_target_player = player;
						instance->damage_target_card = -1;
					}
				}
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}


int card_blood_of_the_martyr(int player, int card, event_t event){
/* CARD_ID_BLOOD_OF_THE_MARTYR	301
Blood of the Martyr	|W|W|W
Instant
Until end of turn, if damage would be dealt to any creature, you may have that damage dealt to you instead. */

	if( event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, &blood_of_the_martyr_legacy);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/*
  Bog Imp English |1|B --> vanilla
  Creature - Imp 1/1
  Flying (This creature can't be blocked except by creatures with flying or reach.)
*/

int card_bog_rats(int player, int card, event_t event)
{
/* CARD_ID_BOG_RATS	350
Bog Rats	|B
Creature - Rat 1/1
~ can't be blocked by Walls. */

  // 0x429040
  if (event == EVENT_BLOCK_LEGALITY
	  && attacking_card_controller == player && attacking_card == card
	  && has_subtype(affected_card_controller, affected_card, SUBTYPE_WALL)
	  && !is_humiliated(player, card))
	event_result = 1;

  return 0;
}

int card_bone_flute(int player, int card, event_t event){
	/*
	  Bone Flute |3
	  Artifact
	  {2}, {T}: All creatures get -1/-0 until end of turn.
	*/

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, ANYBODY, -1, -1, 0, 0, 0);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL);

}

int card_book_of_rass(int player, int card, event_t event){
	/*
	  Book of Rass |6
	  Artifact
	  {2}, Pay 2 life: Draw a card.
	*/

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_X(2), 2, NULL, NULL);

}

int card_brainwash(int player, int card, event_t event){
	/*
	  Brainwash |W
	  Enchantment - Aura
	  Enchant creature
	  Enchanted creature can't attack unless its controller pays {3}.
	*/

if (((trigger_condition == TRIGGER_PAY_TO_ATTACK && current_phase == PHASE_DECLARE_ATTACKERS && reason_for_trigger_controller == current_turn)
	  && affect_me(player, card)))
	{
	  card_instance_t* instance = in_play(player, card);
	  if (instance && !instance->info_slot
		  && instance->damage_target_card == trigger_cause && instance->damage_target_player == trigger_cause_controller)
		{
		  if (!has_mana(reason_for_trigger_controller, COLOR_ANY, 3))
			forbid_attack = 1;
		  else if (event == EVENT_TRIGGER)
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		  else if (event == EVENT_RESOLVE_TRIGGER)
			{
			  if (charge_mana_while_resolving(player, card, event, reason_for_trigger_controller, COLOR_COLORLESS, 3))
				{
				  forbid_attack = 0;
				  instance->info_slot = 1;
				}
			  else
				{
				  forbid_attack = 1;
				  cancel = 0;
				}
			}

		  if (forbid_attack)
			remove_state(instance->damage_target_player, instance->damage_target_card, STATE_UNKNOWN8000);
		}
	}

  if (event == EVENT_ATTACK_LEGALITY)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (affect_me(instance->damage_target_player, instance->damage_target_card)
		  && !instance->info_slot
		  && !has_mana(event == EVENT_ATTACK_LEGALITY ? current_turn : 1-current_turn, COLOR_ANY, 3))
		event_result = 1;
	}

	return disabling_aura(player, card, event);
}

int card_brothers_of_fire(int player, int card, event_t event){
	/* Brothers of Fire	|1|R|R	0x200c753
	 * Creature - Human Shaman 2/2
	 * |1|R|R: ~ deals 1 damage to any target and 1 damage to you. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		damage_target0(player, card, 1);
		damage_player(instance->parent_controller, 1, instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 2), 0, &td, "TARGET_ANY");
}

/*
Carnivorous Plant |3|G --> vanilla
Creature - Plant Wall 4/5
Defender
*/

int card_cave_people(int player, int card, event_t event){
	/*
	  Cave People |1|R|R
	  Creature - Human 1/4
	  Whenever Cave People attacks, it gets +1/-2 until end of turn.
	  {1}{R}{R}, {T}: Target creature gains mountainwalk until end of turn.
	*/
	if( declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		alternate_legacy_text(1, player, pump_until_eot(player, card, player, card, 1, -2));
	}


	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		alternate_legacy_text(2, player, pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
								                        0, 0, get_hacked_walk(instance->parent_controller, instance->parent_card, KEYWORD_MOUNTAINWALK), 0));
	}
	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_XR(1, 2), 0, &td, "TARGET_CREATURE");
}

int card_city_of_shadows(int player, int card, event_t event){

	/* City of Shadows	""
	 * Land
	 * |T, Exile a creature you control: Put a storage counter on ~.
	 * |T: Add |X to your mana pool, where X is the number of storage counters on ~. */

	if (event == EVENT_COUNT_MANA && affect_me(player, card) && mana_producer(player, card, EVENT_CAN_ACTIVATE) ){
		declare_mana_available(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_STORAGE));
	}

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;
	if( IS_AI(player) ){
		td.required_state = TARGET_STATE_DESTROYED;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_EXILE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			if (!IS_AI(player)){
				return 1;
			}
			else if (land_can_be_played & LCBP_REGENERATION){
				return 99;
			}
		}
		if( ! IS_AI(player) || count_counters(player, card, COUNTER_STORAGE) ){
			return mana_producer(player, card, event);
		}
	}

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( IS_AI(player) && ! count_counters(player, card, COUNTER_STORAGE) ){
			choice = CHOICE_EXILE;
		}
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) )
		{
			choice = CHOICE_EXILE;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_EXILE && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Exile a creature", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			produce_mana_tapped(player, card, COLOR_COLORLESS, count_counters(player, card, COUNTER_STORAGE));
		}

		if( choice == CHOICE_EXILE ){
			instance->number_of_targets = 0;
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									  &td, "Select target creature you control.");					  
									  
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION && instance->info_slot == CHOICE_EXILE ){
		add_counter(instance->parent_controller, instance->parent_card, COUNTER_STORAGE);
	}

	return 0;
}

int card_cleansing2(int player, int card, event_t event){
/* CARD_ID_CLEANSING	367
Cleansing	|W|W|W
Sorcery
For each land, destroy that land unless any player pays 1 life. */

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p, {
					int count = active_cards_count[p]-1;
					while( count > -1 ){
							if( in_play(p, count) && is_what(p, count, TYPE_LAND) ){
								int kill = 1;
								if( can_pay_life(p, 1) ){
									if( do_dialog(p, p, count, -1, -1, " Save this land\n Pass", life[p] > 5) == 0 ){
										lose_life(p, 1);
										kill = 0;
									}
								}
								if( kill == 1 ){
									kill_card(p, count, KILL_DESTROY);
								}
							}
							count--;
					}
				}
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_coal_golem(int player, int card, event_t event){
	/*
	  Coal Golem |5
	  Artifact Creature - Golem 3/3
	  {3}, Sacrifice Coal Golem: Add {R}{R}{R} to your mana pool.
	*/
	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(3), 0, NULL, NULL) ){
			return can_produce_mana(player, card);
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(3)) ){
			produce_mana(player, COLOR_RED, 3);
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_curse_artifact(int player, int card, event_t event){
	/*
	  Curse Artifact English |2|B|B
	  Enchantment - Aura
	  Enchant artifact
	  At the beginning of the upkeep of enchanted artifact's controller, Curse Artifact deals 2 damage to that player unless he or she sacrifices that artifact.
	*/

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		upkeep_trigger_ability(player, card, event, p);

		if (event == EVENT_UPKEEP_TRIGGER_ABILITY){
			if( DIALOG(p, c, EVENT_ACTIVATE,
						  DLG_FULLCARD(player, card), DLG_SMALLCARD(p, c), DLG_WHO_CHOOSES(p),
						  DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
						  "Take 2 damage", 1, life[player]-2 > 6 ? 2 : 0,
						  "Sacrifice this artifact", 1, 1) == 1
			  ){
				damage_player(p, 2, player, card);
			}
			else
			{
				kill_card(p, c, KILL_SACRIFICE);
			}
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	return targeted_aura(player, card, event, &td, "TARGET_ARTIFACT");
}

static int dance_of_many_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		card_instance_t *token = get_card_instance(p, c);
		int dom_p = token->damage_source_player;
		int dom_c = token->damage_source_card;

		if( other_leaves_play(player, card, dom_p, dom_c, event) ){
			kill_card(p, c, KILL_EXILE);
		}
		if( other_leaves_play(player, card, p, c, event) ){
			kill_card(dom_p, dom_c, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_dance_of_many(int player, int card, event_t event){
/*
Dance of Many |U|U
Enchantment
When Dance of Many enters the battlefield, put a token that's a copy of target nontoken creature onto the battlefield.
When Dance of Many leaves the battlefield, exile the token.
When the token leaves the battlefield, sacrifice Dance of Many.
At the beginning of your upkeep, sacrifice Dance of Many unless you pay {U}{U}.
*/
	basic_upkeep(player, card, event, MANACOST_U(2));

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.illegal_type = TARGET_TYPE_TOKEN;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			token_generation_t token;
			default_token_definition(player, card, get_id(instance->targets[0].player, instance->targets[0].card), &token);
			token.legacy = 1;
			token.special_code_for_legacy = &dance_of_many_legacy;
			generate_token(&token);
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_dark_heart_of_the_woods(int player, int card, event_t event){
/* CARD_ID_DARK_HEART_OF_THE_WOOD	298
Dark Heart of the Wood	|B|G
Enchantment
Sacrifice |Ha Forest: You gain 3 life. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t test;
	default_test_definition(&test, TYPE_LAND);
	test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);

	if( event == EVENT_ACTIVATE ){
		scnprintf(test.message, 100, "%s", get_hacked_land_text(player, card, "Select a %s to sacrifice", SUBTYPE_FOREST));
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 3);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, 0);
}

int card_dark_sphere(int player, int card, event_t event){
/* CARD_ID_DARK_SPHERE	369
Dark Sphere	|0
Artifact
|T, Sacrifice ~: The next time a source of your choice would deal damage to you this turn, prevent half that damage, rounded down. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = (get_card_instance(instance->targets[0].player, instance->targets[0].card)->info_slot +1) >> 1;
			prevent_damage(instance->targets[0].player, instance->targets[0].card, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME | GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET,
									MANACOST0, 0, &td, "TARGET_DAMAGE");
}

int card_deep_water(int player, int card, event_t event){
	// New card
	/*
	  Deep Water English |U|U
	  Enchantment
	  {U}: Until end of turn, if you tap a land you control for mana, it produces {U} instead of any other type.
	*/
	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[2].card == 66 && event == EVENT_CAST_SPELL && affected_card_controller == player &&
		is_what(affected_card_controller, affected_card, TYPE_LAND)
	  ){
		set_special_flags(affected_card_controller, affected_card, SF2_DEEP_WATER);
	}

	if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
		return player == AI ? 	generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST_U(1), 0, NULL, NULL) :
								generic_activated_ability(player, card, event, 0, MANACOST_U(1), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		get_card_instance(instance->parent_controller, instance->parent_card)->targets[2].card = 66;
		set_special_flags(player, -1, SF2_DEEP_WATER);
	}

	if (event == EVENT_CLEANUP){
		instance->targets[2].player = 0; // Resetting GAA_ONCE_PER_TURN for AI
		instance->targets[2].card = 0;
		remove_special_flags(player, -1, SF2_DEEP_WATER);
	}

	return global_enchantment(player, card, event);
}

int card_diabolic_machine(int player, int card, event_t event){
	/*
	  Diabolic Machine |7
	  Artifact Creature - Construct 4/4
	  {3}: Regenerate Diabolic Machine.
	*/
	return regeneration(player, card, event, MANACOST_X(3));
}

/*
Drowned |1|U --> Drudge Skeletons
Creature - Zombie 1/1
{B}: Regenerate Drowned.
*/

int card_dust_to_dust(int player, int card, event_t event){
	/*
	  Dust to Dust |1|W|W
	  Sorcery
	  Exile two target artifacts.
	*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_EXILE);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 2, NULL);
}

int card_eater_of_the_dead(int player, int card, event_t event){
/* CARD_ID_EATER_OF_THE_DEAD	312
Eater of the Dead	|4|B
Creature - Horror 3/4
|0: If ~ is tapped, exile target creature card from a graveyard and untap ~. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( is_tapped(instance->parent_controller, instance->parent_card) ){
			int selected = validate_target_from_grave(player, card, instance->targets[0].player, 1);
			if( selected != -1 ){
				rfg_card_from_grave(instance->targets[0].player, selected);
				untap_card(instance->parent_controller, instance->parent_card);
			}
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST0, 0, NULL, NULL,
															0, &this_test, GS_GRAVE_RECYCLER_BOTH_GRAVES);
}

int card_electric_eel(int player, int card, event_t event){
/* CARD_ID_ELECTRIC_EEL	933
Electric Eel	|U
Creature - Fish 1/1
When ~ enters the battlefield, it deals 1 damage to you.
|R|R: ~ gets +2/+0 until end of turn and deals 1 damage to you. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		damage_player(player, 1, player, card);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
										instance->parent_controller, instance->parent_card, 2, 0);
		damage_player(player, 1, instance->parent_controller, instance->parent_card);
		return 0;
	}

	return generic_shade(player, card, event, 0, MANACOST_R(2), 2, 0, 0, 0);
}

int card_elves_of_the_deep_shadow(int player, int card, event_t event){
	/*
	  Elves of Deep Shadow |G
	  Creature - Elf Druid 1/1
	  {T}: Add {B} to your mana pool. Elves of Deep Shadow deals 1 damage to you.
	*/

	if( event == EVENT_ACTIVATE ){
		damage_player(player, 1, player, card);
		return mana_producer(player, card, event);
	}

	return mana_producer(player, card, event);
}

int card_erosion(int player, int card, event_t event){
	/*
	  Erosion English |U|U|U
	  Enchantment - Aura
	  Enchant land
	  At the beginning of the upkeep of enchanted land's controller, destroy that land unless that player pays {1} or 1 life.
	*/

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		upkeep_trigger_ability(player, card, event, instance->damage_target_player);

		if (event == EVENT_UPKEEP_TRIGGER_ABILITY){
			int sac = 1;
			if( has_mana(player, COLOR_COLORLESS, 1) || can_pay_life(player, 1) ){
				int choice = DIALOG(player, card, EVENT_ACTIVATE,
									DLG_FULLCARD(instance->damage_target_player, instance->damage_target_card),
									DLG_WHO_CHOOSES(instance->damage_target_player),
									DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
									"Pay 1", has_mana(player, COLOR_COLORLESS, 1), 10,
									"Pay 1 life", can_pay_life(player, 1), 5,
									"Decline", 1, 1);
				if( choice == 1 && charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, instance->damage_target_player, MANACOST_X(1))){
					sac = 0;
				}
				if( choice == 2 ){
					lose_life(instance->damage_target_player, 1);
					sac = 0;
				}
			}
			if( sac ){
				kill_card(instance->damage_target_player, instance->damage_target_card, KILL_DESTROY);
			}
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	return targeted_aura(player, card, event, &td, "TARGET_LAND");
}

int card_eternal_flame(int player, int card, event_t event){
	/* Eternal Flame	|2|R|R	0x200c77b
	 * Sorcery
	 * ~ deals X damage to target opponent or planeswalker and half X damage, rounded up, to you, where X is the number of |H1Mountains you control. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	opponent_or_pw_target_definition(player, card, &td);

	if( event == EVENT_RESOLVE_SPELL && valid_target(&td) ){
		int amount = count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN));
		damage_target0(player, card, amount);
		damage_player(player, (amount+1)/2, player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_OPPONENT_OR_PLANESWALKER", 1, NULL);
}

int card_exorcist(int player, int card, event_t event){
	/*
	  Exorcist |W|W
	  Creature - Human Cleric 1/1
	  {1}{W}, {T}: Destroy target black creature.
	*/
	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XW(1, 1), 0,
									&td, get_sleighted_color_text(player, card, "Select target %s creature.", COLOR_BLACK));
}

int card_fasting(int player, int card, event_t event){

	/* Fasting	|W
	 * Enchantment
	 * At the beginning of your upkeep, put a hunger counter on ~. Then destroy ~ if it has five or more hunger counters on it.
	 * If you would begin your draw step, you may skip that step instead. If you do, you gain 2 life.
	 * When you draw a card, destroy ~. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( life[player] > 6 ){
			ai_modifier-=500;
		}
	}

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counter(player, card, COUNTER_HUNGER);
		if( count_counters(player, card, COUNTER_HUNGER) >= 5 ){
			kill_card(player, card, KILL_DESTROY);
		}
	}

	if( event == EVENT_DRAW_PHASE && current_turn == player ){
		int choice = do_dialog(player, player, card, -1, -1, " Skip draw step (gain 2 life)\n Don't skip draw step", 0);
		if( choice == 0 ){
			event_result -= 99;
			gain_life(player, 2);
		}
	}

	if (card_drawn_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY)){
		kill_card(player, card, KILL_DESTROY);
	}
	return global_enchantment(player, card, event);
}

int card_fellwar_stone(int player, int card, event_t event){
/* CARD_ID_FELLWAR_STONE	318
Fellwar Stone	|2
Artifact
|T: Add to your mana pool one mana of any color that a land an opponent controls could produce. */

	// Used externally
	// original code : 0x422360.  Also Exotic Orchard / Sylvok Explorer

	if (event == EVENT_CHANGE_TYPE && affect_me(player, card)){
		card_instance_t* instance = get_card_instance(player, card);
		instance->mana_color = instance->card_color = get_color_of_mana_produced_by_id(get_id(player, card), COLOR_TEST_ANY_COLORED, player);
	}

	if (get_card_instance(player, card)->mana_color == 0 || get_card_instance(player, card)->mana_color == COLOR_TEST_COLORLESS ){
		// Can't produce mana, but can still tap for 0
		if (event == EVENT_CAN_ACTIVATE && player == AI){
			return 0;
		}
		if (event == EVENT_ACTIVATE){
			if (can_produce_mana(player, card)){
				produce_mana_tapped(player, card, COLOR_COLORLESS, 0);
				return 0;
			}
		}
		if (event == EVENT_COUNT_MANA){
			return 0;
		}
	}

	return mana_producer(player, card, event);
}

int card_festival(int player, int card, event_t event){
/* CARD_ID_FESTIVAL	934
Festival	|W
Instant
Cast ~ only during an opponent's upkeep.
Creatures can't attack this turn. */

	// Cast ~ only during an opponent's upkeep.
	if( event == EVENT_CAN_CAST ){
		if( !(current_turn == 1-player && current_phase == PHASE_UPKEEP) ){
			return 0;
		}
	}
	// Creatures can't attack this turn.
	if( event == EVENT_RESOLVE_SPELL ){
		nobody_can_attack_until_eot(player, card, ANYBODY, -1, -1);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_fire_and_brimstone(int player, int card, event_t event){
	/*
	  Fire and Brimstone English |3|W|W
	  Instant
	  Fire and Brimstone deals 4 damage to target player who declared an attacking creature this turn and 4 damage to you.
	*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
    td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( current_phase > PHASE_DECLARE_ATTACKERS ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.state = STATE_ATTACKING | STATE_ATTACKED;
			if( check_battlefield_for_special_card(player, card, current_turn, 0, &this_test) ){
				return would_validate_arbitrary_target(&td, current_turn, -1);
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->targets[0].player = current_turn;
		instance->targets[0].card = -1;
		instance->number_of_targets = 1;
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_player(instance->targets[0].player, 4, player, card);
			damage_player(player, 4, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_fire_drake(int player, int card, event_t event){
	/*
	  Fire Drake English |1|R|R
	  Creature - Drake 1/2
	  Flying
	  {R}: Fire Drake gets +1/+0 until end of turn. Activate this ability only once each turn.
	*/

	if(event == EVENT_POW_BOOST ){
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_ONCE_PER_TURN, MANACOST_R(1), 0, NULL, NULL) ){
			return 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade(player, card, event, 0, MANACOST_R(1), 1, 0, 0, 0);
	}

	return generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST_R(1), 0, NULL, NULL);
}

int card_fissure(int player, int card, event_t event){
	/*
	  Fissure |3|R|R
	  Instant
	  Destroy target creature or land. It can't be regenerated.
	*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_LAND);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature or land.", 1, NULL);
}

int card_flood2(int player, int card, event_t event){
	/*
	  Flood English |U
	  Enchantment
	  {U}{U}: Tap target creature without flying
	*/

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_U(2), 0, &td, "TARGET_CREATURE");
}

int card_fountain_of_youth(int player, int card, event_t event){
/* CARD_ID_FOUNTAIN_OF_YOUTH	324
Fountain of Youth	|0
Artifact
|2, |T: You gain 1 life. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL);
}

int card_frankensteins_monster(int player, int card, event_t event){

	/* Frankenstein's Monster	|X|B|B
	 * Creature - Zombie 0/1
	 * As ~ enters the battlefield, exile X creature cards from your graveyard. If you can't, put ~ into its owner's graveyard instead of onto the
	 * battlefield. For each creature card exiled this way, ~ enters the battlefield with a +2/+0, +1/+1, or +0/+2 counter on it. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( count_graveyard_by_type(player, TYPE_CREATURE) > 0 ){
			int choice = 0;
			int diff = true_get_updated_casting_cost(player, card, -1, event, -1);
			if( player != AI ){
				choice = do_dialog(player, player, card, -1, -1, " Auto mode\n Manual mode\n Cancel", 0);
			}
			if( choice == 0 ){
				int max = count_graveyard_by_type(player, TYPE_CREATURE);
				while( ! has_mana(player, COLOR_COLORLESS, max + diff) ){
						max--;
				}
				charge_mana(player, COLOR_COLORLESS, max + diff);
				if( spell_fizzled != 1 ){
					instance->info_slot = max;
				}
			}
			else if( choice == 1 ){
					charge_mana(player, COLOR_COLORLESS, -1);
					if( spell_fizzled != 1 ){
						instance->info_slot = x_value;
					}
			}
			else{
				spell_fizzled = 1;
			}
		}
		else{
			if( player == AI ){
				ai_modifier -= 96;
			}
		}
	}
	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		add_state(player, card, STATE_OUBLIETTED);

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

		int i;
		for (i = 0; i < instance->info_slot; ++i){
			if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test) != -1 ){
				int choice = 0;
				if (IS_AI(player)){
					if (i == 0){
						choice = 1;
					} else {
						choice = recorded_rand(player, 3);
					}
				} else {
					choice = do_dialog(player, player, card, -1, -1, " +2/+0\n +1/+1\n +0/+2", 1);
				}
				if( choice == 0 ){
					add_counter(player, card, COUNTER_P2_P0);
				} else if (choice == 1){
					add_counter(player, card, COUNTER_P1_P1);
				} else {
					add_counter(player, card, COUNTER_P0_P2);
				}
			}
		}
		remove_state(player, card, STATE_OUBLIETTED);
	}

	return 0;
}

int card_gaeas_touch(int player, int card, event_t event){
/* CARD_ID_GAEAS_TOUCH	334
Gaea's Touch	|G|G
Enchantment
|0: You may put a basic |H2Forest card from your hand onto the battlefield. Activate this ability only any time you could cast a sorcery and only once each turn.
Sacrifice ~: Add |G|G to your mana pool. */

	if( event == EVENT_COUNT_MANA ){
		if( affect_me(player, card) && can_produce_mana(player, card) && can_sacrifice_this_as_cost(player, card) ){
			declare_mana_available(player, COLOR_GREEN, 2);
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t this_test;
	default_test_definition(&this_test, TYPE_LAND);
	this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
	this_test.sub2 = SUBTYPE_BASIC;
	this_test.subtype_flag = F2_MULTISUBTYPE_ALL;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_SAC_FOR_MANA = 1,
		CHOICE_BASIC_FOREST
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_produce_mana(player, card) && can_sacrifice_this_as_cost(player, card) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED | GAA_ONCE_PER_TURN, MANACOST0, 0, NULL, NULL)
			&& ! IS_AI(player) && check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &this_test) ){
				return 1;
		}
	}
	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, (can_produce_mana(player, card) && can_sacrifice_this_as_cost(player, card) ? 1 : 0), 0};
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_ONCE_PER_TURN, MANACOST0, 0, NULL, NULL) ){
			if( can_sorcery_be_played(player, event) && ! paying_mana() &&
				! IS_AI(player) && check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &this_test) )
			{
				abils[2] = 1;
			}
		}
		char msg[100];
		scnprintf(msg, 100, "%s", get_hacked_land_text(player, card, "Put a basic %s on BF", SUBTYPE_FOREST));
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Sacrifice for GG", abils[1], 1,
							msg, abils[2], 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_SAC_FOR_MANA ){
			produce_mana(player, COLOR_GREEN, 2);
			kill_card(player, card, KILL_SACRIFICE);
		}
		if( choice == CHOICE_BASIC_FOREST ){
			return generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_BASIC_FOREST ){
			scnprintf(this_test.message, 100, "%s", get_hacked_land_text(player, card, "Select a basic %s card.", SUBTYPE_FOREST));
			new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test);
		}
	}

	return 0;
}

int card_ghost_ship(int player, int card, event_t event){
	/*
	  Ghost Ship |2|U|U
	  Creature - Spirit 2/4
	  Flying
	  {U}{U}{U}: Regenerate Ghost Ship.
	*/
	return regeneration(player, card, event, MANACOST_U(3));
}


int card_giant_shark(int player, int card, event_t event){
/* CARD_ID_GIANT_SHARK	327
Giant Shark	|5|U
Creature - Fish 4/4
~ can't attack unless defending player controls |Han Island.
Whenever ~ blocks or becomes blocked by a creature that has been dealt damage this turn, ~ gets +2/+0 and gains trample until end of turn.
When you control no |H1Islands, sacrifice ~. */

	if( ! is_humiliated(player, card) ){

		islandhome(player, card, event);

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_DECLARE_BLOCKERS ){
			if( instance->blocking < 255 ){
				int leader = instance->blocking;
				card_instance_t *leader_instance = get_card_instance(1-player, leader);
				if( leader_instance->damage_on_card > 0 ){
					pump_ability_until_eot(player, card, player, card, 2, 0, KEYWORD_TRAMPLE, 0);
				}
				int i;
				for( i=0; i<active_cards_count[1-player]; i++ ){
					card_instance_t *oba_instance = get_card_instance(1-player, i);
					if( i != leader && oba_instance->state & STATE_ATTACKING && oba_instance->blocking == leader && oba_instance->damage_on_card > 0 ){
						pump_ability_until_eot(player, card, player, card, 2, 0, KEYWORD_TRAMPLE, 0);
					}
				}
			}
			else if( instance->state & STATE_ATTACKING && !is_unblocked(player, card) ){
				int j;
				for( j=0; j<active_cards_count[1-player]; j++ ){
					card_instance_t *blocker_instance = get_card_instance(1-player, j);
					if( blocker_instance->blocking == card && blocker_instance->damage_on_card > 0 ){
						pump_ability_until_eot(player, card, player, card, 2, 0, KEYWORD_TRAMPLE, 0);
					}
				}
			}
		}
	}
	return 0;
}

int card_goblin_caves(int player, int card, event_t event){
/* CARD_ID_GOBLIN_CAVES	329
Goblin Caves	|1|R|R
Enchantment - Aura
Enchant land
As long as enchanted land is a basic |H2Mountain, Goblin creatures get +0/+2. */

	if( is_enchanting_a_basic_mountain(player, card) ){
		boost_creature_type(player, card, event, SUBTYPE_GOBLIN, 0, 2, 0, BCT_INCLUDE_SELF);
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;
	if( player == AI ){
		td.extra = 3; // AKA internal card id of "Mountain"
	}

	return targeted_aura(player, card, event, &td, "TARGET_LAND");
}

int card_goblin_digging_team(int player, int card, event_t event)
{
	/*
	  Goblin Digging Team |R
	  Creature - Goblin 1/1
	  {T}, Sacrifice Goblin Digging Team: Destroy target Wall.
	*/

	if (!IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.required_subtype = SUBTYPE_WALL;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_WALL");
}

/*
Goblin Hero English |2|R --> vanilla
Creature - Goblin 2/2, 2R (3)
*/

int card_goblin_rock_sled(int player, int card, event_t event){
/* CARD_ID_GOBLIN_ROCK_SLED	937
Goblin Rock Sled	|1|R
Creature - Goblin 3/1
Trample
~ doesn't untap during your untap step if it attacked during your last turn.
~ can't attack unless defending player controls |Ha Mountain. */
  // 0x4C90F0

	if( event == EVENT_ATTACK_LEGALITY && affect_me(player, card) && ! is_humiliated(player, card) )
		if (!basiclandtypes_controlled[1-player][get_hacked_color(player, card, COLOR_RED)])
		event_result = 1;

	if (event == EVENT_CLEANUP && affect_me(player, card))
		if (player == current_turn)
			get_card_instance(player, card)->info_slot = get_card_instance(player, card)->state & STATE_ATTACKED;

	if( get_card_instance(player, card)->info_slot && ! is_humiliated(player, card) )
		does_not_untap(player, card, event);

	return 0;
}

int card_goblin_shrine(int player, int card, event_t event){
/* CARD_ID_GOBLIN_SHRINE	938
Goblin Shrine	|1|R|R
Enchantment - Aura
Enchant land
As long as enchanted land is a basic |H2Mountain, Goblin creatures get +1/+0.
When ~ leaves the battlefield, it deals 1 damage to each Goblin creature. */

	if( is_enchanting_a_basic_mountain(player, card) ){
		boost_creature_type(player, card, event, SUBTYPE_GOBLIN, 1, 0, 0, BCT_INCLUDE_SELF);
	}

	if( leaves_play(player, card, event) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.subtype = SUBTYPE_GOBLIN;
		new_damage_all(player, card, ANYBODY, 1, 0, &this_test);
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;
	if( player == AI ){
		td.extra = 3; // AKA internal card id of "Mountain"
	}

	return targeted_aura(player, card, event, &td, "TARGET_LAND");
}


int card_goblin_wizard(int player, int card, event_t event){
/*
Goblin Wizard |2|R|R
Creature — Goblin Wizard 1/1
{T}: You may put a Goblin permanent card from your hand onto the battlefield.
{R}: Target Goblin gains protection from white until end of turn.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_GOBLIN;

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a Goblin permanent.");
	this_test.subtype = SUBTYPE_GOBLIN;
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_TUTOR_GOBLIN = 1,
		CHOICE_GIVE_PROT_WHITE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			if( ! IS_AI(player) || check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &this_test) ){
				return 1;
			}
		}
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_R(1), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, 0, generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_R(1), 0, &td, NULL)};
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			if( ! IS_AI(player) || check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &this_test) ){
				abils[1] = 1;
			}
		}
		char msg[100];
		scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Give Prot. from %s", COLOR_WHITE));
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Put a Goblin into play", abils[1], 10,
							msg, abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_TUTOR_GOBLIN ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
		if( choice == CHOICE_GIVE_PROT_WHITE ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_R(1), 0,
											&td, "Select target Goblin.");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_TUTOR_GOBLIN ){
			new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
		if( instance->info_slot == CHOICE_GIVE_PROT_WHITE && valid_target(&td) ){
			int key = get_sleighted_protection(player, card, KEYWORD_PROT_WHITE);
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, key, 0);
		}
	}
	return 0;
}

/*
Goblins of the Flarg English |R --> vanilla
Creature - Goblin Warrior 1/1
Mountainwalk
*/

int card_grave_robbers(int player, int card, event_t event){
/* CARD_ID_GRAVE_ROBBERS	333
Grave Robbers	|1|B|B
Creature - Human Rogue 1/1
|B, |T: Exile target artifact card from a graveyard. You gain 2 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an artifact card.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			rfg_card_from_grave(instance->targets[0].player, selected);
			gain_life(player, 2);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST_B(1), 0, NULL, NULL,
															0, &this_test, GS_GRAVE_RECYCLER_BOTH_GRAVES);
}

int card_hidden_path(int player, int card, event_t event){
	/*
	  Hidden Path |2|G|G|G|G
	  Enchantment
	  Green creatures have forestwalk.
	*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI  ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

		ai_modifier+= (check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test)-
						check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &this_test))*5;
	}

	if( in_play(player, card) && event == EVENT_ABILITIES && in_play(affected_card_controller, affected_card) &&
		! is_humiliated(player, card) )
	{
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		if( in_play(affected_card_controller, affected_card) &&
			new_make_test_in_play(affected_card_controller, affected_card, -1, &this_test) )
		{
			event_result |= get_hacked_walk(player, card, KEYWORD_FORESTWALK);
		}
	}

	return basic_spell(player, card, event);
}

int card_holy_light(int player, int card, event_t event){
	/*
	  Holy Light |2|W
	  Instant
	  Nonwhite creatures get -1/-1 until end of turn.
	*/
	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		this_test.color_flag = DOESNT_MATCH;
		pump_creatures_until_eot(player, card, ANYBODY, 0, -1, -1, 0, 0, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_inferno2(int player, int card, event_t event){
	/*
	  Inferno |5|R|R
	  Instant
	  Inferno deals 6 damage to each creature and each player.
	*/
	if( event == EVENT_RESOLVE_SPELL ){
		new_damage_all(player, card, ANYBODY, 6, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_inquisition2(int player, int card, event_t event){
	/*
	  Inquisition |2|B
	  Sorcery
	  Target player reveals his or her hand. Inquisition deals damage to that player equal to the number of white cards in his or her hand.
	*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			reveal_target_player_hand(instance->targets[0].player);

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
			this_test.zone = TARGET_ZONE_HAND;
			int amount = check_battlefield_for_special_card(player, card, instance->targets[0].player, CBFSC_GET_COUNT, &this_test);
			damage_target0(player, card, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

/*
Knights of Thorn English |3|W --> vanilla
Creature - Human Knight 2/2
Protection from red; banding

Land Leeches English |1|G|G --> vanilla
Creature - Leech 2/2
First strike
*/

int card_leviathan(int player, int card, event_t event)
{
#define DECL_TEST(test)																											\
  test_definition_t test;																										\
  new_default_test_definition(&test, TYPE_LAND, get_hacked_land_text(player, card, "Select %a to sacrifice.", SUBTYPE_ISLAND));	\
  test.subtype = get_hacked_subtype(player, card, SUBTYPE_ISLAND)

  // ~ enters the battlefield tapped and doesn't untap during your untap step.
  comes_into_play_tapped(player, card, event);
  does_not_untap(player, card, event);

  // At the beginning of your upkeep, you may sacrifice two |H1Islands. If you do, untap ~.
  // Simplification: only ask to sacrifice once, even if there's multiple upkeeps due to Paradox Haze
  if (trigger_condition == TRIGGER_UPKEEP && (event == EVENT_TRIGGER || event == EVENT_RESOLVE_TRIGGER) && affect_me(player, card)
	  && !is_humiliated(player, card) && current_turn == player && count_upkeeps(player) > 0)
	{
	  DECL_TEST(test);

	  if (event == EVENT_TRIGGER && (test.qty = 2) && new_can_sacrifice_as_cost(player, card, &test))
		event_result |= ((player == HUMAN && ai_is_speculating != 1) ? RESOLVE_TRIGGER_OPTIONAL
						 : is_tapped(player, card) ? 0
						 : RESOLVE_TRIGGER_MANDATORY);

	  if (event == EVENT_RESOLVE_TRIGGER
		  && new_sacrifice(player, card, player, 0, &test)
		  && new_sacrifice(player, card, player, SAC_NO_CANCEL, &test))
		untap_card(player, card);
	}

  // ~ can't attack unless you sacrifice two |H1Islands.
  if (trigger_condition == TRIGGER_PAY_TO_ATTACK && current_phase == PHASE_DECLARE_ATTACKERS
	  && affect_me(player, card) && reason_for_trigger_controller == player && forbid_attack == 0
	  && trigger_cause_controller == player && trigger_cause == card
	  && (event == EVENT_TRIGGER || event == EVENT_RESOLVE_TRIGGER))
	{
	  DECL_TEST(test);

	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_MANDATORY;

	  if (event == EVENT_RESOLVE_TRIGGER
		  && !((test.qty = 2) && new_can_sacrifice_as_cost(player, card, &test) && (test.qty = 1)
			   && new_sacrifice(player, card, player, 0, &test)
			   && new_sacrifice(player, card, player, SAC_NO_CANCEL, &test)))
		forbid_attack = 1;
	}

  return 0;
#undef DECL_TEST
}

int card_living_armor(int player, int card, event_t event){

	/* Living Armor	|4
	 * Artifact
	 * |T, Sacrifice ~: Put X +0/+1 counters on target creature, where X is that creature's converted mana cost. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			add_counters(instance->targets[0].player, instance->targets[0].card, COUNTER_P0_P1,
						 get_cmc(instance->targets[0].player, instance->targets[0].card));
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_SACRIFICE_ME|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_lurker(int player, int card, event_t event){
/* CARD_ID_LURKER	342
Lurker	|2|G
Creature - Beast 2/3
~ can't be the target of spells unless it attacked or blocked this turn. */

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card) &&
		get_card_instance(player, card)->targets[1].player != 66 )
	{
		event_result |= KEYWORD_SHROUD;
	}

	if( current_turn == player && event == EVENT_DECLARE_ATTACKERS ){
		if( get_card_instance(player, card)->state & STATE_ATTACKING ){
			get_card_instance(player, card)->targets[1].player = 66;
		}
	}

	if( current_turn == 1-player && event == EVENT_DECLARE_BLOCKERS ){
		if( get_card_instance(player, card)->blocking < 255 ){
			get_card_instance(player, card)->targets[1].player = 66;
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = -1;
	}

	return 0;
}

int card_mana_clash(int player, int card, event_t event){
	/*
	  Mana Clash |R
	  Sorcery
	  You and target opponent each flip a coin.
	  Mana Clash deals 1 damage to each player whose coin comes up tails.
	  Repeat this process until both players' coins come up heads on the same flip.
	*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int cf[2] = {1, 1};
			while( cf[0] || cf[1] ){
					cf[0] = player_flips_a_coin(player, card, player);
					if( cf[0] ){
						damage_player(player, 1, player, card);
					}
					cf[1] = player_flips_a_coin(player, card, 1-player);
					if( cf[1] ){
						damage_player(1-player, 1, player, card);
					}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_mana_vortex(int player, int card, event_t event){
/*
Mana Vortex |1|U|U
Enchantment
When you cast Mana Vortex, counter it unless you sacrifice a land.
At the beginning of each player's upkeep, that player sacrifices a land.
When there are no lands on the battlefield, sacrifice Mana Vortex.
*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		get_card_instance(player, card)->info_slot = new_sacrifice(player, card, player, SAC_AS_COST, &test);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( get_card_instance(player, card)->info_slot == 0 ){
			real_counter_a_spell(player, card, player, card);
		}
	}

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		new_sacrifice(player, card, current_turn, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	if( event == EVENT_STATIC_EFFECTS && count_permanents_by_type(ANYBODY, TYPE_LAND) == 0 ){
		kill_card(player, card, KILL_DESTROY);
	}

	return global_enchantment(player, card, event);
}

int card_marsh_gas(int player, int card, event_t event){
	/*
	  Marsh Gas English |B
	  Instant
	  All creatures get -2/-0 until end of turn.
	*/

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, ANYBODY, -1, -2, 0, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/*
Marsh Goblins |B|R --> vanilla
Creature - Goblin 1/1
Swampwalk
*/

int card_marsh_viper(int player, int card, event_t event){
	/*
	  Marsh Viper English |3|G
	  Creature - Snake 1/2
	  Whenever Marsh Viper deals damage to a player, that player gets two poison counters. (A player with ten or more poison counters loses the game.)
	*/
	int result = whenever_i_deal_damage_to_a_player(player, card, event, 0);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p, { poison(p, times_damaged[p] * 2); };);
	}
	return 0;
}

int card_martyrs_cry(int player, int card, event_t event){
	/*
	  Martyr's Cry English |W|W
	  Sorcery
	  Exile all white creatures. For each creature exiled this way, its controller draws a card.
	*/

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);

		int amount = new_manipulate_all(player, card, player, &this_test, KILL_EXILE);
		draw_cards(player, amount);

		amount = new_manipulate_all(player, card, 1-player, &this_test, KILL_EXILE);
		draw_cards(1-player, amount);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static int maze_of_ith_legacy(int player, int card, event_t event){

	card_instance_t* damage = combat_damage_being_prevented(event);
	if (damage){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( (damage->damage_source_player == p && damage->damage_source_card == c) ||
			(damage->damage_target_player == p && damage->damage_target_card == c) )
		{
			prevent_damage(affected_card_controller, affected_card, -1);
		}
	}

	if ((event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY) && affect_me(player, card)){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int maze_of_ith_effect(int player, int card, int t_player, int t_card){
	return create_targetted_legacy_effect( player, card, &maze_of_ith_legacy, t_player, t_card);
}

int card_maze_of_ith(int player, int card, event_t event){
/* CARD_ID_MAZE_OF_ITH	377
Maze of Ith	""
Land
|T: Untap target attacking creature. Prevent all combat damage that would be dealt to and dealt by that creature this turn. */

	// Used externally
	// original code : 00406100

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier+=100;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			untap_card(instance->targets[0].player, instance->targets[0].card);
			maze_of_ith_effect(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ATTACKING_CREATURE");
}

int card_merfolk_assassin(int player, int card, event_t event){
/* CARD_ID_MERFOLK_ASSASSIN	940
Merfolk Assassin	|U|U
Creature - Merfolk Assassin 1/2
|T: Destroy target creature with |H2islandwalk. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_abilities = get_hacked_walk(player, card, KEYWORD_ISLANDWALK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		char msg[100];
		scnprintf(msg, 100, "%s", get_hacked_land_text(player, card, "Select target creature with %swalk.", KEYWORD_ISLANDWALK));
		return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST0, 0, &td, msg);
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card( instance->targets[0].player, instance->targets[0].card, KILL_DESTROY );
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
}

int card_mind_bomb(int player, int card, event_t event){
/*
Mind Bomb |U
Sorcery
Each player may discard up to three cards. Mind Bomb deals damage to each player equal to 3 minus the number of cards he or she
	discarded this way.
*/

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card to discard");

		APNAP(p,{
					int dmg = 3;
					while( dmg && hand_count[p] > 0 && (p == HUMAN || (p== AI && life[p]-dmg < 6)) ){
							int selected = new_select_a_card(p, p, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
							if( selected != -1 ){
								new_discard_card(p, selected, player, 0);
								dmg--;
							}
							else{
								break;
							}
					}
					damage_player(p, dmg, player, card);
				};
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static const char* aura_enchanting_creature(int who_chooses, int player, int card){
	card_instance_t *instance = get_card_instance(player, card);
	if( instance->damage_target_player == who_chooses ){
		if( is_what(instance->damage_target_player, instance->damage_target_card, TYPE_CREATURE) ){
			return NULL;
		}
	}

	return "must be an Aura enchanting a creature you control.";
}

int card_miracle_worker(int player, int card, event_t event){
	/*
	  Miracle Worker |W
	  Creature - Human Cleric 1/1
	  {T}: Destroy target Aura attached to a creature you control.
	*/
	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = (int32_t)aura_enchanting_creature;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target Aura attached to a creature you control.");
}

int card_morale(int player, int card, event_t event){
	/*
	  Morale |1|W|W
	  Instant
	  Attacking creatures get +1/+1 until end of turn.
	*/

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.state = STATE_ATTACKING;
		pump_creatures_until_eot_merge_pt(player, card, current_turn, 1, 1, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_murk_dwellers(int player, int card, event_t event){
	/*
	  Murk Dwellers |3|B
	  Creature - Zombie 2/2
	  Whenever Murk Dwellers attacks and isn't blocked, it gets +2/+0 until end of combat.
	*/

	if( ! is_humiliated(player, card) && event == EVENT_DECLARE_BLOCKERS && is_unblocked(player, card) ){
		pump_until_eot(player, card, player, card, 2, 0);
	}

	return 0;
}

int card_nameless_race(int player, int card, event_t event){
/*
Nameless Race |3|B
Creature x/x
Trample
As Nameless Race enters the battlefield, pay any amount of life. The amount you pay can't be more than the total number of white
	nontoken permanents your opponents control plus the total number of white cards in their graveyards.
Nameless Race's power and toughness are each equal to the life paid as it entered the battlefield.
*/
	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && player > -1 && card > -1 ){
		if( ! is_humiliated(player, card) ){
			event_result += get_card_instance(player, card)->info_slot;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		add_state(player, card, STATE_OUBLIETTED);
		if( can_pay_life(player, 1) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_PERMANENT);
			this_test.type_flag = F1_NO_TOKEN;
			this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);

			int max = check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &this_test);
			max+=new_special_count_grave(1-player, &this_test);
			if( max ){
				int number = 0;
				if( !IS_AI(player) ){
					char msg[100];
					scnprintf(msg, 100, "Nameless Race: Pay how much life? (max %d)", max);
					number = choose_a_number(player, msg, max);
					if( number > max ){
						number = max;
					}
				}
				else{
					number = max;
					while( life[player]-number < 6 ){
							number--;
					}
				}
				lose_life(player, number);
				get_card_instance(player, card)->info_slot = number;
			}
		}
		remove_state(player, card, STATE_OUBLIETTED);
	}
	return 0;
}

int card_necropolis(int player, int card, event_t event){

	/* Necropolis	|5
	 * Artifact Creature - Wall 0/1
	 * Defender
	 * Exile a creature card from your graveyard: Put X +0/+1 counters on ~, where X is the exiled card's converted mana cost. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card to exile.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			return count_graveyard_by_type(player, TYPE_CREATURE) ? 1 : 0;
		}
	}

	if( event == EVENT_ACTIVATE ){
		int selected = new_select_a_card(player, player, TUTOR_FROM_GRAVE, 0, AI_MAX_CMC, -1, &test);
		if( selected == -1 ){
			spell_fizzled = 1;
			return 0;
		}
		generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			instance->info_slot = get_cmc_by_internal_id(get_grave(player)[selected]);
			rfg_card_from_grave(player, selected);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		add_counters(instance->parent_controller, instance->parent_card, COUNTER_P0_P1, instance->info_slot);
	}

	return 0;
}

int card_niall_silvain(int player, int card, event_t event){
	/*
	  Niall Silvain English |G|G|G
	  Creature - Ouphe 2/2
	  {G}{G}{G}{G}, {T}: Regenerate target creature.
	*/
	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) &&
		can_be_regenerated(instance->targets[0].player, instance->targets[0].card) )
	{
		regenerate_target(instance->targets[0].player, instance->targets[0].card);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_G(4), 0, &td, "TARGET_CREATURE") ? 99 : 0;
}

static const char* is_orc_or_goblin_and_can_be_sacrificed(int who_chooses, int player, int card)
{
	if(	can_sacrifice_this_as_cost(player, card) ){
		if( has_subtype(player, card, SUBTYPE_ORC) || has_subtype(player, card, SUBTYPE_GOBLIN) ){
			return NULL;
		}
	}

	return "must be an Orc or Goblin and you could sacrifice it.";
}

int card_orc_general(int player, int card, event_t event){
/*
Orc General |2|R
Creature — Orc Warrior 2/2
{T}, Sacrifice another Orc or Goblin: Other Orc creatures get +1/+1 until end of turn.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = td.allowed_controller = player;
	td.illegal_abilities = 0;
	td.extra = (int32_t)is_orc_or_goblin_and_can_be_sacrificed;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
								&td, "Select an Orc or Goblin to sacrifice.");
		if( spell_fizzled != 1 ){
			instance->number_of_targets = 0;
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, SUBTYPE_ORC, 1, 1, 0, 0);
	}

	return 0;
}

int card_people_of_the_woods(int player, int card, event_t event){
	/*
	  People of the Woods English |G|G
	  Creature - Human 1/X
	  People of the Woods's toughness is equal to the number of Forests you control.
	*/
	if( event == EVENT_TOUGHNESS && affect_me(player, card) && !is_humiliated(player, card) && player != -1){
		event_result += count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_FOREST));
	}

	return 0;
}

int card_preacher(int player, int card, event_t event){

	choose_to_untap(player, card, event);

	/* Preacher	|1|W|W
	 * Creature - Human Cleric 1/1
	 * You may choose not to untap ~ during your untap step.
	 * |T: Gain control of target creature of an opponent's choice that he or she controls for as long as ~ remains tapped. */

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.who_chooses = 1 - player;
	td.allowed_controller = 1 - player;
	td.preferred_controller = 1 - player;
	td.allow_cancel = 0;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		card_instance_t* instance = get_card_instance(player, card);
		gain_control_until_source_is_in_play_and_tapped(instance->parent_controller, instance->parent_card,
														instance->targets[0].player, instance->targets[0].card, GCUS_TAPPED);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "ASHNODS_BATTLEGEAR"); // "Select target creature you control."
}

/*
Pikemen English |1|W --> vanilla
Creature - Human Soldier 1/1
First strike; banding
*/

int card_psychic_allergy(int player, int card, event_t event){
/*
Psychic Allergy |3|U|U
Enchantment
As Psychic Allergy enters the battlefield, choose a color.
At the beginning of each opponent's upkeep, Psychic Allergy deals X damage to that player, where X is the number of nontoken permanents
	of the chosen color he or she controls.
At the beginning of your upkeep, destroy Psychic Allergy unless you sacrifice two Islands.
*/
	if( event == EVENT_RESOLVE_SPELL ){
		int clr = 0;
		if( player != AI ){
			clr = choose_a_color(player, 1);
		}
		else{
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_PERMANENT);
			this_test.type_flag = F1_NO_TOKEN;
			int i;
			for(i=1; i<6; i++){
				this_test.color = 1<<i;
				if( check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &this_test) >
					check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test)
				  ){
					clr = i;
				}
			}
		}
		create_card_name_legacy(player, card, CARD_ID_BLACK+(clr-1));
		get_card_instance(player, card)->info_slot = 1<<clr;
	}

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			if( current_turn == player ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND,
										get_hacked_land_text(player, card, "Select a %s to sacrifice.", SUBTYPE_ISLAND));
			this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_ISLAND);
			this_test.qty = 2;

            int kill_me = 1;
			if( new_can_sacrifice(player, card, player, &this_test) ){
			    if( do_dialog(player, player, card, -1, -1, " Sac two Islands\n Destroy Psychic Allergy", 0) == 0 ){
				     new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &this_test);
				     kill_me = 0;
			    }
		    }
			if( kill_me ){
				kill_card(player, card, KILL_DESTROY);
			}
		}
		else{
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_PERMANENT);
			this_test.type_flag = F1_NO_TOKEN;
			this_test.color = get_card_instance(player, card)->info_slot;
			int amount = check_battlefield_for_special_card(player, card, current_turn, CBFSC_GET_COUNT, &this_test);
			damage_player(current_turn, amount, player, card);
		}
	}

	return global_enchantment(player, card, event);
}

int card_rag_man(int player, int card, event_t event){
	/*
	  Rag Man English |2|B|B
	  Creature - Human Minion 2/1
	  {B}{B}{B}, {T}: Target opponent reveals his or her hand and discards a creature card at random. Activate this ability only during your turn.
	*/
	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) && hand_count[instance->targets[0].player] > 0){
		reveal_target_player_hand(instance->targets[0].player);
		int opp_hand[hand_count[instance->targets[0].player]];
		int ohc = 0;
		int i;
		for(i=0; i<active_cards_count[instance->targets[0].player]; i++){
			if( in_hand(instance->targets[0].player, i) && is_what(instance->targets[0].player, i, TYPE_CREATURE) ){
				opp_hand[ohc] = i;
				ohc++;
			}
		}
		if( ohc ){
			int rnd = ohc > 1 ? internal_rand(ohc) : 0;
			new_discard_card(instance->targets[0].player, opp_hand[rnd], player, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_ONLY_TARGET_OPPONENT, MANACOST_B(3), 0, &td, "TARGET_PLAYER");
}

static const char* spell_is_targeting_me(int who_chooses, int player, int card)
{
	card_instance_t* eff = get_card_instance(player, card);
	if( eff->number_of_targets == 0 || eff->number_of_targets > 1)
		return "must have only 1 target";
	if( !(eff->targets[0].player == who_chooses && eff->targets[0].card == -1) )
		return "must target you.";
	if( ! call_card_function(player, card, EVENT_CAN_CHANGE_TARGET) )
		return "has only 1 legal target";

	return NULL;
}

int card_reflecting_mirror(int player, int card, event_t event){
/* CARD_ID_REFLECTING_MIRROR	354
Reflecting Mirror	|4
Artifact
|X, |T: Change the target of target spell with a single target if that target is you.
The new target must be a player. X is twice the converted mana cost of that spell. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td_spell;
	counterspell_target_definition(player, card, &td_spell, TYPE_SPELL);
	td_spell.extra = (int32_t)spell_is_targeting_me;
	td_spell.special |= TARGET_SPECIAL_EXTRA_FUNCTION;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( counterspell(player, card, EVENT_CAN_CAST, NULL, 0) ){
			int cmc = 2*get_cmc(card_on_stack_controller, card_on_stack);
			return generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK | GAA_UNTAPPED, MANACOST_X(cmc), 0, NULL, NULL);
		}
	}

	if(event == EVENT_ACTIVATE ){
		set_special_flags2(card_on_stack_controller, card_on_stack_controller, SF2_REFLECTING_MIRROR);
		call_card_function(card_on_stack_controller, card_on_stack_controller, EVENT_CHANGE_TARGET);
		if( spell_fizzled != 1 ){
			instance->targets[0] = get_card_instance(instance->targets[0].player, instance->targets[0].card)->targets[9];
			instance->number_of_targets = 1;
			get_card_instance(instance->targets[0].player, instance->targets[0].card)->targets[9].player = -1;
			get_card_instance(instance->targets[0].player, instance->targets[0].card)->targets[9].card = -1;
			get_card_instance(instance->targets[0].player, instance->targets[0].card)->number_of_targets = 1;
		}
		remove_special_flags2(card_on_stack_controller, card_on_stack_controller, SF2_REFLECTING_MIRROR);
	}

	if(event == EVENT_RESOLVE_ACTIVATION){
		if( counterspell_validate(player, card, &td_spell, 0) ){
			get_card_instance(instance->targets[0].player, instance->targets[0].card)->targets[0] = instance->targets[0];
		}
	}
	return 0;
}

int card_riptide2(int player, int card, event_t event){
	/*
	  Riptide |U
	  Instant
	  Tap all blue creatures.
	*/
	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color(player, card, COLOR_TEST_BLUE);
		new_manipulate_all(player, card, ANYBODY, &this_test, ACT_TAP);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static int effect_runesword(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( other_leaves_play(player, card, p, c, event) ){
			// Must check if the controller of Runeswoed didn't changed after the activation of its ability.
			if( instance->targets[0].player == instance->targets[1].player ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
			}
		}

		if( event == EVENT_POWER && affect_me(p, c) ){
			event_result += 2;
		}

		if (event == EVENT_DEAL_DAMAGE){
			card_instance_t* damage = get_card_instance(affected_card_controller, affected_card);
			if (damage->internal_card_id == damage_card && damage->info_slot > 0
				&& damage->damage_source_card == p
				&& damage->damage_source_player == c
				&& damage->damage_target_card != -1 )
			{
				if( instance->targets[2].player < 3 ){
					instance->targets[2].player = 3;
				}
				int pos = instance->targets[2].player;
				if( pos < 10 ){
					instance->targets[pos].player = damage->damage_target_player;
					instance->targets[pos].card = damage->damage_target_card;
					instance->targets[2].player++;
				}
			}
		}

		if( trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player &&
			instance->targets[2].player > 3 )
		{
			if (event == EVENT_TRIGGER){
				event_result = RESOLVE_TRIGGER_MANDATORY;
			}
			else if (event == EVENT_RESOLVE_TRIGGER){
				int i;
				for (i=3; i<instance->targets[2].player; i++){
					cannot_regenerate_until_eot(instance->targets[0].player, instance->targets[0].card,
												instance->targets[i].player, instance->targets[i].card);
					exile_if_would_be_put_into_graveyard(instance->targets[0].player, instance->targets[0].card,
														 instance->targets[i].player, instance->targets[i].card, 1);
				}
				instance->targets[2].player = 3;
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_runesword(int player, int card, event_t event){
/*
Runesword |6
Artifact
{3}, {T}: Target attacking creature gets +2/+0 until end of turn. When that creature leaves the battlefield this turn, sacrifice Runesword.
	If the creature deals damage to a creature this turn, the creature dealt damage can't be regenerated this turn.
	If a creature dealt damage by the targeted creature would die this turn, exile that creature instead.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int legacy_card = create_targetted_legacy_effect(player, instance->parent_card, &effect_runesword,
															instance->targets[0].player, instance->targets[0].card);
			card_instance_t *legacy_instance = get_card_instance(player, legacy_card);
			legacy_instance->targets[0].player = instance->parent_controller;
			legacy_instance->targets[0].card = instance->parent_card;
			legacy_instance->number_of_targets = 1;
			legacy_instance->targets[1] = legacy_instance->targets[0];
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_ATTACKING_CREATURE");
}

int card_safe_haven(int player, int card, event_t event){
/*
Safe Haven
Land
{2}, {T}: Exile target creature you control.
At the beginning of your upkeep, you may sacrifice Safe Haven. If you do, return each card exiled with Safe Haven to the battlefield
	under its owner's control.
*/
	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_LAND, -1)*12);
	}

	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) && current_turn == player &&
		! check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED) )
	{
		int mode = get_card_instance(player, card)->info_slot > 1 ? RESOLVE_TRIGGER_MANDATORY : 0;
		int trig_mode = IS_AI(player) ? mode : RESOLVE_TRIGGER_OPTIONAL;
		if( upkeep_trigger_mode(player, card, event, trig_mode) ){
			int result = exiledby_detach(player, card);
			kill_card(player, card, KILL_SACRIFICE);
			int leg = 0;
			int idx = 0;
			int* loc;

			while ((loc = exiledby_find_any(player-2, result, &leg, &idx)) != NULL){
					int owner = (*loc & 0x80000000) ? 1 : 0;
					int iid = *loc & ~0x80000000;
					*loc = -1;
					if( check_rfg(owner, cards_data[iid].id) ){
						int card_added = add_card_to_hand(owner, iid);
						remove_card_from_rfg(owner, cards_data[iid].id);
						put_into_play(owner, card_added);
					}
			}
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	if( player == AI ){
		td.required_state = TARGET_STATE_DESTROYED;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_CREATURE") ){
			if( player == HUMAN ){
				return 1;
			}
			else{
				if( land_can_be_played & LCBP_REGENERATION ){
					return 0x63;
				}
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_CREATURE");
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			if( player == AI ){
				regenerate_target(instance->targets[0].player, instance->targets[0].card);
			}
			create_card_name_legacy(instance->parent_controller, instance->parent_card,
									get_id(instance->targets[0].player, instance->targets[0].card));
			exile_card_and_remember_it_on_exiledby(instance->parent_controller, instance->parent_card,
													instance->targets[0].player, instance->targets[0].card);
			get_card_instance(instance->parent_controller, instance->parent_card)->info_slot++;
		}
	}
	return 0;
}

static const char* aura_enchanting_land(int who_chooses, int player, int card){
	card_instance_t *instance = get_card_instance(player, card);
	if( instance->damage_target_player > -1 ){
		if( is_what(instance->damage_target_player, instance->damage_target_card, TYPE_LAND) ){
			return NULL;
		}
	}

	return "must be an Aura enchanting a land.";
}

int card_savaen_elves(int player, int card, event_t event){
	/*
	  Savaen Elves |G
	  Creature - Elf 1/1
	  {G}{G}, {T}: Destroy target Aura attached to a land.
	*/
	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = (int32_t)aura_enchanting_land;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_G(2), 0,
									&td, "Select target Aura attached to a land.");
}

static int effect_scarecrow(int player, int card, event_t event){
	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *affected = get_card_instance(affected_card_controller, affected_card);
		if( affected->internal_card_id == damage_card ){
			if( (get_abilities(affected->damage_source_player, affected->damage_source_card, EVENT_ABILITIES, -1) & KEYWORD_FLYING) &&
				affected->damage_target_player == player && affected->damage_target_card == -1 && affected->info_slot > 0 )
			{
				prevent_damage(affected_card_controller, affected_card, -1);
			}
		}
	}
	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_SACRIFICE);
	}
	return 0;
}

int card_scarecrow(int player, int card, event_t event){
/* CARD_ID_SCARECROW	358
Scarecrow	|5
Artifact Creature - Scarecrow 2/2
|6, |T: Prevent all damage that would be dealt to you this turn by creatures with flying. */

	if( event == EVENT_ACTIVATE ){
		if( current_turn == 1-player ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.allowed_controller = td.preferred_controller = 1-player;
			td.required_abilities = KEYWORD_FLYING;
			td.illegal_abilities = 0;
			td.required_state = TARGET_STATE_ATTACKING;
			ai_modifier += (target_available(player, card, &td) * 12);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		create_legacy_effect(instance->parent_controller, instance->parent_card, &effect_scarecrow);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(6), 0, NULL, NULL);
}

int card_scarwood_bandits(int player, int card, event_t event){
/* CARD_ID_SCARWOOD_BANDITS	945
Scarwood Bandits	|2|G|G
Creature - Human Rogue 2/2
|H2Forestwalk (This creature can't be blocked as long as defending player controls |Ha Forest.)
|2|G, |T: Unless an opponent pays |2, gain control of target artifact for as long as ~ remains on the battlefield. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			if( in_play(instance->parent_controller, instance->parent_card) ){
				int steal = 1;
				if( has_mana(1-player, COLOR_ANY, 2) && charge_mana_while_resolving(player, card, event, 1-player, COLOR_ANY, 2) ){
					steal = 0;
				}
				if( steal == 1 ){
					gain_control_until_source_is_in_play_and_tapped(instance->parent_controller, instance->parent_card,
																	instance->targets[0].player, instance->targets[0].card,
																	GCUS_SOURCE_IN_PLAY);
				}
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, 2, 0, 0, 1, 0, 0, 0, &td, "TARGET_ARTIFACT");
}

/*
Scarwood Goblins |R|G --> vanilla
Creature - Goblin 2/2
*/

int card_scarwood_hag(int player, int card, event_t event){
/* CARD_ID_SCARWOOD_HAG	322
Scarwood Hag	|1|G
Creature - Hag 1/1
|G|G|G|G, |T: Target creature gains |H2forestwalk until end of turn. (It can't be blocked as long as defending player controls |Ha Forest.)
|T: Target creature loses |H2forestwalk until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_GRANT_WALK =1,
		CHOICE_REMOVE_WALK
	};

	if( event == EVENT_CAN_ACTIVATE ){
		td.preferred_controller = ANYBODY;
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST0, 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, has_mana_for_activated_ability(player, card, MANACOST_G(4)), 1};
		char msg[100];
		char msg2[100];
		scnprintf(msg, 100, "%s", get_hacked_land_text(player, card, "Grant %swalk", SUBTYPE_FOREST));
		scnprintf(msg2, 100, "%s", get_hacked_land_text(player, card, "Remove %lwalk", SUBTYPE_FOREST));
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							msg, abils[1], current_turn == player ? 10 : 1,
							msg2, abils[2], current_turn == 1-player ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_GRANT_WALK ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_G(4), 0, &td, "TARGET_CREATURE");
		}
		if( choice == CHOICE_REMOVE_WALK ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST0, 0, &td2, "TARGET_CREATURE");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int walk = get_hacked_walk(player, card, KEYWORD_FORESTWALK);
		int p = instance->parent_controller, c = instance->parent_card;
		if( instance->info_slot == CHOICE_GRANT_WALK && valid_target(&td) ){
			alternate_legacy_text(1, player, pump_ability_until_eot(p, c, instance->targets[0].player, instance->targets[0].card, 0, 0, walk, 0));
			// pump_ability_until_eot() will hack_walk it
		}
		if( instance->info_slot == CHOICE_REMOVE_WALK && valid_target(&td2) ){
			alternate_legacy_text(2, player, negate_ability_until_eot(p, c, instance->targets[0].player, instance->targets[0].card, walk));
			// pump_ability_until_eot() will hack_walk it
		}
	}

	return 0;
}

int card_scavenger_folk(int player, int card, event_t event)
{
	/*
	  Scavenger Folk |G
	  Creature - Human 1/1
	  {G}, {T}, Sacrifice Scavenger Folk: Destroy target artifact.
	*/

	if (!IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_G(1), 0,
									&td, "TARGET_ARTIFACT");
}

int card_season_of_the_witch(int player, int card, event_t event){
/* CARD_ID_SEASON_OF_THE_WITCH	359
Season of the Witch	|B|B|B
Enchantment
At the beginning of your upkeep, sacrifice ~ unless you pay 2 life.
At the beginning of the end step, destroy all untapped creatures that didn't attack this turn, except for creatures that couldn't attack. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int kill = 1;
		if( can_pay_life(player, 2) ){
			int choice = do_dialog(player, player, card, -1, -1, " Pay Upkeep\n Pass", life[player]-2 < 6);
			if( choice == 0 ){
				lose_life(player, 2);
				kill = 0;
			}
		}

		if( kill == 1 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( beginning_of_combat(player, card, event, ANYBODY, -1) ){
		card_instance_t *instance = get_card_instance(player, card);
		instance->info_slot = 0;
		int count = active_cards_count[current_turn]-1;
		while( count > -1 ){
				if( in_play(current_turn, count) && is_what(current_turn, count, TYPE_CREATURE) && can_attack(current_turn, count) ){
					while( instance->info_slot < 19 ){
							if( instance->targets[instance->info_slot].player == -1 ){
								instance->targets[instance->info_slot].player = count;
								break;
							}
							if( instance->targets[instance->info_slot].card == -1 ){
								instance->targets[instance->info_slot].card = count;
								break;
							}
							instance->info_slot++;
					}
				}
				count--;
		}
	}

	if( eot_trigger(player, card, event) ){
		card_instance_t *instance = get_card_instance(player, card);
		int i = 0;
		while( i < 19 ){
				if( instance->targets[i].player != -1 ){
					if( ! check_state(current_turn, instance->targets[i].player, STATE_ATTACKED)){
						kill_card(current_turn, instance->targets[i].player, KILL_DESTROY);
					}
					instance->targets[i].player = -1;
				}
				else{
					break;
				}
				if( instance->targets[i].card != -1 ){
					if( ! check_state(current_turn, instance->targets[i].card, STATE_ATTACKED)){
						kill_card(current_turn, instance->targets[i].card, KILL_DESTROY);
					}
					instance->targets[i].card = -1;
				}
				else{
					break;
				}
				i++;
		}
		instance->info_slot = 0;
	}
	return global_enchantment(player, card, event);
}

/* Sisters of the Flame	|1|R|R	=>battle_for_zendikar.c:card_generic_combat_1_mana_producing_creature
 * Creature - Human Shaman 2/2
 * |T: Add |R to your mana pool. */

int card_skull_of_orm(int player, int card, event_t event){
/* CARD_ID_SKULL_OF_ORM	361
Skull of Orm	|3
Artifact
|5, |T: Return target enchantment card from your graveyard to your hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ENCHANTMENT, "Select an enchantment card.");

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST_X(5), 0, NULL, NULL,
															0, &this_test, GS_GRAVE_RECYCLER);
}

/*
Sorrow's Path --> Uber complex + junk. No, thanx.
Land
{T}: Choose two target blocking creatures an opponent controls.
If each of those creatures could block all creatures that the other is blocking, remove both of them from combat.
Each one then blocks all creatures the other was blocking.
Whenever Sorrow's Path becomes tapped, it deals 2 damage to you and each creature you control.
*/

static void give_first_strike(int player, int card, int t_player, int t_card)
{
	pump_ability_until_eot(player, card, t_player, t_card, 0, 0, KEYWORD_FIRST_STRIKE, 0);
}

int card_spitting_slug(int player, int card, event_t event){
/*
Spitting Slug |1|G|G
Creature — Slug 2/4
Whenever Spitting Slug blocks or becomes blocked, you may pay {1}{G}. If you do, Spitting Slug gains first strike until end of turn.
	Otherwise, each creature blocking or blocked by Spitting Slug gains first strike until end of turn.
*/
	if( event == EVENT_DECLARE_BLOCKERS && ! is_humiliated(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		int pump_others = 0;
		if( (current_turn != player && (instance->state & STATE_ATTACKING)) ||
			(current_turn == player && is_attacking(player, card) && ! is_unblocked(player, card)) )
		{
			if( has_mana_multi(player, MANACOST_XG(1, 1)) ){
				charge_mana_multi(player, MANACOST_XG(1, 1));
				if( spell_fizzled != 1 ){
					pump_ability_until_eot(player, card, player, card, 0, 0, KEYWORD_FIRST_STRIKE, 0);
					pump_others = 0;
				}
			}
		}
		if( pump_others ){
			if (player == current_turn && (instance->state & STATE_ATTACKING) ){
				for_each_creature_blocking_me(player, card, give_first_strike, player, card);
			}
			if (player == 1-current_turn && (instance->state & STATE_BLOCKING)){
				for_each_creature_blocked_by_me(player, card, give_first_strike, player, card);
			}
		}
	}

	return 0;
}

/*
Squire English |1|W --> VANILLAAAAAA !
Creature - Human Soldier 1/2
*/

int card_standing_stones(int player, int card, event_t event){
/* CARD_ID_STANDING_STONES	362
Standing Stones	|3
Artifact
|1, |T, Pay 1 life: Add one mana of any color to your mana pool. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( has_mana(player, COLOR_ANY, 1) && !is_tapped(player, card) && !is_animated_and_sick(player, card) &&
			can_produce_mana(player, card) && can_pay_life(player, 1) )
		{
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		ai_modifier -= 36;

		card_instance_t* instance = get_card_instance(player, card);
		instance->state |= STATE_TAPPED;

		charge_mana(player, COLOR_COLORLESS, 1);
		if( spell_fizzled == 1 ){
			instance->state &= ~STATE_TAPPED;
			return 0;
		}
		produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, 1);
		if( spell_fizzled == 1 ){
			instance->state &= ~STATE_TAPPED;
			return 0;
		}
		lose_life(player, 1);
	}

	return 0;
}

int card_stone_calendar(int player, int card, event_t event){
	/*
	  Stone Calendar |5
	  Artifact
	  Spells you cast cost up to {1} less to cast.
	*/
	if( ! is_humiliated(player, card) && event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player ){
		COST_COLORLESS--;
	}

	return 0;
}

int card_sunken_city(int player, int card, event_t event){
	/*
	  Sunken City |U|U
	  Enchantment
	  At the beginning of your upkeep, sacrifice Sunken City unless you pay {U}{U}.
	  Blue creatures get +1/+1.
	*/
	basic_upkeep(player, card, event, MANACOST_U(2));

	boost_creature_by_color(player, card, event, get_sleighted_color_test(player, card, COLOR_TEST_BLUE), 1, 1, 0, 0);

	return global_enchantment(player, card, event);
}


int card_tangle_kelp(int player, int card, event_t event)
{
  // 0x40f3f0
/* CARD_ID_TANGLE_KELP	949
Tangle Kelp	|U
Enchantment - Aura
Enchant creature
When ~ enters the battlefield, tap enchanted creature.
Enchanted creature doesn't untap during its controller's untap step if it attacked during its controller's last turn. */


	if( event == EVENT_CLEANUP && affect_me(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		if (instance->damage_target_player == current_turn)
		{
			card_instance_t* ench_inst = get_card_instance(instance->damage_target_player, instance->damage_target_card);
			instance->info_slot = ench_inst->state & STATE_ATTACKED;
		}
	}

	if (comes_into_play(player, card, event) ){
		card_instance_t* instance = get_card_instance(player, card);
		tap_card(instance->damage_target_player, instance->damage_target_card);
	}

	if( get_card_instance(player, card)->info_slot && ! is_humiliated(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);
	}

	return disabling_aura(player, card, event);
}

int card_the_fallen(int player, int card, event_t event){
	// !!!DOM: each opponent it has dealt damage to this game=>each opponent or planeswalker it has dealt damage to this game
	/* The Fallen	|1|B|B|B	0x2002073
	 * Creature - Zombie 2/3
	 * At the beginning of your upkeep, ~ deals 1 damage to each opponent and planeswalker it has dealt damage to this game. */
	if( event == EVENT_DEAL_DAMAGE ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! BYTE0(instance->info_slot) && ! BYTE1(instance->info_slot) ){
			check_damage_test(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS, player, card, 0, NULL);
		}
	}

	if( resolve_damage_trigger(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS, player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int result = instance->targets[1].player;
		instance->targets[1].player = 0;
		if( BYTE0(result) ){
			SET_BYTE0(instance->info_slot) = 1;
		}
		if( BYTE1(result) ){
			SET_BYTE1(instance->info_slot) = 1;
		}
	}

	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( BYTE0(instance->info_slot) || BYTE1(instance->info_slot) ){
			upkeep_trigger_ability(player, card, event, player);
		}
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( BYTE0(instance->info_slot) ){
			damage_player(0, 1, player, card);
		}
		if( BYTE1(instance->info_slot) ){
			damage_player(1, 1, player, card);
		}
	}


	return 0;
}

int card_tivadars_crusade(int player, int card, event_t event){
	/*
	  Tivadar's Crusade |1|W|W
	  Sorcery
	  Destroy all Goblins.
	*/
	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_GOBLIN;
		APNAP(p, { new_manipulate_all(player, card, p, &this_test, KILL_DESTROY); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_tormods_crypt(int player, int card, event_t event)
{
  // 0x40c080

  /* Tormod's Crypt	|0
   * Artifact
   * |T, Sacrifice ~: Exile all cards from target player's graveyard. */

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;

  card_instance_t* instance = get_card_instance(player, card);

  int rval = generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_UNTAPPED|GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_PLAYER");
  if (event == EVENT_ACTIVATE && IS_AI(player))
	{
	  int tgt = instance->targets[0].player;
	  int mod = 2 * count_graveyard_by_type(tgt, TYPE_ARTIFACT | TYPE_SORCERY | TYPE_CREATURE) + count_graveyard(tgt) - 30;
	  if (tgt == player)
		mod -= 128;

	  if (player == 1)
		ai_modifier += mod;
	  else
		ai_modifier -= mod;
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	rfg_whole_graveyard(instance->targets[0].player);

  return rval;
}

static int effect_tower_of_coireall(int player, int card, event_t event){

	if( event == EVENT_BLOCK_LEGALITY ){
		if( get_card_instance(player, card)->damage_target_player > -1 ){
			card_instance_t *instance = get_card_instance(player, card);
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			if( p == attacking_card_controller && c == attacking_card ){
				if( has_creature_type(affected_card_controller, affected_card, SUBTYPE_WALL) ){
					event_result = 1;
				}
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_tower_of_coireall(int player, int card, event_t event){
/* CARD_ID_TOWER_OF_COIREALL	379
Tower of Coireall	|2
Artifact
|T: Target creature can't be blocked by Walls this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &effect_tower_of_coireall, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_tracker2(int player, int card, event_t event){
	/*
	  Tracker |2|G
	  Creature - Human 2/2
	  {G}{G}, {T}: Tracker deals damage equal to its power to target creature. That creature deals damage equal to its power to Tracker.
	*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			// Not a fight.  Stop replacing this with fight().
			get_card_instance(instance->parent_controller, instance->parent_card)->regen_status |= KEYWORD_RECALC_POWER;
			int mypow = get_power(instance->parent_controller, instance->parent_card);
			damage_creature(instance->targets[0].player, instance->targets[0].card, mypow, instance->parent_controller, instance->parent_card);
			if (in_play(instance->parent_controller, instance->parent_card)){
				get_card_instance(instance->targets[0].player, instance->targets[0].card)->regen_status |= KEYWORD_RECALC_POWER;
				int hispow = get_power(instance->targets[0].player, instance->targets[0].card);
				damage_creature(instance->parent_controller, instance->parent_card, hispow, instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_G(2), 0, &td, "TARGET_CREATURE");
}

int card_uncle_istvan(int player, int card, event_t event){
	/*
	  Uncle Istvan |1|B|B|B
	  Creature - Human 1/3
	  Prevent all damage that would be dealt to Uncle Istvan by creatures.
	*/
	if( ! is_humiliated(player, card) && event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->damage_target_player == player &&
			damage->damage_target_card == card && damage->info_slot > 0 && (damage->targets[3].player & TYPE_CREATURE) )
		{
			prevent_damage(affected_card_controller, affected_card, -1);
		}
	}

	return 0;
}

static void destroy_nonwall_at_end_of_combat(int player, int card, int t_player, int t_card)
{
  if (!has_subtype(t_player, t_card, SUBTYPE_WALL))
	create_legacy_effect_exe(player, card, LEGACY_EFFECT_STONING, t_player, t_card);
}

int card_venom(int player, int card, event_t event)
{
/* CARD_ID_VENOM	383
Venom	|1|G|G
Enchantment - Aura
Enchant creature
Whenever enchanted creature blocks or becomes blocked by a non-Wall creature, destroy the other creature at end of combat. */

  // 0x4ca760
  card_instance_t* instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0)
	{
	  int p = instance->damage_target_player, c = instance->damage_target_card;

	  if (event == EVENT_CHANGE_TYPE && affect_me(p, c) && !is_humiliated(player, card))	// only if the aura loses abilities, not if the enchanted creature does
		get_card_instance(p, c)->destroys_if_blocked |= DIFB_DESTROYS_NONWALLS;

	  if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card))
		{
		  if (p == current_turn && is_attacking(p, c) )
			for_each_creature_blocking_me(p, c, destroy_nonwall_at_end_of_combat, player, card);

		  if (p == 1-current_turn &&  blocking(p, c, event) )
			for_each_creature_blocked_by_me(p, c, destroy_nonwall_at_end_of_combat, player, card);
		}
	}

  return vanilla_aura(player, card, event, player);
}

int card_wand_of_ith(int player, int card, event_t event)
{
 /* Wand of Ith	|4
  * Artifact
  * |3, |T: Target player reveals a card at random from his or her hand. If it's a land card, that player discards it unless he or she
  * 	pays 1 life. If it isn't a land card, the player discards it unless he or she pays life equal to its converted mana cost.
  *		Activate this ability only during your turn. */

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int p = instance->targets[0].player;

	  if (hand_count[p] <= 0)
		{
		  if (player == AI)
			ai_modifier -= 48;
		  return 0;
		}

	  int c = get_random_card_in_hand(p);

	  int cost = is_what(p, c, TYPE_LAND) ? 1 : get_cmc(p, c);

	  char pay_x_life[100];
	  if (ai_is_speculating == 1)
		pay_x_life[0] = 0;
	  else
		{
		  load_text(0, "WANDOFITH");
		  scnprintf(pay_x_life, 100, text_lines[1], cost);	// "Pay %d life."
		}

	  int cpl = can_pay_life(p, cost);

	  // DLG_RANDOM_NO_SAVE because of the random card selection above
	  if (DIALOG(player, card, event,
				 DLG_RANDOM_NO_SAVE, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_WHO_CHOOSES(p), DLG_SMALLCARD(p, c),
				 pay_x_life, cpl, life[p] - cost,
				 text_lines[2], 1, 5) == 1)
		lose_life(p, cost);
	  else
		new_discard_card(p, c, player, 0);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_IN_YOUR_TURN, MANACOST_X(3), 0,
									&td, "TARGET_PLAYER");
}

int war_barge_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		int wb_p = instance->targets[0].player;
		int wb_c = instance->targets[0].card;

		if( other_leaves_play(player, card, wb_p, wb_c, event) ){
			kill_card(p, c, KILL_BURY);
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_war_barge(int player, int card, event_t event){
	/*
	  War Barge |4
	  Artifact
	  {3}: Target creature gains islandwalk until end of turn.
	  When War Barge leaves the battlefield this turn, destroy that creature. A creature destroyed this way can't be regenerated.
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
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card,
									0, 0, get_hacked_walk(player, card, KEYWORD_ISLANDWALK), 0);
			int legacy = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &war_barge_legacy,
											instance->targets[0].player, instance->targets[0].card);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[0].player = instance->parent_controller;
			leg->targets[0].card = instance->parent_card;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_CREATURE");
}

int card_water_wurm(int player, int card, event_t event){
	/*
	  Water Wurm |U
	  Creature - Wurm 1/1
	  Water Wurm gets +0/+1 as long as an opponent controls an Island.
	*/
	if( ! is_humiliated(player, card) && event == EVENT_TOUGHNESS && affect_me(player, card) ){
		if( check_battlefield_for_subtype(1-player, TYPE_LAND, SUBTYPE_ISLAND) ){
			event_result++;
		}
	}

	return 0;
}

static int whippoorwill_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;
		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *source = get_card_instance(affected_card_controller, affected_card);
			if( damage_card == source->internal_card_id &&
				source->damage_target_player == t_player && source->damage_target_card == t_card )
			{
				state_untargettable(affected_card_controller, affected_card, 1);
			}
		}
		if( event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(t_player, t_card) ){
			card_instance_t *this = get_card_instance( t_player, t_card );
			if( this->kill_code > 0 && this->kill_code < KILL_EXILE ){
				this->kill_code = KILL_EXILE;
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		card_instance_t *instance = get_card_instance(player, card);
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;
		remove_status(t_player, t_card, STATUS_CANNOT_REGENERATE);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_whippoorwill(int player, int card, event_t event){
	/* Whippoorwill	|G	0x20020aa
	 * Creature - Bird 1/1
	 * |G|G, |T: Target creature can't be regenerated this turn. Damage that would be dealt to that creature this turn can't be prevented or dealt instead to another permanent or player. When the creature dies this turn, exile the creature. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			add_status(instance->targets[0].player, instance->targets[0].card, STATUS_CANNOT_REGENERATE);
			int legacy = create_legacy_effect(player, card, &whippoorwill_legacy);
			get_card_instance(player, legacy)->targets[0] = instance->targets[0];
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_G(2), 0, &td, "TARGET_CREATURE");
}

static const char* dangerous_attacking_creature(int who_chooses, int player, int card){

	if ( is_attacking(player, card) && is_unblocked(player, card) && life[who_chooses]-get_power(player, card) < 6 ){
		return NULL;
	}

	return "AI helper function";
}

int card_witch_hunter(int player, int card, event_t event){
	/* Witch Hunter	|2|W|W	0x200c81b
	 * Creature - Human Cleric 1/1
	 * |T: ~ deals 1 damage to target player or planeswalker.
	 * |1|W|W, |T: Return target creature an opponent controls to its owner's hand. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller = 1-player;
	if( player == AI ){
		td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		td1.extra = (int32_t)dangerous_attacking_creature;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER_OR_PLANESWALKER") ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XW(1, 2), 0, &td1, "TARGET_CREATURE") ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER_OR_PLANESWALKER") ){
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XW(1, 2), 0, &td1, "TARGET_CREATURE") ){
				int ai_choice = (life[1-player] == 1 || current_turn == player) ? 0 : 1;
				choice = do_dialog(player, player, card, -1, -1, " Damage player\n Bounce creature\n Cancel", ai_choice);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 2 ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == 0 ){
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER_OR_PLANESWALKER");
			if( spell_fizzled != 1 ){
				instance->info_slot = 66;
			}
		}
		if( choice == 1 ){
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XW(1, 2), 0,
								&td1, "Select target creature an opponent controls.");
			if( spell_fizzled != 1 ){
				instance->info_slot = 67;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( instance->info_slot == 66 && valid_target(&td) ){
			damage_target0(player, card, 1);
		}
		if( instance->info_slot == 67 && valid_target(&td1) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}
	return 0;
}

int card_word_of_binding(int player, int card, event_t event){
/* CARD_ID_WORD_OF_BINDING	388
Word of Binding	|X|B|B
Sorcery
Tap X target creatures. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int i, num[2] = {0};
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				tap_card(instance->targets[i].player, instance->targets[i].card);
				++num[instance->targets[i].player];
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return x_spell_with_x_targets(player, card, event, &td, "TARGET_CREATURE", 1);
}

int card_worms_of_the_earth(int player, int card, event_t event){
	// new card
	/*
	  Worms of the Earth |2|B|B|B
	  Enchantment
	  Players can't play lands.
	  Lands can't enter the battlefield.
	  At the beginning of each upkeep, any player may sacrifice two lands or have Worms of the Earth deal 5 damage to him or her. If a player does either, destroy Worms of the Earth.
	*/
	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_NO_STORAGE, DLG_WHO_CHOOSES(current_turn),
						"Sac two lands", count_subtype(current_turn, TYPE_LAND, -1), count_subtype(current_turn, TYPE_LAND, -1)*5,
						"Take 5 damage", 1, life[current_turn]*2);
		if( choice == 1 ){
			if( sacrifice(player, card, current_turn, 0, TYPE_LAND, 0, 0, 0, 0, 0, 0, 0, -1, 0) ){
				impose_sacrifice(player, card, current_turn, 1, TYPE_LAND, 0, 0, 0, 0, 0, 0, 0, -1, 0);
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
		if( choice == 2 ){
			damage_player(current_turn, 5, player, card);
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( in_play(player, card) ){
		player_bits[player] |= PB_COUNT_TOTAL_PLAYABLE_LANDS;
		player_bits[1-player] |= PB_COUNT_TOTAL_PLAYABLE_LANDS;

		if( !(land_can_be_played & LCBP_LAND_HAS_BEEN_PLAYED) ){
			land_can_be_played |= LCBP_LAND_HAS_BEEN_PLAYED;
		}
	}

	if( leaves_play(player, card, event) && lands_played < total_playable_lands(current_turn) ){
		land_can_be_played &= ~LCBP_LAND_HAS_BEEN_PLAYED;
		player_bits[player] &= ~PB_COUNT_TOTAL_PLAYABLE_LANDS;
		player_bits[1-player] &= ~PB_COUNT_TOTAL_PLAYABLE_LANDS;
	}

	return global_enchantment(player, card, event);
}

int card_wormwood_treefolk(int player, int card, event_t event){
	/*
	  Wormwood Treefolk English |3|G|G
	  Creature - Treefolk 4/4
	  {G}{G}: Wormwood Treefolk gains forestwalk until end of turn and deals 2 damage to you.
	  {B}{B}: Wormwood Treefolk gains swampwalk until end of turn and deals 2 damage to you.
	*/


	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			return has_mana_for_activated_ability(player, card, MANACOST_B(2)) || has_mana_for_activated_ability(player, card, MANACOST_G(2)) ;
		}
	}

	if( event == EVENT_ACTIVATE ){
		card_instance_t *instance = get_card_instance(player, card);
    int choice = 0;
    int has_BB = has_mana_for_activated_ability(player, card, MANACOST_B(2));
    int has_GG = has_mana_for_activated_ability(player, card, MANACOST_G(2));
    if (has_BB && has_GG)
      choice = do_dialog(player, player,card, -1,-1, " Forestwalk\n Swampwalk\n Cancel",
                         check_battlefield_for_subtype(1-player, TYPE_LAND, SUBTYPE_SWAMP));
    else if (has_GG)
      choice = 0;
    else if (has_BB)
      choice = 1;
    else
      choice = 2;
		if( choice == 2 ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, 0, (choice == 1 ? 2 : 0), 0, (choice == 0 ? 2 : 0), 0, 0) ){
			instance->info_slot = 66+choice;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->parent_controller, c = instance->parent_card;
		if(instance->info_slot == 66){
			alternate_legacy_text(1, p, pump_ability_until_eot(p, c, p, c, 0, 0, get_hacked_walk(p, c, KEYWORD_FORESTWALK), 0));
		}
		if(instance->info_slot == 67){
			alternate_legacy_text(2, p, pump_ability_until_eot(p, c, p, c, 0, 0, get_hacked_walk(p, c, KEYWORD_SWAMPWALK), 0));
		}
		damage_player(instance->parent_controller, 2, instance->parent_controller, instance->parent_card);
	}

	return 0;
}

