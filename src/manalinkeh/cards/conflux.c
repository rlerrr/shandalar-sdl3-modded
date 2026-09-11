#include "manalink.h"

// Functions
static int basic_landcycling(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white)
{
  if (event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND)
	return cycling(player, card, event, colorless, black, blue, green, red, white);
  else if (event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1)
	tutor_basic_lands(player, TUTOR_HAND, 1);

  return 0;
}

static int reset_subtype_at_eot(int player, int card, event_t event){

	if( event == EVENT_CLEANUP ){
		card_instance_t *instance = get_card_instance( player, card);
		reset_subtypes(instance->damage_target_player, instance->damage_target_card, 1);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}


// Cards

int card_absorb_vis(int player, int card, event_t event){
/* CARD_ID_ABSORB_VIS	10105
Absorb Vis	|6|B
Sorcery
Target player loses 4 life and you gain 4 life.
Basic landcycling |1|B (|1|B, Discard this card: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library.)*/

		if( IS_ACTIVATING_FROM_HAND(event) ){
		return basic_landcycling(player, card, event, MANACOST_XB(1,1) );
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				lose_life(instance->targets[0].player, 4);
				gain_life(player, 4);
			}
			kill_card(player, card, KILL_DESTROY);
		}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_apocalypse_hydra(int player, int card, event_t event){
	/* Apocalypse Hydra	|X|R|G	0x20082a2
	 * Creature - Hydra 0/0
	 * ~ enters the battlefield with X +1/+1 counters on it. If X is 5 or more, it enters the battlefield with an additional X +1/+1 counters on it.
	 * |1|R, Remove a +1/+1 counter from ~: ~ deals 1 damage to any target. */
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->info_slot > 4 ){
			instance->info_slot*=2;
		}
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, instance->info_slot);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_1_1_COUNTER, MANACOST_XR(1, 1), 0,
									&td, "TARGET_ANY");
}

int card_armillary_sphere(int player, int card, event_t event){
/* CARD_ID_ARMILLARY_SPHERE	10109
Armillary Sphere	|2
Artifact
|2, |T, Sacrifice ~: Search your library for up to two basic land cards, reveal them, and put them into your hand. Then shuffle your library.*/

	if( event == EVENT_RESOLVE_ACTIVATION ){
		char buffer[500] = "Select a basic land card";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, buffer);
		this_test.subtype = SUBTYPE_BASIC;
		this_test.qty = 2;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL);
}

int card_banefire(int player, int card, event_t event){
	/* Banefire	|X|R	0x200b4b6
	 * Sorcery
	 * ~ deals X damage to any target.
	 * If X is 5 or more, this spell can't be countered and the damage can't be prevented. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		}
		if( instance->info_slot > 4 ){
			state_untargettable(player, card, 1);
		}
		pick_target(&td, "TARGET_ANY");
	}

	if( event == EVENT_RESOLVE_SPELL ){
		state_untargettable(player, card, 0);
		if( valid_target(&td) ){
			if( instance->info_slot > 4 ){
				int legacy = create_legacy_effect(player, card, &my_damage_cannot_be_prevented);
				get_card_instance(player, legacy)->targets[0].player = player;
				get_card_instance(player, legacy)->targets[0].card = card;
			}
			damage_creature_or_player(player, card, event, instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_beacon_behemoth(int player, int card, event_t event){
/* CARD_ID_BEACON_BEHEMOTH	10114
Beacon Behemoth	|3|G|G
Creature - Beast 5/3
|1: Target creature with power 5 or greater gains vigilance until end of turn.*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = 5 | TARGET_PT_GREATER_OR_EQUAL;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_VIGILANCE);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_CREATURE");
}

int card_blood_tyrant(int player, int card, event_t event){
/* CARD_ID_BLOOD_TYRANT	10115
Blood Tyrant	|4|U|B|R
Creature - Vampire 5/5
Flying, trample
At the beginning of your upkeep, each player loses 1 life. Put a +1/+1 counter on ~ for each 1 life lost this way.
Whenever a player loses the game, put five +1/+1 counters on ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int i;
		for(i=0; i<2; i++){
			int result = lose_life(i, 1);
			add_1_1_counters(player, card, result);
		}
	}

	return 0;
}

int card_bloodhall_ooze(int player, int card, event_t event){
/* CARD_ID_BLOODHALL_OOZE	10116
Bloodhall Ooze	|R
Creature - Ooze 1/1
At the beginning of your upkeep, if you control a |Sblack permanent, you may put a +1/+1 counter on ~.
At the beginning of your upkeep, if you control a |Sgreen permanent, you may put a +1/+1 counter on ~. */

	if( trigger_condition == TRIGGER_UPKEEP && current_turn == player ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK) |
						get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));
		}
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			add_1_1_counter(player, card);
		}
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			add_1_1_counter(player, card);
		}
	}

	return 0;
}

int card_bone_saw(int player, int card, event_t event){
/* CARD_ID_BONE_SAW	10117
Bone Saw	|0
Artifact - Equipment
Equipped creature gets +1/+0.
Equip |1 (|1: Attach to target creature you control. Equip only as a sorcery.) */

	return vanilla_equipment(player, card, event, 1, 1, 0, 0, 0);
}

int card_charnelhoard_wurm(int player, int card, event_t event){
/* CARD_ID_CHARNELHOARD_WURM	10121
Charnelhoard Wurm	|4|B|R|G
Creature - Wurm 6/6
Trample
Whenever ~ deals damage to an opponent, you may return target card from your graveyard to your hand. */

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_OPPONENT+DDBM_MUST_BE_COMBAT_DAMAGE+DDBM_TRIGGER_OPTIONAL) ){
		char buffer[500] = "Select a card to return to your hand.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, buffer);
		new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return 0;
}

int card_child_of_alara(int player, int card, event_t event){
/* CARD_ID_CHILD_OF_ALARA	10122
Child of Alara	|W|U|B|R|G
Legendary Creature - Avatar 6/6
Trample
When ~ dies, destroy all nonland permanents. They can't be regenerated. */

	check_legend_rule(player, card, event);

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.type_flag = DOESNT_MATCH;
		new_manipulate_all(player, card, 2, &this_test, KILL_BURY);
	}

	return 0;
}

int card_cliffrunner_behemoth(int player, int card, event_t event){
/* CARD_ID_CLIFFRUNNER_BEHEMOTH	10123
Cliffrunner Behemoth	|3|G
Creature - Rhino Beast 5/3
~ has haste as long as you control a |Sred permanent.
~ has lifelink as long as you control a |Swhite permanent. */

	if (event == EVENT_ABILITIES && affect_me(player, card) && in_play(player, card) && ! is_humiliated(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);

		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			haste(player, card, event);
		}
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			lifelink(player, card, event);
		}
	}

	return 0;
}

int card_conflux(int player, int card, event_t event){
/*
Conflux |3|W|U|R|B|G
Sorcery
Search your library for a white card, a blue card, a black card, a red card, and a green card.
Reveal those cards and put them into your hand. Then shuffle your library.
*/

	if(event == EVENT_RESOLVE_SPELL ){
		load_text(0, "COLORWORDS");
		test_definition_t this_test;
		int i;
		for(i=1; i<6; i++){
			char msg[100];
			scnprintf(msg, 100, " Select a %s card.", text_lines[i-1]);
			new_default_test_definition(&this_test, TYPE_ANY, msg);
			this_test.color = 1<<i;
			this_test.no_shuffle = 1;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		}
		shuffle(player);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_countersquall(int player, int card, event_t event){
/* CARD_ID_COUNTERSQUALL	10128
Countersquall	|U|B
Instant
Counter target noncreature spell. Its controller loses 2 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);
	td.illegal_type = TYPE_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return counterspell(player, card, event, &td, 0);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, &td, 0) ){
			lose_life(instance->targets[0].player, 2);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_court_homunculus(int player, int card, event_t event){
/* CARD_ID_COURT_HOMUNCULUS	10129
Court Homunculus	|W
Artifact Creature - Homunculus 1/1
~ gets +1/+1 as long as you control another artifact. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && in_play(player, card) &&
		! is_humiliated(player, card) )
	{
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT);
		this_test.not_me = 1;
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			event_result++;
		}
	}

	return 0;
}

int card_cumber_stone(int player, int card, event_t event){
/* CARD_ID_CUMBER_STONE	1466
Cumber Stone	|3|U
Artifact
Creatures your opponents control get -1/-0. */

	boost_creature_type(player, card, event, -1, -1, 0, 0, BCT_OPPONENT_ONLY);

	if (event == EVENT_CAN_CAST){	// So it can be reused with enchantments, flash permanents, etc.
		return basic_spell(player, card, event);
	}
	return 0;
}

int card_cylian_sunsinger(int player, int card, event_t event){
/* CARD_ID_CYLIAN_SUNSINGER	10130
Cylian Sunsinger	|1|G
Creature - Elf Shaman 2/2
|R|G|W: ~ and each other creature with the same name as it get +3/+3 until end of turn. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.id = get_id(player, instance->parent_card);
		pump_creatures_until_eot(player, card, player, 0, 3, 3, 0, 0, &this_test);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_RGW(1, 1, 1), 0, NULL, NULL);
}

int card_drag_down(int player, int card, event_t event){
	/* Drag Down	|2|B
	 * Instant
	 * Domain - Target creature gets -1/-1 until end of turn for each basic land type among lands you control. */

	 return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, -count_domain(player, card), -count_domain(player, card), 0, 0);
}

int card_dragonsoul_knight(int player, int card, event_t event){
/*
Dragonsoul Knight |2|R
Creature — Human Knight 2/2
First strike
{W}{U}{B}{R}{G}: Until end of turn, Dragonsoul Knight becomes a Dragon, gets +5/+3, and gains flying and trample.
*/

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card);
		force_a_subtype(instance->parent_controller, instance->parent_card, SUBTYPE_DRAGON);
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								5, 3, KEYWORD_FLYING+KEYWORD_TRAMPLE, 0);
		int legacy = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &reset_subtype_at_eot,
													instance->parent_controller, instance->parent_card);
		add_status(player, legacy, STATUS_INVISIBLE_FX);
	}

	return generic_activated_ability(player, card, event, 0, 0, 1, 1, 1, 1, 1, 0, NULL, NULL);
}

int card_dreadwing(int player, int card, event_t event){
/* CARD_ID_DREADWING	10135
Dreadwing	|B
Creature - Zombie 1/1
|1|U|R: ~ gets +3/+0 and gains flying until end of turn. */

	return generic_shade(player, card, event, 0, MANACOST_XUR(1, 1, 1), 3, 0, KEYWORD_FLYING, 0);
}

int card_esperzoa(int player, int card, event_t event){
/* CARD_ID_ESPERZOA	10138
Esperzoa	|2|U
Artifact Creature - Jellyfish 4/3
Flying
At the beginning of your upkeep, return an artifact you control to its owner's hand. */

	if( current_turn == player && !is_humiliated(player, card) && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;

		return bounce_permanent_at_upkeep(player, card, event, &td);
	}

	return 0;
}

int card_ethersworn_adjudicator(int player, int card, event_t event){
	/* Ethersworn Adjudicator	|4|U
	 * Artifact Creature - Vedalken Knight 4/4
	 * Flying
	 * |1|W|B, |T: Destroy target creature or enchantment.
	 * |2|U: Untap ~. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_ENCHANTMENT);

	enum{
		CHOICE_KILL = 1,
		CHOICE_UNTAP
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_XU(2, 1), 0, NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_XBW(1, 1, 1), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = instance->info_slot = 0;
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_XBW(1, 1, 1),
													0, &td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XU(2, 1), 0, NULL, NULL)
		};

		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Kill creature or enchantment", abils[1], 10,
							"Untap", 						abils[2], is_tapped(player, card) ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_KILL ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED | GAA_LITERAL_PROMPT, MANACOST_XBW(1, 1, 1),
											0, &td, "Select target enchantment or creature.");
		}
		if( choice == CHOICE_UNTAP ){
			return generic_activated_ability(player, card, event, 0, MANACOST_XU(2, 1), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_KILL && valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( instance->info_slot == CHOICE_UNTAP ){
			untap_card(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_exploding_borders(int player, int card, event_t event){//UNUSEDCARD
/* CARD_ID_EXPLODING_BORDERS	10140
Exploding Borders	|2|R|G
Sorcery
Domain - Search your library for a basic land card, put that card onto the battlefield tapped, then shuffle your library.
~ deals X damage to target player, where X is the number of basic land types among lands you control. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.zone = TARGET_ZONE_PLAYERS;
	td1.allow_cancel = 0;

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			tutor_basic_land(player, 1, 1);
			int damage = count_domain(player, card);
			if( damage > 0 ){
				damage_player(instance->targets[0].player, damage, player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_PLAYER", 1, NULL);
}

int card_extractor_demon(int player, int card, event_t event){
/* CARD_ID_EXTRACTOR_DEMON	10141
Extractor Demon	|4|B|B
Creature - Demon 5/5
Flying
Whenever another creature leaves the battlefield, you may have target player put the top two cards of his or her library into his or her graveyard.
Unearth |2|B (|2|B: Return this card from your graveyard to the battlefield. It gains haste.
Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

	if( trigger_condition == TRIGGER_LEAVE_PLAY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.zone = TARGET_ZONE_PLAYERS;

		if( affect_me( player, card) && !( trigger_cause == card && trigger_cause_controller == player) &&
			reason_for_trigger_controller == player && can_target(&td)
		  ){
			if( is_what(trigger_cause_controller, trigger_cause, TYPE_CREATURE) && in_play(trigger_cause_controller, trigger_cause) ){
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_AI(player);
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						card_instance_t *instance = get_card_instance(player, card);
						instance->number_of_targets = 0;
						if( pick_target(&td, "TARGET_PLAYER") ){
							mill(instance->targets[0].player, 2);
						}
				}
			}
		}
	}

	return unearth(player, event, MANACOST_XB(2, 1));
}

int card_faerie_mechanist(int player, int card, event_t event){
/* CARD_ID_FAERIE_MECHANIST	10142
Faerie Mechanist	|3|U
Artifact Creature - Faerie Artificer 2/2
Flying
When ~ enters the battlefield, look at the top three cards of your library. You may reveal an artifact card from among them and put it into your hand.
Put the rest on the bottom of your library in any order. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int amount = 3;
		if( count_deck(player) < amount ){
			amount = count_deck(player);
		}
		if( amount > 0 ){
			char msg[100] = "Select an artifact card.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ARTIFACT, msg);
			this_test.create_minideck = amount;
			this_test.no_shuffle = 1;
			if( new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test) != -1 ){
				amount--;
			}
			if( amount > 0 ){
				put_top_x_on_bottom(player, player, amount);
			}
		}
	}

	return 0;
}

int card_fiery_fall(int player, int card, event_t event){
/* CARD_ID_FIERY_FALL	10143
Fiery Fall	|5|R
Instant
~ deals 5 damage to target creature.
Basic landcycling |1|R (|1|R, Discard this card: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library.)*/

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return basic_landcycling(player, card, event, MANACOST_XR(1,1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, 5, player, card);
			}
			kill_card(player, card, KILL_DESTROY);
		}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_fleshformer(int player, int card, event_t event){
/* CARD_ID_FLESHFORMER	10145
Fleshformer	|2|B
Creature - Human Wizard 2/2
|W|U|B|R|G: ~ gets +2/+2 and gains fear until end of turn. Target creature gets -2/-2 until end of turn.
Activate this ability only during your turn. (A creature with fear can't be blocked except by artifact creatures and/or black creatures.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									2, 2, 0, SP_KEYWORD_FEAR);
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							-2, -2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_IN_YOUR_TURN+GAA_CAN_TARGET, 0, 1, 1, 1, 1, 1, 0, &td, "TARGET_CREATURE");
}

int card_font_of_mythos(int player, int card, event_t event)
{
/* CARD_ID_FONT_OF_MYTHOS	1599
Font of Mythos	|4
Artifact
At the beginning of each player's draw step, that player draws two additional cards. */

	if (event == EVENT_DRAW_PHASE && in_play(player, card) && ! is_humiliated(player, card) )
		event_result += 2;

	return 0;
}

static const char* target_damaged_me_this_turn(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
	int good = who_chooses == 0 ? check_special_flags2(player, card, SF2_HAS_DAMAGED_PLAYER0) :
							check_special_flags2(player, card, SF2_HAS_DAMAGED_PLAYER1);
	return good ? NULL : "dealt damage to you this turn";
}

int card_giltspire_avenger(int player, int card, event_t event)
{
/* CARD_ID_GILTSPIRE_AVENGER	10147
Giltspire Avenger	|G|W|U
Creature - Human Soldier 2/2
Exalted (Whenever a creature you control attacks alone, that creature gets +1/+1 until end of turn.)
|T: Destroy target creature that dealt damage to you this turn. */

	exalted(player, card, event, 0, 0);

 	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	// |T: Destroy target creature that dealt damage to you this turn.
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = (int)target_damaged_me_this_turn;

	if (event == EVENT_RESOLVE_ACTIVATION)
	{
		card_instance_t* instance = get_card_instance(player, card);
		if (valid_target(&td))
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_gleam_of_resistence(int player, int card, event_t event){
/* CARD_ID_GLEAM_OF_RESISTANCE	10148
Gleam of Resistance	|4|W
Instant
Creatures you control get +1/+2 until end of turn. Untap those creatures.
Basic landcycling |1|W (|1|W, Discard this card: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 1, 2, 0, 0);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_landcycling(player, card, event, MANACOST_XW(1, 1));
}


int card_gluttonous_slime(int player, int card, event_t event){
/* CARD_ID_GLUTTONOUS_SLIME	10149
Gluttonous Slime	|2|G
Creature - Ooze 2/2
Flash
Devour 1 (As this enters the battlefield, you may sacrifice any number of creatures. This creature enters the battlefield with that many +1/+1 counters on it.)*/

	devour(player, card, event, 1);
	return flash(player, card, event);
}

// goblin outlander --> vanilla

int card_goblin_razerunners(int player, int card, event_t event){
	/* Goblin Razerunners	|2|R|R	0x2008306
	 * Creature - Goblin Warrior 3/4
	 * |1|R, Sacrifice a land: Put a +1/+1 counter on ~.
	 * At the beginning of your end step, you may have ~ deal damage equal to the number of +1/+1 counters on it to target player or planeswalker. */
	if( trigger_condition == TRIGGER_EOT && current_turn == player){
		target_definition_t td;
		default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		card_instance_t *instance = get_card_instance(player, card);

		if( eot_trigger_mode(player, card, event, player, count_1_1_counters(player, card) > 0 &&
			can_target(&td) ? RESOLVE_TRIGGER_AI(player) : 0) )
		{
			if( pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER") ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, count_1_1_counters(player, card), player, card);
			}
		}
		instance->number_of_targets = 0;
	}

	if( (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST) &&
		generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XR(1, 1), 0, NULL, NULL, TYPE_LAND,
														NULL, 0) )
	{
		int max_sac = 0;
		int total_pump = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_LAND) && ! check_special_flags2(player, c, SF2_CANNOT_BE_SACRIFICED) ){
				max_sac++;
			}
		}
		for(c = 0; c < max_sac; ++c){
				if( has_mana_for_activated_ability(player, card, MANACOST_XR(c, c)) ){
					total_pump = c;
				}
				else{
					break;
				}
		}
		return total_pump;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_LAND, "Select a land card to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		add_1_1_counter(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST_XR(1, 1), 0, NULL, NULL, 0, &this_test, 0);
}

static int grixis_slavedriver_death_effect(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance= get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( trigger_condition == TRIGGER_LEAVE_PLAY ){
			if( affect_me( player, card) && trigger_cause == c && trigger_cause_controller == p
				&& reason_for_trigger_controller == player )
			{
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_MANDATORY;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						generate_token_by_id(p, c, CARD_ID_ZOMBIE);
						kill_card(player, card, KILL_EXILE);
				}
			}
		}
	}

	return 0;
}

int card_grixis_slavedriver(int player, int card, event_t event){
	/* Grixis Slavedriver	|5|B
	 * Creature - Zombie Giant 4/4
	 * When ~ leaves the battlefield, put a 2/2 |Sblack Zombie creature token onto the battlefield.
	 * Unearth |3|B */

	if( event == EVENT_RESOLVE_SPELL ){
		create_targetted_legacy_effect(player, card, &grixis_slavedriver_death_effect, player, card);
	}

	return unearth(player, event, MANACOST_XB(3, 1));
}

int card_hellkite_hatchling(int player, int card, event_t event){

	devour(player, card, event, 1);

	if( get_card_instance(player, card)->targets[1].player == 66 && in_play(player, card) && ! is_humiliated(player, card) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_FLYING | KEYWORD_TRAMPLE);
	}

	return 0;
}

int card_hellspark_elemental(int player, int card, event_t event){
/* CARD_ID_HELLSPARK_ELEMENTAL	10156
Hellspark Elemental	|1|R
Creature - Elemental 3/1
Trample, haste
At the beginning of the end step, sacrifice ~.
Unearth |1|R (|1|R: Return this card from your graveyard to the battlefield. It gains haste.
Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

	card_ball_lightning(player, card, event);
	return unearth(player, event, MANACOST_XR(1, 1));
}

int card_kaleidostone(int player, int card, event_t event){
/*
Kaleidostone |2
Artifact
When Kaleidostone enters the battlefield, draw a card.
{5}, {T}, Sacrifice Kaleidostone: Add {W}{U}{B}{R}{G} to your mana pool.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(5), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		ai_modifier -= 24;

		card_instance_t* instance = get_card_instance(player, card);
		instance->state |= STATE_TAPPED;
		charge_mana(player, COLOR_COLORLESS, 5);
		if( spell_fizzled == 1 ){
			instance->state &= ~STATE_TAPPED;
		}
		else{
			produce_mana_tapped_multi(player, card, 0, 1, 1, 1, 1, 1);
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_kederekt_parasite(int player, int card, event_t event){

	/* Kederekt Parasite	|B
	 * Creature - Horror 1/1
	 * Whenever an opponent draws a card, if you control a |Sred permanent, you may have ~ deal 1 damage to that player. */

	if( trigger_condition == TRIGGER_CARD_DRAWN && affect_me(player, card) && reason_for_trigger_controller == player &&
		trigger_cause_controller == 1-player && in_play(player, card) && !is_humiliated(player, card)
	  ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);

		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			if( event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_AI(player);
			}
			else if( event == EVENT_RESOLVE_TRIGGER ){
					damage_player(1-player, 1, player, card);
			}
		}
	}

	return 0;
}

int card_knight_of_the_reliquary(int player, int card, event_t event){
/* Knight of the Reliquary	|1|G|W
 * Creature - Human Knight 2/2
 * ~ gets +1/+1 for each land card in your graveyard.
 * |T, Sacrifice |Ha Forest or |H2Plains: Search your library for a land card, put it onto the battlefield, then shuffle your library. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
		if( player == -1 || (player > -1 && in_play(player, card) && ! is_humiliated(player, card)) ){
			event_result += count_graveyard_by_type(player, TYPE_LAND );
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Forest or a Plains to sacrifice.");
	test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
	test.sub2 = get_hacked_subtype(player, card, SUBTYPE_PLAINS);
	test.subtype_flag = F2_MULTISUBTYPE;


	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t test2;
		new_default_test_definition(&test2, TYPE_LAND, "Select a land card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test2);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL, 0, &test, 0);
}

int card_knotvine_mystic(int player, int card, event_t event){
/*
Knotvine Mystic |W|R|G
Creature — Elf Druid 2/2
{1}, {T}: Add {R}{G}{W} to your mana pool.
*/
	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		card_instance_t* instance = get_card_instance(player, card);
		instance->state |= STATE_TAPPED;
		charge_mana(player, COLOR_COLORLESS, 1);
		if( spell_fizzled == 1 ){
			instance->state &= ~STATE_TAPPED;
		}
		else{
			produce_mana_tapped_multi(player, card, MANACOST_RGW(1, 1, 1));
		}
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( event == EVENT_CAN_ACTIVATE &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL) )
		{
			int i;
			for(i=COLOR_GREEN; i<=COLOR_WHITE; i++){
				declare_mana_available(player, i, 1);
			}
		}
	}

	return 0;
}

// Lapse of Certainty --> memory lapse

int card_maelstrom_archangel (int player, int card, event_t event){
/* CARD_ID_MAELSTROM_ARCHANGEL	10166
Maelstrom Archangel	|W|U|B|R|G
Creature - Angel 5/5
Flying
Whenever ~ deals combat damage to a player, you may cast a nonland card from your hand without paying its mana cost. */

	if( has_combat_damage_been_inflicted_to_a_player(player, card, event) ){
		if( hand_count[player] ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, "Choose a nonland card to play for free.");
			test.type_flag = DOESNT_MATCH;

			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &test);

			if( selected > -1 ){
				play_card_in_hand_for_free(player, selected);
			}
		}
	}

	return 0;
}

int card_magister_sphinx(int player, int card, event_t event){
/* CARD_ID_MAGISTER_SPHINX	10167
Magister Sphinx	|4|W|U|B
Artifact Creature - Sphinx 5/5
Flying
When ~ enters the battlefield, target player's life total becomes 10. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		select_target(player, card, &td, "Choose a target player", NULL);

		card_instance_t *instance = get_card_instance(player, card);

		set_life_total(instance->targets[0].player, 10);

		instance->number_of_targets = 0;
	}

	return 0;
}

int card_malfegor(int player, int card, event_t event){
/* CARD_ID_MALFEGOR	10168
Malfegor	|2|B|B|R|R
Legendary Creature - Demon Dragon 6/6
Flying
When ~ enters the battlefield, discard your hand. Each opponent sacrifices a creature for each card discarded this way. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int amount = hand_count[player];

		discard_all(player);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.qty = amount;
		new_sacrifice(player, card, 1-player, SAC_CAUSED | SAC_NO_CANCEL, &this_test);
	}

	return 0;
}

int card_mark_of_asylum(int player, int card, event_t event){
/* CARD_ID_MARK_OF_ASYLUM	10170
Mark of Asylum	|1|W
Enchantment
Prevent all noncombat damage that would be dealt to creatures you control. */

	card_instance_t* damage = noncombat_damage_being_prevented(event);
	if (damage && damage->damage_target_player == player && damage->damage_target_card != -1){
		int amount = damage->info_slot;
		prevent_damage(affected_card_controller, affected_card, amount);
	}

	return global_enchantment(player, card, event);
}

int card_martial_coup(int player, int card, event_t event){
	/* Martial Coup	|X|W|W
	 * Sorcery
	 * Put X 1/1 |Swhite Soldier creature tokens onto the battlefield. If X is 5 or more, destroy all other creatures. */

	if(event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		APNAP(p, {new_manipulate_all(player, card, p, &this_test, KILL_DESTROY);};);

		card_instance_t *instance = get_card_instance( player, card);

		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SOLDIER, &token);
		token.pow = token.tou = 1;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		token.qty = instance->info_slot;
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_master_transmuter(int player, int card, event_t event){
/*
Master Transmuter |3|U
Artifact Creature — Human Artificer 1/2
{U}, {T}, Return an artifact you control to its owner's hand: You may put an artifact card from your hand onto the battlefield.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(1), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_U(1), 0,
								&td, "Select an artifact you control.");
		if( spell_fizzled != 1 ){
			instance->number_of_targets = 0;
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an artifact card.");
		new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
	}

	return 0;
}

int card_matca_rioters(int player, int card, event_t event){
	/*
	  Matca Rioters |2|G
	  Creature - Human Warrior x/x
	  Domain - Matca Rioters's power and toughness are each equal to the number of basic land types among lands you control.
	*/
	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && !is_humiliated(player, card) && player != -1){
		event_result += count_domain(player, card);
	}
	return 0;
}

int card_meglonoth(int player, int card, event_t event){
/* CARD_ID_MEGLONOTH	10173
Meglonoth	|3|R|G|W
Creature - Beast 6/6
Vigilance, trample
Whenever ~ blocks a creature, ~ deals damage to that creature's controller equal to ~'s power. */

	card_instance_t *instance = get_card_instance(player, card);

	vigilance(player, card, event);

	if( event == EVENT_DECLARE_BLOCKERS && current_turn != player && !is_humiliated(player, card) ){
		if( instance->blocking < 255 ){
			damage_player(1-player, get_power(player, card), player, card);
		}
	}

	return 0;
}


int card_might_of_alara_unlocked(int player, int card, event_t event){
	// Used externally
	return vanilla_instant_pump(player, card, event, ANYBODY, player, count_domain(player, card), count_domain(player, card), 0, 0);
}
int card_might_of_alara(int player, int card, event_t event){
/* CARD_ID_MIGHT_OF_ALARA	10174
Might of Alara	|G
Instant
Domain - Target creature gets +1/+1 until end of turn for each basic land type among lands you control. */

	if( ! is_unlocked(player, card, event, 24) ){ return 0; }
	return card_might_of_alara_unlocked(player, card, event);
}

int card_mirror_sigil_sergeant(int player, int card, event_t event){
/* CARD_ID_MIRROR_SIGIL_SERGEANT	10175
Mirror-Sigil Sergeant	|5|W
Creature - Rhino Soldier 4/4
Trample
At the beginning of your upkeep, if you control a |Sblue permanent, you may create a token that's a copy of ~. */

	if( trigger_condition == TRIGGER_UPKEEP && current_turn == player && get_special_infos(player, card) != 88 ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));
		}
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			token_generation_t token;
			default_token_definition(player, card, get_id(player, card), &token);
			token.special_infos = 88;
			generate_token(&token);
		}
	}

	if (event == EVENT_CLEANUP && affect_me(player, card)){
		set_special_infos(player, card, 0);
	}

	return 0;
}

// nacatl outlander --> vanilla

int card_nicol_bolas_planeswalker(int player, int card, event_t event){
	/* Nicol Bolas, Planeswalker	|4|U|B|B|R	0x200186c
	 * Legendary Planeswalker - Bolas (5)
	 * +3: Destroy target noncreature permanent.
	 * -2: Gain control of target creature.
	 * -9: ~ deals 7 damage to target player or planeswalker. That player or that planeswalker's controller discards seven cards, then sacrifices seven permanents. */
	if (IS_ACTIVATING(event)){

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_CREATURE;

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		if( player == AI ){
			td1.allowed_controller = 1-player;
			td1.preferred_controller = 1-player;
		}

		target_definition_t td2;
		default_target_definition(player, card, &td2, TARGET_TYPE_PLANESWALKER);
		td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		card_instance_t *instance = get_card_instance(player, card);

		enum{
			CHOICE_KILL_NONCREATURE = 1,
			CHOICE_STEAL_CREATURE,
			CHOICE_CRUEL_ULTIMATUM
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Destroy noncreature", can_target(&td), 10, 3, DLG_LITERAL_TARGET(&td, "Select target noncreature permanent"),
						"Gain control", can_target(&td1), 8, -2, DLG_TARGET(&td1, "TARGET_CREATURE"),
						"7 damage, discard 7, sac 7", can_target(&td2), 20, -9, DLG_TARGET(&td2, "TARGET_PLAYER_OR_PLANESWALKER"));

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_RESOLVE_ACTIVATION)
		switch (choice)
		{
			case CHOICE_KILL_NONCREATURE:
			{
				if( valid_target(&td) ){
					kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
				}
			}
			break;

			case CHOICE_STEAL_CREATURE:
			{
				if( valid_target(&td1) ){
					gain_control(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
				}
			}
			break;

			case CHOICE_CRUEL_ULTIMATUM:
			{
				if( valid_target(&td2) ){
					damage_target0(player, card, 7);
					multidiscard(instance->targets[0].player, 7, 0);
					impose_sacrifice(player, card, instance->targets[0].player, 7, TYPE_PERMANENT, 0, 0, 0, 0, 0, 0, 0, -1, 0);
				}
			}
			break;
		}
	}

	return planeswalker(player, card, event, 5);
}

int card_noble_hierarch( int player, int card, event_t event){
/* CARD_ID_NOBLE_HIERARCH	10181
Noble Hierarch	|G
Creature - Human Druid 0/1
Exalted (Whenever a creature you control attacks alone, that creature gets +1/+1 until end of turn.)
|T: Add |G, |W, or |U to your mana pool. */

	exalted(player, card, event, 0, 0);
	return card_generic_noncombat_1_mana_producing_creature(player, card, event);
}

int card_nyxathid  (int player, int card, event_t event){
/* CARD_ID_NYXATHID	10182
Nyxathid	|1|B|B
Creature - Elemental 7/7
As ~ enters the battlefield, choose an opponent.
~ gets -1/-1 for each card in the chosen player's hand. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
		if( in_play(player, card) && ! is_humiliated(player, card) ){
			event_result -= hand_count[1-player];
		}
	}
	return 0;
}

int card_obelisk_of_alara2(int player, int card, event_t event){
	/* Obelisk of Alara	|6	0x20057b9
	 * Artifact
	 * |1|W, |T: You gain 5 life.
	 * |1|U, |T: Draw a card, then discard a card.
	 * |1|B, |T: Target creature gets -2/-2 until end of turn.
	 * |1|R, |T: ~ deals 3 damage to target player or planeswalker.
	 * |1|G, |T: Target creature gets +4/+4 until end of turn. */
	if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td_player;
		default_target_definition(player, card, &td_player, TARGET_TYPE_PLANESWALKER);
		td_player.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		target_definition_t td_opp_creature;
		default_target_definition(player, card, &td_opp_creature, TYPE_CREATURE);

		target_definition_t td_own_creature;
		default_target_definition(player, card, &td_own_creature, TYPE_CREATURE);
		td_own_creature.preferred_controller = player;

		target_definition_t td_own_creature_attacking;
		default_target_definition(player, card, &td_own_creature, TYPE_CREATURE);
		td_own_creature_attacking.required_state = TARGET_STATE_ATTACKING;
		td_own_creature_attacking.preferred_controller = player;

		if (event == EVENT_CAN_ACTIVATE
			&& (!can_use_activated_abilities(player, card) || is_tapped(player, card) || is_animated_and_sick(player, card))){
			return 0;
		}

		enum
		{
			CHOICE_GAIN_LIFE = 1,
			CHOICE_DRAW_DISCARD,
			CHOICE_WEAKEN_CREATURE,
			CHOICE_DAMAGE_PLAYER,
			CHOICE_PUMP_CREATURE,
		} choice = DIALOG(player, card, event,
						  "Gain life", 1,
						  life[player] >= 6 ? 1	// Worse choice if I have 6 or more life
						  : 8,					// Otherwise, only outweighed by red, and then only if opponent has 3 or less life.
						  DLG_MANA(MANACOST_XW(1,1)),

						  "Draw & discard", 1,
						  4,	// Just behind black.
						  DLG_MANA(MANACOST_XU(1,1)),

						  "Weaken creature", can_target(&td_opp_creature),
						  5,	// First choice unless (red: opponent <= 6 life; white: self <= 5 life; or green: have an attacking creature after blocking)
						  DLG_MANA(MANACOST_XB(1,1)),

						  "Damage player or planeswalker", can_target(&td_player),
						  life[1-player] > 6 ? 2		// Second-worse choice if opponent has more than six life
						  : life[1-player] > 3 ? 7		// Best choice if opponent has three or less life
						  : 9,							// Otherwise, only outweighed by white, and then only if I have 5 or less life.
						  DLG_MANA(MANACOST_XR(1,1)),

						  "Pump creature", can_target(&td_own_creature),
						  player == AI && current_turn == AI && current_phase == PHASE_AFTER_BLOCKING && can_target(&td_own_creature_attacking) ? 6
								// Pulls in front of black and blue if it's my turn, it's the post-block phase, and I have an attacking creature
						  : 3,	// Otherwise, worse than black or blue, but better than red or white (unless those are modified by a player with low life)
						  DLG_MANA(MANACOST_XG(1,1))
						  );

		if (event == EVENT_CAN_ACTIVATE){
			return choice;
		} else if (event == EVENT_ACTIVATE){
			instance->number_of_targets = 0;
			switch (choice){
				case CHOICE_GAIN_LIFE:
				case CHOICE_DRAW_DISCARD:
					break;

				case CHOICE_WEAKEN_CREATURE:
					pick_target(&td_opp_creature, "TARGET_CREATURE");
					break;

				case CHOICE_DAMAGE_PLAYER:
					pick_target(&td_player, "TARGET_PLAYER_OR_PLANESWALKER");
					break;

				case CHOICE_PUMP_CREATURE:
					if (player == AI && current_turn == AI && current_phase == PHASE_AFTER_BLOCKING && can_target(&td_own_creature_attacking)){
						pick_target(&td_own_creature_attacking, "TARGET_CREATURE");
					} else {
						pick_target(&td_own_creature, "TARGET_CREATURE");
					}
					break;
			}

			if (spell_fizzled != 1){
				tap_card(player, card);
			}
		} else if (event == EVENT_RESOLVE_ACTIVATION){
			switch (choice){
				case CHOICE_GAIN_LIFE:
					gain_life(player, 5);
					break;
				case CHOICE_DRAW_DISCARD:
					draw_cards(player, 1);
					discard(player, 0, 0);
					break;
				case CHOICE_WEAKEN_CREATURE:
					if (valid_target(&td_opp_creature)){
						pump_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, -2, -2);
					}
					break;
				case CHOICE_DAMAGE_PLAYER:
					if (valid_target(&td_player)){
						damage_target0(player, card, 3);
					}
					break;
				case CHOICE_PUMP_CREATURE:
					if (valid_target(&td_own_creature)){
						pump_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 4, 4);
					}
					break;
			}
		}
	}

	return 0;
}

int card_paleoloth(int player, int card, event_t event){
/*
Paleoloth |4|G|G
Creature — Beast 5/5
Whenever another creature with power 5 or greater enters the battlefield under your control, you may return target creature
	card from your graveyard to your hand.
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.power = 4;
		this_test.power_flag = 2;
		this_test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test2;
		new_default_test_definition(&this_test2, TYPE_CREATURE, "Select target creature card.");

		if( ! graveyard_has_shroud(player) && count_graveyard_by_type(player, TYPE_CREATURE) ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test2);
		}
	}

	return 0;
}

int card_paragon_of_the_amesha(int player, int card, event_t event){
/*
Paragon of the Amesha |2|W
Creature — Human Knight 2/2
First strike
{W}{U}{B}{R}{G}: Until end of turn, Paragon of the Amesha becomes an Angel, gets +3/+3, and gains flying and lifelink.
*/
	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t* instance = get_card_instance(player, card);
		force_a_subtype(instance->parent_controller, instance->parent_card, SUBTYPE_ANGEL);
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								3, 3, KEYWORD_FLYING, SP_KEYWORD_LIFELINK);
		int legacy = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &reset_subtype_at_eot,
													instance->parent_controller, instance->parent_card);
		add_status(player, legacy, STATUS_INVISIBLE_FX);
	}

	return generic_activated_ability(player, card, event, 0, 0, 1, 1, 1, 1, 1, 0, NULL, NULL);
}

int card_parasitic_strix(int player, int card, event_t event){
/* CARD_ID_PARASITIC_STRIX	10185
Parasitic Strix	|2|U
Artifact Creature - Bird 2/2
Flying
When ~ enters the battlefield, if you control a |Sblack permanent, target player loses 2 life and you gain 2 life. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

		card_instance_t *instance = get_card_instance(player, card);

		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.zone = TARGET_ZONE_PLAYERS;
			td.allow_cancel = 0;
			select_target(player, card, &td, "Choose a target player", NULL);
			lose_life(instance->targets[0].player, 2);
			gain_life(player, 2);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_path_to_exile(int player, int card, event_t event){
/* CARD_ID_PATH_TO_EXILE	1559
Path to Exile	|W
Instant
Exile target creature. Its controller may search his or her library for a basic land card, put that card onto the battlefield tapped, then shuffle his or her library.*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			int choice = 0;
			if( instance->targets[0].player != AI ){
				choice = do_dialog(instance->targets[0].player, player, card, -1, -1, " Search a basic land\n Pass", 0);
			}
			if( choice == 0 ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_LAND, "Select a basic land card.");
				this_test.subtype = SUBTYPE_BASIC;
				new_global_tutor(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_PLAY_TAPPED, 0,
								AI_FIRST_FOUND, &this_test);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_progenitus(int player, int card, event_t event)
{
  /* Progenitus	|W|W|U|U|B|B|R|R|G|G
   * Legendary Creature - Hydra Avatar 10/10
   * Protection from everything
   * If ~ would be put into a graveyard from anywhere, reveal ~ and shuffle it into its owner's library instead. */

  // Last ability handled in special_mill() and raw_put_iid_on_top_of_graveyard_with_triggers().

	check_legend_rule(player, card, event);
	unblockable(player, card, event);
	return 0;
}

int card_rakka_mar(int player, int card, event_t event){
/* CARD_ID_RAKKA_MAR	10189
Rakka Mar	|2|R|R
Legendary Creature - Human Shaman 2/2
Haste
|R, |T: Create a 3/1 |Sred Elemental creature token with haste. */

	check_legend_rule(player, card, event);

	haste(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		token.pow = 3;
		token.tou = 1;
		token.s_key_plus = SP_KEYWORD_HASTE;
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_R(1), 0, NULL, NULL);
}

int card_reliquiary_tower(int player, int card, event_t event){
/* CARD_ID_RELIQUARY_TOWER	10190
Reliquary Tower	""
Land
You have no maximum hand size.
|T: Add |C to your mana pool. */

	if( current_turn == player && event == EVENT_MAX_HAND_SIZE && in_play(player, card) && ! is_humiliated(player, card) ){
		event_result+=99;
	}
	return mana_producer(player, card, event);
}

int card_rotting_rats(int player, int card, event_t event){
/* CARD_ID_ROTTING_RATS	10193
Rotting Rats	|1|B
Creature - Zombie Rat 1/1
When ~ enters the battlefield, each player discards a card.
Unearth |1|B (|1|B: Return this card from your graveyard to the battlefield. It gains haste.
Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		APNAP(p, {discard(p, 0, player);};);
	}

	return unearth(player, event, MANACOST_XB(1, 1));
}

// rupture spire --> transguild promenade

int card_salvage_slasher(int player, int card, event_t event){
/* CARD_ID_SALVAGE_SLASHER	10196
Salvage Slasher	|1|B
Artifact Creature - Human Rogue 1/1
~ gets +1/+0 for each artifact card in your graveyard. */

	if( event == EVENT_POWER && affect_me(player, card) && in_play(player, card) && ! is_humiliated(player, card) ){
		event_result += count_graveyard_by_type(player, TYPE_ARTIFACT );
	}
	return 0;
}

int card_scarland_thrinax(int player, int card, event_t event){
	/* Scarland Thrinax	|B|R|G
	 * Creature - Lizard 2/2
	 * Sacrifice a creature: Put a +1/+1 counter on ~. */

	/* Bloodflow Connoisseur	|2|B
	 * Creature - Vampire 1/1
	 * Sacrifice a creature: Put a +1/+1 counter on ~. */

	if( event != EVENT_RESOLVE_ACTIVATION ){
		return generic_husk(player, card, event, TYPE_CREATURE, 1, 1, 0, 0);
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		add_1_1_counter(instance->parent_controller, instance->parent_card);
	}
	return 0;
}

int card_scattershot_archer(int player, int card, event_t event){
/* CARD_ID_SCATTERSHOT_ARCHER	10198
Scattershot Archer	|G
Creature - Elf Archer 1/2
|T: ~ deals 1 damage to each creature with flying. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.keyword = KEYWORD_FLYING;
		new_damage_all(instance->parent_controller, instance->parent_card, ANYBODY, 1, 0, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_scepter_of_dominance(int player, int card, event_t event){
/* CARD_ID_SCEPTER_OF_DOMINANCE	10199
Scepter of Dominance	|1|W|W
Artifact
|W, |T: Tap target permanent. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(1), 0, &td, "TARGET_CREATURE");
}

int card_scepter_of_fugue( int player, int card, event_t event){
/* CARD_ID_SCEPTER_OF_FUGUE	10200
Scepter of Fugue	|B|B
Artifact
|1|B, |T: Target player discards a card. Activate this ability only during your turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_NONE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			discard(instance->targets[0].player, 0, player);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_IN_YOUR_TURN, MANACOST_XB(1, 1), 0,
									&td, "TARGET_PLAYER");
}

int card_scepter_of_insight( int player, int card, event_t event){
/* CARD_ID_SCEPTER_OF_INSIGHT	10201
Scepter of Insight	|1|U|U
Artifact
|3|U, |T: Draw a card. */

	if(event == EVENT_RESOLVE_ACTIVATION){
		draw_cards(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XU(3, 1), 0, NULL, NULL);
}

int card_sedraxis_alchemist(int player, int card, event_t event){
/* CARD_ID_SEDRAXIS_ALCHEMIST	10203
Sedraxis Alchemist	|2|B
Creature - Zombie Wizard 2/2
When ~ enters the battlefield, if you control a |Sblue permanent, return target nonland permanent to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);

		card_instance_t *instance = get_card_instance(player, card);

		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_PERMANENT);
			td.illegal_type = TYPE_LAND;
			td.allow_cancel = 0;
			select_target(player, card, &td, "Choose target nonland permanent", NULL);
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_shambling_remains(int player, int card, event_t event){
/* CARD_ID_SHAMBLING_REMAINS	10204
Shambling Remains	|1|B|R
Creature - Zombie Horror 4/3
~ can't block.
Unearth |B|R (|B|R: Return this card from your graveyard to the battlefield. It gains haste.
Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

	cannot_block(player, card, event);
	return unearth(player, event, MANACOST_BR(1, 1));
}

int card_shard_convergence(int player, int card, event_t event)
{

  /* Search your library for a Plains card, an Island card, a Swamp card, and a Mountain card. Reveal those cards and put them into your hand. Then shuffle your
   * library. */
	if (event == EVENT_RESOLVE_SPELL)
	{
	  subtype_t subtypes[4] = { SUBTYPE_PLAINS, SUBTYPE_ISLAND, SUBTYPE_SWAMP, SUBTYPE_MOUNTAIN };
	  int i;
	  for (i = 0; i < 4; ++i)
		{
		  test_definition_t this_test;
		  new_default_test_definition(&this_test, TYPE_LAND, get_hacked_land_text(player, card, "Select %a card", subtypes[i]));
		  this_test.subtype = get_hacked_subtype(player, card, subtypes[i]);
		  if (i != 3)
			this_test.no_shuffle = 1;
		  new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_sigil_of_the_empty_throne(int player, int card, event_t event){
	/* Sigil of the Empty Throne	|3|W|W
	 * Enchantment
	 * Whenever you cast an enchantment spell, put a 4/4 |Swhite Angel creature token with flying onto the battlefield. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ENCHANTMENT);

		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ANGEL, &token);
			token.pow = token.tou = 4;
			token.key_plus = KEYWORD_FLYING;
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
			generate_token(&token);
		}
	}

	return global_enchantment(player, card, event);
}


int card_sludge_strider(int player, int card, event_t event){
/*
Sludge Strider |1|W|U|B
Artifact Creature — Insect 3/3
Whenever another artifact enters the battlefield under your control or another artifact you control leaves the battlefield, you may pay {1}.
	If you do, target player loses 1 life and you gain 1 life.
*/ 


	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT);
		this_test.not_me = 1;
	
		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &this_test, player, card);
	}
	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);
							instance->number_of_targets = 0;
		if( has_mana(player, COLOR_COLORLESS, 1) && can_target(&td) ){
			charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_COLORLESS, 1);
			if( spell_fizzled != 1 && pick_target(&td, "TARGET_PLAYER") ){
				lose_life(instance->targets[0].player, 1);
				gain_life(player, 1);
			}
		}
		instance->number_of_targets = 0;
	}
	
		if( trigger_condition == TRIGGER_LEAVE_PLAY && affect_me(player, card) && !( trigger_cause == card && trigger_cause_controller == player) 
			&& reason_for_trigger_controller == player ){
			if( is_what(trigger_cause_controller, trigger_cause, TYPE_ARTIFACT) ){
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_AI(player);
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						call_card_function(player, card, EVENT_ETB_ABILITY);
				}
			}
		}



	return 0;
}

int card_souls_majesty(int player, int card, event_t event){
/*
Soul's Majesty |4|G
Sorcery
Draw cards equal to the power of target creature you control.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(player, get_power(instance->targets[0].player, instance->targets[0].card));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature you control.", 1, NULL);
}

int card_sphinx_summoner(int player, int card, event_t event){
/* CARD_ID_SPHINX_SUMMONER	10210
Sphinx Summoner	|3|U|B
Artifact Creature - Sphinx 3/3
Flying
When ~ enters the battlefield, you may search your library for an artifact creature card, reveal it, and put it into your hand. If you do, shuffle your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select an artifact creature card.");
		this_test.special_selection_function = &is_artifact_creature_by_internal_id;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}
	return 0;
}

int card_spore_burst(int player, int card, event_t event){
	/* Spore Burst	|3|G
	 * Sorcery
	 * Domain - Put a 1/1 |Sgreen Saproling creature token onto the battlefield for each basic land type among lands you control. */

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, count_domain(player, card));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_sylvan_bounty(int player, int card, event_t event){
/* CARD_ID_SYLVAN_BOUNTY	10213
Sylvan Bounty	|5|G
Instant
Target player gains 8 life.
Basic landcycling |1|G (|1|G, Discard this card: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library.)*/

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return basic_landcycling(player, card, event, MANACOST_XG(1,1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				gain_life(instance->targets[0].player, 8);
			}
			kill_card(player, card, KILL_DESTROY);
		}

     return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}


int card_telemin_performance(int player, int card, event_t event){
/*
Telemin Performance |3|U|U
Sorcery
Target opponent reveals cards from the top of his or her library until he or she reveals a creature card.
That player puts all noncreature cards revealed this way into his or her graveyard, then you put the creature card onto
	the battlefield under your control.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			reveal_card_until_you_reveal_test(player, instance->targets[0].player, &this_test,
												RCUYRT_PUT_TEST_INTO_PLAY | RCUYRT_MILL_REST);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_thornling(int player, int card, event_t event){
/*
Thornling |3|G|G
Creature — Elemental Shapeshifter 4/4
{G}: Thornling gains haste until end of turn.
{G}: Thornling gains trample until end of turn.
{G}: Thornling gains indestructible until end of turn.
{1}: Thornling gets +1/-1 until end of turn.
{1}: Thornling gets -1/+1 until end of turn.
*/
	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( current_turn == player ){
			return generic_shade(player, card, event, 0, MANACOST_X(1), 1, -1, 0, 0);
		}
		else{
			return generic_shade(player, card, event, 0, MANACOST_X(1), -1, 1, 0, 0);
		}
	}

	if( event == EVENT_CLEANUP ){
		card_instance_t *instance = get_card_instance( player, card);
		instance->targets[1].player = instance->targets[1].card = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_HASTE = 1,
		CHOICE_TRAMPLE,
		CHOICE_INDESTRUCTIBLE,
		CHOICE_P1_M1,
		CHOICE_M1_P1
	};

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int green_ability = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_G(1), 0, NULL, NULL);
		int cless_ability = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(1), 0, NULL, NULL);
		int actual_key = get_abilities(player, card, EVENT_ABILITIES, -1);
		actual_key |= (instance->targets[1].player > -1 ? instance->targets[1].player : 0);
		int actual_skey = get_special_abilities(player, card);
		actual_skey |= (instance->targets[1].card > -1 ? instance->targets[1].card : 0);

		int priorities[6] = {0};
		priorities[CHOICE_HASTE] = !(actual_skey & SP_KEYWORD_HASTE) && current_phase < PHASE_DECLARE_ATTACKERS &&
									current_turn == player && is_sick(player, card) ? 10 : 1;
		priorities[CHOICE_TRAMPLE] = !(actual_key & KEYWORD_TRAMPLE) && current_phase == PHASE_AFTER_BLOCKING &&
									! is_unblocked(player, card) ? 10 : 1;
		priorities[CHOICE_INDESTRUCTIBLE] = !(actual_skey & SP_KEYWORD_INDESTRUCTIBLE) ? 5 : 1;
		priorities[CHOICE_P1_M1] = current_turn == player && current_phase == PHASE_AFTER_BLOCKING ? 8 : 1;
		priorities[CHOICE_M1_P1] = current_turn != player && current_phase == PHASE_AFTER_BLOCKING ? 8 : 1;

		int choice = DIALOG(player, card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_NO_CANCEL,
							"Haste", 			green_ability, priorities[1],
							"Trample", 			green_ability, priorities[2],
							"Indestructible", 	green_ability, priorities[3],
							"+1/-1", 			cless_ability, priorities[4],
							"-1/+1", 			cless_ability, priorities[5]);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_HASTE || choice == CHOICE_TRAMPLE || choice == CHOICE_INDESTRUCTIBLE ){
			if( charge_mana_for_activated_ability(player, card, MANACOST_G(1)) ){
				if( instance->targets[1].player < -1 ){
					instance->targets[1].player = 0;
				}
				if( instance->targets[1].card < -1 ){
					instance->targets[1].card = 0;
				}
				instance->targets[1].player |= (choice == CHOICE_TRAMPLE ? KEYWORD_TRAMPLE : 0);
				instance->targets[1].card |= (choice == CHOICE_HASTE ? SP_KEYWORD_HASTE : 0);
				instance->targets[1].card |= (choice == CHOICE_INDESTRUCTIBLE ? SP_KEYWORD_INDESTRUCTIBLE : 0);
			}
		}
		if( choice == CHOICE_P1_M1 || choice == CHOICE_M1_P1 ){
			charge_mana_for_activated_ability(player, card, MANACOST_X(1));
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		if( choice == CHOICE_HASTE ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									0, 0, 0, SP_KEYWORD_HASTE );
		}
		if( choice == CHOICE_TRAMPLE ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									0, 0, KEYWORD_TRAMPLE, 0 );
		}
		if( choice == CHOICE_INDESTRUCTIBLE ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									0, 0, 0, SP_KEYWORD_INDESTRUCTIBLE);
		}
		if (choice == CHOICE_P1_M1 ){
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
										instance->parent_controller, instance->parent_card, 1, -1);
		}
		if (choice == CHOICE_M1_P1 ){
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
										instance->parent_controller, instance->parent_card, -1, 1);
		}
	}

	return 0;
}

int card_traumatic_visions(int player, int card, event_t event)
{
/* CARD_ID_TRAUMATIC_VISIONS	10217
Traumatic Visions	|3|U|U
Instant
Counter target spell.
Basic landcycling |1|U (|1|U, Discard this card: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library.)*/

  int cyc = basic_landcycling(player, card, event, MANACOST_XU(1,1));
  return cyc ? cyc : card_counterspell(player, card, event);
}

int card_tukatongue_thallid(int player, int card, event_t event){
	/* Tukatongue Thallid	|G
	 * Creature - Fungus 1/1
	 * When ~ dies, put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		generate_token_by_id(player, card, CARD_ID_SAPROLING);
	}
	return 0;
}

// valeron outlander --> vanilla

// vedalken outlander --> esper cormorants


int card_voices_from_the_void(int player, int card, event_t event){
/* CARD_ID_VOICES_FROM_THE_VOID	10227
Voices from the Void	|4|B
Sorcery
Domain - Target player discards a card for each basic land type among lands you control. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			multidiscard(instance->targets[0].player, count_domain(player, card), 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_volcanic_fallout(int player, int card, event_t event){
/* CARD_ID_VOLCANIC_FALLOUT	10228
Volcanic Fallout	|1|R|R
Instant
~ can't be countered.
~ deals 2 damage to each creature and each player. */

	cannot_be_countered(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		new_damage_all(player, card, ANYBODY, 2, NDA_PLAYER_TOO | NDA_ALL_CREATURES, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_voracious_dragon(int player, int card, event_t event){
	/* Voracious Dragon	|3|R|R	0x2008392
	 * Creature - Dragon 4/4
	 * Flying
	 * Devour 1
	 * When ~ enters the battlefield, it deals damage to any target equal to twice the number of Goblins it devoured. */
	devour(player, card, event, 1);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		// Yes, it still targets even if it didn't devour any goblins.
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		if( can_target(&td) && pick_target(&td, "TARGET_ANY") ){
			damage_target0(player, card, get_card_instance(player, card)->targets[1].player*2);
		}
		get_card_instance(player, card)->number_of_targets = 0;
	}

	return 0;
}

int card_wall_of_reverence(int player, int card, event_t event){
/* CARD_ID_WALL_OF_REVERENCE	10230
Wall of Reverence	|3|W
Creature - Spirit Wall 1/6
Defender, flying
At the beginning of your end step, you may gain life equal to the power of target creature you control. */

	if( trigger_condition == TRIGGER_EOT && current_turn == player){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance( player, card );

		if( eot_trigger_mode(player, card, event, player, can_target(&td) ? RESOLVE_TRIGGER_AI(player) : 0) ){
			if( pick_target(&td, "ASHNODS_BATTLEGEAR") ){	// "Select target creature you control."
				gain_life(player, get_power(instance->targets[0].player, instance->targets[0].card) );
			}
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_worldly_counsel(int player, int card, event_t event){
/* CARD_ID_WORLDLY_COUNSEL	5194
Worldly Counsel	|1|U
Instant
Domain - Look at the top X cards of your library, where X is the number of basic land types among lands you control.
Put one of those cards into your hand and the rest on the bottom of your library in any order. */

	if( event == EVENT_RESOLVE_SPELL ){
		impulse_effect(player, count_domain(player, card), 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

// zombie outlander --> vanilla



