#include "manalink.h"

// Functions

// Cards
int card_acolythe_of_xathrid(int player, int card, event_t event){
/* CARD_ID_ACOLYTE_OF_XATHRID	10380
Acolyte of Xathrid	|B
Creature - Human Cleric 0/1
|1|B, |T: Target player loses 1 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XB(1, 1), 0, &td, "TARGET_PLAYER");
}

int card_act_of_treason(int player, int card, event_t event){
/* CARD_ID_ACT_OF_TREASON	10381
Act of Treason	|2|R
Sorcery
Gain control of target creature until end of turn. Untap that creature. It gains haste until end of turn. (It can attack and |T this turn.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier_for_act_of_treason(player, card);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			effect_act_of_treason( player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

static int effect_must_attack(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_card > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		attack_if_able(instance->damage_target_player, instance->damage_target_card, event);
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_alluring_siren(int player, int card, event_t event){
/* CARD_ID_ALLURING_SIREN	10382
Alluring Siren	|1|U
Creature - Siren 1/1
|T: Target creature an opponent controls attacks you this turn if able. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.allowed_controller = opp;
	td.preferred_controller = opp;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		if( player == AI && current_turn == player ){
			ai_modifier-=100;
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &effect_must_attack,
											instance->targets[0].player, instance->targets[0].card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target creature your opponent controls.");
}

int card_ant_queen(int player, int card, event_t event){
	/* Ant Queen	|3|G|G
	 * Creature - Insect 5/5
	 * |1|G: Put a 1/1 |Sgreen Insect creature token onto the battlefield. */

	if(event == EVENT_RESOLVE_ACTIVATION ) {
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_INSECT, &token);
		token.pow = token.tou = 1;
		token.color_forced = COLOR_TEST_GREEN;
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XG(1, 1), 0, NULL, NULL);
}

int card_awakener_druid(int player, int card, event_t event){
/* CARD_ID_AWAKENER_DRUID	10385
Awakener Druid	|2|G
Creature - Human Druid 1/1
When ~ enters the battlefield, target |H2Forest becomes a 4/5 |Sgreen Treefolk creature for as long as ~ remains on the battlefield. It's still a land.*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.required_subtype = SUBTYPE_FOREST;
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_LAND", 0, 0) ){
			add_a_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_TREEFOLK);
			land_animation2(player, card, instance->targets[0].player, instance->targets[0].card, 2, 4, 5, 0, 0, 0, 0);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_baneslayer_angel(int player, int card, event_t event){
/* CARD_ID_BANESLAYER_ANGEL	10386
Baneslayer Angel	|3|W|W
Creature - Angel 5/5
Flying, first strike, lifelink, protection from Demons and from Dragons */

	lifelink(player, card, event);
	protection_from_subtype(player, card, event, SUBTYPE_DEMON);
	protection_from_subtype(player, card, event, SUBTYPE_DRAGON);
	return 0;
}

int card_berserkers_of_blood_ridge(int player, int card, event_t event){
/* CARD_ID_BERSERKERS_OF_BLOOD_RIDGE	10387
Berserkers of Blood Ridge	|4|R
Creature - Human Berserker 4/4
~ attacks each turn if able. */

	attack_if_able(player, card, event);
	return 0;
}

int card_bountiful_harvest(int player, int card, event_t event){
/* CARD_ID_BOUNTIFUL_HARVEST	10390
Bountiful Harvest	|4|G
Sorcery
You gain 1 life for each land you control. */

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, count_subtype(player, TYPE_LAND, -1));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}


int card_burning_inquiry(int player, int card, event_t event){
	/*
	  Burning Inquiry |R
	  Sorcery
	  Each player draws three cards, then discards three cards at random.
	*/

	if(event == EVENT_RESOLVE_SPELL ){
		APNAP(p, {
					draw_cards(p, 3);
					new_multidiscard(p, 3, DISC_RANDOM, player);
				};
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_burst_of_speed(int player, int card, event_t event){
/* CARD_ID_BURST_OF_SPEED	10393
Burst of Speed	|R
Sorcery
Creatures you control gain haste until end of turn. (They can attack and |T even if they just came under your control.) */

	if(event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 0, 0, 0, SP_KEYWORD_HASTE);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_bramble_creeper(int player, int card, event_t event)
{
	/* Bramble Creeper	|4|G	0x200ec83
	 * Creature - Elemental 0/3
	 * Whenever ~ attacks, it gets +5/+0 until end of turn. */

	return when_attacks_pump_self(player, card, event, 5,0);
}

int card_capricious_efreet(int player, int card, event_t event){
/*
Capricious Efreet |4|R|R
Creature — Efreet 6/4
At the beginning of your upkeep, choose target nonland permanent you control and up to two target nonland permanents you don't control.
Destroy one of them at random.
*/
	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_LAND;
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.allow_cancel = 0;

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_PERMANENT);
		td1.illegal_type = TYPE_LAND;
		td1.allowed_controller = 1-player;
		td1.preferred_controller = 1-player;

		card_instance_t *instance= get_card_instance(player, card);

		int chosen = 0;
		if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
			chosen++;
		}
		if( can_target(&td1) ){
			int max = 0;
			while( max < 2 ){
					if( new_pick_target(&td1, "TARGET_PERMANENT", max+chosen, 0) ){
						state_untargettable(instance->targets[max+chosen].player, instance->targets[max+chosen].card, 1);
						max++;
					}
					else{
						break;
					}
			}
			chosen+=max;
		}
		if( chosen > 0 ){
			int rnd = 0;
			if( chosen > 1 ){
				rnd = internal_rand(chosen);
			}
			kill_card(instance->targets[rnd].player, instance->targets[rnd].card, KILL_DESTROY);
		}
		remove_state(1-player, -1, STATE_CANNOT_TARGET);
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_captain_of_the_watch( int player, int card, event_t event){
	/* Captain of the Watch	|4|W|W
	 * Creature - Human Soldier 3/3
	 * Vigilance
	 * Other Soldier creatures you control get +1/+1 and have vigilance.
	 * When ~ enters the battlefield, put three 1/1 |Swhite Soldier creature tokens onto the battlefield. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SOLDIER, &token);
		token.pow = token.tou = 1;
		token.color_forced = COLOR_TEST_WHITE;
		token.qty = 3;
		generate_token(&token);
	}

	vigilance(player, card, event);

	if(	in_play(player, card) && (event == EVENT_ABILITIES || event == EVENT_POWER || event == EVENT_TOUGHNESS) &&
		affected_card_controller == player && has_creature_type(player, affected_card, SUBTYPE_SOLDIER) &&
		! affect_me(player, card)
	  ){
		switch( event ){
				case EVENT_POWER:
				case EVENT_TOUGHNESS:
					event_result++;
					break;
				case EVENT_ABILITIES:
					vigilance(player, affected_card, event);
					break;
				default:
					break;
		}
	}

	return 0;
}

int card_cemetery_reaper(int player, int card, event_t event){
	/* Cemetery Reaper	|1|B|B
	 * Creature - Zombie 2/2
	 * Other Zombie creatures you control get +1/+1.
	 * |2|B, |T: Exile target creature card from a graveyard. Put a 2/2 |Sblack Zombie creature token onto the battlefield. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XB(2, 1), 0, NULL, NULL) ){
			if( (count_graveyard_by_type(AI, TYPE_CREATURE) > 0 && ! graveyard_has_shroud(AI)) ||
				(count_graveyard_by_type(HUMAN, TYPE_CREATURE) > 0 && ! graveyard_has_shroud(HUMAN))
			  ){
				return 1;
			}
		}
	}

	if(event == EVENT_ACTIVATE){
		card_instance_t *instance = get_card_instance(player, card);
		if( charge_mana_for_activated_ability(player, card, MANACOST_XB(2, 1)) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card to exile.");
			if( select_target_from_either_grave(player, card, 0, AI_MIN_VALUE, AI_MAX_VALUE, &this_test, 0, 1) != -1 ){
				int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
				if( selected != -1 ){
					rfg_card_from_grave(instance->targets[0].player, selected);
					add_state(player, card, STATE_TAPPED);
				}
				else{
					spell_fizzled = 1;
				}
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t *instance = get_card_instance(player, card);
		token_generation_t token;
		default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_ZOMBIE, &token);
		token.pow = token.tou = 2;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		generate_token(&token);
	}

	return boost_creature_type(player, card, event, SUBTYPE_ZOMBIE, 1, 1, 0, BCT_CONTROLLER_ONLY);
}

int card_child_of_night(int player, int card, event_t event){
	/* Ajani's Sunstriker	|W|W
	 * Creature - Cat Cleric 2/2
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */
	/* Archangel of Thune	|3|W|W
	 * Creature - Angel 3/4
	 * Flying
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.)
	 * Whenever you gain life, put a +1/+1 counter on each creature you control. */
	/* Bishop's Soldier	|1|W
	 * Creature - Vampire Soldier 2/2
	 * Lifelink */
	/* Caravan Hurda	|4|W
	 * Creature - Giant 1/5
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */
	/* Champion of Arashin	|3|W
	 * Creature - Hound Warrior 3/2
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */
	/* Chancellor of the Dross	|4|B|B|B
	 * Creature - Vampire 6/6
	 * You may reveal this card from your opening hand. If you do, at the beginning of the first upkeep, each opponent loses 3 life, then you gain life equal to the life lost this way.
	 * Flying, lifelink */
	/* Child of Night	|1|B
	 * Creature - Vampire 2/1
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */
	/* Daggerdrome Imp	|1|B
	 * Creature - Imp 1/1
	 * Flying
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */
	/* Faithbearer Paladin	|4|W
	 * Creature - Human Knight 3/4
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */
	/* Flensermite	|1|B
	 * Creature - Gremlin 1/1
	 * Infect (This creature deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.)
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */
	/* Insatiable Harpy	|2|B|B
	 * Creature - Harpy 2/2
	 * Flying, lifelink */
	/* It That Rides as One	(Transforms from Lone Rider)
	 * Creature - Eldrazi Horror 4/4
	 * First strike, trample, lifelink */
	/* Knight of Meadowgrain	|W|W
	 * Creature - Kithkin Knight 2/2
	 * First strike
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */
	/* Markov Patrician	|2|B
	 * Creature - Vampire 3/1
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */
	/* Rhox Faithmender	|3|W
	 * Creature - Rhino Monk 1/5
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.)
	 * If you would gain life, you gain twice that much life instead. */
	/* Rhox War Monk	|G|W|U
	 * Creature - Rhino Monk 3/4
	 * Lifelink */
	/* Scrounger of Souls	|4|B
	 * Creature - Horror 3/4
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */
	/* Seraph of Dawn	|2|W|W
	 * Creature - Angel 2/4
	 * Flying
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */
	/* Sungrace Pegasus	|1|W
	 * Creature - Pegasus 1/2
	 * Flying (This creature can't be blocked except by creatures with flying or reach.)
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */
	/* Trained Caracal	|W
	 * Creature - Cat 1/1
	 * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */

	/* Archangel of Thune's and Rhox Faithminder's last abilities and Chancellor of the Dross's first ability handled
	 * elsewhere. */
	lifelink(player, card, event);

	return 0;
}

int card_deadly_recluse(int player, int card, event_t event){
	/* Deadly Recluse	|1|G
	 * Creature - Spider 1/2
	 * Reach
	 * Deathtouch */

	/* Deathgaze Cockatrice	|2|B|B
	 * Creature - Cockatrice 2/2
	 * Flying
	 * Deathtouch */

	/* Giant Scorpion	|2|B
	 * Creature - Scorpion 1/3
	 * Deathtouch */

	/* Greater Basilisk	|3|G|G
	 * Creature - Basilisk 3/5
	 * Deathtouch */

	/* Kessig Recluse	|2|G|G
	 * Creature - Spider 2/3
	 * Reach
	 * Deathtouch */

	/* Pharika's Chosen	|B
	 * Creature - Snake 1/1
	 * Deathtouch */

	/* Sedge Scorpion	|G
	 * Creature - Scorpion 1/1
	 * Deathtouch */

	/* Thornweald Archer	|1|G
	 * Creature - Elf Archer 2/1
	 * Reach
	 * Deathtouch */

	/* Tidehollow Strix	|U|B
	 * Artifact Creature - Bird 2/1
	 * Flying
	 * Deathtouch */

	/* Typhoid Rats	|B
	 * Creature - Rat 1/1
	 * Deathtouch */

	deathtouch(player, card, event);
	return 0;
}

int card_divine_verdict(int player, int card, event_t event){
/* CARD_ID_DIVINE_VERDICT	10406
Divine Verdict	|3|W
Instant
Destroy target attacking or blocking creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_IN_COMBAT;

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_SPELL){
		if (valid_target(&td)){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target attacking or blocking creature.", 1, NULL);
}

int card_gargoyle_castle(int player, int card, event_t event){
/* CARD_ID_GARGOYLE_CASTLE	10418
Gargoyle Castle	""
Land
|T: Add |C to your mana pool.
|5, |T, Sacrifice ~: Create a 3/4 colorless Gargoyle artifact creature token with flying. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_ACTIVATE ){
		int choice = instance->info_slot = 0;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(6), 0, NULL, NULL) ){
			int ai_choice = count_permanents_by_type(player, TYPE_LAND) > 6 ? 1 : 0;
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Generate a Gargoyle\n Cancel", ai_choice);
		}
		if( choice == 0 ){
			return mana_producer(player, card, event);
		}
		if( choice == 1 ){
			add_state(player, card, STATE_TAPPED);
			if( charge_mana_for_activated_ability(player, card, MANACOST_X(5)) ){
				tap_card(player, card);
				instance->info_slot = 1;
				kill_card(player, card, KILL_SACRIFICE);
			}
			else{
				remove_state(player, card, STATE_TAPPED);
			}
		}
		if( choice == 2 ){
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION && instance->info_slot == 1 ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_GARGOYLE, &token);
		token.pow = 3;
		token.tou = 4;
		token.key_plus = KEYWORD_FLYING;
		generate_token(&token);
	}

	return 0;
}

int card_gorgon_flail(int player, int card, event_t event){
/* CARD_ID_GORGON_FLAIL	10424
Gorgon Flail	|2
Artifact - Equipment
Equipped creature gets +1/+1 and has deathtouch. (Any amount of damage it deals to a creature is enough to destroy that creature.)
Equip |2 (|2: Attach to target creature you control. Equip only as a sorcery.) */

	return vanilla_equipment(player, card, event, 2, 1, 1, 0, SP_KEYWORD_DEATHTOUCH);
}

int card_guardian_seraph(int player, int card, event_t event){
/* CARD_ID_GUARDIAN_SERAPH	10427
Guardian Seraph	|2|W|W
Creature - Angel 3/4
Flying
If a source an opponent controls would deal damage to you, prevent 1 of that damage. */

	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card) ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_source_player != player && damage->damage_target_card == -1 && damage->damage_target_player == player &&
				!damage_is_to_planeswalker(damage) && damage->info_slot > 0 )
			{
				prevent_damage(affected_card_controller, affected_card, 1);
			}
		}
	}

	return 0;
}

int card_harms_way(int player, int card, event_t event){
	/* Harm's Way	|W	0x2009797
	 * Instant
	 * The next 2 damage that a source of your choice would deal to you and/or permanents you control this turn is dealt to any target instead. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0);
	td1.extra = damage_card;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td1, NULL, 1, NULL) ){
			return can_target(&td2);
		}
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( pick_target(&td1, "TARGET_DAMAGE") ){
			new_pick_target(&td2, "TARGET_ANY", 1, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td2, 1) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			if( target->info_slot <= 2 ){
				target->damage_target_player = instance->targets[1].player;
				target->damage_target_card = instance->targets[1].card;
			}
			else{
				target->info_slot-=2;
				damage_creature(instance->targets[1].player, instance->targets[1].card, 2,
								target->damage_source_player, target->damage_source_card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_hive_mind(int player, int card, event_t event){
/* CARD_ID_HIVE_MIND	10429
Hive Mind	|5|U
Enchantment
Whenever a player casts an instant or sorcery spell, each other player copies that spell. Each of those players may choose new targets for his or her copy.*/

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_SPELL);
		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &test) ){
			copy_spell_from_stack(1-trigger_cause_controller, trigger_cause_controller, trigger_cause);
		}
	}

	return global_enchantment(player, card, event);
}

int card_honor_of_the_pure( int player, int card, event_t event){
/* CARD_ID_HONOR_OF_THE_PURE	10430
Honor of the Pure	|1|W
Enchantment
|SWhite creatures you control get +1/+1. */

	boost_creature_by_color(player, card, event, COLOR_TEST_WHITE, 1, 1, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);

	return global_enchantment(player, card, event);
}

// illusionary servant --> skulking ghost

// jackal familiar --> mogg conscripts

// kalonian behemoth --> vanilla

int card_lurking_predators(int player, int card, event_t event){
/* CARD_ID_LURKING_PREDATORS	10441
Lurking Predators	|4|G|G
Enchantment
Whenever an opponent casts a spell, reveal the top card of your library. If it's a creature card, put it onto the battlefield.
Otherwise, you may put that card on the bottom of your library. */

	if( ai_is_speculating !=1 && new_specific_spell_played(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		int *deck = deck_ptr[player];
		if( deck[0] != -1 ){
			show_deck( player, deck, 1, "Here's the first card of deck.", 0, 0x7375B0 );
			if( is_what(-1, deck[0], TYPE_CREATURE) ){
				put_into_play_a_card_from_deck(player, player, 0);
			}
			else{
				int choice = do_dialog(player, player, card, -1, -1, " Put the first card on bottom\n Pass", 0);
				if( choice == 0 ){
					put_top_card_of_deck_to_bottom(player);
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_magebane_armor(int player, int card, event_t event){
/*
Magebane Armor |3
Artifact — Equipment
Equipped creature gets +2/+4 and loses flying.
Prevent all noncombat damage that would be dealt to equipped creature.
Equip {2} ({2}: Attach to target creature you control. Equip only as a sorcery.)
*/
	card_instance_t* instance;
	if( (instance = in_play(player, card)) && is_equipping(player, card) ){
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_ABILITIES && affect_me(p, c) ){
			event_result &= ~KEYWORD_FLYING;
		}

		card_instance_t* damage = noncombat_damage_being_prevented(event);
		if( damage && damage->damage_target_card == c && damage->damage_target_player == p ){
			prevent_damage(affected_card_controller, affected_card, -1);
		}
	}

	return vanilla_equipment(player, card, event, 2, 2, 4, 0, 0);
}

int card_master_of_the_wild_hunt(int player, int card, event_t event){
/* Master of the Wild Hunt	|2|G|G
 * Creature - Human Shaman 3/3
 * At the beginning of your upkeep, put a 2/2 |Sgreen Wolf creature token onto the battlefield.
 * |T: Tap all untapped Wolf creatures you control. Each Wolf tapped this way deals damage equal to its power to target creature.
 *		That creature deals damage equal to its power divided as its controller chooses among any number of those Wolves. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_WOLF, &token);
		token.pow = token.tou = 2;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		generate_token(&token);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		int mode = 0;
		if( player != AI ){
			mode = do_dialog(1-player, player, card, -1, -1, " Damaging : auto mode\n Damaging : manual mode", 0);
		}

		int incoming_damage = get_power(instance->targets[0].player, instance->targets[0].card);
		int count = active_cards_count[player]-1;
		while( count > -1 ){
				if( is_what(player, count, TYPE_CREATURE) && has_subtype(player, count, SUBTYPE_WOLF) && in_play(player, count) && ! is_tapped(player, count) ){
					tap_card(player, count);
					damage_creature(instance->targets[0].player, instance->targets[0].card, get_power(player, count), player, count);
					if( mode == 0 ){
						int wolfie = get_toughness(player, count);
						if(incoming_damage >= wolfie){
							damage_creature(player, count, wolfie,instance->targets[0].player, instance->targets[0].card );
							incoming_damage-=wolfie;
						}
						else if(incoming_damage > 0){
								damage_creature(player, count, incoming_damage,instance->targets[0].player, instance->targets[0].card );
								incoming_damage = 0;
						}
					}
					if( mode == 1 ){
						if(incoming_damage > 0){
							get_card_instance(player, count)->token_status |= STATUS_RED_BORDER;
							int number = choose_a_number(player, "Assign how much damage?", incoming_damage);
							get_card_instance(player, count)->token_status &= ~STATUS_RED_BORDER;
							if(number > incoming_damage){
								number = incoming_damage;
							}
							damage_creature(player, count, number, instance->targets[0].player, instance->targets[0].card );
							incoming_damage-=number;
						}
					}
				}
				count--;
		}
	}


	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_mirror_of_fate(int player, int card, event_t event){
/*
Mirror of Fate |5
Artifact
{T}, Sacrifice Mirror of Fate: Choose up to seven face-up exiled cards you own.
	Exile all the cards from your library, then put the chosen cards on top of your library.
*/
	if( event == EVENT_RESOLVE_ACTIVATION ){
		char msg[100] = "Select a card to put into your deck.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, msg);
		int max = 0;
		int count = count_rfg(player);
		int tutored[7];
		int *rfg = rfg_ptr[player];
		while( max < 7 && count > 0 ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_RFG, 0, AI_MAX_VALUE, -1, &this_test);
				if( selected != -1 ){
					tutored[max] = rfg[selected];
					remove_card_from_rfg(player, cards_data[rfg[selected]].id);
					max++;
					count--;
				}
				else{
					break;
				}
		}
		int c1 = count_deck(player)-1;
		while( c1 > -1 ){
				rfg_card_in_deck(player, c1);
				c1--;
		}
		if( max > 0 ){
			count = 0;
			while( count < max ){
					raw_put_iid_on_top_of_deck(player, tutored[count]);
					count++;
			}
			rearrange_top_x(player, player, max);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_open_the_vaults(int player, int card, event_t event){
/* CARD_ID_OPEN_THE_VAULTS	10452
Open the Vaults	|4|W|W
Sorcery
Return all artifact and enchantment cards from all graveyards to the battlefield under their owners' control. (Auras with nothing to enchant remain in graveyards.)*/

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT | TYPE_ENCHANTMENT);
		APNAP(p,{ new_reanimate_all(p, -1, p, &test, REANIMATE_DEFAULT);};);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_palace_guard(int player, int card, event_t event)
{
/* CARD_ID_PALACE_GUARD	10453
Palace Guard	|2|W
Creature - Human Soldier 1/4
~ can block any number of creatures. */

	// 415910
	creature_can_block_additional(player, card, event, 255);

	return 0;
}

int card_planar_cleansing(int player, int card, event_t event){
/* CARD_ID_PLANAR_CLEANSING	10454
Planar Cleansing	|3|W|W|W
Sorcery
Destroy all nonland permanents. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.type_flag = 1;
		APNAP(p,{new_manipulate_all(player, card, p, &this_test, KILL_DESTROY);};);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_rhox_pikemaster(int player, int card, event_t event){
/* CARD_ID_RHOX_PIKEMASTER	10458
Rhox Pikemaster	|2|W|W
Creature - Rhino Soldier 3/3
First strike (This creature deals combat damage before creatures without first strike.)
Other Soldier creatures you control have first strike. */

	boost_creature_type(player, card, event, SUBTYPE_SOLDIER, 0, 0, KEYWORD_FIRST_STRIKE, BCT_CONTROLLER_ONLY);
	return 0;
}

static void sphinx_ambassador_ability(int player, int card, int t_player){
/* Search that [t_player] library for a card, then that player names a card. If you searched for a creature card that isn't the named card,
 * you may put it onto the battlefield under your control. Then that player shuffles his or her library.
*/
	int *deck = deck_ptr[t_player];
	if( deck[0] != -1 ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card.");
		if( IS_AI(player) ){
			default_test_definition(&this_test, TYPE_CREATURE);
		}
		int selected = new_select_a_card(player, 1-player, TUTOR_FROM_DECK, 0, AI_MAX_VALUE, -1, &this_test);
		if( selected != -1 ){
			int csvid = -1;
			if( ! IS_AI(t_player) ){
				csvid = card_from_list(player, 3, TYPE_ANY, 0, 0, 0, 0, 0, 0, 0, -1, 0);
			}
			if( cards_data[deck[selected]].id != csvid && is_what(-1, deck[selected], TYPE_CREATURE) ){
				put_into_play_a_card_from_deck(player, t_player, selected);
			}
		}
		shuffle(1-player);
	}
}

int card_sphinx_ambassador(int player, int card, event_t event){
/*
Sphinx Ambassador |5|U|U
Creature — Sphinx
Flying
Whenever Sphinx Ambassador deals combat damage to a player, search that player's library for a card,
	then that player names a card. If you searched for a creature card that isn't the named card, you may put it onto the battlefield
	under your control. Then that player shuffles his or her library.
*/
	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					while( times_damaged[p] ){
							sphinx_ambassador_ability(player, card, p);
							times_damaged[p]--;
					}
				}
		);
	}

	return 0;
}

static const char* target_is_skeleton_vampire_or_zombie(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  return (has_subtype(player, card, SUBTYPE_SKELETON) || has_subtype(player, card, SUBTYPE_VAMPIRE) || has_subtype(player, card, SUBTYPE_ZOMBIE)
		  ? NULL : gs_target_error_subtype);	//",subtype"
}
int card_undead_slayer(int player, int card, event_t event)
{
  /* Undead Slayer	|2|W
   * Creature - Human Cleric 2/2
   * |W, |T: Exile target Skeleton, Vampire, or Zombie. */

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_PERMANENT);
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = target_predicate_extra(target_is_skeleton_vampire_or_zombie);

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_LITERAL_PROMPT,
								   MANACOST_W(1), 0, &td, "Select target Skeleton, Vampire, or Zombie.");
}

int card_vampire_nocturnus(int player, int card, event_t event){
/* CARD_ID_VAMPIRE_NOCTURNUS	10480
Vampire Nocturnus	|1|B|B|B
Creature - Vampire 3/3
Play with the top card of your library revealed.
As long as the top card of your library is |Sblack, ~ and other Vampire creatures you control get +2/+1 and have flying. */

	reveal_top_card(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		int iid = deck_ptr[player][0];
		if( iid > -1 ){
			if( get_color_by_internal_id(player, iid) & get_sleighted_color_test(player, card, COLOR_TEST_BLACK) ){
				boost_creature_type(player, card, event, SUBTYPE_VAMPIRE, 2, 1, KEYWORD_FLYING, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
			}
		}
	}

	return 0;
}

int card_veteran_armorsmith( int player, int card, event_t event){
/* CARD_ID_VETERAN_ARMORSMITH	10481
Veteran Armorsmith	|W|W
Creature - Human Soldier 2/3
Other Soldier creatures you control get +0/+1. */

	return boost_creature_type(player, card, event, SUBTYPE_SOLDIER, 0, 1, 0, BCT_CONTROLLER_ONLY);
}

int card_veteran_swordsmith( int player, int card, event_t event){
/* CARD_ID_VETERAN_SWORDSMITH	10482
Veteran Swordsmith	|2|W
Creature - Human Soldier 3/2
Other Soldier creatures you control get +1/+0. */

	return boost_creature_type(player, card, event, SUBTYPE_SOLDIER, 1, 0, 0, BCT_CONTROLLER_ONLY);
}

int card_xathrid_demon(int player, int card, event_t event){
/*
Xathrid Demon |3|B|B|B
Creature — Demon 7/7
Flying, trample
At the beginning of your upkeep, sacrifice a creature other than Xathrid Demon, then each opponent loses life equal
	to the sacrificed creature's power. If you can't sacrifice a creature, tap Xathrid Demon and you lose 7 life.
*/
	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int kill = 1;

		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		test.not_me = 1;
		if( new_can_sacrifice(player, card, player, &test) ){
			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_RETURN_CHOICE|SAC_NO_CANCEL, &test);
			if( sac ){
				int amount = get_power(BYTE2(sac), BYTE3(sac));
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				lose_life(1-player, amount);
				kill = 0;
			}
		}

		if( kill ){
			tap_card(player, card);
			lose_life(player, 7);
		}
	}

	return 0;
}

