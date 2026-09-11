#include "manalink.h"

// global functions
static int join_forces(int player, int prevent_if_duh){
	charge_mana(player, COLOR_COLORLESS, -1);
	int amount = x_value;
	if (!(prevent_if_duh && duh_mode(1-player))){
		charge_mana(1-player, COLOR_COLORLESS, -1);
		amount += x_value;
	}
	return amount;
}

static int vow(int player, int card, event_t event, int pow, int tou, int key, int s_key){

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player != player ){
			cannot_attack(instance->damage_target_player, instance->damage_target_card, event);
		}
	}

	return generic_aura(player, card, event, player, pow, tou, key, s_key, 0, 0, 0);
}

// cards
int card_acorn_catapult(int player, int card, event_t event){
	/* Acorn Catapult	|4	0x2006506
	 * Artifact
	 * |1, |T: ~ deals 1 damage to any target. That permanent's controller or that player creates a 1/1 |Sgreen Squirrel creature token. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SQUIRREL, &token);
			token.pow = token.tou = 1;
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
			token.t_player = instance->targets[0].player;
			generate_token(&token);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_ANY");
}

int card_alliance_of_arms(int player, int card, event_t event){
/* Alliance of Arms	|W
 * Sorcery
 * Join forces - Starting with you, each player may pay any amount of mana. Each player puts X 1/1 |Swhite Soldier creature tokens
 *		onto the battlefield, where X is the total amount of mana paid this way. */
	if( event == EVENT_RESOLVE_SPELL ){
		int amount = join_forces(player, 0);
		if( amount > 0 ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SOLDIER, &token);
			token.qty = amount;
			token.pow = token.tou = 1;
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
			generate_token(&token);

			token.t_player = 1-player;
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_animar_soul_of_elements(int player, int card, event_t event){
/* CARD_ID_ANIMAR_SOUL_OF_ELEMENTS	11751
Animar, Soul of Elements	|U|R|G
Legendary Creature - Elemental 1/1
Protection from |Swhite and from |Sblack
Whenever you cast a creature spell, put a +1/+1 counter on ~.
Creature spells you cast cost |1 less to cast for each +1/+1 counter on ~. */

	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);

		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			add_1_1_counter(player, card);
		}
	}

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card) ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			COST_COLORLESS -= count_1_1_counters(player, card);
		}
	}

	return 0;
}

int card_archangel_of_strife(int player, int card, event_t event){
/* CARD_ID_ARCHANGEL_OF_STRIFE	11721
Archangel of Strife	|5|W|W
Creature - Angel 6/6
Flying
As ~ enters the battlefield, each player chooses war or peace.
Creatures controlled by players who chose war get +3/+0.
Creatures controlled by players who chose peace get +0/+3. */

	if( comes_into_play(player, card, event) ){
		card_instance_t *instance = get_card_instance(player, card);
		int i;
		for(i=0; i<2; i++){
			int ai_choice = 0;
			if( count_permanents_by_type(1-i, TYPE_CREATURE) < count_permanents_by_type(i, TYPE_CREATURE)){
				ai_choice = 1;
			}
			int choice = do_dialog(i, player, card, -1, -1, " War\n Peace", ai_choice);
			instance->targets[i+1].player = choice == 0 ? 3 : 0;
			instance->targets[i+1].card = choice == 1 ? 3 : 0;
		}
	}

	if( get_card_instance(player, card)->targets[1].player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		boost_creature_type(player, card, event, -1, instance->targets[1].player, instance->targets[1].card, 0, player == 0 ? BCT_CONTROLLER_ONLY|BCT_INCLUDE_SELF : BCT_OPPONENT_ONLY);
		boost_creature_type(player, card, event, -1, instance->targets[2].player, instance->targets[2].card, 0, player == 1 ? BCT_CONTROLLER_ONLY|BCT_INCLUDE_SELF : BCT_OPPONENT_ONLY);
	}

	return 0;
}

int card_avatar_of_slaughter(int player, int card, event_t event){
/* CARD_ID_AVATAR_OF_SLAUGHTER	11738
Avatar of Slaughter	|6|R|R
Creature - Avatar 8/8
All creatures have double strike and attack each turn if able. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_ABILITIES && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			event_result |= KEYWORD_FIRST_STRIKE;
			event_result |= KEYWORD_DOUBLE_STRIKE;
		}
		all_must_attack_if_able(current_turn, event, -1);
	}


	return 0;
}

int card_basandra_battle_seraph(int player, int card, event_t event){
/*
Basandra, Battle Seraph |3|R|W
Legendary Creature — Angel 4/4
Flying
Players can't cast spells during combat.
{R}: Target creature attacks this turn if able.
*/
	check_legend_rule(player, card, event);

	if( event == EVENT_MODIFY_COST_GLOBAL && current_phase > PHASE_MAIN1 && current_phase < PHASE_MAIN2 ){
		if( in_play(player, card) && ! is_humiliated(player, card) ){
			infinite_casting_cost();
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (current_turn != player ? 15 : -15);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_MUST_ATTACK);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_CREATURE");
}

int card_celestial_force(int player, int card, event_t event){
/* CARD_ID_CELESTIAL_FORCE	11722
Celestial Force	|5|W|W|W
Creature - Elemental 7/7
At the beginning of each upkeep, you gain 3 life. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		gain_life(player, 3);
	}

	return 0;
}

int card_champions_helm(int player, int card, event_t event){
/* CARD_ID_CHAMPIONS_HELM	11768
Champion's Helm	|3
Artifact - Equipment
Equipped creature gets +2/+2.
As long as equipped creature is legendary, it has hexproof. (It can't be the target of spells or abilities your opponents control.)
Equip |1 */

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( event == EVENT_ABILITIES && is_legendary(instance->damage_target_player, instance->damage_target_card) ){
			hexproof(instance->targets[8].player, instance->targets[8].card, event);
		}
	}

	return vanilla_equipment(player, card, event, 1, 2, 2, 0, 0);
}

int card_chaos_warp(int player, int card, event_t event){
/* Chaos Warp	|2|R
 * Instant
 * The owner of target permanent shuffles it into his or her library, then reveals the top card of his or her library.
 * If it's a permanent card, he or she puts it onto the battlefield. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int t_player = get_owner(instance->targets[0].player, instance->targets[0].card);
			shuffle_into_library(instance->targets[0].player, instance->targets[0].card);
			int *deck = deck_ptr[t_player];
			if( deck[0] != -1 ){
				reveal_card_iid(player, card, deck[0]);
				if( is_what(-1, deck[0], TYPE_PERMANENT) ){
					put_into_play_a_card_from_deck(t_player, t_player, 0);
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
}

int card_collective_voyage(int player, int card, event_t event){
/* CARD_ID_COLLECTIVE_VOYAGE	11745
Collective Voyage	|G
Sorcery
Join forces - Starting with you, each player may pay any amount of mana. Each player searches his or her library for up to X basic land cards,
where X is the total amount of mana paid this way, puts them onto the battlefield tapped, then shuffles his or her library. */

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = join_forces(player, 0);
		APNAP(p,{
					test_definition_t test;
					new_default_test_definition(&test, TYPE_LAND, "Select a basic land card.");
					test.subtype = SUBTYPE_BASIC;
					test.qty = amount;
					new_global_tutor(p, p, TUTOR_FROM_DECK, TUTOR_PLAY_TAPPED, 0, AI_FIRST_FOUND, &test);
				};
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_command_tower(int player, int card, event_t event){
/* CARD_ID_COMMAND_TOWER	11769
Command Tower	""
Land
|T: Add to your mana pool one mana of any color in your commander's color identity. */

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

	if( event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		if( instance->info_slot > 0 ){
			return mana_producer(player, card, event);
		}
	}

	return 0;
}

int card_crescendo_of_war(int player, int card, event_t event){
/* Crescendo of War	|3|W
 * Enchantment
 * At the beginning of each upkeep, put a strife counter on ~.
 * Attacking creatures get +1/+0 for each strife counter on ~.
 * Blocking creatures you control get +1/+0 for each strife counter on ~. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counter(player, card, COUNTER_STRIFE);
	}

	if( event == EVENT_POWER ){
		int ok = 0;
		if (is_attacking(affected_card_controller, affected_card)){
			ok = 1;
		}
		else if (affected_card_controller == player && player != current_turn){
			card_instance_t* aff = get_card_instance(affected_card_controller, affected_card);
			if ((aff->state & STATE_BLOCKING) && aff->blocking != 255){
				ok = 1;
			}
		}
		if (ok){
			event_result += count_counters(player, card, COUNTER_STRIFE);
		}
	}

	return global_enchantment(player, card, event);
}

int card_damia_sage_of_stone(int player, int card, event_t event ){
/* CARD_ID_DAMIA_SAGE_OF_STONE	11753
Damia, Sage of Stone	|4|G|U|B
Legendary Creature - Gorgon Wizard 4/4
Deathtouch
Skip your draw step.
At the beginning of your upkeep, if you have fewer than seven cards in hand, draw cards equal to the difference. */

	check_legend_rule(player, card, event);

	deathtouch(player, card, event);

	skip_your_draw_step(player, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int amount = 7-hand_count[player];
		if( amount > 0 ){
			draw_cards(player, amount);
		}
	}

	return 0;
}

int card_death_by_dragons(int player, int card, event_t event){
	/* Death by Dragons	|4|R|R
	 * Sorcery
	 * Each player other than target player puts a 5/5 |Sred Dragon creature token with flying onto the battlefield. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_DRAGON, &token);
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_RED);
			token.pow = token.tou = 5;
			token.t_player = 1-instance->targets[0].player;
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_edric_spymaster_of_trest(int player, int card, event_t event ){
/* CARD_ID_EDRIC_SPYMASTER_OF_TREST	11754
Edric, Spymaster of Trest	|1|G|U
Legendary Creature - Elf Rogue 2/2
Whenever a creature deals combat damage to one of your opponents, its controller may draw a card. */

	check_legend_rule(player, card, event);

	return card_coastal_piracy(player, card, event);
}

int card_flusterstorm(int player, int card, event_t event ){
/* Flusterstorm	|U
 * Instant
 * Counter target instant or sorcery spell unless its controller pays |1.
 * Storm */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_SPELL);

	if( event == EVENT_CAN_CAST ){
		return counterspell(player, card, event, &td, 0);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		counterspell(player, card, event, &td, 0);
		instance->info_slot = 0;
		if( ! check_special_flags(player, card, SF_NOT_CAST) ){
			instance->info_slot = get_storm_count();
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		int amount = 1 + instance->info_slot;
		counterspell_resolve_unless_pay_x(player, card, &td, 0, amount);
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

/* CARD_ID_FOOTBOTTOM_FEAST	9144 => has Gravepurge 's code pointer => Bone Harvest
Footbottom Feast	|2|B
Instant
Put any number of target creature cards from your graveyard on top of your library.
Draw a card. */

int card_dread_cacodemon(int player, int card, event_t event){
/* CARD_ID_DREAD_CACODEMON	11732
Dread Cacodemon	|7|B|B|B
Creature - Demon 8/8
When ~ enters the battlefield, if you cast it from your hand, destroy all creatures your opponents control, then tap all other creatures you control. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		instance->info_slot = not_played_from_hand(player, card);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->info_slot != 1 ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.not_me = 1;
			APNAP(p, {new_manipulate_all(player, card, p, &this_test, KILL_DESTROY);};);
		}
	}

	return 0;
}

int card_ghave_guru_of_spores(int player, int card, event_t event){
/* Ghave, Guru of Spores	|2|B|G|W
 * Legendary Creature - Fungus Shaman 0/0
 * ~ enters the battlefield with five +1/+1 counters on it.
 * |1, Remove a +1/+1 counter from a creature you control: Put a 1/1 |Sgreen Saproling creature token onto the battlefield.
 * |1, Sacrifice a creature: Put a +1/+1 counter on target creature. */

	check_legend_rule(player, card, event);

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 5);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_SAPROLING = 1,
		CHOICE_P1_P1
	};

	target_definition_t td;
	base_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_P1_P1;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE, MANACOST_X(1), 0, &td1, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int ab1 = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_X(1), 0, &td, NULL);
		int ab2 = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE, MANACOST_X(1),
												0, &td1, NULL);
		int p1 = current_phase < PHASE_DECLARE_BLOCKERS ? 5 : 1;
		int p2 = current_phase == PHASE_AFTER_BLOCKING ? 10 : 1;
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Generate Saproling", 			ab1, p1,
							"+1/+1 to target creatures", 	ab2, p2);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_SAPROLING  ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(1), 0,
										&td, "Select a creature you control with a +1/+1 counter.");
			if( spell_fizzled != 1 ){
				remove_1_1_counter(instance->targets[0].player, instance->targets[0].card);
				instance->number_of_targets = 0;
			}
		}

		if( choice == CHOICE_P1_P1 ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE, MANACOST_X(1), 0,
												&td1, "TARGET_CREATURE");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAPROLING ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING);
		}
		if( instance->info_slot == CHOICE_P1_P1 && valid_target(&td1) ){
			add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_homeward_path(int player, int card, event_t event){
/* Homeward Path	""
 * Land
 * |T: Add |C to your mana pool.
 * |T: Each player gains control of all creatures he or she owns. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_GET_BACK_YOUR_PERMANENTS
	};

	if (event == EVENT_ACTIVATE){
		int choice = CHOICE_MANA;
		int ab2 = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		if( !paying_mana() && ab2 ){
			int found = 0;
			if (player == AI || ai_is_speculating == 1){
				int c;
				for (c = 0; c < active_cards_count[1 - player] ; ++c){
					if (in_play(1 - player, c) && is_what(1 - player, c, TYPE_CREATURE) && is_stolen(1-player, c)){
						found = 1;
						break;
					}
				}
				if( ! found ){
					ai_modifier -= 24;
				}
			}
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 5,
							"Get back your permanents", ab2, found ? 10 : 1);
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_GET_BACK_YOUR_PERMANENTS ){
			tapped_for_mana_color = -1;
			instance->state |= STATE_TAPPED;
			if (!charge_mana_for_activated_ability(player, card, MANACOST0) ){
				instance->state &= ~STATE_TAPPED;
				cancel = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_GET_BACK_YOUR_PERMANENTS ){
			APNAP(p, {get_back_your_permanents(p, -1, TYPE_CREATURE);};);
		}
	}

	return 0;
}

int card_hornet_queen(int player, int card, event_t event){
/* CARD_ID_HORNET_QUEEN	11746
Hornet Queen	|4|G|G|G
Creature - Insect 2/2
Flying
Deathtouch (Any amount of damage this deals to a creature is enough to destroy it.)
When ~ enters the battlefield, create four 1/1 |Sgreen Insect creature tokens with flying and deathtouch. */

	deathtouch(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_INSECT, &token);
		token.qty = 4;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		token.key_plus = KEYWORD_FLYING;
		token.s_key_plus = SP_KEYWORD_DEATHTOUCH;
		generate_token(&token);
	}

	return 0;
}

// hydra omnivore --> vanilla

int card_kaalia_of_the_vast(int player, int card, event_t event)
{
/*
Kaalia of the Vast |1|W|B|R
Legendary Creature — Human Cleric 2/2
Flying
Whenever Kaalia of the Vast attacks an opponent, you may put an Angel, Demon, or Dragon creature card from your hand
	onto the battlefield tapped and attacking that opponent.
*/
	check_legend_rule(player, card, event);

	if (declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player) | DAT_ATTACKS_PLAYER, player, card))
	{
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select an Angel, Demon, or Dragon creature");
		test.subtype = SUBTYPE_ANGEL;
		test.sub2 = SUBTYPE_DEMON;
		test.sub3 = SUBTYPE_DRAGON;
		test.subtype_flag = F2_MULTISUBTYPE;
		test.zone = TARGET_ZONE_HAND;

		if( check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test) ){
			int result = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &test);
			if( result ){
				add_state(player, result, STATE_TAPPED | STATE_ATTACKING);
				put_into_play(player, result);
			}
		}
	}

	return 0;
}

int card_karador_ghost_chieftain(int player, int card, event_t event){
	/*
	  Karador, Ghost Chieftain English |5|W|B|G
	  Legendary Creature - Centaur Spirit 3/4
	  Karador, Ghost Chieftain costs {1} less to cast for each creature card in your graveyard.
	  During each of your turns, you may cast one creature card from your graveyard.
	*/
	card_instance_t *instance = get_card_instance(player, card);

	check_legend_rule(player, card, event);

	if( event == EVENT_MODIFY_COST && affect_me(player, card) && count_graveyard_by_type(player, TYPE_CREATURE) > 0 ){
		int minus = count_graveyard_by_type(player, TYPE_CREATURE);
		if( minus > 5 ){
			minus = 5;
		}
		COST_COLORLESS -= minus;
	}

	if( event == EVENT_CAN_ACTIVATE && ! is_humiliated(player, card) && current_turn == player ){
		if( instance->info_slot != 66 ){
			int result = 0;
			int i;
			const int *grave = get_grave(player);
			for(i=0; i<count_graveyard(player); i++){
				if( is_what(-1, grave[i], TYPE_CREATURE) && has_mana_to_cast_iid(player, event, grave[i]) &&
					(is_what(-1, grave[i], TYPE_INSTANT | TYPE_INTERRUPT) || can_sorcery_be_played(player, event))
				  ){
					int cpt = can_legally_play_iid(player, grave[i]);
					if( cpt == 99 ){
						return 99;
					}
					if( cpt ){
						result = cpt;
					}
				}
			}
			return result;
		}
	}

	if( event == EVENT_ACTIVATE ){
		int playable[2][count_graveyard(player)];
		int pc = 0;
		int i;
		const int *grave = get_grave(player);
		for(i=0; i<count_graveyard(player); i++){
			if( is_what(-1, grave[i], TYPE_CREATURE) && has_mana_to_cast_iid(player, event, grave[i]) &&
				(is_what(-1, grave[i], TYPE_INSTANT | TYPE_INTERRUPT) || can_sorcery_be_played(player, event))
			  ){
				if( can_legally_play_iid(player, grave[i]) ){
					playable[0][pc] = grave[i];
					playable[1][pc] = i;
					pc++;
				}
			}
		}
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card to play.");
		int selected = select_card_from_zone(player, player, playable[0], pc, 0, AI_MAX_VALUE, -1, &test);
		if( selected != -1 && charge_mana_from_id(player, -1, event, cards_data[playable[0][selected]].id)){
			instance->info_slot = 66;
			play_card_in_grave_for_free(player, player, playable[1][selected]); // Not really for free, but cards that check it usually allow cancelling, which would be disastrous here.  Should probably factor out the entire charge-mana/remove-from-grave/play-spell sequence.
			cant_be_responded_to = 1;	// The card cast from graveyard will be respondable to, but the extra activation effect from Karador won't be
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_CLEANUP ){
		instance->info_slot = 0;
	}

	return 0;
}

int card_magmatic_force(int player, int card, event_t event){
	/* Magmatic Force	|5|R|R|R	0x200654c
	 * Creature - Elemental 7/7
	 * At the beginning of each upkeep, ~ deals 3 damage to any target. */
	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		instance->number_of_targets = 0;

		if( can_target(&td) && new_pick_target(&td, "TARGET_ANY", 0, 0) ){
			damage_target0(player, card, 3);
		}
	}

	return 0;
}

int card_mana_charged_dragon(int player, int card, event_t event)
{
/* CARD_ID_MANA_CHARGED_DRAGON	11742
Mana-Charged Dragon	|4|R|R
Creature - Dragon 5/5
Flying, trample
Join forces - Whenever ~ attacks or blocks, each player starting with you may pay any amount of mana.
~ gets +X/+0 until end of turn, where X is the total amount of mana paid this way. */

  if (blocking(player, card, event) || declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  int amt = join_forces(player, 1);
	  if (amt > 0)
		pump_until_eot(player, card, player, card, amt, 0);
	}

  return 0;
}

static int martyrs_bond_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_MARTYRS_BOND ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY && affected_card_controller == player ){
		card_instance_t *instance= get_card_instance(player, card);
		if( in_play(affected_card_controller, affected_card) ){
			int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
			if( kill_code ){
				if( instance->damage_target_player > -1 ){
					int p = instance->damage_target_player;
					int c = instance->damage_target_card;
					if( affect_me(p, c) ){
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
					}
				}
			}
			if( is_what(affected_card_controller, affected_card, TYPE_PERMANENT) &&
				! is_what(affected_card_controller, affected_card, TYPE_LAND) )
			{
				if( is_this_dying(affected_card_controller, affected_card) ){
					if( instance->targets[11].player < 0 ){
						instance->targets[11].player = 0;
					}
					int pointer = instance->targets[11].player;
					if( pointer < 10 ){
						instance->targets[pointer].card = get_type(affected_card_controller, affected_card);
						instance->targets[11].player++;
					}
				}
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( amount ){
			card_instance_t *instance= get_card_instance(player, card);
			int i;
			for(i=0; i<amount; i++){
				test_definition_t test;
				default_test_definition(&test, instance->targets[i].card);
				new_sacrifice(player, card, 1-player, SAC_NO_CANCEL | SAC_CAUSED, &test);
			}
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

int card_martyrs_bond(int player, int card, event_t event){
/*
Martyr's Bond |4|W|W
Enchantment
Whenever Martyr's Bond or another nonland permanent you control is put into a graveyard from the battlefield,
	each opponent sacrifices a permanent that shares a card type with it.
*/

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)martyrs_bond_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &martyrs_bond_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY && affected_card_controller == player ){
		if( in_play(affected_card_controller, affected_card) &&
			is_what(affected_card_controller, affected_card, TYPE_PERMANENT) &&
			! is_what(affected_card_controller, affected_card, TYPE_LAND) )
		{
			if( is_this_dying(affected_card_controller, affected_card) ){
				card_instance_t *instance= get_card_instance(player, card);
				if( instance->targets[11].player < 0 ){
					instance->targets[11].player = 0;
				}
				int pointer = instance->targets[11].player;
				if( pointer < 10 ){
					instance->targets[pointer].card = get_type(affected_card_controller, affected_card);
					instance->targets[11].player++;
				}
			}
		}
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( amount ){
			card_instance_t *instance= get_card_instance(player, card);
			int i;
			for(i=0; i<amount; i++){
				test_definition_t test;
				default_test_definition(&test, instance->targets[i].card);
				new_sacrifice(player, card, 1-player, SAC_NO_CANCEL | SAC_CAUSED, &test);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_minds_aglow(int player, int card, event_t event){
/* CARD_ID_MINDS_AGLOW	11728
Minds Aglow	|U
Sorcery
Join forces - Starting with you, each player may pay any amount of mana. Each player draws X cards, where X is the total amount of mana paid this way.*/

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = join_forces(player, 0);
		draw_cards(player, amount);
		draw_cards(1-player, amount);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_nin_the_pain_artist(int player, int card, event_t event){
/* CARD_ID_NIN_THE_PAIN_ARTIST	11759
Nin, the Pain Artist	|U|R
Legendary Creature - Vedalken Wizard 1/1
|X|U|R, |T: ~ deals X damage to target creature. That creature's controller draws X cards. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		   if( valid_target(&td)  ){
			   damage_target0(player, card, instance->info_slot);
			   draw_cards(instance->targets[0].player, instance->info_slot);
		   }
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XUR(-1, 1, 1), 0, &td, "TARGET_CREATURE");
}

int card_riddlekeeper(int player, int card, event_t event)
{
/* CARD_ID_RIDDLEKEEPER	11729
Riddlekeeper	|2|U
Creature - Homunculus 1/4
Whenever a creature attacks you or a planeswalker you control, that creature's controller puts the top two cards of his or her library into his or her graveyard.*/

  if (declare_attackers_trigger(player, card, event, DAT_SEPARATE_TRIGGERS, 1-player, -1))
	mill(1-player, 2);

  return 0;
}

int card_riku_of_two_reflections(int player, int card, event_t event){

	/* Riku of Two Reflections	|2|U|R|G
	 * Legendary Creature - Human Wizard 2/2
	 * Whenever you cast an instant or sorcery spell, you may pay |U|R. If you do, copy that spell. You may choose new targets for the copy.
	 * Whenever another nontoken creature enters the battlefield under your control, you may pay |G|U. If you do, put a token that's a copy of that creature
	 * onto the battlefield. */

	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller &&
		has_mana_multi(player, MANACOST_UR(1, 1))
	  ){
		test_definition_t test;
		default_test_definition(&test, TYPE_SPELL);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test) &&
			charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, MANACOST_UR(1,1))
		  ){
			copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
		}
	}

	if(	trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && player == reason_for_trigger_controller ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.not_me = 1;
		test.type_flag = F1_NO_TOKEN;

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		if( has_mana_multi(player, MANACOST_UG(1, 1)) &&
			charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, MANACOST_GU(1,1)))
		{
			token_generation_t token;
			copy_token_definition(player, card, &token, trigger_cause_controller, trigger_cause);
			generate_token(&token);
		}
	}

	return 0;
}

int card_ruhan_of_the_fomori(int player, int card, event_t event ){
/* CARD_ID_RUHAN_OF_THE_FOMORI	11761
Ruhan of the Fomori	|1|R|W|U
Legendary Creature - Giant Warrior 7/7
At the beginning of combat on your turn, choose an opponent at random. ~ attacks that player this combat if able. */

	check_legend_rule(player, card, event);

	attack_if_able(player, card, event);

	return 0;
}

int card_scavenging_ooze(int player, int card, event_t event)
{
 /* Scavenging Ooze	|1|G
  * Creature - Ooze 2/2
  * |G: Exile target card from a graveyard. If it was a creature card, put a +1/+1 counter on ~ and you gain 1 life. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		int pump = 0;
		if( can_use_activated_abilities(player, card) ){
			int i;
			for(i=0; i<2; i++){
				if( ! graveyard_has_shroud(i) ){
					pump = count_graveyard_by_type(i, TYPE_CREATURE);
				}
			}
			while( ! has_mana_for_activated_ability(player, card, MANACOST_G(pump)) ){
					pump--;
			}
		}
		return pump;
	}


    test_definition_t test;
	default_test_definition(&test, TYPE_ANY);

	if (event == EVENT_CAN_ACTIVATE){
		return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL, TYPE_ANY, &test,
																GS_GRAVE_RECYCLER_BOTH_GRAVES);
	}

	if (event == EVENT_ACTIVATE)
	{
		if( player == AI ){
			generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL, TYPE_CREATURE, NULL,
															GS_GRAVE_RECYCLER_BOTH_GRAVES);
			if( cancel == 1 ){
				cancel = 0;
				return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL, TYPE_ANY,
																		&test, GS_GRAVE_RECYCLER_BOTH_GRAVES);
			}
		}
		else{
			return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL, TYPE_ANY, &test,
																	GS_GRAVE_RECYCLER_BOTH_GRAVES);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			int pow = is_what(-1, get_grave(instance->targets[0].player)[selected], TYPE_CREATURE) ? 1 : 0;
			rfg_card_from_grave(instance->targets[0].player, selected);
			if( pow ){
				add_1_1_counter(instance->parent_controller, instance->parent_card);
				gain_life(player, 1);
			}
		}
	}

	return 0;
}

int card_scythe_specter(int player, int card, event_t event){
/*
Scythe Specter |4|B|B
Creature — Specter 4/4
Flying
Whenever Scythe Specter deals combat damage to a player, each opponent discards a card.
Each player who discarded a card with the highest converted mana cost among cards discarded this way loses life equal to that
	converted mana cost.
*/
	if( has_combat_damage_been_inflicted_to_a_player(player, card, event) && hand_count[1-player] > 0 ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a card to discard.");

		int result = new_select_a_card(1-player, 1-player, TUTOR_FROM_HAND, 1, AI_MIN_CMC, -1, &test);
		if( result > -1 ){
			int cmc = get_cmc(1-player, result);
			new_discard_card(1-player, result, 0, player);
			lose_life(1-player, cmc);
		}
	}

	return 0;
}

int card_sewer_nemesis(int player, int card, event_t event){
	/* Sewer Nemesis	|3|B
	 * Creature - Horror 100/100
	 * As ~ enters the battlefield, choose a player.
	 * ~'s power and toughness are each equal to the number of cards in the chosen player's graveyard.
	 * Whenever the chosen player casts a spell, that player puts the top card of his or her library into his or her graveyard. */

	if (card == -1){
		return 0;	// no player chosen if this isn't on the bf
	}

	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;
		td.illegal_abilities = 0;

		pick_target(&td, "TARGET_PLAYER");
		get_card_instance(player, card)->targets[1] = get_card_instance(player, card)->targets[0];
		get_card_instance(player, card)->number_of_targets = 0;
	}

	if( get_card_instance(player, card)->targets[1].player > -1 ){
		int t_player = get_card_instance(player, card)->targets[1].player;
		if( new_specific_spell_played(player, card, event, t_player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
			mill(t_player, 1);
		}
		if ( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && !is_humiliated(player, card) ){
			event_result += count_graveyard(t_player);
		}
	}

	return 0;
}

int card_shared_trauma(int player, int card, event_t event){
/* CARD_ID_SHARED_TRAUMA	11735
Shared Trauma	|B
Sorcery
Join forces - Starting with you, each player may pay any amount of mana. Each player puts the top X cards of his or her library into his or her graveyard,
where X is the total amount of mana paid this way. */

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = join_forces(player, 0);
		mill(player, amount);
		mill(1-player, amount);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static int effect_skullbriar_the_walking_grave(int player, int card, event_t event)
{
  return 0;	// just a marker
}
int card_skullbriar_the_walking_grave(int player, int card, event_t event)
{
  /* Skullbriar, the Walking Grave	|B|G
   * Legendary Creature - Zombie Elemental 1/1
   * Haste
   * Whenever ~ deals combat damage to a player, put a +1/+1 counter on it.
   * Counters remain on Skullbriar as it moves to any zone other than a player's hand or library. */

  check_legend_rule(player, card, event);

  haste(player, card, event);

  card_instance_t* instance;
  if (event == EVENT_RESOLVE_SPELL)
	{
	  int p, c;
	  p = get_owner(player, card);
	  for (c = 0; c < active_cards_count[p]; ++c)
		if ((instance = in_play(p, c)) && instance->internal_card_id == LEGACY_EFFECT_CUSTOM && instance->info_slot == (int)effect_skullbriar_the_walking_grave)
		  {
			copy_counters(player, card, p, c, 1);
			kill_card(p, c, KILL_EXILE);
		  }
	}

  if (trigger_condition == TRIGGER_LEAVE_PLAY
	  && (instance = get_card_instance(player, card))
	  && instance->kill_code > 0 && instance->kill_code <= KILL_EXILE
	  && leaves_play(player, card, event))
	{
	  int legacy, p = get_owner(player, card);
	  if (p != player)
		legacy = create_legacy_effect_for_opponent(player, card, &effect_skullbriar_the_walking_grave, -1, -1);
	  else
		legacy = create_legacy_effect(player, card, &effect_skullbriar_the_walking_grave);

	  if (legacy != -1)
		copy_counters(p, legacy, player, card, 1);
	}

  return card_slith_predator(player, card, event);
}


int card_soul_snare(int player, int card, event_t event){
/*
Soul Snare |W
Enchantment
{W}, Sacrifice Soul Snare: Exile target creature that's attacking you or a planeswalker you control.
*/
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME | GAA_LITERAL_PROMPT, MANACOST_W(1), 0,
									&td, "Select target attacking creature.");
}

int card_spell_crumple(int player, int card, event_t event){
/* CARD_ID_SPELL_CRUMPLE	11730
Spell Crumple	|1|U|U
Instant
Counter target spell. If that spell is countered this way, put it on the bottom of its owner's library instead of into that player's graveyard.
Put ~ on the bottom of its owner's library. */

	if( event != EVENT_RESOLVE_SPELL ){
		return counterspell(player, card, event, NULL, 0);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card );
		set_flags_when_spell_is_countered(player, card, instance->targets[0].player, instance->targets[0].card);
		put_on_bottom_of_deck(instance->targets[0].player, instance->targets[0].card);
		put_on_bottom_of_deck(player, card);
	}

	return 0;
}

int card_syphon_flesh(int player, int card, event_t event){
	/* Syphon Flesh	|4|B
	 * Sorcery
	 * Each other player sacrifices a creature. You put a 2/2 |Sblack Zombie creature token onto the battlefield for each creature sacrificed this way. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		int result = new_sacrifice(player, card, 1-player, SAC_NO_CANCEL | SAC_RETURN_CHOICE | SAC_CAUSED, &this_test);
		if( result ){
			generate_token_by_id(player, card, CARD_ID_ZOMBIE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_tariel_reckoner_of_souls(int player, int card, event_t event){
/*
Tariel, Reckoner of Souls |4|W|B|R
Legendary Creature — Angel 4/7
Flying, vigilance
{T}: Choose a creature card at random from target opponent's graveyard. Put that card onto the battlefield under your control.
*/
	check_legend_rule(player, card, event);

	vigilance(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tutor_random_permanent_from_grave(instance->parent_controller, instance->parent_card, instance->targets[0].player,
												TUTOR_PLAY, TYPE_CREATURE, 1, REANIMATE_DEFAULT);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_ONLY_TARGET_OPPONENT | GAA_UNTAPPED, MANACOST0, 0, &td, NULL);
}

int card_the_mimeoplasm(int player, int card, event_t event)
{
  /* The Mimeoplasm	|2|G|U|B
   * Legendary Creature - Ooze 0/0
   * As ~ enters the battlefield, you may exile two creature cards from graveyards. If you do, it enters the battlefield as a copy of one of those cards with a
   * number of additional +1/+1 counters on it equal to the power of the other card. */

  if (event == EVENT_CAST_SPELL && affect_me(player, card) && IS_AI(player) && count_graveyard_by_type(ANYBODY, TYPE_CREATURE) < 2)
	ai_modifier += (player == AI ? -1000 : 1000);

  int iid = -1;
  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  instance->state |= STATE_OUBLIETTED;

	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card to copy.");
	  test.id = 904; // face-down card (draw-a-card effect)
	  test.id_flag = DOESNT_MATCH;

	  if (select_target_from_either_grave(player, card, SFG_NOTARGET, AI_MAX_VALUE, AI_MAX_VALUE, &test, 0, 1) != -1)
		{
		  iid = turn_card_in_grave_face_down(instance->targets[0].player, instance->targets[1].player);

		  strcpy(test.message, "Select a creature card for counters.");
		  int iid2 = select_target_from_either_grave(player, card, SFG_NOTARGET, AI_MAX_VALUE, AI_MAX_VALUE, &test, 2, 3);
		  turn_card_in_grave_face_up(instance->targets[0].player, instance->targets[1].player, iid);
		  if (iid2 == -1)
			iid = -1;
		  else
			{
			  ++hack_silent_counters;
			  add_1_1_counters(player, card, get_base_power_iid(instance->targets[2].player, iid2));
			  --hack_silent_counters;

			  // If both from the same graveyard, remove the one with the higher index first
			  int first, second;
			  if (instance->targets[0].player == instance->targets[2].player)
				{
				  first = MAX(instance->targets[1].player, instance->targets[3].player);
				  second = MIN(instance->targets[1].player, instance->targets[3].player);
				}
			  else
				{
				  first = instance->targets[1].player;
				  second = instance->targets[3].player;
				}
				rfg_card_from_grave(instance->targets[0].player, first);
				rfg_card_from_grave(instance->targets[2].player, second);
				//Reset the target array to avoid unwanted interactions with Duplicant, for example.
				int i;
				for(i=0; i<4; i++){
					instance->targets[i].player = -1;
					instance->targets[i].card = -1;
				}
			}
		}
	  instance->state &= ~STATE_OUBLIETTED;
	}

  enters_the_battlefield_as_copy_of(player, card, event, -1, iid);	// enters_the_battlefield_as_copy_of() only examines iid during EVENT_RESOLVE_SPELL

  return 0;
}

int card_trench_gorger(int player, int card, event_t event){
/*
Trench Gorger |6|U|U
Creature — Leviathan 6/6
Trample
When Trench Gorger enters the battlefield, you may search your library for any number of land cards, exile them,
	then shuffle your library. If you do, Trench Gorger has base power and base toughness each equal to the number of cards exiled this way.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t* instance = get_card_instance(player, card);
		if( instance->targets[1].card < 0 ){
			instance->targets[1].card = 0;
		}

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a land card to exile.");
		this_test.no_shuffle = 1;

		while( new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test) != -1 ){
				instance->targets[1].card++;
				if( instance->targets[1].card == 4 && player == AI ){
					break;
				}
		}
		shuffle(player);
	}

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		int pow = 6-get_base_power(player, card);
		modify_pt_and_abilities(player, card, event, pow, pow, 0);
	}

	return 0;
}

int card_tribute_to_the_wild(int player, int card, event_t event){
/* CARD_ID_TRIBUTE_TO_THE_WILD	11749
Tribute to the Wild	|1|G
Instant
Each opponent sacrifices an artifact or enchantment. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT | TYPE_ENCHANTMENT);
		new_sacrifice(player, card, 1-player, SAC_NO_CANCEL | SAC_RETURN_CHOICE | SAC_CAUSED, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}


int card_vish_kal_blood_arbiter( int player, int card, event_t event){
/*
Vish Kal, Blood Arbiter |4|W|B|B
Legendary Creature — Vampire 5/5,
Flying, lifelink
Sacrifice a creature: Put X +1/+1 counters on Vish Kal, Blood Arbiter, where X is the sacrificed creature's power.
Remove all +1/+1 counters from Vish Kal: Target creature gets -1/-1 until end of turn for each +1/+1 counter removed this way.
*/
	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		// Assuming the AI will sacrifice the creature with least power
		int pump = 0;
		if( can_use_activated_abilities(player, card) && has_mana_for_activated_ability(player, card, MANACOST0) ){
			int min_pow = 100;
			int found = 0;
			int c;
			for(c=0; c<active_cards_count[player]; c++){
				if( in_play(player, c) && is_what(player, c, TYPE_CREATURE) && !check_special_flags2(player, c, SF2_CANNOT_BE_SACRIFICED) ){
					int this_pow = get_power(player, c);
					if( this_pow < min_pow ){
						min_pow = this_pow;
						found =1;
					}
				}
			}
			pump = found ? min_pow : 0;
		}
		return pump;
	}

	check_legend_rule(player, card, event);

	lifelink(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_PUMP = 1,
		CHOICE_WEAKEN
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL) ){
			return 1;
		}
		if( count_1_1_counters(player, card) ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int ab1 = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL);
			int ab2 = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) &&
						count_1_1_counters(player, card) ? 1 : 0;
			int p1 = current_turn == PHASE_AFTER_BLOCKING ? 10 : 1;
			int p2 = current_phase != PHASE_AFTER_BLOCKING ? count_1_1_counters(player, card) * 2 : 1;
			int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
								"Sac and add +1/+1 counters", ab1, p1,
								"Remove +1/+1 counters and weaken creature", ab2, p2);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( choice == CHOICE_PUMP ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
				int sac = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
				if( sac ){
					SET_BYTE1(instance->info_slot) = get_power(BYTE2(sac), BYTE3(sac));
					kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				}
				else{
					spell_fizzled = 1;
				}
			}
			if( choice == CHOICE_WEAKEN ){
				if( pick_target(&td, "TARGET_CREATURE") ){
					int amount = count_counters(player, card, COUNTER_P1_P1);
					remove_1_1_counters(player, card, amount);
					SET_BYTE1(instance->info_slot) = amount;
				}
			}
			SET_BYTE0(instance->info_slot) = choice;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = BYTE0(instance->info_slot);
		int amount = BYTE1(instance->info_slot);
		if( choice == CHOICE_PUMP ){
			add_1_1_counters(instance->parent_controller, instance->parent_card, amount);
		}
		if( choice == CHOICE_WEAKEN ){
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
											instance->parent_controller, instance->parent_card, -amount, -amount);
		}
	}

	return 0;
}

int card_stranglehold(int player, int card, event_t event){
/* CARD_ID_STRANGLEHOLD	11743
Stranglehold	|3|R
Enchantment
Your opponents can't search libraries.
If an opponent would begin an extra turn, that player skips that turn instead. */

	return global_enchantment(player, card, event);
}

int card_vow_of_duty(int player, int card, event_t event){
/* CARD_ID_VOW_OF_DUTY	11726
Vow of Duty	|2|W
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2, has vigilance, and can't attack you or a planeswalker you control. */

	return vow(player, card, event, 2, 2, 0, SP_KEYWORD_VIGILANCE);
}

int card_vow_of_flight(int player, int card, event_t event){
/* CARD_ID_VOW_OF_FLIGHT	11731
Vow of Flight	|2|U
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2, has flying, and can't attack you or a planeswalker you control. */

	return vow(player, card, event, 2, 2, KEYWORD_FLYING, 0);
}

int card_vow_of_lightning(int player, int card, event_t event){
/* CARD_ID_VOW_OF_LIGHTNING	11744
Vow of Lightning	|2|R
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2, has first strike, and can't attack you or a planeswalker you control. */

	return vow(player, card, event, 2, 2, KEYWORD_FIRST_STRIKE, 0);
}

int card_vow_of_malice(int player, int card, event_t event){
/* CARD_ID_VOW_OF_MALICE	11737
Vow of Malice	|2|B
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2, has intimidate, and can't attack you or a planeswalker you control.
(A creature with intimidate can't be blocked except by artifact creatures and/or creatures that share a color with it.) */

	return vow(player, card, event, 2, 2, 0, SP_KEYWORD_INTIMIDATE);
}

int card_vow_of_wildness(int player, int card, event_t event){
/* CARD_ID_VOW_OF_WILDNESS	11750
Vow of Wildness	|2|G
Enchantment - Aura
Enchant creature
Enchanted creature gets +3/+3, has trample, and can't attack you or a planeswalker you control. */

	return vow(player, card, event, 3, 3, KEYWORD_TRAMPLE, 0);
}

int card_zedruu_the_greathearted( int player, int card, event_t event){
/*
Zedruu the Greathearted |1|W|U|R
Legendary Creature — Minotaur Monk 2/4
At the beginning of your upkeep, you gain X life and draw X cards, where X is the number of permanents you own that your opponents control.
{R}{W}{U}: Target opponent gains control of target permanent you control.
*/
	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int amount = 0;
		int count = active_cards_count[1-player]-1;
		while( count > -1 ){
				if( in_play(1-player, count) && is_what(1-player, count, TYPE_PERMANENT) ){
					if( is_stolen(1-player, count) ){
						amount++;
					}
				}
				count--;
		}
		gain_life(player, amount);
		draw_cards(player, amount);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.illegal_abilities = 0;
	td.allowed_controller = player;
	td.preferred_controller = player;

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0);
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( would_validate_arbitrary_target(&td1, 1-player, -1) ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_RWU(1, 1, 1), 0, &td, NULL);
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_RWU(1, 1, 1)) ){
			if( new_pick_target(&td, "Select target permanent you control.", 0, 1 | GS_LITERAL_PROMPT) ){
				instance->targets[1].player = 1-player;
				instance->targets[1].card = -1;
				instance->number_of_targets = 2;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td1, 1) ){
			give_control_of_self(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}


